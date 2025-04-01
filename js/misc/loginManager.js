import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GioUnix from 'gi://GioUnix';
import Shell from 'gi://Shell';
import * as Signals from './signals.js';

import {loadInterfaceXML} from './fileUtils.js';

const SystemdLoginManagerIface = loadInterfaceXML('org.freedesktop.login1.Manager');
const SystemdLoginSessionIface = loadInterfaceXML('org.freedesktop.login1.Session');
const SystemdLoginUserIface = loadInterfaceXML('org.freedesktop.login1.User');

const SystemdLoginManager = Gio.DBusProxy.makeProxyWrapper(SystemdLoginManagerIface);
const SystemdLoginSession = Gio.DBusProxy.makeProxyWrapper(SystemdLoginSessionIface);
const SystemdLoginUser = Gio.DBusProxy.makeProxyWrapper(SystemdLoginUserIface);

function haveSystemd() {
    return GLib.access('/run/systemd/seats', 0) >= 0;
}

function versionCompare(required, reference) {
    required = required.split('.');
    reference = reference.split('.');

    for (let i = 0; i < required.length; i++) {
        let requiredInt = parseInt(required[i]);
        let referenceInt = parseInt(reference[i]);
        if (requiredInt !== referenceInt)
            return requiredInt < referenceInt;
    }

    return true;
}

/**
 * @returns {boolean}
 */
export function canLock() {
    try {
        let params = GLib.Variant.new('(ss)', ['org.gnome.DisplayManager.Manager', 'Version']);
        let result = Gio.DBus.system.call_sync(
            'org.gnome.DisplayManager',
            '/org/gnome/DisplayManager/Manager',
            'org.freedesktop.DBus.Properties',
            'Get', params, null,
            Gio.DBusCallFlags.NONE,
            -1, null);

        let version = result.deepUnpack()[0].deepUnpack();
        return haveSystemd() && versionCompare('3.5.91', version);
    } catch {
        return false;
    }
}

export async function registerSessionWithGDM() {
    log('Registering session with GDM');
    try {
        await Gio.DBus.system.call(
            'org.gnome.DisplayManager',
            '/org/gnome/DisplayManager/Manager',
            'org.gnome.DisplayManager.Manager',
            'RegisterSession',
            GLib.Variant.new('(a{sv})', [{}]), null,
            Gio.DBusCallFlags.NONE, -1, null);
    } catch (e) {
        if (!e.matches(Gio.DBusError, Gio.DBusError.UNKNOWN_METHOD))
            log(`Error registering session with GDM: ${e.message}`);
        else
            log('Not calling RegisterSession(): method not exported, GDM too old?');
    }
}

let _loginManager = null;

/**
 * An abstraction over systemd/logind and ConsoleKit.
 *
 * @returns {LoginManagerSystemd | LoginManagerDummy} - the LoginManager singleton
 */
export function getLoginManager() {
    if (_loginManager == null) {
        if (haveSystemd())
            _loginManager = new LoginManagerSystemd();
        else
            _loginManager = new LoginManagerDummy();
    }

    return _loginManager;
}

class LoginManagerSystemd extends Signals.EventEmitter {
    constructor() {
        super();

        this._preparingForSleep = false;

        this._proxy = new SystemdLoginManager(Gio.DBus.system,
            'org.freedesktop.login1',
            '/org/freedesktop/login1');
        this._proxy.connectSignal('PrepareForSleep',
            this._prepareForSleep.bind(this));
        this._proxy.connectSignal('SessionRemoved',
            this._sessionRemoved.bind(this));
    }

    async getCurrentUserProxy() {
        if (this._userProxy)
            return this._userProxy;

        const uid = Shell.util_get_uid();
        try {
            const [objectPath] = await this._proxy.GetUserAsync(uid);
            this._userProxy = await SystemdLoginUser.newAsync(
                Gio.DBus.system, 'org.freedesktop.login1', objectPath);
            return this._userProxy;
        } catch (error) {
            logError(error, `Could not get a proxy for user ${uid}`);
            return null;
        }
    }

    async getCurrentSessionProxy() {
        if (this._currentSession)
            return this._currentSession;

        let sessionId = GLib.getenv('XDG_SESSION_ID');
        if (!sessionId) {
            log('Unset XDG_SESSION_ID, getCurrentSessionProxy() called outside a user session. Asking logind directly.');
            const userProxy = await this.getCurrentUserProxy();
            let [session, objectPath] = userProxy.Display;
            if (session) {
                log(`Will monitor session ${session}`);
                sessionId = session;
            } else {
                log('Failed to find "Display" session; are we the greeter?');

                for ([session, objectPath] of userProxy.Sessions) {
                    let sessionProxy = new SystemdLoginSession(Gio.DBus.system,
                        'org.freedesktop.login1',
                        objectPath);
                    log(`Considering ${session}, class=${sessionProxy.Class}`);
                    if (sessionProxy.Class === 'greeter') {
                        log(`Yes, will monitor session ${session}`);
                        sessionId = session;
                        break;
                    }
                }

                if (!sessionId) {
                    log('No, failed to get session from logind.');
                    return null;
                }
            }
        }

        try {
            const [objectPath] = await this._proxy.GetSessionAsync(sessionId);
            this._currentSession = await SystemdLoginSession.newAsync(
                Gio.DBus.system, 'org.freedesktop.login1', objectPath);
            return this._currentSession;
        } catch (error) {
            logError(error, 'Could not get a proxy for the current session');
            return null;
        }
    }

    async canSuspend() {
        let canSuspend, needsAuth;

        try {
            const [result] = await this._proxy.CanSuspendAsync();
            needsAuth = result === 'challenge';
            canSuspend = needsAuth || result === 'yes';
        } catch {
            canSuspend = false;
            needsAuth = false;
        }
        return {canSuspend, needsAuth};
    }

    async canRebootToBootLoaderMenu() {
        let canRebootToBootLoaderMenu, needsAuth;

        try {
            const [result] = await this._proxy.CanRebootToBootLoaderMenuAsync();
            needsAuth = result === 'challenge';
            canRebootToBootLoaderMenu = needsAuth || result === 'yes';
        } catch {
            canRebootToBootLoaderMenu = false;
            needsAuth = false;
        }
        return {canRebootToBootLoaderMenu, needsAuth};
    }

    setRebootToBootLoaderMenu() {
        /* Parameter is timeout in usec, show to menu for 60 seconds */
        this._proxy.SetRebootToBootLoaderMenuAsync(60000000);
    }

    async listSessions() {
        try {
            const [sessions] = await this._proxy.ListSessionsAsync();
            return sessions;
        } catch {
            return [];
        }
    }

    getSession(objectPath) {
        return new SystemdLoginSession(Gio.DBus.system, 'org.freedesktop.login1', objectPath);
    }

    suspend() {
        this._proxy.SuspendAsync(true);
    }

    async inhibit(reason, cancellable) {
        const inVariant = new GLib.Variant('(ssss)',
            ['sleep', 'GNOME Shell', reason, 'delay']);
        const [outVariant_, fdList] =
            await this._proxy.call_with_unix_fd_list('Inhibit',
                inVariant, 0, -1, null, cancellable);
        const [fd] = fdList.steal_fds();
        return new GioUnix.InputStream({fd});
    }

    _prepareForSleep(proxy, sender, [aboutToSuspend]) {
        this._preparingForSleep = aboutToSuspend;
        this.emit('prepare-for-sleep', aboutToSuspend);
    }

    /**
     * Whether the machine is preparing to sleep.
     *
     * This is true between paired emissions of `prepare-for-sleep`.
     *
     * @type {boolean}
     */
    get preparingForSleep() {
        return this._preparingForSleep;
    }

    _sessionRemoved(proxy, sender, [sessionId]) {
        this.emit('session-removed', sessionId);
    }
}

class LoginManagerDummy extends Signals.EventEmitter  {
    constructor() {
        super();

        this._preparingForSleep = false;
    }

    getCurrentUserProxy() {
        // we could return a DummyUser object that fakes whatever callers
        // expect, but just never settling the promise should be safer
        return new Promise(() => {});
    }

    getCurrentSessionProxy() {
        // we could return a DummySession object that fakes whatever callers
        // expect (at the time of writing: connect() and connectSignal()
        // methods), but just never settling the promise should be safer
        return new Promise(() => {});
    }

    canSuspend() {
        return new Promise(resolve => resolve({
            canSuspend: false,
            needsAuth: false,
        }));
    }

    canRebootToBootLoaderMenu() {
        return new Promise(resolve => resolve({
            canRebootToBootLoaderMenu: false,
            needsAuth: false,
        }));
    }

    setRebootToBootLoaderMenu() {
    }

    listSessions() {
        return new Promise(resolve => resolve([]));
    }

    getSession(_objectPath) {
        return null;
    }

    suspend() {
        this._preparingForSleep = true;
        this.emit('prepare-for-sleep', true);
        this._preparingForSleep = false;
        this.emit('prepare-for-sleep', false);
    }

    get preparingForSleep() {
        return this._preparingForSleep;
    }

    /* eslint-disable-next-line require-await */
    async inhibit() {
        return null;
    }
}
