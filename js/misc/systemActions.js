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

const SensorProxyInterface = `
<node>
<interface name="net.hadess.SensorProxy">
  <property name="HasAccelerometer" type="b" access="read"/>
</interface>
</node>`;

const POWER_OFF_ACTION_ID        = 'power-off';
const LOCK_SCREEN_ACTION_ID      = 'lock-screen';
const LOGOUT_ACTION_ID           = 'logout';
const SUSPEND_ACTION_ID          = 'suspend';
const SWITCH_USER_ACTION_ID      = 'switch-user';
const LOCK_ORIENTATION_ACTION_ID = 'lock-orientation';

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

    _init() {
        this.parent();

        this._canHavePowerOff = true;
        this._canHaveSuspend = true;

        this._actions = new Map();
        this._actions.set(POWER_OFF_ACTION_ID,
                          { // Translators: The name of the power-off action in search
                            name: C_("search-result", "Power Off"),
                            iconName: 'system-shutdown-symbolic',
                            // Translators: A list of keywords that match the power-off action, separated by semicolons
                            keywords: _("power off;shutdown;reboot;restart").split(';'),
                            available: false });
        this._actions.set(LOCK_SCREEN_ACTION_ID,
                          { // Translators: The name of the lock screen action in search
                            name: C_("search-result", "Lock Screen"),
                            iconName: 'system-lock-screen-symbolic',
                            // Translators: A list of keywords that match the lock screen action, separated by semicolons
                            keywords: _("lock screen").split(';'),
                            available: false });
        this._actions.set(LOGOUT_ACTION_ID,
                          { // Translators: The name of the logout action in search
                            name: C_("search-result", "Log Out"),
                            iconName: 'application-exit-symbolic',
                            // Translators: A list of keywords that match the logout action, separated by semicolons
                            keywords: _("logout;sign off").split(';'),
                            available: false });
        this._actions.set(SUSPEND_ACTION_ID,
                          { // Translators: The name of the suspend action in search
                            name: C_("search-result", "Suspend"),
                            iconName: 'media-playback-pause-symbolic',
                            // Translators: A list of keywords that match the suspend action, separated by semicolons
                            keywords: _("suspend;sleep").split(';'),
                            available: false });
        this._actions.set(SWITCH_USER_ACTION_ID,
                          { // Translators: The name of the switch user action in search
                            name: C_("search-result", "Switch User"),
                            iconName: 'system-switch-user-symbolic',
                            // Translators: A list of keywords that match the switch user action, separated by semicolons
                            keywords: _("switch user").split(';'),
                            available: false });
        this._actions.set(LOCK_ORIENTATION_ACTION_ID,
                          { // Translators: The name of the lock orientation action in search
                            name: C_("search-result", "Lock Orientation"),
                            iconName: '',
                            // Translators: A list of keywords that match the lock orientation action, separated by semicolons
                            keywords: _("lock orientation;screen;rotation").split(';'),
                            available: false });

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
        return this._actions.get(POWER_OFF_ACTION_ID).available;
    },

    get can_suspend() {
        return this._actions.get(SUSPEND_ACTION_ID).available;
    },

    get can_lock_screen() {
        return this._actions.get(LOCK_SCREEN_ACTION_ID).available;
    },

    get can_switch_user() {
        return this._actions.get(SWITCH_USER_ACTION_ID).available;
    },

    get can_logout() {
        return this._actions.get(LOGOUT_ACTION_ID).available;
    },

    get can_lock_orientation() {
        return this._actions.get(LOCK_ORIENTATION_ACTION_ID).available;
    },

    get orientation_lock_icon() {
        return this._actions.get(LOCK_ORIENTATION_ACTION_ID).iconName;
    },

    _sensorProxyAppeared() {
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

    _updateOrientationLock() {
        let available = false;
        if (this._sensorProxy)
            available = this._sensorProxy.HasAccelerometer &&
                        this._monitorManager.get_is_builtin_display_on();

        this._actions.get(LOCK_ORIENTATION_ACTION_ID).available = available;

        this.notify('can-lock-orientation');
    },

    _updateOrientationLockIcon() {
        let locked = this._orientationSettings.get_boolean('orientation-lock');
        let iconName = locked ? 'rotation-locked-symbolic'
                              : 'rotation-allowed-symbolic';
        this._actions.get(LOCK_ORIENTATION_ACTION_ID).iconName = iconName;

        this.notify('orientation-lock-icon');
    },

    _sessionUpdated() {
        this._updateLockScreen();
        this._updatePowerOff();
        this._updateSuspend();
        this._updateMultiUser();
    },

    forceUpdate() {
        // Whether those actions are available or not depends on both lockdown
        // settings and Polkit policy - we don't get change notifications for the
        // latter, so their value may be outdated; force an update now
        this._updateHaveShutdown();
        this._updateHaveSuspend();
    },

    getMatchingActions(terms) {
        // terms is a list of strings
        terms = terms.map((term) => { return term.toLowerCase(); });

        let results = [];

        for (let [key, {available, keywords}] of this._actions)
            if (available && terms.every(t => keywords.some(k => (k.indexOf(t) >= 0))))
                results.push(key);

        return results;
    },

    getName(id) {
        return this._actions.get(id).name;
    },

    getIconName(id) {
        return this._actions.get(id).iconName;
    },

    activateAction(id) {
        switch (id) {
            case POWER_OFF_ACTION_ID:
                this.activatePowerOff();
                break;
            case LOCK_SCREEN_ACTION_ID:
                this.activateLockScreen();
                break;
            case LOGOUT_ACTION_ID:
                this.activateLogout();
                break;
            case SUSPEND_ACTION_ID:
                this.activateSuspend();
                break;
            case SWITCH_USER_ACTION_ID:
                this.activateSwitchUser();
                break;
            case LOCK_ORIENTATION_ACTION_ID:
                this.activateLockOrientation();
                break;
        }
    },

    _updateLockScreen() {
        let showLock = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        this._actions.get(LOCK_SCREEN_ACTION_ID).available = showLock && allowLockScreen && LoginManager.canLock();
        this.notify('can-lock-screen');
    },

    _updateHaveShutdown() {
        this._session.CanShutdownRemote((result, error) => {
            if (error)
                return;

            this._canHavePowerOff = result[0];
            this._updatePowerOff();
        });
    },

    _updatePowerOff() {
        let disabled = Main.sessionMode.isLocked ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._actions.get(POWER_OFF_ACTION_ID).available = this._canHavePowerOff && !disabled;
        this.notify('can-power-off');
    },

    _updateHaveSuspend() {
        this._loginManager.canSuspend(
            (canSuspend, needsAuth) => {
                this._canHaveSuspend = canSuspend;
                this._suspendNeedsAuth = needsAuth;
                this._updateSuspend();
            });
    },

    _updateSuspend() {
        let disabled = (Main.sessionMode.isLocked &&
                        this._suspendNeedsAuth) ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._actions.get(SUSPEND_ACTION_ID).available = this._canHaveSuspend && !disabled;
        this.notify('can-suspend');
    },

    _updateMultiUser() {
        this._updateLogout();
        this._updateSwitchUser();
    },

    _updateSwitchUser() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        let multiUser = this._userManager.can_switch() && this._userManager.has_multiple_users;
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        let visible = allowSwitch && multiUser && shouldShowInMode;
        this._actions.get(SWITCH_USER_ACTION_ID).available = visible;
        this.notify('can-switch-user');

        return visible;
    },

    _updateLogout() {
        let user = this._userManager.get_user(GLib.get_user_name());

        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        let alwaysShow = global.settings.get_boolean(ALWAYS_SHOW_LOG_OUT_KEY);
        let systemAccount = user.system_account;
        let localAccount = user.local_account;
        let multiUser = this._userManager.has_multiple_users;
        let multiSession = Gdm.get_session_ids().length > 1;
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        let visible = allowLogout && (alwaysShow || multiUser || multiSession || systemAccount || !localAccount) && shouldShowInMode;
        this._actions.get(LOGOUT_ACTION_ID).available = visible;
        this.notify('can-logout');

        return visible;
    },

    activateLockOrientation() {
        if (!this._actions.get(LOCK_ORIENTATION_ACTION_ID).available)
            throw new Error('The lock-orientation action is not available!');

        let locked = this._orientationSettings.get_boolean('orientation-lock');
        this._orientationSettings.set_boolean('orientation-lock', !locked);
    },

    activateLockScreen() {
        if (!this._actions.get(LOCK_SCREEN_ACTION_ID).available)
            throw new Error('The lock-screen action is not available!');

        Main.screenShield.lock(true);
    },

    activateSwitchUser() {
        if (!this._actions.get(SWITCH_USER_ACTION_ID).available)
            throw new Error('The switch-user action is not available!');

        if (Main.screenShield)
            Main.screenShield.lock(false);

        Clutter.threads_add_repaint_func_full(Clutter.RepaintFlags.POST_PAINT, () => {
            Gdm.goto_login_session_sync(null);
            return false;
        });
    },

    activateLogout() {
        if (!this._actions.get(LOGOUT_ACTION_ID).available)
            throw new Error('The logout action is not available!');

        Main.overview.hide();
        this._session.LogoutRemote(0);
    },

    activatePowerOff() {
        if (!this._actions.get(POWER_OFF_ACTION_ID).available)
            throw new Error('The power-off action is not available!');

        this._session.ShutdownRemote(0);
    },

    activateSuspend() {
        if (!this._actions.get(SUSPEND_ACTION_ID).available)
            throw new Error('The suspend action is not available!');

        this._loginManager.suspend();
    }
});
