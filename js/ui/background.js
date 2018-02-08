// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

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
// MetaBackgroundImage
//   An object represented an image file that will be used for drawing
//   the background. MetaBackgroundImage objects asynchronously load,
//   so they are first created in an unloaded state, then later emit
//   a ::loaded signal when the Cogl object becomes available.
//
// MetaBackgroundImageCache
//   A cache from filename to MetaBackgroundImage.
//
// BackgroundSource
//   An object that is created for each GSettings schema (separate
//   settings schemas are used for the lock screen and main background),
//   and holds a reference to shared Background objects.
//
// MetaBackground
//   Holds the specification of a background - a background color
//   or gradient and one or two images blended together.
//
// Background
//   JS delegate object that Connects a MetaBackground to the GSettings
//   schema for the background.
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
//                MetaBackgroundImage            looked up in MetaBackgroundImageCache
//
// The animated case is tricker because the animation XML file can specify different
// files for different monitor resolutions and aspect ratios. For this reason,
// the BackgroundSource provides different Background share a single Animation object,
// which tracks the animation, but use different MetaBackground objects. In the
// common case, the different MetaBackground objects will be created for the
// same filename and look up the *same* MetaBackgroundImage object, so there is
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
//                MetaBackgroundImage            looked up in MetaBackgroundImageCache
//                MetaBackgroundImage
//
// But the case of different filenames and different background images
// is possible as well:
//                        ....
//      MetaBackground              MetaBackground
//             |                          |
//     MetaBackgroundImage         MetaBackgroundImage
//     MetaBackgroundImage         MetaBackgroundImage

const Clutter = imports.gi.Clutter;
const GDesktopEnums = imports.gi.GDesktopEnums;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;

const LoginManager = imports.misc.loginManager;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

var DEFAULT_BACKGROUND_COLOR = Clutter.Color.from_pixel(0x2e3436ff);

const BACKGROUND_SCHEMA = 'org.gnome.desktop.background';
const PRIMARY_COLOR_KEY = 'primary-color';
const SECONDARY_COLOR_KEY = 'secondary-color';
const COLOR_SHADING_TYPE_KEY = 'color-shading-type';
const BACKGROUND_STYLE_KEY = 'picture-options';
const PICTURE_OPACITY_KEY = 'picture-opacity';
const PICTURE_URI_KEY = 'picture-uri';

var FADE_ANIMATION_TIME = 1.0;

// These parameters affect how often we redraw.
// The first is how different (percent crossfaded) the slide show
// has to look before redrawing and the second is the minimum
// frequency (in seconds) we're willing to wake up
var ANIMATION_OPACITY_STEP_INCREMENT = 4.0;
var ANIMATION_MIN_WAKEUP_INTERVAL = 1.0;

let _backgroundCache = null;

function _fileEqual0(file1, file2) {
    if (file1 == file2)
        return true;

    if (!file1 || !file2)
        return false;

    return file1.equal(file2);
}

var BackgroundCache = new Lang.Class({
    Name: 'BackgroundCache',

    _init() {
        this._fileMonitors = {};
        this._backgroundSources = {};
        this._animations = {};
    },

    monitorFile(file) {
        let key = file.hash();
        if (this._fileMonitors[key])
            return;

        let monitor = file.monitor(Gio.FileMonitorFlags.NONE, null);
        monitor.connect('changed',
                        (obj, file, otherFile, eventType) => {
                            // Ignore CHANGED and CREATED events, since in both cases
                            // we'll get a CHANGES_DONE_HINT event when done.
                            if (eventType != Gio.FileMonitorEvent.CHANGED &&
                                eventType != Gio.FileMonitorEvent.CREATED)
                                this.emit('file-changed', file);
                        });

        this._fileMonitors[key] = monitor;
    },

    getAnimation(params) {
        params = Params.parse(params, { file: null,
                                        settingsSchema: null,
                                        onLoaded: null });

        let animation = this._animations[params.settingsSchema];
        if (animation && _fileEqual0(animation.file, params.file)) {
            if (params.onLoaded) {
                let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                    params.onLoaded(this._animations[params.settingsSchema]);
                    return GLib.SOURCE_REMOVE;
                });
                GLib.Source.set_name_by_id(id, '[gnome-shell] params.onLoaded');
            }
            return;
        }

        animation = new Animation({ file: params.file });

        animation.load(() => {
            this._animations[params.settingsSchema] = animation;

            if (params.onLoaded) {
                let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                    params.onLoaded(this._animations[params.settingsSchema]);
                    return GLib.SOURCE_REMOVE;
                });
                GLib.Source.set_name_by_id(id, '[gnome-shell] params.onLoaded');
            }
        });
    },

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
    },

    releaseBackgroundSource(settingsSchema) {
        if (settingsSchema in this._backgroundSources) {
            let source = this._backgroundSources[settingsSchema];
            source._useCount--;
            if (source._useCount == 0) {
                delete this._backgroundSources[settingsSchema];
                source.destroy();
            }
        }
    }
});
Signals.addSignalMethods(BackgroundCache.prototype);

function getBackgroundCache() {
    if (!_backgroundCache)
        _backgroundCache = new BackgroundCache();
    return _backgroundCache;
}

var Background = new Lang.Class({
    Name: 'Background',

    _init(params) {
        params = Params.parse(params, { monitorIndex: 0,
                                        layoutManager: Main.layoutManager,
                                        settings: null,
                                        file: null,
                                        style: null });

        this.background = new Meta.Background({ meta_screen: global.screen });
        this.background._delegate = this;

        this._settings = params.settings;
        this._file = params.file;
        this._style = params.style;
        this._monitorIndex = params.monitorIndex;
        this._layoutManager = params.layoutManager;
        this._fileWatches = {};
        this._cancellable = new Gio.Cancellable();
        this.isLoaded = false;

        this._clock = new GnomeDesktop.WallClock();
        this._timezoneChangedId = this._clock.connect('notify::timezone',
            () => {
                if (this._animation)
                    this._loadAnimation(this._animation.file);
            });

        let loginManager = LoginManager.getLoginManager();
        this._prepareForSleepId = loginManager.connect('prepare-for-sleep',
            (lm, aboutToSuspend) => {
                if (aboutToSuspend)
                    return;
                this._refreshAnimation();
            });

        this._settingsChangedSignalId = this._settings.connect('changed', () => {
            this.emit('changed');
        });

        this._load();
    },

    destroy() {
        this._cancellable.cancel();
        this._removeAnimationTimeout();

        let i;
        let keys = Object.keys(this._fileWatches);
        for (i = 0; i < keys.length; i++) {
            this._cache.disconnect(this._fileWatches[keys[i]]);
        }
        this._fileWatches = null;

        if (this._timezoneChangedId != 0)
            this._clock.disconnect(this._timezoneChangedId);
        this._timezoneChangedId = 0;

        this._clock = null;

        if (this._prepareForSleepId != 0)
            LoginManager.getLoginManager().disconnect(this._prepareForSleepId);
        this._prepareForSleepId = 0;

        if (this._settingsChangedSignalId != 0)
            this._settings.disconnect(this._settingsChangedSignalId);
        this._settingsChangedSignalId = 0;
    },

    updateResolution() {
        if (this._animation)
            this._refreshAnimation();
    },

    _refreshAnimation() {
        if (!this._animation)
            return;

        this._removeAnimationTimeout();
        this._updateAnimation();
    },

    _setLoaded() {
        if (this.isLoaded)
            return;

        this.isLoaded = true;

        let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this.emit('loaded');
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(id, '[gnome-shell] this.emit');
    },

    _loadPattern() {
        let colorString, res, color, secondColor;

        colorString = this._settings.get_string(PRIMARY_COLOR_KEY);
        [res, color] = Clutter.Color.from_string(colorString);
        colorString = this._settings.get_string(SECONDARY_COLOR_KEY);
        [res, secondColor] = Clutter.Color.from_string(colorString);

        let shadingType = this._settings.get_enum(COLOR_SHADING_TYPE_KEY);

        if (shadingType == GDesktopEnums.BackgroundShading.SOLID)
            this.background.set_color(color);
        else
            this.background.set_gradient(shadingType, color, secondColor);
    },

    _watchFile(file) {
        let key = file.hash();
        if (this._fileWatches[key])
            return;

        this._cache.monitorFile(file);
        let signalId = this._cache.connect('file-changed',
                                           (cache, changedFile) => {
                                               if (changedFile.equal(file)) {
                                                   let imageCache = Meta.BackgroundImageCache.get_default();
                                                   imageCache.purge(changedFile);
                                                   this.emit('changed');
                                               }
                                           });
        this._fileWatches[key] = signalId;
    },

    _removeAnimationTimeout() {
        if (this._updateAnimationTimeoutId) {
            GLib.source_remove(this._updateAnimationTimeoutId);
            this._updateAnimationTimeoutId = 0;
        }
    },

    _updateAnimation() {
        this._updateAnimationTimeoutId = 0;

        this._animation.update(this._layoutManager.monitors[this._monitorIndex]);
        let files = this._animation.keyFrameFiles;

        let finish = () => {
            this._setLoaded();
            if (files.length > 1) {
                this.background.set_blend(files[0], files[1],
                                          this._animation.transitionProgress,
                                          this._style);
            } else if (files.length > 0) {
                this.background.set_file(files[0], this._style);
            } else {
                this.background.set_file(null, this._style);
            }
            this._queueUpdateAnimation();
        };

        let cache = Meta.BackgroundImageCache.get_default();
        let numPendingImages = files.length;
        for (let i = 0; i < files.length; i++) {
            this._watchFile(files[i]);
            let image = cache.load(files[i]);
            if (image.is_loaded()) {
                numPendingImages--;
                if (numPendingImages == 0)
                    finish();
            } else {
                let id = image.connect('loaded', () => {
                    image.disconnect(id);
                    numPendingImages--;
                    if (numPendingImages == 0)
                        finish();
                });
            }
        }
    },

    _queueUpdateAnimation() {
        if (this._updateAnimationTimeoutId != 0)
            return;

        if (!this._cancellable || this._cancellable.is_cancelled())
            return;

        if (!this._animation.transitionDuration)
            return;

        let nSteps = 255 / ANIMATION_OPACITY_STEP_INCREMENT;
        let timePerStep = (this._animation.transitionDuration * 1000) / nSteps;

        let interval = Math.max(ANIMATION_MIN_WAKEUP_INTERVAL * 1000,
                                timePerStep);

        if (interval > GLib.MAXUINT32)
            return;

        this._updateAnimationTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                      interval,
                                                      () => {
                                                          this._updateAnimationTimeoutId = 0;
                                                          this._updateAnimation();
                                                          return GLib.SOURCE_REMOVE;
                                                      });
        GLib.Source.set_name_by_id(this._updateAnimationTimeoutId, '[gnome-shell] this._updateAnimation');
    },

    _loadAnimation(file) {
        this._cache.getAnimation({ file: file,
                                   settingsSchema: this._settings.schema_id,
                                   onLoaded: animation => {
                                       this._animation = animation;

                                       if (!this._animation || this._cancellable.is_cancelled()) {
                                           this._setLoaded();
                                           return;
                                       }

                                       this._updateAnimation();
                                       this._watchFile(file);
                                   }
                                 });
    },

    _loadImage(file) {
        this.background.set_file(file, this._style);
        this._watchFile(file);

        let cache = Meta.BackgroundImageCache.get_default();
        let image = cache.load(file);
        if (image.is_loaded())
            this._setLoaded();
        else {
            let id = image.connect('loaded', () => {
                this._setLoaded();
                image.disconnect(id);
            });
        }
    },

    _loadFile(file) {
        if (file.get_basename().endsWith('.xml'))
            this._loadAnimation(file);
        else
            this._loadImage(file);
    },

    _load () {
        this._cache = getBackgroundCache();

        this._loadPattern();

        if (!this._file) {
            this._setLoaded();
            return;
        }

        this._loadFile(this._file);
    },
});
Signals.addSignalMethods(Background.prototype);

let _systemBackground;

var SystemBackground = new Lang.Class({
    Name: 'SystemBackground',

    _init() {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/noise-texture.png');

        if (_systemBackground == null) {
            _systemBackground = new Meta.Background({ meta_screen: global.screen });
            _systemBackground.set_color(DEFAULT_BACKGROUND_COLOR);
            _systemBackground.set_file(file, GDesktopEnums.BackgroundStyle.WALLPAPER);
        }

        this.actor = new Meta.BackgroundActor({ meta_screen: global.screen,
                                                monitor: 0,
                                                background: _systemBackground });

        let cache = Meta.BackgroundImageCache.get_default();
        let image = cache.load(file);
        if (image.is_loaded()) {
            image = null;
            let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this.emit('loaded');
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] SystemBackground.loaded');
        } else {
            let id = image.connect('loaded', () => {
                this.emit('loaded');
                image.disconnect(id);
                image = null;
            });
        }
    },
});
Signals.addSignalMethods(SystemBackground.prototype);

var BackgroundSource = new Lang.Class({
    Name: 'BackgroundSource',

    _init(layoutManager, settingsSchema) {
        // Allow override the background image setting for performance testing
        this._layoutManager = layoutManager;
        this._overrideImage = GLib.getenv('SHELL_BACKGROUND_IMAGE');
        this._settings = new Gio.Settings({ schema_id: settingsSchema });
        this._backgrounds = [];

        this._monitorsChangedId = global.screen.connect('monitors-changed',
                                                        Lang.bind(this, this._onMonitorsChanged));
    },

    _onMonitorsChanged() {
        for (let monitorIndex in this._backgrounds) {
            let background = this._backgrounds[monitorIndex];

            if (monitorIndex < this._layoutManager.monitors.length) {
                background.updateResolution();
            } else {
                background.disconnect(background._changedId);
                background.destroy();
                delete this._backgrounds[monitorIndex];
            }
        }
    },

    getBackground(monitorIndex) {
        let file = null;
        let style;

        // We don't watch changes to settings here,
        // instead we rely on Background to watch those
        // and emit 'changed' at the right time

        if (this._overrideImage != null) {
            file = Gio.File.new_for_path(this._overrideImage);
            style = GDesktopEnums.BackgroundStyle.ZOOM; // Hardcode
        } else {
            style = this._settings.get_enum(BACKGROUND_STYLE_KEY);
            if (style != GDesktopEnums.BackgroundStyle.NONE) {
                let uri = this._settings.get_string(PICTURE_URI_KEY);
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
            let background = new Background({
                monitorIndex: monitorIndex,
                layoutManager: this._layoutManager,
                settings: this._settings,
                file: file,
                style: style
            });

            background._changedId = background.connect('changed', () => {
                background.disconnect(background._changedId);
                background.destroy();
                delete this._backgrounds[monitorIndex];
            });

            this._backgrounds[monitorIndex] = background;
        }

        return this._backgrounds[monitorIndex];
    },

    destroy() {
        global.screen.disconnect(this._monitorsChangedId);

        for (let monitorIndex in this._backgrounds) {
            let background = this._backgrounds[monitorIndex];
            background.disconnect(background._changedId);
            background.destroy();
        }

        this._backgrounds = null;
    }
});

var Animation = new Lang.Class({
    Name: 'Animation',

    _init(params) {
        params = Params.parse(params, { file: null });

        this.file = params.file;
        this.keyFrameFiles = [];
        this.transitionProgress = 0.0;
        this.transitionDuration = 0.0;
        this.loaded = false;
    },

    load(callback) {
        this._show = new GnomeDesktop.BGSlideShow({ filename: this.file.get_path() });

        this._show.load_async(null, (object, result) => {
            this.loaded = true;
            if (callback)
                callback();
        });
    },

    update(monitor) {
        this.keyFrameFiles = [];

        if (!this._show)
            return;

        if (this._show.get_num_slides() < 1)
            return;

        let [progress, duration, isFixed, filename1, filename2] = this._show.get_current_slide(monitor.width, monitor.height);

        this.transitionDuration = duration;
        this.transitionProgress = progress;

        if (filename1)
            this.keyFrameFiles.push(Gio.File.new_for_path(filename1));

        if (filename2)
            this.keyFrameFiles.push(Gio.File.new_for_path(filename2));
    },
});
Signals.addSignalMethods(Animation.prototype);

var BackgroundManager = new Lang.Class({
    Name: 'BackgroundManager',

    _init(params) {
        params = Params.parse(params, { container: null,
                                        layoutManager: Main.layoutManager,
                                        monitorIndex: null,
                                        vignette: false,
                                        controlPosition: true,
                                        settingsSchema: BACKGROUND_SCHEMA });

        let cache = getBackgroundCache();
        this._settingsSchema = params.settingsSchema;
        this._backgroundSource = cache.getBackgroundSource(params.layoutManager, params.settingsSchema);

        this._container = params.container;
        this._layoutManager = params.layoutManager;
        this._vignette = params.vignette;
        this._monitorIndex = params.monitorIndex;
        this._controlPosition = params.controlPosition;

        this.backgroundActor = this._createBackgroundActor();
        this._newBackgroundActor = null;
    },

    destroy() {
        let cache = getBackgroundCache();
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
    },

    _swapBackgroundActor() {
        let oldBackgroundActor = this.backgroundActor;
        this.backgroundActor = this._newBackgroundActor;
        this._newBackgroundActor = null;
        this.emit('changed');

        Tweener.addTween(oldBackgroundActor,
                         { opacity: 0,
                           time: FADE_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete() {
                               oldBackgroundActor.background.run_dispose();
                               oldBackgroundActor.destroy();
                           }
                         });
    },

    _updateBackgroundActor() {
        if (this._newBackgroundActor) {
            /* Skip displaying existing background queued for load */
            this._newBackgroundActor.destroy();
            this._newBackgroundActor = null;
        }

        let newBackgroundActor = this._createBackgroundActor();
        newBackgroundActor.vignette_sharpness = this.backgroundActor.vignette_sharpness;
        newBackgroundActor.brightness = this.backgroundActor.brightness;
        newBackgroundActor.visible = this.backgroundActor.visible;

        this._newBackgroundActor = newBackgroundActor;

        let background = newBackgroundActor.background._delegate;

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
    },

    _createBackgroundActor() {
        let background = this._backgroundSource.getBackground(this._monitorIndex);
        let backgroundActor = new Meta.BackgroundActor({ meta_screen: global.screen,
                                                         monitor: this._monitorIndex,
                                                         background: background.background,
                                                         vignette: this._vignette,
                                                         vignette_sharpness: 0.5,
                                                         brightness: 0.5,
                                                       });

        this._container.add_child(backgroundActor);

        let monitor = this._layoutManager.monitors[this._monitorIndex];

        backgroundActor.set_size(monitor.width, monitor.height);
        if (this._controlPosition) {
            backgroundActor.set_position(monitor.x, monitor.y);
            backgroundActor.lower_bottom();
        }

        let changeSignalId = background.connect('changed', () => {
            background.disconnect(changeSignalId);
            changeSignalId = null;
            this._updateBackgroundActor();
        });

        backgroundActor.connect('destroy', () => {
            if (changeSignalId)
                background.disconnect(changeSignalId);

            if (backgroundActor.loadedSignalId)
                background.disconnect(backgroundActor.loadedSignalId);
        });

        return backgroundActor;
    },
});
Signals.addSignalMethods(BackgroundManager.prototype);
