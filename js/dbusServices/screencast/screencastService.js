// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreencastService */

imports.gi.versions.Gst = '1.0';
imports.gi.versions.Gtk = '4.0';

const { Gio, GLib, Gst, Gtk } = imports.gi;

const { loadInterfaceXML, loadSubInterfaceXML } = imports.misc.dbusUtils;
const Signals = imports.misc.signals;
const { ServiceImplementation } = imports.dbusService;

const ScreencastIface = loadInterfaceXML('org.gnome.Shell.Screencast');

const IntrospectIface = loadInterfaceXML('org.gnome.Shell.Introspect');
const IntrospectProxy = Gio.DBusProxy.makeProxyWrapper(IntrospectIface);

const ScreenCastIface = loadSubInterfaceXML(
    'org.gnome.Mutter.ScreenCast', 'org.gnome.Mutter.ScreenCast');
const ScreenCastSessionIface = loadSubInterfaceXML(
    'org.gnome.Mutter.ScreenCast.Session', 'org.gnome.Mutter.ScreenCast');
const ScreenCastStreamIface = loadSubInterfaceXML(
    'org.gnome.Mutter.ScreenCast.Stream', 'org.gnome.Mutter.ScreenCast');
const ScreenCastProxy = Gio.DBusProxy.makeProxyWrapper(ScreenCastIface);
const ScreenCastSessionProxy = Gio.DBusProxy.makeProxyWrapper(ScreenCastSessionIface);
const ScreenCastStreamProxy = Gio.DBusProxy.makeProxyWrapper(ScreenCastStreamIface);

const DEFAULT_FRAMERATE = 30;
const DEFAULT_DRAW_CURSOR = true;

const PIPELINES = [
    {
        pipelineString:
            'capsfilter caps=video/x-raw(memory:DMABuf),max-framerate=%F/1 ! \
             glupload ! glcolorconvert ! gldownload ! \
             queue ! \
             vp8enc cpu-used=16 max-quantizer=17 deadline=1 keyframe-mode=disabled threads=%T static-threshold=1000 buffer-size=20000 ! \
             queue ! \
             webmmux',
    },
    {
        pipelineString:
            'capsfilter caps=video/x-raw,max-framerate=%F/1 ! \
             videoconvert chroma-mode=none dither=none matrix-mode=output-only n-threads=%T ! \
             queue ! \
             vp8enc cpu-used=16 max-quantizer=17 deadline=1 keyframe-mode=disabled threads=%T static-threshold=1000 buffer-size=20000 ! \
             queue ! \
             webmmux',
    },
];

const PipelineState = {
    INIT: 'INIT',
    STARTING: 'STARTING',
    PLAYING: 'PLAYING',
    FLUSHING: 'FLUSHING',
    STOPPED: 'STOPPED',
    ERROR: 'ERROR',
};

const SessionState = {
    INIT: 'INIT',
    ACTIVE: 'ACTIVE',
    STOPPED: 'STOPPED',
};

var Recorder = class extends Signals.EventEmitter {
    constructor(sessionPath, x, y, width, height, filePath, options,
        invocation) {
        super();

        this._startInvocation = invocation;
        this._dbusConnection = invocation.get_connection();
        this._stopInvocation = null;

        this._x = x;
        this._y = y;
        this._width = width;
        this._height = height;
        this._filePath = filePath;

        try {
            const dir = Gio.File.new_for_path(filePath).get_parent();
            dir.make_directory_with_parents(null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                throw e;
        }

        this._pipelineString = null;
        this._framerate = DEFAULT_FRAMERATE;
        this._drawCursor = DEFAULT_DRAW_CURSOR;

        this._pipelineState = PipelineState.INIT;
        this._pipeline = null;

        this._applyOptions(options);
        this._watchSender(invocation.get_sender());

        this._sessionState = SessionState.INIT;
        this._initSession(sessionPath);
    }

    _applyOptions(options) {
        for (const option in options)
            options[option] = options[option].deepUnpack();

        if (options['pipeline'] !== undefined)
            this._pipelineString = options['pipeline'];
        if (options['framerate'] !== undefined)
            this._framerate = options['framerate'];
        if ('draw-cursor' in options)
            this._drawCursor = options['draw-cursor'];
    }

    _addRecentItem() {
        const file = Gio.File.new_for_path(this._filePath);
        Gtk.RecentManager.get_default().add_item(file.get_uri());
    }

    _watchSender(sender) {
        this._nameWatchId = this._dbusConnection.watch_name(
            sender,
            Gio.BusNameWatcherFlags.NONE,
            null,
            this._senderVanished.bind(this));
    }

    _unwatchSender() {
        if (this._nameWatchId !== 0) {
            this._dbusConnection.unwatch_name(this._nameWatchId);
            this._nameWatchId = 0;
        }
    }

    _teardownPipeline() {
        if (!this._pipeline)
            return;

        if (this._pipeline.set_state(Gst.State.NULL) !== Gst.StateChangeReturn.SUCCESS)
            log('Failed to set pipeline state to NULL');

        this._pipelineState = PipelineState.STOPPED;
        this._pipeline = null;
    }

    _stopSession() {
        if (this._sessionState === SessionState.ACTIVE) {
            this._sessionState = SessionState.STOPPED;
            this._sessionProxy.StopSync();
        }
    }

    _bailOutOnError(error) {
        this._teardownPipeline();
        this._unwatchSender();
        this._stopSession();

        log(`Recorder error: ${error.message}`);

        if (this._startRequest) {
            this._startRequest.reject(error);
            delete this._startRequest;
        }

        if (this._stopRequest) {
            this._stopRequest.reject(error);
            delete this._stopRequest;
        }

        this.emit('error', error);
    }

    _handleFatalPipelineError(message) {
        this._pipelineState = PipelineState.ERROR;
        this._bailOutOnError(new Error(`Fatal pipeline error: ${message}`));
    }

    _senderVanished() {
        this._bailOutOnError(new Error('Sender has vanished'));
    }

    _onSessionClosed() {
        if (this._sessionState === SessionState.STOPPED)
            return; // We closed the session ourselves

        this._sessionState = SessionState.STOPPED;
        this._bailOutOnError(new Error('Session closed unexpectedly'));
    }

    _initSession(sessionPath) {
        this._sessionProxy = new ScreenCastSessionProxy(Gio.DBus.session,
            'org.gnome.Mutter.ScreenCast',
            sessionPath);
        this._sessionProxy.connectSignal('Closed', this._onSessionClosed.bind(this));
    }

    _tryNextPipeline() {
        const {done, value: pipelineConfig} = this._pipelineConfigs.next();
        if (done) {
            this._handleFatalPipelineError('All pipelines failed to start');
            return;
        }

        if (this._pipeline) {
            if (this._pipeline.set_state(Gst.State.NULL) !== Gst.StateChangeReturn.SUCCESS)
                log('Failed to set pipeline state to NULL');

            this._pipeline = null;
        }

        try {
            this._pipeline = this._createPipeline(this._nodeId, pipelineConfig,
                this._framerate);
        } catch (error) {
            this._tryNextPipeline();
            return;
        }

        if (!this._pipeline) {
            this._tryNextPipeline();
            return;
        }

        const bus = this._pipeline.get_bus();
        bus.add_watch(bus, this._onBusMessage.bind(this));

        const retval = this._pipeline.set_state(Gst.State.PLAYING);

        if (retval === Gst.StateChangeReturn.SUCCESS ||
            retval === Gst.StateChangeReturn.ASYNC) {
            // We'll wait for the state change message to PLAYING on the bus
        } else {
            this._tryNextPipeline();
        }
    }

    *_getPipelineConfigs() {
        if (this._pipelineString) {
            yield {
                pipelineString:
                    `capsfilter caps=video/x-raw,max-framerate=%F/1 ! ${this._pipelineString}`,
            };
            return;
        }

        const fallbackSupported =
                Gst.Registry.get().check_feature_version('pipewiresrc', 0, 3, 67);
        if (fallbackSupported)
            yield* PIPELINES;
        else
            yield PIPELINES.at(-1);
    }

    startRecording() {
        return new Promise((resolve, reject) => {
            this._startRequest = {resolve, reject};

            const [streamPath] = this._sessionProxy.RecordAreaSync(
                this._x, this._y,
                this._width, this._height,
                {
                    'is-recording': GLib.Variant.new('b', true),
                    'cursor-mode': GLib.Variant.new('u', this._drawCursor ? 1 : 0),
                });

            this._streamProxy = new ScreenCastStreamProxy(Gio.DBus.session,
                'org.gnome.ScreenCast.Stream',
                streamPath);

            this._streamProxy.connectSignal('PipeWireStreamAdded',
                (_proxy, _sender, params) => {
                    const [nodeId] = params;
                    this._nodeId = nodeId;

                    this._pipelineState = PipelineState.STARTING;
                    this._pipelineConfigs = this._getPipelineConfigs();
                    this._tryNextPipeline();
                });
            this._sessionProxy.StartSync();
            this._sessionState = SessionState.ACTIVE;
        });
    }

    stopRecording() {
        if (this._startRequest)
            return Promise.reject(new Error('Unable to stop recorder while still starting'));

        return new Promise((resolve, reject) => {
            this._stopRequest = {resolve, reject};

            this._pipelineState = PipelineState.FLUSHING;
            this._pipeline.send_event(Gst.Event.new_eos());
        });
    }

    _onBusMessage(bus, message, _) {
        switch (message.type) {
        case Gst.MessageType.STATE_CHANGED: {
            const [, newState] = message.parse_state_changed();

            if (this._pipelineState === PipelineState.STARTING &&
                message.src === this._pipeline &&
                newState === Gst.State.PLAYING) {
                this._pipelineState = PipelineState.PLAYING;

                this._startRequest.resolve();
                delete this._startRequest;
            }

            break;
        }

        case Gst.MessageType.EOS:
            switch (this._pipelineState) {
            case PipelineState.INIT:
            case PipelineState.STOPPED:
            case PipelineState.ERROR:
                // In these cases there should be no pipeline, so should never happen
                break;

            case PipelineState.STARTING:
                // This is something we can handle, try to switch to the next pipeline
                this._tryNextPipeline();
                break;

            case PipelineState.PLAYING:
                this._addRecentItem();
                this._handleFatalPipelineError('Unexpected EOS message');
                break;

            case PipelineState.FLUSHING:
                this._addRecentItem();

                this._teardownPipeline();
                this._unwatchSender();
                this._stopSession();

                this._stopRequest.resolve();
                delete this._stopRequest;
                break;
            default:
                break;
            }

            break;

        case Gst.MessageType.ERROR:
            switch (this._pipelineState) {
            case PipelineState.INIT:
            case PipelineState.STOPPED:
            case PipelineState.ERROR:
                // In these cases there should be no pipeline, so should never happen
                break;

            case PipelineState.STARTING:
                // This is something we can handle, try to switch to the next pipeline
                this._tryNextPipeline();
                break;

            case PipelineState.PLAYING:
            case PipelineState.FLUSHING:
                // Everything else we can't handle, so error out
                this._handleFatalPipelineError(
                    `GStreamer error while in state ${this._pipelineState}: ${message.parse_error()[0].message}`);
                break;

            default:
                break;
            }

            break;

        default:
            break;
        }
        return true;
    }

    _substituteVariables(pipelineDescr, framerate) {
        const numProcessors = GLib.get_num_processors();
        const numThreads = Math.min(Math.max(1, numProcessors), 64);
        return pipelineDescr.replaceAll('%T', numThreads).replaceAll('%F', framerate);
    }

    _createPipeline(nodeId, pipelineConfig, framerate) {
        const {pipelineString} = pipelineConfig;
        const finalPipelineString = this._substituteVariables(pipelineString, framerate);

        const fullPipeline = `
            pipewiresrc path=${nodeId}
                        do-timestamp=true
                        keepalive-time=1000
                        resend-last=true !
            ${finalPipelineString} !
            filesink location="${this._filePath}"`;

        return Gst.parse_launch_full(fullPipeline, null,
            Gst.ParseFlags.FATAL_ERRORS);
    }
};

var ScreencastService = class extends ServiceImplementation {
    static canScreencast() {
        if (!Gst.init_check(null))
            return false;

        let elements = [
            'pipewiresrc',
            'filesink',
        ];

        if (elements.some(e => Gst.ElementFactory.find(e) === null))
            return false;

        // The fallback pipeline must be available, the other ones are not
        // guaranteed to work because they depend on hw encoders.
        const fallbackPipeline = PIPELINES.at(-1);

        elements = fallbackPipeline.pipelineString.split('!').map(
            e => e.trim().split(' ').at(0));

        if (elements.every(e => Gst.ElementFactory.find(e) !== null))
            return true;

        return false;
    }

    constructor() {
        super(ScreencastIface, '/org/gnome/Shell/Screencast');

        this.hold(); // gstreamer initializing can take a bit
        this._canScreencast = ScreencastService.canScreencast();

        Gst.init(null);
        Gtk.init();

        this.release();

        this._recorders = new Map();
        this._senders = new Map();

        this._lockdownSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.lockdown',
        });

        this._proxy = new ScreenCastProxy(Gio.DBus.session,
            'org.gnome.Mutter.ScreenCast',
            '/org/gnome/Mutter/ScreenCast');

        this._introspectProxy = new IntrospectProxy(Gio.DBus.session,
            'org.gnome.Shell.Introspect',
            '/org/gnome/Shell/Introspect');
    }

    get ScreencastSupported() {
        return this._canScreencast;
    }

    _removeRecorder(sender) {
        if (!this._recorders.delete(sender))
            return;

        if (this._recorders.size === 0)
            this.release();
    }

    _addRecorder(sender, recorder) {
        this._recorders.set(sender, recorder);
        if (this._recorders.size === 1)
            this.hold();
    }

    _getAbsolutePath(filename) {
        if (GLib.path_is_absolute(filename))
            return filename;

        const videoDir =
            GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_VIDEOS) ||
            GLib.get_home_dir();

        return GLib.build_filenamev([videoDir, filename]);
    }

    _generateFilePath(template) {
        let filename = '';
        let escape = false;

        [...template].forEach(c => {
            if (escape) {
                switch (c) {
                case '%':
                    filename += '%';
                    break;
                case 'd': {
                    const datetime = GLib.DateTime.new_now_local();
                    const datestr = datetime.format('%Y-%m-%d');

                    filename += datestr;
                    break;
                }

                case 't': {
                    const datetime = GLib.DateTime.new_now_local();
                    const datestr = datetime.format('%H-%M-%S');

                    filename += datestr;
                    break;
                }

                default:
                    log(`Warning: Unknown escape ${c}`);
                }

                escape = false;
            } else if (c === '%') {
                escape = true;
            } else {
                filename += c;
            }
        });

        if (escape)
            filename += '%';

        return this._getAbsolutePath(filename);
    }

    async ScreencastAsync(params, invocation) {
        let returnValue = [false, ''];

        if (this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        const sender = invocation.get_sender();

        if (this._recorders.get(sender)) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        const [sessionPath] = this._proxy.CreateSessionSync({});

        const [fileTemplate, options] = params;
        const [screenWidth, screenHeight] = this._introspectProxy.ScreenSize;
        const filePath = this._generateFilePath(fileTemplate);

        let recorder;

        try {
            recorder = new Recorder(
                sessionPath,
                0, 0,
                screenWidth, screenHeight,
                filePath,
                options,
                invocation);
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        this._addRecorder(sender, recorder);

        try {
            await recorder.startRecording();
            returnValue = [true, filePath];
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
        } finally {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
        }

        recorder.connect('error', (r, error) => {
            this._removeRecorder(sender);
            this._dbusImpl.emit_signal('Error',
                new GLib.Variant('(s)', [error.message]));
        });
    }

    async ScreencastAreaAsync(params, invocation) {
        let returnValue = [false, ''];

        if (this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        const sender = invocation.get_sender();

        if (this._recorders.get(sender)) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        const [sessionPath] = this._proxy.CreateSessionSync({});

        const [x, y, width, height, fileTemplate, options] = params;
        const filePath = this._generateFilePath(fileTemplate);

        let recorder;

        try {
            recorder = new Recorder(
                sessionPath,
                x, y,
                width, height,
                filePath,
                options,
                invocation);
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        this._addRecorder(sender, recorder);

        try {
            await recorder.startRecording();
            returnValue = [true, filePath];
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
        } finally {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
        }

        recorder.connect('error', (r, error) => {
            this._removeRecorder(sender);
            this._dbusImpl.emit_signal('Error',
                new GLib.Variant('(s)', [error.message]));
        });
    }

    async StopScreencastAsync(params, invocation) {
        const sender = invocation.get_sender();

        const recorder = this._recorders.get(sender);
        if (!recorder) {
            invocation.return_value(GLib.Variant.new('(b)', [false]));
            return;
        }

        try {
            await recorder.stopRecording();
        } catch (error) {
            log(`${sender}: Error while stopping recorder: ${error.message}`);
        } finally {
            this._removeRecorder(sender);
            invocation.return_value(GLib.Variant.new('(b)', [true]));
        }
    }
};
