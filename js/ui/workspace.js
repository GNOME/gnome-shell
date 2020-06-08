// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Workspace */

const { Atk, Clutter, GLib, GObject,
        Graphene, Meta, Pango, Shell, St } = imports.gi;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Overview = imports.ui.overview;

var WINDOW_DND_SIZE = 256;

var WINDOW_PREVIEW_MAXIMUM_SCALE = 1.0;

var WINDOW_OVERLAY_IDLE_HIDE_TIMEOUT = 750;
var WINDOW_OVERLAY_FADE_TIME = 200;

var WINDOW_REPOSITIONING_DELAY = 750;

var DRAGGING_WINDOW_OPACITY = 100;

// When calculating a layout, we calculate the scale of windows and the percent
// of the available area the new layout uses. If the values for the new layout,
// when weighted with the values as below, are worse than the previous layout's,
// we stop looking for a new layout and use the previous layout.
// Otherwise, we keep looking for a new layout.
var LAYOUT_SCALE_WEIGHT = 1;
var LAYOUT_SPACE_WEIGHT = 0.1;

var WINDOW_ANIMATION_MAX_NUMBER_BLENDING = 3;

function _interpolate(start, end, step) {
    return start + (end - start) * step;
}

var WindowPreviewLayout = GObject.registerClass({
    Properties: {
        'bounding-box': GObject.ParamSpec.boxed(
            'bounding-box', 'Bounding box', 'Bounding box',
            GObject.ParamFlags.READABLE,
            Clutter.ActorBox.$gtype),
    },
}, class WindowPreviewLayout extends Clutter.LayoutManager {
    _init() {
        super._init();

        this._container = null;
        this._boundingBox = new Clutter.ActorBox();
        this._windows = new Map();
    }

    _layoutChanged() {
        let frameRect;

        for (const windowInfo of this._windows.values()) {
            const frame = windowInfo.metaWindow.get_frame_rect();
            frameRect = frameRect ? frameRect.union(frame) : frame;
        }

        if (!frameRect)
            frameRect = new Meta.Rectangle();

        const oldBox = this._boundingBox.copy();
        this._boundingBox.set_origin(frameRect.x, frameRect.y);
        this._boundingBox.set_size(frameRect.width, frameRect.height);

        if (!this._boundingBox.equal(oldBox))
            this.notify('bounding-box');

        // Always call layout_changed(), a size or position change of an
        // attached dialog might not affect the boundingBox
        this.layout_changed();
    }

    vfunc_set_container(container) {
        this._container = container;
    }

    vfunc_get_preferred_height(_container, _forWidth) {
        return [0, this._boundingBox.get_height()];
    }

    vfunc_get_preferred_width(_container, _forHeight) {
        return [0, this._boundingBox.get_width()];
    }

    vfunc_allocate(container, box) {
        // If the scale isn't 1, we weren't allocated our preferred size
        // and have to scale the children allocations accordingly.
        const scaleX = box.get_width() / this._boundingBox.get_width();
        const scaleY = box.get_height() / this._boundingBox.get_height();

        const childBox = new Clutter.ActorBox();

        for (const child of container) {
            if (!child.visible)
                continue;

            const windowInfo = this._windows.get(child);
            if (windowInfo) {
                const bufferRect = windowInfo.metaWindow.get_buffer_rect();
                childBox.set_origin(
                    bufferRect.x - this._boundingBox.x1,
                    bufferRect.y - this._boundingBox.y1);

                const [, , natWidth, natHeight] = child.get_preferred_size();
                childBox.set_size(natWidth, natHeight);

                childBox.x1 *= scaleX;
                childBox.x2 *= scaleX;
                childBox.y1 *= scaleY;
                childBox.y2 *= scaleY;

                child.allocate(childBox);
            } else {
                child.allocate_preferred_size();
            }
        }
    }

    /**
     * addWindow:
     * @param {Clutter.Actor} window: a single window to add to the grid
     * @param {Meta.Window} metaWindow: the MetaWindow instance of @window
     *
     * Adds a ClutterActor @window which is painting the texture of a
     * MetaWindowActor to the preview. If @window is already part of the
     * preview, this function will do nothing.
     */
    addWindow(window, metaWindow) {
        if (this._windows.has(window))
            return;

        const windowActor = metaWindow.get_compositor_private();

        this._windows.set(window, {
            metaWindow,
            windowActor,
            sizeChangedId: metaWindow.connect('size-changed', () =>
                this._layoutChanged()),
            positionChangedId: metaWindow.connect('position-changed', () =>
                this._layoutChanged()),
            windowActorDestroyId: windowActor.connect('destroy', () =>
                window.destroy()),
            destroyId: window.connect('destroy', () =>
                this.removeWindow(window)),
        });

        this._container.add_child(window);

        this._layoutChanged();
    }

    /**
     * removeItem:
     * @param {Clutter.Actor} window: a window to remove from the grid
     *
     * Removes a ClutterActor @window which has been added previously using
     * addWindow() from the preview. If @window is not part of preview, this
     * function will do nothing.
     */
    removeWindow(window) {
        const windowInfo = this._windows.get(window);
        if (!windowInfo)
            return;

        windowInfo.metaWindow.disconnect(windowInfo.sizeChangedId);
        windowInfo.metaWindow.disconnect(windowInfo.positionChangedId);
        windowInfo.windowActor.disconnect(windowInfo.windowActorDestroyId);
        window.disconnect(windowInfo.destroyId);

        this._windows.delete(window);
        this._container.remove_child(window);

        this._layoutChanged();
    }

    /**
     * getWindows:
     *
     * Gets an array of all ClutterActors that were added to the layout
     * using addWindow(), ordered by the insertion order.
     *
     * @returns {Array} An array including all windows
     */
    getWindows() {
        return Array.from(this._windows.keys());
    }

    /**
     * getMetaWindow:
     * @param {Clutter.Actor} window: the window to get the MetaWindow for
     *
     * Gets the MetaWindow associated to the ClutterActor @window that was
     * added to the layout using addWindow(). If @window is not found,
     * null is returned.
     *
     * @returns {Meta.Window} The metaWindow of the window
     */
    getMetaWindow(window) {
        const windowInfo = this._windows.get(window);
        if (!windowInfo)
            return null;

        return windowInfo.metaWindow;
    }

    // eslint-disable-next-line camelcase
    get bounding_box() {
        return this._boundingBox;
    }
});

var WindowPreview = GObject.registerClass({
    Signals: {
        'drag-begin': {},
        'drag-cancelled': {},
        'drag-end': {},
        'selected': { param_types: [GObject.TYPE_UINT] },
        'show-chrome': {},
        'size-changed': {},
    },
}, class WindowPreview extends St.Widget {
    _init(metaWindow, workspace) {
        this.metaWindow = metaWindow;
        this.metaWindow._delegate = this;
        this._windowActor = metaWindow.get_compositor_private();
        this._workspace = workspace;

        super._init({
            reactive: true,
            can_focus: true,
            accessible_role: Atk.Role.PUSH_BUTTON,
            offscreen_redirect: Clutter.OffscreenRedirect.AUTOMATIC_FOR_OPACITY,
        });

        this._windowContainer = new Clutter.Actor();
        // gjs currently can't handle setting an actors layout manager during
        // the initialization of the actor if that layout manager keeps track
        // of its container, so set the layout manager after creating the
        // container
        this._windowContainer.layout_manager = new WindowPreviewLayout();
        this.add_child(this._windowContainer);

        this._addWindow(metaWindow);

        this._delegate = this;

        this.slotId = 0;
        this._stackAbove = null;

        this._windowContainer.layout_manager.connect(
            'notify::bounding-box', () => this.emit('size-changed'));

        this._windowDestroyId =
            this._windowActor.connect('destroy', () => this.destroy());

        this._updateAttachedDialogs();
        this.x = this.boundingBox.x;
        this.y = this.boundingBox.y;

        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', () => this._activate());
        clickAction.connect('long-press', this._onLongPress.bind(this));
        this.add_action(clickAction);
        this.connect('destroy', this._onDestroy.bind(this));

        this._draggable = DND.makeDraggable(this,
                                            { restoreOnSuccess: true,
                                              manualMode: true,
                                              dragActorMaxSize: WINDOW_DND_SIZE,
                                              dragActorOpacity: DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', this._onDragBegin.bind(this));
        this._draggable.connect('drag-cancelled', this._onDragCancelled.bind(this));
        this._draggable.connect('drag-end', this._onDragEnd.bind(this));
        this.inDrag = false;

        this._selected = false;
        this._closeRequested = false;
        this._idleHideOverlayId = 0;

        // We sync the border actor size with the clone size using a
        // BindConstraint and make it larger by borderSize * 2 using
        // BindConstraints offset property. Then we align the border actor
        // so that its center point equals the center point of the clone
        // using an AlignConstraint.
        this._border = new St.Widget({
            visible: false,
            style_class: 'window-clone-border',
        });
        this._borderConstraint = new Clutter.BindConstraint({
            source: this._windowContainer,
            coordinate: Clutter.BindCoordinate.SIZE,
        });
        this._border.add_constraint(this._borderConstraint);
        this._border.add_constraint(new Clutter.AlignConstraint({
            source: this._windowContainer,
            align_axis: Clutter.AlignAxis.BOTH,
            factor: 0.5,
        }));
        this._border.connect('style-changed',
            this._onBorderStyleChanged.bind(this));

        this._title = new St.Label({
            visible: false,
            style_class: 'window-caption',
            text: this._getCaption(),
            reactive: true,
        });
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: this._windowContainer,
            align_axis: Clutter.AlignAxis.X_AXIS,
            factor: 0.5,
        }));
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: this._windowContainer,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            align_position: Clutter.AlignPosition.ON_EDGE,
            factor: 1,
        }));
        this._title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        this.label_actor = this._title;
        this._updateCaptionId = this.metaWindow.connect('notify::title', () => {
            this._title.text = this._getCaption();
        });

        const layout = Meta.prefs_get_button_layout();
        const side = layout.left_buttons.includes(Meta.ButtonFunction.CLOSE)
            ? St.Side.LEFT : St.Side.RIGHT;

        this._closeButton = new St.Button({
            visible: false,
            style_class: 'window-close',
            child: new St.Icon({ icon_name: 'window-close-symbolic' }),
        });
        this._closeButton.add_constraint(new Clutter.BindConstraint({
            source: this._border,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: this._border,
            align_axis: Clutter.AlignAxis.X_AXIS,
            align_position: Clutter.AlignPosition.ON_EDGE,
            factor: side === St.Side.LEFT ? 0 : 1,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: this._border,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            align_position: Clutter.AlignPosition.ON_EDGE,
            factor: 0,
        }));
        this._closeButton.connect('clicked', () => this._deleteAll());

        this.add_child(this._border);
        this.add_child(this._title);
        this.add_child(this._closeButton);
    }

    vfunc_get_preferred_width(forHeight) {
        const themeNode = this.get_theme_node();
        const [minWidth, natWidth] =
            this._windowContainer.get_preferred_width(
                themeNode.adjust_for_height(forHeight));

        return themeNode.adjust_preferred_width(minWidth, natWidth);
    }

    vfunc_get_preferred_height(forWidth) {
        const themeNode = this.get_theme_node();
        const [minHeight, natHeight] =
            this._windowContainer.get_preferred_height(
                themeNode.adjust_for_width(forWidth));

        return themeNode.adjust_preferred_height(minHeight, natHeight);
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        for (const child of this)
            child.allocate_available_size(0, 0, box.get_width(), box.get_height());
    }

    _onBorderStyleChanged() {
        let borderNode = this._border.get_theme_node();
        this._borderSize = borderNode.get_border_width(St.Side.TOP);

        this._borderConstraint.offset = this._borderSize * 2;
    }

    _windowCanClose() {
        return this.metaWindow.can_close() &&
               !this._hasAttachedDialogs();
    }

    _getCaption() {
        if (this.metaWindow.title)
            return this.metaWindow.title;

        let tracker = Shell.WindowTracker.get_default();
        let app = tracker.get_window_app(this.metaWindow);
        return app.get_name();
    }

    chromeHeights() {
        this._border.ensure_style();
        const [, closeButtonHeight] = this._closeButton.get_preferred_height(-1);
        const [, titleHeight] = this._title.get_preferred_height(-1);

        return [this._borderSize + closeButtonHeight / 2,
                Math.max(this._borderSize, titleHeight / 2)];
    }

    chromeWidths() {
        this._border.ensure_style();
        const [, closeButtonWidth] = this._closeButton.get_preferred_width(-1);

        return [this._borderSize,
                this._borderSize + closeButtonWidth / 2];
    }

    showOverlay(animate) {
        const ongoingTransition = this._border.get_transition('opacity');

        // Don't do anything if we're fully visible already
        if (this._border.visible && !ongoingTransition)
            return;

        // If we're supposed to animate and an animation in our direction
        // is already happening, let that one continue
        if (animate &&
            ongoingTransition &&
            ongoingTransition.get_interval().peek_final_value() === 255)
            return;

        const toShow = this._windowCanClose()
            ? [this._border, this._title, this._closeButton]
            : [this._border, this._title];

        toShow.forEach(a => {
            a.remove_transition('opacity');
            a.opacity = 0;
            a.show();
            a.ease({
                opacity: 255,
                duration: animate ? WINDOW_OVERLAY_FADE_TIME : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        });

        this.emit('show-chrome');
    }

    hideOverlay(animate) {
        const ongoingTransition = this._border.get_transition('opacity');

        // Don't do anything if we're fully hidden already
        if (!this._border.visible && !ongoingTransition)
            return;

        // If we're supposed to animate and an animation in our direction
        // is already happening, let that one continue
        if (animate &&
            ongoingTransition &&
            ongoingTransition.get_interval().peek_final_value() === 0)
            return;

        [this._border, this._title, this._closeButton].forEach(a => {
            a.remove_transition('opacity');
            a.opacity = 255;
            a.ease({
                opacity: 0,
                duration: animate ? WINDOW_OVERLAY_FADE_TIME : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => a.hide(),
            });
        });
    }

    _addWindow(metaWindow) {
        const windowActor = metaWindow.get_compositor_private();
        const clone = new Clutter.Clone({ source: windowActor });

        // We expect this to be used for all interaction rather than
        // the ClutterClone; as the former is reactive and the latter
        // is not, this just works for most cases. However, for DND all
        // actors are picked, so DND operations would operate on the clone.
        // To avoid this, we hide it from pick.
        Shell.util_set_hidden_from_pick(clone, true);

        this._windowContainer.layout_manager.addWindow(clone, metaWindow);
    }

    vfunc_has_overlaps() {
        return this._hasAttachedDialogs();
    }

    _deleteAll() {
        const windows = this._windowContainer.layout_manager.getWindows();

        // Delete all windows, starting from the bottom-most (most-modal) one
        for (const window of windows.reverse()) {
            const metaWindow =
                this._windowContainer.layout_manager.getMetaWindow(window);

            metaWindow.delete(global.get_current_time());
        }

        this._closeRequested = true;
    }

    addDialog(win) {
        let parent = win.get_transient_for();
        while (parent.is_attached_dialog())
            parent = parent.get_transient_for();

        // Display dialog if it is attached to our metaWindow
        if (win.is_attached_dialog() && parent == this.metaWindow)
            this._addWindow(win);

        // The dialog popped up after the user tried to close the window,
        // assume it's a close confirmation and leave the overview
        if (this._closeRequested)
            this._activate();
    }

    _hasAttachedDialogs() {
        return this._windowContainer.layout_manager.getWindows().length > 1;
    }

    _updateAttachedDialogs() {
        let iter = win => {
            let actor = win.get_compositor_private();

            if (!actor)
                return false;
            if (!win.is_attached_dialog())
                return false;

            this._addWindow(win);
            win.foreach_transient(iter);
            return true;
        };
        this.metaWindow.foreach_transient(iter);
    }

    get boundingBox() {
        const box = this._windowContainer.layout_manager.bounding_box;

        return {
            x: box.x1,
            y: box.y1,
            width: box.get_width(),
            height: box.get_height(),
        };
    }

    get windowCenter() {
        const box = this._windowContainer.layout_manager.bounding_box;

        return new Graphene.Point({
            x: box.get_x() + box.get_width() / 2,
            y: box.get_y() + box.get_height() / 2,
        });
    }

    // Find the actor just below us, respecting reparenting done by DND code
    _getActualStackAbove() {
        if (this._stackAbove == null)
            return null;

        if (this.inDrag) {
            if (this._stackAbove._delegate)
                return this._stackAbove._delegate._getActualStackAbove();
            else
                return null;
        } else {
            return this._stackAbove;
        }
    }

    setStackAbove(actor) {
        this._stackAbove = actor;
        if (this.inDrag)
            // We'll fix up the stack after the drag
            return;

        let parent = this.get_parent();
        let actualAbove = this._getActualStackAbove();
        if (actualAbove == null)
            parent.set_child_below_sibling(this, null);
        else
            parent.set_child_above_sibling(this, actualAbove);
    }

    _onDestroy() {
        this._windowActor.disconnect(this._windowDestroyId);

        this.metaWindow._delegate = null;
        this._delegate = null;

        this.metaWindow.disconnect(this._updateCaptionId);

        if (this._longPressLater) {
            Meta.later_remove(this._longPressLater);
            delete this._longPressLater;
        }

        if (this._idleHideOverlayId > 0) {
            GLib.source_remove(this._idleHideOverlayId);
            this._idleHideOverlayId = 0;
        }

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }
    }

    _activate() {
        this._selected = true;
        this.emit('selected', global.get_current_time());
    }

    vfunc_enter_event(crossingEvent) {
        this.showOverlay(true);
        return super.vfunc_enter_event(crossingEvent);
    }

    vfunc_leave_event(crossingEvent) {
        if (this._idleHideOverlayId > 0)
            GLib.source_remove(this._idleHideOverlayId);

        this._idleHideOverlayId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            WINDOW_OVERLAY_IDLE_HIDE_TIMEOUT, () => {
                if (this._closeButton['has-pointer'] ||
                    this._title['has-pointer'])
                    return GLib.SOURCE_CONTINUE;

                if (!this['has-pointer'])
                    this.hideOverlay(true);

                this._idleHideOverlayId = 0;
                return GLib.SOURCE_REMOVE;
            });

        GLib.Source.set_name_by_id(this._idleHideOverlayId, '[gnome-shell] this._idleHideOverlayId');

        return super.vfunc_leave_event(crossingEvent);
    }

    vfunc_key_focus_in() {
        super.vfunc_key_focus_in();
        this.showOverlay(true);
    }

    vfunc_key_focus_out() {
        super.vfunc_key_focus_out();
        this.hideOverlay(true);
    }

    vfunc_key_press_event(keyEvent) {
        let symbol = keyEvent.keyval;
        let isEnter = symbol == Clutter.KEY_Return || symbol == Clutter.KEY_KP_Enter;
        if (isEnter) {
            this._activate();
            return true;
        }

        return super.vfunc_key_press_event(keyEvent);
    }

    _onLongPress(action, actor, state) {
        // Take advantage of the Clutter policy to consider
        // a long-press canceled when the pointer movement
        // exceeds dnd-drag-threshold to manually start the drag
        if (state == Clutter.LongPressState.CANCEL) {
            let event = Clutter.get_current_event();
            this._dragTouchSequence = event.get_event_sequence();

            if (this._longPressLater)
                return true;

            // A click cancels a long-press before any click handler is
            // run - make sure to not start a drag in that case
            this._longPressLater = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                delete this._longPressLater;
                if (this._selected)
                    return;
                let [x, y] = action.get_coords();
                action.release();
                this._draggable.startDrag(x, y, global.get_current_time(), this._dragTouchSequence, event.get_device());
            });
        } else {
            this.showOverlay(true);
        }
        return true;
    }

    _onDragBegin(_draggable, _time) {
        this.inDrag = true;
        this.hideOverlay(false);
        this.emit('drag-begin');
    }

    handleDragOver(source, actor, x, y, time) {
        return this._workspace.handleDragOver(source, actor, x, y, time);
    }

    acceptDrop(source, actor, x, y, time) {
        return this._workspace.acceptDrop(source, actor, x, y, time);
    }

    _onDragCancelled(_draggable, _time) {
        this.emit('drag-cancelled');
    }

    _onDragEnd(_draggable, _time, _snapback) {
        this.inDrag = false;

        // We may not have a parent if DnD completed successfully, in
        // which case our clone will shortly be destroyed and replaced
        // with a new one on the target workspace.
        let parent = this.get_parent();
        if (parent !== null) {
            if (this._stackAbove == null)
                parent.set_child_below_sibling(this, null);
            else
                parent.set_child_above_sibling(this, this._stackAbove);
        }

        if (this['has-pointer'])
            this.showOverlay(true);

        this.emit('drag-end');
    }
});

var WindowPositionFlags = {
    NONE: 0,
    INITIAL: 1 << 0,
    ANIMATE: 1 << 1,
};

// Window Thumbnail Layout Algorithm
// =================================
//
// General overview
// ----------------
//
// The window thumbnail layout algorithm calculates some optimal layout
// by computing layouts with some number of rows, calculating how good
// each layout is, and stopping iterating when it finds one that is worse
// than the previous layout. A layout consists of which windows are in
// which rows, row sizes and other general state tracking that would make
// calculating window positions from this information fairly easy.
//
// After a layout is computed that's considered the best layout, we
// compute the layout scale to fit it in the area, and then compute
// slots (sizes and positions) for each thumbnail.
//
// Layout generation
// -----------------
//
// Layout generation is naive and simple: we simply add windows to a row
// until we've added too many windows to a row, and then make a new row,
// until we have our required N rows. The potential issue with this strategy
// is that we may have too many windows at the bottom in some pathological
// cases, which tends to make the thumbnails have the shape of a pile of
// sand with a peak, with one window at the top.
//
// Scaling factors
// ---------------
//
// Thumbnail position is mostly straightforward -- the main issue is
// computing an optimal scale for each window that fits the constraints,
// and doesn't make the thumbnail too small to see. There are two factors
// involved in thumbnail scale to make sure that these two goals are met:
// the window scale (calculated by _computeWindowScale) and the layout
// scale (calculated by computeSizeAndScale).
//
// The calculation logic becomes slightly more complicated because row
// and column spacing are not scaled, they're constant, so we can't
// simply generate a bunch of window positions and then scale it. In
// practice, it's not too bad -- we can simply try to fit the layout
// in the input area minus whatever spacing we have, and then add
// it back afterwards.
//
// The window scale is constant for the window's size regardless of the
// input area or the layout scale or rows or anything else, and right
// now just enlarges the window if it's too small. The fact that this
// factor is stable makes it easy to calculate, so there's no sense
// in not applying it in most calculations.
//
// The layout scale depends on the input area, the rows, etc, but is the
// same for the entire layout, rather than being per-window. After
// generating the rows of windows, we basically do some basic math to
// fit the full, unscaled layout to the input area, as described above.
//
// With these two factors combined, the final scale of each thumbnail is
// simply windowScale * layoutScale... almost.
//
// There's one additional constraint: the thumbnail scale must never be
// larger than WINDOW_PREVIEW_MAXIMUM_SCALE, which means that the inequality:
//
//   windowScale * layoutScale <= WINDOW_PREVIEW_MAXIMUM_SCALE
//
// must always be true. This is for each individual window -- while we
// could adjust layoutScale to make the largest thumbnail smaller than
// WINDOW_PREVIEW_MAXIMUM_SCALE, it would shrink windows which are already
// under the inequality. To solve this, we simply cheat: we simply keep
// each window's "cell" area to be the same, but we shrink the thumbnail
// and center it horizontally, and align it to the bottom vertically.

var LayoutStrategy = class {
    constructor(monitor, rowSpacing, columnSpacing) {
        if (this.constructor === LayoutStrategy)
            throw new TypeError(`Cannot instantiate abstract type ${this.constructor.name}`);

        this._monitor = monitor;
        this._rowSpacing = rowSpacing;
        this._columnSpacing = columnSpacing;
    }

    _newRow() {
        // Row properties:
        //
        // * x, y are the position of row, relative to area
        //
        // * width, height are the scaled versions of fullWidth, fullHeight
        //
        // * width also has the spacing in between windows. It's not in
        //   fullWidth, as the spacing is constant, whereas fullWidth is
        //   meant to be scaled
        //
        // * neither height/fullHeight have any sort of spacing or padding
        return { x: 0, y: 0,
                 width: 0, height: 0,
                 fullWidth: 0, fullHeight: 0,
                 windows: [] };
    }

    // Computes and returns an individual scaling factor for @window,
    // to be applied in addition to the overall layout scale.
    _computeWindowScale(window) {
        // Since we align windows next to each other, the height of the
        // thumbnails is much more important to preserve than the width of
        // them, so two windows with equal height, but maybe differering
        // widths line up.
        let ratio = window.boundingBox.height / this._monitor.height;

        // The purpose of this manipulation here is to prevent windows
        // from getting too small. For something like a calculator window,
        // we need to bump up the size just a bit to make sure it looks
        // good. We'll use a multiplier of 1.5 for this.

        // Map from [0, 1] to [1.5, 1]
        return _interpolate(1.5, 1, ratio);
    }

    // Compute the size of each row, by assigning to the properties
    // row.width, row.height, row.fullWidth, row.fullHeight, and
    // (optionally) for each row in @layout.rows. This method is
    // intended to be called by subclasses.
    _computeRowSizes(_layout) {
        throw new GObject.NotImplementedError(`_computeRowSizes in ${this.constructor.name}`);
    }

    // Compute strategy-specific window slots for each window in
    // @windows, given the @layout. The strategy may also use @layout
    // as strategy-specific storage.
    //
    // This must calculate:
    //  * maxColumns - The maximum number of columns used by the layout.
    //  * gridWidth - The total width used by the grid, unscaled, unspaced.
    //  * gridHeight - The totial height used by the grid, unscaled, unspaced.
    //  * rows - A list of rows, which should be instantiated by _newRow.
    computeLayout(_windows, _layout) {
        throw new GObject.NotImplementedError(`computeLayout in ${this.constructor.name}`);
    }

    // Given @layout, compute the overall scale and space of the layout.
    // The scale is the individual, non-fancy scale of each window, and
    // the space is the percentage of the available area eventually
    // used by the layout.

    // This method does not return anything, but instead installs
    // the properties "scale" and "space" on @layout directly.
    //
    // Make sure to call this methods before calling computeWindowSlots(),
    // as it depends on the scale property installed in @layout here.
    computeScaleAndSpace(layout) {
        let area = layout.area;

        let hspacing = (layout.maxColumns - 1) * this._columnSpacing;
        let vspacing = (layout.numRows - 1) * this._rowSpacing;

        let spacedWidth = area.width - hspacing;
        let spacedHeight = area.height - vspacing;

        let horizontalScale = spacedWidth / layout.gridWidth;
        let verticalScale = spacedHeight / layout.gridHeight;

        // Thumbnails should be less than 70% of the original size
        let scale = Math.min(
            horizontalScale, verticalScale, WINDOW_PREVIEW_MAXIMUM_SCALE);

        let scaledLayoutWidth = layout.gridWidth * scale + hspacing;
        let scaledLayoutHeight = layout.gridHeight * scale + vspacing;
        let space = (scaledLayoutWidth * scaledLayoutHeight) / (area.width * area.height);

        layout.scale = scale;
        layout.space = space;
    }

    computeWindowSlots(layout, area) {
        this._computeRowSizes(layout);

        let { rows, scale } = layout;

        let slots = [];

        // Do this in three parts.
        let heightWithoutSpacing = 0;
        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            heightWithoutSpacing += row.height;
        }

        let verticalSpacing = (rows.length - 1) * this._rowSpacing;
        let additionalVerticalScale = Math.min(1, (area.height - verticalSpacing) / heightWithoutSpacing);

        // keep track how much smaller the grid becomes due to scaling
        // so it can be centered again
        let compensation = 0;
        let y = 0;

        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];

            // If this window layout row doesn't fit in the actual
            // geometry, then apply an additional scale to it.
            let horizontalSpacing = (row.windows.length - 1) * this._columnSpacing;
            let widthWithoutSpacing = row.width - horizontalSpacing;
            let additionalHorizontalScale = Math.min(1, (area.width - horizontalSpacing) / widthWithoutSpacing);

            if (additionalHorizontalScale < additionalVerticalScale) {
                row.additionalScale = additionalHorizontalScale;
                // Only consider the scaling in addition to the vertical scaling for centering.
                compensation += (additionalVerticalScale - additionalHorizontalScale) * row.height;
            } else {
                row.additionalScale = additionalVerticalScale;
                // No compensation when scaling vertically since centering based on a too large
                // height would undo what vertical scaling is trying to achieve.
            }

            row.x = area.x + (Math.max(area.width - (widthWithoutSpacing * row.additionalScale + horizontalSpacing), 0) / 2);
            row.y = area.y + (Math.max(area.height - (heightWithoutSpacing + verticalSpacing), 0) / 2) + y;
            y += row.height * row.additionalScale + this._rowSpacing;
        }

        compensation /= 2;

        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            let x = row.x;
            for (let j = 0; j < row.windows.length; j++) {
                let window = row.windows[j];

                let s = scale * this._computeWindowScale(window) * row.additionalScale;
                let cellWidth = window.boundingBox.width * s;
                let cellHeight = window.boundingBox.height * s;

                s = Math.min(s, WINDOW_PREVIEW_MAXIMUM_SCALE);
                let cloneWidth = window.boundingBox.width * s;
                const cloneHeight = window.boundingBox.height * s;

                let cloneX = x + (cellWidth - cloneWidth) / 2;
                let cloneY = row.y + row.height * row.additionalScale - cellHeight + compensation;

                // Align with the pixel grid to prevent blurry windows at scale = 1
                cloneX = Math.floor(cloneX);
                cloneY = Math.floor(cloneY);

                slots.push([cloneX, cloneY, cloneWidth, cloneHeight, window]);
                x += cellWidth + this._columnSpacing;
            }
        }
        return slots;
    }
};

var UnalignedLayoutStrategy = class extends LayoutStrategy {
    _computeRowSizes(layout) {
        let { rows, scale } = layout;
        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            row.width = row.fullWidth * scale + (row.windows.length - 1) * this._columnSpacing;
            row.height = row.fullHeight * scale;
        }
    }

    _keepSameRow(row, window, width, idealRowWidth) {
        if (row.fullWidth + width <= idealRowWidth)
            return true;

        let oldRatio = row.fullWidth / idealRowWidth;
        let newRatio = (row.fullWidth + width) / idealRowWidth;

        if (Math.abs(1 - newRatio) < Math.abs(1 - oldRatio))
            return true;

        return false;
    }

    _sortRow(row) {
        // Sort windows horizontally to minimize travel distance.
        // This affects in what order the windows end up in a row.
        row.windows.sort((a, b) => a.windowCenter.x - b.windowCenter.x);
    }

    computeLayout(windows, layout) {
        let numRows = layout.numRows;

        let rows = [];
        let totalWidth = 0;
        for (let i = 0; i < windows.length; i++) {
            let window = windows[i];
            let s = this._computeWindowScale(window);
            totalWidth += window.boundingBox.width * s;
        }

        let idealRowWidth = totalWidth / numRows;

        // Sort windows vertically to minimize travel distance.
        // This affects what rows the windows get placed in.
        let sortedWindows = windows.slice();
        sortedWindows.sort((a, b) => a.windowCenter.y - b.windowCenter.y);

        let windowIdx = 0;
        for (let i = 0; i < numRows; i++) {
            let row = this._newRow();
            rows.push(row);

            for (; windowIdx < sortedWindows.length; windowIdx++) {
                let window = sortedWindows[windowIdx];
                let s = this._computeWindowScale(window);
                let width = window.boundingBox.width * s;
                let height = window.boundingBox.height * s;
                row.fullHeight = Math.max(row.fullHeight, height);

                // either new width is < idealWidth or new width is nearer from idealWidth then oldWidth
                if (this._keepSameRow(row, window, width, idealRowWidth) || (i == numRows - 1)) {
                    row.windows.push(window);
                    row.fullWidth += width;
                } else {
                    break;
                }
            }
        }

        let gridHeight = 0;
        let maxRow;
        for (let i = 0; i < numRows; i++) {
            let row = rows[i];
            this._sortRow(row);

            if (!maxRow || row.fullWidth > maxRow.fullWidth)
                maxRow = row;
            gridHeight += row.fullHeight;
        }

        layout.rows = rows;
        layout.maxColumns = maxRow.windows.length;
        layout.gridWidth = maxRow.fullWidth;
        layout.gridHeight = gridHeight;
    }
};

function padArea(area, padding) {
    return {
        x: area.x + padding.left,
        y: area.y + padding.top,
        width: area.width - padding.left - padding.right,
        height: area.height - padding.top - padding.bottom,
    };
}

function rectEqual(one, two) {
    if (one == two)
        return true;

    if (!one || !two)
        return false;

    return one.x == two.x &&
            one.y == two.y &&
            one.width == two.width &&
            one.height == two.height;
}

/**
 * @metaWorkspace: a #Meta.Workspace, or null
 */
var Workspace = GObject.registerClass(
class Workspace extends St.Widget {
    _init(metaWorkspace, monitorIndex) {
        super._init({ style_class: 'window-picker' });

        // When dragging a window, we use this slot for reserve space.
        this._reservedSlot = null;
        this._reservedSlotWindow = null;
        this.metaWorkspace = metaWorkspace;

        // The full geometry is the geometry we should try and position
        // windows for. The actual geometry we allocate may be less than
        // this, like if the workspace switcher is slid out.
        this._fullGeometry = null;

        // The actual geometry is the geometry we need to arrange windows
        // in. If this is a smaller area than the full geometry, we'll
        // do some simple aspect ratio like math to fit the layout calculated
        // for the full geometry into this area.
        this._actualGeometry = null;
        this._actualGeometryLater = 0;

        this._currentLayout = null;

        this.monitorIndex = monitorIndex;
        this._monitor = Main.layoutManager.monitors[this.monitorIndex];

        if (monitorIndex != Main.layoutManager.primaryIndex)
            this.add_style_class_name('external-monitor');
        this.set_size(0, 0);

        this._dropRect = new Clutter.Actor({ opacity: 0 });
        this._dropRect._delegate = this;

        this.add_actor(this._dropRect);

        this.connect('destroy', this._onDestroy.bind(this));

        let windows = global.get_window_actors().map(a => a.meta_window);
        windows = windows.filter(this._isMyWindow, this);

        // Create clones for windows that should be
        // visible in the Overview
        this._windows = [];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i]))
                this._addWindowClone(windows[i], true);
        }

        // Track window changes
        if (this.metaWorkspace) {
            this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                             this._windowAdded.bind(this));
            this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                               this._windowRemoved.bind(this));
        }
        this._windowEnteredMonitorId = global.display.connect('window-entered-monitor',
                                                              this._windowEnteredMonitor.bind(this));
        this._windowLeftMonitorId = global.display.connect('window-left-monitor',
                                                           this._windowLeftMonitor.bind(this));
        this._repositionWindowsId = 0;

        this.leavingOverview = false;

        this._positionWindowsFlags = 0;
        this._positionWindowsId = 0;
    }

    vfunc_map() {
        super.vfunc_map();
        this._syncActualGeometry();
    }

    vfunc_get_focus_chain() {
        return this.get_children().filter(c => c.visible).sort((a, b) => {
            if (a instanceof WindowPreview && b instanceof WindowPreview)
                return a.slotId - b.slotId;

            return 0;
        });
    }

    setFullGeometry(geom) {
        if (rectEqual(this._fullGeometry, geom))
            return;

        this._fullGeometry = geom;

        if (this.mapped)
            this._recalculateWindowPositions(WindowPositionFlags.NONE);
    }

    setActualGeometry(geom) {
        if (rectEqual(this._actualGeometry, geom))
            return;

        this._actualGeometry = geom;
        this._actualGeometryDirty = true;

        if (this.mapped)
            this._syncActualGeometry();
    }

    _syncActualGeometry() {
        if (this._actualGeometryLater || !this._actualGeometryDirty)
            return;
        if (!this._actualGeometry)
            return;

        this._actualGeometryLater = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this._actualGeometryLater = 0;
            if (!this.mapped)
                return false;

            let geom = this._actualGeometry;

            this._dropRect.set_position(geom.x, geom.y);
            this._dropRect.set_size(geom.width, geom.height);
            this._updateWindowPositions(Main.overview.animationInProgress ? WindowPositionFlags.ANIMATE : WindowPositionFlags.NONE);

            return false;
        });
    }

    _lookupIndex(metaWindow) {
        return this._windows.findIndex(w => w.metaWindow == metaWindow);
    }

    containsMetaWindow(metaWindow) {
        return this._lookupIndex(metaWindow) >= 0;
    }

    isEmpty() {
        return this._windows.length == 0;
    }

    setReservedSlot(metaWindow) {
        if (this._reservedSlotWindow == metaWindow)
            return;

        if (!metaWindow || this.containsMetaWindow(metaWindow)) {
            this._reservedSlotWindow = null;
            this._reservedSlot = null;
        } else {
            this._reservedSlotWindow = metaWindow;
            this._reservedSlot = this._windows[this._lookupIndex(metaWindow)];
        }

        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE);
    }

    _recalculateWindowPositions(flags) {
        this._positionWindowsFlags |= flags;

        if (this._positionWindowsId > 0)
            return;

        this._positionWindowsId = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this._realRecalculateWindowPositions(this._positionWindowsFlags);
            this._positionWindowsFlags = 0;
            this._positionWindowsId = 0;
            return false;
        });
    }

    _realRecalculateWindowPositions(flags) {
        if (this._repositionWindowsId > 0) {
            GLib.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        let clones = this._windows.slice();
        if (clones.length == 0)
            return;

        clones.sort((a, b) => {
            return a.metaWindow.get_stable_sequence() - b.metaWindow.get_stable_sequence();
        });

        if (this._reservedSlot)
            clones.push(this._reservedSlot);

        this._currentLayout = this._computeLayout(clones);
        this._updateWindowPositions(flags);
    }

    _updateWindowPositions(flags) {
        if (this._currentLayout == null) {
            this._recalculateWindowPositions(flags);
            return;
        }

        // We will reposition windows anyway when enter again overview or when ending the windows
        // animations with fade animation.
        // In this way we avoid unwanted animations of windows repositioning while
        // animating overview.
        if (this.leavingOverview || this._animatingWindowsFade)
            return;

        let initialPositioning = flags & WindowPositionFlags.INITIAL;
        let animate = flags & WindowPositionFlags.ANIMATE;

        let layout = this._currentLayout;
        let strategy = layout.strategy;

        let [, , padding] = this._getSpacingAndPadding();
        let area = padArea(this._actualGeometry, padding);
        let slots = strategy.computeWindowSlots(layout, area);

        let workspaceManager = global.workspace_manager;
        let currentWorkspace = workspaceManager.get_active_workspace();
        let isOnCurrentWorkspace = this.metaWorkspace == null || this.metaWorkspace == currentWorkspace;

        for (let i = 0; i < slots.length; i++) {
            let slot = slots[i];
            const [x, y, cellWidth, cellHeight, clone] = slot;

            clone.slotId = i;

            // Positioning a window currently being dragged must be avoided;
            // we'll just leave a blank spot in the layout for it.
            if (clone.inDrag)
                continue;

            const cloneWidth = cellWidth;
            const cloneHeight = cellHeight;

            if (!clone.positioned) {
                // This window appeared after the overview was already up
                // Grow the clone from the center of the slot
                clone.x = x + cloneWidth / 2;
                clone.y = y + cloneHeight / 2;
                clone.width = 0;
                clone.height = 0;
                clone.positioned = true;
            }

            if (animate && isOnCurrentWorkspace) {
                if (!clone.metaWindow.showing_on_its_workspace()) {
                    /* Hidden windows should fade in and grow
                     * therefore we need to resize them now so they
                     * can be scaled up later */
                    if (initialPositioning) {
                        clone.opacity = 0;
                        clone.x = x;
                        clone.y = y;
                        clone.width = cloneWidth;
                        clone.height = cloneHeight;
                    }

                    clone.ease({
                        opacity: 255,
                        mode: Clutter.AnimationMode.EASE_IN_QUAD,
                        duration: Overview.ANIMATION_TIME,
                    });
                }

                this._animateClone(clone, x, y, cloneWidth, cloneHeight);
            } else {
                // cancel any active tweens (otherwise they might override our changes)
                clone.remove_all_transitions();
                clone.set_position(x, y);
                clone.set_size(cloneWidth, cloneHeight);
                clone.set_opacity(255);
            }
        }
    }

    syncStacking(stackIndices) {
        let clones = this._windows.slice();
        clones.sort((a, b) => {
            let indexA = stackIndices[a.metaWindow.get_stable_sequence()];
            let indexB = stackIndices[b.metaWindow.get_stable_sequence()];
            return indexA - indexB;
        });

        for (let i = 0; i < clones.length; i++) {
            let clone = clones[i];
            if (i == 0) {
                clone.setStackAbove(this._dropRect);
            } else {
                let previousClone = clones[i - 1];
                clone.setStackAbove(previousClone);
            }
        }
    }

    _animateClone(clone, x, y, width, height) {
        clone.ease({
            x, y,
            width, height,
            duration: Overview.ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _delayedWindowRepositioning() {
        let [x, y] = global.get_pointer();

        let pointerHasMoved = this._cursorX != x && this._cursorY != y;
        let inWorkspace = this._fullGeometry.x < x && x < this._fullGeometry.x + this._fullGeometry.width &&
                           this._fullGeometry.y < y && y < this._fullGeometry.y + this._fullGeometry.height;

        if (pointerHasMoved && inWorkspace) {
            // store current cursor position
            this._cursorX = x;
            this._cursorY = y;
            return GLib.SOURCE_CONTINUE;
        }

        let actorUnderPointer = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i] == actorUnderPointer)
                return GLib.SOURCE_CONTINUE;
        }

        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE);
        this._repositionWindowsId = 0;
        return GLib.SOURCE_REMOVE;
    }

    _doRemoveWindow(metaWin) {
        let win = metaWin.get_compositor_private();

        let clone = this._removeWindowClone(metaWin);

        if (clone) {
            // If metaWin.get_compositor_private() returned non-NULL, that
            // means the window still exists (and is just being moved to
            // another workspace or something), so set its overviewHint
            // accordingly. (If it returned NULL, then the window is being
            // destroyed; we'd like to animate this, but it's too late at
            // this point.)
            if (win) {
                let [stageX, stageY] = clone.get_transformed_position();
                const [transformedWidth, transformedHeight] =
                    clone.get_transformed_size();

                win._overviewHint = {
                    x: stageX,
                    y: stageY,
                    width: transformedWidth,
                    height: transformedHeight,
                };
            }
            clone.destroy();
        }

        // We need to reposition the windows; to avoid shuffling windows
        // around while the user is interacting with the workspace, we delay
        // the positioning until the pointer remains still for at least 750 ms
        // or is moved outside the workspace

        // remove old handler
        if (this._repositionWindowsId > 0) {
            GLib.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        // setup new handler
        let [x, y] = global.get_pointer();
        this._cursorX = x;
        this._cursorY = y;

        this._currentLayout = null;
        this._repositionWindowsId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, WINDOW_REPOSITIONING_DELAY,
            this._delayedWindowRepositioning.bind(this));
        GLib.Source.set_name_by_id(this._repositionWindowsId, '[gnome-shell] this._delayedWindowRepositioning');
    }

    _doAddWindow(metaWin) {
        if (this.leavingOverview)
            return;

        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                if (metaWin.get_compositor_private() &&
                    metaWin.get_workspace() == this.metaWorkspace)
                    this._doAddWindow(metaWin);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._doAddWindow');
            return;
        }

        // We might have the window in our list already if it was on all workspaces and
        // now was moved to this workspace
        if (this._lookupIndex(metaWin) != -1)
            return;

        if (!this._isMyWindow(metaWin))
            return;

        if (!this._isOverviewWindow(metaWin)) {
            if (metaWin.get_transient_for() == null)
                return;

            // Let the top-most ancestor handle all transients
            let parent = metaWin.find_root_ancestor();
            let clone = this._windows.find(c => c.metaWindow == parent);

            // If no clone was found, the parent hasn't been created yet
            // and will take care of the dialog when added
            if (clone)
                clone.addDialog(metaWin);

            return;
        }

        let clone = this._addWindowClone(metaWin, false);

        if (win._overviewHint) {
            let x = win._overviewHint.x - this.x;
            let y = win._overviewHint.y - this.y;
            const width = win._overviewHint.width;
            const height = win._overviewHint.height;
            delete win._overviewHint;

            clone.positioned = true;

            clone.set_position(x, y);
            clone.set_size(width, height);
        }

        this._currentLayout = null;
        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE);
    }

    _windowAdded(metaWorkspace, metaWin) {
        this._doAddWindow(metaWin);
    }

    _windowRemoved(metaWorkspace, metaWin) {
        this._doRemoveWindow(metaWin);
    }

    _windowEnteredMonitor(metaDisplay, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex)
            this._doAddWindow(metaWin);
    }

    _windowLeftMonitor(metaDisplay, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex)
            this._doRemoveWindow(metaWin);
    }

    // check for maximized windows on the workspace
    hasMaximizedWindows() {
        for (let i = 0; i < this._windows.length; i++) {
            let metaWindow = this._windows[i].metaWindow;
            if (metaWindow.showing_on_its_workspace() &&
                metaWindow.maximized_horizontally &&
                metaWindow.maximized_vertically)
                return true;
        }
        return false;
    }

    fadeToOverview() {
        // We don't want to reposition windows while animating in this way.
        this._animatingWindowsFade = true;
        this._overviewShownId = Main.overview.connect('shown', this._doneShowingOverview.bind(this));
        if (this._windows.length == 0)
            return;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        if (this.metaWorkspace != null && this.metaWorkspace != activeWorkspace)
            return;

        // Special case maximized windows, since it doesn't make sense
        // to animate windows below in the stack
        let topMaximizedWindow;
        // It is ok to treat the case where there is no maximized
        // window as if the bottom-most window was maximized given that
        // it won't affect the result of the animation
        for (topMaximizedWindow = this._windows.length - 1; topMaximizedWindow > 0; topMaximizedWindow--) {
            let metaWindow = this._windows[topMaximizedWindow].metaWindow;
            if (metaWindow.maximized_horizontally && metaWindow.maximized_vertically)
                break;
        }

        let nTimeSlots = Math.min(WINDOW_ANIMATION_MAX_NUMBER_BLENDING + 1, this._windows.length - topMaximizedWindow);
        let windowBaseTime = Overview.ANIMATION_TIME / nTimeSlots;

        let topIndex = this._windows.length - 1;
        for (let i = 0; i < this._windows.length; i++) {
            if (i < topMaximizedWindow) {
                // below top-most maximized window, don't animate
                this._windows[i].hideOverlay(false);
                this._windows[i].opacity = 0;
            } else {
                let fromTop = topIndex - i;
                let time;
                if (fromTop < nTimeSlots) // animate top-most windows gradually
                    time = windowBaseTime * (nTimeSlots - fromTop);
                else
                    time = windowBaseTime;

                this._windows[i].opacity = 255;
                this._fadeWindow(i, time, 0);
            }
        }
    }

    fadeFromOverview() {
        this.leavingOverview = true;
        this._overviewHiddenId = Main.overview.connect('hidden', this._doneLeavingOverview.bind(this));
        if (this._windows.length == 0)
            return;

        for (let i = 0; i < this._windows.length; i++)
            this._windows[i].remove_all_transitions();

        if (this._repositionWindowsId > 0) {
            GLib.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        if (this.metaWorkspace != null && this.metaWorkspace != activeWorkspace)
            return;

        // Special case maximized windows, since it doesn't make sense
        // to animate windows below in the stack
        let topMaximizedWindow;
        // It is ok to treat the case where there is no maximized
        // window as if the bottom-most window was maximized given that
        // it won't affect the result of the animation
        for (topMaximizedWindow = this._windows.length - 1; topMaximizedWindow > 0; topMaximizedWindow--) {
            let metaWindow = this._windows[topMaximizedWindow].metaWindow;
            if (metaWindow.maximized_horizontally && metaWindow.maximized_vertically)
                break;
        }

        let nTimeSlots = Math.min(WINDOW_ANIMATION_MAX_NUMBER_BLENDING + 1, this._windows.length - topMaximizedWindow);
        let windowBaseTime = Overview.ANIMATION_TIME / nTimeSlots;

        let topIndex = this._windows.length - 1;
        for (let i = 0; i < this._windows.length; i++) {
            if (i < topMaximizedWindow) {
                // below top-most maximized window, don't animate
                this._windows[i].hideOverlay(false);
                this._windows[i].opacity = 0;
            } else {
                let fromTop = topIndex - i;
                let time;
                if (fromTop < nTimeSlots) // animate top-most windows gradually
                    time = windowBaseTime * (fromTop + 1);
                else
                    time = windowBaseTime * nTimeSlots;

                this._windows[i].opacity = 0;
                this._fadeWindow(i, time, 255);
            }
        }
    }

    _fadeWindow(index, duration, opacity) {
        let clone = this._windows[index];
        clone.hideOverlay(false);

        if (clone.metaWindow.showing_on_its_workspace()) {
            clone.x = clone.boundingBox.x;
            clone.y = clone.boundingBox.y;
            clone.width = clone.boundingBox.width;
            clone.height = clone.boundingBox.height;
            clone.ease({
                opacity,
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            // The window is hidden
            clone.opacity = 0;
        }
    }

    zoomToOverview() {
        // Position and scale the windows.
        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE | WindowPositionFlags.INITIAL);
    }

    zoomFromOverview() {
        let workspaceManager = global.workspace_manager;
        let currentWorkspace = workspaceManager.get_active_workspace();

        this.leavingOverview = true;

        for (let i = 0; i < this._windows.length; i++)
            this._windows[i].remove_all_transitions();

        if (this._repositionWindowsId > 0) {
            GLib.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }
        this._overviewHiddenId = Main.overview.connect('hidden', this._doneLeavingOverview.bind(this));

        if (this.metaWorkspace != null && this.metaWorkspace != currentWorkspace)
            return;

        // Position and scale the windows.
        for (let i = 0; i < this._windows.length; i++)
            this._zoomWindowFromOverview(i);
    }

    _zoomWindowFromOverview(index) {
        let clone = this._windows[index];
        clone.hideOverlay(false);

        if (clone.metaWindow.showing_on_its_workspace()) {
            clone.ease({
                x: clone.boundingBox.x,
                y: clone.boundingBox.y,
                width: clone.boundingBox.width,
                height: clone.boundingBox.height,
                opacity: 255,
                duration: Overview.ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            // The window is hidden, make it shrink and fade it out
            clone.ease({
                width: 0,
                height: 0,
                opacity: 0,
                duration: Overview.ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        }
    }

    _onDestroy() {
        if (this._overviewHiddenId) {
            Main.overview.disconnect(this._overviewHiddenId);
            this._overviewHiddenId = 0;
        }

        if (this.metaWorkspace) {
            this.metaWorkspace.disconnect(this._windowAddedId);
            this.metaWorkspace.disconnect(this._windowRemovedId);
        }
        global.display.disconnect(this._windowEnteredMonitorId);
        global.display.disconnect(this._windowLeftMonitorId);

        if (this._repositionWindowsId > 0) {
            GLib.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        if (this._positionWindowsId > 0) {
            Meta.later_remove(this._positionWindowsId);
            this._positionWindowsId = 0;
        }

        if (this._actualGeometryLater > 0) {
            Meta.later_remove(this._actualGeometryLater);
            this._actualGeometryLater = 0;
        }

        this._windows = [];
    }

    // Sets this.leavingOverview flag to false.
    _doneLeavingOverview() {
        this.leavingOverview = false;
    }

    _doneShowingOverview() {
        this._animatingWindowsFade = false;
        this._recalculateWindowPositions(WindowPositionFlags.INITIAL);
    }

    _isMyWindow(metaWindow) {
        const isOnWorkspace = this.metaWorkspace === null ||
            metaWindow.located_on_workspace(this.metaWorkspace);
        const isOnMonitor = metaWindow.get_monitor() === this.monitorIndex;

        return isOnWorkspace && isOnMonitor;
    }

    _isOverviewWindow(metaWindow) {
        return !metaWindow.skip_taskbar;
    }

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone(metaWindow, positioned) {
        let clone = new WindowPreview(metaWindow, this);
        clone.positioned = positioned;

        clone.connect('selected',
                      this._onCloneSelected.bind(this));
        clone.connect('drag-begin', () => {
            Main.overview.beginWindowDrag(metaWindow);
        });
        clone.connect('drag-cancelled', () => {
            Main.overview.cancelledWindowDrag(metaWindow);
        });
        clone.connect('drag-end', () => {
            Main.overview.endWindowDrag(metaWindow);
        });
        clone.connect('size-changed', () => {
            this._recalculateWindowPositions(WindowPositionFlags.NONE);
        });
        clone.connect('show-chrome', () => {
            let focus = global.stage.key_focus;
            if (focus == null || this.contains(focus))
                clone.grab_key_focus();

            this._windows.forEach(c => {
                if (c !== clone)
                    c.hideOverlay(true);
            });
        });
        clone.connect('destroy', () => {
            this._removeWindowClone(metaWindow);
        });

        this.add_child(clone);

        if (this._windows.length == 0)
            clone.setStackAbove(null);
        else
            clone.setStackAbove(this._windows[this._windows.length - 1]);

        this._windows.push(clone);

        return clone;
    }

    _removeWindowClone(metaWin) {
        // find the position of the window in our list
        let index = this._lookupIndex(metaWin);

        if (index == -1)
            return null;

        return this._windows.splice(index, 1).pop();
    }

    _isBetterLayout(oldLayout, newLayout) {
        if (oldLayout.scale === undefined)
            return true;

        let spacePower = (newLayout.space - oldLayout.space) * LAYOUT_SPACE_WEIGHT;
        let scalePower = (newLayout.scale - oldLayout.scale) * LAYOUT_SCALE_WEIGHT;

        if (newLayout.scale > oldLayout.scale && newLayout.space > oldLayout.space) {
            // Win win -- better scale and better space
            return true;
        } else if (newLayout.scale > oldLayout.scale && newLayout.space <= oldLayout.space) {
            // Keep new layout only if scale gain outweighs aspect space loss
            return scalePower > spacePower;
        } else if (newLayout.scale <= oldLayout.scale && newLayout.space > oldLayout.space) {
            // Keep new layout only if aspect space gain outweighs scale loss
            return spacePower > scalePower;
        } else {
            // Lose -- worse scale and space
            return false;
        }
    }

    _getBestLayout(windows, area, rowSpacing, columnSpacing) {
        // We look for the largest scale that allows us to fit the
        // largest row/tallest column on the workspace.

        let lastLayout = {};

        let strategy = new UnalignedLayoutStrategy(this._monitor, rowSpacing, columnSpacing);

        for (let numRows = 1; ; numRows++) {
            let numColumns = Math.ceil(windows.length / numRows);

            // If adding a new row does not change column count just stop
            // (for instance: 9 windows, with 3 rows -> 3 columns, 4 rows ->
            // 3 columns as well => just use 3 rows then)
            if (numColumns == lastLayout.numColumns)
                break;

            let layout = { area, strategy, numRows, numColumns };
            strategy.computeLayout(windows, layout);
            strategy.computeScaleAndSpace(layout);

            if (!this._isBetterLayout(lastLayout, layout))
                break;

            lastLayout = layout;
        }

        return lastLayout;
    }

    _getSpacingAndPadding() {
        let node = this.get_theme_node();

        // Window grid spacing
        let columnSpacing = node.get_length('-horizontal-spacing');
        let rowSpacing = node.get_length('-vertical-spacing');
        let padding = {
            left: node.get_padding(St.Side.LEFT),
            top: node.get_padding(St.Side.TOP),
            bottom: node.get_padding(St.Side.BOTTOM),
            right: node.get_padding(St.Side.RIGHT),
        };

        // All of the overlays have the same chrome sizes,
        // so just pick the first one.
        let clone = this._windows[0];
        let [topBorder, bottomBorder] = clone.chromeHeights();
        let [leftBorder, rightBorder] = clone.chromeWidths();

        rowSpacing += (topBorder + bottomBorder) / 2;
        columnSpacing += (rightBorder + leftBorder) / 2;
        padding.top += topBorder;
        padding.bottom += bottomBorder;
        padding.left += leftBorder;
        padding.right += rightBorder;

        return [rowSpacing, columnSpacing, padding];
    }

    _computeLayout(windows) {
        let [rowSpacing, columnSpacing, padding] = this._getSpacingAndPadding();
        let area = padArea(this._fullGeometry, padding);
        return this._getBestLayout(windows, area, rowSpacing, columnSpacing);
    }

    _onCloneSelected(clone, time) {
        let wsIndex;
        if (this.metaWorkspace)
            wsIndex = this.metaWorkspace.index();
        Main.activateWindow(clone.metaWindow, time, wsIndex);
    }

    // Draggable target interface
    handleDragOver(source, _actor, _x, _y, _time) {
        if (source.metaWindow && !this._isMyWindow(source.metaWindow))
            return DND.DragMotionResult.MOVE_DROP;
        if (source.app && source.app.can_open_new_window())
            return DND.DragMotionResult.COPY_DROP;
        if (!source.app && source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    }

    acceptDrop(source, actor, x, y, time) {
        let workspaceManager = global.workspace_manager;
        let workspaceIndex = this.metaWorkspace
            ? this.metaWorkspace.index()
            : workspaceManager.get_active_workspace_index();

        if (source.metaWindow) {
            const metaWindow = source.metaWindow;
            if (this._isMyWindow(metaWindow))
                return false;

            // Set a hint on the Mutter.Window so its initial position
            // in the new workspace will be correct
            metaWindow.get_compositor_private()._overviewHint = {
                x: actor.x,
                y: actor.y,
                width: actor.width,
                heigth: actor.height,
            };

            // We need to move the window before changing the workspace, because
            // the move itself could cause a workspace change if the window enters
            // the primary monitor
            if (metaWindow.get_monitor() != this.monitorIndex)
                metaWindow.move_to_monitor(this.monitorIndex);

            metaWindow.change_workspace_by_index(workspaceIndex, false);
            return true;
        } else if (source.app && source.app.can_open_new_window()) {
            if (source.animateLaunchAtPos)
                source.animateLaunchAtPos(actor.x, actor.y);

            source.app.open_new_window(workspaceIndex);
            return true;
        } else if (!source.app && source.shellWorkspaceLaunch) {
            // While unused in our own drag sources, shellWorkspaceLaunch allows
            // extensions to define custom actions for their drag sources.
            source.shellWorkspaceLaunch({ workspace: workspaceIndex,
                                          timestamp: time });
            return true;
        }

        return false;
    }
});
