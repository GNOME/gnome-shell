import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Soup from 'gi://Soup';

import * as Config from '../misc/config.js';
import * as Dialog from './dialog.js';
import * as ExtensionUtils from '../misc/extensionUtils.js';
import * as FileUtils from '../misc/fileUtils.js';
import * as Main from './main.js';
import * as ModalDialog from './modalDialog.js';

import {ExtensionErrors, ExtensionError} from '../misc/dbusErrors.js';

Gio._promisify(Soup.Session.prototype, 'send_and_read_async');
Gio._promisify(Gio.OutputStream.prototype, 'write_bytes_async');
Gio._promisify(Gio.IOStream.prototype, 'close_async');
Gio._promisify(Gio.Subprocess.prototype, 'wait_check_async');

const REPOSITORY_URL_DOWNLOAD = 'https://extensions.gnome.org/download-extension/%s.shell-extension.zip';
const REPOSITORY_URL_INFO     = 'https://extensions.gnome.org/extension-info/';
const REPOSITORY_URL_UPDATE   = 'https://extensions.gnome.org/update-info/';

let _httpSession;

/**
 * @param {string} uuid - extension uuid
 * @param {Gio.DBusMethodInvocation} invocation - the caller
 * @returns {void}
 */
export async function installExtension(uuid, invocation) {
    if (!global.settings.get_boolean('allow-extension-installation')) {
        invocation.return_error_literal(
            ExtensionErrors, ExtensionError.NOT_ALLOWED,
            'Extension installation is not allowed');
        return;
    }

    const params = {
        uuid,
        shell_version: Config.PACKAGE_VERSION,
    };

    const message = Soup.Message.new_from_encoded_form('GET',
        REPOSITORY_URL_INFO,
        Soup.form_encode_hash(params));

    let info;
    try {
        const bytes = await _httpSession.send_and_read_async(
            message,
            GLib.PRIORITY_DEFAULT,
            null);
        checkResponse(message);
        const decoder = new TextDecoder();
        info = JSON.parse(decoder.decode(bytes.get_data()));
    } catch (e) {
        Main.extensionManager.logExtensionError(uuid, e);
        invocation.return_error_literal(
            ExtensionErrors, ExtensionError.INFO_DOWNLOAD_FAILED,
            e.message);
        return;
    }

    const dialog = new InstallExtensionDialog(uuid, info, invocation);
    dialog.open();
}

/**
 * @param {string} uuid
 */
export function uninstallExtension(uuid) {
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
    } catch {
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
    const {statusCode} = message;
    const phrase = Soup.Status.get_phrase(statusCode);
    if (statusCode !== Soup.Status.OK)
        throw new Error(`Unexpected response: ${phrase}`);
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

    const schemasPath = dir.get_child('schemas');

    try {
        const info = await schemasPath.query_info_async(
            Gio.FILE_ATTRIBUTE_STANDARD_TYPE,
            Gio.FileQueryInfoFlags.NONE,
            GLib.PRIORITY_DEFAULT,
            null);

        if (info.get_file_type() !== Gio.FileType.DIRECTORY)
            throw new Error('schemas is not a directory');
    } catch (e) {
        if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
            console.warn(`Error while looking for schema for extension ${dir.get_basename()}: ${e.message}`);
        return;
    }

    const compileSchema = Gio.Subprocess.new(
        ['glib-compile-schemas', '--strict', schemasPath.get_path()],
        Gio.SubprocessFlags.NONE);

    try {
        await compileSchema.wait_check_async(null);
    } catch (e) {
        log(`Error while compiling schema for extension ${dir.get_basename()}: (${e.message})`);
    }
}

/**
 * @param {string} uuid - extension uuid
 * @returns {void}
 */
export async function downloadExtensionUpdate(uuid) {
    if (!Main.extensionManager.updatesSupported)
        return;

    const dir = Gio.File.new_for_path(
        GLib.build_filenamev([global.userdatadir, 'extension-updates', uuid]));

    const params = {shell_version: Config.PACKAGE_VERSION};
    const message = Soup.Message.new_from_encoded_form('GET',
        REPOSITORY_URL_DOWNLOAD.format(uuid),
        Soup.form_encode_hash(params));

    try {
        const bytes = await _httpSession.send_and_read_async(
            message,
            GLib.PRIORITY_DEFAULT,
            null);
        checkResponse(message);

        await extractExtensionArchive(bytes, dir);
        Main.extensionManager.notifyExtensionUpdate(uuid);
    } catch (e) {
        log(`Error while downloading update for extension ${uuid}: (${e.message})`);
    }
}

/**
 * Check extensions.gnome.org for updates
 *
 * @returns {void}
 */
export async function checkForUpdates() {
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
        disable_version_validation: `${versionCheck}`,
    };
    const requestBody = new GLib.Bytes(JSON.stringify(metadatas));

    const message = Soup.Message.new('POST',
        `${REPOSITORY_URL_UPDATE}?${Soup.form_encode_hash(params)}`);
    message.set_request_body_from_bytes('application/json', requestBody);

    let json;
    try {
        const bytes = await _httpSession.send_and_read_async(
            message,
            GLib.PRIORITY_DEFAULT,
            null);
        checkResponse(message);
        json = new TextDecoder().decode(bytes.get_data());
    } catch (e) {
        log(`Update check failed: ${e.message}`);
        return;
    }

    const operations = JSON.parse(json);
    const updates = [];
    for (const uuid in operations) {
        const operation = operations[uuid];
        if (operation === 'upgrade' || operation === 'downgrade')
            updates.push(uuid);
    }

    try {
        await Promise.allSettled(
            updates.map(uuid => downloadExtensionUpdate(uuid)));
    } catch (e) {
        log(`Some extension updates failed to download: ${e.message}`);
    }
}

class ExtractError extends Error {
    get name() {
        return 'ExtractError';
    }
}

class EnableError extends Error {
    get name() {
        return 'EnableError';
    }
}

const InstallExtensionDialog = GObject.registerClass(
class InstallExtensionDialog extends ModalDialog.ModalDialog {
    _init(uuid, info, invocation) {
        super._init({styleClass: 'extension-dialog'});

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

        this.contentLayout.add_child(content);
    }

    _onCancelButtonPressed() {
        this.close();
        this._invocation.return_value(GLib.Variant.new('(s)', ['cancelled']));
    }

    async _onInstallButtonPressed() {
        this.close();

        const params = {shell_version: Config.PACKAGE_VERSION};
        const message = Soup.Message.new_from_encoded_form('GET',
            REPOSITORY_URL_DOWNLOAD.format(this._uuid),
            Soup.form_encode_hash(params));

        const dir = Gio.File.new_for_path(
            GLib.build_filenamev([global.userdatadir, 'extensions', this._uuid]));

        try {
            const bytes = await _httpSession.send_and_read_async(
                message,
                GLib.PRIORITY_DEFAULT,
                null);
            checkResponse(message);

            try {
                await extractExtensionArchive(bytes, dir);
            } catch (e) {
                throw new ExtractError(e.message);
            }

            const extension = Main.extensionManager.createExtensionObject(
                this._uuid, dir, ExtensionUtils.ExtensionType.PER_USER);
            Main.extensionManager.loadExtension(extension);
            if (!Main.extensionManager.enableExtension(this._uuid))
                throw new EnableError(`Cannot enable ${this._uuid}`);

            this._invocation.return_value(new GLib.Variant('(s)', ['successful']));
        } catch (e) {
            let code;
            if (e instanceof ExtractError)
                code = ExtensionError.EXTRACT_FAILED;
            else if (e instanceof EnableError)
                code = ExtensionError.ENABLE_FAILED;
            else
                code = ExtensionError.DOWNLOAD_FAILED;

            log(`Error while installing ${this._uuid}: ${e.message}`);
            this._invocation.return_error_literal(
                ExtensionErrors, code, e.message);
        }
    }
});

export function init() {
    _httpSession = new Soup.Session();
}
