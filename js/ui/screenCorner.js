// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenCorner */

const { Clutter, GObject, Meta, St } = imports.gi;
const Cairo = imports.cairo;

const Layout = imports.ui.layout;

var ScreenCorner = GObject.registerClass(
class ScreenCorner extends St.DrawingArea {
    _init(corner, monitor) {
        super._init({ style_class: 'screen-corner' });

        this._corner = corner;

        this.add_constraint(new Layout.MonitorConstraint({ index: monitor.index }));

        if (corner === Meta.DisplayCorner.TOPRIGHT ||
            corner === Meta.DisplayCorner.BOTTOMRIGHT)
            this.x_align = Clutter.ActorAlign.END;

        if (corner === Meta.DisplayCorner.BOTTOMLEFT ||
            corner === Meta.DisplayCorner.BOTTOMRIGHT)
            this.y_align = Clutter.ActorAlign.END;
    }

    vfunc_repaint() {
        let node = this.get_theme_node();

        let cornerRadius = node.get_length("-screen-corner-radius");
        let backgroundColor = node.get_color('-screen-corner-background-color');

        let cr = this.get_context();
        cr.setOperator(Cairo.Operator.SOURCE);

        switch (this._corner) {
        case Meta.DisplayCorner.TOPLEFT:
            cr.arc(cornerRadius, cornerRadius,
                   cornerRadius, Math.PI, 3 * Math.PI / 2);
            cr.lineTo(0, 0);
            break;

        case Meta.DisplayCorner.TOPRIGHT:
            cr.arc(0, cornerRadius,
                   cornerRadius, 3 * Math.PI / 2, 2 * Math.PI);
            cr.lineTo(cornerRadius, 0);
            break;

        case Meta.DisplayCorner.BOTTOMLEFT:
            cr.arc(cornerRadius, 0,
                   cornerRadius, Math.PI / 2, Math.PI);
            cr.lineTo(0, cornerRadius);
            break;

        case Meta.DisplayCorner.BOTTOMRIGHT:
            cr.arc(0, 0,
                   cornerRadius, 0, Math.PI / 2);
            cr.lineTo(cornerRadius, cornerRadius);
            break;
        }

        cr.closePath();

        Clutter.cairo_set_source_color(cr, backgroundColor);
        cr.fill();

        cr.$dispose();
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();

        let node = this.get_theme_node();

        let cornerRadius = node.get_length("-screen-corner-radius");

        this.set_size(cornerRadius, cornerRadius);
    }
});
