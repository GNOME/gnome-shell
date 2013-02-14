// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;

const SIDE_CONTROLS_ANIMATION_TIME = 0.2;

const SlideDirection = {
    LEFT: 0,
    RIGHT: 1
};

const SlideLayout = new Lang.Class({
    Name: 'SlideLayout',
    Extends: Clutter.FixedLayout,

    _init: function(params) {
        this._slideX = 1;
        this._direction = SlideDirection.LEFT;

        this.parent(params);
    },

    _getRealSlideDirection: function(child) {
        let direction = this._direction;

        let rtl = (child.text_direction == Clutter.TextDirection.RTL);
        if (rtl)
            direction = (direction == SlideDirection.LEFT) ?
                SlideDirection.RIGHT : SlideDirection.LEFT;

        return direction;
    },

    vfunc_get_preferred_width: function(container, forHeight) {
        let child = container.get_first_child();

        let [minWidth, natWidth] = child.get_preferred_width(forHeight);

        minWidth *= this._slideX;
        natWidth *= this._slideX;

        return [minWidth, natWidth];
    },

    vfunc_allocate: function(container, box, flags) {
        let child = container.get_first_child();

        let [, , natWidth, natHeight] = child.get_preferred_size();
        let availWidth = Math.round(box.x2 - box.x1);
        let availHeight = Math.round(box.y2 - box.y1);

        let realDirection = this._getRealSlideDirection(child);
        let translationX = (realDirection == SlideDirection.LEFT) ?
            (availWidth - natWidth) : (natWidth - availWidth);

        let actorBox = new Clutter.ActorBox({ x1: translationX,
                                              y1: 0,
                                              x2: child.x_expand ? availWidth : natWidth,
                                              y2: child.y_expand ? availHeight : natHeight });

        child.allocate(actorBox, flags);
    },

    set slideX(value) {
        this._slideX = value;
        this.layout_changed();
    },

    get slideX() {
        return this._slideX;
    },

    set slideDirection(direction) {
        this._direction = direction;
        this.layout_changed();
    },

    get slideDirection() {
        return this._direction;
    }
});

const SlidingControl = new Lang.Class({
    Name: 'SlidingControl',

    _init: function() {
        this.visible = true;
        this.inDrag = false;

        this.layout = new SlideLayout();
        this.actor = new St.Widget({ layout_manager: this.layout,
                                     clip_to_allocation: true });

        Main.overview.connect('showing', Lang.bind(this, this._onOverviewShowing));

        Main.overview.connect('item-drag-begin', Lang.bind(this, this._onDragBegin));
        Main.overview.connect('item-drag-end', Lang.bind(this, this._onDragEnd));
        Main.overview.connect('item-drag-cancelled', Lang.bind(this, this._onDragEnd));
        Main.overview.connect('window-drag-begin', Lang.bind(this, this._onDragBegin));
        Main.overview.connect('window-drag-cancelled', Lang.bind(this, this._onDragEnd));
        Main.overview.connect('window-drag-end', Lang.bind(this, this._onDragEnd));
    },

    getSlide: function() {
        throw new Error('getSlide() must be overridden');
    },

    updateSlide: function() {
        Tweener.addTween(this.layout, { slideX: this.getSlide(),
                                        time: SIDE_CONTROLS_ANIMATION_TIME,
                                        transition: 'easeOutQuad' });
    },

    _onOverviewShowing: function() {
        // reset any translation and make sure the actor is visible when
        // entering the overview
        this.visible = true;
        this.layout.slideX = this.getSlide();
    },

    _onDragBegin: function() {
        this.inDrag = true;
        this.updateSlide();
    },

    _onDragEnd: function() {
        this.inDrag = false;
        this.updateSlide();
    },

    slideIn: function() {
        this.visible = true;
        this.updateSlide();
    },

    slideOut: function() {
        this.visible = false;
        this.updateSlide();
    }
});
