// READ THIS FIRST
// Background handling is a maze of objects, both objects in this file, and
// also objects inside Mutter. They all have a role.
//
// BackgroundManager
//   The only object that other parts of GNOME Shell deal with; a
//   BackgroundManager creates background actors and adds them to
//   the specified container. When the background is changed by the
//   user it will fade out the old actor and fade in the new actor.
//   (This is separate from the fading for an animated background,
//   since using two actors is quite inefficient.)
//
// BackgroundTextureCache
//   Shell-side cache from filename to CoglTexture. Handles image loading
//   using glycin, creates textures, and manages GL video memory purge events.
//
// BackgroundSource
//   An object that is created for each GSettings schema (separate
//   settings schemas are used for the lock screen and main background),
//   and holds a reference to shared Background objects.
//
// MetaBackground
//   Holds the specification of a background - a background color
//   or gradient and one or two textures blended together.
//
// Background
//   JS delegate object that connects a MetaBackground to the GSettings
//   schema for the background. Loads images via BackgroundTextureCache
//   and provides textures to MetaBackground.
//
// Animation
//   A helper object that handles loading a XML-based animation; it is a
//   wrapper for GnomeDesktop.BGSlideShow
//
// MetaBackgroundActor
//   An actor that draws the background for a single monitor
//
// BackgroundCache
//   A cache of Settings schema => BackgroundSource and of a single Animation.
//   Also used to share file monitors.
//
// A static image, background color or gradient is relatively straightforward. The
// calling code creates a separate BackgroundManager for each monitor. Since they
// are created for the same GSettings schema, they will use the same BackgroundSource
// object, which provides a single Background and correspondingly a single
// MetaBackground object.
//
// BackgroundManager               BackgroundManager
//        |        \               /        |
//        |         BackgroundSource        |        looked up in BackgroundCache
//        |                |                |
//        |            Background           |
//        |                |                |
//   MetaBackgroundActor   |    MetaBackgroundActor
//         \               |               /
//          `------- MetaBackground ------'
//                         |
//                   CoglTexture                 looked up in BackgroundTextureCache
//
// The animated case is trickier because the animation XML file can specify different
// files for different monitor resolutions and aspect ratios. For this reason,
// the BackgroundSource provides different Background objects that share a single
// Animation object, which tracks the animation, but use different MetaBackground
// objects. In the common case, the different MetaBackground objects will be created
// for the same filename and look up the *same* CoglTexture object, so there is
// little wasted memory:
//
// BackgroundManager               BackgroundManager
//        |        \               /        |
//        |         BackgroundSource        |        looked up in BackgroundCache
//        |             /      \            |
//        |     Background   Background     |
//        |       |     \      /   |        |
//        |       |    Animation   |        |        looked up in BackgroundCache
// MetaBackgroundA|tor           Me|aBackgroundActor
//         \      |                |       /
//      MetaBackground           MetaBackground
//                 \                 /
//                   CoglTexture                 looked up in BackgroundTextureCache
//                   CoglTexture
//
// But the case of different filenames and different background images
// is possible as well:
//                        ....
//      MetaBackground              MetaBackground
//             |                          |
//        CoglTexture                CoglTexture
//        CoglTexture                CoglTexture

import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import GDesktopEnums from 'gi://GDesktopEnums';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Glycin from 'gi://Gly';
import GnomeBG from 'gi://GnomeBG';
import GnomeDesktop from 'gi://GnomeDesktop';
import Meta from 'gi://Meta';
import * as Signals from '../misc/signals.js';

import * as LoginManager from '../misc/loginManager.js';
import * as Main from './main.js';
import * as Params from '../misc/params.js';

const DEFAULT_BACKGROUND_COLOR = new Cogl.Color({red: 40, green: 40, blue: 40, alpha: 255});

const BACKGROUND_SCHEMA = 'org.gnome.desktop.background';
const PRIMARY_COLOR_KEY = 'primary-color';
const SECONDARY_COLOR_KEY = 'secondary-color';
const COLOR_SHADING_TYPE_KEY = 'color-shading-type';
const BACKGROUND_STYLE_KEY = 'picture-options';
const PICTURE_URI_KEY = 'picture-uri';
const PICTURE_URI_DARK_KEY = 'picture-uri-dark';

const INTERFACE_SCHEMA = 'org.gnome.desktop.interface';
const COLOR_SCHEME_KEY = 'color-scheme';

export const FADE_ANIMATION_TIME = 1000;

// These parameters affect how often we redraw.
// The first is how different (percent crossfaded) the slide show
// has to look before redrawing and the second is the minimum
// frequency (in seconds) we're willing to wake up
const ANIMATION_OPACITY_STEP_INCREMENT = 4.0;
const ANIMATION_MIN_WAKEUP_INTERVAL = 1.0;

let _backgroundCache = null;
let _backgroundTextureCache = null;

function _fileEqual0(file1, file2) {
    if (file1 === file2)
        return true;

    if (!file1 || !file2)
        return false;

    return file1.equal(file2);
}

class BackgroundCache extends Signals.EventEmitter {
    constructor() {
        super();

        this._fileMonitors = {};
        this._backgroundSources = {};
        this._animations = {};
    }

    monitorFile(file) {
        const key = file.hash();
        if (this._fileMonitors[key])
            return;

        const monitor = file.monitor(Gio.FileMonitorFlags.NONE, null);
        monitor.connect('changed',
            (obj, theFile, otherFile, eventType) => {
                // Ignore CHANGED and CREATED events, since in both cases
                // we'll get a CHANGES_DONE_HINT event when done.
                if (eventType !== Gio.FileMonitorEvent.CHANGED &&
                    eventType !== Gio.FileMonitorEvent.CREATED)
                    this.emit('file-changed', file);
            });

        this._fileMonitors[key] = monitor;
    }

    getAnimation(params) {
        params = Params.parse(params, {
            file: null,
            settingsSchema: null,
            onLoaded: null,
        });

        let animation = this._animations[params.settingsSchema];
        if (animation && _fileEqual0(animation.file, params.file)) {
            if (params.onLoaded) {
                const id = GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
                    params.onLoaded(this._animations[params.settingsSchema]);
                });
                GLib.Source.set_name_by_id(id, '[gnome-shell] params.onLoaded');
            }
            return;
        }

        animation = new Animation({file: params.file});

        animation.load_async(null, () => {
            this._animations[params.settingsSchema] = animation;

            if (params.onLoaded) {
                const id = GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
                    params.onLoaded(this._animations[params.settingsSchema]);
                });
                GLib.Source.set_name_by_id(id, '[gnome-shell] params.onLoaded');
            }
        });
    }

    getBackgroundSource(layoutManager, settingsSchema) {
        // The layoutManager is always the same one; we pass in it since
        // Main.layoutManager may not be set yet

        if (!(settingsSchema in this._backgroundSources)) {
            this._backgroundSources[settingsSchema] = new BackgroundSource(layoutManager, settingsSchema);
            this._backgroundSources[settingsSchema]._useCount = 1;
        } else {
            this._backgroundSources[settingsSchema]._useCount++;
        }

        return this._backgroundSources[settingsSchema];
    }

    releaseBackgroundSource(settingsSchema) {
        if (settingsSchema in this._backgroundSources) {
            const source = this._backgroundSources[settingsSchema];
            source._useCount--;
            if (source._useCount === 0) {
                delete this._backgroundSources[settingsSchema];
                source.destroy();
            }
        }
    }
}

/**
 * @returns {BackgroundCache}
 */
function getBackgroundCache() {
    if (!_backgroundCache)
        _backgroundCache = new BackgroundCache();
    return _backgroundCache;
}

class BackgroundTextureCache {
    constructor() {
        this._textures = new Map(); // uri -> {texture, colorState}

        global.display.connect('gl-video-memory-purged', () => {
            this._textures.clear();
        });
    }

    async load(file, cancellable) {
        const uri = file.get_uri();

        if (this._textures.has(uri))
            return this._textures.get(uri);

        // Load image using glycin
        const [frameData, colorState] = await this._loadGlycinFrame(file, cancellable);

        // Create CoglTexture from glycin frame data
        const texture = this._createTexture(frameData);

        const entry = {texture, colorState};
        this._textures.set(uri, entry);
        return entry;
    }

    async _loadGlycinFrame(file, cancellable) {
        const stream = await file.read_async(GLib.PRIORITY_DEFAULT, cancellable);
        const loader = Glycin.Loader.new_for_stream(stream);

        loader.set_accepted_memory_formats(
            Glycin.MemoryFormatSelection.B8G8R8A8_PREMULTIPLIED |
            Glycin.MemoryFormatSelection.A8R8G8B8_PREMULTIPLIED |
            Glycin.MemoryFormatSelection.R8G8B8A8_PREMULTIPLIED |
            Glycin.MemoryFormatSelection.B8G8R8A8 |
            Glycin.MemoryFormatSelection.A8R8G8B8 |
            Glycin.MemoryFormatSelection.R8G8B8A8 |
            Glycin.MemoryFormatSelection.A8B8G8R8 |
            Glycin.MemoryFormatSelection.R8G8B8 |
            Glycin.MemoryFormatSelection.B8G8R8 |
            Glycin.MemoryFormatSelection.R16G16B16A16_PREMULTIPLIED |
            Glycin.MemoryFormatSelection.R16G16B16A16 |
            Glycin.MemoryFormatSelection.R16G16B16A16_FLOAT |
            Glycin.MemoryFormatSelection.R32G32B32A32_FLOAT_PREMULTIPLIED |
            Glycin.MemoryFormatSelection.R32G32B32A32_FLOAT
        );

        const image = loader.load();
        const frame = image.next_frame();

        const width = frame.get_width();
        const height = frame.get_height();
        const stride = frame.get_stride();
        const bytes = frame.get_buf_bytes();
        const format = frame.get_memory_format();
        const cicp = frame.get_color_cicp();

        let colorState = null;
        if (cicp) {
            const clutterCicp = new Clutter.Cicp({
                primaries: cicp.color_primaries,
                transfer: cicp.transfer_characteristics,
                matrix_coefficients: cicp.matrix_coefficients,
                video_full_range_flag: cicp.video_full_range_flag,
            });

            try {
                const clutterContext = global.stage.context;
                colorState = Clutter.ColorStateParams.new_from_cicp(clutterContext, clutterCicp);
            } catch (e) {
                logError(e, 'Failed to create color state from CICP');
            }
        }

        return [{width, height, stride, bytes, format}, colorState];
    }

    _glyMemoryFormatToCogl(format) {
        switch (format) {
        case Glycin.MemoryFormat.B8G8R8A8_PREMULTIPLIED:
            return Cogl.PixelFormat.BGRA_8888_PRE;
        case Glycin.MemoryFormat.A8R8G8B8_PREMULTIPLIED:
            return Cogl.PixelFormat.ARGB_8888_PRE;
        case Glycin.MemoryFormat.R8G8B8A8_PREMULTIPLIED:
            return Cogl.PixelFormat.RGBA_8888_PRE;
        case Glycin.MemoryFormat.B8G8R8A8:
            return Cogl.PixelFormat.BGRA_8888;
        case Glycin.MemoryFormat.A8R8G8B8:
            return Cogl.PixelFormat.ARGB_8888;
        case Glycin.MemoryFormat.R8G8B8A8:
            return Cogl.PixelFormat.RGBA_8888;
        case Glycin.MemoryFormat.A8B8G8R8:
            return Cogl.PixelFormat.ABGR_8888;
        case Glycin.MemoryFormat.R8G8B8:
            return Cogl.PixelFormat.RGB_888;
        case Glycin.MemoryFormat.B8G8R8:
            return Cogl.PixelFormat.BGR_888;
        case Glycin.MemoryFormat.R16G16B16A16_PREMULTIPLIED:
            return Cogl.PixelFormat.RGBA_16161616_PRE;
        case Glycin.MemoryFormat.R16G16B16A16:
            return Cogl.PixelFormat.RGBA_16161616;
        case Glycin.MemoryFormat.R16G16B16A16_FLOAT:
            return Cogl.PixelFormat.RGBA_FP_16161616;
        case Glycin.MemoryFormat.R32G32B32A32_FLOAT_PREMULTIPLIED:
            return Cogl.PixelFormat.RGBA_FP_32323232_PRE;
        case Glycin.MemoryFormat.R32G32B32A32_FLOAT:
            return Cogl.PixelFormat.RGBA_FP_32323232;
        default:
            throw new Error(`Unsupported glycin memory format: ${format}`);
        }
    }

    _createTexture(frameData) {
        const {width, height, stride, bytes, format} = frameData;

        const coglFormat = this._glyMemoryFormatToCogl(format);
        const data = bytes.get_data();
        const clutterContext = global.stage.context;
        const clutterBackend = clutterContext.get_backend();
        const ctx = clutterBackend.get_cogl_context();

        const hasAlpha = Glycin.memory_format_has_alpha(format);
        const components = hasAlpha
            ? Cogl.TextureComponents.RGBA
            : Cogl.TextureComponents.RGB;

        let texture = Cogl.Texture2D.new_with_size(ctx, width, height);
        texture.set_components(components);

        // Try to allocate
        // if it fails (texture too large), use sliced
        try {
            texture.allocate();
        } catch {
            texture = Cogl.Texture2DSliced.new_with_size(ctx, width, height, Cogl.TEXTURE_MAX_WASTE);
            texture.set_components(components);
        }

        if (!texture.set_data(coglFormat, stride, data, 0))
            throw new Error('Failed to set texture data');

        return texture;
    }

    purge(file) {
        this._textures.delete(file.get_uri());
    }
}

/**
 * @returns {BackgroundTextureCache}
 */
function getBackgroundTextureCache() {
    if (!_backgroundTextureCache)
        _backgroundTextureCache = new BackgroundTextureCache();
    return _backgroundTextureCache;
}

const Background = GObject.registerClass({
    Signals: {'loaded': {}, 'bg-changed': {}},
}, class Background extends Meta.Background {
    _init(params) {
        params = Params.parse(params, {
            monitorIndex: 0,
            layoutManager: Main.layoutManager,
            settings: null,
            file: null,
            style: null,
        });

        super._init({meta_display: global.display});

        this._settings = params.settings;
        this._file = params.file;
        this._style = params.style;
        this._monitorIndex = params.monitorIndex;
        this._layoutManager = params.layoutManager;
        this._fileWatches = {};
        this._cancellable = new Gio.Cancellable();
        this.isLoaded = false;

        this._interfaceSettings = new Gio.Settings({schema_id: INTERFACE_SCHEMA});

        this._clock = new GnomeDesktop.WallClock();
        this._clock.connectObject('notify::timezone',
            () => {
                if (this._animation)
                    this._loadAnimation(this._animation.file);
            }, this);

        const loginManager = LoginManager.getLoginManager();
        loginManager.connectObject('prepare-for-sleep',
            (lm, aboutToSuspend) => {
                if (aboutToSuspend)
                    return;
                this._refreshAnimation();
            }, this);

        this._settings.connectObject('changed',
            this._emitChangedSignal.bind(this), this);

        this._interfaceSettings.connectObject(`changed::${COLOR_SCHEME_KEY}`,
            this._emitChangedSignal.bind(this), this);

        this._videoMemoryPurgedId = global.display.connect('gl-video-memory-purged',
            () => this._load());

        this._load();
    }

    destroy() {
        this._cancellable.cancel();
        this._removeAnimationTimeout();

        let i;
        const keys = Object.keys(this._fileWatches);
        for (i = 0; i < keys.length; i++)
            this._cache.disconnect(this._fileWatches[keys[i]]);

        this._fileWatches = null;

        this._clock.disconnectObject(this);
        this._clock = null;

        global.display.disconnect(this._videoMemoryPurgedId);
        LoginManager.getLoginManager().disconnectObject(this);
        this._settings.disconnectObject(this);
        this._interfaceSettings.disconnectObject(this);

        if (this._changedIdleId) {
            GLib.source_remove(this._changedIdleId);
            this._changedIdleId = 0;
        }
    }

    _emitChangedSignal() {
        if (this._changedIdleId)
            return;

        this._changedIdleId = GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
            this._changedIdleId = 0;
            this.emit('bg-changed');
        });
        GLib.Source.set_name_by_id(this._changedIdleId,
            '[gnome-shell] Background._emitChangedSignal');
    }

    updateResolution() {
        if (this._animation)
            this._refreshAnimation();
    }

    _refreshAnimation() {
        if (!this._animation)
            return;

        this._removeAnimationTimeout();
        this._updateAnimation();
    }

    _setLoaded() {
        if (this.isLoaded)
            return;

        this.isLoaded = true;
        if (this._cancellable?.is_cancelled())
            return;

        const id = GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
            this.emit('loaded');
        });
        GLib.Source.set_name_by_id(id, '[gnome-shell] Background._setLoaded Idle');
    }

    _loadPattern() {
        let colorString = this._settings.get_string(PRIMARY_COLOR_KEY);
        const [, color] = Cogl.Color.from_string(colorString);
        colorString = this._settings.get_string(SECONDARY_COLOR_KEY);
        const [, secondColor] = Cogl.Color.from_string(colorString);

        const shadingType = this._settings.get_enum(COLOR_SHADING_TYPE_KEY);

        if (shadingType === GDesktopEnums.BackgroundShading.SOLID)
            this.set_color(color);
        else
            this.set_gradient(shadingType, color, secondColor);
    }

    _watchFile(file) {
        const key = file.hash();
        if (this._fileWatches[key])
            return;

        this._cache.monitorFile(file);
        const signalId = this._cache.connect('file-changed',
            (cache, changedFile) => {
                if (changedFile.equal(file)) {
                    const textureCache = getBackgroundTextureCache();
                    textureCache.purge(changedFile);
                    this._emitChangedSignal();
                }
            });
        this._fileWatches[key] = signalId;
    }

    _removeAnimationTimeout() {
        if (this._updateAnimationTimeoutId) {
            GLib.source_remove(this._updateAnimationTimeoutId);
            this._updateAnimationTimeoutId = 0;
        }
    }

    async _updateAnimation() {
        this._updateAnimationTimeoutId = 0;

        this._animation.update(this._layoutManager.monitors[this._monitorIndex]);
        const files = this._animation.keyFrameFiles;

        if (files.length === 0) {
            this.set_file(null, this._style);
            this._setLoaded();
            this._queueUpdateAnimation();
            return;
        }

        const cache = getBackgroundTextureCache();

        try {
            const entries = await Promise.all(
                files.map(f => {
                    this._watchFile(f);
                    return cache.load(f, this._cancellable);
                })
            );

            const textures = entries.map(e => e.texture);
            const colorState = entries[0]?.colorState || null;

            if (textures.length > 1) {
                this.set_blend_textures(
                    textures[0],
                    textures[1],
                    this._animation.transitionProgress,
                    this._style,
                    colorState
                );
            } else if (textures.length > 0) {
                this.set_texture(textures[0], this._style, colorState);
            }

            this._setLoaded();
            this._queueUpdateAnimation();
        } catch (err) {
            if (!err.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(err, 'Failed to load animation');
            this._setLoaded();
        }
    }

    _queueUpdateAnimation() {
        if (this._updateAnimationTimeoutId !== 0)
            return;

        if (!this._cancellable || this._cancellable.is_cancelled())
            return;

        if (!this._animation.transitionDuration)
            return;

        const nSteps = 255 / ANIMATION_OPACITY_STEP_INCREMENT;
        const timePerStep = (this._animation.transitionDuration * 1000) / nSteps;

        const interval = Math.max(
            ANIMATION_MIN_WAKEUP_INTERVAL * 1000,
            timePerStep);

        if (interval > GLib.MAXUINT32)
            return;

        this._updateAnimationTimeoutId = GLib.timeout_add_once(GLib.PRIORITY_DEFAULT,
            interval,
            () => {
                this._updateAnimationTimeoutId = 0;
                this._updateAnimation();
            });
        GLib.Source.set_name_by_id(this._updateAnimationTimeoutId, '[gnome-shell] this._updateAnimation');
    }

    _loadAnimation(file) {
        this._cache.getAnimation({
            file,
            settingsSchema: this._settings.schema_id,
            onLoaded: animation => {
                this._animation = animation;

                if (!this._animation || this._cancellable.is_cancelled()) {
                    this._setLoaded();
                    return;
                }

                this._updateAnimation();
                this._watchFile(file);
            },
        });
    }

    async _loadImage(file) {
        this._watchFile(file);

        const cache = getBackgroundTextureCache();

        try {
            const {texture, colorState} = await cache.load(file, this._cancellable);
            this.set_texture(texture, this._style, colorState);
            this._setLoaded();
        } catch (err) {
            if (!err.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(err, 'Failed to load background');
            this._setLoaded();
        }
    }

    async _loadFile(file) {
        let info;
        try {
            info = await file.query_info_async(
                Gio.FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                Gio.FileQueryInfoFlags.NONE,
                0,
                this._cancellable);
        } catch {
            this._setLoaded();
            return;
        }

        const contentType = info.get_content_type();
        if (contentType === 'application/xml')
            this._loadAnimation(file);
        else
            this._loadImage(file);
    }

    _load() {
        this._cache = getBackgroundCache();

        this._loadPattern();

        if (!this._file) {
            this._setLoaded();
            return;
        }

        this._loadFile(this._file).catch(logError);
    }
});

let _systemBackground;

export const SystemBackground = GObject.registerClass({
    Signals: {'loaded': {}},
}, class SystemBackground extends Meta.BackgroundActor {
    _init() {
        if (_systemBackground == null) {
            _systemBackground = new Meta.Background({meta_display: global.display});
            _systemBackground.set_color(DEFAULT_BACKGROUND_COLOR);
        }

        super._init({
            meta_display: global.display,
            monitor: 0,
        });
        this.content.background = _systemBackground;

        const id = GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
            this.emit('loaded');
        });
        GLib.Source.set_name_by_id(id, '[gnome-shell] SystemBackground.loaded');
    }
});

class BackgroundSource {
    constructor(layoutManager, settingsSchema) {
        // Allow override the background image setting for performance testing
        this._layoutManager = layoutManager;
        this._overrideImage = GLib.getenv('SHELL_BACKGROUND_IMAGE');
        this._settings = new Gio.Settings({schema_id: settingsSchema});
        this._backgrounds = [];

        const monitorManager = global.backend.get_monitor_manager();
        this._monitorsChangedId =
            monitorManager.connect('monitors-changed',
                this._onMonitorsChanged.bind(this));

        this._interfaceSettings = new Gio.Settings({schema_id: INTERFACE_SCHEMA});
    }

    _onMonitorsChanged() {
        for (const monitorIndex in this._backgrounds) {
            const background = this._backgrounds[monitorIndex];

            if (monitorIndex < this._layoutManager.monitors.length) {
                background.updateResolution();
            } else {
                background.disconnect(background._changedId);
                background.destroy();
                delete this._backgrounds[monitorIndex];
            }
        }
    }

    getBackground(monitorIndex) {
        let file = null;
        let style;

        // We don't watch changes to settings here,
        // instead we rely on Background to watch those
        // and emit 'bg-changed' at the right time

        if (this._overrideImage != null) {
            file = Gio.File.new_for_path(this._overrideImage);
            style = GDesktopEnums.BackgroundStyle.ZOOM; // Hardcode
        } else {
            style = this._settings.get_enum(BACKGROUND_STYLE_KEY);
            if (style !== GDesktopEnums.BackgroundStyle.NONE) {
                const colorScheme = this._interfaceSettings.get_enum('color-scheme');
                const uri = this._settings.get_string(
                    colorScheme === GDesktopEnums.ColorScheme.PREFER_DARK
                        ? PICTURE_URI_DARK_KEY
                        : PICTURE_URI_KEY);

                file = Gio.File.new_for_commandline_arg(uri);
            }
        }

        // Animated backgrounds are (potentially) per-monitor, since
        // they can have variants that depend on the aspect ratio and
        // size of the monitor; for other backgrounds we can use the
        // same background object for all monitors.
        if (file == null || !file.get_basename().endsWith('.xml'))
            monitorIndex = 0;

        if (!(monitorIndex in this._backgrounds)) {
            const background = new Background({
                monitorIndex,
                layoutManager: this._layoutManager,
                settings: this._settings,
                file,
                style,
            });

            background._changedId = background.connect('bg-changed', () => {
                background.disconnect(background._changedId);
                background.destroy();
                delete this._backgrounds[monitorIndex];
            });

            this._backgrounds[monitorIndex] = background;
        }

        return this._backgrounds[monitorIndex];
    }

    destroy() {
        const monitorManager = global.backend.get_monitor_manager();
        monitorManager.disconnect(this._monitorsChangedId);

        for (const monitorIndex in this._backgrounds) {
            const background = this._backgrounds[monitorIndex];
            background.disconnect(background._changedId);
            background.destroy();
        }

        this._backgrounds = null;
    }
}

const Animation = GObject.registerClass(
class Animation extends GnomeBG.BGSlideShow {
    _init(params) {
        super._init(params);

        this.keyFrameFiles = [];
        this.transitionProgress = 0.0;
        this.transitionDuration = 0.0;
        this.loaded = false;
    }

    load_async(cancellable, callback) {
        super.load_async(cancellable, () => {
            this.loaded = true;

            callback?.();
        });
    }

    update(monitor) {
        this.keyFrameFiles = [];

        if (this.get_num_slides() < 1)
            return;

        const [progress, duration, isFixed_, filename1, filename2] =
            this.get_current_slide(monitor.width, monitor.height);

        this.transitionDuration = duration;
        this.transitionProgress = progress;

        if (filename1)
            this.keyFrameFiles.push(Gio.File.new_for_path(filename1));

        if (filename2)
            this.keyFrameFiles.push(Gio.File.new_for_path(filename2));
    }
});

export class BackgroundManager extends Signals.EventEmitter {
    constructor(params) {
        super();
        params = Params.parse(params, {
            container: null,
            layoutManager: Main.layoutManager,
            monitorIndex: null,
            vignette: false,
            controlPosition: true,
            settingsSchema: BACKGROUND_SCHEMA,
            useContentSize: true,
        });

        const cache = getBackgroundCache();
        this._settingsSchema = params.settingsSchema;
        this._backgroundSource = cache.getBackgroundSource(params.layoutManager, params.settingsSchema);

        this._container = params.container;
        this._layoutManager = params.layoutManager;
        this._vignette = params.vignette;
        this._monitorIndex = params.monitorIndex;
        this._controlPosition = params.controlPosition;
        this._useContentSize = params.useContentSize;

        this.backgroundActor = this._createBackgroundActor();
        this._newBackgroundActor = null;
    }

    destroy() {
        const cache = getBackgroundCache();
        cache.releaseBackgroundSource(this._settingsSchema);
        this._backgroundSource = null;

        if (this._newBackgroundActor) {
            this._newBackgroundActor.destroy();
            this._newBackgroundActor = null;
        }

        if (this.backgroundActor) {
            this.backgroundActor.destroy();
            this.backgroundActor = null;
        }
    }

    _swapBackgroundActor() {
        const oldBackgroundActor = this.backgroundActor;
        this.backgroundActor = this._newBackgroundActor;
        this._newBackgroundActor = null;
        this.emit('changed');

        if (Main.layoutManager.screenTransition.visible) {
            oldBackgroundActor.destroy();
            return;
        }

        oldBackgroundActor.ease({
            opacity: 0,
            duration: FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => oldBackgroundActor.destroy(),
        });
    }

    _updateBackgroundActor() {
        if (this._newBackgroundActor) {
            /* Skip displaying existing background queued for load */
            this._newBackgroundActor.destroy();
            this._newBackgroundActor = null;
        }

        const newBackgroundActor = this._createBackgroundActor();

        const oldContent = this.backgroundActor.content;
        const newContent = newBackgroundActor.content;

        newContent.vignette_sharpness = oldContent.vignette_sharpness;
        newContent.brightness = oldContent.brightness;

        newBackgroundActor.visible = this.backgroundActor.visible;

        this._newBackgroundActor = newBackgroundActor;

        const {background} = newBackgroundActor.content;

        if (background.isLoaded) {
            this._swapBackgroundActor();
        } else {
            newBackgroundActor.loadedSignalId = background.connect('loaded',
                () => {
                    background.disconnect(newBackgroundActor.loadedSignalId);
                    newBackgroundActor.loadedSignalId = 0;

                    this._swapBackgroundActor();
                });
        }
    }

    _createBackgroundActor() {
        const background = this._backgroundSource.getBackground(this._monitorIndex);
        const backgroundActor = new Meta.BackgroundActor({
            meta_display: global.display,
            monitor: this._monitorIndex,
            request_mode: this._useContentSize
                ? Clutter.RequestMode.CONTENT_SIZE
                : Clutter.RequestMode.HEIGHT_FOR_WIDTH,
            x_expand: !this._useContentSize,
            y_expand: !this._useContentSize,
        });
        backgroundActor.content.set({
            background,
            vignette: this._vignette,
            vignette_sharpness: 0.5,
            brightness: 0.5,
        });

        this._container.add_child(backgroundActor);

        if (this._controlPosition) {
            const monitor = this._layoutManager.monitors[this._monitorIndex];
            backgroundActor.set_position(monitor.x, monitor.y);
            this._container.set_child_below_sibling(backgroundActor, null);
        }

        let changeSignalId = background.connect('bg-changed', () => {
            background.disconnect(changeSignalId);
            changeSignalId = null;
            this._updateBackgroundActor();
        });

        let loadedSignalId;
        if (background.isLoaded) {
            GLib.idle_add_once(GLib.PRIORITY_DEFAULT, () => {
                this.emit('loaded');
            });
        } else {
            loadedSignalId = background.connect('loaded', () => {
                background.disconnect(loadedSignalId);
                loadedSignalId = null;
                this.emit('loaded');
            });
        }

        backgroundActor.connect('destroy', () => {
            if (changeSignalId)
                background.disconnect(changeSignalId);

            if (loadedSignalId)
                background.disconnect(loadedSignalId);

            if (backgroundActor.loadedSignalId)
                background.disconnect(backgroundActor.loadedSignalId);
        });

        return backgroundActor;
    }
}
