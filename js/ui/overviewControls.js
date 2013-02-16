// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const ViewSelector = imports.ui.viewSelector;

const SIDE_CONTROLS_ANIMATION_TIME = 0.2;

function getRtlSlideDirection(direction, actor) {
    let rtl = (actor.text_direction == Clutter.TextDirection.RTL);
    if (rtl)
        direction = (direction == SlideDirection.LEFT) ?
            SlideDirection.RIGHT : SlideDirection.LEFT;

    return direction;
};

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

        let realDirection = getRtlSlideDirection(this._direction, child);
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

const ThumbnailsSlider = new Lang.Class({
    Name: 'ThumbnailsSlider',
    Extends: SlidingControl,

    _init: function(thumbnailsBox) {
        this.parent();

        this.layout.slideDirection = SlideDirection.RIGHT;

        this._thumbnailsBox = thumbnailsBox;

        // SlideLayout reads the actor's expand flags to decide
        // whether to allocate the natural size to its child, or the whole
        // available allocation
        this._thumbnailsBox.actor.y_expand = true;

        this.actor.request_mode = Clutter.RequestMode.WIDTH_FOR_HEIGHT;
        this.actor.reactive = true;
        this.actor.track_hover = true;
        this.actor.add_actor(this._thumbnailsBox.actor);

        Main.layoutManager.connect('monitors-changed', Lang.bind(this, this.updateSlide));
        this.actor.connect('notify::hover', Lang.bind(this, this.updateSlide));
    },

    getSlide: function() {
        if (!this.visible)
            return 0;

        // Always show the pager when hover, during a drag, or if workspaces are
        // actually used, e.g. there are windows on more than one
        let alwaysZoomOut = this.actor.hover || this.inDrag || global.screen.n_workspaces > 2;

        if (!alwaysZoomOut) {
            let monitors = Main.layoutManager.monitors;
            let primary = Main.layoutManager.primaryMonitor;

            /* Look for any monitor to the right of the primary, if there is
             * one, we always keep zoom out, otherwise its hard to reach
             * the thumbnail area without passing into the next monitor. */
            for (let i = 0; i < monitors.length; i++) {
                if (monitors[i].x >= primary.x + primary.width) {
                    alwaysZoomOut = true;
                    break;
                }
            }
        }

        if (alwaysZoomOut)
            return 1;

        let child = this.actor.get_first_child();
        let preferredHeight = child.get_preferred_height(-1)[1];
        let expandedWidth = child.get_preferred_width(preferredHeight)[1];
        let visibleWidth = child.get_theme_node().get_length('visible-width');

        return visibleWidth / expandedWidth;
    }
});

const DashSlider = new Lang.Class({
    Name: 'DashSlider',
    Extends: SlidingControl,

    _init: function(dash) {
        this.parent();

        this.layout.slideDirection = SlideDirection.LEFT;

        this._dash = dash;

        // SlideLayout reads the actor's expand flags to decide
        // whether to allocate the natural size to its child, or the whole
        // available allocation
        this._dash.actor.x_expand = true;
        this._dash.actor.y_expand = true;
        this.actor.add_actor(this._dash.actor);

        this._dash.connect('icon-size-changed', Lang.bind(this, this.updateSlide));
    },

    getSlide: function() {
        if (this.visible || this.inDrag)
            return 1;
        else
            return 0;
    }
});

const ControlsManager = new Lang.Class({
    Name: 'ControlsManager',

    _init: function(dash, thumbnails, viewSelector) {
        this._dashSlider = new DashSlider(dash);
        this.dashActor = this._dashSlider.actor;

        this._thumbnailsSlider = new ThumbnailsSlider(thumbnails);
        this.thumbnailsActor = this._thumbnailsSlider.actor;

        this._viewSelector = viewSelector;
        this._viewSelector.connect('page-changed', Lang.bind(this, this._setVisibility));
    },

    _setVisibility: function() {
        // Ignore the case when we're leaving the overview, since
        // actors will be made visible again when entering the overview
        // next time, and animating them while doing so is just
        // unnecessary noise
        if (!Main.overview.visible)
            return;

        let activePage = this._viewSelector.getActivePage();
        let dashVisible = (activePage == ViewSelector.ViewPage.WINDOWS ||
                           activePage == ViewSelector.ViewPage.APPS);
        let thumbnailsVisible = (activePage == ViewSelector.ViewPage.WINDOWS);

        if (dashVisible)
            this._dashSlider.slideIn();
        else
            this._dashSlider.slideOut();

        if (thumbnailsVisible)
            this._thumbnailsSlider.slideIn();
        else
            this._thumbnailsSlider.slideOut();
    }
});
