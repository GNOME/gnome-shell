// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenshotService */

const { Clutter, Gio, GObject, GLib, Meta, Shell, St } = imports.gi;

const GrabHelper = imports.ui.grabHelper;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

Gio._promisify(Shell.Screenshot.prototype, 'pick_color', 'pick_color_finish');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot', 'screenshot_finish');
Gio._promisify(Shell.Screenshot.prototype,
    'screenshot_window', 'screenshot_window_finish');
Gio._promisify(Shell.Screenshot.prototype,
    'screenshot_area', 'screenshot_area_finish');

const { loadInterfaceXML } = imports.misc.fileUtils;

const ScreenshotIface = loadInterfaceXML('org.gnome.Shell.Screenshot');

var ScreenshotService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(ScreenshotIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Screenshot');

        this._screenShooter = new Map();

        this._lockdownSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });

        Gio.DBus.session.own_name('org.gnome.Shell.Screenshot', Gio.BusNameOwnerFlags.REPLACE, null, null);
    }

    _createScreenshot(invocation, needsDisk = true) {
        let lockedDown = false;
        if (needsDisk)
            lockedDown = this._lockdownSettings.get_boolean('disable-save-to-disk');

        let sender = invocation.get_sender();
        if (this._screenShooter.has(sender)) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.BUSY,
                'There is an ongoing operation for this sender');
            return null;
        } else if (lockedDown) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.PERMISSION_DENIED,
                'Saving to disk is disabled');
            return null;
        }

        let shooter = new Shell.Screenshot();
        shooter._watchNameId =
                        Gio.bus_watch_name(Gio.BusType.SESSION, sender, 0, null,
                                           this._onNameVanished.bind(this));

        this._screenShooter.set(sender, shooter);

        return shooter;
    }

    _onNameVanished(connection, name) {
        this._removeShooterForSender(name);
    }

    _removeShooterForSender(sender) {
        let shooter = this._screenShooter.get(sender);
        if (!shooter)
            return;

        Gio.bus_unwatch_name(shooter._watchNameId);
        this._screenShooter.delete(sender);
    }

    _checkArea(x, y, width, height) {
        return x >= 0 && y >= 0 &&
               width > 0 && height > 0 &&
               x + width <= global.screen_width &&
               y + height <= global.screen_height;
    }

    *_resolveRelativeFilename(filename) {
        filename = filename.replace(/\.png$/, '');

        let path = [
            GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_PICTURES),
            GLib.get_home_dir(),
        ].find(p => p && GLib.file_test(p, GLib.FileTest.EXISTS));

        if (!path)
            return null;

        yield Gio.File.new_for_path(
            GLib.build_filenamev([path, '%s.png'.format(filename)]));

        for (let idx = 1; ; idx++) {
            yield Gio.File.new_for_path(
                GLib.build_filenamev([path, '%s-%s.png'.format(filename, idx)]));
        }
    }

    _createStream(filename, invocation) {
        if (filename == '')
            return [Gio.MemoryOutputStream.new_resizable(), null];

        if (GLib.path_is_absolute(filename)) {
            try {
                let file = Gio.File.new_for_path(filename);
                let stream = file.replace(null, false, Gio.FileCreateFlags.NONE, null);
                return [stream, file];
            } catch (e) {
                invocation.return_gerror(e);
                this._removeShooterForSender(invocation.get_sender());
                return [null, null];
            }
        }

        let err;
        for (let file of this._resolveRelativeFilename(filename)) {
            try {
                let stream = file.create(Gio.FileCreateFlags.NONE, null);
                return [stream, file];
            } catch (e) {
                err = e;
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                    break;
            }
        }

        invocation.return_gerror(err);
        this._removeShooterForSender(invocation.get_sender());
        return [null, null];
    }

    _flashAsync(shooter) {
        return new Promise((resolve, _reject) => {
            shooter.connect('screenshot_taken', (s, area) => {
                const flashspot = new Flashspot(area);
                flashspot.fire(resolve);

                global.display.get_sound_player().play_from_theme(
                    'screen-capture', _('Screenshot taken'), null);
            });
        });
    }

    _onScreenshotComplete(stream, file, invocation) {
        stream.close(null);

        let filenameUsed = '';
        if (file) {
            filenameUsed = file.get_path();
        } else {
            let bytes = stream.steal_as_bytes();
            let clipboard = St.Clipboard.get_default();
            clipboard.set_content(St.ClipboardType.CLIPBOARD, 'image/png', bytes);
        }

        let retval = GLib.Variant.new('(bs)', [true, filenameUsed]);
        invocation.return_value(retval);
    }

    _scaleArea(x, y, width, height) {
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        x *= scaleFactor;
        y *= scaleFactor;
        width *= scaleFactor;
        height *= scaleFactor;
        return [x, y, width, height];
    }

    _unscaleArea(x, y, width, height) {
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        x /= scaleFactor;
        y /= scaleFactor;
        width /= scaleFactor;
        height /= scaleFactor;
        return [x, y, width, height];
    }

    async ScreenshotAreaAsync(params, invocation) {
        let [x, y, width, height, flash, filename] = params;
        [x, y, width, height] = this._scaleArea(x, y, width, height);
        if (!this._checkArea(x, y, width, height)) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot_area(x, y, width, height, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async ScreenshotWindowAsync(params, invocation) {
        let [includeFrame, includeCursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot_window(includeFrame, includeCursor, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async ScreenshotAsync(params, invocation) {
        let [includeCursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot(includeCursor, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async SelectAreaAsync(params, invocation) {
        let selectArea = new SelectArea();
        try {
            let areaRectangle = await selectArea.selectAsync();
            let retRectangle = this._unscaleArea(
                areaRectangle.x, areaRectangle.y,
                areaRectangle.width, areaRectangle.height);
            invocation.return_value(GLib.Variant.new('(iiii)', retRectangle));
        } catch (e) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                'Operation was cancelled');
        }
    }

    FlashAreaAsync(params, invocation) {
        let [x, y, width, height] = params;
        [x, y, width, height] = this._scaleArea(x, y, width, height);
        if (!this._checkArea(x, y, width, height)) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }
        let flashspot = new Flashspot({ x, y, width, height });
        flashspot.fire();
        invocation.return_value(null);
    }

    async PickColorAsync(params, invocation) {
        const screenshot = this._createScreenshot(invocation, false);
        if (!screenshot)
            return;

        const pickPixel = new PickPixel(screenshot);
        try {
            const color = await pickPixel.pickAsync();
            const { red, green, blue } = color;
            const retval = GLib.Variant.new('(a{sv})', [{
                color: GLib.Variant.new('(ddd)', [
                    red / 255.0,
                    green / 255.0,
                    blue / 255.0,
                ]),
            }]);
            invocation.return_value(retval);
        } catch (e) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                'Operation was cancelled');
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }
};

var SelectArea = GObject.registerClass(
class SelectArea extends St.Widget {
    _init() {
        this._startX = -1;
        this._startY = -1;
        this._lastX = 0;
        this._lastY = 0;
        this._result = null;

        super._init({
            visible: false,
            reactive: true,
            x: 0,
            y: 0,
        });
        Main.uiGroup.add_actor(this);

        this._grabHelper = new GrabHelper.GrabHelper(this);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this.add_constraint(constraint);

        this._rubberband = new St.Widget({
            style_class: 'select-area-rubberband',
            visible: false,
        });
        this.add_actor(this._rubberband);
    }

    async selectAsync() {
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        Main.uiGroup.set_child_above_sibling(this, null);
        this.show();

        try {
            await this._grabHelper.grabAsync({ actor: this });
        } finally {
            global.display.set_cursor(Meta.Cursor.DEFAULT);

            GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this.destroy();
                return GLib.SOURCE_REMOVE;
            });
        }

        return this._result;
    }

    _getGeometry() {
        return new Meta.Rectangle({
            x: Math.min(this._startX, this._lastX),
            y: Math.min(this._startY, this._lastY),
            width: Math.abs(this._startX - this._lastX) + 1,
            height: Math.abs(this._startY - this._lastY) + 1,
        });
    }

    vfunc_motion_event(motionEvent) {
        if (this._startX == -1 || this._startY == -1 || this._result)
            return Clutter.EVENT_PROPAGATE;

        [this._lastX, this._lastY] = [motionEvent.x, motionEvent.y];
        this._lastX = Math.floor(this._lastX);
        this._lastY = Math.floor(this._lastY);
        let geometry = this._getGeometry();

        this._rubberband.set_position(geometry.x, geometry.y);
        this._rubberband.set_size(geometry.width, geometry.height);
        this._rubberband.show();

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_press_event(buttonEvent) {
        if (this._result)
            return Clutter.EVENT_PROPAGATE;

        [this._startX, this._startY] = [buttonEvent.x, buttonEvent.y];
        this._startX = Math.floor(this._startX);
        this._startY = Math.floor(this._startY);
        this._rubberband.set_position(this._startX, this._startY);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event() {
        if (this._startX === -1 || this._startY === -1 || this._result)
            return Clutter.EVENT_PROPAGATE;

        this._result = this._getGeometry();
        this.ease({
            opacity: 0,
            duration: 200,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._grabHelper.ungrab(),
        });
        return Clutter.EVENT_PROPAGATE;
    }
});

var RecolorEffect = GObject.registerClass({
    Properties: {
        color: GObject.ParamSpec.boxed(
            'color', 'color', 'replacement color',
            GObject.ParamFlags.WRITABLE,
            Clutter.Color.$gtype),
        chroma: GObject.ParamSpec.boxed(
            'chroma', 'chroma', 'color to replace',
            GObject.ParamFlags.WRITABLE,
            Clutter.Color.$gtype),
        threshold: GObject.ParamSpec.float(
            'threshold', 'threshold', 'threshold',
            GObject.ParamFlags.WRITABLE,
            0.0, 1.0, 0.0),
        smoothing: GObject.ParamSpec.float(
            'smoothing', 'smoothing', 'smoothing',
            GObject.ParamFlags.WRITABLE,
            0.0, 1.0, 0.0),
    },
}, class RecolorEffect extends Shell.GLSLEffect {
    _init(params) {
        this._color = new Clutter.Color();
        this._chroma = new Clutter.Color();
        this._threshold = 0;
        this._smoothing = 0;

        this._colorLocation = null;
        this._chromaLocation = null;
        this._thresholdLocation = null;
        this._smoothingLocation = null;

        super._init(params);

        this._colorLocation = this.get_uniform_location('recolor_color');
        this._chromaLocation = this.get_uniform_location('chroma_color');
        this._thresholdLocation = this.get_uniform_location('threshold');
        this._smoothingLocation = this.get_uniform_location('smoothing');

        this._updateColorUniform(this._colorLocation, this._color);
        this._updateColorUniform(this._chromaLocation, this._chroma);
        this._updateFloatUniform(this._thresholdLocation, this._threshold);
        this._updateFloatUniform(this._smoothingLocation, this._smoothing);
    }

    _updateColorUniform(location, color) {
        if (!location)
            return;

        this.set_uniform_float(location,
            3, [color.red / 255, color.green / 255, color.blue / 255]);
        this.queue_repaint();
    }

    _updateFloatUniform(location, value) {
        if (!location)
            return;

        this.set_uniform_float(location, 1, [value]);
        this.queue_repaint();
    }

    set color(c) {
        if (this._color.equal(c))
            return;

        this._color = c;
        this.notify('color');

        this._updateColorUniform(this._colorLocation, this._color);
    }

    set chroma(c) {
        if (this._chroma.equal(c))
            return;

        this._chroma = c;
        this.notify('chroma');

        this._updateColorUniform(this._chromaLocation, this._chroma);
    }

    set threshold(value) {
        if (this._threshold === value)
            return;

        this._threshold = value;
        this.notify('threshold');

        this._updateFloatUniform(this._thresholdLocation, this._threshold);
    }

    set smoothing(value) {
        if (this._smoothing === value)
            return;

        this._smoothing = value;
        this.notify('smoothing');

        this._updateFloatUniform(this._smoothingLocation, this._smoothing);
    }

    vfunc_build_pipeline() {
        // Conversion parameters from https://en.wikipedia.org/wiki/YCbCr
        const decl = `
            vec3 rgb2yCrCb(vec3 c) {                                \n
                float y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;  \n
                float cr = 0.7133 * (c.r - y);                      \n
                float cb = 0.5643 * (c.b - y);                      \n
                return vec3(y, cr, cb);                             \n
            }                                                       \n
                                                                    \n
            uniform vec3 chroma_color;                              \n
            uniform vec3 recolor_color;                             \n
            uniform float threshold;                                \n
            uniform float smoothing;                                \n`;
        const src = `
            vec3 mask = rgb2yCrCb(chroma_color.rgb);                \n
            vec3 yCrCb = rgb2yCrCb(cogl_color_out.rgb);             \n
            float blend =                                           \n
              smoothstep(threshold,                                 \n
                         threshold + smoothing,                     \n
                         distance(yCrCb.gb, mask.gb));              \n
            cogl_color_out.rgb =                                    \n
              mix(recolor_color, cogl_color_out.rgb, blend);        \n`;

        this.add_glsl_snippet(Shell.SnippetHook.FRAGMENT, decl, src, false);
    }
});

var PickPixel = GObject.registerClass(
class PickPixel extends St.Widget {
    _init(screenshot) {
        super._init({ visible: false, reactive: true });

        this._screenshot = screenshot;

        this._result = null;
        this._color = null;
        this._inPick = false;

        Main.uiGroup.add_actor(this);

        this._grabHelper = new GrabHelper.GrabHelper(this);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this.add_constraint(constraint);

        const action = new Clutter.ClickAction();
        action.connect('clicked', async () => {
            await this._pickColor(...action.get_coords());
            this._result = this._color;
            this._grabHelper.ungrab();
        });
        this.add_action(action);

        this._recolorEffect = new RecolorEffect({
            chroma: new Clutter.Color({
                red: 80,
                green: 219,
                blue: 181,
            }),
            threshold: 0.04,
            smoothing: 0.07,
        });
        this._previewCursor = new St.Icon({
            icon_name: 'color-pick',
            icon_size: Meta.prefs_get_cursor_size(),
            effect: this._recolorEffect,
            visible: false,
        });
        Main.uiGroup.add_actor(this._previewCursor);
    }

    async pickAsync() {
        global.display.set_cursor(Meta.Cursor.BLANK);
        Main.uiGroup.set_child_above_sibling(this, null);
        this.show();

        this._pickColor(...global.get_pointer());

        try {
            await this._grabHelper.grabAsync({ actor: this });
        } finally {
            global.display.set_cursor(Meta.Cursor.DEFAULT);
            this._previewCursor.destroy();

            GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this.destroy();
                return GLib.SOURCE_REMOVE;
            });
        }

        return this._result;
    }

    async _pickColor(x, y) {
        if (this._inPick)
            return;

        this._inPick = true;
        this._previewCursor.set_position(x, y);
        [this._color] = await this._screenshot.pick_color(x, y);
        this._inPick = false;

        if (!this._color)
            return;

        this._recolorEffect.color = this._color;
        this._previewCursor.show();
    }

    vfunc_motion_event(motionEvent) {
        const { x, y } = motionEvent;
        this._pickColor(x, y);
        return Clutter.EVENT_PROPAGATE;
    }
});

var FLASHSPOT_ANIMATION_OUT_TIME = 500; // milliseconds

var Flashspot = GObject.registerClass(
class Flashspot extends Lightbox.Lightbox {
    _init(area) {
        super._init(Main.uiGroup, {
            inhibitEvents: true,
            width: area.width,
            height: area.height,
        });
        this.style_class = 'flashspot';
        this.set_position(area.x, area.y);
    }

    fire(doneCallback) {
        this.set({ visible: true, opacity: 255 });
        this.ease({
            opacity: 0,
            duration: FLASHSPOT_ANIMATION_OUT_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                if (doneCallback)
                    doneCallback();
                this.destroy();
            },
        });
    }
});
