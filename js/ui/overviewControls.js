// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GObject = imports.gi.GObject;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Dash = imports.ui.dash;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const ViewSelector = imports.ui.viewSelector;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

var SIDE_CONTROLS_ANIMATION_TIME = 0.16;

function getRtlSlideDirection(direction, actor) {
    let rtl = (actor.text_direction == Clutter.TextDirection.RTL);
    if (rtl)
        direction = (direction == SlideDirection.LEFT) ?
            SlideDirection.RIGHT : SlideDirection.LEFT;

    return direction;
};

var SlideDirection = {
    LEFT: 0,
    RIGHT: 1
};

var SlideLayout = new Lang.Class({
    Name: 'SlideLayout',
    Extends: Clutter.FixedLayout,

    _init(params) {
        this._slideX = 1;
        this._translationX = undefined;
        this._direction = SlideDirection.LEFT;

        this.parent(params);
    },

    vfunc_get_preferred_width(container, forHeight) {
        let child = container.get_first_child();

        let [minWidth, natWidth] = child.get_preferred_width(forHeight);

        minWidth *= this._slideX;
        natWidth *= this._slideX;

        return [minWidth, natWidth];
    },

    vfunc_allocate(container, box, flags) {
        let child = container.get_first_child();

        let availWidth = Math.round(box.x2 - box.x1);
        let availHeight = Math.round(box.y2 - box.y1);
        let [, natWidth] = child.get_preferred_width(availHeight);

        // Align the actor inside the clipped box, as the actor's alignment
        // flags only determine what to do if the allocated box is bigger
        // than the actor's box.
        let realDirection = getRtlSlideDirection(this._direction, child);
        let alignX = (realDirection == SlideDirection.LEFT) ? (availWidth - natWidth)
                                                            : (availWidth - natWidth * this._slideX);

        let actorBox = new Clutter.ActorBox();
        actorBox.x1 = box.x1 + alignX + this._translationX;
        actorBox.x2 = actorBox.x1 + (child.x_expand ? availWidth : natWidth);
        actorBox.y1 = box.y1;
        actorBox.y2 = actorBox.y1 + availHeight;

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
    },

    set translationX(value) {
        this._translationX = value;
        this.layout_changed();
    },

    get translationX() {
        return this._translationX;
    },
});

var SlidingControl = new Lang.Class({
    Name: 'SlidingControl',

    _init(params) {
        params = Params.parse(params, { slideDirection: SlideDirection.LEFT });

        this._visible = true;
        this._inDrag = false;

        this.layout = new SlideLayout();
        this.layout.slideDirection = params.slideDirection;
        this.actor = new St.Widget({ layout_manager: this.layout,
                                     style_class: 'overview-controls',
                                     clip_to_allocation: true });

        Main.overview.connect('hiding', this._onOverviewHiding.bind(this));

        Main.overview.connect('item-drag-begin', this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-cancelled', this._onDragEnd.bind(this));

        Main.overview.connect('window-drag-begin', this._onWindowDragBegin.bind(this));
        Main.overview.connect('window-drag-cancelled', this._onWindowDragEnd.bind(this));
        Main.overview.connect('window-drag-end', this._onWindowDragEnd.bind(this));
    },

    _getSlide() {
        throw new Error('getSlide() must be overridden');
    },

    _updateSlide() {
        Tweener.addTween(this.layout, { slideX: this._getSlide(),
                                        time: SIDE_CONTROLS_ANIMATION_TIME,
                                        transition: 'easeOutQuad' });
    },

    getVisibleWidth() {
        let child = this.actor.get_first_child();
        let [, , natWidth, ] = child.get_preferred_size();
        return natWidth;
    },

    _getTranslation() {
        let child = this.actor.get_first_child();
        let direction = getRtlSlideDirection(this.layout.slideDirection, child);
        let visibleWidth = this.getVisibleWidth();

        if (direction == SlideDirection.LEFT)
            return - visibleWidth;
        else
            return visibleWidth;
    },

    _updateTranslation() {
        let translationStart = 0;
        let translationEnd = 0;
        let translation = this._getTranslation();

        let shouldShow = (this._getSlide() > 0);
        if (shouldShow) {
            translationStart = translation;
        } else {
            translationEnd = translation;
        }

        if (this.layout.translationX == translationEnd)
            return;

        this.layout.translationX = translationStart;
        Tweener.addTween(this.layout, { translationX: translationEnd,
                                        time: SIDE_CONTROLS_ANIMATION_TIME,
                                        transition: 'easeOutQuad' });
    },

    _onOverviewHiding() {
        // We need to explicitly slideOut since showing pages
        // doesn't imply sliding out, instead, hiding the overview does.
        this.slideOut();
    },

    _onWindowDragBegin() {
        this._onDragBegin();
    },

    _onWindowDragEnd() {
        this._onDragEnd();
    },

    _onDragBegin() {
        this._inDrag = true;
        this._updateTranslation();
        this._updateSlide();
    },

    _onDragEnd() {
        this._inDrag = false;
        this._updateSlide();
    },

    fadeIn() {
        Tweener.addTween(this.actor, { opacity: 255,
                                       time: SIDE_CONTROLS_ANIMATION_TIME / 2,
                                       transition: 'easeInQuad'
                                     });
    },

    fadeHalf() {
        Tweener.addTween(this.actor, { opacity: 128,
                                       time: SIDE_CONTROLS_ANIMATION_TIME / 2,
                                       transition: 'easeOutQuad'
                                     });
    },

    slideIn() {
        this._visible = true;
        // we will update slideX and the translation from pageEmpty
    },

    slideOut() {
        this._visible = false;
        this._updateTranslation();
        // we will update slideX from pageEmpty
    },

    pageEmpty() {
        // When pageEmpty is received, there's no visible view in the
        // selector; this means we can now safely set the full slide for
        // the next page, since slideIn or slideOut might have been called,
        // changing the visiblity
        this.layout.slideX = this._getSlide();
        this._updateTranslation();
    }
});

var ThumbnailsSlider = new Lang.Class({
    Name: 'ThumbnailsSlider',
    Extends: SlidingControl,

    _init(thumbnailsBox) {
        this.parent({ slideDirection: SlideDirection.RIGHT });

        this._thumbnailsBox = thumbnailsBox;

        this.actor.request_mode = Clutter.RequestMode.WIDTH_FOR_HEIGHT;
        this.actor.reactive = true;
        this.actor.track_hover = true;
        this.actor.add_actor(this._thumbnailsBox.actor);

        Main.layoutManager.connect('monitors-changed', this._updateSlide.bind(this));
        this.actor.connect('notify::hover', this._updateSlide.bind(this));
        this._thumbnailsBox.actor.bind_property('visible', this.actor, 'visible', GObject.BindingFlags.SYNC_CREATE);
    },

    _getAlwaysZoomOut() {
        // Always show the pager on hover or during a drag
        let alwaysZoomOut = this.actor.hover || this._inDrag;

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

        return alwaysZoomOut;
    },

    getNonExpandedWidth() {
        let child = this.actor.get_first_child();
        return child.get_theme_node().get_length('visible-width');
    },

    _onDragEnd() {
        this.actor.sync_hover();
        this.parent();
    },

    _getSlide() {
        if (!this._visible)
            return 0;

        let alwaysZoomOut = this._getAlwaysZoomOut();
        if (alwaysZoomOut)
            return 1;

        let child = this.actor.get_first_child();
        let preferredHeight = child.get_preferred_height(-1)[1];
        let expandedWidth = child.get_preferred_width(preferredHeight)[1];

        return this.getNonExpandedWidth() / expandedWidth;
    },

    getVisibleWidth() {
        let alwaysZoomOut = this._getAlwaysZoomOut();
        if (alwaysZoomOut)
            return this.parent();
        else
            return this.getNonExpandedWidth();
    }
});

var DashSlider = new Lang.Class({
    Name: 'DashSlider',
    Extends: SlidingControl,

    _init(dash) {
        this.parent({ slideDirection: SlideDirection.LEFT });

        this._dash = dash;

        // SlideLayout reads the actor's expand flags to decide
        // whether to allocate the natural size to its child, or the whole
        // available allocation
        this._dash.actor.x_expand = true;

        this.actor.x_expand = true;
        this.actor.x_align = Clutter.ActorAlign.START;
        this.actor.y_expand = true;

        this.actor.add_actor(this._dash.actor);

        this._dash.connect('icon-size-changed', this._updateSlide.bind(this));
    },

    _getSlide() {
        if (this._visible || this._inDrag)
            return 1;
        else
            return 0;
    },

    _onWindowDragBegin() {
        this.fadeHalf();
    },

    _onWindowDragEnd() {
        this.fadeIn();
    }
});

var DashSpacer = new Lang.Class({
    Name: 'DashSpacer',
    Extends: St.Widget,

    _init(params) {
        this.parent(params);

        this._bindConstraint = null;
    },

    setDashActor(dashActor) {
        if (this._bindConstraint) {
            this.remove_constraint(this._bindConstraint);
            this._bindConstraint = null;
        }

        if (dashActor) {
            this._bindConstraint = new Clutter.BindConstraint({ source: dashActor,
                                                                coordinate: Clutter.BindCoordinate.SIZE });
            this.add_constraint(this._bindConstraint);
        }
    },

    vfunc_get_preferred_width(forHeight) {
        let box = this.get_allocation_box();
        let minWidth = this.parent(forHeight)[0];
        let natWidth = box.x2 - box.x1;
        return [minWidth, natWidth];
    },

    vfunc_get_preferred_height(forWidth) {
        let box = this.get_allocation_box();
        let minHeight = this.parent(forWidth)[0];
        let natHeight = box.y2 - box.y1;
        return [minHeight, natHeight];
    }
});

var ControlsLayout = new Lang.Class({
    Name: 'ControlsLayout',
    Extends: Clutter.BinLayout,
    Signals: { 'allocation-changed': { flags: GObject.SignalFlags.RUN_LAST } },

    vfunc_allocate(container, box, flags) {
        this.parent(container, box, flags);
        this.emit('allocation-changed');
    }
});

var ControlsManager = new Lang.Class({
    Name: 'ControlsManager',

    _init(searchEntry) {
        this.dash = new Dash.Dash();
        this._dashSlider = new DashSlider(this.dash);
        this._dashSpacer = new DashSpacer();
        this._dashSpacer.setDashActor(this._dashSlider.actor);

        this._thumbnailsBox = new WorkspaceThumbnail.ThumbnailsBox();
        this._thumbnailsSlider = new ThumbnailsSlider(this._thumbnailsBox);

        this.viewSelector = new ViewSelector.ViewSelector(searchEntry,
                                                          this.dash.showAppsButton);
        this.viewSelector.connect('page-changed', this._setVisibility.bind(this));
        this.viewSelector.connect('page-empty', this._onPageEmpty.bind(this));

        let layout = new ControlsLayout();
        this.actor = new St.Widget({ layout_manager: layout,
                                     x_expand: true, y_expand: true,
                                     clip_to_allocation: true });
        this._group = new St.BoxLayout({ name: 'overview-group',
                                        x_expand: true, y_expand: true });
        this.actor.add_actor(this._group);

        this.actor.add_actor(this._dashSlider.actor);

        this._group.add_actor(this._dashSpacer);
        this._group.add(this.viewSelector.actor, { x_fill: true,
                                                   expand: true });
        this._group.add_actor(this._thumbnailsSlider.actor);

        layout.connect('allocation-changed', this._updateWorkspacesGeometry.bind(this));

        Main.overview.connect('showing', this._updateSpacerVisibility.bind(this));
        Main.overview.connect('item-drag-begin', () => {
            let activePage = this.viewSelector.getActivePage();
            if (activePage != ViewSelector.ViewPage.WINDOWS)
                this.viewSelector.fadeHalf();
        });
        Main.overview.connect('item-drag-end', () => {
            this.viewSelector.fadeIn();
        });
        Main.overview.connect('item-drag-cancelled', () => {
            this.viewSelector.fadeIn();
        });
    },

    _updateWorkspacesGeometry() {
        let [x, y] = this.actor.get_transformed_position();
        let [width, height] = this.actor.get_transformed_size();
        let geometry = { x: x, y: y, width: width, height: height };

        let spacing = this.actor.get_theme_node().get_length('spacing');
        let dashWidth = this._dashSlider.getVisibleWidth() + spacing;
        let thumbnailsWidth = this._thumbnailsSlider.getNonExpandedWidth() + spacing;

        geometry.width -= dashWidth;
        geometry.width -= thumbnailsWidth;

        if (this.actor.get_text_direction() == Clutter.TextDirection.LTR)
            geometry.x += dashWidth;
        else
            geometry.x += thumbnailsWidth;

        this.viewSelector.setWorkspacesFullGeometry(geometry);
    },

    _setVisibility() {
        // Ignore the case when we're leaving the overview, since
        // actors will be made visible again when entering the overview
        // next time, and animating them while doing so is just
        // unnecessary noise
        if (!Main.overview.visible ||
            (Main.overview.animationInProgress && !Main.overview.visibleTarget))
            return;

        let activePage = this.viewSelector.getActivePage();
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
    },

    _updateSpacerVisibility() {
        if (Main.overview.animationInProgress && !Main.overview.visibleTarget)
            return;

        let activePage = this.viewSelector.getActivePage();
        this._dashSpacer.visible = (activePage == ViewSelector.ViewPage.WINDOWS);
    },

    _onPageEmpty() {
        this._dashSlider.pageEmpty();
        this._thumbnailsSlider.pageEmpty();

        this._updateSpacerVisibility();
    }
});
