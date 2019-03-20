const { Clutter, GLib, GObject, Meta, St } = imports.gi;
const Tweener = imports.ui.tweener;
const Main = imports.ui.main;
const Cairo = imports.cairo;

const ANIMATION_STEPS = 36.;

var PieTimer = GObject.registerClass(
class PieTimer extends St.DrawingArea {
    _init() {
        this._x = 0;
        this._y = 0;
        this._startTime = 0;
        this._duration = 0;
        super._init( { style_class: 'pie-timer',
                       visible: false,
                       can_focus: false,
                       reactive: false });
    }

    vfunc_repaint() {
        let node = this.get_theme_node();
        let backgroundColor = node.get_color('-pie-background-color');
        let borderColor = node.get_color('-pie-border-color');
        let borderWidth = node.get_length('-pie-border-width');
        let [width, height] = this.get_surface_size();
        let radius = Math.min(width / 2, height / 2);

        let currentTime = GLib.get_monotonic_time() / 1000.0;
        let ellapsed = currentTime - this._startTime;
        let angle = (ellapsed / this._duration) * 2 * Math.PI;
        let startAngle = 3 * Math.PI / 2;
        let endAngle = startAngle + angle;

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
        Tweener.removeTweens(this);

        this.x = x - this.width / 2;
        this.y = y - this.height / 2;
        this.show();
        Main.uiGroup.set_child_above_sibling(this, null);

        this._startTime = GLib.get_monotonic_time() / 1000.0;
        this._duration = duration;

        Tweener.addTween(this,
                         { opacity: 255,
                           time: duration / 1000,
                           transition: 'easeOutQuad',
                           onUpdateScope: this,
                           onUpdate() { this.queue_repaint() },
                           onCompleteScope: this,
                           onComplete() { this.stop(); }
                          });
    }

    stop() {
        Tweener.removeTweens(this);
        this.hide();
    }
});

var PointerA11yTimeout = class PointerA11yTimeout {
    constructor() {
        let manager = Clutter.DeviceManager.get_default();
        let pieTimer = new PieTimer();

        Main.uiGroup.add_actor(pieTimer);

        manager.connect('ptr-a11y-timeout-started', (manager, device, type, timeout) => {
            let [x, y, mods] = global.get_pointer();
            pieTimer.start(x, y, timeout);
            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
              global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        });

        manager.connect('ptr-a11y-timeout-stopped', (manager, device, type) => {
            pieTimer.stop();
            if (type == Clutter.PointerA11yTimeoutType.GESTURE)
              global.display.set_cursor(Meta.Cursor.DEFAULT);
        });
    }
};
