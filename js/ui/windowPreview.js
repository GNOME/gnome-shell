// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WindowPreview */

const { Atk, Clutter, GLib, GObject,
        Graphene, Meta, Pango, Shell, St } = imports.gi;

const DND = imports.ui.dnd;

var WINDOW_DND_SIZE = 256;

var WINDOW_OVERLAY_IDLE_HIDE_TIMEOUT = 750;
var WINDOW_OVERLAY_FADE_TIME = 200;

var DRAGGING_WINDOW_OPACITY = 100;

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
        const scaleX = this._boundingBox.get_width() > 0
            ? box.get_width() / this._boundingBox.get_width()
            : 1;
        const scaleY = this._boundingBox.get_height() > 0
            ? box.get_height() / this._boundingBox.get_height()
            : 1;

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
                child.allocate_preferred_size(0, 0);
            }
        }
    }

    /**
     * addWindow:
     * @param {Meta.Window} window: the MetaWindow instance
     *
     * Creates a ClutterActor drawing the texture of @window and adds it
     * to the container. If @window is already part of the preview, this
     * function will do nothing.
     *
     * @returns {Clutter.Actor} The newly created actor drawing @window
     */
    addWindow(window) {
        const index = [...this._windows.values()].findIndex(info =>
            info.metaWindow === window);

        if (index !== -1)
            return null;

        const windowActor = window.get_compositor_private();
        const actor = new Clutter.Clone({ source: windowActor });

        this._windows.set(actor, {
            metaWindow: window,
            windowActor,
            sizeChangedId: window.connect('size-changed', () =>
                this._layoutChanged()),
            positionChangedId: window.connect('position-changed', () =>
                this._layoutChanged()),
            windowActorDestroyId: windowActor.connect('destroy', () =>
                actor.destroy()),
            destroyId: actor.connect('destroy', () =>
                this.removeWindow(window)),
        });

        this._container.add_child(actor);

        this._layoutChanged();

        return actor;
    }

    /**
     * removeWindow:
     * @param {Meta.Window} window: the window to remove from the preview
     *
     * Removes a MetaWindow @window from the preview which has been added
     * previously using addWindow(). If @window is not part of preview,
     * this function will do nothing.
     */
    removeWindow(window) {
        const entry = [...this._windows].find(
            ([, i]) => i.metaWindow === window);

        if (!entry)
            return;

        const [actor, windowInfo] = entry;

        windowInfo.metaWindow.disconnect(windowInfo.sizeChangedId);
        windowInfo.metaWindow.disconnect(windowInfo.positionChangedId);
        windowInfo.windowActor.disconnect(windowInfo.windowActorDestroyId);
        actor.disconnect(windowInfo.destroyId);

        this._windows.delete(actor);
        this._container.remove_child(actor);

        this._layoutChanged();
    }

    /**
     * getWindows:
     *
     * Gets an array of all MetaWindows that were added to the layout
     * using addWindow(), ordered by the insertion order.
     *
     * @returns {Array} An array including all windows
     */
    getWindows() {
        return [...this._windows.values()].map(i => i.metaWindow);
    }

    // eslint-disable-next-line camelcase
    get bounding_box() {
        return this._boundingBox;
    }
});

var WindowPreview = GObject.registerClass({
    Properties: {
        'overlay-enabled': GObject.ParamSpec.boolean(
            'overlay-enabled', 'overlay-enabled', 'overlay-enabled',
            GObject.ParamFlags.READWRITE,
            true),
    },
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

        this._stackAbove = null;

        this._windowContainer.layout_manager.connect(
            'notify::bounding-box', layout => {
                // A bounding box of 0x0 means all windows were removed
                if (layout.bounding_box.get_area() > 0)
                    this.emit('size-changed');
            });

        this._windowDestroyId =
            this._windowActor.connect('destroy', () => this.destroy());

        this._updateAttachedDialogs();

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
        this._overlayEnabled = true;
        this._closeRequested = false;
        this._idleHideOverlayId = 0;

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
        this._borderCenter = new Clutter.Actor();
        this._border.bind_property('visible', this._borderCenter, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._borderCenterConstraint = new Clutter.BindConstraint({
            source: this._windowContainer,
            coordinate: Clutter.BindCoordinate.SIZE,
        });
        this._borderCenter.add_constraint(this._borderCenterConstraint);
        this._borderCenter.add_constraint(new Clutter.AlignConstraint({
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
        this._title.add_constraint(new Clutter.BindConstraint({
            source: this._borderCenter,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: this._borderCenter,
            align_axis: Clutter.AlignAxis.X_AXIS,
            factor: 0.5,
        }));
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: this._borderCenter,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: 0.5 }),
            factor: 1,
        }));
        this._title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        this.label_actor = this._title;
        this._updateCaptionId = this.metaWindow.connect('notify::title', () => {
            this._title.text = this._getCaption();
        });

        const layout = Meta.prefs_get_button_layout();
        this._closeButtonSide =
            layout.left_buttons.includes(Meta.ButtonFunction.CLOSE)
                ? St.Side.LEFT : St.Side.RIGHT;

        this._closeButton = new St.Button({
            visible: false,
            style_class: 'window-close',
            child: new St.Icon({ icon_name: 'window-close-symbolic' }),
        });
        this._closeButton.add_constraint(new Clutter.BindConstraint({
            source: this._borderCenter,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: this._borderCenter,
            align_axis: Clutter.AlignAxis.X_AXIS,
            pivot_point: new Graphene.Point({ x: 0.5, y: -1 }),
            factor: this._closeButtonSide === St.Side.LEFT ? 0 : 1,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: this._borderCenter,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: 0.5 }),
            factor: 0,
        }));
        this._closeButton.connect('clicked', () => this._deleteAll());

        this.add_child(this._borderCenter);
        this.add_child(this._border);
        this.add_child(this._title);
        this.add_child(this._closeButton);

        this.connect('notify::realized', () => {
            if (!this.realized)
                return;

            this._border.ensure_style();
            this._title.ensure_style();
        });
    }

    vfunc_get_preferred_width(forHeight) {
        const themeNode = this.get_theme_node();

        // Only include window previews in size request, not chrome
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

        // Increase the size of the border actor so the border outlines
        // the bounding box
        this._borderConstraint.offset = this._borderSize * 2;
        this._borderCenterConstraint.offset = this._borderSize;
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
        const [, closeButtonHeight] = this._closeButton.get_preferred_height(-1);
        const [, titleHeight] = this._title.get_preferred_height(-1);

        const topOversize = (this._borderSize / 2) + (closeButtonHeight / 2);
        const bottomOversize = Math.max(
            this._borderSize,
            (titleHeight / 2) + (this._borderSize / 2));

        return [topOversize, bottomOversize];
    }

    chromeWidths() {
        const [, closeButtonWidth] = this._closeButton.get_preferred_width(-1);

        const leftOversize = this._closeButtonSide === St.Side.LEFT
            ? (this._borderSize / 2) + (closeButtonWidth / 2)
            : this._borderSize;
        const rightOversize = this._closeButtonSide === St.Side.LEFT
            ? this._borderSize
            : (this._borderSize / 2) + (closeButtonWidth / 2);

        return [leftOversize, rightOversize];
    }

    showOverlay(animate) {
        if (!this._overlayEnabled)
            return;

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
        const clone = this._windowContainer.layout_manager.addWindow(metaWindow);
        if (!clone)
            return;

        // We expect this to be used for all interaction rather than
        // the ClutterClone; as the former is reactive and the latter
        // is not, this just works for most cases. However, for DND all
        // actors are picked, so DND operations would operate on the clone.
        // To avoid this, we hide it from pick.
        Shell.util_set_hidden_from_pick(clone, true);
    }

    vfunc_has_overlaps() {
        return this._hasAttachedDialogs();
    }

    _deleteAll() {
        const windows = this._windowContainer.layout_manager.getWindows();

        // Delete all windows, starting from the bottom-most (most-modal) one
        for (const window of windows.reverse())
            window.delete(global.get_current_time());

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

    // eslint-disable-next-line camelcase
    get overlay_enabled() {
        return this._overlayEnabled;
    }

    // eslint-disable-next-line camelcase
    set overlay_enabled(enabled) {
        if (this._overlayEnabled === enabled)
            return;

        this._overlayEnabled = enabled;
        this.notify('overlay-enabled');

        if (!enabled)
            this.hideOverlay(false);
        else if (this['has-pointer'] || global.stage.key_focus === this)
            this.showOverlay(true);
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
