/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Tweener = imports.ui.tweener;

const POPUP_ANIMATION_TIME = 0.15;

/**
 * BoxPointer:
 * @side: side to draw the arrow on
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

    show: function(animate, onComplete) {
        let x = this.actor.x;
        let y = this.actor.y;
        let themeNode = this.actor.get_theme_node();
        let rise = themeNode.get_length('-arrow-rise');

        this.actor.opacity = 0;
        this.actor.show();

        if (animate) {
            switch (this._arrowSide) {
                case St.Side.TOP:
                    this.actor.y -= rise;
                    break;
                case St.Side.BOTTOM:
                    this.actor.y += rise;
                    break;
                case St.Side.LEFT:
                    this.actor.x -= rise;
                    break;
                case St.Side.RIGHT:
                    this.actor.x += rise;
                    break;
            }
        }

        Tweener.addTween(this.actor, { opacity: 255,
                                       x: x,
                                       y: y,
                                       transition: "linear",
                                       onComplete: onComplete,
                                       time: POPUP_ANIMATION_TIME });
    },

    hide: function(animate, onComplete) {
        let x = this.actor.x;
        let y = this.actor.y;
        let originalX = this.actor.x;
        let originalY = this.actor.y;
        let themeNode = this.actor.get_theme_node();
        let rise = themeNode.get_length('-arrow-rise');

        if (animate) {
            switch (this._arrowSide) {
                case St.Side.TOP:
                    y += rise;
                    break;
                case St.Side.BOTTOM:
                    y -= rise;
                    break;
                case St.Side.LEFT:
                    x += rise;
                    break;
                case St.Side.RIGHT:
                    x -= rise;
                    break;
            }
        }

        Tweener.addTween(this.actor, { opacity: 0,
                                       x: x,
                                       y: y,
                                       transition: "linear",
                                       time: POPUP_ANIMATION_TIME,
                                       onComplete: Lang.bind(this, function () {
                                           this.actor.hide();
                                           this.actor.x = originalX;
                                           this.actor.y = originalY;
                                           if (onComplete)
                                               onComplete();
                                       })
                                     });
    },

    _adjustAllocationForArrow: function(isWidth, alloc) {
        let themeNode = this.actor.get_theme_node();
        let borderWidth = themeNode.get_length('-arrow-border-width');
        alloc.min_size += borderWidth * 2;
        alloc.natural_size += borderWidth * 2;
        if ((!isWidth && (this._arrowSide == St.Side.TOP || this._arrowSide == St.Side.BOTTOM))
            || (isWidth && (this._arrowSide == St.Side.LEFT || this._arrowSide == St.Side.RIGHT))) {
            let rise = themeNode.get_length('-arrow-rise');
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
        let borderWidth = themeNode.get_length('-arrow-border-width');
        let rise = themeNode.get_length('-arrow-rise');
        let childBox = new Clutter.ActorBox();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        childBox.x1 = 0;
        childBox.y1 = 0;
        childBox.x2 = availWidth;
        childBox.y2 = availHeight;
        this._border.allocate(childBox, flags);

        childBox.x1 = borderWidth;
        childBox.y1 = borderWidth;
        childBox.x2 = availWidth - borderWidth;
        childBox.y2 = availHeight - borderWidth;
        switch (this._arrowSide) {
            case St.Side.TOP:
                childBox.y1 += rise;
                break;
            case St.Side.BOTTOM:
                childBox.y2 -= rise;
                break;
            case St.Side.LEFT:
                childBox.x1 += rise;
                break;
            case St.Side.RIGHT:
                childBox.x2 -= rise;
                break;
        }
        this.bin.allocate(childBox, flags);
    },

    _drawBorder: function(area) {
        let themeNode = this.actor.get_theme_node();

        let borderWidth = themeNode.get_length('-arrow-border-width');
        let base = themeNode.get_length('-arrow-base');
        let rise = themeNode.get_length('-arrow-rise');
        let borderRadius = themeNode.get_length('-arrow-border-radius');

        let halfBorder = borderWidth / 2;
        let halfBase = Math.floor(base/2);

        let borderColor = new Clutter.Color();
        themeNode.get_color('-arrow-border-color', borderColor);
        let backgroundColor = new Clutter.Color();
        themeNode.get_color('-arrow-background-color', backgroundColor);

        let [width, height] = area.get_surface_size();
        let [boxWidth, boxHeight] = [width, height];
        if (this._arrowSide == St.Side.TOP || this._arrowSide == St.Side.BOTTOM) {
            boxHeight -= rise;
        } else {
            boxWidth -= rise;
        }
        let cr = area.get_context();
        Clutter.cairo_set_source_color(cr, borderColor);

        // Translate so that box goes from 0,0 to boxWidth,boxHeight,
        // with the arrow poking out of that
        if (this._arrowSide == St.Side.TOP) {
            cr.translate(0, rise);
        } else if (this._arrowSide == St.Side.LEFT) {
            cr.translate(rise, 0);
        }

        cr.moveTo(borderRadius, halfBorder);

        if (this._arrowSide == St.Side.TOP) {
            cr.lineTo(this._arrowOrigin - halfBase, halfBorder);
            cr.lineTo(this._arrowOrigin, halfBorder - rise);
            cr.lineTo(this._arrowOrigin + halfBase, halfBorder);
        }
        cr.lineTo(boxWidth - borderRadius, halfBorder);

        cr.arc(boxWidth - borderRadius - halfBorder, borderRadius + halfBorder, borderRadius,
               3*Math.PI/2, Math.PI*2);

        if (this._arrowSide == St.Side.RIGHT) {
            cr.lineTo(boxWidth - halfBorder, this._arrowOrigin - halfBase);
            cr.lineTo(boxWidth - halfBorder + rise, this._arrowOrigin);
            cr.lineTo(boxWidth - halfBorder, this._arrowOrigin + halfBase);
        }
        cr.lineTo(boxWidth - halfBorder, boxHeight - borderRadius);

        cr.arc(boxWidth - borderRadius - halfBorder, boxHeight - borderRadius - halfBorder, borderRadius,
               0, Math.PI/2);

        if (this._arrowSide == St.Side.BOTTOM) {
            cr.lineTo(this._arrowOrigin + halfBase, boxHeight - halfBorder);
            cr.lineTo(this._arrowOrigin, boxHeight - halfBorder + rise);
            cr.lineTo(this._arrowOrigin - halfBase, boxHeight - halfBorder);
        }
        cr.lineTo(borderRadius, boxHeight - halfBorder);

        cr.arc(borderRadius + halfBorder, boxHeight - borderRadius - halfBorder, borderRadius,
               Math.PI/2, Math.PI);

        if (this._arrowSide == St.Side.LEFT) {
            cr.lineTo(halfBorder, this._arrowOrigin + halfBase);
            cr.lineTo(halfBorder - rise, this._arrowOrigin);
            cr.lineTo(halfBorder, this._arrowOrigin - halfBase);
        }
        cr.lineTo(halfBorder, borderRadius);

        cr.arc(borderRadius + halfBorder, borderRadius + halfBorder, borderRadius,
               Math.PI, 3*Math.PI/2);

        Clutter.cairo_set_source_color(cr, backgroundColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, borderColor);
        cr.setLineWidth(borderWidth);
        cr.stroke();
    },

    setPosition: function(sourceActor, gap, alignment) {
        // We need to show it now to force an allocation,
        // so that we can query the correct size.
        this.actor.show();

        // Position correctly relative to the sourceActor
        let [sourceX, sourceY] = sourceActor.get_transformed_position();
        let [sourceWidth, sourceHeight] = sourceActor.get_transformed_size();

        let [minWidth, minHeight, natWidth, natHeight] = this.actor.get_preferred_size();

        // We also want to keep it onscreen, and separated from the
        // edge by the same distance as the main part of the box is
        // separated from its sourceActor
        let primary = global.get_primary_monitor();
        let themeNode = this.actor.get_theme_node();
        let arrowRise = themeNode.get_length('-arrow-rise');
        let borderRadius = themeNode.get_length('-arrow-border-radius');

        let resX, resY;

        switch (this._arrowSide) {
        case St.Side.TOP:
            resY = sourceY + sourceHeight + gap;
            break;
        case St.Side.BOTTOM:
            resY = sourceY - natHeight - gap;
            break;
        case St.Side.LEFT:
            resX = sourceX + sourceWidth + gap;
            break;
        case St.Side.RIGHT:
            resX = sourceX - natWidth - gap;
            break;
        }

        // Now align and position the pointing axis, making sure
        // it fits on screen
        switch (this._arrowSide) {
        case St.Side.TOP:
        case St.Side.BOTTOM:
            switch (alignment) {
            case St.Align.START:
                resX = sourceX - 2 * borderRadius;
                break;
            case St.Align.MIDDLE:
                resX = sourceX - Math.floor((natWidth - sourceWidth) / 2);
                break;
            case St.Align.END:
                resX = sourceX - (natWidth - sourceWidth) + 2 * borderRadius;
                break;
            }

            resX = Math.min(resX, primary.x + primary.width - natWidth - arrowRise - gap);
            resX = Math.max(resX, primary.x);

            this.setArrowOrigin((sourceX - resX) + Math.floor(sourceWidth / 2));
            break;

        case St.Side.LEFT:
        case St.Side.RIGHT:
            switch (alignment) {
            case St.Align.START:
                resY = sourceY - 2 * borderRadius;
                break;
            case St.Align.MIDDLE:
                resY = sourceY - Math.floor((natHeight - sourceHeight) / 2);
                break;
            case St.Align.END:
                resY = sourceY - (natHeight - sourceHeight) + 2 * borderRadius;
                break;
            }

            resY = Math.min(resY, primary.y + primary.height - natHeight - arrowRise - gap);
            resY = Math.max(resY, primary.y);

            this.setArrowOrigin((sourceY - resY) + Math.floor(sourceHeight / 2));
            break;
        }

        let parent = this.actor.get_parent();
        let success, x, y;
        while (!success) {
            [success, x, y] = parent.transform_stage_point(resX, resY);
            parent = parent.get_parent();
        }

        // Actually set the position
        this.actor.x = Math.floor(x);
        this.actor.y = Math.floor(y);
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
};
