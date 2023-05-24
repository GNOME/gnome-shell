// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported getIBusManager */

const { Gio, GLib, IBus, Meta, Shell } = imports.gi;
const Signals = imports.misc.signals;
const BoxPointer = imports.ui.boxpointer;

const IBusCandidatePopup = imports.ui.ibusCandidatePopup;

Gio._promisify(IBus.Bus.prototype,
    'list_engines_async', 'list_engines_async_finish');
Gio._promisify(IBus.Bus.prototype,
    'request_name_async', 'request_name_async_finish');
Gio._promisify(IBus.Bus.prototype,
    'get_global_engine_async', 'get_global_engine_async_finish');
Gio._promisify(IBus.Bus.prototype,
    'set_global_engine_async', 'set_global_engine_async_finish');
Gio._promisify(Shell, 'util_systemd_unit_exists');

// Ensure runtime version matches
_checkIBusVersion(1, 5, 2);

let _ibusManager = null;
const IBUS_SYSTEMD_SERVICE = 'org.freedesktop.IBus.session.GNOME.service';

const TYPING_BOOSTER_ENGINE = 'typing-booster';
const IBUS_TYPING_BOOSTER_SCHEMA = 'org.freedesktop.ibus.engine.typing-booster';
const KEY_EMOJIPREDICTIONS = 'emojipredictions';
const KEY_DICTIONARY = 'dictionary';
const KEY_INLINECOMPLETION = 'inlinecompletion';
const KEY_INPUTMETHOD = 'inputmethod';

function _checkIBusVersion(requiredMajor, requiredMinor, requiredMicro) {
    if ((IBus.MAJOR_VERSION > requiredMajor) ||
        (IBus.MAJOR_VERSION == requiredMajor && IBus.MINOR_VERSION > requiredMinor) ||
        (IBus.MAJOR_VERSION == requiredMajor && IBus.MINOR_VERSION == requiredMinor &&
         IBus.MICRO_VERSION >= requiredMicro))
        return;

    throw new Error(`Found IBus version ${
        IBus.MAJOR_VERSION}.${IBus.MINOR_VERSION}.${IBus.MINOR_VERSION} ` +
        `but required is ${requiredMajor}.${requiredMinor}.${requiredMicro}`);
}

function getIBusManager() {
    if (_ibusManager == null)
        _ibusManager = new IBusManager();
    return _ibusManager;
}

var IBusManager = class extends Signals.EventEmitter {
    constructor() {
        super();

        IBus.init();

        // This is the longest we'll keep the keyboard frozen until an input
        // source is active.
        this._MAX_INPUT_SOURCE_ACTIVATION_TIME = 4000; // ms
        this._PRELOAD_ENGINES_DELAY_TIME = 30; // sec


        this._candidatePopup = new IBusCandidatePopup.CandidatePopup();

        this._panelService = null;
        this._engines = new Map();
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;
        this._preloadEnginesId = 0;

        this._ibus = IBus.Bus.new_async();
        this._ibus.connect('connected', this._onConnected.bind(this));
        this._ibus.connect('disconnected', this._clear.bind(this));
        // Need to set this to get 'global-engine-changed' emitions
        this._ibus.set_watch_ibus_signal(true);
        this._ibus.connect('global-engine-changed', this._engineChanged.bind(this));

        this._queueSpawn();
    }

    async _ibusSystemdServiceExists() {
        if (this._ibusIsSystemdService)
            return true;

        try {
            this._ibusIsSystemdService =
                await Shell.util_systemd_unit_exists(
                    IBUS_SYSTEMD_SERVICE, null);
        } catch (e) {
            this._ibusIsSystemdService = false;
        }

        return this._ibusIsSystemdService;
    }

    async _queueSpawn() {
        const isSystemdService = await this._ibusSystemdServiceExists();
        if (!isSystemdService)
            this._spawn(Meta.is_wayland_compositor() ? [] : ['--xim']);
    }

    _tryAppendEnv(env, varname) {
        const value = GLib.getenv(varname);
        if (value)
            env.push(`${varname}=${value}`);
    }

    _spawn(extraArgs = []) {
        try {
            let cmdLine = ['ibus-daemon', '--panel', 'disable', ...extraArgs];
            let env = [];

            this._tryAppendEnv(env, 'DBUS_SESSION_BUS_ADDRESS');
            this._tryAppendEnv(env, 'WAYLAND_DISPLAY');
            this._tryAppendEnv(env, 'HOME');
            this._tryAppendEnv(env, 'LANG');
            this._tryAppendEnv(env, 'LC_CTYPE');
            this._tryAppendEnv(env, 'COMPOSE_FILE');
            this._tryAppendEnv(env, 'DISPLAY');

            // Use DO_NOT_REAP_CHILD to avoid adouble-fork internally
            // since ibus-daemon refuses to start with init as its parent.
            const [success_, pid] = GLib.spawn_async(
                null, cmdLine, env,
                GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                () => {
                    try {
                        global.context.restore_rlimit_nofile();
                    } catch (err) {
                    }
                }
            );
            GLib.child_watch_add(
                GLib.PRIORITY_DEFAULT,
                pid,
                () => GLib.spawn_close_pid(pid)
            );
        } catch (e) {
            log(`Failed to launch ibus-daemon: ${e.message}`);
        }
    }

    async restartDaemon(extraArgs = []) {
        const isSystemdService = await this._ibusSystemdServiceExists();
        if (!isSystemdService)
            this._spawn(['-r', ...extraArgs]);
    }

    _clear() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        if (this._preloadEnginesId) {
            GLib.source_remove(this._preloadEnginesId);
            this._preloadEnginesId = 0;
        }

        if (this._panelService)
            this._panelService.destroy();

        this._panelService = null;
        this._candidatePopup.setPanelService(null);
        this._engines.clear();
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;

        this.emit('ready', false);
    }

    _onConnected() {
        this._cancellable = new Gio.Cancellable();
        this._initEngines();
        this._initPanelService();
    }

    async _initEngines() {
        try {
            const enginesList =
                await this._ibus.list_engines_async(-1, this._cancellable);
            for (let i = 0; i < enginesList.length; ++i) {
                let name = enginesList[i].get_name();
                this._engines.set(name, enginesList[i]);
            }
            this._updateReadiness();
        } catch (e) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;

            logError(e);
            this._clear();
        }
    }

    async _initPanelService() {
        try {
            await this._ibus.request_name_async(IBus.SERVICE_PANEL,
                IBus.BusNameFlag.REPLACE_EXISTING, -1, this._cancellable);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                logError(e);
                this._clear();
            }
            return;
        }

        this._panelService = new IBus.PanelService({
            connection: this._ibus.get_connection(),
            object_path: IBus.PATH_PANEL,
        });
        this._candidatePopup.setPanelService(this._panelService);
        this._panelService.connect('update-property', this._updateProperty.bind(this));
        this._panelService.connect('set-cursor-location', (ps, x, y, w, h) => {
            let cursorLocation = { x, y, width: w, height: h };
            this.emit('set-cursor-location', cursorLocation);
        });
        this._panelService.connect('focus-in', (panel, path) => {
            if (!GLib.str_has_suffix(path, '/InputContext_1'))
                this.emit('focus-in');
        });
        this._panelService.connect('focus-out', () => this.emit('focus-out'));

        try {
            // IBus versions older than 1.5.10 have a bug which
            // causes spurious set-content-type emissions when
            // switching input focus that temporarily lose purpose
            // and hints defeating its intended semantics and
            // confusing users. We thus don't use it in that case.
            _checkIBusVersion(1, 5, 10);
            this._panelService.connect('set-content-type', this._setContentType.bind(this));
        } catch (e) {
        }
        this._updateReadiness();

        try {
            // If an engine is already active we need to get its properties
            const engine =
                await this._ibus.get_global_engine_async(-1, this._cancellable);
            this._engineChanged(this._ibus, engine.get_name());
        } catch (e) {
        }
    }

    _updateReadiness() {
        this._ready = this._engines.size > 0 && this._panelService != null;
        this.emit('ready', this._ready);
    }

    _engineChanged(bus, engineName) {
        if (!this._ready)
            return;

        this._currentEngineName = engineName;
        this._candidatePopup.close(BoxPointer.PopupAnimation.NONE);

        if (this._registerPropertiesId != 0)
            return;

        this._registerPropertiesId =
            this._panelService.connect('register-properties', (p, props) => {
                if (!props.get(0))
                    return;

                this._panelService.disconnect(this._registerPropertiesId);
                this._registerPropertiesId = 0;

                this.emit('properties-registered', this._currentEngineName, props);
            });
    }

    _updateProperty(panel, prop) {
        this.emit('property-updated', this._currentEngineName, prop);
    }

    _setContentType(panel, purpose, hints) {
        this.emit('set-content-type', purpose, hints);
    }

    activateProperty(key, state) {
        this._panelService.property_activate(key, state);
    }

    getEngineDesc(id) {
        if (!this._ready || !this._engines.has(id))
            return null;

        return this._engines.get(id);
    }

    async _setEngine(id, callback) {
        // Send id even if id == this._currentEngineName
        // because 'properties-registered' signal can be emitted
        // while this._ibusSources == null on a lock screen.
        if (!this._ready) {
            if (callback)
                callback();
            return;
        }

        try {
            await this._ibus.set_global_engine_async(id,
                this._MAX_INPUT_SOURCE_ACTIVATION_TIME,
                this._cancellable);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }

        if (callback)
            callback();
    }

    async setEngine(id, callback) {
        if (this._preOskState)
            this._preOskState.engine = id;

        const isXkb = id.startsWith('xkb:');
        if (this._oskCompletion && isXkb)
            return;

        if (this._oskCompletion)
            this.setCompletionEnabled(false, callback);
        else
            await this._setEngine(id, callback);
    }

    preloadEngines(ids) {
        if (!this._ibus || !this._ready)
            return;

        if (!ids.includes(TYPING_BOOSTER_ENGINE))
            ids.push(TYPING_BOOSTER_ENGINE);

        if (this._preloadEnginesId != 0) {
            GLib.source_remove(this._preloadEnginesId);
            this._preloadEnginesId = 0;
        }

        this._preloadEnginesId =
            GLib.timeout_add_seconds(
                GLib.PRIORITY_DEFAULT,
                this._PRELOAD_ENGINES_DELAY_TIME,
                () => {
                    this._ibus.preload_engines_async(
                        ids,
                        -1,
                        this._cancellable,
                        null);
                    this._preloadEnginesId = 0;
                    return GLib.SOURCE_REMOVE;
                });
    }

    setCompletionEnabled(enabled, callback) {
        /* Needs typing-booster available */
        if (enabled && !this._engines.has(TYPING_BOOSTER_ENGINE))
            return false;
        /* Can do only on xkb engines */
        if (enabled && !this._currentEngineName.startsWith('xkb:'))
            return false;

        if (this._oskCompletion === enabled)
            return true;

        this._oskCompletion = enabled;
        let settings =
            new Gio.Settings({schema_id: IBUS_TYPING_BOOSTER_SCHEMA});

        if (enabled) {
            this._preOskState = {
                'engine': this._currentEngineName,
                'emoji': settings.get_value(KEY_EMOJIPREDICTIONS),
                'langs': settings.get_value(KEY_DICTIONARY),
                'completion': settings.get_value(KEY_INLINECOMPLETION),
                'inputMethod': settings.get_value(KEY_INPUTMETHOD),
            };
            settings.reset(KEY_EMOJIPREDICTIONS);

            const removeEncoding = l => l.replace(/\..*/, '');
            const removeDups = (l, pos, arr) => {
                return !pos || arr[pos - 1] !== l;
            };
            settings.set_string(
                KEY_DICTIONARY,
                GLib.get_language_names().map(removeEncoding)
                    .sort().filter(removeDups).join(','));

            settings.reset(KEY_INLINECOMPLETION);
            settings.set_string(KEY_INPUTMETHOD, 'NoIME');
            this._setEngine(TYPING_BOOSTER_ENGINE, callback);
        } else if (this._preOskState) {
            const {engine, emoji, langs, completion, inputMethod} =
                  this._preOskState;
            this._preOskState = null;
            this._setEngine(engine, callback);
            settings.set_value(KEY_EMOJIPREDICTIONS, emoji);
            settings.set_value(KEY_DICTIONARY, langs);
            settings.set_value(KEY_INLINECOMPLETION, completion);
            settings.set_value(KEY_INPUTMETHOD, inputMethod);
        }
        return true;
    }
};
