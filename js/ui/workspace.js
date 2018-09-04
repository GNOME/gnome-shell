// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Signals = imports.signals;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Tweener = imports.ui.tweener;
const WindowManager = imports.ui.windowManager;

var WINDOW_DND_SIZE = 256;

var WINDOW_CLONE_MAXIMUM_SCALE = 1.0;

var CLOSE_BUTTON_FADE_TIME = 0.1;

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

var WindowCloneLayout = new Lang.Class({
    Name: 'WindowCloneLayout',
    Extends: Clutter.LayoutManager,

    _init(boundingBox) {
        this.parent();

        this._boundingBox = boundingBox;
    },

    get boundingBox() {
        return this._boundingBox;
    },

    set boundingBox(b) {
        this._boundingBox = b;
        this.layout_changed();
    },

    _makeBoxForWindow(window) {
        // We need to adjust the position of the actor because of the
        // consequences of invisible borders -- in reality, the texture
        // has an extra set of "padding" around it that we need to trim
        // down.

        // The outer rect (from which we compute the bounding box)
        // paradoxically is the smaller rectangle, containing the positions
        // of the visible frame. The input rect contains everything,
        // including the invisible border padding.
        let inputRect = window.get_buffer_rect();

        let box = new Clutter.ActorBox();

        box.set_origin(inputRect.x - this._boundingBox.x,
                       inputRect.y - this._boundingBox.y);
        box.set_size(inputRect.width, inputRect.height);

        return box;
    },

    vfunc_get_preferred_height(container, forWidth) {
        return [this._boundingBox.height, this._boundingBox.height];
    },

    vfunc_get_preferred_width(container, forHeight) {
        return [this._boundingBox.width, this._boundingBox.width];
    },

    vfunc_allocate(container, box, flags) {
        container.get_children().forEach(child => {
            let realWindow;
            if (child == container._delegate._windowClone)
                realWindow = container._delegate.realWindow;
            else
                realWindow = child.source;

            child.allocate(this._makeBoxForWindow(realWindow.meta_window),
                           flags);
        });
    }
});

var WindowClone = new Lang.Class({
    Name: 'WindowClone',

    _init(realWindow, workspace) {
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;
        this.metaWindow._delegate = this;
        this._workspace = workspace;

        this._windowClone = new Clutter.Clone({ source: realWindow });
        // We expect this.actor to be used for all interaction rather than
        // this._windowClone; as the former is reactive and the latter
        // is not, this just works for most cases. However, for DND all
        // actors are picked, so DND operations would operate on the clone.
        // To avoid this, we hide it from pick.
        Shell.util_set_hidden_from_pick(this._windowClone, true);

        // The MetaShapedTexture that we clone has a size that includes
        // the invisible border; this is inconvenient; rather than trying
        // to compensate all over the place we insert a ClutterActor into
        // the hierarchy that is sized to only the visible portion.
        this.actor = new St.Widget({ reactive: true,
                                     can_focus: true,
                                     accessible_role: Atk.Role.PUSH_BUTTON,
                                     layout_manager: new WindowCloneLayout() });

        this.actor.add_child(this._windowClone);

        this.actor._delegate = this;

        this.slotId = 0;
        this._slot = [0, 0, 0, 0];
        this._dragSlot = [0, 0, 0, 0];
        this._stackAbove = null;

        this._windowClone._sizeChangedId = this.metaWindow.connect('size-changed',
            this._onMetaWindowSizeChanged.bind(this));
        this._windowClone._posChangedId = this.metaWindow.connect('position-changed',
            this._computeBoundingBox.bind(this));
        this._windowClone._destroyId =
            this.realWindow.connect('destroy', () => {
                // First destroy the clone and then destroy everything
                // This will ensure that we never see it in the
                // _disconnectSignals loop
                this._windowClone.destroy();
                this.destroy();
            });

        this._updateAttachedDialogs();
        this._computeBoundingBox();
        this.actor.x = this._boundingBox.x;
        this.actor.y = this._boundingBox.y;

        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', this._onClicked.bind(this));
        clickAction.connect('long-press', this._onLongPress.bind(this));
        this.actor.add_action(clickAction);
        this.actor.connect('destroy', this._onDestroy.bind(this));
        this.actor.connect('key-press-event', this._onKeyPress.bind(this));

        this.actor.connect('enter-event', () => { this.emit('show-chrome'); });
        this.actor.connect('key-focus-in', () => { this.emit('show-chrome'); });

        this.actor.connect('leave-event', () => { this.emit('hide-chrome'); });
        this.actor.connect('key-focus-out', () => { this.emit('hide-chrome'); });

        this._draggable = DND.makeDraggable(this.actor,
                                            { restoreOnSuccess: true,
                                              manualMode: true,
                                              dragActorMaxSize: WINDOW_DND_SIZE,
                                              dragActorOpacity: DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', this._onDragBegin.bind(this));
        this._draggable.connect('drag-cancelled', this._onDragCancelled.bind(this));
        this._draggable.connect('drag-end', this._onDragEnd.bind(this));
        this.inDrag = false;

        this._selected = false;
    },

    set slot(slot) {
        this._slot = slot;
    },

    get slot() {
        if (this.inDrag)
            return this._dragSlot;
        else
            return this._slot;
    },

    deleteAll() {
        // Delete all windows, starting from the bottom-most (most-modal) one

        let windows = this.actor.get_children();
        for (let i = windows.length - 1; i >= 1; i--) {
            let realWindow = windows[i].source;
            let metaWindow = realWindow.meta_window;

            metaWindow.delete(global.get_current_time());
        }

        this.metaWindow.delete(global.get_current_time());
    },

    addAttachedDialog(win) {
        this._doAddAttachedDialog(win, win.get_compositor_private());
        this._onMetaWindowSizeChanged();
    },

    hasAttachedDialogs() {
        return this.actor.get_n_children() > 1;
    },

    _doAddAttachedDialog(metaWin, realWin) {
        let clone = new Clutter.Clone({ source: realWin });
        clone._sizeChangedId = metaWin.connect('size-changed',
            this._onMetaWindowSizeChanged.bind(this));
        clone._posChangedId = metaWin.connect('position-changed',
            this._onMetaWindowSizeChanged.bind(this));
        clone._destroyId = realWin.connect('destroy', () => {
            clone.destroy();

            this._onMetaWindowSizeChanged();
        });
        this.actor.add_child(clone);
    },

    _updateAttachedDialogs() {
        let iter = win => {
            let actor = win.get_compositor_private();

            if (!actor)
                return false;
            if (!win.is_attached_dialog())
                return false;

            this._doAddAttachedDialog(win, actor);
            win.foreach_transient(iter);
            return true;
        };
        this.metaWindow.foreach_transient(iter);
    },

    get boundingBox() {
        return this._boundingBox;
    },

    get width() {
        return this._boundingBox.width;
    },

    get height() {
        return this._boundingBox.height;
    },

    getOriginalPosition() {
        return [this._boundingBox.x, this._boundingBox.y];
    },

    _computeBoundingBox() {
        let rect = this.metaWindow.get_frame_rect();

        this.actor.get_children().forEach(child => {
            let realWindow;
            if (child == this._windowClone)
                realWindow = this.realWindow;
            else
                realWindow = child.source;

            let metaWindow = realWindow.meta_window;
            rect = rect.union(metaWindow.get_frame_rect());
        });

        // Convert from a MetaRectangle to a native JS object
        this._boundingBox = { x: rect.x, y: rect.y, width: rect.width, height: rect.height };
        this.actor.layout_manager.boundingBox = rect;
    },

    // Find the actor just below us, respecting reparenting done
    // by DND code
    getActualStackAbove() {
        if (this._stackAbove == null)
            return null;

        if (this.inDrag) {
            if (this._stackAbove._delegate)
                return this._stackAbove._delegate.getActualStackAbove();
            else
                return null;
        } else {
            return this._stackAbove;
        }
    },

    setStackAbove(actor) {
        this._stackAbove = actor;
        if (this.inDrag)
            // We'll fix up the stack after the drag
            return;

        let actualAbove = this.getActualStackAbove();
        if (actualAbove == null)
            this.actor.lower_bottom();
        else
            this.actor.raise(actualAbove);
    },

    destroy() {
        this.actor.destroy();
    },

    _disconnectSignals() {
        this.actor.get_children().forEach(child => {
            let realWindow;
            if (child == this._windowClone)
                realWindow = this.realWindow;
            else
                realWindow = child.source;

            realWindow.meta_window.disconnect(child._sizeChangedId);
            realWindow.meta_window.disconnect(child._posChangedId);
            realWindow.disconnect(child._destroyId);
        });
    },

    _onMetaWindowSizeChanged() {
        this._computeBoundingBox();
        this.emit('size-changed');
    },

    _onDestroy() {
        this._disconnectSignals();

        this.metaWindow._delegate = null;
        this.actor._delegate = null;

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }

        this.disconnectAll();
    },

    _activate() {
        this._selected = true;
        this.emit('selected', global.get_current_time());
    },

    _onKeyPress(actor, event) {
        let symbol = event.get_key_symbol();
        let isEnter = (symbol == Clutter.KEY_Return || symbol == Clutter.KEY_KP_Enter);
        if (isEnter) {
            this._activate();
            return true;
        }

        return false;
    },

    _onClicked(action, actor) {
        this._activate();
    },

    _onLongPress(action, actor, state) {
        // Take advantage of the Clutter policy to consider
        // a long-press canceled when the pointer movement
        // exceeds dnd-drag-threshold to manually start the drag
        if (state == Clutter.LongPressState.CANCEL) {
            let event = Clutter.get_current_event();
            this._dragTouchSequence = event.get_event_sequence();

            // A click cancels a long-press before any click handler is
            // run - make sure to not start a drag in that case
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                if (this._selected)
                    return;
                let [x, y] = action.get_coords();
                action.release();
                this._draggable.startDrag(x, y, global.get_current_time(), this._dragTouchSequence);
            });
        } else {
            this.emit('show-chrome');
        }
        return true;
    },

    _onDragBegin(draggable, time) {
        this._dragSlot = this._slot;
        [this.dragOrigX, this.dragOrigY] = this.actor.get_position();
        this.dragOrigScale = this.actor.scale_x;
        this.inDrag = true;
        this.emit('drag-begin');
    },

    handleDragOver(source, actor, x, y, time) {
        return this._workspace.handleDragOver(source, actor, x, y, time);
    },

    acceptDrop(source, actor, x, y, time) {
        this._workspace.acceptDrop(source, actor, x, y, time);
    },

    _onDragCancelled(draggable, time) {
        this.emit('drag-cancelled');
    },

    _onDragEnd(draggable, time, snapback) {
        this.inDrag = false;

        // We may not have a parent if DnD completed successfully, in
        // which case our clone will shortly be destroyed and replaced
        // with a new one on the target workspace.
        if (this.actor.get_parent() != null) {
            if (this._stackAbove == null)
                this.actor.lower_bottom();
            else
                this.actor.raise(this._stackAbove);
        }


        this.emit('drag-end');
    }
});
Signals.addSignalMethods(WindowClone.prototype);


/**
 * @windowClone: Corresponding window clone
 * @parentActor: The actor which will be the parent of all overlay items
 *               such as app icon and window caption
 */
var WindowOverlay = new Lang.Class({
    Name: 'WindowOverlay',

    _init(windowClone, parentActor) {
        let metaWindow = windowClone.metaWindow;

        this._windowClone = windowClone;
        this._parentActor = parentActor;
        this._hidden = false;

        this.borderSize = 0;
        this.border = new St.Bin({ style_class: 'window-clone-border' });

        let title = new St.Label({ style_class: 'window-caption',
                                   text: metaWindow.title });
        title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        windowClone.actor.label_actor = title;

        this._updateCaptionId = metaWindow.connect('notify::title', w => {
            this.title.text = w.title;
            this.relayout(false);
        });

        let button = new St.Button({ style_class: 'window-close' });
        button._overlap = 0;

        this._idleToggleCloseId = 0;
        button.connect('clicked', this._closeWindow.bind(this));

        windowClone.actor.connect('destroy', this._onDestroy.bind(this));
        windowClone.connect('show-chrome', this._onShowChrome.bind(this));
        windowClone.connect('hide-chrome', this._onHideChrome.bind(this));

        this._windowAddedId = 0;

        button.hide();

        this.title = title;
        this.closeButton = button;

        // Don't block drop targets
        Shell.util_set_hidden_from_pick(this.title, true);
        Shell.util_set_hidden_from_pick(this.border, true);

        parentActor.add_actor(this.border);
        parentActor.add_actor(this.title);
        parentActor.add_actor(this.closeButton);
        title.connect('style-changed',
                      this._onStyleChanged.bind(this));
        button.connect('style-changed',
                       this._onStyleChanged.bind(this));
        this.border.connect('style-changed', this._onStyleChanged.bind(this));
        // force a style change if we are already on a stage - otherwise
        // the signal will be emitted normally when we are added
        if (parentActor.get_stage())
            this._onStyleChanged();
    },

    hide() {
        this._hidden = true;

        this.hideCloseButton();
    },

    show() {
        this._hidden = false;

        if (this._windowClone.actor['has-pointer'])
            this._animateVisible();
    },

    chromeHeights() {
        return [Math.max(this.borderSize, this.closeButton.height - this.closeButton._overlap),
                (this.title.height - this.borderSize) / 2];
    },

    chromeWidths() {
        return [this.borderSize,
                Math.max(this.borderSize, this.closeButton.width - this.closeButton._overlap)];
    },

    relayout(animate) {
        let button = this.closeButton;
        let title = this.title;
        let border = this.border;

        Tweener.removeTweens(button);
        Tweener.removeTweens(border);
        Tweener.removeTweens(title);

        let [cloneX, cloneY, cloneWidth, cloneHeight] = this._windowClone.slot;

        let layout = Meta.prefs_get_button_layout();
        let side = layout.left_buttons.indexOf(Meta.ButtonFunction.CLOSE) > -1 ? St.Side.LEFT : St.Side.RIGHT;

        let buttonX;
        let buttonY = cloneY - (button.height - button._overlap);
        if (side == St.Side.LEFT)
            buttonX = cloneX - (button.width - button._overlap);
        else
            buttonX = cloneX + (cloneWidth - button._overlap);

        if (animate)
            this._animateOverlayActor(button, Math.floor(buttonX), Math.floor(buttonY), button.width);
        else
            button.set_position(Math.floor(buttonX), Math.floor(buttonY));

        let titleX = cloneX + (cloneWidth - title.width) / 2;
        let titleY = cloneY + cloneHeight - (title.height - this.borderSize) / 2;

        if (animate)
            this._animateOverlayActor(title, Math.floor(titleX), Math.floor(titleY));
        else
            title.set_position(Math.floor(titleX), Math.floor(titleY));

        let borderX = cloneX - this.borderSize;
        let borderY = cloneY - this.borderSize;
        let borderWidth = cloneWidth + 2 * this.borderSize;
        let borderHeight = cloneHeight + 2 * this.borderSize;

        if (animate) {
            this._animateOverlayActor(this.border, borderX, borderY,
                                      borderWidth, borderHeight);
        } else {
            this.border.set_position(borderX, borderY);
            this.border.set_size(borderWidth, borderHeight);
        }
    },

    _animateOverlayActor(actor, x, y, width, height) {
        let params = { x: x,
                       y: y,
                       time: Overview.ANIMATION_TIME,
                       transition: 'easeOutQuad' };

        if (width !== undefined)
            params.width = width;

        if (height !== undefined)
            params.height = height;

        Tweener.addTween(actor, params);
    },

    _closeWindow(actor) {
        let metaWindow = this._windowClone.metaWindow;
        this._workspace = metaWindow.get_workspace();

        this._windowAddedId = this._workspace.connect('window-added',
                                                      this._onWindowAdded.bind(this));

        this._windowClone.deleteAll();
    },

    _windowCanClose() {
        return this._windowClone.metaWindow.can_close() &&
               !this._windowClone.hasAttachedDialogs();
    },

    _onWindowAdded(workspace, win) {
        let metaWindow = this._windowClone.metaWindow;

        if (win.get_transient_for() == metaWindow) {
            workspace.disconnect(this._windowAddedId);
            this._windowAddedId = 0;

            // use an idle handler to avoid mapping problems -
            // see comment in Workspace._windowAdded
            let id = Mainloop.idle_add(() => {
                this._windowClone.emit('selected');
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._windowClone.emit');
        }
    },

    _onDestroy() {
        if (this._windowAddedId > 0) {
            this._workspace.disconnect(this._windowAddedId);
            this._windowAddedId = 0;
        }
        if (this._idleToggleCloseId > 0) {
            Mainloop.source_remove(this._idleToggleCloseId);
            this._idleToggleCloseId = 0;
        }
        this._windowClone.metaWindow.disconnect(this._updateCaptionId);
        this.title.destroy();
        this.closeButton.destroy();
        this.border.destroy();
    },

    _animateVisible() {
        this._parentActor.raise_top();

        let toAnimate = [this.border, this.title];
        if (this._windowCanClose())
            toAnimate.push(this.closeButton);

        toAnimate.forEach(a => {
            a.show();
            a.opacity = 0;
            Tweener.addTween(a,
                             { opacity: 255,
                               time: CLOSE_BUTTON_FADE_TIME,
                               transition: 'easeOutQuad' });
        });
    },

    _animateInvisible() {
        [this.closeButton, this.border, this.title].forEach(a => {
            a.opacity = 255;
            Tweener.addTween(a,
                             { opacity: 0,
                               time: CLOSE_BUTTON_FADE_TIME,
                               transition: 'easeInQuad' });
        });
    },

    _onShowChrome() {
        // We might get enter events on the clone while the overlay is
        // hidden, e.g. during animations, we ignore these events,
        // as the close button will be shown as needed when the overlays
        // are shown again
        if (this._hidden)
            return;

        this._animateVisible();
        this.emit('show-close-button');
    },

    _onHideChrome() {
        if (this._idleToggleCloseId == 0) {
            this._idleToggleCloseId = Mainloop.timeout_add(750, this._idleToggleCloseButton.bind(this));
            GLib.Source.set_name_by_id(this._idleToggleCloseId, '[gnome-shell] this._idleToggleCloseButton');
        }
    },

    _idleToggleCloseButton() {
        this._idleToggleCloseId = 0;

        if (!this._windowClone.actor['has-pointer'] &&
            !this.closeButton['has-pointer'])
            this._animateInvisible();

        return GLib.SOURCE_REMOVE;
    },

    hideCloseButton() {
        if (this._idleToggleCloseId > 0) {
            Mainloop.source_remove(this._idleToggleCloseId);
            this._idleToggleCloseId = 0;
        }
        this.closeButton.hide();
        this.border.hide();
        this.title.hide();
    },

    _onStyleChanged() {
        let closeNode = this.closeButton.get_theme_node();
        this.closeButton._overlap = closeNode.get_length('-shell-close-overlap');

        let borderNode = this.border.get_theme_node();
        this.borderSize = borderNode.get_border_width(St.Side.TOP);

        this._parentActor.queue_relayout();
    }
});
Signals.addSignalMethods(WindowOverlay.prototype);

var WindowPositionFlags = {
    NONE: 0,
    INITIAL: 1 << 0,
    ANIMATE: 1 << 1
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
// We don't compute some global order of windows right now for optimal
// travel when animating into the overview; windows are assumed to be
// in some stable order.
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
// larger than WINDOW_CLONE_MAXIMUM_SCALE, which means that the inequality:
//
//   windowScale * layoutScale <= WINDOW_CLONE_MAXIMUM_SCALE
//
// must always be true. This is for each individual window -- while we
// could adjust layoutScale to make the largest thumbnail smaller than
// WINDOW_CLONE_MAXIMUM_SCALE, it would shrink windows which are already
// under the inequality. To solve this, we simply cheat: we simply keep
// each window's "cell" area to be the same, but we shrink the thumbnail
// and center it horizontally, and align it to the bottom vertically.

var LayoutStrategy = new Lang.Class({
    Name: 'LayoutStrategy',
    Abstract: true,

    _init(monitor, rowSpacing, columnSpacing) {
        this._monitor = monitor;
        this._rowSpacing = rowSpacing;
        this._columnSpacing = columnSpacing;
    },

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
    },

    // Computes and returns an individual scaling factor for @window,
    // to be applied in addition to the overal layout scale.
    _computeWindowScale(window) {
        // Since we align windows next to each other, the height of the
        // thumbnails is much more important to preserve than the width of
        // them, so two windows with equal height, but maybe differering
        // widths line up.
        let ratio = window.height / this._monitor.height;

        // The purpose of this manipulation here is to prevent windows
        // from getting too small. For something like a calculator window,
        // we need to bump up the size just a bit to make sure it looks
        // good. We'll use a multiplier of 1.5 for this.

        // Map from [0, 1] to [1.5, 1]
        return _interpolate(1.5, 1, ratio);
    },

    // Compute the size of each row, by assigning to the properties
    // row.width, row.height, row.fullWidth, row.fullHeight, and
    // (optionally) for each row in @layout.rows. This method is
    // intended to be called by subclasses.
    _computeRowSizes(layout) {
        throw new Error('_computeRowSizes not implemented');
    },

    // Compute strategy-specific window slots for each window in
    // @windows, given the @layout. The strategy may also use @layout
    // as strategy-specific storage.
    //
    // This must calculate:
    //  * maxColumns - The maximum number of columns used by the layout.
    //  * gridWidth - The total width used by the grid, unscaled, unspaced.
    //  * gridHeight - The totial height used by the grid, unscaled, unspaced.
    //  * rows - A list of rows, which should be instantiated by _newRow.
    computeLayout(windows, layout) {
        throw new Error('computeLayout not implemented');
    },

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
        let scale = Math.min(horizontalScale, verticalScale, WINDOW_CLONE_MAXIMUM_SCALE);

        let scaledLayoutWidth = layout.gridWidth * scale + hspacing;
        let scaledLayoutHeight = layout.gridHeight * scale + vspacing;
        let space = (scaledLayoutWidth * scaledLayoutHeight) / (area.width * area.height);

        layout.scale = scale;
        layout.space = space;
    },

    computeWindowSlots(layout, area) {
        this._computeRowSizes(layout);

        let { rows: rows, scale: scale } = layout;

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
        let compensation = 0
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

            row.x = area.x + (Math.max(area.width - (widthWithoutSpacing * row.additionalScale + horizontalSpacing), 0) / 2)
            row.y = area.y + (Math.max(area.height - (heightWithoutSpacing + verticalSpacing), 0) / 2) + y;
            y += row.height * row.additionalScale + this._rowSpacing;
        }

        compensation = compensation / 2;

        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            let x = row.x;
            for (let j = 0; j < row.windows.length; j++) {
                let window = row.windows[j];

                let s = scale * this._computeWindowScale(window) * row.additionalScale;
                let cellWidth = window.width * s;
                let cellHeight = window.height * s;

                s = Math.min(s, WINDOW_CLONE_MAXIMUM_SCALE);
                let cloneWidth = window.width * s;

                let cloneX = x + (cellWidth - cloneWidth) / 2;
                let cloneY = row.y + row.height * row.additionalScale - cellHeight + compensation;

                // Align with the pixel grid to prevent blurry windows at scale = 1
                cloneX = Math.floor(cloneX);
                cloneY = Math.floor(cloneY);

                slots.push([cloneX, cloneY, s, window]);
                x += cellWidth + this._columnSpacing;
            }
        }
        return slots;
    }
});

var UnalignedLayoutStrategy = new Lang.Class({
    Name: 'UnalignedLayoutStrategy',
    Extends: LayoutStrategy,

    _computeRowSizes(layout) {
        let { rows: rows, scale: scale } = layout;
        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            row.width = row.fullWidth * scale + (row.windows.length - 1) * this._columnSpacing;
            row.height = row.fullHeight * scale;
        }
    },

    _keepSameRow(row, window, width, idealRowWidth) {
        if (row.fullWidth + width <= idealRowWidth)
            return true;

        let oldRatio = row.fullWidth / idealRowWidth;
        let newRatio = (row.fullWidth + width) / idealRowWidth;

        if (Math.abs(1 - newRatio) < Math.abs(1 - oldRatio))
            return true;

        return false;
    },

    _sortRow(row) {
        // Sort windows horizontally to minimize travel distance
        row.windows.sort((a, b) => a.realWindow.x - b.realWindow.x);
    },

    computeLayout(windows, layout) {
        let numRows = layout.numRows;

        let rows = [];
        let totalWidth = 0;
        for (let i = 0; i < windows.length; i++) {
            let window = windows[i];
            let s = this._computeWindowScale(window);
            totalWidth += window.width * s;
        }

        let idealRowWidth = totalWidth / numRows;
        let windowIdx = 0;
        for (let i = 0; i < numRows; i++) {
            let col = 0;
            let row = this._newRow();
            rows.push(row);

            for (; windowIdx < windows.length; windowIdx++) {
                let window = windows[windowIdx];
                let s = this._computeWindowScale(window);
                let width = window.width * s;
                let height = window.height * s;
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
});

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

    return (one.x == two.x &&
            one.y == two.y &&
            one.width == two.width &&
            one.height == two.height);
}

const WorkspaceActor = new Lang.Class({
    Name: 'WorkspaceActor',
    Extends: St.Widget,

    vfunc_get_focus_chain() {
        return this.get_children().filter(c => c.visible).sort((a,b) => {
            let cloneA = (a._delegate && a._delegate instanceof WindowClone) ? a._delegate: null;
            let cloneB = (b._delegate && b._delegate instanceof WindowClone) ? b._delegate: null;
            if (cloneA && cloneB)
                return cloneA.slotId - cloneB.slotId;

            return 0;
        });
    }
});

/**
 * @metaWorkspace: a #Meta.Workspace, or null
 */
var Workspace = new Lang.Class({
    Name: 'Workspace',

    _init(metaWorkspace, monitorIndex) {
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
        this._windowOverlaysGroup = new Clutter.Actor();
        // Without this the drop area will be overlapped.
        this._windowOverlaysGroup.set_size(0, 0);

        this.actor = new WorkspaceActor({ style_class: 'window-picker' });
        if (monitorIndex != Main.layoutManager.primaryIndex)
            this.actor.add_style_class_name('external-monitor');
        this.actor.set_size(0, 0);

        this._dropRect = new Clutter.Actor({ opacity: 0 });
        this._dropRect._delegate = this;

        this.actor.add_actor(this._dropRect);
        this.actor.add_actor(this._windowOverlaysGroup);

        this.actor.connect('destroy', this._onDestroy.bind(this));

        let windows = global.get_window_actors().filter(this._isMyWindow, this);

        // Create clones for windows that should be
        // visible in the Overview
        this._windows = [];
        this._windowOverlays = [];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i])) {
                this._addWindowClone(windows[i], true);
            }
        }

        // Track window changes
        if (this.metaWorkspace) {
            this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                             this._windowAdded.bind(this));
            this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                               this._windowRemoved.bind(this));
        }
        this._windowEnteredMonitorId = global.screen.connect('window-entered-monitor',
                                                             this._windowEnteredMonitor.bind(this));
        this._windowLeftMonitorId = global.screen.connect('window-left-monitor',
                                                          this._windowLeftMonitor.bind(this));
        this._repositionWindowsId = 0;

        this.leavingOverview = false;

        this._positionWindowsFlags = 0;
        this._positionWindowsId = 0;

        this.actor.connect('notify::mapped', () => {
            if (this.actor.mapped)
                this._syncActualGeometry();
        });
    },

    setFullGeometry(geom) {
        if (rectEqual(this._fullGeometry, geom))
            return;

        this._fullGeometry = geom;

        if (this.actor.mapped)
            this._recalculateWindowPositions(WindowPositionFlags.NONE);
    },

    setActualGeometry(geom) {
        if (rectEqual(this._actualGeometry, geom))
            return;

        this._actualGeometry = geom;
        this._actualGeometryDirty = true;

        if (this.actor.mapped)
            this._syncActualGeometry();
    },

    _syncActualGeometry() {
        if (this._actualGeometryLater || !this._actualGeometryDirty)
            return;
        if (!this._actualGeometry)
            return;

        this._actualGeometryLater = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this._actualGeometryLater = 0;
            if (!this.actor.mapped)
                return false;

            let geom = this._actualGeometry;

            this._dropRect.set_position(geom.x, geom.y);
            this._dropRect.set_size(geom.width, geom.height);
            this._updateWindowPositions(Main.overview.animationInProgress ? WindowPositionFlags.ANIMATE : WindowPositionFlags.NONE);

            return false;
        });
    },

    _lookupIndex(metaWindow) {
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].metaWindow == metaWindow) {
                return i;
            }
        }
        return -1;
    },

    containsMetaWindow(metaWindow) {
        return this._lookupIndex(metaWindow) >= 0;
    },

    isEmpty() {
        return this._windows.length == 0;
    },

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
    },

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
    },

    _realRecalculateWindowPositions(flags) {
        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
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
    },

    _updateWindowPositions(flags) {
        if (this._currentLayout == null) {
            this._recalculateWindowPositions(flags);
            return;
        }

        // We will reposition windows anyway when enter again overview or when ending the windows
        // animations whith fade animation.
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

        let currentWorkspace = global.screen.get_active_workspace();
        let isOnCurrentWorkspace = this.metaWorkspace == null || this.metaWorkspace == currentWorkspace;

        for (let i = 0; i < slots.length; i++) {
            let slot = slots[i];
            let [x, y, scale, clone] = slot;
            let metaWindow = clone.metaWindow;
            let overlay = clone.overlay;
            clone.slotId = i;

            // Positioning a window currently being dragged must be avoided;
            // we'll just leave a blank spot in the layout for it.
            if (clone.inDrag)
                continue;

            let cloneWidth = clone.actor.width * scale;
            let cloneHeight = clone.actor.height * scale;
            clone.slot = [x, y, cloneWidth, cloneHeight];

            if (overlay && (initialPositioning || !clone.positioned))
                overlay.hide();

            if (!clone.positioned) {
                // This window appeared after the overview was already up
                // Grow the clone from the center of the slot
                clone.actor.x = x + cloneWidth / 2;
                clone.actor.y = y + cloneHeight / 2;
                clone.actor.scale_x = 0;
                clone.actor.scale_y = 0;
                clone.positioned = true;
            }

            if (animate && isOnCurrentWorkspace) {
                if (!metaWindow.showing_on_its_workspace()) {
                    /* Hidden windows should fade in and grow
                     * therefore we need to resize them now so they
                     * can be scaled up later */
                    if (initialPositioning) {
                        clone.actor.opacity = 0;
                        clone.actor.scale_x = 0;
                        clone.actor.scale_y = 0;
                        clone.actor.x = x;
                        clone.actor.y = y;
                    }

                    Tweener.addTween(clone.actor,
                                     { opacity: 255,
                                       time: Overview.ANIMATION_TIME,
                                       transition: 'easeInQuad'
                                     });
                }

                this._animateClone(clone, overlay, x, y, scale);
            } else {
                // cancel any active tweens (otherwise they might override our changes)
                Tweener.removeTweens(clone.actor);
                clone.actor.set_position(x, y);
                clone.actor.set_scale(scale, scale);
                clone.actor.set_opacity(255);
                clone.overlay.relayout(false);
                this._showWindowOverlay(clone, overlay);
            }
        }
    },

    syncStacking(stackIndices) {
        let clones = this._windows.slice();
        clones.sort((a, b) => {
            let indexA = stackIndices[a.metaWindow.get_stable_sequence()];
            let indexB = stackIndices[b.metaWindow.get_stable_sequence()];
            return indexA - indexB;
        });

        for (let i = 0; i < clones.length; i++) {
            let clone = clones[i];
            let metaWindow = clone.metaWindow;
            if (i == 0) {
                clone.setStackAbove(this._dropRect);
            } else {
                let previousClone = clones[i - 1];
                clone.setStackAbove(previousClone.actor);
            }
        }
    },

    _animateClone(clone, overlay, x, y, scale) {
        Tweener.addTween(clone.actor,
                         { x: x,
                           y: y,
                           scale_x: scale,
                           scale_y: scale,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                               this._showWindowOverlay(clone, overlay);
                           }
                         });

        clone.overlay.relayout(true);
    },

    _showWindowOverlay(clone, overlay) {
        if (clone.inDrag)
            return;

        if (overlay && overlay._hidden)
                overlay.show();
    },

    _delayedWindowRepositioning() {
        let [x, y, mask] = global.get_pointer();

        let pointerHasMoved = (this._cursorX != x && this._cursorY != y);
        let inWorkspace = (this._fullGeometry.x < x && x < this._fullGeometry.x + this._fullGeometry.width &&
                           this._fullGeometry.y < y && y < this._fullGeometry.y + this._fullGeometry.height);

        if (pointerHasMoved && inWorkspace) {
            // store current cursor position
            this._cursorX = x;
            this._cursorY = y;
            return GLib.SOURCE_CONTINUE;
        }

        let actorUnderPointer = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].actor == actorUnderPointer)
                return GLib.SOURCE_CONTINUE;
        }

        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE);
        this._repositionWindowsId = 0;
        return GLib.SOURCE_REMOVE;
    },

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
                let [stageX, stageY] = clone.actor.get_transformed_position();
                let [stageWidth, stageHeight] = clone.actor.get_transformed_size();
                win._overviewHint = {
                    x: stageX,
                    y: stageY,
                    scale: stageWidth / clone.actor.width
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
            Mainloop.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        // setup new handler
        let [x, y, mask] = global.get_pointer();
        this._cursorX = x;
        this._cursorY = y;

        this._currentLayout = null;
        this._repositionWindowsId = Mainloop.timeout_add(750,
            this._delayedWindowRepositioning.bind(this));
        GLib.Source.set_name_by_id(this._repositionWindowsId, '[gnome-shell] this._delayedWindowRepositioning');
    },

    _doAddWindow(metaWin) {
        if (this.leavingOverview)
            return;

        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            let id = Mainloop.idle_add(() => {
                if (this.actor &&
                    metaWin.get_compositor_private() &&
                    metaWin.get_workspace() == this.metaWorkspace)
                    this._doAddWindow(metaWin);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._doAddWindow');
            return;
        }

        // We might have the window in our list already if it was on all workspaces and
        // now was moved to this workspace
        if (this._lookupIndex (metaWin) != -1)
            return;

        if (!this._isMyWindow(win))
            return;

        if (!this._isOverviewWindow(win)) {
            if (metaWin.is_attached_dialog()) {
                let parent = metaWin.get_transient_for();
                while (parent.is_attached_dialog())
                    parent = parent.get_transient_for();

                let idx = this._lookupIndex (parent);
                if (idx < 0) {
                    // parent was not created yet, it will take care
                    // of the dialog when created
                    return;
                }

                let clone = this._windows[idx];
                clone.addAttachedDialog(metaWin);
            }

            return;
        }

        let [clone, overlay] = this._addWindowClone(win, false);

        if (win._overviewHint) {
            let x = win._overviewHint.x - this.actor.x;
            let y = win._overviewHint.y - this.actor.y;
            let scale = win._overviewHint.scale;
            delete win._overviewHint;

            clone.slot = [x, y, clone.actor.width * scale, clone.actor.height * scale];
            clone.positioned = true;
            clone.actor.set_position (x, y);
            clone.actor.set_scale (scale, scale);
            clone.overlay.relayout(false);
        }

        this._currentLayout = null;
        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE);
    },

    _windowAdded(metaWorkspace, metaWin) {
        this._doAddWindow(metaWin);
    },

    _windowRemoved(metaWorkspace, metaWin) {
        this._doRemoveWindow(metaWin);
    },

    _windowEnteredMonitor(metaScreen, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex) {
            this._doAddWindow(metaWin);
        }
    },

    _windowLeftMonitor(metaScreen, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex) {
            this._doRemoveWindow(metaWin);
        }
    },

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
    },

    fadeToOverview() {
        // We don't want to reposition windows while animating in this way.
        this._animatingWindowsFade = true;
        this._overviewShownId = Main.overview.connect('shown', this._doneShowingOverview.bind(this));
        if (this._windows.length == 0)
            return;

        if (this.metaWorkspace != null && this.metaWorkspace != global.screen.get_active_workspace())
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
                let overlay = this._windowOverlays[i];
                if (overlay)
                    overlay.hide();
                this._windows[i].actor.opacity = 0;
            } else {
                let fromTop = topIndex - i;
                let time;
                if (fromTop < nTimeSlots) // animate top-most windows gradually
                    time = windowBaseTime * (nTimeSlots - fromTop);
                else
                    time = windowBaseTime;

                this._windows[i].actor.opacity = 255;
                this._fadeWindow(i, time, 0);
            }
        }
    },

    fadeFromOverview() {
        this.leavingOverview = true;
        this._overviewHiddenId = Main.overview.connect('hidden', this._doneLeavingOverview.bind(this));
        if (this._windows.length == 0)
            return;

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            Tweener.removeTweens(clone.actor);
        }

        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        if (this.metaWorkspace != null && this.metaWorkspace != global.screen.get_active_workspace())
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
                let overlay = this._windowOverlays[i];
                if (overlay)
                    overlay.hide();
                this._windows[i].actor.opacity = 0;
            } else {
                let fromTop = topIndex - i;
                let time;
                if (fromTop < nTimeSlots) // animate top-most windows gradually
                    time = windowBaseTime * (fromTop + 1);
                else
                    time = windowBaseTime * nTimeSlots;

                this._windows[i].actor.opacity = 0;
                this._fadeWindow(i, time, 255);
            }
        }
    },

    _fadeWindow(index, time, opacity) {
        let clone = this._windows[index];
        let overlay = this._windowOverlays[index];

        if (overlay)
            overlay.hide();

        if (clone.metaWindow.showing_on_its_workspace()) {
            let [origX, origY] = clone.getOriginalPosition();
            clone.actor.scale_x = 1;
            clone.actor.scale_y = 1;
            clone.actor.x = origX;
            clone.actor.y = origY;
            Tweener.addTween(clone.actor,
                             { time: time,
                               opacity: opacity,
                               transition: 'easeOutQuad'
                             });
        } else {
            // The window is hidden
            clone.actor.opacity = 0;
        }
    },

    zoomToOverview() {
        // Position and scale the windows.
        this._recalculateWindowPositions(WindowPositionFlags.ANIMATE | WindowPositionFlags.INITIAL);
    },

    zoomFromOverview() {
        let currentWorkspace = global.screen.get_active_workspace();

        this.leavingOverview = true;

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            Tweener.removeTweens(clone.actor);
        }

        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }
        this._overviewHiddenId = Main.overview.connect('hidden', this._doneLeavingOverview.bind(this));

        if (this.metaWorkspace != null && this.metaWorkspace != currentWorkspace)
            return;

        // Position and scale the windows.
        for (let i = 0; i < this._windows.length; i++)
           this._zoomWindowFromOverview(i);
    },

    _zoomWindowFromOverview(index) {
        let clone = this._windows[index];
        let overlay = this._windowOverlays[index];

        if (overlay)
            overlay.hide();

        if (clone.metaWindow.showing_on_its_workspace()) {
            let [origX, origY] = clone.getOriginalPosition();
            Tweener.addTween(clone.actor,
                             { x: origX,
                               y: origY,
                               scale_x: 1.0,
                               scale_y: 1.0,
                               time: Overview.ANIMATION_TIME,
                               opacity: 255,
                               transition: 'easeOutQuad'
                             });
        } else {
            // The window is hidden, make it shrink and fade it out
            Tweener.addTween(clone.actor,
                             { scale_x: 0,
                               scale_y: 0,
                               opacity: 0,
                               time: Overview.ANIMATION_TIME,
                               transition: 'easeOutQuad'
                             });
        }
    },

    destroy() {
        this.actor.destroy();
    },

    _onDestroy(actor) {
        if (this._overviewHiddenId) {
            Main.overview.disconnect(this._overviewHiddenId);
            this._overviewHiddenId = 0;
        }
        Tweener.removeTweens(actor);

        if (this.metaWorkspace) {
            this.metaWorkspace.disconnect(this._windowAddedId);
            this.metaWorkspace.disconnect(this._windowRemovedId);
        }
        global.screen.disconnect(this._windowEnteredMonitorId);
        global.screen.disconnect(this._windowLeftMonitorId);

        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
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
    },

    // Sets this.leavingOverview flag to false.
    _doneLeavingOverview() {
        this.leavingOverview = false;
    },

    _doneShowingOverview() {
        this._animatingWindowsFade = false;
        this._recalculateWindowPositions(WindowPositionFlags.INITIAL);
    },

    // Tests if @actor belongs to this workspaces and monitor
    _isMyWindow(actor) {
        let win = actor.meta_window;
        return (this.metaWorkspace == null || win.located_on_workspace(this.metaWorkspace)) &&
            (win.get_monitor() == this.monitorIndex);
    },

    // Tests if @win should be shown in the Overview
    _isOverviewWindow(win) {
        return !win.get_meta_window().skip_taskbar;
    },

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone(win, positioned) {
        let clone = new WindowClone(win, this);
        let overlay = new WindowOverlay(clone, this._windowOverlaysGroup);
        clone.overlay = overlay;
        clone.positioned = positioned;

        clone.connect('selected',
                      this._onCloneSelected.bind(this));
        clone.connect('drag-begin', () => {
            Main.overview.beginWindowDrag(clone.metaWindow);
            overlay.hide();
        });
        clone.connect('drag-cancelled', () => {
            Main.overview.cancelledWindowDrag(clone.metaWindow);
        });
        clone.connect('drag-end', () => {
            Main.overview.endWindowDrag(clone.metaWindow);
            overlay.show();
        });
        clone.connect('size-changed', () => {
            this._recalculateWindowPositions(WindowPositionFlags.NONE);
        });
        clone.actor.connect('destroy', () => {
            this._removeWindowClone(clone.metaWindow);
        });

        this.actor.add_actor(clone.actor);

        overlay.connect('show-close-button', () => {
            let focus = global.stage.key_focus;
            if (focus == null || this.actor.contains(focus))
                clone.actor.grab_key_focus();
            this._onShowOverlayClose(overlay);
        });

        if (this._windows.length == 0)
            clone.setStackAbove(null);
        else
            clone.setStackAbove(this._windows[this._windows.length - 1].actor);

        this._windows.push(clone);
        this._windowOverlays.push(overlay);

        return [clone, overlay];
    },

    _removeWindowClone(metaWin) {
        // find the position of the window in our list
        let index = this._lookupIndex (metaWin);

        if (index == -1)
            return null;

        this._windowOverlays.splice(index, 1);
        return this._windows.splice(index, 1).pop();
    },

    _onShowOverlayClose(windowOverlay) {
        for (let i = 0; i < this._windowOverlays.length; i++) {
            let overlay = this._windowOverlays[i];
            if (overlay == windowOverlay)
                continue;
            overlay.hideCloseButton();
        }
    },

    _isBetterLayout(oldLayout, newLayout) {
        if (oldLayout.scale === undefined)
            return true;

        let spacePower = (newLayout.space - oldLayout.space) * LAYOUT_SPACE_WEIGHT;
        let scalePower = (newLayout.scale - oldLayout.scale) * LAYOUT_SCALE_WEIGHT;

        if (newLayout.scale > oldLayout.scale && newLayout.space > oldLayout.space) {
            // Win win -- better scale and better space
            return true;
        } else if (newLayout.scale > oldLayout.scale && newLayout.space <= oldLayout.space) {
            // Keep new layout only if scale gain outweights aspect space loss
            return scalePower > spacePower;
        } else if (newLayout.scale <= oldLayout.scale && newLayout.space > oldLayout.space) {
            // Keep new layout only if aspect space gain outweights scale loss
            return spacePower > scalePower;
        } else {
            // Lose -- worse scale and space
            return false;
        }
    },

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

            let layout = { area: area, strategy: strategy, numRows: numRows, numColumns: numColumns };
            strategy.computeLayout(windows, layout);
            strategy.computeScaleAndSpace(layout);

            if (!this._isBetterLayout(lastLayout, layout))
                break;

            lastLayout = layout;
        }

        return lastLayout;
    },

    _getSpacingAndPadding() {
        let node = this.actor.get_theme_node();

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
        let overlay = this._windowOverlays[0];
        let [topBorder, bottomBorder] = overlay.chromeHeights();
        let [leftBorder, rightBorder] = overlay.chromeWidths();

        rowSpacing += (topBorder + bottomBorder) / 2;
        columnSpacing += (rightBorder + leftBorder) / 2;
        padding.top += topBorder;
        padding.bottom += bottomBorder;
        padding.left += leftBorder;
        padding.right += rightBorder;

        return [rowSpacing, columnSpacing, padding];
    },

    _computeLayout(windows) {
        let [rowSpacing, columnSpacing, padding] = this._getSpacingAndPadding();
        let area = padArea(this._fullGeometry, padding);
        return this._getBestLayout(windows, area, rowSpacing, columnSpacing);
    },

    _onCloneSelected(clone, time) {
        let wsIndex = undefined;
        if (this.metaWorkspace)
            wsIndex = this.metaWorkspace.index();
        Main.activateWindow(clone.metaWindow, time, wsIndex);
    },

    // Draggable target interface
    handleDragOver(source, actor, x, y, time) {
        if (source.realWindow && !this._isMyWindow(source.realWindow))
            return DND.DragMotionResult.MOVE_DROP;
        if (source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    },

    acceptDrop(source, actor, x, y, time) {
        if (source.realWindow) {
            let win = source.realWindow;
            if (this._isMyWindow(win))
                return false;

            // Set a hint on the Mutter.Window so its initial position
            // in the new workspace will be correct
            win._overviewHint = {
                x: actor.x,
                y: actor.y,
                scale: actor.scale_x
            };

            let metaWindow = win.get_meta_window();

            // We need to move the window before changing the workspace, because
            // the move itself could cause a workspace change if the window enters
            // the primary monitor
            if (metaWindow.get_monitor() != this.monitorIndex)
                metaWindow.move_to_monitor(this.monitorIndex);

            let index = this.metaWorkspace ? this.metaWorkspace.index() : global.screen.get_active_workspace_index();
            metaWindow.change_workspace_by_index(index, false);
            return true;
        } else if (source.shellWorkspaceLaunch) {
            source.shellWorkspaceLaunch({ workspace: this.metaWorkspace ? this.metaWorkspace.index() : -1,
                                          timestamp: time });
            return true;
        }

        return false;
    }
});

Signals.addSignalMethods(Workspace.prototype);
