/* exported Indicator */
const {Clutter, Gio, GLib, GObject, Shell, St} = imports.gi;

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

const {QuickToggle, SystemIndicator} = imports.ui.quickSettings;
const {loadInterfaceXML} = imports.misc.dbusUtils;

const DBUS_NAME = 'org.freedesktop.background.Monitor';
const DBUS_OBJECT_PATH = '/org/freedesktop/background/monitor';

const BackgroundMonitorIface = loadInterfaceXML('org.freedesktop.background.Monitor');
const BackgroundMonitorProxy = Gio.DBusProxy.makeProxyWrapper(BackgroundMonitorIface);

Gio._promisify(Gio.DBusConnection.prototype, 'call');

var BackgroundAppMenuItem = GObject.registerClass({
    Properties: {
        'app': GObject.ParamSpec.object('app', '', '',
            GObject.ParamFlags.READWRITE,
            Shell.App),
        'instance': GObject.ParamSpec.int64('instance', '', '',
            GObject.ParamFlags.READWRITE,
            -1, GLib.MAXINT64_BIGINT, -1),
        'message': GObject.ParamSpec.string('message', '', '',
            GObject.ParamFlags.READWRITE,
            null),
    },
}, class BackgroundAppMenuItem extends PopupMenu.PopupImageMenuItem {
    _init(app, params = {}) {
        const message = params.message;
        delete params.message;

        const instance = params.instance;
        delete params.instance;

        super._init(app.get_name(), app.get_icon(), {
            activate: false,
            reactive: false,
            ...params,
        });

        this.set({message, instance});

        this.add_style_class_name('background-app-item');
        this.label.add_style_class_name('title');

        this.app = app;

        const box = new St.BoxLayout({
            vertical: true,
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

        this.set_child_above_sibling(this._ornamentLabel, null);

        const closeButton = new St.Button({
            iconName: 'window-close-symbolic',
            styleClass: 'close-button',
            x_expand: true,
            y_expand: false,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(closeButton);

        closeButton.connect('clicked', () => this._quitApp().catch(logError));
    }

    async _quitApp() {
        const appId = this.app.get_id().replace(/\.desktop$/, '');

        try {
            await Gio.DBus.session.call(
                appId,
                `/${appId.replaceAll('.', '/')}`,
                'org.freedesktop.Application',
                'ActivateAction',
                new GLib.Variant('(sava{sv})', ['quit', [], {}]),
                null,
                Gio.DBusCallFlags.NONE,
                -1,
                null);
        } catch (_error) {
            try {
                Util.trySpawn(['flatpak', 'kill', this.instance]);
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

        const nBackgroundApps = backgroundApps?.length ?? 0;
        this.title = nBackgroundApps === 0
            ? _('No Background Apps')
            : ngettext(
                '%d Background App',
                '%d Background Apps',
                nBackgroundApps).format(nBackgroundApps);
        this._listTitle.visible = nBackgroundApps > 0;

        this._appsSection.removeAll();

        if (!backgroundApps)
            return;

        backgroundApps
            .map(backgroundApp => {
                const appId = backgroundApp.app_id.deepUnpack();
                const app = this._appSystem.lookup_app(`${appId}.desktop`);
                const message = backgroundApp.message?.deepUnpack();
                const instance = backgroundApp.instance.deepUnpack();

                return {app, message, instance};
            })
            .sort((a, b) => {
                return a.app.get_name().localeCompare(b.app.get_name());
            })
            .forEach(backgroundApp => {
                const {app, message, instance} = backgroundApp;

                const item = new BackgroundAppMenuItem(app,
                    {instance, message});
                this._appsSection.addMenuItem(item);
            });
    }

    vfunc_clicked() {
        this.menu.open();
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new BackgroundAppsToggle());
    }
});
