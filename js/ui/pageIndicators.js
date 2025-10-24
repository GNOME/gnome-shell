import Clutter from 'gi://Clutter';
import Graphene from 'gi://Graphene';
import GObject from 'gi://GObject';
import St from 'gi://St';

const INDICATOR_INACTIVE_OPACITY = 128;
const INDICATOR_INACTIVE_OPACITY_HOVER = 255;
const INDICATOR_INACTIVE_SCALE = 2 / 3;
const INDICATOR_INACTIVE_SCALE_PRESSED = 0.5;

export const PageIndicators = GObject.registerClass({
    Signals: {'page-activated': {param_types: [GObject.TYPE_INT]}},
}, class PageIndicators extends St.BoxLayout {
    _init(orientation = Clutter.Orientation.VERTICAL) {
        const vertical = orientation === Clutter.Orientation.VERTICAL;
        super._init({
            style_class: 'page-indicators',
            orientation,
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
        this._orientation = orientation;
    }

    vfunc_get_preferred_height(forWidth) {
        // We want to request the natural height of all our children as our
        // natural height, so we chain up to St.BoxLayout, but we only request 0
        // as minimum height, since it's not that important if some indicators
        // are not shown
        const [, natHeight] = super.vfunc_get_preferred_height(forWidth);
        return [0, natHeight];
    }

    setReactive(reactive) {
        const children = this.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].reactive = reactive;

        this._reactive = reactive;
    }

    setNPages(nPages) {
        if (this._nPages === nPages)
            return;

        const diff = nPages - this._nPages;
        if (diff > 0) {
            for (let i = 0; i < diff; i++) {
                const pageIndex = this._nPages + i;
                const indicator = new St.Button({
                    style_class: 'page-indicator',
                    button_mask: St.ButtonMask.ONE |
                                 St.ButtonMask.TWO |
                                 St.ButtonMask.THREE,
                    reactive: this._reactive,
                    child: new St.Widget({
                        style_class: 'page-indicator-icon',
                        pivot_point: new Graphene.Point({x: 0.5, y: 0.5}),
                    }),
                });
                indicator.connect('clicked', () => {
                    this.emit('page-activated', pageIndex);
                });
                indicator.connect('notify::hover', () => {
                    this._updateIndicator(indicator, pageIndex);
                });
                indicator.connect('notify::pressed', () => {
                    this._updateIndicator(indicator, pageIndex);
                });
                this._updateIndicator(indicator, pageIndex);
                this.add_child(indicator);
            }
        } else {
            const children = this.get_children().splice(diff);
            for (let i = 0; i < children.length; i++)
                children[i].destroy();
        }
        this._nPages = nPages;
        this.visible = this._nPages > 1;
    }

    _updateIndicator(indicator, pageIndex) {
        const progress =
            Math.max(1 - Math.abs(this._currentPosition - pageIndex), 0);

        const inactiveScale = indicator.pressed
            ? INDICATOR_INACTIVE_SCALE_PRESSED : INDICATOR_INACTIVE_SCALE;
        const inactiveOpacity = indicator.hover
            ? INDICATOR_INACTIVE_OPACITY_HOVER : INDICATOR_INACTIVE_OPACITY;

        const scale = inactiveScale + (1 - inactiveScale) * progress;
        const opacity = inactiveOpacity + (255 - inactiveOpacity) * progress;

        indicator.child.set_scale(scale, scale);
        indicator.child.opacity = opacity;
    }

    setCurrentPosition(currentPosition) {
        this._currentPosition = currentPosition;

        const children = this.get_children();
        for (let i = 0; i < children.length; i++)
            this._updateIndicator(children[i], i);
    }

    get nPages() {
        return this._nPages;
    }
});
