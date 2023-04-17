// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WindowPreview */

const {
    Atk, Clutter, GLib, GObject, Graphene, Meta, Pango, Shell, St,
} = imports.gi;

const DND = imports.ui.dnd;
const OverviewControls = imports.ui.overviewControls;

var WINDOW_DND_SIZE = 256;

var WINDOW_OVERLAY_IDLE_HIDE_TIMEOUT = 750;
var WINDOW_OVERLAY_FADE_TIME = 200;

var WINDOW_SCALE_TIME = 200;
var WINDOW_ACTIVE_SIZE_INC = 5; // in each direction

var DRAGGING_WINDOW_OPACITY = 100;

const ICON_SIZE = 64;
const ICON_OVERLAP = 0.7;

const ICON_TITLE_SPACING = 6;

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
}, class WindowPreview extends Shell.WindowPreview {
    _init(metaWindow, workspace, overviewAdjustment) {
        this.metaWindow = metaWindow;
        this.metaWindow._delegate = this;
        this._windowActor = metaWindow.get_compositor_private();
        this._workspace = workspace;
        this._overviewAdjustment = overviewAdjustment;

        super._init({
            reactive: true,
            can_focus: true,
            accessible_role: Atk.Role.PUSH_BUTTON,
            offscreen_redirect: Clutter.OffscreenRedirect.AUTOMATIC_FOR_OPACITY,
        });

        const windowContainer = new Clutter.Actor({
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });
        this.window_container = windowContainer;

        windowContainer.connect('notify::scale-x',
            () => this._adjustOverlayOffsets());
        // gjs currently can't handle setting an actors layout manager during
        // the initialization of the actor if that layout manager keeps track
        // of its container, so set the layout manager after creating the
        // container
        windowContainer.layout_manager = new Shell.WindowPreviewLayout();
        this.add_child(windowContainer);

        this._addWindow(metaWindow);

        this._delegate = this;

        this._stackAbove = null;

        this._cachedBoundingBox = {
            x: windowContainer.layout_manager.bounding_box.x1,
            y: windowContainer.layout_manager.bounding_box.y1,
            width: windowContainer.layout_manager.bounding_box.get_width(),
            height: windowContainer.layout_manager.bounding_box.get_height(),
        };

        windowContainer.layout_manager.connect(
            'notify::bounding-box', layout => {
                this._cachedBoundingBox = {
                    x: layout.bounding_box.x1,
                    y: layout.bounding_box.y1,
                    width: layout.bounding_box.get_width(),
                    height: layout.bounding_box.get_height(),
                };

                // A bounding box of 0x0 means all windows were removed
                if (layout.bounding_box.get_area() > 0)
                    this.emit('size-changed');
            });

        this._windowActor.connectObject('destroy', () => this.destroy(), this);

        this._updateAttachedDialogs();

        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', () => this._activate());
        clickAction.connect('long-press', this._onLongPress.bind(this));
        this.add_action(clickAction);
        this.connect('destroy', this._onDestroy.bind(this));

        this._draggable = DND.makeDraggable(this, {
            restoreOnSuccess: true,
            manualMode: true,
            dragActorMaxSize: WINDOW_DND_SIZE,
            dragActorOpacity: DRAGGING_WINDOW_OPACITY,
        });
        this._draggable.connect('drag-begin', this._onDragBegin.bind(this));
        this._draggable.connect('drag-cancelled', this._onDragCancelled.bind(this));
        this._draggable.connect('drag-end', this._onDragEnd.bind(this));
        this.inDrag = false;

        this._selected = false;
        this._overlayEnabled = true;
        this._overlayShown = false;
        this._closeRequested = false;
        this._idleHideOverlayId = 0;

        const tracker = Shell.WindowTracker.get_default();
        const app = tracker.get_window_app(this.metaWindow);
        this._icon = app.create_icon_texture(ICON_SIZE);
        this._icon.add_style_class_name('icon-dropshadow');
        this._icon.set({
            reactive: true,
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });
        this._icon.add_constraint(new Clutter.BindConstraint({
            source: windowContainer,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._icon.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.X_AXIS,
            factor: 0.5,
        }));
        this._icon.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: ICON_OVERLAP }),
            factor: 1,
        }));

        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        this._title = new St.Label({
            visible: false,
            style_class: 'window-caption',
            text: this._getCaption(),
            reactive: true,
        });
        this._title.clutter_text.single_line_mode = true;
        this._title.add_constraint(new Clutter.BindConstraint({
            source: windowContainer,
            coordinate: Clutter.BindCoordinate.X,
        }));
        const iconBottomOverlap = ICON_SIZE * (1 - ICON_OVERLAP);
        this._title.add_constraint(new Clutter.BindConstraint({
            source: windowContainer,
            coordinate: Clutter.BindCoordinate.Y,
            offset: scaleFactor * (iconBottomOverlap + ICON_TITLE_SPACING),
        }));
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.X_AXIS,
            factor: 0.5,
        }));
        this._title.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: 0 }),
            factor: 1,
        }));
        this._title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        this.label_actor = this._title;
        this.metaWindow.connectObject(
            'notify::title', () => (this._title.text = this._getCaption()),
            this);

        const layout = Meta.prefs_get_button_layout();
        this._closeButtonSide =
            layout.left_buttons.includes(Meta.ButtonFunction.CLOSE)
                ? St.Side.LEFT : St.Side.RIGHT;

        this._closeButton = new St.Button({
            visible: false,
            style_class: 'window-close',
            icon_name: 'preview-close-symbolic',
        });
        this._closeButton.add_constraint(new Clutter.BindConstraint({
            source: windowContainer,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.X_AXIS,
            pivot_point: new Graphene.Point({ x: 0.5, y: -1 }),
            factor: this._closeButtonSide === St.Side.LEFT ? 0 : 1,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: windowContainer,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: 0.5 }),
            factor: 0,
        }));
        this._closeButton.connect('clicked', () => this._deleteAll());

        this.add_child(this._title);
        this.add_child(this._icon);
        this.add_child(this._closeButton);

        this._overviewAdjustment.connectObject(
            'notify::value', () => this._updateIconScale(), this);
        this._updateIconScale();

        this.connect('notify::realized', () => {
            if (!this.realized)
                return;

            this._title.ensure_style();
            this._icon.ensure_style();
        });
    }

    _updateIconScale() {
        const { ControlsState } = OverviewControls;
        const { currentState, initialState, finalState } =
            this._overviewAdjustment.getStateTransitionParams();
        const visible =
            initialState === ControlsState.WINDOW_PICKER ||
            finalState === ControlsState.WINDOW_PICKER;
        const scale = visible
            ? 1 - Math.abs(ControlsState.WINDOW_PICKER - currentState) : 0;

        this._icon.set({
            scale_x: scale,
            scale_y: scale,
        });
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

    overlapHeights() {
        const [, titleHeight] = this._title.get_preferred_height(-1);

        const topOverlap = 0;
        const bottomOverlap = ICON_TITLE_SPACING + titleHeight;

        return [topOverlap, bottomOverlap];
    }

    chromeHeights() {
        const [, closeButtonHeight] = this._closeButton.get_preferred_height(-1);
        const [, iconHeight] = this._icon.get_preferred_height(-1);
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        const activeExtraSize = WINDOW_ACTIVE_SIZE_INC * scaleFactor;

        const topOversize = closeButtonHeight / 2;
        const bottomOversize = (1 - ICON_OVERLAP) * iconHeight;

        return [
            topOversize + activeExtraSize,
            bottomOversize + activeExtraSize,
        ];
    }

    chromeWidths() {
        const [, closeButtonWidth] = this._closeButton.get_preferred_width(-1);
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        const activeExtraSize = WINDOW_ACTIVE_SIZE_INC * scaleFactor;

        const leftOversize = this._closeButtonSide === St.Side.LEFT
            ? closeButtonWidth / 2
            : 0;
        const rightOversize = this._closeButtonSide === St.Side.LEFT
            ? 0
            : closeButtonWidth / 2;

        return [
            leftOversize + activeExtraSize,
            rightOversize  + activeExtraSize,
        ];
    }

    showOverlay(animate) {
        if (!this._overlayEnabled)
            return;

        if (this._overlayShown)
            return;

        this._overlayShown = true;
        this._restack();

        // If we're supposed to animate and an animation in our direction
        // is already happening, let that one continue
        const ongoingTransition = this._title.get_transition('opacity');
        if (animate &&
            ongoingTransition &&
            ongoingTransition.get_interval().peek_final_value() === 255)
            return;

        const toShow = this._windowCanClose()
            ? [this._title, this._closeButton]
            : [this._title];

        toShow.forEach(a => {
            a.opacity = 0;
            a.show();
            a.ease({
                opacity: 255,
                duration: animate ? WINDOW_OVERLAY_FADE_TIME : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        });

        const [width, height] = this.window_container.get_size();
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        const activeExtraSize = WINDOW_ACTIVE_SIZE_INC * 2 * scaleFactor;
        const origSize = Math.max(width, height);
        const scale = (origSize + activeExtraSize) / origSize;

        this.window_container.ease({
            scale_x: scale,
            scale_y: scale,
            duration: animate ? WINDOW_SCALE_TIME : 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this.emit('show-chrome');
    }

    hideOverlay(animate) {
        if (!this._overlayShown)
            return;

        this._overlayShown = false;
        this._restack();

        // If we're supposed to animate and an animation in our direction
        // is already happening, let that one continue
        const ongoingTransition = this._title.get_transition('opacity');
        if (animate &&
            ongoingTransition &&
            ongoingTransition.get_interval().peek_final_value() === 0)
            return;

        [this._title, this._closeButton].forEach(a => {
            a.opacity = 255;
            a.ease({
                opacity: 0,
                duration: animate ? WINDOW_OVERLAY_FADE_TIME : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => a.hide(),
            });
        });

        this.window_container.ease({
            scale_x: 1,
            scale_y: 1,
            duration: animate ? WINDOW_SCALE_TIME : 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _adjustOverlayOffsets() {
        // Assume that scale-x and scale-y update always set
        // in lock-step; that allows us to not use separate
        // handlers for horizontal and vertical offsets
        const previewScale = this.window_container.scale_x;
        const [previewWidth, previewHeight] =
            this.window_container.allocation.get_size();

        const heightIncrease =
            Math.floor(previewHeight * (previewScale - 1) / 2);
        const widthIncrease =
            Math.floor(previewWidth * (previewScale - 1) / 2);

        const closeAlign = this._closeButtonSide === St.Side.LEFT ? -1 : 1;

        this._icon.translation_y = heightIncrease;
        this._title.translation_y = heightIncrease;
        this._closeButton.set({
            translation_x: closeAlign * widthIncrease,
            translation_y: -heightIncrease,
        });
    }

    _addWindow(metaWindow) {
        const clone = this.window_container.layout_manager.add_window(metaWindow);
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
        return this._hasAttachedDialogs() ||
            this._icon.visible ||
            this._closeButton.visible;
    }

    _deleteAll() {
        const windows = this.window_container.layout_manager.get_windows();

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
        return this.window_container.layout_manager.get_windows().length > 1;
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
        return { ...this._cachedBoundingBox };
    }

    get windowCenter() {
        return {
            x: this._cachedBoundingBox.x + this._cachedBoundingBox.width / 2,
            y: this._cachedBoundingBox.y + this._cachedBoundingBox.height / 2,
        };
    }

    get overlayEnabled() {
        return this._overlayEnabled;
    }

    set overlayEnabled(enabled) {
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
        this.metaWindow._delegate = null;
        this._delegate = null;
        this._destroyed = true;

        if (this._longPressLater) {
            const laters = global.compositor.get_laters();
            laters.remove(this._longPressLater);
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
        if (this._destroyed)
            return super.vfunc_leave_event(crossingEvent);

        if ((crossingEvent.flags & Clutter.EventFlags.FLAG_GRAB_NOTIFY) !== 0 &&
            global.stage.get_grab_actor() === this._closeButton)
            return super.vfunc_leave_event(crossingEvent);

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

        if (global.stage.get_grab_actor() !== this._closeButton)
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
            const laters = global.compositor.get_laters();
            this._longPressLater = laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                delete this._longPressLater;
                if (this._selected) {
                    this._selected = false;
                    return;
                }
                let [x, y] = action.get_coords();
                action.release();
                this._draggable.startDrag(x, y, global.get_current_time(), this._dragTouchSequence, event.get_device());
            });
        } else {
            this.showOverlay(true);
        }
        return true;
    }

    _restack() {
        // We may not have a parent if DnD completed successfully, in
        // which case our clone will shortly be destroyed and replaced
        // with a new one on the target workspace.
        const parent = this.get_parent();
        if (parent !== null) {
            if (this._overlayShown)
                parent.set_child_above_sibling(this, null);
            else if (this._stackAbove === null)
                parent.set_child_below_sibling(this, null);
            else if (!this._stackAbove._overlayShown)
                parent.set_child_above_sibling(this, this._stackAbove);
        }
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

        this._restack();

        if (this['has-pointer'])
            this.showOverlay(true);

        this.emit('drag-end');
    }
});
