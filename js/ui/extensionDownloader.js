// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init, installExtension, uninstallExtension, checkForUpdates */

const { Clutter, Gio, GLib, GObject, Soup } = imports.gi;

const Config = imports.misc.config;
const Dialog = imports.ui.dialog;
const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;

Gio._promisify(Gio.OutputStream.prototype,
    'write_bytes_async', 'write_bytes_finish');
Gio._promisify(Gio.IOStream.prototype,
    'close_async', 'close_finish');
Gio._promisify(Gio.Subprocess.prototype,
    'wait_check_async', 'wait_check_finish');

var REPOSITORY_URL_DOWNLOAD = 'https://extensions.gnome.org/download-extension/%s.shell-extension.zip';
var REPOSITORY_URL_INFO     = 'https://extensions.gnome.org/extension-info/';
var REPOSITORY_URL_UPDATE   = 'https://extensions.gnome.org/update-info/';

let _httpSession;

function installExtension(uuid, invocation) {
    const params = {
        uuid,
        shell_version: Config.PACKAGE_VERSION,
    };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message, () => {
        let info;
        try {
            checkResponse(message);
            info = JSON.parse(message.response_body.data);
        } catch (e) {
            Main.extensionManager.logExtensionError(uuid, e);
            invocation.return_dbus_error(
                'org.gnome.Shell.ExtensionError', e.message);
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
    if (extension.type !== ExtensionUtils.ExtensionType.PER_USER)
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

/**
 * Check return status of reponse
 *
 * @param {Soup.Message} message - an http response
 * @returns {void}
 * @throws
 */
function checkResponse(message) {
    const { statusCode } = message;
    const phrase = Soup.Status.get_phrase(statusCode);
    if (statusCode !== Soup.KnownStatusCode.OK)
        throw new Error('Unexpected response: %s'.format(phrase));
}

/**
 * @param {GLib.Bytes} bytes - archive data
 * @param {Gio.File} dir - target directory
 * @returns {void}
 */
async function extractExtensionArchive(bytes, dir) {
    if (!dir.query_exists(null))
        dir.make_directory_with_parents(null);

    const [file, stream] = Gio.File.new_tmp('XXXXXX.shell-extension.zip');
    await stream.output_stream.write_bytes_async(bytes,
        GLib.PRIORITY_DEFAULT, null);
    stream.close_async(GLib.PRIORITY_DEFAULT, null);

    const unzip = Gio.Subprocess.new(
        ['unzip', '-uod', dir.get_path(), '--', file.get_path()],
        Gio.SubprocessFlags.NONE);
    await unzip.wait_check_async(null);
}

function downloadExtensionUpdate(uuid) {
    if (!Main.extensionManager.updatesSupported)
        return;

    const dir = Gio.File.new_for_path(
        GLib.build_filenamev([global.userdatadir, 'extension-updates', uuid]));

    const params = { shell_version: Config.PACKAGE_VERSION };

    let url = REPOSITORY_URL_DOWNLOAD.format(uuid);
    let message = Soup.form_request_new_from_hash('GET', url, params);

    _httpSession.queue_message(message, async () => {
        try {
            checkResponse(message);

            const bytes = message.response_body.flatten().get_as_bytes();
            await extractExtensionArchive(bytes, dir);
            Main.extensionManager.notifyExtensionUpdate(uuid);
        } catch (e) {
            log('Error while downloading update for extension %s: %s'
                .format(uuid, e.message));
        }
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

    const versionCheck = global.settings.get_boolean(
        'disable-extension-version-validation');
    const params = {
        shell_version: Config.PACKAGE_VERSION,
        disable_version_validation: versionCheck.toString(),
    };

    const uri = Soup.URI.new(REPOSITORY_URL_UPDATE);
    uri.set_query_from_form(params);

    const message = Soup.Message.new_from_uri('POST', uri);
    message.set_request(
        'application/json',
        Soup.MemoryUse.COPY,
        JSON.stringify(metadatas)
    );

    _httpSession.queue_message(message, () => {
        if (message.status_code !== Soup.KnownStatusCode.OK)
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
            label: _('Cancel'),
            action: this._onCancelButtonPressed.bind(this),
            key: Clutter.KEY_Escape,
        }, {
            label: _('Install'),
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
        const params = { shell_version: Config.PACKAGE_VERSION };

        let url = REPOSITORY_URL_DOWNLOAD.format(this._uuid);
        let message = Soup.form_request_new_from_hash('GET', url, params);

        const dir = Gio.File.new_for_path(GLib.build_filenamev(
            [global.userdatadir, 'extensions', this._uuid]));
        let uuid = this._uuid;
        let invocation = this._invocation;

        _httpSession.queue_message(message, async () => {
            try {
                checkResponse(message);

                const bytes = message.response_body.flatten().get_as_bytes();
                await extractExtensionArchive(bytes, dir);

                const extension = Main.extensionManager.createExtensionObject(
                    uuid, dir, ExtensionUtils.ExtensionType.PER_USER);
                Main.extensionManager.loadExtension(extension);
                if (!Main.extensionManager.enableExtension(uuid))
                    throw new Error('Cannot add %s to enabled extensions gsettings key'.format(uuid));

                invocation.return_value(GLib.Variant.new('(s)', ['successful']));
            } catch (e) {
                log('Error while installing %s: %s'.format(uuid, e.message));
                uninstallExtension(uuid);
                invocation.return_dbus_error(
                    'org.gnome.Shell.ExtensionError', e.message);
            }
        });

        this.close();
    }
});

function init() {
    _httpSession = new Soup.Session();
}
