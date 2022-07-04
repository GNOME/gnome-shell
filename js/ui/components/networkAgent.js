// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Component */

const { Clutter, Gio, GLib, GObject, NM, Pango, Shell, St } = imports.gi;
const Signals = imports.misc.signals;

const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;

Gio._promisify(Shell.NetworkAgent.prototype, 'init_async');
Gio._promisify(Shell.NetworkAgent.prototype, 'search_vpn_plugin');

const VPN_UI_GROUP = 'VPN Plugin UI';

var NetworkSecretDialog = GObject.registerClass(
class NetworkSecretDialog extends ModalDialog.ModalDialog {
    _init(agent, requestId, connection, settingName, hints, flags, contentOverride) {
        super._init({ styleClass: 'prompt-dialog' });

        this._agent = agent;
        this._requestId = requestId;
        this._connection = connection;
        this._settingName = settingName;
        this._hints = hints;

        if (contentOverride)
            this._content = contentOverride;
        else
            this._content = this._getContent();

        let contentBox = new Dialog.MessageDialogContent({
            title: this._content.title,
            description: this._content.message,
        });

        let initialFocusSet = false;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            let reactive = secret.key != null;

            let entryParams = {
                style_class: 'prompt-dialog-password-entry',
                hint_text: secret.label,
                text: secret.value,
                can_focus: reactive,
                reactive,
                x_align: Clutter.ActorAlign.CENTER,
            };
            if (secret.password)
                secret.entry = new St.PasswordEntry(entryParams);
            else
                secret.entry = new St.Entry(entryParams);
            ShellEntry.addContextMenu(secret.entry);
            contentBox.add_child(secret.entry);

            if (secret.validate)
                secret.valid = secret.validate(secret);
            else // no special validation, just ensure it's not empty
                secret.valid = secret.value.length > 0;

            if (reactive) {
                if (!initialFocusSet) {
                    this.setInitialKeyFocus(secret.entry);
                    initialFocusSet = true;
                }

                secret.entry.clutter_text.connect('activate', this._onOk.bind(this));
                secret.entry.clutter_text.connect('text-changed', () => {
                    secret.value = secret.entry.get_text();
                    if (secret.validate)
                        secret.valid = secret.validate(secret);
                    else
                        secret.valid = secret.value.length > 0;
                    this._updateOkButton();
                });
            } else {
                secret.valid = true;
            }
        }

        if (this._content.secrets.some(s => s.password)) {
            let capsLockWarning = new ShellEntry.CapsLockWarning();
            contentBox.add_child(capsLockWarning);
        }

        if (flags & NM.SecretAgentGetSecretsFlags.WPS_PBC_ACTIVE) {
            let descriptionLabel = new St.Label({
                text: _('Alternatively you can connect by pushing the “WPS” button on your router.'),
                style_class: 'message-dialog-description',
            });
            descriptionLabel.clutter_text.line_wrap = true;
            descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

            contentBox.add_child(descriptionLabel);
        }

        this.contentLayout.add_child(contentBox);

        this._okButton = {
            label: _("Connect"),
            action: this._onOk.bind(this),
            default: true,
        };

        this.setButtons([{
            label: _("Cancel"),
            action: this.cancel.bind(this),
            key: Clutter.KEY_Escape,
        }, this._okButton]);

        this._updateOkButton();
    }

    _updateOkButton() {
        let valid = true;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            valid &&= secret.valid;
        }

        this._okButton.button.reactive = valid;
        this._okButton.button.can_focus = valid;
    }

    _onOk() {
        let valid = true;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            valid &&= secret.valid;
            if (secret.key !== null) {
                if (this._settingName === 'vpn')
                    this._agent.add_vpn_secret(this._requestId, secret.key, secret.value);
                else
                    this._agent.set_password(this._requestId, secret.key, secret.value);
            }
        }

        if (valid) {
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.CONFIRMED);
            this.close(global.get_current_time());
        }
        // do nothing if not valid
    }

    cancel() {
        this._agent.respond(this._requestId, Shell.NetworkAgentResponse.USER_CANCELED);
        this.close(global.get_current_time());
    }

    _validateWpaPsk(secret) {
        let value = secret.value;
        if (value.length == 64) {
            // must be composed of hexadecimal digits only
            for (let i = 0; i < 64; i++) {
                if (!((value[i] >= 'a' && value[i] <= 'f') ||
                      (value[i] >= 'A' && value[i] <= 'F') ||
                      (value[i] >= '0' && value[i] <= '9')))
                    return false;
            }
            return true;
        }

        return value.length >= 8 && value.length <= 63;
    }

    _validateStaticWep(secret) {
        let value = secret.value;
        if (secret.wep_key_type == NM.WepKeyType.KEY) {
            if (value.length == 10 || value.length == 26) {
                for (let i = 0; i < value.length; i++) {
                    if (!((value[i] >= 'a' && value[i] <= 'f') ||
                          (value[i] >= 'A' && value[i] <= 'F') ||
                          (value[i] >= '0' && value[i] <= '9')))
                        return false;
                }
            } else if (value.length == 5 || value.length == 13) {
                for (let i = 0; i < value.length; i++) {
                    if (!((value[i] >= 'a' && value[i] <= 'z') ||
                          (value[i] >= 'A' && value[i] <= 'Z')))
                        return false;
                }
            } else {
                return false;
            }
        } else if (secret.wep_key_type == NM.WepKeyType.PASSPHRASE) {
            if (value.length < 0 || value.length > 64)
                return false;
        }
        return true;
    }

    _getWirelessSecrets(secrets, _wirelessSetting) {
        let wirelessSecuritySetting = this._connection.get_setting_wireless_security();

        if (this._settingName == '802-1x') {
            this._get8021xSecrets(secrets);
            return;
        }

        switch (wirelessSecuritySetting.key_mgmt) {
        // First the easy ones
        case 'wpa-none':
        case 'wpa-psk':
        case 'sae':
            secrets.push({
                label: _('Password'),
                key: 'psk',
                value: wirelessSecuritySetting.psk || '',
                validate: this._validateWpaPsk,
                password: true,
            });
            break;
        case 'none': // static WEP
            secrets.push({
                label: _('Key'),
                key: `wep-key${wirelessSecuritySetting.wep_tx_keyidx}`,
                value: wirelessSecuritySetting.get_wep_key(wirelessSecuritySetting.wep_tx_keyidx) || '',
                wep_key_type: wirelessSecuritySetting.wep_key_type,
                validate: this._validateStaticWep,
                password: true,
            });
            break;
        case 'ieee8021x':
            if (wirelessSecuritySetting.auth_alg == 'leap') { // Cisco LEAP
                secrets.push({
                    label: _('Password'),
                    key: 'leap-password',
                    value: wirelessSecuritySetting.leap_password || '',
                    password: true,
                });
            } else { // Dynamic (IEEE 802.1x) WEP
                this._get8021xSecrets(secrets);
            }
            break;
        case 'wpa-eap':
            this._get8021xSecrets(secrets);
            break;
        default:
            log(`Invalid wireless key management: ${wirelessSecuritySetting.key_mgmt}`);
        }
    }

    _get8021xSecrets(secrets) {
        let ieee8021xSetting = this._connection.get_setting_802_1x();

        /* If hints were given we know exactly what we need to ask */
        if (this._settingName == "802-1x" && this._hints.length) {
            if (this._hints.includes('identity')) {
                secrets.push({
                    label: _('Username'),
                    key: 'identity',
                    value: ieee8021xSetting.identity || '',
                    password: false,
                });
            }
            if (this._hints.includes('password')) {
                secrets.push({
                    label: _('Password'),
                    key: 'password',
                    value: ieee8021xSetting.password || '',
                    password: true,
                });
            }
            if (this._hints.includes('private-key-password')) {
                secrets.push({
                    label: _('Private key password'),
                    key: 'private-key-password',
                    value: ieee8021xSetting.private_key_password || '',
                    password: true,
                });
            }
            return;
        }

        switch (ieee8021xSetting.get_eap_method(0)) {
        case 'md5':
        case 'leap':
        case 'ttls':
        case 'peap':
        case 'fast':
            // TTLS and PEAP are actually much more complicated, but this complication
            // is not visible here since we only care about phase2 authentication
            // (and don't even care of which one)
            secrets.push({
                label: _('Username'),
                key: null,
                value: ieee8021xSetting.identity || '',
                password: false,
            });
            secrets.push({
                label: _('Password'),
                key: 'password',
                value: ieee8021xSetting.password || '',
                password: true,
            });
            break;
        case 'tls':
            secrets.push({
                label: _('Identity'),
                key: null,
                value: ieee8021xSetting.identity || '',
                password: false,
            });
            secrets.push({
                label: _('Private key password'),
                key: 'private-key-password',
                value: ieee8021xSetting.private_key_password || '',
                password: true,
            });
            break;
        default:
            log(`Invalid EAP/IEEE802.1x method: ${ieee8021xSetting.get_eap_method(0)}`);
        }
    }

    _getPPPoESecrets(secrets) {
        let pppoeSetting = this._connection.get_setting_pppoe();
        secrets.push({
            label: _('Username'),
            key: 'username',
            value: pppoeSetting.username || '',
            password: false,
        });
        secrets.push({
            label: _('Service'), key: 'service',
            value: pppoeSetting.service || '',
            password: false,
        });
        secrets.push({
            label: _('Password'), key: 'password',
            value: pppoeSetting.password || '',
            password: true,
        });
    }

    _getMobileSecrets(secrets, connectionType) {
        let setting;
        if (connectionType == 'bluetooth')
            setting = this._connection.get_setting_cdma() || this._connection.get_setting_gsm();
        else
            setting = this._connection.get_setting_by_name(connectionType);
        secrets.push({
            label: _('Password'),
            key: 'password',
            value: setting.value || '',
            password: true,
        });
    }

    _getContent() {
        let connectionSetting = this._connection.get_setting_connection();
        let connectionType = connectionSetting.get_connection_type();
        let wirelessSetting;
        let ssid;

        let content = { };
        content.secrets = [];

        switch (connectionType) {
        case '802-11-wireless':
            wirelessSetting = this._connection.get_setting_wireless();
            ssid = NM.utils_ssid_to_utf8(wirelessSetting.get_ssid().get_data());
            content.title = _('Authentication required');
            content.message = _("Passwords or encryption keys are required to access the wireless network “%s”.").format(ssid);
            this._getWirelessSecrets(content.secrets, wirelessSetting);
            break;
        case '802-3-ethernet':
            content.title = _("Wired 802.1X authentication");
            content.message = null;
            content.secrets.push({
                label: _('Network name'),
                key: null,
                value: connectionSetting.get_id(),
                password: false,
            });
            this._get8021xSecrets(content.secrets);
            break;
        case 'pppoe':
            content.title = _("DSL authentication");
            content.message = null;
            this._getPPPoESecrets(content.secrets);
            break;
        case 'gsm':
            if (this._hints.includes('pin')) {
                let gsmSetting = this._connection.get_setting_gsm();
                content.title = _("PIN code required");
                content.message = _("PIN code is needed for the mobile broadband device");
                content.secrets.push({
                    label: _('PIN'),
                    key: 'pin',
                    value: gsmSetting.pin || '',
                    password: true,
                });
                break;
            }
            // fall through
        case 'cdma':
        case 'bluetooth':
            content.title = _('Authentication required');
            content.message = _("A password is required to connect to “%s”.").format(connectionSetting.get_id());
            this._getMobileSecrets(content.secrets, connectionType);
            break;
        default:
            log(`Invalid connection type: ${connectionType}`);
        }

        return content;
    }
});

var VPNRequestHandler = class extends Signals.EventEmitter {
    constructor(agent, requestId, authHelper, serviceType, connection, hints, flags) {
        super();

        this._agent = agent;
        this._requestId = requestId;
        this._connection = connection;
        this._flags = flags;
        this._pluginOutBuffer = [];
        this._title = null;
        this._description = null;
        this._content = [];
        this._shellDialog = null;

        let connectionSetting = connection.get_setting_connection();

        const argv = [
            authHelper.fileName,
            '-u', connectionSetting.uuid,
            '-n', connectionSetting.id,
            '-s', serviceType,
        ];
        if (authHelper.externalUIMode)
            argv.push('--external-ui-mode');
        if (flags & NM.SecretAgentGetSecretsFlags.ALLOW_INTERACTION)
            argv.push('-i');
        if (flags & NM.SecretAgentGetSecretsFlags.REQUEST_NEW)
            argv.push('-r');
        if (authHelper.supportsHints) {
            for (let i = 0; i < hints.length; i++) {
                argv.push('-t');
                argv.push(hints[i]);
            }
        }

        this._newStylePlugin = authHelper.externalUIMode;

        try {
            let [success_, pid, stdin, stdout, stderr] =
                GLib.spawn_async_with_pipes(
                    null, /* pwd */
                    argv,
                    null, /* envp */
                    GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                    () => {
                        try {
                            global.context.restore_rlimit_nofile();
                        } catch (err) {
                        }
                    });

            this._childPid = pid;
            this._stdin = new Gio.UnixOutputStream({ fd: stdin, close_fd: true });
            this._stdout = new Gio.UnixInputStream({ fd: stdout, close_fd: true });
            GLib.close(stderr);
            this._dataStdout = new Gio.DataInputStream({ base_stream: this._stdout });

            if (this._newStylePlugin)
                this._readStdoutNewStyle();
            else
                this._readStdoutOldStyle();

            this._childWatch = GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid,
                                                    this._vpnChildFinished.bind(this));

            this._writeConnection();
        } catch (e) {
            logError(e, 'error while spawning VPN auth helper');

            this._agent.respond(requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
        }
    }

    cancel(respond) {
        if (respond)
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.USER_CANCELED);

        if (this._newStylePlugin && this._shellDialog) {
            this._shellDialog.close(global.get_current_time());
            this._shellDialog.destroy();
        } else {
            try {
                this._stdin.write('QUIT\n\n', null);
            } catch (e) { /* ignore broken pipe errors */ }
        }

        this.destroy();
    }

    destroy() {
        if (this._destroyed)
            return;

        this.emit('destroy');
        if (this._childWatch)
            GLib.source_remove(this._childWatch);

        this._stdin.close(null);
        // Stdout is closed when we finish reading from it

        this._destroyed = true;
    }

    _vpnChildFinished(pid, status, _requestObj) {
        this._childWatch = 0;
        if (this._newStylePlugin) {
            // For new style plugin, all work is done in the async reading functions
            // Just reap the process here
            return;
        }

        let [exited, exitStatus] = Shell.util_wifexited(status);

        if (exited) {
            if (exitStatus != 0)
                this._agent.respond(this._requestId, Shell.NetworkAgentResponse.USER_CANCELED);
            else
                this._agent.respond(this._requestId, Shell.NetworkAgentResponse.CONFIRMED);
        } else {
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
        }

        this.destroy();
    }

    _vpnChildProcessLineOldStyle(line) {
        if (this._previousLine != undefined) {
            // Two consecutive newlines mean that the child should be closed
            // (the actual newlines are eaten by Gio.DataInputStream)
            // Send a termination message
            if (line == '' && this._previousLine == '') {
                try {
                    this._stdin.write('QUIT\n\n', null);
                } catch (e) { /* ignore broken pipe errors */ }
            } else {
                this._agent.add_vpn_secret(this._requestId, this._previousLine, line);
                this._previousLine = undefined;
            }
        } else {
            this._previousLine = line;
        }
    }

    async _readStdoutOldStyle() {
        const [line, len_] =
            await this._dataStdout.read_line_async(GLib.PRIORITY_DEFAULT, null);

        if (line === null) {
            // end of file
            this._stdout.close(null);
            return;
        }

        const decoder = new TextDecoder();
        this._vpnChildProcessLineOldStyle(decoder.decode(line));

        // try to read more!
        this._readStdoutOldStyle();
    }

    async _readStdoutNewStyle() {
        const cnt =
            await this._dataStdout.fill_async(-1, GLib.PRIORITY_DEFAULT, null);

        if (cnt === 0) {
            // end of file
            this._showNewStyleDialog();

            this._stdout.close(null);
            return;
        }

        // Try to read more
        this._dataStdout.set_buffer_size(2 * this._dataStdout.get_buffer_size());
        this._readStdoutNewStyle();
    }

    _showNewStyleDialog() {
        let keyfile = new GLib.KeyFile();
        let data;
        let contentOverride;

        try {
            data = new GLib.Bytes(this._dataStdout.peek_buffer());
            keyfile.load_from_bytes(data, GLib.KeyFileFlags.NONE);

            if (keyfile.get_integer(VPN_UI_GROUP, 'Version') != 2)
                throw new Error('Invalid plugin keyfile version, is %d');

            contentOverride = {
                title: keyfile.get_string(VPN_UI_GROUP, 'Title'),
                message: keyfile.get_string(VPN_UI_GROUP, 'Description'),
                secrets: [],
            };

            let [groups, len_] = keyfile.get_groups();
            for (let i = 0; i < groups.length; i++) {
                if (groups[i] == VPN_UI_GROUP)
                    continue;

                let value = keyfile.get_string(groups[i], 'Value');
                let shouldAsk = keyfile.get_boolean(groups[i], 'ShouldAsk');

                if (shouldAsk) {
                    contentOverride.secrets.push({
                        label: keyfile.get_string(groups[i], 'Label'),
                        key: groups[i],
                        value,
                        password: keyfile.get_boolean(groups[i], 'IsSecret'),
                    });
                } else {
                    if (!value.length) // Ignore empty secrets
                        continue;

                    this._agent.add_vpn_secret(this._requestId, groups[i], value);
                }
            }
        } catch (e) {
            // No output is a valid case it means "both secrets are stored"
            if (data.length > 0) {
                logError(e, 'error while reading VPN plugin output keyfile');

                this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
                this.destroy();
                return;
            }
        }

        if (contentOverride && contentOverride.secrets.length) {
            // Only show the dialog if we actually have something to ask
            this._shellDialog = new NetworkSecretDialog(this._agent, this._requestId, this._connection, 'vpn', [], this._flags, contentOverride);
            this._shellDialog.open(global.get_current_time());
        } else {
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.CONFIRMED);
            this.destroy();
        }
    }

    _writeConnection() {
        let vpnSetting = this._connection.get_setting_vpn();

        try {
            vpnSetting.foreach_data_item((key, value) => {
                this._stdin.write(`DATA_KEY=${key}\n`, null);
                this._stdin.write(`DATA_VAL=${value || ''}\n\n`, null);
            });
            vpnSetting.foreach_secret((key, value) => {
                this._stdin.write(`SECRET_KEY=${key}\n`, null);
                this._stdin.write(`SECRET_VAL=${value || ''}\n\n`, null);
            });
            this._stdin.write('DONE\n\n', null);
        } catch (e) {
            logError(e, 'internal error while writing connection to helper');

            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
            this.destroy();
        }
    }
};

var NetworkAgent = class {
    constructor() {
        this._native = new Shell.NetworkAgent({
            identifier: 'org.gnome.Shell.NetworkAgent',
            capabilities: NM.SecretAgentCapabilities.VPN_HINTS,
            auto_register: false,
        });

        this._dialogs = { };
        this._vpnRequests = { };
        this._notifications = { };

        this._native.connect('new-request', this._newRequest.bind(this));
        this._native.connect('cancel-request', this._cancelRequest.bind(this));

        this._initialized = false;
        this._initNative();
    }

    async _initNative() {
        try {
            await this._native.init_async(GLib.PRIORITY_DEFAULT, null);
            this._initialized = true;
        } catch (e) {
            this._native = null;
            logError(e, 'error initializing the NetworkManager Agent');
        }
    }

    enable() {
        if (!this._native)
            return;

        this._native.auto_register = true;
        if (this._initialized && !this._native.registered)
            this._native.register_async(null, null);
    }

    disable() {
        let requestId;

        for (requestId in this._dialogs)
            this._dialogs[requestId].cancel();
        this._dialogs = { };

        for (requestId in this._vpnRequests)
            this._vpnRequests[requestId].cancel(true);
        this._vpnRequests = { };

        for (requestId in this._notifications)
            this._notifications[requestId].destroy();
        this._notifications = { };

        if (!this._native)
            return;

        this._native.auto_register = false;
        if (this._initialized && this._native.registered)
            this._native.unregister_async(null, null);
    }

    _showNotification(requestId, connection, settingName, hints, flags) {
        let source = new MessageTray.Source(_("Network Manager"), 'network-transmit-receive');
        source.policy = new MessageTray.NotificationApplicationPolicy('gnome-network-panel');

        let title, body;

        let connectionSetting = connection.get_setting_connection();
        let connectionType = connectionSetting.get_connection_type();
        switch (connectionType) {
        case '802-11-wireless': {
            let wirelessSetting = connection.get_setting_wireless();
            let ssid = NM.utils_ssid_to_utf8(wirelessSetting.get_ssid().get_data());
            title = _('Authentication required');
            body = _("Passwords or encryption keys are required to access the wireless network “%s”.").format(ssid);
            break;
        }
        case '802-3-ethernet':
            title = _("Wired 802.1X authentication");
            body = _('A password is required to connect to “%s”.').format(connection.get_id());
            break;
        case 'pppoe':
            title = _("DSL authentication");
            body = _('A password is required to connect to “%s”.').format(connection.get_id());
            break;
        case 'gsm':
            if (hints.includes('pin')) {
                title = _("PIN code required");
                body = _("PIN code is needed for the mobile broadband device");
                break;
            }
            // fall through
        case 'cdma':
        case 'bluetooth':
            title = _('Authentication required');
            body = _("A password is required to connect to “%s”.").format(connectionSetting.get_id());
            break;
        case 'vpn':
            title = _("VPN password");
            body = _("A password is required to connect to “%s”.").format(connectionSetting.get_id());
            break;
        default:
            log(`Invalid connection type: ${connectionType}`);
            this._native.respond(requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
            return;
        }

        let notification = new MessageTray.Notification(source, title, body);

        notification.connect('activated', () => {
            notification.answered = true;
            this._handleRequest(requestId, connection, settingName, hints, flags);
        });

        this._notifications[requestId] = notification;
        notification.connect('destroy', () => {
            if (!notification.answered)
                this._native.respond(requestId, Shell.NetworkAgentResponse.USER_CANCELED);
            delete this._notifications[requestId];
        });

        Main.messageTray.add(source);
        source.showNotification(notification);
    }

    _newRequest(agent, requestId, connection, settingName, hints, flags) {
        if (!(flags & NM.SecretAgentGetSecretsFlags.USER_REQUESTED))
            this._showNotification(requestId, connection, settingName, hints, flags);
        else
            this._handleRequest(requestId, connection, settingName, hints, flags);
    }

    _handleRequest(requestId, connection, settingName, hints, flags) {
        if (settingName == 'vpn') {
            this._vpnRequest(requestId, connection, hints, flags);
            return;
        }

        let dialog = new NetworkSecretDialog(this._native, requestId, connection, settingName, hints, flags);
        dialog.connect('destroy', () => {
            delete this._dialogs[requestId];
        });
        this._dialogs[requestId] = dialog;
        dialog.open(global.get_current_time());
    }

    _cancelRequest(agent, requestId) {
        if (this._dialogs[requestId]) {
            this._dialogs[requestId].close(global.get_current_time());
            this._dialogs[requestId].destroy();
            delete this._dialogs[requestId];
        } else if (this._vpnRequests[requestId]) {
            this._vpnRequests[requestId].cancel(false);
            delete this._vpnRequests[requestId];
        }
    }

    async _vpnRequest(requestId, connection, hints, flags) {
        let vpnSetting = connection.get_setting_vpn();
        let serviceType = vpnSetting.service_type;

        let binary = await this._findAuthBinary(serviceType);
        if (!binary) {
            log('Invalid VPN service type (cannot find authentication binary)');

            /* cancel the auth process */
            this._native.respond(requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
            return;
        }

        let vpnRequest = new VPNRequestHandler(this._native, requestId, binary, serviceType, connection, hints, flags);
        vpnRequest.connect('destroy', () => {
            delete this._vpnRequests[requestId];
        });
        this._vpnRequests[requestId] = vpnRequest;
    }

    async _findAuthBinary(serviceType) {
        let plugin;

        try {
            plugin = await this._native.search_vpn_plugin(serviceType);
        } catch (e) {
            logError(e);
            return null;
        }

        const fileName = plugin.get_auth_dialog();
        if (!GLib.file_test(fileName, GLib.FileTest.IS_EXECUTABLE)) {
            log(`VPN plugin at ${fileName} is not executable`);
            return null;
        }

        const prop = plugin.lookup_property('GNOME', 'supports-external-ui-mode');
        const trimmedProp = prop?.trim().toLowerCase() ?? '';

        return {
            fileName,
            supportsHints: plugin.supports_hints(),
            externalUIMode: ['true', 'yes', 'on', '1'].includes(trimmedProp),
        };
    }
};
var Component = NetworkAgent;
