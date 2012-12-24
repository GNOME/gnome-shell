// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GDesktopEnums = imports.gi.GDesktopEnums;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;

const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const BACKGROUND_SCHEMA = 'org.gnome.desktop.background';
const DRAW_BACKGROUND_KEY = 'draw-background';
const PRIMARY_COLOR_KEY = 'primary-color';
const SECONDARY_COLOR_KEY = 'secondary-color';
const COLOR_SHADING_TYPE_KEY = 'color-shading-type';
const BACKGROUND_STYLE_KEY = 'picture-options';
const PICTURE_OPACITY_KEY = 'picture-opacity';
const PICTURE_URI_KEY = 'picture-uri';

const FADE_ANIMATION_TIME = 1.0;

let _backgroundCache = null;

const BackgroundCache = new Lang.Class({
    Name: 'BackgroundCache',

    _init: function() {
       this._patterns = [];
       this._images = [];
       this._fileMonitors = {};
    },

    getPatternContent: function(params) {
        params = Params.parse(params, { monitorIndex: 0,
                                        color: null,
                                        secondColor: null,
                                        shadingType: null,
                                        effects: Meta.BackgroundEffects.NONE });

        let content = null;
        let candidateContent = null;
        for (let i = 0; i < this._patterns.length; i++) {
            if (!this._patterns[i])
                continue;

            if (this._patterns[i].get_shading() != params.shadingType)
                continue;

            if (!params.color.equal(this._patterns[i].get_color()))
                continue;

            if (params.shadingType != GDesktopEnums.BackgroundShading.SOLID &&
                !params.secondColor.equal(this._patterns[i].get_second_color()))
                continue;

            candidateContent = this._patterns[i];

            if (params.effects != this._patterns[i].effects)
                continue;

            break;
        }

        if (candidateContent) {
            content = candidateContent.copy(params.monitorIndex, params.effects);
        } else {
            content = new Meta.Background({ meta_screen: global.screen,
                                            monitor: params.monitorIndex,
                                            effects: params.effects });

            if (params.shadingType == GDesktopEnums.BackgroundShading.SOLID) {
                content.load_color(params.color);
            } else {
                content.load_gradient(params.shadingType, params.color, params.secondColor);
            }

            this._patterns.push(content);
        }

        return content;
    },

    _monitorFile: function(filename) {
        if (this._fileMonitors[filename])
            return;

        let file = Gio.File.new_for_path(filename);
        let monitor = file.monitor(Gio.FileMonitorFlags.NONE, null);

        let signalId = monitor.connect('changed',
                                       Lang.bind(this, function() {
                                           for (let i = 0; i < this._images.length; i++) {
                                               if (this._images[i].get_filename() == filename)
                                                   this._images.splice(i, 1);
                                           }

                                           monitor.disconnect(signalId);

                                           this.emit('file-changed', filename);
                                       }));

        this._fileMonitors[filename] = monitor;
    },

    _removeContent: function(contentList, content) {
        let index = contentList.indexOf(content);

        if (index >= 0)
            contentList.splice(index, 1);
    },

    removePatternContent: function(content) {
        this._removeContent(this._patterns, content);
    },

    removeImageContent: function(content) {
        this._removeContent(this._images, content);
    },

    getImageContent: function(params) {
        params = Params.parse(params, { monitorIndex: 0,
                                        style: null,
                                        filename: null,
                                        effects: Meta.BackgroundEffects.NONE,
                                        cancellable: null,
                                        onFinished: null });

        let content = null;
        let candidateContent = null;
        for (let i = 0; i < this._images.length; i++) {
            if (!this._images[i])
                continue;

            if (this._images[i].get_style() != params.style)
                continue;

            if (this._images[i].get_filename() != params.filename)
                continue;

            if (params.style == GDesktopEnums.BackgroundStyle.SPANNED &&
                this._images[i].monitor_index != this._monitorIndex)
                continue;

            candidateContent = this._images[i];

            if (params.effects != this._images[i].effects)
                continue;

            break;
        }

        if (candidateContent) {
            content = candidateContent.copy(params.monitorIndex, params.effects);

            if (params.onFinished)
                params.onFinished(content);
        } else {
            content = new Meta.Background({ meta_screen: global.screen,
                                            monitor: params.monitorIndex,
                                            effects: params.effects });

            content.load_file_async(params.filename,
                                    params.style,
                                    params.cancellable,
                                    Lang.bind(this,
                                              function(object, result) {
                                                  try {
                                                      content.load_file_finish(result);

                                                      this._monitorFile(params.filename);
                                                      this._images.push(content);
                                                  } catch(e) {
                                                       content = null;
                                                  }

                                                  if (params.onFinished)
                                                      params.onFinished(content);
                                              }));
        }
    }
});
Signals.addSignalMethods(BackgroundCache.prototype);

function getBackgroundCache() {
    if (!_backgroundCache)
        _backgroundCache = new BackgroundCache();
    return _backgroundCache;
}

const Background = new Lang.Class({
    Name: 'Background',

    _init: function(params) {
        params = Params.parse(params, { monitorIndex: 0,
                                        effects: Meta.BackgroundEffects.NONE });
        this.actor = new Meta.BackgroundGroup();
        this.actor._delegate = this;

        this._destroySignalId = this.actor.connect('destroy',
                                                   Lang.bind(this, this._destroy));

        this._settings = new Gio.Settings({ schema: BACKGROUND_SCHEMA });
        this._monitorIndex = params.monitorIndex;
        this._effects = params.effects;
        this._fileWatches = {};
        this._pattern = null;
        this._image = null;

        this._brightness = 1.0;
        this._vignetteSharpness = 0.2;
        this._saturation = 1.0;
        this._cancellable = new Gio.Cancellable();
        this.isLoaded = false;

        this._settings.connect('changed', Lang.bind(this, function() {
                                   this.emit('changed');
                               }));

        this._load();
    },

    _destroy: function() {
        this._cancellable.cancel();
        this._cancellable = null;

        let i;

        let keys = Object.keys(this._fileWatches);
        for (i = 0; i < keys.length; i++) {
            this._cache.disconnect(this._fileWatches[keys[i]]);
        }
        this._fileWatches = null;

        if (this._pattern) {
            if (this._pattern.content)
                this._cache.removePatternContent(this._pattern.content);

            this._pattern.destroy();
            this._pattern = null;
        }

        if (this._image) {
            if (this._image.content)
                this._cache.removeImageContent(this._image.content);

            this._image.destroy();
            this._image = null;
        }

        this.actor.disconnect(this._destroySignalId);
        this._destroySignalId = 0;
        this.actor.destroy();
    },

    _setLoaded: function() {
        if (this.isLoaded)
            return;

        this.isLoaded = true;

        GLib.idle_add(GLib.PRIORITY_DEFAULT, Lang.bind(this, function() {
            this.emit('loaded');
            return false;
        }));
    },

    _loadPattern: function() {
        let colorString, res, color, secondColor;

        colorString = this._settings.get_string(PRIMARY_COLOR_KEY);
        [res, color] = Clutter.Color.from_string(colorString);
        colorString = this._settings.get_string(SECONDARY_COLOR_KEY);
        [res, secondColor] = Clutter.Color.from_string(colorString);

        let shadingType = this._settings.get_enum(COLOR_SHADING_TYPE_KEY);

        let content = this._cache.getPatternContent({ monitorIndex: this._monitorIndex,
                                                      effects: this._effects,
                                                      color: color,
                                                      secondColor: secondColor,
                                                      shadingType: shadingType });

        this._pattern = new Meta.BackgroundActor();
        this.actor.add_child(this._pattern);

        this._pattern.content = content;
    },

    _watchCacheFile: function(filename) {
        if (this._fileWatches[filename])
            return;

        let signalId = this._cache.connect('file-changed',
                                           Lang.bind(this, function(cache, changedFile) {
                                               if (changedFile == filename) {
                                                   this.emit('changed');
                                               }
                                           }));
        this._fileWatches[filename] = signalId;
    },

    _setImage: function(content, filename) {
        content.saturation = this._saturation;
        content.brightness = this._brightness;
        content.vignette_sharpness = this._vignetteSharpness;

        this._image = new Meta.BackgroundActor();
        this._image.content = content;
        this.actor.add_child(this._image);
        this._watchCacheFile(filename);
    },

    _loadFile: function(filename) {
        this._cache.getImageContent({ monitorIndex: this._monitorIndex,
                                      effects: this._effects,
                                      style: this._style,
                                      filename: filename,
                                      cancellable: this._cancellable,
                                      onFinished: Lang.bind(this, function(content) {
                                          if (!content) {
                                              return;
                                          }

                                          this._setImage(content, filename);
                                          this._setLoaded();
                                      })
                                    });

    },

    _load: function () {
        if (!this._settings.get_boolean(DRAW_BACKGROUND_KEY)) {
            this._setLoaded();
            return;
        }

        this._cache = getBackgroundCache();

        this._loadPattern(this._cache);

        this._style = this._settings.get_enum(BACKGROUND_STYLE_KEY);
        if (this._style == GDesktopEnums.BackgroundStyle.NONE) {
            this._setLoaded();
            return;
        }

        let uri = this._settings.get_string(PICTURE_URI_KEY);
        let filename = Gio.File.new_for_uri(uri).get_path();

        this._loadFile(filename);
    },

    get saturation() {
        return this._saturation;
    },

    set saturation(saturation) {
        this._saturation = saturation;

        if (this._pattern && this._pattern.content)
            this._pattern.content.saturation = saturation;

        if (this._image && this._image.content)
            this._image.content.saturation = saturation;
    },

    get brightness() {
        return this._brightness;
    },

    set brightness(factor) {
        this._brightness = factor;
        if (this._pattern && this._pattern.content)
            this._pattern.content.brightness = factor;

        if (this._image && this._image.content)
            this._image.content.brightness = factor;
    },

    get vignetteSharpness() {
        return this._vignetteSharpness;
    },

    set vignetteSharpness(sharpness) {
        this._vignetteSharpness = sharpness;
        if (this._pattern && this._pattern.content)
            this._pattern.content.vignette_sharpness = sharpness;

        if (this._image && this._image.content)
            this._image.content.vignette_sharpness = sharpness;
    }
});
Signals.addSignalMethods(Background.prototype);

const StillFrame = new Lang.Class({
    Name: 'StillFrame',

    _init: function(monitorIndex) {
        this.actor = new Meta.BackgroundActor();
        this.actor._delegate = this;

        let content = new Meta.Background({ meta_screen: global.screen,
                                            monitor: monitorIndex,
                                            effects: Meta.BackgroundEffects.NONE });
        content.load_still_frame();

        this.actor.content = content;
    }
});
Signals.addSignalMethods(StillFrame.prototype);

const BackgroundManager = new Lang.Class({
    Name: 'BackgroundManager',

    _init: function(params) {
        params = Params.parse(params, { container: null,
                                        layoutManager: Main.layoutManager,
                                        monitorIndex: null,
                                        effects: Meta.BackgroundEffects.NONE });

        this._container = params.container;
        this._layoutManager = params.layoutManager;
        this._effects = params.effects;
        this._monitorIndex = params.monitorIndex;

        this.background = this._createBackground();
        this._newBackground = null;
        this._loadedSignalId = 0;
        this._changedSignalId = 0;
    },

    destroy: function() {
        if (this._loadedSignalId)
            this._newBackground.disconnect(this._loadedSignalId);

        if (this._changedSignalId)
            this.background.disconnect(this._changedSignalId);

        if (this._newBackground) {
            let container = this._newBackground.actor.get_parent();
            if (container)
                container.remove_actor(this._newBackground.actor);
            this._newBackground = null;
        }

        if (this.background) {
            let container = this.background.actor.get_parent();
            if (container)
                container.remove_actor(this.background.actor);
            this.background = null;
        }
    },

    _updateBackground: function(background, monitorIndex) {
        let newBackground = this._createBackground(monitorIndex);
        newBackground.vignetteSharpness = background.vignetteSharpness;
        newBackground.brightness = background.brightness;
        newBackground.saturation = background.saturation;
        newBackground.visible = background.visible;

        let signalId = newBackground.connect('loaded',
            Lang.bind(this, function() {
                newBackground.disconnect(signalId);
                Tweener.addTween(background.actor,
                                 { opacity: 0,
                                   time: FADE_ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete: Lang.bind(this, function() {
                                       this.background = newBackground;
                                       this._newBackground = null;
                                       this._container.remove_actor(background.actor);
                                       this.emit('changed');
                                   })
                                 });
        }));
        this._loadedSignalId = signalId;

        this._newBackground = newBackground;
    },

    _createBackground: function() {
        let background = new Background({ monitorIndex: this._monitorIndex,
                                          layoutManager: this._layoutManager,
                                          effects: this._effects });
        this._container.add_child(background.actor);

        let monitor = this._layoutManager.monitors[this._monitorIndex];
        background.actor.set_position(monitor.x, monitor.y);
        background.actor.set_size(monitor.width, monitor.height);
        background.actor.lower_bottom();

        let signalId = background.connect('changed', Lang.bind(this, function() {
            background.disconnect(signalId);
            this._updateBackground(background, this._monitorIndex);
        }));

        this._changedSignalId = signalId;

        return background;
    },
});
Signals.addSignalMethods(BackgroundManager.prototype);
