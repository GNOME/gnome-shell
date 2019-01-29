// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const GrabHelper = imports.ui.grabHelper;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

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
            invocation.return_value(GLib.Variant.new('(bs)', [false, '']));
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

    _onScreenshotComplete(result, area, filenameUsed, flash, invocation) {
        if (result) {
            if (flash) {
                let flashspot = new Flashspot(area);
                flashspot.fire(() => {
                    this._removeShooterForSender(invocation.get_sender());
                });
            } else {
                this._removeShooterForSender(invocation.get_sender());
            }
        }

        let retval = GLib.Variant.new('(bs)', [result, filenameUsed]);
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
        screenshot.screenshot_area (x, y, width, height, filename,
            (o, res) => {
                try {
                    let [result, area, filenameUsed] =
                        screenshot.screenshot_area_finish(res);
                    this._onScreenshotComplete(result, area, filenameUsed,
                                               flash, invocation);
                } catch (e) {
                    invocation.return_gerror (e);
                }
            });
    }

    ScreenshotWindowAsync(params, invocation) {
        let [include_frame, include_cursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;
        screenshot.screenshot_window (include_frame, include_cursor, filename,
            (o, res) => {
                try {
                    let [result, area, filenameUsed] =
                        screenshot.screenshot_window_finish(res);
                    this._onScreenshotComplete(result, area, filenameUsed,
                                               flash, invocation);
                } catch (e) {
                    invocation.return_gerror (e);
                }
            });
    }

    ScreenshotAsync(params, invocation) {
        let [include_cursor, flash, filename] = params;
        let screenshot = this._createScreenshot(invocation);
        if (!screenshot)
            return;
        screenshot.screenshot(include_cursor, filename,
            (o, res) => {
                try {
                    let [result, area, filenameUsed] =
                        screenshot.screenshot_finish(res);
                    this._onScreenshotComplete(result, area, filenameUsed,
                                               flash, invocation);
                } catch (e) {
                    invocation.return_gerror (e);
                }
            });
    }

    SelectAreaAsync(params, invocation) {
        let selectArea = new SelectArea();
        selectArea.show();
        selectArea.connect('finished', (selectArea, areaRectangle) => {
            if (areaRectangle) {
                let retRectangle = this._unscaleArea(areaRectangle.x, areaRectangle.y,
                    areaRectangle.width, areaRectangle.height);
                let retval = GLib.Variant.new('(iiii)', retRectangle);
                invocation.return_value(retval);
            } else {
                invocation.return_error_literal(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                    "Operation was cancelled");
            }
        });
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
        let flashspot = new Flashspot({ x: x, y: y, width: width, height: height });
        flashspot.fire();
        invocation.return_value(null);
    }

    PickColorAsync(params, invocation) {
        let pickPixel = new PickPixel();
        pickPixel.show();
        pickPixel.connect('finished', (pickPixel, coords) => {
            if (coords) {
                let screenshot = this._createScreenshot(invocation, false);
                if (!screenshot)
                    return;
                screenshot.pick_color(...coords, (o, res) => {
                    let [success, color] = screenshot.pick_color_finish(res);
                    let { red, green, blue } = color;
                    let retval = GLib.Variant.new('(a{sv})', [{
                        color: GLib.Variant.new('(ddd)', [
                            red / 255.0,
                            green / 255.0,
                            blue / 255.0
                        ])
                    }]);
                    this._removeShooterForSender(invocation.get_sender());
                    invocation.return_value(retval);
                });
            } else {
                invocation.return_error_literal(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                    "Operation was cancelled");
            }
        });
    }
};

var SelectArea = class {
    constructor() {
        this._startX = -1;
        this._startY = -1;
        this._lastX = 0;
        this._lastY = 0;
        this._result = null;

        this._group = new St.Widget({ visible: false,
                                      reactive: true,
                                      x: 0,
                                      y: 0 });
        Main.uiGroup.add_actor(this._group);

        this._grabHelper = new GrabHelper.GrabHelper(this._group);

        this._group.connect('button-press-event',
                            this._onButtonPress.bind(this));
        this._group.connect('button-release-event',
                            this._onButtonRelease.bind(this));
        this._group.connect('motion-event',
                            this._onMotionEvent.bind(this));

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._group.add_constraint(constraint);

        this._rubberband = new St.Widget({
            style_class: 'select-area-rubberband',
            visible: false
        });
        this._group.add_actor(this._rubberband);
    }

    show() {
        if (!this._grabHelper.grab({ actor: this._group,
                                     onUngrab: this._onUngrab.bind(this) }))
            return;

        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        Main.uiGroup.set_child_above_sibling(this._group, null);
        this._group.visible = true;
    }

    _getGeometry() {
        return { x: Math.min(this._startX, this._lastX),
                 y: Math.min(this._startY, this._lastY),
                 width: Math.abs(this._startX - this._lastX) + 1,
                 height: Math.abs(this._startY - this._lastY) + 1 };
    }

    _onMotionEvent(actor, event) {
        if (this._startX == -1 || this._startY == -1)
            return Clutter.EVENT_PROPAGATE;

        [this._lastX, this._lastY] = event.get_coords();
        this._lastX = Math.floor(this._lastX);
        this._lastY = Math.floor(this._lastY);
        let geometry = this._getGeometry();

        this._rubberband.set_position(geometry.x, geometry.y);
        this._rubberband.set_size(geometry.width, geometry.height);
        this._rubberband.show();

        return Clutter.EVENT_PROPAGATE;
    }

    _onButtonPress(actor, event) {
        [this._startX, this._startY] = event.get_coords();
        this._startX = Math.floor(this._startX);
        this._startY = Math.floor(this._startY);
        this._rubberband.set_position(this._startX, this._startY);

        return Clutter.EVENT_PROPAGATE;
    }

    _onButtonRelease(actor, event) {
        this._result = this._getGeometry();
        Tweener.addTween(this._group,
                         { opacity: 0,
                           time: 0.2,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                               this._grabHelper.ungrab();
                           }
                         });
        return Clutter.EVENT_PROPAGATE;
    }

    _onUngrab() {
        global.display.set_cursor(Meta.Cursor.DEFAULT);
        this.emit('finished', this._result);

        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._group.destroy();
            return GLib.SOURCE_REMOVE;
        });
    }
};
Signals.addSignalMethods(SelectArea.prototype);

var PickPixel = class {
    constructor() {
        this._result = null;

        this._group = new St.Widget({ visible: false,
                                      reactive: true });
        Main.uiGroup.add_actor(this._group);

        this._grabHelper = new GrabHelper.GrabHelper(this._group);

        this._group.connect('button-release-event',
                            this._onButtonRelease.bind(this));

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._group.add_constraint(constraint);
    }

    show() {
        if (!this._grabHelper.grab({ actor: this._group,
                                     onUngrab: this._onUngrab.bind(this) }))
            return;

        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        Main.uiGroup.set_child_above_sibling(this._group, null);
        this._group.visible = true;
    }

    _onButtonRelease(actor, event) {
        this._result = event.get_coords();
        this._grabHelper.ungrab();
        return Clutter.EVENT_PROPAGATE;
    }

    _onUngrab() {
        global.display.set_cursor(Meta.Cursor.DEFAULT);
        this.emit('finished', this._result);

        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._group.destroy();
            return GLib.SOURCE_REMOVE;
        });
    }
};
Signals.addSignalMethods(PickPixel.prototype);

var FLASHSPOT_ANIMATION_OUT_TIME = 0.5; // seconds

var Flashspot = class extends Lightbox.Lightbox {
    constructor(area) {
        super(Main.uiGroup, { inhibitEvents: true,
                              width: area.width,
                              height: area.height });

        this.actor.style_class = 'flashspot';
        this.actor.set_position(area.x, area.y);
    }

    fire(doneCallback) {
        this.actor.show();
        this.actor.opacity = 255;
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: FLASHSPOT_ANIMATION_OUT_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                               if (doneCallback)
                                   doneCallback();
                               this.destroy();
                           }
                         });
    }
};
