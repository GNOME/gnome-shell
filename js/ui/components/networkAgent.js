// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const NetworkManager = imports.gi.NetworkManager;
const NMClient = imports.gi.NMClient;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Config = imports.misc.config;
const ModalDialog = imports.ui.modalDialog;
const PopupMenu = imports.ui.popupMenu;
const ShellEntry = imports.ui.shellEntry;

const VPN_UI_GROUP = 'VPN Plugin UI';

const NetworkSecretDialog = new Lang.Class({
    Name: 'NetworkSecretDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(agent, requestId, connection, settingName, hints, contentOverride) {
        this.parent({ styleClass: 'prompt-dialog' });

        this._agent = agent;
        this._requestId = requestId;
        this._connection = connection;
        this._settingName = settingName;
        this._hints = hints;

        if (contentOverride)
            this._content = contentOverride;
        else
            this._content = this._getContent();

        let mainContentBox = new St.BoxLayout({ style_class: 'prompt-dialog-main-layout',
                                                vertical: false });
        this.contentLayout.add(mainContentBox,
                               { x_fill: true,
                                 y_fill: true });

        let icon = new St.Icon({ icon_name: 'dialog-password-symbolic' });
        mainContentBox.add(icon,
                           { x_fill:  true,
                             y_fill:  false,
                             x_align: St.Align.END,
                             y_align: St.Align.START });

        let messageBox = new St.BoxLayout({ style_class: 'prompt-dialog-message-layout',
                                            vertical: true });
        mainContentBox.add(messageBox,
                           { y_align: St.Align.START });

        let subjectLabel = new St.Label({ style_class: 'prompt-dialog-headline',
                                            text: this._content.title });
        messageBox.add(subjectLabel,
                       { y_fill:  false,
                         y_align: St.Align.START });

        if (this._content.message != null) {
            let descriptionLabel = new St.Label({ style_class: 'prompt-dialog-description',
                                                  text: this._content.message });
            descriptionLabel.clutter_text.line_wrap = true;
            descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

            messageBox.add(descriptionLabel,
                           { y_fill:  true,
                             y_align: St.Align.START,
                             expand: true });
        }

        let layout = new Clutter.TableLayout();
        let secretTable = new St.Widget({ style_class: 'network-dialog-secret-table',
                                          layout_manager: layout });
        layout.hookup_style(secretTable);

        let initialFocusSet = false;
        let pos = 0;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            let label = new St.Label({ style_class: 'prompt-dialog-password-label',
                                       text: secret.label });
            label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

            let reactive = secret.key != null;

            secret.entry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                          text: secret.value, can_focus: reactive,
                                          reactive: reactive });
            ShellEntry.addContextMenu(secret.entry,
                                      { isPassword: secret.password });

            if (secret.validate)
                secret.valid = secret.validate(secret);
            else // no special validation, just ensure it's not empty
                secret.valid = secret.value.length > 0;

            if (reactive) {
                if (!initialFocusSet) {
                    this.setInitialKeyFocus(secret.entry);
                    initialFocusSet = true;
                }

                secret.entry.clutter_text.connect('activate', Lang.bind(this, this._onOk));
                secret.entry.clutter_text.connect('text-changed', Lang.bind(this, function() {
                    secret.value = secret.entry.get_text();
                    if (secret.validate)
                        secret.valid = secret.validate(secret);
                    else
                        secret.valid = secret.value.length > 0;
                    this._updateOkButton();
                }));
            } else
                secret.valid = true;

            layout.pack(label, 0, pos);
            layout.child_set(label, { x_expand: false, y_fill: false,
                                      x_align: Clutter.TableAlignment.START });
            layout.pack(secret.entry, 1, pos);
            pos++;

            if (secret.password)
                secret.entry.clutter_text.set_password_char('\u25cf');
        }

        messageBox.add(secretTable);

        this._okButton = { label:  _("Connect"),
                           action: Lang.bind(this, this._onOk),
                           default: true
                         };

        this.setButtons([{ label: _("Cancel"),
                           action: Lang.bind(this, this.cancel),
                           key:    Clutter.KEY_Escape,
                         },
                         this._okButton]);

        this._updateOkButton();
    },

    _updateOkButton: function() {
        let valid = true;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            valid = valid && secret.valid;
        }

        this._okButton.button.reactive = valid;
        this._okButton.button.can_focus = valid;
    },

    _onOk: function() {
        let valid = true;
        for (let i = 0; i < this._content.secrets.length; i++) {
            let secret = this._content.secrets[i];
            valid = valid && secret.valid;
            if (secret.key != null)
                this._agent.set_password(this._requestId, secret.key, secret.value);
        }

        if (valid) {
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.CONFIRMED);
            this.close(global.get_current_time());
        }
        // do nothing if not valid
    },

    cancel: function() {
        this._agent.respond(this._requestId, Shell.NetworkAgentResponse.USER_CANCELED);
        this.close(global.get_current_time());
    },

    _validateWpaPsk: function(secret) {
        let value = secret.value;
        if (value.length == 64) {
            // must be composed of hexadecimal digits only
            for (let i = 0; i < 64; i++) {
                if (!((value[i] >= 'a' && value[i] <= 'f')
                      || (value[i] >= 'A' && value[i] <= 'F')
                      || (value[i] >= '0' && value[i] <= '9')))
                    return false;
            }
            return true;
        }

        return (value.length >= 8 && value.length <= 63);
    },

    _validateStaticWep: function(secret) {
        let value = secret.value;
        if (secret.wep_key_type == NetworkManager.WepKeyType.KEY) {
            if (value.length == 10 || value.length == 26) {
		for (let i = 0; i < value.length; i++) {
                    if (!((value[i] >= 'a' && value[i] <= 'f')
                          || (value[i] >= 'A' && value[i] <= 'F')
                          || (value[i] >= '0' && value[i] <= '9')))
                        return false;
		}
	    } else if (value.length == 5 || value.length == 13) {
		for (let i = 0; i < value.length; i++) {
                    if (!((value[i] >= 'a' && value[i] <= 'z')
                          || (value[i] >= 'A' && value[i] <= 'Z')))
                        return false;
                }
            } else
                return false;
	} else if (secret.wep_key_type == NetworkManager.WepKeyType.PASSPHRASE) {
	    if (value.length < 0 || value.length > 64)
	        return false;
	}
        return true;
    },

    _getWirelessSecrets: function(secrets, wirelessSetting) {
        let wirelessSecuritySetting = this._connection.get_setting_wireless_security();
        switch (wirelessSecuritySetting.key_mgmt) {
        // First the easy ones
        case 'wpa-none':
        case 'wpa-psk':
            secrets.push({ label: _("Password: "), key: 'psk',
                           value: wirelessSecuritySetting.psk || '',
                           validate: this._validateWpaPsk, password: true });
            break;
        case 'none': // static WEP
            secrets.push({ label: _("Key: "), key: 'wep-key' + wirelessSecuritySetting.wep_tx_keyidx,
                           value: wirelessSecuritySetting.get_wep_key(wirelessSecuritySetting.wep_tx_keyidx) || '',
                           wep_key_type: wirelessSecuritySetting.wep_key_type,
                           validate: this._validateStaticWep, password: true });
            break;
        case 'ieee8021x':
            if (wirelessSecuritySetting.auth_alg == 'leap') // Cisco LEAP
                secrets.push({ label: _("Password: "), key: 'leap-password',
                               value: wirelessSecuritySetting.leap_password || '', password: true });
            else // Dynamic (IEEE 802.1x) WEP
                this._get8021xSecrets(secrets);
            break;
        case 'wpa-eap':
            this._get8021xSecrets(secrets);
            break;
        default:
            log('Invalid wireless key management: ' + wirelessSecuritySetting.key_mgmt);
        }
    },

    _get8021xSecrets: function(secrets) {
        let ieee8021xSetting = this._connection.get_setting_802_1x();
        let phase2method;

        switch (ieee8021xSetting.get_eap_method(0)) {
        case 'md5':
        case 'leap':
        case 'ttls':
        case 'peap':
        case 'fast':
            // TTLS and PEAP are actually much more complicated, but this complication
            // is not visible here since we only care about phase2 authentication
            // (and don't even care of which one)
            secrets.push({ label: _("Username: "), key: null,
                           value: ieee8021xSetting.identity || '', password: false });
            secrets.push({ label: _("Password: "), key: 'password',
                           value: ieee8021xSetting.password || '', password: true });
            break;
        case 'tls':
            secrets.push({ label: _("Identity: "), key: null,
                           value: ieee8021xSetting.identity || '', password: false });
            secrets.push({ label: _("Private key password: "), key: 'private-key-password',
                           value: ieee8021xSetting.private_key_password || '', password: true });
            break;
        default:
            log('Invalid EAP/IEEE802.1x method: ' + ieee8021xSetting.get_eap_method(0));
        }
    },

    _getPPPoESecrets: function(secrets) {
        let pppoeSetting = this._connection.get_setting_pppoe();
        secrets.push({ label: _("Username: "), key: 'username',
                       value: pppoeSetting.username || '', password: false });
        secrets.push({ label: _("Service: "), key: 'service',
                       value: pppoeSetting.service || '', password: false });
        secrets.push({ label: _("Password: "), key: 'password',
                       value: pppoeSetting.password || '', password: true });
    },

    _getMobileSecrets: function(secrets, connectionType) {
        let setting;
        if (connectionType == 'bluetooth')
            setting = this._connection.get_setting_cdma() || this._connection.get_setting_gsm();
        else
            setting = this._connection.get_setting_by_name(connectionType);
        secrets.push({ label: _("Password: "), key: 'password',
                       value: setting.value || '', password: true });
    },

    _getContent: function() {
        let connectionSetting = this._connection.get_setting_connection();
        let connectionType = connectionSetting.get_connection_type();
        let wirelessSetting;
        let ssid;

        let content = { };
        content.secrets = [ ];

        switch (connectionType) {
        case '802-11-wireless':
            wirelessSetting = this._connection.get_setting_wireless();
            ssid = NetworkManager.utils_ssid_to_utf8(wirelessSetting.get_ssid());
            content.title = _("Authentication required by wireless network");
            content.message = _("Passwords or encryption keys are required to access the wireless network '%s'.").format(ssid);
            this._getWirelessSecrets(content.secrets, wirelessSetting);
            break;
        case '802-3-ethernet':
            content.title = _("Wired 802.1X authentication");
            content.message = null;
            content.secrets.push({ label: _("Network name: "), key: null,
                                   value: connectionSetting.get_id(), password: false });
            this._get8021xSecrets(content.secrets);
            break;
        case 'pppoe':
            content.title = _("DSL authentication");
            content.message = null;
            this._getPPPoESecrets(content.secrets);
            break;
        case 'gsm':
            if (this._hints.indexOf('pin') != -1) {
                let gsmSetting = this._connection.get_setting_gsm();
                content.title = _("PIN code required");
                content.message = _("PIN code is needed for the mobile broadband device");
                content.secrets.push({ label: _("PIN: "), key: 'pin',
                                       value: gsmSetting.pin || '', password: true });
            }
            // fall through
        case 'cdma':
        case 'bluetooth':
            content.title = _("Mobile broadband network password");
            content.message = _("A password is required to connect to '%s'.").format(connectionSetting.get_id());
            this._getMobileSecrets(content.secrets, connectionType);
            break;
        default:
            log('Invalid connection type: ' + connectionType);
        };

        return content;
    }
});

const VPNRequestHandler = new Lang.Class({
    Name: 'VPNRequestHandler',

    _init: function(agent, requestId, authHelper, serviceType, connection, hints, flags) {
        this._agent = agent;
        this._requestId = requestId;
        this._connection = connection;
        this._pluginOutBuffer = [];
        this._title = null;
        this._description = null;
        this._content = [ ];
        this._shellDialog = null;

        let connectionSetting = connection.get_setting_connection();

        let argv = [ authHelper.fileName,
                     '-u', connectionSetting.uuid,
                     '-n', connectionSetting.id,
                     '-s', serviceType
                   ];
        if (authHelper.externalUIMode)
            argv.push('--external-ui-mode');
        if (flags & NMClient.SecretAgentGetSecretsFlags.ALLOW_INTERACTION)
            argv.push('-i');
        if (flags & NMClient.SecretAgentGetSecretsFlags.REQUEST_NEW)
            argv.push('-r');

        this._newStylePlugin = authHelper.externalUIMode;

        try {
            let [success, pid, stdin, stdout, stderr] =
                GLib.spawn_async_with_pipes(null, /* pwd */
                                            argv,
                                            null, /* envp */
                                            GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                            null /* child_setup */);

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
                                                    Lang.bind(this, this._vpnChildFinished));

            this._writeConnection();
        } catch(e) {
            logError(e, 'error while spawning VPN auth helper');

            this._agent.respond(requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
        }
    },

    cancel: function(respond) {
        if (respond)
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.USER_CANCELED);

        if (this._newStylePlugin && this._shellDialog) {
            this._shellDialog.close(global.get_current_time());
            this._shellDialog.destroy();
        } else {
            try {
                this._stdin.write('QUIT\n\n', null);
            } catch(e) { /* ignore broken pipe errors */ }
        }

        this.destroy();
    },

    destroy: function() {
        if (this._destroyed)
            return;

        GLib.source_remove(this._childWatch);

        this._stdin.close(null);
        // Stdout is closed when we finish reading from it

        this._destroyed = true;
    },

    _vpnChildFinished: function(pid, status, requestObj) {
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
        } else
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);

        this.destroy();
    },

    _vpnChildProcessLineOldStyle: function(line) {
        if (this._previousLine != undefined) {
            // Two consecutive newlines mean that the child should be closed
            // (the actual newlines are eaten by Gio.DataInputStream)
            // Send a termination message
            if (line == '' && this._previousLine == '') {
                try {
                    this._stdin.write('QUIT\n\n', null);
                } catch(e) { /* ignore broken pipe errors */ }
            } else {
                this._agent.set_password(this._requestId, this._previousLine, line);
                this._previousLine = undefined;
            }
        } else {
            this._previousLine = line;
        }
    },

    _readStdoutOldStyle: function() {
        this._dataStdout.read_line_async(GLib.PRIORITY_DEFAULT, null, Lang.bind(this, function(stream, result) {
            let [line, len] = this._dataStdout.read_line_finish_utf8(result);

            if (line == null) {
                // end of file
                this._stdout.close(null);
                return;
            }

            this._vpnChildProcessLineOldStyle(line);

            // try to read more!
            this._readStdoutOldStyle();
        }));
    },

    _readStdoutNewStyle: function() {
        this._dataStdout.fill_async(-1, GLib.PRIORITY_DEFAULT, null, Lang.bind(this, function(stream, result) {
            let cnt = this._dataStdout.fill_finish(result);

            if (cnt == 0) {
                // end of file
                this._showNewStyleDialog();

                this._stdout.close(null);
                return;
            }

            // Try to read more
            this._dataStdout.set_buffer_size(2 * this._dataStdout.get_buffer_size());
            this._readStdoutNewStyle();
        }));
    },

    _showNewStyleDialog: function() {
        let keyfile = new GLib.KeyFile();
        let contentOverride;

        try {
            let data = this._dataStdout.peek_buffer();
            keyfile.load_from_data(data.toString(), data.length,
                                   GLib.KeyFileFlags.NONE);

            if (keyfile.get_integer(VPN_UI_GROUP, 'Version') != 2)
                throw new Error('Invalid plugin keyfile version, is %d');

            contentOverride = { title: keyfile.get_string(VPN_UI_GROUP, 'Title'),
                                message: keyfile.get_string(VPN_UI_GROUP, 'Description'),
                                secrets: [] };

            let [groups, len] = keyfile.get_groups();
            for (let i = 0; i < groups.length; i++) {
                if (groups[i] == VPN_UI_GROUP)
                    continue;

                let value = keyfile.get_string(groups[i], 'Value');
                let shouldAsk = keyfile.get_boolean(groups[i], 'ShouldAsk');

                if (shouldAsk) {
                    contentOverride.secrets.push({ label: keyfile.get_string(groups[i], 'Label'),
                                                   key: groups[i],
                                                   value: value,
                                                   password: keyfile.get_boolean(groups[i], 'IsSecret')
                                                 });
                } else {
                    if (!value.length) // Ignore empty secrets
                        continue;

                    this._agent.set_password(this._requestId, groups[i], value);
                }
            }
        } catch(e) {
            logError(e, 'error while reading VPN plugin output keyfile');

            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
            return;
        }

        if (contentOverride.secrets.length) {
            // Only show the dialog if we actually have something to ask
            this._shellDialog = new NetworkSecretDialog(this._agent, this._requestId, this._connection, 'vpn', [], contentOverride);
            this._shellDialog.open(global.get_current_time());
        } else {
            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.CONFIRMED);
        }
    },

    _writeConnection: function() {
        let vpnSetting = this._connection.get_setting_vpn();

        try {
            vpnSetting.foreach_data_item(Lang.bind(this, function(key, value) {
                this._stdin.write('DATA_KEY=' + key + '\n', null);
                this._stdin.write('DATA_VAL=' + (value || '') + '\n\n', null);
            }));
            vpnSetting.foreach_secret(Lang.bind(this, function(key, value) {
                this._stdin.write('SECRET_KEY=' + key + '\n', null);
                this._stdin.write('SECRET_VAL=' + (value || '') + '\n\n', null);
            }));
            this._stdin.write('DONE\n\n', null);
        } catch(e) {
            logError(e, 'internal error while writing connection to helper');

            this._agent.respond(this._requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
        }
    },
});

const NetworkAgent = new Lang.Class({
    Name: 'NetworkAgent',

    _init: function() {
        this._native = new Shell.NetworkAgent({ identifier: 'org.gnome.Shell.NetworkAgent' });

        this._dialogs = { };
        this._vpnRequests = { };

        this._native.connect('new-request', Lang.bind(this, this._newRequest));
        this._native.connect('cancel-request', Lang.bind(this, this._cancelRequest));

        this._enabled = false;
    },

    enable: function() {
        this._enabled = true;
    },

    disable: function() {
        let requestId;

        for (requestId in this._dialogs)
            this._dialogs[requestId].cancel();
        this._dialogs = { };

        for (requestId in this._vpnRequests)
            this._vpnRequests[requestId].cancel(true);
        this._vpnRequests = { };

        this._enabled = false;
    },

    _newRequest:  function(agent, requestId, connection, settingName, hints, flags) {
        if (!this._enabled) {
            agent.respond(requestId, Shell.NetworkAgentResponse.USER_CANCELED);
            return;
        }

        if (settingName == 'vpn') {
            this._vpnRequest(requestId, connection, hints, flags);
            return;
        }

        let dialog = new NetworkSecretDialog(agent, requestId, connection, settingName, hints);
        dialog.connect('destroy', Lang.bind(this, function() {
            delete this._dialogs[requestId];
        }));
        this._dialogs[requestId] = dialog;
        dialog.open(global.get_current_time());
    },

    _cancelRequest: function(agent, requestId) {
        if (this._dialogs[requestId]) {
            this._dialogs[requestId].close(global.get_current_time());
            this._dialogs[requestId].destroy();
            delete this._dialogs[requestId];
        } else if (this._vpnRequests[requestId]) {
            this._vpnRequests[requestId].cancel(false);
            delete this._vpnRequests[requestId];
        }
    },

    _vpnRequest: function(requestId, connection, hints, flags) {
        let vpnSetting = connection.get_setting_vpn();
        let serviceType = vpnSetting.service_type;

        this._buildVPNServiceCache();

        let binary = this._vpnBinaries[serviceType];
        if (!binary) {
            log('Invalid VPN service type (cannot find authentication binary)');

            /* cancel the auth process */
            this._native.respond(requestId, Shell.NetworkAgentResponse.INTERNAL_ERROR);
            return;
        }

        this._vpnRequests[requestId] = new VPNRequestHandler(this._native, requestId, binary, serviceType, connection, hints, flags);
    },

    _buildVPNServiceCache: function() {
        if (this._vpnCacheBuilt)
            return;

        this._vpnCacheBuilt = true;
        this._vpnBinaries = { };

        let dir = Gio.file_new_for_path(GLib.build_filenamev([Config.SYSCONFDIR, 'NetworkManager/VPN']));
        try {
            let fileEnum = dir.enumerate_children('standard::name', Gio.FileQueryInfoFlags.NONE, null);
            let info;

            while ((info = fileEnum.next_file(null))) {
                let name = info.get_name();
                if (name.substr(-5) != '.name')
                    continue;

                try {
                    let keyfile = new GLib.KeyFile();
                    keyfile.load_from_file(dir.get_child(name).get_path(), GLib.KeyFileFlags.NONE);
                    let service = keyfile.get_string('VPN Connection', 'service');
                    let binary = keyfile.get_string('GNOME', 'auth-dialog');
                    let externalUIMode = false;
                    try {
                        externalUIMode = keyfile.get_boolean('GNOME', 'supports-external-ui-mode');
                    } catch(e) { } // ignore errors if key does not exist
                    let path = binary;
                    if (!GLib.path_is_absolute(path)) {
                        path = GLib.build_filenamev([Config.LIBEXECDIR, path]);
                    }

                    if (GLib.file_test(path, GLib.FileTest.IS_EXECUTABLE))
                        this._vpnBinaries[service] = { fileName: path, externalUIMode: externalUIMode };
                    else
                        throw new Error('VPN plugin at %s is not executable'.format(path));
                } catch(e) {
                    log('Error \'%s\' while processing VPN keyfile \'%s\''.
                        format(e.message, dir.get_child(name).get_path()));
                    continue;
                }
            }
        } catch(e) {
            logError(e, 'error while enumerating VPN auth helpers');
        }
    }
});
const Component = NetworkAgent;
