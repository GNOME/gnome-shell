/* exported getDefault */
const {AccountsService, Clutter, Gdm, Gio, GLib, GObject} = imports.gi;

const GnomeSession = imports.misc.gnomeSession;
const LoginManager = imports.misc.loginManager;
const Main = imports.ui.main;
const Screenshot = imports.ui.screenshot;

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const DISABLE_USER_SWITCH_KEY = 'disable-user-switching';
const DISABLE_LOCK_SCREEN_KEY = 'disable-lock-screen';
const DISABLE_LOG_OUT_KEY = 'disable-log-out';
const DISABLE_RESTART_KEY = 'disable-restart-buttons';
const ALWAYS_SHOW_LOG_OUT_KEY = 'always-show-log-out';

const POWER_OFF_ACTION_ID        = 'power-off';
const RESTART_ACTION_ID          = 'restart';
const LOCK_SCREEN_ACTION_ID      = 'lock-screen';
const LOGOUT_ACTION_ID           = 'logout';
const SUSPEND_ACTION_ID          = 'suspend';
const SWITCH_USER_ACTION_ID      = 'switch-user';
const LOCK_ORIENTATION_ACTION_ID = 'lock-orientation';
const SCREENSHOT_UI_ACTION_ID    = 'open-screenshot-ui';

let _singleton = null;

function getDefault() {
    if (_singleton == null)
        _singleton = new SystemActions();

    return _singleton;
}

const SystemActions = GObject.registerClass({
    Properties: {
        'can-power-off': GObject.ParamSpec.boolean(
            'can-power-off', 'can-power-off', 'can-power-off',
            GObject.ParamFlags.READABLE,
            false),
        'can-restart': GObject.ParamSpec.boolean(
            'can-restart', 'can-restart', 'can-restart',
            GObject.ParamFlags.READABLE,
            false),
        'can-suspend': GObject.ParamSpec.boolean(
            'can-suspend', 'can-suspend', 'can-suspend',
            GObject.ParamFlags.READABLE,
            false),
        'can-lock-screen': GObject.ParamSpec.boolean(
            'can-lock-screen', 'can-lock-screen', 'can-lock-screen',
            GObject.ParamFlags.READABLE,
            false),
        'can-switch-user': GObject.ParamSpec.boolean(
            'can-switch-user', 'can-switch-user', 'can-switch-user',
            GObject.ParamFlags.READABLE,
            false),
        'can-logout': GObject.ParamSpec.boolean(
            'can-logout', 'can-logout', 'can-logout',
            GObject.ParamFlags.READABLE,
            false),
        'can-lock-orientation': GObject.ParamSpec.boolean(
            'can-lock-orientation', 'can-lock-orientation', 'can-lock-orientation',
            GObject.ParamFlags.READABLE,
            false),
        'orientation-lock-icon': GObject.ParamSpec.string(
            'orientation-lock-icon', 'orientation-lock-icon', 'orientation-lock-icon',
            GObject.ParamFlags.READWRITE,
            null),
    },
}, class SystemActions extends GObject.Object {
    _init() {
        super._init();

        this._canHavePowerOff = true;
        this._canHaveSuspend = true;

        function tokenizeKeywords(keywords) {
            return keywords.split(';').map(keyword => GLib.str_tokenize_and_fold(keyword, null)).flat(2);
        }

        this._actions = new Map();
        this._actions.set(POWER_OFF_ACTION_ID, {
            // Translators: The name of the power-off action in search
            name: C_("search-result", "Power Off"),
            iconName: 'system-shutdown-symbolic',
            // Translators: A list of keywords that match the power-off action, separated by semicolons
            keywords: tokenizeKeywords(_('power off;poweroff;shutdown;halt;stop')),
            available: false,
        });
        this._actions.set(RESTART_ACTION_ID, {
            // Translators: The name of the restart action in search
            name: C_('search-result', 'Restart'),
            iconName: 'system-reboot-symbolic',
            // Translators: A list of keywords that match the restart action, separated by semicolons
            keywords: tokenizeKeywords(_('reboot;restart;')),
            available: false,
        });
        this._actions.set(LOCK_SCREEN_ACTION_ID, {
            // Translators: The name of the lock screen action in search
            name: C_("search-result", "Lock Screen"),
            iconName: 'system-lock-screen-symbolic',
            // Translators: A list of keywords that match the lock screen action, separated by semicolons
            keywords: tokenizeKeywords(_('lock screen')),
            available: false,
        });
        this._actions.set(LOGOUT_ACTION_ID, {
            // Translators: The name of the logout action in search
            name: C_("search-result", "Log Out"),
            iconName: 'system-log-out-symbolic',
            // Translators: A list of keywords that match the logout action, separated by semicolons
            keywords: tokenizeKeywords(_('logout;log out;sign off')),
            available: false,
        });
        this._actions.set(SUSPEND_ACTION_ID, {
            // Translators: The name of the suspend action in search
            name: C_("search-result", "Suspend"),
            iconName: 'media-playback-pause-symbolic',
            // Translators: A list of keywords that match the suspend action, separated by semicolons
            keywords: tokenizeKeywords(_('suspend;sleep')),
            available: false,
        });
        this._actions.set(SWITCH_USER_ACTION_ID, {
            // Translators: The name of the switch user action in search
            name: C_("search-result", "Switch User"),
            iconName: 'system-switch-user-symbolic',
            // Translators: A list of keywords that match the switch user action, separated by semicolons
            keywords: tokenizeKeywords(_('switch user')),
            available: false,
        });
        this._actions.set(LOCK_ORIENTATION_ACTION_ID, {
            name: '',
            iconName: '',
            // Translators: A list of keywords that match the lock orientation action, separated by semicolons
            keywords: tokenizeKeywords(_('lock orientation;unlock orientation;screen;rotation')),
            available: false,
        });
        this._actions.set(SCREENSHOT_UI_ACTION_ID, {
            // Translators: The name of the screenshot UI action in search
            name: C_('search-result', 'Take a Screenshot'),
            iconName: 'record-screen-symbolic',
            // Translators: A list of keywords that match the screenshot UI action, separated by semicolons
            keywords: tokenizeKeywords(_('screenshot;screencast;snip;capture;record')),
            available: true,
        });

        this._loginScreenSettings = new Gio.Settings({ schema_id: LOGIN_SCREEN_SCHEMA });
        this._lockdownSettings = new Gio.Settings({ schema_id: LOCKDOWN_SCHEMA });
        this._orientationSettings = new Gio.Settings({ schema_id: 'org.gnome.settings-daemon.peripherals.touchscreen' });

        this._session = new GnomeSession.SessionManager();
        this._loginManager = LoginManager.getLoginManager();
        this._monitorManager = global.backend.get_monitor_manager();

        this._userManager = AccountsService.UserManager.get_default();

        this._userManager.connect('notify::is-loaded',
                                  () => this._updateMultiUser());
        this._userManager.connect('notify::has-multiple-users',
                                  () => this._updateMultiUser());
        this._userManager.connect('user-added',
                                  () => this._updateMultiUser());
        this._userManager.connect('user-removed',
                                  () => this._updateMultiUser());

        this._lockdownSettings.connect(`changed::${DISABLE_USER_SWITCH_KEY}`,
                                       () => this._updateSwitchUser());
        this._lockdownSettings.connect(`changed::${DISABLE_LOG_OUT_KEY}`,
                                       () => this._updateLogout());
        global.settings.connect(`changed::${ALWAYS_SHOW_LOG_OUT_KEY}`,
                                () => this._updateLogout());

        this._lockdownSettings.connect(`changed::${DISABLE_LOCK_SCREEN_KEY}`,
                                       () => this._updateLockScreen());

        this._lockdownSettings.connect(`changed::${DISABLE_LOG_OUT_KEY}`,
                                       () => this._updateHaveShutdown());

        this.forceUpdate();

        this._orientationSettings.connect('changed::orientation-lock', () => {
            this._updateOrientationLock();
            this._updateOrientationLockStatus();
        });
        Main.layoutManager.connect('monitors-changed',
                                   () => this._updateOrientationLock());
        this._monitorManager.connect('notify::panel-orientation-managed',
            () => this._updateOrientationLock());
        this._updateOrientationLock();
        this._updateOrientationLockStatus();

        Main.sessionMode.connect('updated', () => this._sessionUpdated());
        this._sessionUpdated();
    }

    get canPowerOff() {
        return this._actions.get(POWER_OFF_ACTION_ID).available;
    }

    get canRestart() {
        return this._actions.get(RESTART_ACTION_ID).available;
    }

    get canSuspend() {
        return this._actions.get(SUSPEND_ACTION_ID).available;
    }

    get canLockScreen() {
        return this._actions.get(LOCK_SCREEN_ACTION_ID).available;
    }

    get canSwitchUser() {
        return this._actions.get(SWITCH_USER_ACTION_ID).available;
    }

    get canLogout() {
        return this._actions.get(LOGOUT_ACTION_ID).available;
    }

    get canLockOrientation() {
        return this._actions.get(LOCK_ORIENTATION_ACTION_ID).available;
    }

    get orientationLockIcon() {
        return this._actions.get(LOCK_ORIENTATION_ACTION_ID).iconName;
    }

    _updateOrientationLock() {
        const available = this._monitorManager.get_panel_orientation_managed();

        this._actions.get(LOCK_ORIENTATION_ACTION_ID).available = available;

        this.notify('can-lock-orientation');
    }

    _updateOrientationLockStatus() {
        let locked = this._orientationSettings.get_boolean('orientation-lock');
        let action = this._actions.get(LOCK_ORIENTATION_ACTION_ID);

        // Translators: The name of the lock orientation action in search
        // and in the system status menu
        let name = locked
            ? C_('search-result', 'Unlock Screen Rotation')
            : C_('search-result', 'Lock Screen Rotation');
        let iconName = locked
            ? 'rotation-locked-symbolic'
            : 'rotation-allowed-symbolic';

        action.name = name;
        action.iconName = iconName;

        this.notify('orientation-lock-icon');
    }

    _sessionUpdated() {
        this._updateLockScreen();
        this._updatePowerOff();
        this._updateSuspend();
        this._updateMultiUser();
    }

    forceUpdate() {
        // Whether those actions are available or not depends on both lockdown
        // settings and Polkit policy - we don't get change notifications for the
        // latter, so their value may be outdated; force an update now
        this._updateHaveShutdown();
        this._updateHaveSuspend();
    }

    getMatchingActions(terms) {
        // terms is a list of strings
        terms = terms.map(
            term => GLib.str_tokenize_and_fold(term, null)[0]).flat(2);

        // tokenizing may return an empty array
        if (terms.length === 0)
            return [];

        let results = [];

        for (let [key, { available, keywords }] of this._actions) {
            if (available && terms.every(t => keywords.some(k => k.startsWith(t))))
                results.push(key);
        }

        return results;
    }

    getName(id) {
        return this._actions.get(id).name;
    }

    getIconName(id) {
        return this._actions.get(id).iconName;
    }

    activateAction(id) {
        switch (id) {
        case POWER_OFF_ACTION_ID:
            this.activatePowerOff();
            break;
        case RESTART_ACTION_ID:
            this.activateRestart();
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
        case SCREENSHOT_UI_ACTION_ID:
            this.activateScreenshotUI();
            break;
        }
    }

    _updateLockScreen() {
        let showLock = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        this._actions.get(LOCK_SCREEN_ACTION_ID).available = showLock && allowLockScreen && LoginManager.canLock();
        this.notify('can-lock-screen');
    }

    async _updateHaveShutdown() {
        try {
            const [canShutdown] = await this._session.CanShutdownAsync();
            this._canHavePowerOff = canShutdown;
        } catch (e) {
            this._canHavePowerOff = false;
        }
        this._updatePowerOff();
    }

    _updatePowerOff() {
        let disabled = Main.sessionMode.isLocked ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._actions.get(POWER_OFF_ACTION_ID).available = this._canHavePowerOff && !disabled;
        this.notify('can-power-off');

        this._actions.get(RESTART_ACTION_ID).available = this._canHavePowerOff && !disabled;
        this.notify('can-restart');
    }

    async _updateHaveSuspend() {
        const {canSuspend, needsAuth} = await this._loginManager.canSuspend();
        this._canHaveSuspend = canSuspend;
        this._suspendNeedsAuth = needsAuth;
        this._updateSuspend();
    }

    _updateSuspend() {
        let disabled = (Main.sessionMode.isLocked &&
                        this._suspendNeedsAuth) ||
                       (Main.sessionMode.isGreeter &&
                        this._loginScreenSettings.get_boolean(DISABLE_RESTART_KEY));
        this._actions.get(SUSPEND_ACTION_ID).available = this._canHaveSuspend && !disabled;
        this.notify('can-suspend');
    }

    _updateMultiUser() {
        this._updateLogout();
        this._updateSwitchUser();
    }

    _updateSwitchUser() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        let multiUser = this._userManager.can_switch() && this._userManager.has_multiple_users;
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        let visible = allowSwitch && multiUser && shouldShowInMode;
        this._actions.get(SWITCH_USER_ACTION_ID).available = visible;
        this.notify('can-switch-user');

        return visible;
    }

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
    }

    activateLockOrientation() {
        if (!this._actions.get(LOCK_ORIENTATION_ACTION_ID).available)
            throw new Error('The lock-orientation action is not available!');

        let locked = this._orientationSettings.get_boolean('orientation-lock');
        this._orientationSettings.set_boolean('orientation-lock', !locked);
    }

    activateLockScreen() {
        if (!this._actions.get(LOCK_SCREEN_ACTION_ID).available)
            throw new Error('The lock-screen action is not available!');

        Main.screenShield.lock(true);
    }

    activateSwitchUser() {
        if (!this._actions.get(SWITCH_USER_ACTION_ID).available)
            throw new Error('The switch-user action is not available!');

        if (Main.screenShield)
            Main.screenShield.lock(false);

        Clutter.threads_add_repaint_func_full(Clutter.RepaintFlags.POST_PAINT, () => {
            Gdm.goto_login_session_sync(null);
            return false;
        });
    }

    activateLogout() {
        if (!this._actions.get(LOGOUT_ACTION_ID).available)
            throw new Error('The logout action is not available!');

        Main.overview.hide();
        this._session.LogoutAsync(0).catch(logError);
    }

    activatePowerOff() {
        if (!this._actions.get(POWER_OFF_ACTION_ID).available)
            throw new Error('The power-off action is not available!');

        this._session.ShutdownAsync(0).catch(logError);
    }

    activateRestart() {
        if (!this._actions.get(RESTART_ACTION_ID).available)
            throw new Error('The restart action is not available!');

        this._session.RebootAsync().catch(logError);
    }

    activateSuspend() {
        if (!this._actions.get(SUSPEND_ACTION_ID).available)
            throw new Error('The suspend action is not available!');

        this._loginManager.suspend();
    }

    activateScreenshotUI() {
        if (!this._actions.get(SCREENSHOT_UI_ACTION_ID).available)
            throw new Error('The screenshot UI action is not available!');

        if (this._overviewHiddenId)
            return;

        this._overviewHiddenId = Main.overview.connect('hidden', () => {
            Main.overview.disconnect(this._overviewHiddenId);
            delete this._overviewHiddenId;
            Screenshot.showScreenshotUI();
        });
    }
});
