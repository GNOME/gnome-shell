import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from '../main.js';
import * as PopupMenu from '../popupMenu.js';
import * as Util from '../../misc/util.js';

import {Spinner} from '../animation.js';
import {QuickToggle, SystemIndicator} from '../quickSettings.js';
import {loadInterfaceXML} from '../../misc/dbusUtils.js';

const DBUS_NAME = 'org.freedesktop.background.Monitor';
const DBUS_OBJECT_PATH = '/org/freedesktop/background/monitor';

const SPINNER_TIMEOUT = 5; // seconds

const BackgroundMonitorIface = loadInterfaceXML('org.freedesktop.background.Monitor');
const BackgroundMonitorProxy = Gio.DBusProxy.makeProxyWrapper(BackgroundMonitorIface);

Gio._promisify(Gio.DBusConnection.prototype, 'call');

const BackgroundAppMenuItem = GObject.registerClass({
    Properties: {
        'app': GObject.ParamSpec.object('app', null, null,
            GObject.ParamFlags.READWRITE,
            Shell.App),
        'message': GObject.ParamSpec.string('message', null, null,
            GObject.ParamFlags.READWRITE,
            null),
    },
}, class BackgroundAppMenuItem extends PopupMenu.PopupImageMenuItem {
    _init(app, params = {}) {
        const message = params.message;
        delete params.message;

        super._init(app.get_name(), app.get_icon(), {
            ...params,
        });

        this.set({message});

        this.add_style_class_name('background-app-item');
        this.label.add_style_class_name('title');

        this.app = app;

        const box = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(box);

        this.remove_child(this.label);
        box.add_child(this.label);

        const messageLabel = new St.Label({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
            style_class: 'subtitle',
        });
        box.add_child(messageLabel);

        this.bind_property('message',
            messageLabel, 'text', GObject.BindingFlags.SYNC_CREATE);
        this.bind_property_full('message',
            messageLabel, 'visible', GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source !== null],
            null);

        this.set_child_above_sibling(this._ornamentIcon, null);

        this._spinner = new Spinner(16, {hideOnStop: true});
        this._spinner.add_style_class_name('spinner');
        this.add_child(this._spinner);

        const closeButton = new St.Button({
            iconName: 'window-close-symbolic',
            styleClass: 'icon-button',
            x_expand: true,
            y_expand: false,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(closeButton);

        this._spinner.bind_property('visible',
            closeButton, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN);

        closeButton.connect('clicked', () => this._quitApp().catch(logError));

        this.connect('activate', () => {
            Main.overview.hide();
            Main.panel.closeQuickSettings();
            this.app.activate();
        });

        this.connect('destroy', () => this._onDestroy());
    }

    _onDestroy() {
        if (this._spinnerTimeoutId)
            GLib.source_remove(this._spinnerTimeoutId);
        delete this._spinnerTimeoutId;
    }

    async _quitApp() {
        this._spinner.play();
        this._spinnerTimeoutId =
            GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, SPINNER_TIMEOUT,
                () => {
                    // Assume the quit request has failed, stop the spinner
                    this._spinner.stop();
                    delete this._spinnerTimeoutId;
                    return GLib.SOURCE_REMOVE;
                });

        try {
            await this.app.activate_action('quit', null, 0, -1, null);
        } catch {
            try {
                const appId = this.app.get_id().replace(/\.desktop$/, '');
                Util.trySpawn(['flatpak', 'kill', appId]);
            } catch (pidError) {
                logError(pidError, 'Failed to kill application');
            }
        }
    }
});

const BackgroundAppsToggle = GObject.registerClass(
class BackgroundAppsToggle extends QuickToggle {
    _init() {
        super._init({
            visible: false,
            hasMenu: true,
            // The background apps toggle looks like a flat menu, but doesn't
            // have a separate menu button. Fake it with an arrow icon.
            iconName: 'go-next-symbolic',
        });

        this.add_style_class_name('background-apps-quick-toggle');

        this._box.set_child_above_sibling(this._icon, null);

        this._appSystem = Shell.AppSystem.get_default();

        this.menu.setHeader(
            'background-app-ghost-symbolic',
            C_('title', 'Background Apps'));

        new BackgroundMonitorProxy(
            Gio.DBus.session,
            DBUS_NAME,
            DBUS_OBJECT_PATH,
            proxy => {
                this._proxy = proxy;
                proxy?.connect('g-properties-changed', () => this._sync());
                this._sync();
            },
            null,
            Gio.DBusProxyFlags.DO_NOT_AUTO_START);

        this._listTitle = new PopupMenu.PopupMenuItem(
            _('Apps known to be running without a window'),
            {reactive: false});
        this._listTitle.label.clutter_text.set({
            line_wrap: true,
        });
        this.menu.addMenuItem(this._listTitle);

        this._appsSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._appsSection);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_('App Settings'),
            'gnome-applications-panel.desktop');

        this.connect('popup-menu', () => this.menu.open());

        this.menu.connect('open-state-changed', () => this._syncVisibility());
        Main.sessionMode.connect('updated', () => this._syncVisibility());
    }

    _syncVisibility() {
        const {isLocked} = Main.sessionMode;
        const nBackgroundApps = this._proxy?.BackgroundApps?.length;
        // We cannot hide the quick toggle while the menu is open, otherwise
        // the menu position goes bogus. We can't show it in locked sessions
        // either
        this.visible = !isLocked && (this.menu.isOpen || nBackgroundApps > 0);
    }

    _sync() {
        this._syncVisibility();

        if (!this._proxy)
            return;

        const {BackgroundApps: backgroundApps} = this._proxy;

        this._appsSection.removeAll();

        const items = new Map();
        (backgroundApps ?? [])
            .map(backgroundApp => {
                const appId = backgroundApp.app_id.deepUnpack();
                const app = this._appSystem.lookup_app(`${appId}.desktop`);
                const message = backgroundApp.message?.deepUnpack();

                return {app, message};
            })
            .filter(item => !!item.app)
            .sort((a, b) => {
                return a.app.get_name().localeCompare(b.app.get_name());
            })
            .forEach(backgroundApp => {
                const {app, message} = backgroundApp;

                let item = items.get(app);
                if (!item) {
                    item = new BackgroundAppMenuItem(app);
                    items.set(app, item);
                    this._appsSection.addMenuItem(item);
                }

                if (message)
                    item.set({message});
            });

        const nBackgroundApps = items.size;
        this.title = nBackgroundApps === 0
            ? _('No Background Apps')
            : ngettext(
                '%d Background App',
                '%d Background Apps',
                nBackgroundApps).format(nBackgroundApps);
        this._listTitle.visible = nBackgroundApps > 0;
    }

    vfunc_clicked() {
        this.menu.open();
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new BackgroundAppsToggle());
    }
});
