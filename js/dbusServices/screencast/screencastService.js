import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Gst from 'gi://Gst?version=1.0';
import Gtk from 'gi://Gtk?version=4.0';

import {ServiceImplementation} from './dbusService.js';

import {ScreencastErrors, ScreencastError} from './misc/dbusErrors.js';
import {loadInterfaceXML, loadSubInterfaceXML} from './misc/dbusUtils.js';
import * as Signals from './misc/signals.js';

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

const PIPELINE_BLOCKLIST_FILENAME = 'gnome-shell-screencast-pipeline-blocklist';

const PIPELINES = [
    {
        id: 'hwenc-dmabuf-h264-vaapi-lp',
        fileExtension: 'mp4',
        pipelineString:
            'capsfilter caps=video/x-raw(memory:DMABuf),format=DMA_DRM,max-framerate=%F/1 ! \
             vapostproc ! \
             vah264lpenc ! \
             queue ! \
             h264parse ! \
             mp4mux fragment-duration=500 fragment-mode=first-moov-then-finalise',
    },
    {
        id: 'hwenc-dmabuf-h264-vaapi',
        fileExtension: 'mp4',
        pipelineString:
            'capsfilter caps=video/x-raw(memory:DMABuf),format=DMA_DRM,max-framerate=%F/1 ! \
             vapostproc ! \
             vah264enc ! \
             queue ! \
             h264parse ! \
             mp4mux fragment-duration=500 fragment-mode=first-moov-then-finalise',
    },
    {
        id: 'swenc-dmabuf-h264-openh264',
        fileExtension: 'mp4',
        pipelineString:
            'capsfilter caps=video/x-raw(memory:DMABuf),max-framerate=%F/1 ! \
             glupload ! glcolorconvert ! gldownload ! \
             queue ! \
             openh264enc deblocking=off background-detection=false complexity=low adaptive-quantization=false qp-max=26 qp-min=26 multi-thread=%T slice-mode=auto ! \
             queue ! \
             h264parse ! \
             mp4mux fragment-duration=500 fragment-mode=first-moov-then-finalise',
    },
    {
        id: 'swenc-memfd-h264-openh264',
        fileExtension: 'mp4',
        pipelineString:
            'capsfilter caps=video/x-raw,max-framerate=%F/1 ! \
             videoconvert chroma-mode=none dither=none matrix-mode=output-only n-threads=%T ! \
             queue ! \
             openh264enc deblocking=off background-detection=false complexity=low adaptive-quantization=false qp-max=26 qp-min=26 multi-thread=%T slice-mode=auto ! \
             queue ! \
             h264parse ! \
             mp4mux fragment-duration=500 fragment-mode=first-moov-then-finalise',
    },
    {
        id: 'swenc-dmabuf-vp8-vp8enc',
        fileExtension: 'webm',
        pipelineString:
            'capsfilter caps=video/x-raw(memory:DMABuf),max-framerate=%F/1 ! \
             glupload ! glcolorconvert ! gldownload ! \
             queue ! \
             vp8enc cpu-used=16 max-quantizer=17 deadline=1 keyframe-mode=disabled threads=%T static-threshold=1000 buffer-size=20000 ! \
             queue ! \
             webmmux',
    },
    {
        id: 'swenc-memfd-vp8-vp8enc',
        fileExtension: 'webm',
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

class Recorder extends Signals.EventEmitter {
    constructor(sessionPath, x, y, width, height, filePathStem, options,
        invocation) {
        super();

        this._dbusConnection = invocation.get_connection();

        this._x = x;
        this._y = y;
        this._width = width;
        this._height = height;
        this._filePathStem = filePathStem;

        try {
            const dir = Gio.File.new_for_path(filePathStem).get_parent();
            dir.make_directory_with_parents(null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                throw e;
        }

        this._pipelineString = null;
        this._framerate = DEFAULT_FRAMERATE;
        this._drawCursor = DEFAULT_DRAW_CURSOR;
        this._blocklistFromPreviousCrashes = [];

        const pipelineBlocklistPath = GLib.build_filenamev(
            [GLib.get_user_runtime_dir(), PIPELINE_BLOCKLIST_FILENAME]);
        this._pipelineBlocklistFile = Gio.File.new_for_path(pipelineBlocklistPath);

        try {
            const [success_, contents] = this._pipelineBlocklistFile.load_contents(null);
            const decoder = new TextDecoder();
            this._blocklistFromPreviousCrashes = JSON.parse(decoder.decode(contents));
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                throw e;
        }

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

    _bailOutOnError(message, errorDomain = ScreencastErrors, errorCode = ScreencastError.RECORDER_ERROR) {
        const error = new GLib.Error(errorDomain, errorCode, message);

        // If it's a PIPELINE_ERROR, we want to leave the failing pipeline on the
        // blocklist for the next time. Other errors are pipeline-independent, so
        // reset the blocklist to allow the pipeline to be tried again next time.
        if (!error.matches(ScreencastErrors, ScreencastError.PIPELINE_ERROR))
            this._updateServiceCrashBlocklist([...this._blocklistFromPreviousCrashes]);

        this._teardownPipeline();
        this._unwatchSender();
        this._stopSession();

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

    _handleFatalPipelineError(message, errorDomain, errorCode) {
        this._pipelineState = PipelineState.ERROR;
        this._bailOutOnError(message, errorDomain, errorCode);
    }

    _senderVanished() {
        this._bailOutOnError('Sender has vanished');
    }

    _onSessionClosed() {
        if (this._sessionState === SessionState.STOPPED)
            return; // We closed the session ourselves

        this._sessionState = SessionState.STOPPED;
        this._bailOutOnError('Session closed unexpectedly');
    }

    _initSession(sessionPath) {
        this._sessionProxy = new ScreenCastSessionProxy(Gio.DBus.session,
            'org.gnome.Mutter.ScreenCast',
            sessionPath);
        this._sessionProxy.connectSignal('Closed', this._onSessionClosed.bind(this));
    }

    _updateServiceCrashBlocklist(blocklist) {
        try {
            if (blocklist.length === 0) {
                this._pipelineBlocklistFile.delete(null);
            } else {
                this._pipelineBlocklistFile.replace_contents(
                    JSON.stringify(blocklist), null, false,
                    Gio.FileCreateFlags.NONE, null);
            }
        } catch (e) {
            console.log(`Failed to update pipeline-blocklist file: ${e.message}`);
        }
    }

    _tryNextPipeline() {
        if (this._filePath) {
            GLib.unlink(this._filePath);
            delete this._filePath;
        }

        const {done, value: pipelineConfig} = this._pipelineConfigs.next();
        if (done) {
            this._handleFatalPipelineError('All pipelines failed to start',
                ScreencastErrors, ScreencastError.ALL_PIPELINES_FAILED);
            return;
        }

        if (this._blocklistFromPreviousCrashes.includes(pipelineConfig.id)) {
            console.info(`Skipping pipeline '${pipelineConfig.id}' due to pipeline blocklist`);
            this._tryNextPipeline();
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

            // Add the current pipeline to the blocklist, so it is skipped next
            // time in case we crash; we'll remove it again on success or on
            // non-pipeline-related failures.
            this._updateServiceCrashBlocklist(
                [...this._blocklistFromPreviousCrashes, pipelineConfig.id]);
        } catch {
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
                'org.gnome.Mutter.ScreenCast',
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

                this._startRequest.resolve(this._filePath);
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
                this._handleFatalPipelineError('Unexpected EOS message',
                    ScreencastErrors, ScreencastError.PIPELINE_ERROR);
                break;

            case PipelineState.FLUSHING:
                // The pipeline ran successfully and we didn't crash; we can remove it
                // from the blocklist again now.
                this._updateServiceCrashBlocklist([...this._blocklistFromPreviousCrashes]);

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
            case PipelineState.FLUSHING: {
                const [error] = message.parse_error();

                if (error.matches(Gst.ResourceError, Gst.ResourceError.NO_SPACE_LEFT)) {
                    this._handleFatalPipelineError('Out of disk space',
                        ScreencastErrors, ScreencastError.OUT_OF_DISK_SPACE);
                } else {
                    this._handleFatalPipelineError(
                        `GStreamer error while in state ${this._pipelineState}: ${error.message}`,
                        ScreencastErrors, ScreencastError.PIPELINE_ERROR);
                }

                break;
            }

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
        const {fileExtension, pipelineString} = pipelineConfig;
        const finalPipelineString = this._substituteVariables(pipelineString, framerate);
        this._filePath = `${this._filePathStem}.${fileExtension}`;

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
}

export const ScreencastService = class extends ServiceImplementation {
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

        // FIXME: temporarily detect and strip .webm prefix to avoid breaking
        // external consumers of our API, remove this again
        if (template.endsWith('.webm')) {
            console.log("'file_template' for screencast includes '.webm' file-extension. Passing the file-extension as part of the filename has been deprecated, pass the 'file_template' without a file-extension instead.");
            template = template.substring(0, template.length - '.webm'.length);
        }

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
        if (this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.SAVE_TO_DISK_DISABLED,
                'Saving to disk is disabled');
            return;
        }

        const sender = invocation.get_sender();

        if (this._recorders.get(sender)) {
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.ALREADY_RECORDING,
                'Service is already recording');
            return;
        }

        const [sessionPath] = this._proxy.CreateSessionSync({});

        const [fileTemplate, options] = params;
        const [screenWidth, screenHeight] = this._introspectProxy.ScreenSize;
        const filePathStem = this._generateFilePath(fileTemplate);

        let recorder;

        try {
            recorder = new Recorder(
                sessionPath,
                0, 0,
                screenWidth, screenHeight,
                filePathStem,
                options,
                invocation);
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.RECORDER_ERROR,
                error.message);

            return;
        }

        this._addRecorder(sender, recorder);

        try {
            const pathWithExtension = await recorder.startRecording();
            invocation.return_value(GLib.Variant.new('(bs)', [true, pathWithExtension]));
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
            if (error instanceof GLib.Error) {
                invocation.return_gerror(error);
            } else {
                invocation.return_error_literal(ScreencastErrors,
                    ScreencastError.RECORDER_ERROR,
                    error.message);
            }

            return;
        }

        recorder.connect('error', (r, error) => {
            log(`Fatal error while recording: ${error.message}`);
            this._removeRecorder(sender);
            this._dbusImpl.emit_signal('Error',
                new GLib.Variant('(ss)', [
                    Gio.DBusError.encode_gerror(error),
                    error.message,
                ]));
        });
    }

    async ScreencastAreaAsync(params, invocation) {
        if (this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.SAVE_TO_DISK_DISABLED,
                'Saving to disk is disabled');
            return;
        }

        const sender = invocation.get_sender();

        if (this._recorders.get(sender)) {
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.ALREADY_RECORDING,
                'Service is already recording');
            return;
        }

        const [sessionPath] = this._proxy.CreateSessionSync({});

        const [x, y, width, height, fileTemplate, options] = params;
        const filePathStem = this._generateFilePath(fileTemplate);

        let recorder;

        try {
            recorder = new Recorder(
                sessionPath,
                x, y,
                width, height,
                filePathStem,
                options,
                invocation);
        } catch (error) {
            log(`Failed to create recorder: ${error.message}`);
            invocation.return_error_literal(ScreencastErrors,
                ScreencastError.RECORDER_ERROR,
                error.message);

            return;
        }

        this._addRecorder(sender, recorder);

        try {
            const pathWithExtension = await recorder.startRecording();
            invocation.return_value(GLib.Variant.new('(bs)', [true, pathWithExtension]));
        } catch (error) {
            log(`Failed to start recorder: ${error.message}`);
            this._removeRecorder(sender);
            if (error instanceof GLib.Error) {
                invocation.return_gerror(error);
            } else {
                invocation.return_error_literal(ScreencastErrors,
                    ScreencastError.RECORDER_ERROR,
                    error.message);
            }

            return;
        }

        recorder.connect('error', (r, error) => {
            log(`Fatal error while recording: ${error.message}`);
            this._removeRecorder(sender);
            this._dbusImpl.emit_signal('Error',
                new GLib.Variant('(ss)', [
                    Gio.DBusError.encode_gerror(error),
                    error.message,
                ]));
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
