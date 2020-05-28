// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenshotService */

const { Clutter, Graphene, Gio, GObject, GLib, Meta, Shell, St } = imports.gi;

const GrabHelper = imports.ui.grabHelper;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

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
        if (this._screenShooter.has(sender) || lockedDown) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.BUSY,
                'There is an ongoing operation for this sender');
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
        ].find(p => GLib.file_test(p, GLib.FileTest.EXISTS));

        if (!path)
            return null;

        yield Gio.File.new_for_path(
            GLib.build_filenamev([path, `${filename}.png`]));

        for (let idx = 1; ; idx++) {
            yield Gio.File.new_for_path(
                GLib.build_filenamev([path, `${filename}-${idx}.png`]));
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
                invocation.return_value(GLib.Variant.new('(bs)', [false, '']));
                return [null, null];
            }
        }

        for (let file of this._resolveRelativeFilename(filename)) {
            try {
                let stream = file.create(Gio.FileCreateFlags.NONE, null);
                return [stream, file];
            } catch (e) {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                    break;
            }
        }

        invocation.return_value(GLib.Variant.new('(bs)', [false, '']));
        return [null, null];
    }

    _onScreenshotComplete(area, stream, file, flash, invocation) {
        if (flash) {
            let flashspot = new Flashspot(area);
            flashspot.fire(() => {
                this._removeShooterForSender(invocation.get_sender());
            });
        } else {
            this._removeShooterForSender(invocation.get_sender());
        }

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

    ScreenshotAreaAsync(params, invocation) {
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

        screenshot.screenshot_area(x, y, width, height, stream,
            (o, res) => {
                try {
                    let [success_, area] =
                        screenshot.screenshot_area_finish(res);
                    this._onScreenshotComplete(
                        area, stream, file, flash, invocation);
                } catch (e) {
                    this._removeShooterForSender(invocation.get_sender());
                    invocation.return_value(new GLib.Variant('(bs)', [false, '']));
                }
            });
    }

    ScreenshotWindowAsync(params, invocation) {
        let [includeFrame, includeCursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        screenshot.screenshot_window(includeFrame, includeCursor, stream,
            (o, res) => {
                try {
                    let [success_, area] =
                        screenshot.screenshot_window_finish(res);
                    this._onScreenshotComplete(
                        area, stream, file, flash, invocation);
                } catch (e) {
                    this._removeShooterForSender(invocation.get_sender());
                    invocation.return_value(new GLib.Variant('(bs)', [false, '']));
                }
            });
    }

    ScreenshotAsync(params, invocation) {
        let [includeCursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        screenshot.screenshot(includeCursor, stream,
            (o, res) => {
                try {
                    let [success_, area] =
                        screenshot.screenshot_finish(res);
                    this._onScreenshotComplete(
                        area, stream, file, flash, invocation);
                } catch (e) {
                    this._removeShooterForSender(invocation.get_sender());
                    invocation.return_value(new GLib.Variant('(bs)', [false, '']));
                }
            });
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
        let pickPixel = new PickPixel();
        try {
            const coords = await pickPixel.pickAsync();

            let screenshot = this._createScreenshot(invocation, false);
            if (!screenshot)
                return;

            screenshot.pick_color(coords.x, coords.y, (_o, res) => {
                let [success_, color] = screenshot.pick_color_finish(res);
                let { red, green, blue } = color;
                let retval = GLib.Variant.new('(a{sv})', [{
                    color: GLib.Variant.new('(ddd)', [
                        red / 255.0,
                        green / 255.0,
                        blue / 255.0,
                    ]),
                }]);
                this._removeShooterForSender(invocation.get_sender());
                invocation.return_value(retval);
            });
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

        await this._grabHelper.grabAsync({ actor: this });

        global.display.set_cursor(Meta.Cursor.DEFAULT);

        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this.destroy();
            return GLib.SOURCE_REMOVE;
        });

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
        [this._startX, this._startY] = [buttonEvent.x, buttonEvent.y];
        this._startX = Math.floor(this._startX);
        this._startY = Math.floor(this._startY);
        this._rubberband.set_position(this._startX, this._startY);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event() {
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

var PickPixel = GObject.registerClass(
class PickPixel extends St.Widget {
    _init() {
        super._init({ visible: false, reactive: true });

        this._result = null;

        Main.uiGroup.add_actor(this);

        this._grabHelper = new GrabHelper.GrabHelper(this);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this.add_constraint(constraint);
    }

    async pickAsync() {
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        Main.uiGroup.set_child_above_sibling(this, null);
        this.show();

        await this._grabHelper.grabAsync({ actor: this });

        global.display.set_cursor(Meta.Cursor.DEFAULT);

        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this.destroy();
            return GLib.SOURCE_REMOVE;
        });

        return this._result;
    }

    vfunc_button_release_event(buttonEvent) {
        let { x, y } = buttonEvent;
        this._result = new Graphene.Point({ x, y });
        this._grabHelper.ungrab();
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
