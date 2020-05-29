// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ControlsManager */

const { Clutter, GObject, Meta, St } = imports.gi;

const Dash = imports.ui.dash;
const Main = imports.ui.main;
const Params = imports.misc.params;
const ViewSelector = imports.ui.viewSelector;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;
const Overview = imports.ui.overview;

var SIDE_CONTROLS_ANIMATION_TIME = Overview.ANIMATION_TIME;

function getRtlSlideDirection(direction, actor) {
    let rtl = actor.text_direction == Clutter.TextDirection.RTL;
    if (rtl) {
        direction = direction == SlideDirection.LEFT
            ? SlideDirection.RIGHT : SlideDirection.LEFT;
    }
    return direction;
}

var SlideDirection = {
    LEFT: 0,
    RIGHT: 1,
};

var SlideLayout = GObject.registerClass({
    Properties: {
        'slide-x': GObject.ParamSpec.double(
            'slide-x', 'slide-x', 'slide-x',
            GObject.ParamFlags.READWRITE,
            0, 1, 1),
    },
}, class SlideLayout extends Clutter.FixedLayout {
    _init(params) {
        this._slideX = 1;
        this._direction = SlideDirection.LEFT;

        super._init(params);
    }

    vfunc_get_preferred_width(container, forHeight) {
        let child = container.get_first_child();

        let [minWidth, natWidth] = child.get_preferred_width(forHeight);

        minWidth *= this._slideX;
        natWidth *= this._slideX;

        return [minWidth, natWidth];
    }

    vfunc_allocate(container, box, flags) {
        let child = container.get_first_child();

        let availWidth = Math.round(box.x2 - box.x1);
        let availHeight = Math.round(box.y2 - box.y1);
        let [, natWidth] = child.get_preferred_width(availHeight);

        // Align the actor inside the clipped box, as the actor's alignment
        // flags only determine what to do if the allocated box is bigger
        // than the actor's box.
        let realDirection = getRtlSlideDirection(this._direction, child);
        let alignX = realDirection == SlideDirection.LEFT
            ? availWidth - natWidth
            : availWidth - natWidth * this._slideX;

        let actorBox = new Clutter.ActorBox();
        actorBox.x1 = box.x1 + alignX;
        actorBox.x2 = actorBox.x1 + (child.x_expand ? availWidth : natWidth);
        actorBox.y1 = box.y1;
        actorBox.y2 = actorBox.y1 + availHeight;

        child.allocate(actorBox, flags);
    }

    // eslint-disable-next-line camelcase
    set slide_x(value) {
        if (this._slideX == value)
            return;
        this._slideX = value;
        this.notify('slide-x');
        this.layout_changed();
    }

    // eslint-disable-next-line camelcase
    get slide_x() {
        return this._slideX;
    }

    set slideDirection(direction) {
        this._direction = direction;
        this.layout_changed();
    }

    get slideDirection() {
        return this._direction;
    }
});

var SlidingControl = GObject.registerClass(
class SlidingControl extends St.Widget {
    _init(params) {
        params = Params.parse(params, { slideDirection: SlideDirection.LEFT });

        this.layout = new SlideLayout();
        this.layout.slideDirection = params.slideDirection;
        super._init({
            layout_manager: this.layout,
            style_class: 'overview-controls',
            clip_to_allocation: true,
        });

        this._visible = true;
        this._inDrag = false;

        Main.overview.connect('hiding', this._onOverviewHiding.bind(this));

        Main.overview.connect('item-drag-begin', this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-cancelled', this._onDragEnd.bind(this));

        Main.overview.connect('window-drag-begin', this._onWindowDragBegin.bind(this));
        Main.overview.connect('window-drag-cancelled', this._onWindowDragEnd.bind(this));
        Main.overview.connect('window-drag-end', this._onWindowDragEnd.bind(this));
    }

    _getSlide() {
        throw new GObject.NotImplementedError('_getSlide in %s'.format(this.constructor.name));
    }

    _updateSlide() {
        this.ease_property('@layout.slide-x', this._getSlide(), {
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: SIDE_CONTROLS_ANIMATION_TIME,
        });
    }

    getVisibleWidth() {
        let child = this.get_first_child();
        let [, , natWidth] = child.get_preferred_size();
        return natWidth;
    }

    _getTranslation() {
        let child = this.get_first_child();
        let direction = getRtlSlideDirection(this.layout.slideDirection, child);
        let visibleWidth = this.getVisibleWidth();

        if (direction == SlideDirection.LEFT)
            return -visibleWidth;
        else
            return visibleWidth;
    }

    _updateTranslation() {
        let translationStart = 0;
        let translationEnd = 0;
        let translation = this._getTranslation();

        let shouldShow = this._getSlide() > 0;
        if (shouldShow)
            translationStart = translation;
        else
            translationEnd = translation;

        if (this.translation_x === translationEnd)
            return;

        this.translation_x = translationStart;
        this.ease({
            translation_x: translationEnd,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: SIDE_CONTROLS_ANIMATION_TIME,
        });
    }

    _onOverviewHiding() {
        // We need to explicitly slideOut since showing pages
        // doesn't imply sliding out, instead, hiding the overview does.
        this.slideOut();
    }

    _onWindowDragBegin() {
        this._onDragBegin();
    }

    _onWindowDragEnd() {
        this._onDragEnd();
    }

    _onDragBegin() {
        this._inDrag = true;
        this._updateTranslation();
        this._updateSlide();
    }

    _onDragEnd() {
        this._inDrag = false;
        this._updateSlide();
    }

    fadeIn() {
        this.ease({
            opacity: 255,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
        });
    }

    fadeHalf() {
        this.ease({
            opacity: 128,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    slideIn() {
        this._visible = true;
        // we will update slide_x and the translation from pageEmpty
    }

    slideOut() {
        this._visible = false;
        this._updateTranslation();
        // we will update slide_x from pageEmpty
    }

    pageEmpty() {
        // When pageEmpty is received, there's no visible view in the
        // selector; this means we can now safely set the full slide for
        // the next page, since slideIn or slideOut might have been called,
        // changing the visiblity
        this.remove_transition('@layout.slide-x');
        this.layout.slide_x = this._getSlide();
        this._updateTranslation();
    }
});

var ThumbnailsSlider = GObject.registerClass(
class ThumbnailsSlider extends SlidingControl {
    _init(thumbnailsBox) {
        super._init({ slideDirection: SlideDirection.RIGHT });

        this._thumbnailsBox = thumbnailsBox;

        this.request_mode = Clutter.RequestMode.WIDTH_FOR_HEIGHT;
        this.reactive = true;
        this.track_hover = true;
        this.add_actor(this._thumbnailsBox);

        Main.layoutManager.connect('monitors-changed', this._updateSlide.bind(this));
        global.workspace_manager.connect('active-workspace-changed',
                                         this._updateSlide.bind(this));
        global.workspace_manager.connect('notify::n-workspaces',
                                         this._updateSlide.bind(this));
        this.connect('notify::hover', this._updateSlide.bind(this));
        this._thumbnailsBox.bind_property('visible', this, 'visible', GObject.BindingFlags.SYNC_CREATE);
    }

    _getAlwaysZoomOut() {
        // Always show the pager on hover, during a drag, or if workspaces are
        // actually used, e.g. there are windows on any non-active workspace
        let workspaceManager = global.workspace_manager;
        let alwaysZoomOut = this.hover ||
                            this._inDrag ||
                            !Meta.prefs_get_dynamic_workspaces() ||
                            workspaceManager.n_workspaces > 2 ||
                            workspaceManager.get_active_workspace_index() != 0;

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
    }

    getNonExpandedWidth() {
        let child = this.get_first_child();
        return child.get_theme_node().get_length('visible-width');
    }

    _onDragEnd() {
        this.sync_hover();
        super._onDragEnd();
    }

    _getSlide() {
        if (!this._visible)
            return 0;

        let alwaysZoomOut = this._getAlwaysZoomOut();
        if (alwaysZoomOut)
            return 1;

        let child = this.get_first_child();
        let preferredHeight = child.get_preferred_height(-1)[1];
        let expandedWidth = child.get_preferred_width(preferredHeight)[1];

        return this.getNonExpandedWidth() / expandedWidth;
    }

    getVisibleWidth() {
        let alwaysZoomOut = this._getAlwaysZoomOut();
        if (alwaysZoomOut)
            return super.getVisibleWidth();
        else
            return this.getNonExpandedWidth();
    }
});

var DashSlider = GObject.registerClass(
class DashSlider extends SlidingControl {
    _init(dash) {
        super._init({ slideDirection: SlideDirection.LEFT });

        this._dash = dash;

        // SlideLayout reads the actor's expand flags to decide
        // whether to allocate the natural size to its child, or the whole
        // available allocation
        this._dash.x_expand = true;

        this.x_expand = true;
        this.x_align = Clutter.ActorAlign.START;
        this.y_expand = true;

        this.add_actor(this._dash);

        this._dash.connect('icon-size-changed', this._updateSlide.bind(this));
    }

    _getSlide() {
        if (this._visible || this._inDrag)
            return 1;
        else
            return 0;
    }

    _onWindowDragBegin() {
        this.fadeHalf();
    }

    _onWindowDragEnd() {
        this.fadeIn();
    }
});

var DashSpacer = GObject.registerClass(
class DashSpacer extends St.Widget {
    _init(params) {
        super._init(params);

        this._bindConstraint = null;
    }

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
    }

    vfunc_get_preferred_width(forHeight) {
        if (this._bindConstraint)
            return this._bindConstraint.source.get_preferred_width(forHeight);
        return super.vfunc_get_preferred_width(forHeight);
    }

    vfunc_get_preferred_height(forWidth) {
        if (this._bindConstraint)
            return this._bindConstraint.source.get_preferred_height(forWidth);
        return super.vfunc_get_preferred_height(forWidth);
    }
});

var ControlsLayout = GObject.registerClass({
    Signals: { 'allocation-changed': { flags: GObject.SignalFlags.RUN_LAST } },
}, class ControlsLayout extends Clutter.BinLayout {
    vfunc_allocate(container, box, flags) {
        super.vfunc_allocate(container, box, flags);
        this.emit('allocation-changed');
    }
});

var ControlsManager = GObject.registerClass(
class ControlsManager extends St.Widget {
    _init(searchEntry) {
        let layout = new ControlsLayout();
        super._init({
            layout_manager: layout,
            x_expand: true,
            y_expand: true,
            clip_to_allocation: true,
        });

        this.dash = new Dash.Dash();
        this._dashSlider = new DashSlider(this.dash);
        this._dashSpacer = new DashSpacer();
        this._dashSpacer.setDashActor(this._dashSlider);

        let workspaceManager = global.workspace_manager;
        let activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        this._workspaceAdjustment = new St.Adjustment({
            value: activeWorkspaceIndex,
            lower: 0,
            page_increment: 1,
            page_size: 1,
            step_increment: 0,
            upper: workspaceManager.n_workspaces,
        });

        this._nWorkspacesNotifyId =
            workspaceManager.connect('notify::n-workspaces',
                this._updateAdjustment.bind(this));

        this._thumbnailsBox =
            new WorkspaceThumbnail.ThumbnailsBox(this._workspaceAdjustment);
        this._thumbnailsSlider = new ThumbnailsSlider(this._thumbnailsBox);

        this.viewSelector = new ViewSelector.ViewSelector(searchEntry,
            this._workspaceAdjustment, this.dash.showAppsButton);
        this.viewSelector.connect('page-changed', this._setVisibility.bind(this));
        this.viewSelector.connect('page-empty', this._onPageEmpty.bind(this));

        this._group = new St.BoxLayout({ name: 'overview-group',
                                         x_expand: true, y_expand: true });
        this.add_actor(this._group);

        this.add_actor(this._dashSlider);

        this._group.add_actor(this._dashSpacer);
        this._group.add_child(this.viewSelector);
        this._group.add_actor(this._thumbnailsSlider);

        layout.connect('allocation-changed', this._updateWorkspacesGeometry.bind(this));

        Main.overview.connect('showing', this._updateSpacerVisibility.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        global.workspace_manager.disconnect(this._nWorkspacesNotifyId);
    }

    _updateAdjustment() {
        let workspaceManager = global.workspace_manager;
        let newNumWorkspaces = workspaceManager.n_workspaces;
        let activeIndex = workspaceManager.get_active_workspace_index();

        this._workspaceAdjustment.upper = newNumWorkspaces;

        // A workspace might have been inserted or removed before the active
        // one, causing the adjustment to go out of sync, so update the value
        this._workspaceAdjustment.remove_transition('value');
        this._workspaceAdjustment.value = activeIndex;
    }

    _updateWorkspacesGeometry() {
        let [x, y] = this.get_transformed_position();
        let [width, height] = this.get_transformed_size();
        let geometry = { x, y, width, height };

        let spacing = this.get_theme_node().get_length('spacing');
        let dashWidth = this._dashSlider.getVisibleWidth() + spacing;
        let thumbnailsWidth = this._thumbnailsSlider.getNonExpandedWidth() + spacing;

        geometry.width -= dashWidth;
        geometry.width -= thumbnailsWidth;

        if (this.get_text_direction() == Clutter.TextDirection.LTR)
            geometry.x += dashWidth;
        else
            geometry.x += thumbnailsWidth;

        this.viewSelector.setWorkspacesFullGeometry(geometry);
    }

    _setVisibility() {
        // Ignore the case when we're leaving the overview, since
        // actors will be made visible again when entering the overview
        // next time, and animating them while doing so is just
        // unnecessary noise
        if (!Main.overview.visible ||
            (Main.overview.animationInProgress && !Main.overview.visibleTarget))
            return;

        let activePage = this.viewSelector.getActivePage();
        let dashVisible = activePage == ViewSelector.ViewPage.WINDOWS ||
                           activePage == ViewSelector.ViewPage.APPS;
        let thumbnailsVisible = activePage == ViewSelector.ViewPage.WINDOWS;

        if (dashVisible)
            this._dashSlider.slideIn();
        else
            this._dashSlider.slideOut();

        if (thumbnailsVisible)
            this._thumbnailsSlider.slideIn();
        else
            this._thumbnailsSlider.slideOut();
    }

    _updateSpacerVisibility() {
        if (Main.overview.animationInProgress && !Main.overview.visibleTarget)
            return;

        let activePage = this.viewSelector.getActivePage();
        this._dashSpacer.visible = activePage == ViewSelector.ViewPage.WINDOWS;
    }

    _onPageEmpty() {
        this._dashSlider.pageEmpty();
        this._thumbnailsSlider.pageEmpty();

        this._updateSpacerVisibility();
    }
});
