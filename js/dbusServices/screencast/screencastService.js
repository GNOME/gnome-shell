// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreencastService */

imports.gi.versions.Gtk = '4.0';

const { Gio, GLib, Gst, Gtk } = imports.gi;

const { loadInterfaceXML, loadSubInterfaceXML } = imports.misc.fileUtils;
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

const DEFAULT_PIPELINE = 'videoconvert chroma-mode=GST_VIDEO_CHROMA_MODE_NONE dither=GST_VIDEO_DITHER_NONE matrix-mode=GST_VIDEO_MATRIX_MODE_OUTPUT_ONLY n-threads=%T ! queue ! vp8enc cpu-used=16 max-quantizer=17 deadline=1 keyframe-mode=disabled threads=%T static-threshold=1000 buffer-size=20000 ! queue ! webmmux';
const DEFAULT_FRAMERATE = 30;
const DEFAULT_DRAW_CURSOR = true;

const PipelineState = {
    INIT: 0,
    PLAYING: 1,
    FLUSHING: 2,
    STOPPED: 3,
};

const SessionState = {
    INIT: 0,
    ACTIVE: 1,
    STOPPED: 2,
};

var Recorder = class {
    constructor(sessionPath, x, y, width, height, filePath, options,
        invocation,
        onErrorCallback) {
        this._startInvocation = invocation;
        this._dbusConnection = invocation.get_connection();
        this._onErrorCallback = onErrorCallback;
        this._stopInvocation = null;

        this._pipelineIsPlaying = false;
        this._sessionIsActive = false;

        this._x = x;
        this._y = y;
        this._width = width;
        this._height = height;
        this._filePath = filePath;

        this._pipelineString = DEFAULT_PIPELINE;
        this._framerate = DEFAULT_FRAMERATE;
        this._drawCursor = DEFAULT_DRAW_CURSOR;

        this._applyOptions(options);
        this._watchSender(invocation.get_sender());

        this._initSession(sessionPath);
    }

    _applyOptions(options) {
        for (const option in options)
            options[option] = options[option].deep_unpack();

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

    _senderVanished() {
        this._unwatchSender();

        this.stopRecording(null);
    }

    _notifyStopped() {
        this._unwatchSender();
        if (this._onStartedCallback)
            this._onStartedCallback(this, false);
        else if (this._onStoppedCallback)
            this._onStoppedCallback(this);
        else
            this._onErrorCallback(this);
    }

    _onSessionClosed() {
        switch (this._pipelineState) {
        case PipelineState.STOPPED:
            break;
        default:
            this._pipeline.set_state(Gst.State.NULL);
            log(`Unexpected pipeline state: ${this._pipelineState}`);
            break;
        }
        this._notifyStopped();
    }

    _initSession(sessionPath) {
        this._sessionProxy = new ScreenCastSessionProxy(Gio.DBus.session,
            'org.gnome.Mutter.ScreenCast',
            sessionPath);
        this._sessionProxy.connectSignal('Closed', this._onSessionClosed.bind(this));
    }

    _startPipeline(nodeId) {
        if (!this._ensurePipeline(nodeId))
            return;

        const bus = this._pipeline.get_bus();
        bus.add_watch(bus, this._onBusMessage.bind(this));

        this._pipeline.set_state(Gst.State.PLAYING);
        this._pipelineState = PipelineState.PLAYING;

        this._onStartedCallback(this, true);
        this._onStartedCallback = null;
    }

    startRecording(onStartedCallback) {
        this._onStartedCallback = onStartedCallback;

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
            (proxy, sender, params) => {
                const [nodeId] = params;
                this._startPipeline(nodeId);
            });
        this._sessionProxy.StartSync();
        this._sessionState = SessionState.ACTIVE;
    }

    stopRecording(onStoppedCallback) {
        this._pipelineState = PipelineState.FLUSHING;
        this._onStoppedCallback = onStoppedCallback;
        this._pipeline.send_event(Gst.Event.new_eos());
    }

    _stopSession() {
        this._sessionProxy.StopSync();
        this._sessionState = SessionState.STOPPED;
    }

    _onBusMessage(bus, message, _) {
        switch (message.type) {
        case Gst.MessageType.EOS:
            this._pipeline.set_state(Gst.State.NULL);
            this._addRecentItem();

            switch (this._pipelineState) {
            case PipelineState.FLUSHING:
                this._pipelineState = PipelineState.STOPPED;
                break;
            default:
                break;
            }

            switch (this._sessionState) {
            case SessionState.ACTIVE:
                this._stopSession();
                break;
            case SessionState.STOPPED:
                this._notifyStopped();
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

    _substituteThreadCount(pipelineDescr) {
        const numProcessors = GLib.get_num_processors();
        const numThreads = Math.min(Math.max(1, numProcessors), 64);
        return pipelineDescr.replaceAll('%T', numThreads);
    }

    _ensurePipeline(nodeId) {
        const framerate = this._framerate;

        let fullPipeline = `
            pipewiresrc path=${nodeId}
                        do-timestamp=true
                        keepalive-time=1000
                        resend-last=true !
            video/x-raw,max-framerate=${framerate}/1 !
            ${this._pipelineString} !
            filesink location="${this._filePath}"`;
        fullPipeline = this._substituteThreadCount(fullPipeline);

        try {
            this._pipeline = Gst.parse_launch_full(fullPipeline,
                null,
                Gst.ParseFlags.FATAL_ERRORS);
        }  catch (e) {
            log(`Failed to create pipeline: ${e}`);
            this._notifyStopped();
        }
        return !!this._pipeline;
    }
};

var ScreencastService = class extends ServiceImplementation {
    constructor() {
        super(ScreencastIface, '/org/gnome/Shell/Screencast');

        Gst.init(null);
        Gtk.init();

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

    _removeRecorder(sender) {
        this._recorders.delete(sender);
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

        let videoDir = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_VIDEOS);
        if (!GLib.file_test(videoDir, GLib.FileTest.EXISTS))
            videoDir = GLib.get_home_dir();

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
                    const datestr = datetime.format('%0x');
                    const datestrEscaped = datestr.replace(/\//g, '-');

                    filename += datestrEscaped;
                    break;
                }

                case 't': {
                    const datetime = GLib.DateTime.new_now_local();
                    const datestr = datetime.format('%0X');
                    const datestrEscaped = datestr.replace(/\//g, ':');

                    filename += datestrEscaped;
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

    ScreencastAsync(params, invocation) {
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
                invocation,
                _recorder => this._removeRecorder(sender));
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        this._addRecorder(sender, recorder);

        try {
            recorder.startRecording(
                (_, result) => {
                    if (result) {
                        returnValue = [true, filePath];
                        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
                    } else {
                        this._removeRecorder(sender);
                        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
                    }
                });
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
        }
    }

    ScreencastAreaAsync(params, invocation) {
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
                invocation,
                _recorder => this._removeRecorder(sender));
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        this._addRecorder(sender, recorder);

        try {
            recorder.startRecording(
                (_, result) => {
                    if (result) {
                        returnValue = [true, filePath];
                        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
                    } else {
                        this._removeRecorder(sender);
                        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
                    }
                });
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
        }
    }

    StopScreencastAsync(params, invocation) {
        const sender = invocation.get_sender();

        const recorder = this._recorders.get(sender);
        if (!recorder) {
            invocation.return_value(GLib.Variant.new('(b)', [false]));
            return;
        }

        recorder.stopRecording(() => {
            this._removeRecorder(sender);
            invocation.return_value(GLib.Variant.new('(b)', [true]));
        });
    }
};
