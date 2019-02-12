// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init, installExtension, uninstallExtension,
            checkForUpdates, updateExtension */

const { Clutter, Gio, GLib, GObject, Soup } = imports.gi;

const Config = imports.misc.config;
const Dialog = imports.ui.dialog;
const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;

var REPOSITORY_URL_BASE = 'https://extensions.gnome.org';
var REPOSITORY_URL_DOWNLOAD = `${REPOSITORY_URL_BASE}/download-extension/%s.shell-extension.zip`;
var REPOSITORY_URL_INFO     = `${REPOSITORY_URL_BASE}/extension-info/`;
var REPOSITORY_URL_UPDATE   = `${REPOSITORY_URL_BASE}/update-info/`;

let _httpSession;

function installExtension(uuid, invocation) {
    let params = { uuid: uuid,
                   shell_version: Config.PACKAGE_VERSION };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message, (session, message) => {
        if (message.status_code != Soup.KnownStatusCode.OK) {
            Main.extensionManager.logExtensionError(uuid, `downloading info: ${message.status_code}`);
            invocation.return_dbus_error('org.gnome.Shell.DownloadInfoError', message.status_code.toString());
            return;
        }

        let info;
        try {
            info = JSON.parse(message.response_body.data);
        } catch (e) {
            Main.extensionManager.logExtensionError(uuid, `parsing info: ${e}`);
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

    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, (pid, status) => {
        GLib.spawn_close_pid(pid);

        if (status != 0)
            errback('ExtractExtensionError');
        else
            callback();
    });
}

function updateExtension(uuid) {
    // This gets a bit tricky. We want the update to be seamless -
    // if we have any error during downloading or extracting, we
    // want to not unload the current version.

    let oldExtensionTmpDir = GLib.Dir.make_tmp('XXXXXX-shell-extension');
    let newExtensionTmpDir = GLib.Dir.make_tmp('XXXXXX-shell-extension');

    let params = { shell_version: Config.PACKAGE_VERSION };

    let url = REPOSITORY_URL_DOWNLOAD.format(uuid);
    let message = Soup.form_request_new_from_hash('GET', url, params);

    _httpSession.queue_message(message, (session, message) => {
        gotExtensionZipFile(session, message, uuid, newExtensionTmpDir, () => {
            let oldExtension = Main.extensionManager.lookup(uuid);
            let extensionDir = oldExtension.dir;

            if (!Main.extensionManager.unloadExtension(oldExtension))
                return;

            FileUtils.recursivelyMoveDir(extensionDir, oldExtensionTmpDir);
            FileUtils.recursivelyMoveDir(newExtensionTmpDir, extensionDir);

            let extension = null;

            try {
                extension = Main.extensionManager.createExtensionObject(uuid, extensionDir, ExtensionUtils.ExtensionType.PER_USER);
                Main.extensionManager.loadExtension(extension);
            } catch (e) {
                if (extension)
                    Main.extensionManager.unloadExtension(extension);

                logError(e, 'Error loading extension %s'.format(uuid));

                FileUtils.recursivelyDeleteDir(extensionDir, false);
                FileUtils.recursivelyMoveDir(oldExtensionTmpDir, extensionDir);

                // Restore what was there before. We can't do much if we
                // fail here.
                Main.extensionManager.loadExtension(oldExtension);
                return;
            }

            FileUtils.recursivelyDeleteDir(oldExtensionTmpDir, true);
        }, (code, message) => {
            log('Error while updating extension %s: %s (%s)'.format(uuid, code, message ? message : ''));
        });
    });
}

function checkForUpdates() {
    let metadatas = {};
    Main.extensionManager.getUuids().forEach(uuid => {
        metadatas[uuid] = Main.extensionManager.extensions[uuid].metadata;
    });

    let params = { shell_version: Config.PACKAGE_VERSION,
                   installed: JSON.stringify(metadatas) };

    let url = REPOSITORY_URL_UPDATE;
    let message = Soup.form_request_new_from_hash('GET', url, params);
    _httpSession.queue_message(message, (session, message) => {
        if (message.status_code != Soup.KnownStatusCode.OK)
            return;

        let operations = JSON.parse(message.response_body.data);
        for (let uuid in operations) {
            let operation = operations[uuid];
            if (operation == 'blacklist')
                uninstallExtension(uuid);
            else if (operation == 'upgrade' || operation == 'downgrade')
                updateExtension(uuid);
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
            key: Clutter.Escape,
        }, {
            label: _("Install"),
            action: this._onInstallButtonPressed.bind(this),
            default: true,
        }]);

        let content = new Dialog.MessageDialogContent({
            title: _("Download and install “%s” from extensions.gnome.org?").format(info.name),
            icon: new Gio.FileIcon({
                file: Gio.File.new_for_uri(`${REPOSITORY_URL_BASE}${info.icon}`)
            })
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
        function errback(code, message) {
            let msg = message ? message.toString() : '';
            log('Error while installing %s: %s (%s)'.format(uuid, code, msg));
            invocation.return_dbus_error(`org.gnome.Shell.${code}`, msg);
        }

        function callback() {
            try {
                let extension = Main.extensionManager.createExtensionObject(uuid, dir, ExtensionUtils.ExtensionType.PER_USER);
                Main.extensionManager.loadExtension(extension);
                if (!Main.extensionManager.enableExtension(uuid))
                    throw new Error(`Cannot add ${uuid} to enabled extensions gsettings key`);
            } catch (e) {
                uninstallExtension(uuid);
                errback('LoadExtensionError', e);
                return;
            }

            invocation.return_value(GLib.Variant.new('(s)', ['successful']));
        }

        _httpSession.queue_message(message, (session, message) => {
            gotExtensionZipFile(session, message, uuid, dir, callback, errback);
        });

        this.close();
    }
});

function init() {
    _httpSession = new Soup.SessionAsync({ ssl_use_system_ca_file: true });

    // See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.
    // _httpSession.add_feature(new Soup.ProxyResolverDefault());
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());
}
