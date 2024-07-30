import Gio from 'gi://Gio';

import * as GnomeSession from '../../misc/gnomeSession.js';
import * as MessageTray from '../messageTray.js';

Gio._promisify(Gio.Mount.prototype, 'guess_content_type');

import {loadInterfaceXML} from '../../misc/fileUtils.js';

// GSettings keys
const SETTINGS_SCHEMA = 'org.gnome.desktop.media-handling';
const SETTING_DISABLE_AUTORUN = 'autorun-never';
const SETTING_START_APP = 'autorun-x-content-start-app';
const SETTING_IGNORE = 'autorun-x-content-ignore';
const SETTING_OPEN_FOLDER = 'autorun-x-content-open-folder';

/** @enum {number} */
const AutorunSetting = {
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

    return volume.get_identifier('class') === 'network';
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
        log(`Unable to launch the app ${app.get_name()}: ${e}`);
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

class ContentTypeDiscoverer {
    constructor() {
        this._settings = new Gio.Settings({schema_id: SETTINGS_SCHEMA});
    }

    async guessContentTypes(mount) {
        let autorunEnabled = !this._settings.get_boolean(SETTING_DISABLE_AUTORUN);
        let shouldScan = autorunEnabled && !isMountNonLocal(mount);

        let contentTypes = [];
        if (shouldScan) {
            try {
                contentTypes = await mount.guess_content_type(false, null);
            } catch (e) {
                log(`Unable to guess content types on added mount ${mount.get_name()}: ${e}`);
            }

            if (contentTypes.length === 0) {
                const root = mount.get_root();
                const hotplugSniffer = new HotplugSniffer();
                [contentTypes] = await hotplugSniffer.SniffURIAsync(root.get_uri());
            }
        }

        // we're not interested in win32 software content types here
        contentTypes = contentTypes.filter(
            type => type !== 'x-content/win32-software');

        const apps = [];
        contentTypes.forEach(type => {
            const app = Gio.app_info_get_default_for_type(type, false);

            if (app)
                apps.push(app);
        });

        if (apps.length === 0)
            apps.push(Gio.app_info_get_default_for_type('inode/directory', false));

        return [apps, contentTypes];
    }
}

class AutorunManager {
    constructor() {
        this._session = new GnomeSession.SessionManager();
        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._dispatcher = new AutorunDispatcher(this);
    }

    enable() {
        this._volumeMonitor.connectObject(
            'mount-added', this._onMountAdded.bind(this),
            'mount-removed', this._onMountRemoved.bind(this), this);
    }

    disable() {
        this._volumeMonitor.disconnectObject(this);
    }

    async _onMountAdded(monitor, mount) {
        // don't do anything if our session is not the currently
        // active one
        if (!this._session.SessionIsActive)
            return;

        const discoverer = new ContentTypeDiscoverer();
        const [apps, contentTypes] = await discoverer.guessContentTypes(mount);
        this._dispatcher.addMount(mount, apps, contentTypes);
    }

    _onMountRemoved(monitor, mount) {
        this._dispatcher.removeMount(mount);
    }
}

class AutorunDispatcher {
    constructor(manager) {
        this._manager = manager;
        this._notifications = new Map();
        this._settings = new Gio.Settings({schema_id: SETTINGS_SCHEMA});
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

    _addNotification(mount, apps) {
        // Only show a new notification if there isn't already an existing one
        if (this._notifications.has(mount))
            return;

        const source = MessageTray.getSystemSource();
        /* Translators: %s is the name of a partition on a external drive */
        const title = _('“%s” connected'.format(mount.get_name()));
        const body = _('Disk can now be used');
        const notification = new MessageTray.Notification({
            source,
            title,
            body,
        });
        notification.connect('activate', () => {
            const app = Gio.app_info_get_default_for_type('inode/directory', false);
            startAppForMount(app, mount);
        });
        apps.forEach(app => {
            notification.addAction(
                _('Open with %s').format(app.get_name()),
                () => startAppForMount(app, mount)
            );
        });
        notification.connect('destroy', () => this._notifications.delete(mount));
        this._notifications.set(mount, notification);
        source.addNotification(notification);
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
        if (setting === AutorunSetting.IGNORE)
            return; // return right away

        let success = false;
        let app = null;

        if (setting === AutorunSetting.RUN)
            app = Gio.app_info_get_default_for_type(contentTypes[0], false);
        else if (setting === AutorunSetting.FILES)
            app = Gio.app_info_get_default_for_type('inode/directory', false);

        if (app)
            success = startAppForMount(app, mount);

        // we fallback here also in case the settings did not specify 'ask',
        // but we failed launching the default app or the default file manager
        if (!success)
            this._addNotification(mount, apps);
    }

    removeMount(mount) {
        this._notifications.get(mount)?.destroy();
    }
}

export {AutorunManager as Component};
