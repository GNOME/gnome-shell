/* exported PointerA11yTimeout */
const { Clutter, GLib, GObject, Meta, St } = imports.gi;
const Main = imports.ui.main;
const Cairo = imports.cairo;

var PieTimer = GObject.registerClass({
    Properties: {
        'angle': GObject.ParamSpec.double(
            'angle', 'angle', 'angle',
            GObject.ParamFlags.READWRITE,
            0, 2 * Math.PI, 0)
    }
}, class PieTimer extends St.DrawingArea {
    _init() {
        this._angle = 0;
        super._init({
            style_class: 'pie-timer',
            visible: false,
            can_focus: false,
            reactive: false
        });
    }

    get angle() {
        return this._angle;
    }

    set angle(angle) {
        if (this._angle == angle)
            return;

        this._angle = angle;
        this.notify('angle');
        this.queue_repaint();
    }

    vfunc_repaint() {
        let node = this.get_theme_node();
        let backgroundColor = node.get_color('-pie-background-color');
        let borderColor = node.get_color('-pie-border-color');
        let borderWidth = node.get_length('-pie-border-width');
        let [width, height] = this.get_surface_size();
        let radius = Math.min(width / 2, height / 2);

        let startAngle = 3 * Math.PI / 2;
        let endAngle = startAngle + this._angle;

        let cr = this.get_context();
        cr.setLineCap(Cairo.LineCap.ROUND);
        cr.setLineJoin(Cairo.LineJoin.ROUND);
        cr.translate(width / 2, height / 2);

        cr.moveTo(0, 0);
        cr.arc(0, 0, radius - borderWidth, startAngle, endAngle);
        cr.lineTo(0, 0);
        cr.closePath();

        cr.setLineWidth(0);
        Clutter.cairo_set_source_color(cr, backgroundColor);
        cr.fillPreserve();

        cr.setLineWidth(borderWidth);
        Clutter.cairo_set_source_color(cr, borderColor);
        cr.stroke();

        cr.$dispose();
    }

    start(x, y, duration) {
        this.remove_all_transitions();

        this.opacity = 0;
        this.x = x - this.width / 2;
        this.y = y - this.height / 2;
        this._angle = 0;

        this.show();
        Main.uiGroup.set_child_above_sibling(this, null);

        this.ease({
            opacity: 255,
            duration: duration / 4,
            mode: Clutter.AnimationMode.EASE_IN_QUAD
        });

        this.ease_property('angle', 2 * Math.PI, {
            duration,
            mode: Clutter.AnimationMode.LINEAR,
            onComplete: () => this.stop()
        });
    }

    stop() {
        this.remove_all_transitions();
        this.hide();
    }
});

var PointerA11yTimeout = class PointerA11yTimeout {
    constructor() {
        let manager = Clutter.DeviceManager.get_default();
        let pieTimer = new PieTimer();

        Main.uiGroup.add_actor(pieTimer);

        manager.connect('ptr-a11y-timeout-started', (manager, device, type, timeout) => {
            let [x, y] = global.get_pointer();
            pieTimer.start(x, y, timeout);
            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
                global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        });

        manager.connect('ptr-a11y-timeout-stopped', (manager, device, type, clicked) => {
            if (!clicked)
                pieTimer.stop();

            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
                global.display.set_cursor(Meta.Cursor.DEFAULT);
        });
    }
};
