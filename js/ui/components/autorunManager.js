// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Component */

const { Clutter, Gio, GObject, St } = imports.gi;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const { loadInterfaceXML } = imports.misc.fileUtils;

// GSettings keys
const SETTINGS_SCHEMA = 'org.gnome.desktop.media-handling';
const SETTING_DISABLE_AUTORUN = 'autorun-never';
const SETTING_START_APP = 'autorun-x-content-start-app';
const SETTING_IGNORE = 'autorun-x-content-ignore';
const SETTING_OPEN_FOLDER = 'autorun-x-content-open-folder';

var AutorunSetting = {
    RUN: 0,
    IGNORE: 1,
    FILES: 2,
    ASK: 3,
};

// misc utils
function shouldAutorunMount(mount) {
    let root = mount.get_root();
    let volume = mount.get_volume();

    if (!volume || !volume.allowAutorun)
        return false;

    if (root.is_native() && isMountRootHidden(root))
        return false;

    return true;
}

function isMountRootHidden(root) {
    let path = root.get_path();

    // skip any mounts in hidden directory hierarchies
    return path.includes('/.');
}

function isMountNonLocal(mount) {
    // If the mount doesn't have an associated volume, that means it's
    // an uninteresting filesystem. Most devices that we care about will
    // have a mount, like media players and USB sticks.
    let volume = mount.get_volume();
    if (volume == null)
        return true;

    return volume.get_identifier("class") == "network";
}

function startAppForMount(app, mount) {
    let files = [];
    let root = mount.get_root();
    let retval = false;

    files.push(root);

    try {
        retval = app.launch(files,
                            global.create_app_launch_context(0, -1));
    } catch (e) {
        log('Unable to launch the application %s: %s'.format(app.get_name(), e.toString()));
    }

    return retval;
}

const HotplugSnifferIface = loadInterfaceXML('org.gnome.Shell.HotplugSniffer');
const HotplugSnifferProxy = Gio.DBusProxy.makeProxyWrapper(HotplugSnifferIface);
function HotplugSniffer() {
    return new HotplugSnifferProxy(Gio.DBus.session,
                                   'org.gnome.Shell.HotplugSniffer',
                                   '/org/gnome/Shell/HotplugSniffer');
}

var ContentTypeDiscoverer = class {
    constructor(callback) {
        this._callback = callback;
        this._settings = new Gio.Settings({ schema_id: SETTINGS_SCHEMA });
    }

    guessContentTypes(mount) {
        let autorunEnabled = !this._settings.get_boolean(SETTING_DISABLE_AUTORUN);
        let shouldScan = autorunEnabled && !isMountNonLocal(mount);

        if (shouldScan) {
            // guess mount's content types using GIO
            mount.guess_content_type(false, null,
                                     this._onContentTypeGuessed.bind(this));
        } else {
            this._emitCallback(mount, []);
        }
    }

    _onContentTypeGuessed(mount, res) {
        let contentTypes = [];

        try {
            contentTypes = mount.guess_content_type_finish(res);
        } catch (e) {
            log('Unable to guess content types on added mount %s: %s'.format(mount.get_name(), e.toString()));
        }

        if (contentTypes.length) {
            this._emitCallback(mount, contentTypes);
        } else {
            let root = mount.get_root();

            let hotplugSniffer = new HotplugSniffer();
            hotplugSniffer.SniffURIRemote(root.get_uri(),
                result => {
                    [contentTypes] = result;
                    this._emitCallback(mount, contentTypes);
                });
        }
    }

    _emitCallback(mount, contentTypes = []) {
        // we're not interested in win32 software content types here
        contentTypes = contentTypes.filter(
            type => type != 'x-content/win32-software'
        );

        let apps = [];
        contentTypes.forEach(type => {
            let app = Gio.app_info_get_default_for_type(type, false);

            if (app)
                apps.push(app);
        });

        if (apps.length == 0)
            apps.push(Gio.app_info_get_default_for_type('inode/directory', false));

        this._callback(mount, apps, contentTypes);
    }
};

var AutorunManager = class {
    constructor() {
        this._session = new GnomeSession.SessionManager();
        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._dispatcher = new AutorunDispatcher(this);
    }

    enable() {
        this._mountAddedId = this._volumeMonitor.connect('mount-added', this._onMountAdded.bind(this));
        this._mountRemovedId = this._volumeMonitor.connect('mount-removed', this._onMountRemoved.bind(this));
    }

    disable() {
        this._volumeMonitor.disconnect(this._mountAddedId);
        this._volumeMonitor.disconnect(this._mountRemovedId);
    }

    _onMountAdded(monitor, mount) {
        // don't do anything if our session is not the currently
        // active one
        if (!this._session.SessionIsActive)
            return;

        let discoverer = new ContentTypeDiscoverer((m, apps, contentTypes) => {
            this._dispatcher.addMount(mount, apps, contentTypes);
        });
        discoverer.guessContentTypes(mount);
    }

    _onMountRemoved(monitor, mount) {
        this._dispatcher.removeMount(mount);
    }
};

var AutorunDispatcher = class {
    constructor(manager) {
        this._manager = manager;
        this._sources = [];
        this._settings = new Gio.Settings({ schema_id: SETTINGS_SCHEMA });
    }

    _getAutorunSettingForType(contentType) {
        let runApp = this._settings.get_strv(SETTING_START_APP);
        if (runApp.includes(contentType))
            return AutorunSetting.RUN;

        let ignore = this._settings.get_strv(SETTING_IGNORE);
        if (ignore.includes(contentType))
            return AutorunSetting.IGNORE;

        let openFiles = this._settings.get_strv(SETTING_OPEN_FOLDER);
        if (openFiles.includes(contentType))
            return AutorunSetting.FILES;

        return AutorunSetting.ASK;
    }

    _getSourceForMount(mount) {
        let filtered = this._sources.filter(source => source.mount == mount);

        // we always make sure not to add two sources for the same
        // mount in addMount(), so it's safe to assume filtered.length
        // is always either 1 or 0.
        if (filtered.length == 1)
            return filtered[0];

        return null;
    }

    _addSource(mount, apps) {
        // if we already have a source showing for this
        // mount, return
        if (this._getSourceForMount(mount))
            return;

        // add a new source
        this._sources.push(new AutorunSource(this._manager, mount, apps));
    }

    addMount(mount, apps, contentTypes) {
        // if autorun is disabled globally, return
        if (this._settings.get_boolean(SETTING_DISABLE_AUTORUN))
            return;

        // if the mount doesn't want to be autorun, return
        if (!shouldAutorunMount(mount))
            return;

        let setting;
        if (contentTypes.length > 0)
            setting = this._getAutorunSettingForType(contentTypes[0]);
        else
            setting = AutorunSetting.ASK;

        // check at the settings for the first content type
        // to see whether we should ask
        if (setting == AutorunSetting.IGNORE)
            return; // return right away

        let success = false;
        let app = null;

        if (setting == AutorunSetting.RUN)
            app = Gio.app_info_get_default_for_type(contentTypes[0], false);
        else if (setting == AutorunSetting.FILES)
            app = Gio.app_info_get_default_for_type('inode/directory', false);

        if (app)
            success = startAppForMount(app, mount);

        // we fallback here also in case the settings did not specify 'ask',
        // but we failed launching the default app or the default file manager
        if (!success)
            this._addSource(mount, apps);
    }

    removeMount(mount) {
        let source = this._getSourceForMount(mount);

        // if we aren't tracking this mount, don't do anything
        if (!source)
            return;

        // destroy the notification source
        source.destroy();
    }
};

var AutorunSource = GObject.registerClass(
class AutorunSource extends MessageTray.Source {
    _init(manager, mount, apps) {
        super._init(mount.get_name());

        this._manager = manager;
        this.mount = mount;
        this.apps = apps;

        this._notification = new AutorunNotification(this._manager, this);

        // add ourselves as a source, and popup the notification
        Main.messageTray.add(this);
        this.showNotification(this._notification);
    }

    getIcon() {
        return this.mount.get_icon();
    }

    _createPolicy() {
        return new MessageTray.NotificationApplicationPolicy('org.gnome.Nautilus');
    }
});

var AutorunNotification = GObject.registerClass(
class AutorunNotification extends MessageTray.Notification {
    _init(manager, source) {
        super._init(source, source.title);

        this._manager = manager;
        this._mount = source.mount;
    }

    createBanner() {
        let banner = new MessageTray.NotificationBanner(this);

        this.source.apps.forEach(app => {
            let actor = this._buttonForApp(app);

            if (actor)
                banner.addButton(actor);
        });

        return banner;
    }

    _buttonForApp(app) {
        let box = new St.BoxLayout({
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
        });
        let icon = new St.Icon({ gicon: app.get_icon(),
                                 style_class: 'hotplug-notification-item-icon' });
        box.add(icon);

        let label = new St.Bin({
            child: new St.Label({
                text: _("Open with %s").format(app.get_name()),
                y_align: Clutter.ActorAlign.CENTER,
            }),
        });
        box.add(label);

        let button = new St.Button({ child: box,
                                     x_expand: true,
                                     button_mask: St.ButtonMask.ONE,
                                     style_class: 'hotplug-notification-item button' });

        button.connect('clicked', () => {
            startAppForMount(app, this._mount);
            this.destroy();
        });

        return button;
    }

    activate() {
        super.activate();

        let app = Gio.app_info_get_default_for_type('inode/directory', false);
        startAppForMount(app, this._mount);
    }
});

var Component = AutorunManager;
