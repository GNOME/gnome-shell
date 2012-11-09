// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const SelectArea = new Lang.Class({
    Name: 'SelectArea',

    _init: function() {
        this._startX = -1;
        this._startY = -1;
        this._lastX = 0;
        this._lastY = 0;

        this._initRubberbandColors();

        this._group = new St.Widget({ visible: false,
                                      reactive: true,
                                      x: 0,
                                      y: 0 });
        Main.uiGroup.add_actor(this._group);

        this._group.connect('button-press-event',
                            Lang.bind(this, this._onButtonPress));
        this._group.connect('button-release-event',
                            Lang.bind(this, this._onButtonRelease));
        this._group.connect('key-press-event',
                            Lang.bind(this, this._onKeyPress));
        this._group.connect('motion-event',
                            Lang.bind(this, this._onMotionEvent));

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._group.add_constraint(constraint);

        this._rubberband = new Clutter.Rectangle({ color: this._background,
                                                   has_border: true,
                                                   border_width: 1,
                                                   border_color: this._border });
        this._group.add_actor(this._rubberband);
    },

    show: function() {
        if (!Main.pushModal(this._group) || this._group.visible)
            return;

        global.set_cursor(Shell.Cursor.CROSSHAIR);
        this._group.visible = true;
    },

    _initRubberbandColors: function() {
        function colorFromRGBA(rgba) {
            return new Clutter.Color({ red: rgba.red * 255,
                                       green: rgba.green * 255,
                                       blue: rgba.blue * 255,
                                       alpha: rgba.alpha * 255 });
        }

        let path = new Gtk.WidgetPath();
        path.append_type(Gtk.IconView);

        let context = new Gtk.StyleContext();
        context.set_path(path);
        context.add_class('rubberband');

        this._background = colorFromRGBA(context.get_background_color(Gtk.StateFlags.NORMAL));
        this._border = colorFromRGBA(context.get_border_color(Gtk.StateFlags.NORMAL));
    },

    _getGeometry: function() {
        return { x: Math.min(this._startX, this._lastX),
                 y: Math.min(this._startY, this._lastY),
                 width: Math.abs(this._startX - this._lastX),
                 height: Math.abs(this._startY - this._lastY) };
    },

    _onKeyPress: function(actor, event) {
        if (event.get_key_symbol() == Clutter.Escape)
            this._destroy(null, false);

        return;
    },

    _onMotionEvent: function(actor, event) {
        if (this._startX == -1 || this._startY == -1)
            return false;

        [this._lastX, this._lastY] = event.get_coords();
        let geometry = this._getGeometry();

        this._rubberband.set_position(geometry.x, geometry.y);
        this._rubberband.set_size(geometry.width, geometry.height);

        return false;
    },

    _onButtonPress: function(actor, event) {
        [this._startX, this._startY] = event.get_coords();
        this._rubberband.set_position(this._startX, this._startY);

        return false;
    },

    _onButtonRelease: function(actor, event) {
        this._destroy(this._getGeometry(), true);
        return false;
    },

    _destroy: function(geometry, fade) {
        Tweener.addTween(this._group,
                         { opacity: 0,
                           time: fade ? 0.2 : 0,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                               function() {
                                   Main.popModal(this._group);
                                   this._group.destroy();
                                   global.unset_cursor();

                                   this.emit('finished', geometry);
                               })
                         });
    }
});
Signals.addSignalMethods(SelectArea.prototype);
