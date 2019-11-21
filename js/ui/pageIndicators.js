// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PageIndicators, AnimatedPageIndicators */

const { Clutter, GLib, GObject, Meta, St } = imports.gi;
const Cairo = imports.cairo;

const { ANIMATION_TIME_OUT, ANIMATION_MAX_DELAY_OUT_FOR_ITEM, AnimationDirection } = imports.ui.iconGrid;

var INDICATORS_BASE_TIME = 250;
var INDICATORS_BASE_TIME_OUT = 125;
var INDICATORS_ANIMATION_DELAY = 125;
var INDICATORS_ANIMATION_DELAY_OUT = 62.5;
var INDICATORS_ANIMATION_MAX_TIME = 750;
var SWITCH_TIME = 400;
var INDICATORS_ANIMATION_MAX_TIME_OUT =
    Math.min(SWITCH_TIME,
             ANIMATION_TIME_OUT + ANIMATION_MAX_DELAY_OUT_FOR_ITEM);

var ANIMATION_DELAY = 100;

var PageIndicators = GObject.registerClass({
    Signals: { 'page-activated': { param_types: [GObject.TYPE_INT] } },
}, class PageIndicators extends St.BoxLayout {
    _init(orientation = Clutter.Orientation.VERTICAL) {
        let vertical = orientation == Clutter.Orientation.VERTICAL;
        super._init({
            style_class: 'page-indicators',
            vertical,
            x_expand: true, y_expand: true,
            x_align: vertical ? Clutter.ActorAlign.END : Clutter.ActorAlign.CENTER,
            y_align: vertical ? Clutter.ActorAlign.CENTER : Clutter.ActorAlign.END,
            reactive: true,
            clip_to_allocation: true,
        });
        this._nPages = 0;
        this._currentPosition = 0;
        this._reactive = true;
        this._reactive = true;
    }

    vfunc_get_preferred_height(forWidth) {
        // We want to request the natural height of all our children as our
        // natural height, so we chain up to St.BoxLayout, but we only request 0
        // as minimum height, since it's not that important if some indicators
        // are not shown
        let [, natHeight] = super.vfunc_get_preferred_height(forWidth);
        return [0, natHeight];
    }

    setReactive(reactive) {
        let children = this.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].reactive = reactive;

        this._reactive = reactive;
    }

    setNPages(nPages) {
        if (this._nPages == nPages)
            return;

        let diff = nPages - this._nPages;
        if (diff > 0) {
            for (let i = 0; i < diff; i++) {
                let pageIndex = this._nPages + i;
                let indicator = new St.Button({ style_class: 'page-indicator',
                                                button_mask: St.ButtonMask.ONE |
                                                             St.ButtonMask.TWO |
                                                             St.ButtonMask.THREE,
                                                reactive: this._reactive });
                indicator.child = new St.DrawingArea({ style_class: 'page-indicator-icon' });
                indicator.child.connect('repaint', () => {
                    this.drawIndicatorIcon(indicator.child, pageIndex);
                });
                indicator.connect('clicked', () => {
                    this.emit('page-activated', pageIndex);
                });
                this.add_actor(indicator);
            }
        } else {
            let children = this.get_children().splice(diff);
            for (let i = 0; i < children.length; i++)
                children[i].destroy();
        }
        this._nPages = nPages;
        this.visible = this._nPages > 1;
    }

    drawIndicatorIcon(widget, index) {
        let node = widget.get_theme_node();

        let inactiveRadius = node.get_length('-page-indicator-inactive-radius');
        let activeRadius = node.get_length('-page-indicator-active-radius');

        let activeColor = node.get_color('-page-indicator-active-color');
        let inactiveColor = node.get_color('-page-indicator-inactive-color');

        let progress = Math.max(1 - Math.abs(this._currentPosition - index), 0);
        let radius = inactiveRadius + (activeRadius - inactiveRadius) * progress;
        let color = inactiveColor.interpolate(activeColor, progress);

        let cr = widget.get_context();
        cr.setOperator(Cairo.Operator.SOURCE);

        cr.arc(widget.width / 2, widget.height / 2, radius, 0, Math.PI * 2);

        Clutter.cairo_set_source_color(cr, color);
        cr.fill();

        cr.$dispose();
    }

    setCurrentPosition(currentPosition) {
        this._currentPosition = currentPosition;

        let children = this.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].child.queue_repaint();
    }
});

var AnimatedPageIndicators = GObject.registerClass(
class AnimatedPageIndicators extends PageIndicators {
    _init() {
        super._init();
        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this.animateLater) {
            Meta.later_remove(this.animateLater);
            this.animateLater = 0;
        }
    }

    vfunc_map() {
        super.vfunc_map();

        // Implicit animations are skipped for unmapped actors, and our
        // children aren't mapped yet, so defer to a later handler
        this.animateLater = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this.animateLater = 0;
            this.animateIndicators(AnimationDirection.IN);
            return GLib.SOURCE_REMOVE;
        });
    }

    animateIndicators(animationDirection) {
        if (!this.mapped)
            return;

        let children = this.get_children();
        if (children.length == 0)
            return;

        for (let i = 0; i < this._nPages; i++)
            children[i].remove_all_transitions();

        let offset;
        if (this.get_text_direction() == Clutter.TextDirection.RTL)
            offset = -children[0].width;
        else
            offset = children[0].width;

        let isAnimationIn = animationDirection == AnimationDirection.IN;
        let delay = isAnimationIn
            ? INDICATORS_ANIMATION_DELAY
            : INDICATORS_ANIMATION_DELAY_OUT;
        let baseTime = isAnimationIn ? INDICATORS_BASE_TIME : INDICATORS_BASE_TIME_OUT;
        let totalAnimationTime = baseTime + delay * this._nPages;
        let maxTime = isAnimationIn
            ? INDICATORS_ANIMATION_MAX_TIME
            : INDICATORS_ANIMATION_MAX_TIME_OUT;
        if (totalAnimationTime > maxTime)
            delay -= (totalAnimationTime - maxTime) / this._nPages;

        for (let i = 0; i < this._nPages; i++) {
            children[i].translation_x = isAnimationIn ? offset : 0;
            children[i].ease({
                translation_x: isAnimationIn ? 0 : offset,
                duration: baseTime + delay * i,
                mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                delay: isAnimationIn ? ANIMATION_DELAY : 0,
            });
        }
    }
});
