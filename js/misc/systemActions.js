const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const GObject = imports.gi.GObject;

const GnomeSession = imports.misc.gnomeSession;
const LoginManager = imports.misc.loginManager;
const Main = imports.ui.main;

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const DISABLE_USER_SWITCH_KEY = 'disable-user-switching';
const DISABLE_LOCK_SCREEN_KEY = 'disable-lock-screen';
const DISABLE_LOG_OUT_KEY = 'disable-log-out';
const DISABLE_RESTART_KEY = 'disable-restart-buttons';
const ALWAYS_SHOW_LOG_OUT_KEY = 'always-show-log-out';

const SENSOR_BUS_NAME = 'net.hadess.SensorProxy';
const SENSOR_OBJECT_PATH = '/net/hadess/SensorProxy';

const SensorProxyInterface = '<node> \
<interface name="net.hadess.SensorProxy"> \
  <property name="HasAccelerometer" type="b" access="read"/> \
</interface> \
</node>';

const SensorProxy = Gio.DBusProxy.makeProxyWrapper(SensorProxyInterface);

let _singleton = null;

function getDefault() {
    if (_singleton == null)
        _singleton = new SystemActions();

    return _singleton;
}

const SystemActions = new Lang.Class({
    Name: 'SystemActions',
    Extends: GObject.Object,
    Properties: {
        'can-power-off': GObject.ParamSpec.boolean('can-power-off',
                                                   'can-power-off',
                                                   'can-power-off',
                                                   GObject.ParamFlags.READABLE,
                                                   false),
        'can-suspend': GObject.ParamSpec.boolean('can-suspend',
                                                 'can-suspend',
                                                 'can-suspend',
                                                 GObject.ParamFlags.READABLE,
                                                 false),
        'can-lock-screen': GObject.ParamSpec.boolean('can-lock-screen',
                                                     'can-lock-screen',
                                                     'can-lock-screen',
                                                     GObject.ParamFlags.READABLE,
                                                     false),
        'can-switch-user': GObject.ParamSpec.boolean('can-switch-user',
                                                     'can-switch-user',
                                                     'can-switch-user',
                                                     GObject.ParamFlags.READABLE,
                                                     false),
        'can-logout': GObject.ParamSpec.boolean('can-logout',
                                                'can-logout',
                                                'can-logout',
                                                GObject.ParamFlags.READABLE,
                                                false),
        'can-lock-orientation': GObject.ParamSpec.boolean('can-lock-orientation',
                                                          'can-lock-orientation',
                                                          'can-lock-orientation',
                                                          GObject.ParamFlags.READABLE,
                                                          false),
        'orientation-lock-icon': GObject.ParamSpec.string('orientation-lock-icon',
                                                          'orientation-lock-icon',
                                                          'orientation-lock-icon',
                                                          GObject.ParamFlags.READWRITE,
                                                          null)
    },

    _init: function() {
        this.parent();

        this._canPowerOff = false;
        this._canHavePowerOff = true;
        this._canSuspend = false;
        this._canHaveSuspend = true;
        this._canLockScreen = false;
        this._canSwitchUser = false;
        this._canLogout = false;
        this._canLockOrientation = false;
        this._orientationLockIcon = null;

        this._loginScreenSettings = new Gio.Settings({ schema_id: LOGIN_SCREEN_SCHEMA });
        this._lockdownSettings = new Gio.Settings({ schema_id: LOCKDOWN_SCHEMA });
        this._orientationSettings = new Gio.Settings({ schema_id: 'org.gnome.settings-daemon.peripherals.touchscreen' });

        this._session = new GnomeSession.SessionManager();
        this._loginManager = LoginManager.getLoginManager();
        this._monitorManager = Meta.MonitorManager.get();

        this._userManager = AccountsService.UserManager.get_default();

        this._userManager.connect('notify::is-loaded',
                                  () => { this._updateMultiUser(); });
        this._userManager.connect('notify::has-multiple-users',
                                  () => { this._updateMultiUser(); });
        this._userManager.connect('user-added',
                                  () => { this._updateMultiUser(); });
        this._userManager.connect('user-removed',
                                  () => { this._updateMultiUser(); });

        this._lockdownSettings.connect('changed::' + DISABLE_USER_SWITCH_KEY,
                                       () => { this._updateSwitchUser(); });
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       () => { this._updateLogout(); });
        global.settings.connect('changed::' + ALWAYS_SHOW_LOG_OUT_KEY,
                                () => { this._updateLogout(); });

        this._lockdownSettings.connect('changed::' + DISABLE_LOCK_SCREEN_KEY,
                                       () => { this._updateLockScreen(); });

        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       () => { this._updateHaveShutdown(); });

        this.forceUpdate();

        this._orientationSettings.connect('changed::orientation-lock',
                                          () => { this._updateOrientationLock();
                                                  this._updateOrientationLockIcon(); });
        Main.layoutManager.connect('monitors-changed',
                                   () => { this._updateOrientationLock(); });
        Gio.DBus.system.watch_name(SENSOR_BUS_NAME,
                                   Gio.BusNameWatcherFlags.NONE,
                                   () => { this._sensorProxyAppeared(); },
                                   () => {
                                       this._sensorProxy = null;
                                       this._updateOrientationLock();
                                   });
        this._updateOrientationLock();
        this._updateOrientationLockIcon();

        Main.sessionMode.connect('updated', () => { this._sessionUpdated(); });
        this._sessionUpdated();
    },

    get can_power_off() {
        return this._canPowerOff;
    },

    get can_suspend() {
        return this._canSuspend;
    },

    get can_lock_screen() {
        return this._canLockScreen;
    },

    get can_switch_user() {
        return this._canSwitchUser;
    },

    get can_logout() {
        return this._canLogout;
    },

    get can_lock_orientation() {
        return this._canLockOrientation;
    },

    get orientation_lock_icon() {
        return this._orientationLockIcon;
    },

    _sensorProxyAppeared: function() {
        this._sensorProxy = new SensorProxy(Gio.DBus.system, SENSOR_BUS_NAME, SENSOR_OBJECT_PATH,
            (proxy, error)  => {
                if (error) {
                    log(error.message);
                    return;
                }
                this._sensorProxy.connect('g-properties-changed',
                                          () => { this._updateOrientationLock(); });
                this._updateOrientationLock();
            });
    },

    _updateOrientationLock: function() {
        if (this._sensorProxy)
            this._canLockOrientation = this._sensorProxy.HasAccelerometer &&
                                    this._monitorManager.get_is_builtin_display_on();
        else
            this._canLockOrientation = false;

        this.notify('can-lock-orientation');
    },

    _updateOrientationLockIcon: function() {
        let locked = this._orientationSettings.get_boolean('orientation-lock');
        let iconName = locked ? 'rotation-locked-symbolic'
                              : 'rotation-allowed-symbolic';
        this._orientationLockIcon = iconName;

        this.notify('orientation-lock-icon');
    },

    _sessionUpdated: function() {
        this._updateLockScreen();
        this._updatePowerOff();
        this._updateSuspend();
        this._updateMultiUser();
    },

    forceUpdate: function() {
        // Whether those actions are available or not depends on both lockdown
        // settings and Polkit policy - we don't get change notifications for the
        // latter, so their value may be outdated; force an update now
        this._updateHaveShutdown();
        this._updateHaveSuspend();
    },

    _updateLockScreen() {
        let showLock = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        this._canLockScreen = showLock && allowLockScreen && LoginManager.canLock();
        this.notify('can-lock-screen');
    },

    _updateHaveShutdown: function() {
        this._session.CanShutdownRemote((result, error) => {
            if (error)
                return;

            this._canHavePowerOff = result[0];
            this._updatePowerOff();
        });
    },

    _updatePowerOff: function() {
        let disabled = Main.sessionMode.isLocked ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._canPowerOff = this._canHavePowerOff && !disabled;
        this.notify('can-power-off');
    },

    _updateHaveSuspend: function() {
        this._loginManager.canSuspend(
            (canSuspend, needsAuth) => {
                this._canHaveSuspend = canSuspend;
                this._suspendNeedsAuth = needsAuth;
                this._updateSuspend();
            });
    },

    _updateSuspend: function() {
        let disabled = (Main.sessionMode.isLocked &&
                        this._suspendNeedsAuth) ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._canSuspend = this._canHaveSuspend && !disabled;
        this.notify('can-suspend');
    },

    _updateMultiUser: function() {
        this._updateLogout();
        this._updateSwitchUser();
    },

    _updateSwitchUser: function() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        let multiUser = this._userManager.can_switch() && this._userManager.has_multiple_users;
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        let visible = allowSwitch && multiUser && shouldShowInMode;
        this._canSwitchUser = visible;
        this.notify('can-switch-user');

        return visible;
    },

    _updateLogout: function() {
        let user = this._userManager.get_user(GLib.get_user_name());

        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        let alwaysShow = global.settings.get_boolean(ALWAYS_SHOW_LOG_OUT_KEY);
        let systemAccount = user.system_account;
        let localAccount = user.local_account;
        let multiUser = this._userManager.has_multiple_users;
        let multiSession = Gdm.get_session_ids().length > 1;
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        let visible = allowLogout && (alwaysShow || multiUser || multiSession || systemAccount || !localAccount) && shouldShowInMode;
        this._canLogout = visible;
        this.notify('can-logout');

        return visible;
    },

    activateLockOrientation: function() {
        if (!this._canLockOrientation)
            throw new Error('The lock-orientation action is not available!');

        let locked = this._orientationSettings.get_boolean('orientation-lock');
        this._orientationSettings.set_boolean('orientation-lock', !locked);
    },

    activateLockScreen: function() {
        if (!this._canLockScreen)
            throw new Error('The lock-screen action is not available!');

        Main.screenShield.lock(true);
    },

    activateSwitchUser: function() {
        if (!this._canSwitchUser)
            throw new Error('The switch-user action is not available!');

        if (Main.screenShield)
            Main.screenShield.lock(false);

        Clutter.threads_add_repaint_func_full(Clutter.RepaintFlags.POST_PAINT, function() {
            Gdm.goto_login_session_sync(null);
            return false;
        });
    },

    activateLogout: function() {
        if (!this._canLogout)
            throw new Error('The logout action is not available!');

        Main.overview.hide();
        this._session.LogoutRemote(0);
    },

    activatePowerOff: function() {
        if (!this._canPowerOff)
            throw new Error('The power-off action is not available!');

        this._session.ShutdownRemote(0);
    },

    activateSuspend: function() {
        if (!this._canSuspend)
            throw new Error('The suspend action is not available!');

        this._loginManager.suspend();
    }
});
