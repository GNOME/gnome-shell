/* exported PointerA11yTimeout */
const { Clutter, GObject, Meta, St } = imports.gi;
const Main = imports.ui.main;
const Cairo = imports.cairo;

const SUCCESS_ZOOM_OUT_DURATION = 150;

var PieTimer = GObject.registerClass({
    Properties: {
        'angle': GObject.ParamSpec.double(
            'angle', 'angle', 'angle',
            GObject.ParamFlags.READWRITE,
            0, 2 * Math.PI, 0),
    },
}, class PieTimer extends St.DrawingArea {
    _init() {
        this._angle = 0;
        super._init({
            style_class: 'pie-timer',
            opacity: 0,
            visible: false,
            can_focus: false,
            reactive: false,
        });

        this.set_pivot_point(0.5, 0.5);
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

        if (this._angle < 2 * Math.PI)
            cr.moveTo(0, 0);

        cr.arc(0, 0, radius - borderWidth, startAngle, endAngle);

        if (this._angle < 2 * Math.PI)
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
        this.x = x - this.width / 2;
        this.y = y - this.height / 2;
        this.show();

        this.ease({
            opacity: 255,
            duration: duration / 4,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
        });

        this.ease_property('angle', 2 * Math.PI, {
            duration,
            mode: Clutter.AnimationMode.LINEAR,
            onComplete: this._onTransitionComplete.bind(this),
        });
    }

    _onTransitionComplete() {
        this.ease({
            scale_x: 2,
            scale_y: 2,
            opacity: 0,
            duration: SUCCESS_ZOOM_OUT_DURATION,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => this.destroy(),
        });
    }
});

var PointerA11yTimeout = class PointerA11yTimeout {
    constructor() {
        let seat = Clutter.get_default_backend().get_default_seat();

        seat.connect('ptr-a11y-timeout-started', (o, device, type, timeout) => {
            let [x, y] = global.get_pointer();

            this._pieTimer = new PieTimer();
            Main.uiGroup.add_actor(this._pieTimer);
            Main.uiGroup.set_child_above_sibling(this._pieTimer, null);

            this._pieTimer.start(x, y, timeout);

            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
                global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        });

        seat.connect('ptr-a11y-timeout-stopped', (o, device, type, clicked) => {
            if (!clicked)
                this._pieTimer.destroy();

            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
                global.display.set_cursor(Meta.Cursor.DEFAULT);
        });
    }
};
