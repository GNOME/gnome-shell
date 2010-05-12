const Lang = imports.lang;

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

/**
 * BoxPointer:
 * @side: A St.Side type; currently only St.Side.TOP is implemented
 * @binProperties: Properties to set on contained bin
 *
 * An actor which displays a triangle "arrow" pointing to a given
 * side.  The .bin property is a container in which content can be
 * placed.  The arrow position may be controlled via setArrowOrigin().
 *
 */
function BoxPointer(side, binProperties) {
    this._init(side, binProperties);
}

BoxPointer.prototype = {
    _init: function(arrowSide, binProperties) {
        if (arrowSide != St.Side.TOP)
            throw new Error('Not implemented');
        this._arrowSide = arrowSide;
        this._arrowOrigin = 0;
        this.actor = new St.Bin({ x_fill: true,
                                  y_fill: true });
        this._container = new Shell.GenericContainer();
        this.actor.set_child(this._container);
        this._container.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._allocate));
        this.bin = new St.Bin(binProperties);
        this._container.add_actor(this.bin);
        this._border = new St.DrawingArea();
        this._border.connect('repaint', Lang.bind(this, this._drawBorder));
        this._container.add_actor(this._border);
        this.bin.raise(this._border);
    },

    _adjustAllocationForArrow: function(isWidth, alloc) {
        let themeNode = this.actor.get_theme_node();
        let found, borderWidth, base, rise;
        [found, borderWidth] = themeNode.get_length('-arrow-border-width', false);
        alloc.min_size += borderWidth * 2;
        alloc.natural_size += borderWidth * 2;
        if ((!isWidth && (this._arrowSide == St.Side.TOP || this._arrowSide == St.Side.BOTTOM))
            || (isWidth && (this._arrowSide == St.Side.LEFT || this._arrowSide == St.Side.RIGHT))) {
            let [found, rise] = themeNode.get_length('-arrow-rise', false);
            alloc.min_size += rise;
            alloc.natural_size += rise;
        }
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [minInternalSize, natInternalSize] = this.bin.get_preferred_width(forHeight);
        alloc.min_size = minInternalSize;
        alloc.natural_size = natInternalSize;
        this._adjustAllocationForArrow(true, alloc);
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [minSize, naturalSize] = this.bin.get_preferred_height(forWidth);
        alloc.min_size = minSize;
        alloc.natural_size = naturalSize;
        this._adjustAllocationForArrow(false, alloc);
    },

    _allocate: function(actor, box, flags) {
        let themeNode = this.actor.get_theme_node();
        let found, borderWidth, borderRadius, rise, base;
        [found, borderWidth] = themeNode.get_length('-arrow-border-width', false);
        [found, rise] = themeNode.get_length('-arrow-rise', false);
        let childBox = new Clutter.ActorBox();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        childBox.x1 = 0
        childBox.y1 = 0;
        childBox.x2 = availWidth;
        childBox.y2 = availHeight;
        this._border.allocate(childBox, flags);
        switch (this._arrowSide) {
            case St.Side.TOP:
                childBox.x1 = borderWidth;
                childBox.y1 = rise + borderWidth;
                childBox.x2 = availWidth - borderWidth;
                childBox.y2 = availHeight - borderWidth;
                break;
            default:
                break;
        }
        this.bin.allocate(childBox, flags);
    },

    _drawBorder: function(area) {
        let themeNode = this.actor.get_theme_node();

        let found, borderWidth, borderRadius, rise, base;
        [found, borderWidth] = themeNode.get_length('-arrow-border-width', false);
        [found, base] = themeNode.get_length('-arrow-base', false);
        [found, rise] = themeNode.get_length('-arrow-rise', false);
        [found, borderRadius] = themeNode.get_length('-arrow-border-radius', false);

        let halfBorder = borderWidth / 2;

        let borderColor = new Clutter.Color();
        themeNode.get_color('-arrow-border-color', false, borderColor);
        let backgroundColor = new Clutter.Color();
        themeNode.get_color('-arrow-background-color', false, backgroundColor);

        let [width, height] = area.get_surface_size();
        let [boxWidth, boxHeight] = [width, height];
        if (this._arrowSide == St.Side.TOP || this._arrowSide == St.Side.BOTTOM) {
            boxHeight -= rise;
        } else {
            boxWidth -= rise;
        }
        let cr = area.get_context();
        Clutter.cairo_set_source_color(cr, borderColor);
        if (this._arrowSide == St.Side.TOP) {
            cr.translate(0, rise);
        }
        cr.moveTo(borderRadius, halfBorder);
        if (this._arrowSide == St.Side.TOP) {
            cr.translate(0, -rise);
            let halfBase = Math.floor(base/2);
            cr.lineTo(this._arrowOrigin - halfBase, rise + halfBorder);
            cr.lineTo(this._arrowOrigin, halfBorder);
            cr.lineTo(this._arrowOrigin + halfBase, rise + halfBorder);
            cr.translate(0, rise);
        }
        cr.lineTo(boxWidth - borderRadius, halfBorder);
        cr.arc(boxWidth - borderRadius - halfBorder, borderRadius + halfBorder, borderRadius,
               3*Math.PI/2, Math.PI*2);
        cr.lineTo(boxWidth - halfBorder, boxHeight - borderRadius);
        cr.arc(boxWidth - borderRadius - halfBorder, boxHeight - borderRadius - halfBorder, borderRadius,
               0, Math.PI/2);
        cr.lineTo(borderRadius, boxHeight - halfBorder);
        cr.arc(borderRadius + halfBorder, boxHeight - borderRadius - halfBorder, borderRadius,
               Math.PI/2, Math.PI);
        cr.lineTo(halfBorder, borderRadius);
        cr.arc(borderRadius + halfBorder, borderRadius + halfBorder, borderRadius,
               Math.PI, 3*Math.PI/2);
        Clutter.cairo_set_source_color(cr, backgroundColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, borderColor);
        cr.setLineWidth(borderWidth);
        cr.stroke();
    },

    // @origin: Coordinate specifying middle of the arrow, along
    // the Y axis for St.Side.LEFT, St.Side.RIGHT from the top and X axis from
    // the left for St.Side.TOP and St.Side.BOTTOM.
    setArrowOrigin: function(origin) {
        if (this._arrowOrigin != origin) {
            this._arrowOrigin = origin;
            this._border.queue_repaint();
        }
    }
}
