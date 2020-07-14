// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init, installExtension, uninstallExtension, checkForUpdates */

const { Clutter, Gio, GLib, GObject, Soup } = imports.gi;

const Config = imports.misc.config;
const Dialog = imports.ui.dialog;
const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;

var REPOSITORY_URL_DOWNLOAD = 'https://extensions.gnome.org/download-extension/%s.shell-extension.zip';
var REPOSITORY_URL_INFO     = 'https://extensions.gnome.org/extension-info/';
var REPOSITORY_URL_UPDATE   = 'https://extensions.gnome.org/update-info/';

let _httpSession;

function installExtension(uuid, invocation) {
    let params = { uuid,
                   shell_version: Config.PACKAGE_VERSION };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message, () => {
        if (message.status_code != Soup.KnownStatusCode.OK) {
            Main.extensionManager.logExtensionError(uuid, 'downloading info: %d'.format(message.status_code));
            invocation.return_dbus_error('org.gnome.Shell.DownloadInfoError', message.status_code.toString());
            return;
        }

        let info;
        try {
            info = JSON.parse(message.response_body.data);
        } catch (e) {
            Main.extensionManager.logExtensionError(uuid, 'parsing info: %s'.format(e.toString()));
            invocation.return_dbus_error('org.gnome.Shell.ParseInfoError', e.toString());
            return;
        }

        let dialog = new InstallExtensionDialog(uuid, info, invocation);
        dialog.open(global.get_current_time());
    });
}

function uninstallExtension(uuid) {
    let extension = Main.extensionManager.lookup(uuid);
    if (!extension)
        return false;

    // Don't try to uninstall system extensions
    if (extension.type != ExtensionUtils.ExtensionType.PER_USER)
        return false;

    if (!Main.extensionManager.unloadExtension(extension))
        return false;

    FileUtils.recursivelyDeleteDir(extension.dir, true);

    try {
        const updatesDir = Gio.File.new_for_path(GLib.build_filenamev(
            [global.userdatadir, 'extension-updates', extension.uuid]));
        FileUtils.recursivelyDeleteDir(updatesDir, true);
    } catch (e) {
        // not an error
    }

    return true;
}

function gotExtensionZipFile(session, message, uuid, dir, callback, errback) {
    if (message.status_code != Soup.KnownStatusCode.OK) {
        errback('DownloadExtensionError', message.status_code);
        return;
    }

    try {
        if (!dir.query_exists(null))
            dir.make_directory_with_parents(null);
    } catch (e) {
        errback('CreateExtensionDirectoryError', e);
        return;
    }

    let [file, stream] = Gio.File.new_tmp('XXXXXX.shell-extension.zip');
    let contents = message.response_body.flatten().get_as_bytes();
    stream.output_stream.write_bytes(contents, null);
    stream.close(null);
    let [success, pid] = GLib.spawn_async(null,
                                          ['unzip', '-uod', dir.get_path(), '--', file.get_path()],
                                          null,
                                          GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                          null);

    if (!success) {
        errback('ExtractExtensionError');
        return;
    }

    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, (o, status) => {
        GLib.spawn_close_pid(pid);

        if (status != 0)
            errback('ExtractExtensionError');
        else
            callback();
    });
}

function downloadExtensionUpdate(uuid) {
    if (!Main.extensionManager.updatesSupported)
        return;

    let dir = Gio.File.new_for_path(
        GLib.build_filenamev([global.userdatadir, 'extension-updates', uuid]));

    let params = { shell_version: Config.PACKAGE_VERSION };

    let url = REPOSITORY_URL_DOWNLOAD.format(uuid);
    let message = Soup.form_request_new_from_hash('GET', url, params);

    _httpSession.queue_message(message, session => {
        gotExtensionZipFile(session, message, uuid, dir, () => {
            Main.extensionManager.notifyExtensionUpdate(uuid);
        }, (code, msg) => {
            log('Error while downloading update for extension %s: %s (%s)'.format(uuid, code, msg));
        });
    });
}

function checkForUpdates() {
    if (!Main.extensionManager.updatesSupported)
        return;

    let metadatas = {};
    Main.extensionManager.getUuids().forEach(uuid => {
        let extension = Main.extensionManager.lookup(uuid);
        if (extension.type !== ExtensionUtils.ExtensionType.PER_USER)
            return;
        if (extension.hasUpdate)
            return;
        metadatas[uuid] = {
            version: extension.metadata.version,
        };
    });

    if (Object.keys(metadatas).length === 0)
        return; // nothing to update

    let versionCheck = global.settings.get_boolean(
        'disable-extension-version-validation');
    let params = {
        shell_version: Config.PACKAGE_VERSION,
        installed: JSON.stringify(metadatas),
        disable_version_validation: versionCheck.toString(),
    };

    let url = REPOSITORY_URL_UPDATE;
    let message = Soup.form_request_new_from_hash('GET', url, params);
    _httpSession.queue_message(message, () => {
        if (message.status_code != Soup.KnownStatusCode.OK)
            return;

        let operations = JSON.parse(message.response_body.data);
        for (let uuid in operations) {
            let operation = operations[uuid];
            if (operation === 'upgrade' || operation === 'downgrade')
                downloadExtensionUpdate(uuid);
        }
    });
}

var InstallExtensionDialog = GObject.registerClass(
class InstallExtensionDialog extends ModalDialog.ModalDialog {
    _init(uuid, info, invocation) {
        super._init({ styleClass: 'extension-dialog' });

        this._uuid = uuid;
        this._info = info;
        this._invocation = invocation;

        this.setButtons([{
            label: _("Cancel"),
            action: this._onCancelButtonPressed.bind(this),
            key: Clutter.KEY_Escape,
        }, {
            label: _("Install"),
            action: this._onInstallButtonPressed.bind(this),
            default: true,
        }]);

        let content = new Dialog.MessageDialogContent({
            title: _('Install Extension'),
            description: _('Download and install “%s” from extensions.gnome.org?').format(info.name),
        });

        this.contentLayout.add(content);
    }

    _onCancelButtonPressed() {
        this.close();
        this._invocation.return_value(GLib.Variant.new('(s)', ['cancelled']));
    }

    _onInstallButtonPressed() {
        let params = { shell_version: Config.PACKAGE_VERSION };

        let url = REPOSITORY_URL_DOWNLOAD.format(this._uuid);
        let message = Soup.form_request_new_from_hash('GET', url, params);

        let uuid = this._uuid;
        let dir = Gio.File.new_for_path(GLib.build_filenamev([global.userdatadir, 'extensions', uuid]));
        let invocation = this._invocation;
        function errback(code, msg) {
            log('Error while installing %s: %s (%s)'.format(uuid, code, msg));
            invocation.return_dbus_error('org.gnome.Shell.%s'.format(code), msg || '');
        }

        function callback() {
            try {
                let extension = Main.extensionManager.createExtensionObject(uuid, dir, ExtensionUtils.ExtensionType.PER_USER);
                Main.extensionManager.loadExtension(extension);
                if (!Main.extensionManager.enableExtension(uuid))
                    throw new Error('Cannot add %s to enabled extensions gsettings key'.format(uuid));
            } catch (e) {
                uninstallExtension(uuid);
                errback('LoadExtensionError', e);
                return;
            }

            invocation.return_value(GLib.Variant.new('(s)', ['successful']));
        }

        _httpSession.queue_message(message, session => {
            gotExtensionZipFile(session, message, uuid, dir, callback, errback);
        });

        this.close();
    }
});

function init() {
    _httpSession = new Soup.Session({ ssl_use_system_ca_file: true });

    // See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.
    // _httpSession.add_feature(new Soup.ProxyResolverDefault());
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());
}
