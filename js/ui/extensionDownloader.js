// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Soup = imports.gi.Soup;
const St = imports.gi.St;

const Config = imports.misc.config;
const ExtensionUtils = imports.misc.extensionUtils;
const ExtensionSystem = imports.ui.extensionSystem;
const FileUtils = imports.misc.fileUtils;
const ModalDialog = imports.ui.modalDialog;

const _signals = ExtensionSystem._signals;

const REPOSITORY_URL_BASE = 'https://extensions.gnome.org';
const REPOSITORY_URL_DOWNLOAD = REPOSITORY_URL_BASE + '/download-extension/%s.shell-extension.zip';
const REPOSITORY_URL_INFO =     REPOSITORY_URL_BASE + '/extension-info/';

let _httpSession;

function installExtensionFromUUID(uuid) {
    let params = { uuid: uuid,
                   shell_version: Config.PACKAGE_VERSION };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message,
                               function(session, message) {
                                   let info = JSON.parse(message.response_body.data);
                                   let dialog = new InstallExtensionDialog(uuid, info);
                                   dialog.open(global.get_current_time());
                               });
}

function uninstallExtensionFromUUID(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return false;

    // Don't try to uninstall system extensions
    if (extension.type != ExtensionUtils.ExtensionType.PER_USER)
        return false;

    if (!ExtensionSystem.unloadExtension(uuid))
        return false;

    FileUtils.recursivelyDeleteDir(extension.dir);
    return true;
}

function gotExtensionZipFile(session, message, uuid) {
    if (message.status_code != Soup.KnownStatusCode.OK) {
        logExtensionError(uuid, 'downloading extension: ' + message.status_code);
        return;
    }

    let dir = Gio.File.new_for_path(GLib.build_filenamev([global.userdatadir, 'extensions', uuid]));
    try {
        if (!dir.query_exists(null))
            dir.make_directory_with_parents(null);
    } catch (e) {
        logExtensionError('Could not create extension directory');
    }

    let [file, stream] = Gio.File.new_tmp('XXXXXX.shell-extension.zip');
    let contents = message.response_body.flatten().as_bytes();
    stream.output_stream.write_bytes(contents, null);
    stream.close(null);
    let [success, pid] = GLib.spawn_async(null,
                                          ['unzip', '-uod', dir.get_path(), '--', file.get_path()],
                                          null,
                                          GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                          null);

    if (!success) {
        logExtensionError(uuid, 'extract: could not extract');
        return;
    }

    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, function(pid, status) {
        GLib.spawn_close_pid(pid);

        // Add extension to 'enabled-extensions' for the user, always...
        let enabledExtensions = global.settings.get_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY);
        if (enabledExtensions.indexOf(uuid) == -1) {
            enabledExtensions.push(uuid);
            global.settings.set_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        ExtensionSystem.loadExtension(dir, ExtensionUtils.ExtensionType.PER_USER, true);
    });
}

const InstallExtensionDialog = new Lang.Class({
    Name: 'InstallExtensionDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(uuid, info) {
        this.parent({ styleClass: 'extension-dialog' });

        this._uuid = uuid;
        this._info = info;

        this.setButtons([{ label: _("Cancel"),
                           action: Lang.bind(this, this._onCancelButtonPressed),
                           key:    Clutter.Escape
                         },
                         { label:  _("Install"),
                           action: Lang.bind(this, this._onInstallButtonPressed)
                         }]);

        let message = _("Download and install '%s' from extensions.gnome.org?").format(info.name);

        let box = new St.BoxLayout();
        this.contentLayout.add(box);

        let gicon = new Gio.FileIcon({ file: Gio.File.new_for_uri(REPOSITORY_URL_BASE + info.icon) })
        let icon = new St.Icon({ gicon: gicon });
        box.add(icon);

        let label = new St.Label({ text: message });
        box.add(label);
    },

    _onCancelButtonPressed: function(button, event) {
        this.close(global.get_current_time());

        // Even though the extension is already "uninstalled", send through
        // a state-changed signal for any users who want to know if the install
        // went through correctly -- using proper async DBus would block more
        // traditional clients like the plugin
        let meta = { uuid: this._uuid,
                     state: ExtensionSystem.ExtensionState.UNINSTALLED,
                     error: '' };

        _signals.emit('extension-state-changed', meta);
    },

    _onInstallButtonPressed: function(button, event) {
        let state = { uuid: this._uuid,
                      state: ExtensionSystem.ExtensionState.DOWNLOADING,
                      error: '' };

        _signals.emit('extension-state-changed', state);

        let params = { shell_version: Config.PACKAGE_VERSION };

        let url = REPOSITORY_URL_DOWNLOAD.format(this._uuid);
        let message = Soup.form_request_new_from_hash('GET', url, params);

        _httpSession.queue_message(message,
                                   Lang.bind(this, function(session, message) {
                                       gotExtensionZipFile(session, message, this._uuid);
                                   }));

        this.close(global.get_current_time());
    }
});

function init() {
    _httpSession = new Soup.SessionAsync({ ssl_use_system_ca_file: true });

    // See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.
    // _httpSession.add_feature(new Soup.ProxyResolverDefault());
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());
}
