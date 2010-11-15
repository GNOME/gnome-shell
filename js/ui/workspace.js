/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;

const DND = imports.ui.dnd;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;

const FOCUS_ANIMATION_TIME = 0.15;

const WINDOW_DND_SIZE = 256;

const SCROLL_SCALE_AMOUNT = 100 / 5;

const LIGHTBOX_FADE_TIME = 0.1;
const CLOSE_BUTTON_FADE_TIME = 0.1;

const DRAGGING_WINDOW_OPACITY = 100;

// Define a layout scheme for small window counts. For larger
// counts we fall back to an algorithm. We need more schemes here
// unless we have a really good algorithm.

// Each triplet is [xCenter, yCenter, scale] where the scale
// is relative to the width of the workspace.
const POSITIONS = {
        1: [[0.5, 0.5, 0.95]],
        2: [[0.25, 0.5, 0.48], [0.75, 0.5, 0.48]],
        3: [[0.25, 0.25, 0.48],  [0.75, 0.25, 0.48],  [0.5, 0.75, 0.48]],
        4: [[0.25, 0.25, 0.47],   [0.75, 0.25, 0.47], [0.75, 0.75, 0.47], [0.25, 0.75, 0.47]],
        5: [[0.165, 0.25, 0.32], [0.495, 0.25, 0.32], [0.825, 0.25, 0.32], [0.25, 0.75, 0.32], [0.75, 0.75, 0.32]]
};
// Used in _orderWindowsPermutations, 5! = 120 which is probably the highest we can go
const POSITIONING_PERMUTATIONS_MAX = 5;

function _interpolate(start, end, step) {
    return start + (end - start) * step;
}

function _clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}


function ScaledPoint(x, y, scaleX, scaleY) {
    [this.x, this.y, this.scaleX, this.scaleY] = arguments;
}

ScaledPoint.prototype = {
    getPosition : function() {
        return [this.x, this.y];
    },

    getScale : function() {
        return [this.scaleX, this.scaleY];
    },

    setPosition : function(x, y) {
        [this.x, this.y] = arguments;
    },

    setScale : function(scaleX, scaleY) {
        [this.scaleX, this.scaleY] = arguments;
    },

    interpPosition : function(other, step) {
        return [_interpolate(this.x, other.x, step),
                _interpolate(this.y, other.y, step)];
    },

    interpScale : function(other, step) {
        return [_interpolate(this.scaleX, other.scaleX, step),
                _interpolate(this.scaleY, other.scaleY, step)];
    }
};


function WindowClone(realWindow) {
    this._init(realWindow);
}

WindowClone.prototype = {
    _init : function(realWindow) {
        this.actor = new Clutter.Clone({ source: realWindow.get_texture(),
                                         reactive: true,
                                         x: realWindow.x,
                                         y: realWindow.y });
        this.actor._delegate = this;
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;
        this.metaWindow._delegate = this;
        this.origX = realWindow.x;
        this.origY = realWindow.y;

        this._stackAbove = null;

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));

        this.actor.connect('scroll-event',
                           Lang.bind(this, this._onScroll));

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onLeave));

        this._draggable = DND.makeDraggable(this.actor,
                                            { restoreOnSuccess: true,
                                              dragActorMaxSize: WINDOW_DND_SIZE,
                                              dragActorOpacity: DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        this._draggable.connect('drag-end', Lang.bind(this, this._onDragEnd));
        this.inDrag = false;

        this._windowIsZooming = false;
        this._zooming = false;
        this._selected = false;
    },

    setStackAbove: function (actor) {
        this._stackAbove = actor;
        if (this.inDrag || this._zooming)
            // We'll fix up the stack after the drag/zooming
            return;
        this.actor.raise(this._stackAbove);
    },

    destroy: function () {
        this.actor.destroy();
    },

    zoomFromOverview: function() {
        if (this._zooming) {
            // If the user clicked on the zoomed window, or we are
            // returning there anyways, then we can zoom right to the
            // window, but if we are going to some other window, then
            // we need to cancel the zoom before animating, or it
            // will look funny.

            if (!this._selected &&
                this.metaWindow != global.screen.get_display().focus_window)
                this._zoomEnd();
        }
    },

    _onDestroy: function() {
        this.metaWindow._delegate = null;
        this.actor._delegate = null;
        if (this._zoomLightbox)
            this._zoomLightbox.destroy();

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }

        this.disconnectAll();
    },

    _onLeave: function (actor, event) {
        if (this._zoomStep)
            this._zoomEnd();
    },

    _onScroll : function (actor, event) {
        let direction = event.get_scroll_direction();
        if (direction == Clutter.ScrollDirection.UP) {
            if (this._zoomStep == undefined)
                this._zoomStart();
            if (this._zoomStep < 100) {
                this._zoomStep += SCROLL_SCALE_AMOUNT;
                this._zoomUpdate();
            }
        } else if (direction == Clutter.ScrollDirection.DOWN) {
            if (this._zoomStep > 0) {
                this._zoomStep -= SCROLL_SCALE_AMOUNT;
                this._zoomStep = Math.max(0, this._zoomStep);
                this._zoomUpdate();
            }
            if (this._zoomStep <= 0.0)
                this._zoomEnd();
        }

    },

    _zoomUpdate : function () {
        [this.actor.x, this.actor.y] = this._zoomGlobalOrig.interpPosition(this._zoomTarget, this._zoomStep / 100);
        [this.actor.scale_x, this.actor.scale_y] = this._zoomGlobalOrig.interpScale(this._zoomTarget, this._zoomStep / 100);

        let [width, height] = this.actor.get_transformed_size();

        this.actor.x = _clamp(this.actor.x, 0, global.screen_width  - width);
        this.actor.y = _clamp(this.actor.y, Panel.PANEL_HEIGHT, global.screen_height - height);
    },

    _zoomStart : function () {
        this._zooming = true;
        this.emit('zoom-start');

        if (!this._zoomLightbox)
            this._zoomLightbox = new Lightbox.Lightbox(global.stage,
                                                       { fadeTime: LIGHTBOX_FADE_TIME });
        this._zoomLightbox.show();

        this._zoomLocalOrig  = new ScaledPoint(this.actor.x, this.actor.y, this.actor.scale_x, this.actor.scale_y);
        this._zoomGlobalOrig = new ScaledPoint();
        let parent = this._origParent = this.actor.get_parent();
        let [width, height] = this.actor.get_transformed_size();
        this._zoomGlobalOrig.setPosition.apply(this._zoomGlobalOrig, this.actor.get_transformed_position());
        this._zoomGlobalOrig.setScale(width / this.actor.width, height / this.actor.height);

        this.actor.reparent(global.stage);
        this._zoomLightbox.highlight(this.actor);

        [this.actor.x, this.actor.y]             = this._zoomGlobalOrig.getPosition();
        [this.actor.scale_x, this.actor.scale_y] = this._zoomGlobalOrig.getScale();

        this.actor.raise_top();

        this._zoomTarget = new ScaledPoint(0, 0, 1.0, 1.0);
        this._zoomTarget.setPosition(this.actor.x - (this.actor.width - width) / 2, this.actor.y - (this.actor.height - height) / 2);
        this._zoomStep = 0;

        this._zoomUpdate();
    },

    _zoomEnd : function () {
        this._zooming = false;
        this.emit('zoom-end');

        this.actor.reparent(this._origParent);
        // If the workspace has been destroyed while we were reparented to
        // the stage, _stackAbove will be unparented and we can't raise our
        // actor above it - as we are bound to be destroyed anyway in that
        // case, we can skip that step
        if (this._stackAbove && this._stackAbove.get_parent())
            this.actor.raise(this._stackAbove);

        [this.actor.x, this.actor.y]             = this._zoomLocalOrig.getPosition();
        [this.actor.scale_x, this.actor.scale_y] = this._zoomLocalOrig.getScale();

        this._zoomLightbox.hide();

        this._zoomLocalPosition  = undefined;
        this._zoomLocalScale     = undefined;
        this._zoomGlobalPosition = undefined;
        this._zoomGlobalScale    = undefined;
        this._zoomTargetPosition = undefined;
        this._zoomStep           = undefined;
    },

    _onButtonRelease : function (actor, event) {
        this._selected = true;
        this.emit('selected', event.get_time());
    },

    _onDragBegin : function (draggable, time) {
        this.inDrag = true;
        this.emit('drag-begin');
    },

    _onDragEnd : function (draggable, time, snapback) {
        this.inDrag = false;

        // We may not have a parent if DnD completed successfully, in
        // which case our clone will shortly be destroyed and replaced
        // with a new one on the target workspace.
        if (this.actor.get_parent() != null)
            this.actor.raise(this._stackAbove);

        this.emit('drag-end');
    }
};
Signals.addSignalMethods(WindowClone.prototype);


/**
 * @windowClone: Corresponding window clone
 * @parentActor: The actor which will be the parent of all overlay items
 *               such as app icon and window caption
 */
function WindowOverlay(windowClone, parentActor) {
    this._init(windowClone, parentActor);
}

WindowOverlay.prototype = {
    _init : function(windowClone, parentActor) {
        let metaWindow = windowClone.metaWindow;

        this._windowClone = windowClone;
        this._parentActor = parentActor;

        let title = new St.Label({ style_class: 'window-caption',
                                   text: metaWindow.title });
        title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        title._spacing = 0;

        this._updateCaptionId = metaWindow.connect('notify::title',
            Lang.bind(this, function(w) {
                this.title.text = w.title;
            }));

        let button = new St.Button({ style_class: 'window-close' });
        button._overlap = 0;

        this._idleToggleCloseId = 0;
        button.connect('clicked', Lang.bind(this, this._closeWindow));

        windowClone.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        windowClone.actor.connect('enter-event',
                                  Lang.bind(this, this._onEnter));
        windowClone.actor.connect('leave-event',
                                  Lang.bind(this, this._onLeave));

        this._windowAddedId = 0;
        windowClone.connect('zoom-start', Lang.bind(this, this.hide));
        windowClone.connect('zoom-end', Lang.bind(this, this.show));

        button.hide();

        this.title = title;
        this.closeButton = button;

        parentActor.add_actor(this.title);
        parentActor.add_actor(this.closeButton);
        title.connect('style-changed',
                      Lang.bind(this, this._onStyleChanged));
        button.connect('style-changed',
                       Lang.bind(this, this._onStyleChanged));
        // force a style change if we are already on a stage - otherwise
        // the signal will be emitted normally when we are added
        if (parentActor.get_stage())
            this._onStyleChanged();
    },

    hide: function() {
        this.closeButton.hide();
        this.title.hide();
    },

    show: function() {
        let [x, y, mask] = global.get_pointer();
        let actor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE,
                                                  x, y);
        if (actor == this._windowClone.actor) {
            this.closeButton.show();
        }
        this.title.show();
    },

    fadeIn: function() {
        this.title.opacity = 0;
        this.title.show();
        this._parentActor.raise_top();
        Tweener.addTween(this.title,
                        { opacity: 255,
                          time: CLOSE_BUTTON_FADE_TIME,
                          transition: 'easeOutQuad' });
    },

    chromeWidth: function () {
        return this.closeButton.width - this.closeButton._overlap;
    },

    chromeHeights: function () {
        return [this.closeButton.height - this.closeButton._overlap,
               this.title.height + this.title._spacing];
    },

    /**
     * @cloneX: x position of windowClone
     * @cloneY: y position of windowClone
     * @cloneWidth: width of windowClone
     * @cloneHeight height of windowClone
     */
    // These parameters are not the values retrieved with
    // get_transformed_position() and get_transformed_size(),
    // as windowClone might be moving.
    // See Workspace._fadeInWindowOverlay
    updatePositions: function(cloneX, cloneY, cloneWidth, cloneHeight) {
        let button = this.closeButton;
        let title = this.title;

        let buttonX = cloneX + cloneWidth - button._overlap;
        let buttonY = cloneY - button.height + button._overlap;
        button.set_position(Math.floor(buttonX), Math.floor(buttonY));

        if (!title.fullWidth)
            title.fullWidth = title.width;
        title.width = Math.min(title.fullWidth, cloneWidth);

        let titleX = cloneX + (cloneWidth - title.width) / 2;
        let titleY = cloneY + cloneHeight + title._spacing;
        title.set_position(Math.floor(titleX), Math.floor(titleY));
    },

    _closeWindow: function(actor) {
        let metaWindow = this._windowClone.metaWindow;
        this._workspace = metaWindow.get_workspace();

        this._windowAddedId = this._workspace.connect('window-added',
                                                      Lang.bind(this,
                                                                this._onWindowAdded));

        metaWindow.delete(global.get_current_time());
    },

    _onWindowAdded: function(workspace, win) {
        let metaWindow = this._windowClone.metaWindow;

        if (win.get_transient_for() == metaWindow) {
            workspace.disconnect(this._windowAddedId);
            this._windowAddedId = 0;

            // use an idle handler to avoid mapping problems -
            // see comment in Workspace._windowAdded
            Mainloop.idle_add(Lang.bind(this,
                                        function() {
                                            this._windowClone.emit('selected');
                                            return false;
                                        }));
        }
    },

    _onDestroy: function() {
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
    },

    _onEnter: function() {
        this._parentActor.raise_top();
        this.closeButton.show();
        this.emit('show-close-button');
    },

    _onLeave: function() {
        if (this._idleToggleCloseId == 0)
            this._idleToggleCloseId = Mainloop.timeout_add(750, Lang.bind(this, this._idleToggleCloseButton));
    },

    _idleToggleCloseButton: function() {
        this._idleToggleCloseId = 0;
        let [x, y, mask] = global.get_pointer();
        let actor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE,
                                                  x, y);
        if (actor != this._windowClone.actor && actor != this.closeButton) {
            this.closeButton.hide();
        }
        return false;
    },

    hideCloseButton: function() {
        if (this._idleToggleCloseId > 0) {
            Mainloop.source_remove(this._idleToggleCloseId);
            this._idleToggleCloseId = 0;
        }
        this.closeButton.hide();
    },

    _onStyleChanged: function() {
        let titleNode = this.title.get_theme_node();
        this.title._spacing = titleNode.get_length('-shell-caption-spacing');

        let closeNode = this.closeButton.get_theme_node();
        this.closeButton._overlap = closeNode.get_length('-shell-close-overlap');

        this._parentActor.queue_relayout();
    }
};
Signals.addSignalMethods(WindowOverlay.prototype);

const WindowPositionFlags = {
    ZOOM: 1 << 0,
    ANIMATE: 1 << 1
};

/**
 * @metaWorkspace: a #Meta.Workspace
 */
function Workspace(metaWorkspace) {
    this._init(metaWorkspace);
}

Workspace.prototype = {
    _init : function(metaWorkspace) {
        // When dragging a window, we use this slot for reserve space.
        this._reservedSlot = null;
        this.metaWorkspace = metaWorkspace;
        this._windowOverlaysGroup = new Clutter.Group();
        // Without this the drop area will be overlapped.
        this._windowOverlaysGroup.set_size(0, 0);

        this.actor = new Clutter.Group({ reactive: true });
        this.actor._delegate = this;

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this.actor.connect('button-release-event', Lang.bind(this,
            function(actor, event) {
                // Only switch to the workspace when there's no application
                // windows open. The problem is that it's too easy to miss
                // an app window and get the wrong one focused.
                if (this._windows.length == 0) {
                    this.metaWorkspace.activate(event.get_time());
                    Main.overview.hide();
                }
            }));

        // Items in _windowOverlaysGroup should not be scaled, so we don't
        // add them to this.actor, but to its parent whenever it changes
        this.actor.connect('parent-set', Lang.bind(this, this._onParentSet));

        // Auto-sizing is unreliable in the presence of ClutterClone, so rather than
        // implicitly counting on the workspace actor to be sized to the size of the
        // included desktop actor clone, set the size explicitly to the screen size.
        // See http://bugzilla.openedhand.com/show_bug.cgi?id=1755
        this.actor.width = global.screen_width;
        this.actor.height = global.screen_height;
        this.scale = 1.0;

        let windows = global.get_window_actors().filter(this._isMyWindow, this);

        // Create clones for remaining windows that should be
        // visible in the Overview
        this._windows = [];
        this._windowOverlays = [];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        // A filter for what windows we display
        this._showOnlyWindows = null;

        // Track window changes
        this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                          Lang.bind(this, this._windowAdded));
        this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                            Lang.bind(this, this._windowRemoved));
        this._repositionWindowsId = 0;

        this._visible = false;

        this.leavingOverview = false;
    },

    _lookupIndex: function (metaWindow) {
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].metaWindow == metaWindow) {
                return i;
            }
        }
        return -1;
    },

    _onParentSet: function(actor, old_parent) {
        let new_parent = this.actor.get_parent();
        if (new_parent == null)
            return;

        if (old_parent)
            this._windowOverlaysGroup.reparent(new_parent);
        else
            new_parent.add_actor(this._windowOverlaysGroup);
    },

    /**
     * lookupCloneForMetaWindow:
     * @metaWindow: A #MetaWindow
     *
     * Given a #MetaWindow instance, find the WindowClone object
     * which represents it in the workspaces display.
     */
    lookupCloneForMetaWindow: function (metaWindow) {
        let index = this._lookupIndex (metaWindow);
        return index < 0 ? null : this._windows[index];
    },

    containsMetaWindow: function (metaWindow) {
        return this._lookupIndex(metaWindow) >= 0;
    },

    setShowOnlyWindows: function(showOnlyWindows, reposition) {
        this._showOnlyWindows = showOnlyWindows;
        this._resetCloneVisibility();
        if (reposition)
            this.positionWindows(WindowPositionFlags.ANIMATE);
    },

    /**
     * setLightboxMode:
     * @showLightbox: If true, dim background and allow highlighting a specific window
     *
     * This function also resets the highlighted window state.
     */
    setLightboxMode: function (showLightbox) {
        if (!this._lightbox)
            this._lightbox = new Lightbox.Lightbox(this.actor,
                                                   { fadeTime: LIGHTBOX_FADE_TIME });

        if (showLightbox)
            this._lightbox.show();
        else
            this._lightbox.hide();
    },

    /**
     * setHighlightWindow:
     * @metaWindow: A #MetaWindow
     *
     * Draw the user's attention to the given window @metaWindow.
     */
    setHighlightWindow: function (metaWindow) {
        if (!this._lightbox)
            return;

        let actor;
        if (metaWindow != null) {
            let clone = this.lookupCloneForMetaWindow(metaWindow);
            actor = clone.actor;
        }
        this._lightbox.highlight(actor);
    },

    /**
     * setReactive:
     * @reactive: %true iff the workspace should be reactive
     *
     * Set the workspace (desktop) reactive
     **/
    setReactive: function(reactive) {
        this.actor.reactive = reactive;
    },

    _isCloneVisible: function(clone) {
        return this._showOnlyWindows == null || (clone.metaWindow in this._showOnlyWindows);
    },

    /**
     * _getVisibleClones:
     *
     * Returns a list WindowClone objects where the clone isn't filtered
     * out by any application filter.
     * The returned array will always be newly allocated; it is not in any
     * defined order, and thus it's convenient to call .sort() with your
     * choice of sorting function.
     */
    _getVisibleClones: function() {
        let visible = [];

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];

            if (!this._isCloneVisible(clone))
                continue;

            visible.push(clone);
        }
        return visible;
    },

    _resetCloneVisibility: function () {
        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            let overlay = this._windowOverlays[i];

            if (!this._isCloneVisible(clone)) {
                clone.actor.hide();
                overlay.hide();
            } else {
                clone.actor.show();
            }
        }
    },

    // Only use this for n <= 20 say
    _factorial: function(n) {
        let result = 1;
        for (let i = 2; i <= n; i++)
            result *= i;
        return result;
    },

    /**
     * _permutation:
     * @permutationIndex: An integer from [0, list.length!)
     * @list: (inout): Array of objects to permute; will be modified in place
     *
     * Given an integer between 0 and length of array, re-order the array in-place
     * into a permutation denoted by the index.
     */
    _permutation: function(permutationIndex, list) {
        for (let j = 2; j <= list.length; j++) {
            let firstIndex = (permutationIndex % j);
            let secondIndex = j - 1;
            // Swap
            let tmp = list[firstIndex];
            list[firstIndex] = list[secondIndex];
            list[secondIndex] = tmp;
            permutationIndex = Math.floor(permutationIndex / j);
        }
    },

    /**
     * _forEachPermutations:
     * @list: Array
     * @func: Function which takes a single array argument
     *
     * Call @func with each permutation of @list as an argument.
     */
    _forEachPermutations: function(list, func) {
        let nCombinations = this._factorial(list.length);
        for (let i = 0; i < nCombinations; i++) {
            let listCopy = list.concat();
            this._permutation(i, listCopy);
            func(listCopy);
        }
    },

    /**
     * _computeWindowMotion:
     * @actor: A #WindowClone's #ClutterActor
     * @slot: An element of #POSITIONS
     * @slotGeometry: Layout of @slot
     *
     * Returns a number corresponding to how much perceived motion
     * would be involved in moving the window to the given slot.
     * Currently this is the square of the distance between the
     * centers.
     */
    _computeWindowMotion: function (actor, slot) {
        let [xCenter, yCenter, fraction] = slot;
        let xDelta, yDelta, distanceSquared;
        let actorWidth, actorHeight;

        actorWidth = actor.width * actor.scale_x;
        actorHeight = actor.height * actor.scale_y;
        xDelta = actor.x + actorWidth / 2.0 - xCenter * global.screen_width;
        yDelta = actor.y + actorHeight / 2.0 - yCenter * global.screen_height;
        distanceSquared = xDelta * xDelta + yDelta * yDelta;

        return distanceSquared;
    },

    /**
     * _orderWindowsPermutations:
     *
     * Iterate over all permutations of the windows, and determine the
     * permutation which has the least total motion.
     */
    _orderWindowsPermutations: function (clones, slots) {
        let minimumMotionPermutation = null;
        let minimumMotion = -1;
        let permIndex = 0;
        this._forEachPermutations(clones, Lang.bind(this, function (permutation) {
            let motion = 0;
            for (let i = 0; i < permutation.length; i++) {
                let cloneActor = permutation[i].actor;
                let slot = slots[i];

                let delta = this._computeWindowMotion(cloneActor, slot);

                motion += delta;
            }

            if (minimumMotionPermutation == null || motion < minimumMotion) {
                minimumMotionPermutation = permutation;
                minimumMotion = motion;
            }
            permIndex++;
        }));
        return minimumMotionPermutation;
    },

    /**
     * _orderWindowsGreedy:
     *
     * Iterate over available slots in order, placing into each one the window
     * we find with the smallest motion to that slot.
     */
    _orderWindowsGreedy: function(clones, slots) {
        let result = [];
        let slotIndex = 0;
        // Copy since we mutate below
        let clonesCopy = clones.concat();
        for (let i = 0; i < slots.length; i++) {
            let slot = slots[i];
            let minimumMotionIndex = -1;
            let minimumMotion = -1;
            for (let j = 0; j < clonesCopy.length; j++) {
                let cloneActor = clonesCopy[j].actor;
                let delta = this._computeWindowMotion(cloneActor, slot);
                if (minimumMotionIndex == -1 || delta < minimumMotion) {
                    minimumMotionIndex = j;
                    minimumMotion = delta;
                }
            }
            result.push(clonesCopy[minimumMotionIndex]);
            clonesCopy.splice(minimumMotionIndex, 1);
        }
        return result;
    },

    /**
     * _orderWindowsByMotionAndStartup:
     * @windows: Array of #MetaWindow
     * @slots: Array of slots
     *
     * Returns a copy of @windows, ordered in such a way that they require least motion
     * to move to the final screen coordinates of @slots.  Ties are broken in a stable
     * fashion by the order in which the windows were created.
     */
    _orderWindowsByMotionAndStartup: function(clones, slots) {
        clones.sort(function(w1, w2) {
            return w2.metaWindow.get_stable_sequence() - w1.metaWindow.get_stable_sequence();
        });
        if (clones.length <= POSITIONING_PERMUTATIONS_MAX)
            return this._orderWindowsPermutations(clones, slots);
        else
            return this._orderWindowsGreedy(clones, slots);
    },

    /**
     * _getSlotRelativeGeometry:
     * @slot: A layout slot
     *
     * Returns: the workspace-relative [x, y, width, height]
     * of a given window layout slot.
     */
    _getSlotRelativeGeometry: function(slot) {
        let [xCenter, yCenter, fraction] = slot;

        let width = global.screen_width * fraction;
        let height = global.screen_height * fraction;

        let x = xCenter * global.screen_width - width / 2;
        let y = yCenter * global.screen_height - height / 2;

        return [x, y, width, height];
    },

    /**
     * _computeWindowRelativeLayout:
     * @metaWindow: A #MetaWindow
     * @slot: A layout slot
     *
     * Given a window and slot to fit it in, compute its
     * workspace-relative [x, y, scale] where scale applies
     * to both X and Y directions.
     */
    _computeWindowRelativeLayout: function(metaWindow, slot) {
        let [xCenter, yCenter, fraction] = slot;
        let [x, y, width, height] = this._getSlotRelativeGeometry(slot);

        xCenter = xCenter * global.screen_width;

        let rect = new Meta.Rectangle();
        metaWindow.get_outer_rect(rect);
        let buttonOuterHeight, captionHeight;
        let buttonOuterWidth = 0;

        if (this._windowOverlays[0]) {
            [buttonOuterHeight, captionHeight] = this._windowOverlays[0].chromeHeights();
            buttonOuterWidth = this._windowOverlays[0].chromeWidth() / this.scale;
        } else
            [buttonOuterHeight, captionHeight] = [0, 0];
        buttonOuterHeight /= this.scale;
        captionHeight /= this.scale;

        let desiredWidth = global.screen_width * fraction;
        let desiredHeight = global.screen_height * fraction;
        let scale = Math.min((desiredWidth - buttonOuterWidth) / rect.width,
                             (desiredHeight - buttonOuterHeight - captionHeight) / rect.height,
                             1.0 / this.scale);

        x = Math.floor(xCenter - 0.5 * scale * rect.width);

        // We want to center the window in case we have just one
        if (metaWindow.get_workspace().n_windows == 1)
            y = Math.floor(yCenter * global.screen_height - 0.5 * scale * rect.height);
        else
            y = Math.floor(y + height - rect.height * scale - captionHeight);

        return [x, y, scale];
    },

    setReservedSlot: function(clone) {
        if (clone && this.containsMetaWindow(clone.metaWindow)) {
            this._reservedSlot = null;
            this.positionWindows(WindowPositionFlags.ANIMATE);
            return;
        }
        if (clone)
            this._reservedSlot = clone;
        else
            this._reservedSlot = null;
        this.positionWindows(WindowPositionFlags.ANIMATE);
    },

    /**
     * positionWindows:
     * @flags:
     *  ZOOM - workspace is moving at the same time and we need to take that into account.
     *  ANIMATE - Indicates that we need animate changing position.
     */
    positionWindows : function(flags) {
        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }

        let totalVisible = 0;

        let visibleClones = this._getVisibleClones();
        if (this._reservedSlot)
            visibleClones.push(this._reservedSlot);

        let workspaceZooming = flags & WindowPositionFlags.ZOOM;
        let animate = flags & WindowPositionFlags.ANIMATE;

        // Start the animations
        let slots = this._computeAllWindowSlots(visibleClones.length);
        visibleClones = this._orderWindowsByMotionAndStartup(visibleClones, slots);

        for (let i = 0; i < visibleClones.length; i++) {
            let slot = slots[i];
            let clone = visibleClones[i];
            let metaWindow = clone.metaWindow;
            let mainIndex = this._lookupIndex(metaWindow);
            let overlay = this._windowOverlays[mainIndex];

            // Positioning a window currently being dragged must be avoided;
            // we'll just leave a blank spot in the layout for it.
            if (clone.inDrag)
                continue;

            let [x, y, scale] = this._computeWindowRelativeLayout(metaWindow, slot);

            if (overlay)
                overlay.hide();
            if (animate) {
                if (!metaWindow.showing_on_its_workspace()) {
                    /* Hidden windows should fade in and grow
                     * therefore we need to resize them now so they
                     * can be scaled up later */
                     if (workspaceZooming) {
                         clone.actor.opacity = 0;
                         clone.actor.scale_x = 0;
                         clone.actor.scale_y = 0;
                         clone.actor.x = x;
                         clone.actor.y = y;
                     }

                     // Make the window slightly transparent to indicate it's hidden
                     Tweener.addTween(clone.actor,
                                      { opacity: 255,
                                        time: Overview.ANIMATION_TIME,
                                        transition: 'easeInQuad'
                                      });
                }

                Tweener.addTween(clone.actor,
                                 { x: x,
                                   y: y,
                                   scale_x: scale,
                                   scale_y: scale,
                                   workspace_relative: workspaceZooming ? this : null,
                                   time: Overview.ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete: Lang.bind(this, function() {
                                      this._fadeInWindowOverlay(clone, overlay);
                                   })
                                 });
            } else {
                clone.actor.set_position(x, y);
                clone.actor.set_scale(scale, scale);
                this._fadeInWindowOverlay(clone, overlay);
            }
        }
    },

    syncStacking: function(stackIndices) {
        let desktopClone = this._windows[0];

        let visibleClones = this._getVisibleClones();
        visibleClones.sort(function (a, b) { return stackIndices[a.metaWindow.get_stable_sequence()] - stackIndices[b.metaWindow.get_stable_sequence()]; });

        for (let i = 0; i < visibleClones.length; i++) {
            let clone = visibleClones[i];
            let metaWindow = clone.metaWindow;
            if (i == 0) {
                clone.setStackAbove(desktopClone.actor);
            } else {
                let previousClone = visibleClones[i - 1];
                clone.setStackAbove(previousClone.actor);
            }
        }
    },

    _fadeInWindowOverlay: function(clone, overlay) {
        if (clone.inDrag)
            return;

        // This is a little messy and complicated because when we
        // start the fade-in we may not have done the final positioning
        // of the workspaces. (Tweener doesn't necessarily finish
        // all animations before calling onComplete callbacks.)
        // So we need to manually compute where the window will
        // be after the workspace animation finishes.
        let [cloneX, cloneY] = clone.actor.get_position();
        let [cloneWidth, cloneHeight] = clone.actor.get_size();
        cloneX = this.x + this.scale * cloneX;
        cloneY = this.y + this.scale * cloneY;
        cloneWidth = this.scale * clone.actor.scale_x * cloneWidth;
        cloneHeight = this.scale * clone.actor.scale_y * cloneHeight;

        if (!this._windowOverlaysGroup.visible)
            this._windowOverlaysGroup.show();

        if (overlay) {
            overlay.updatePositions(cloneX, cloneY, cloneWidth, cloneHeight);
            overlay.fadeIn();
        }
    },

    _fadeInAllOverlays: function() {
        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            let overlay = this._windowOverlays[i];
            if (this._showOnlyWindows != null && !(clone.metaWindow in this._showOnlyWindows))
                continue;
            this._fadeInWindowOverlay(clone, overlay);
        }
    },

    _hideAllOverlays: function() {
        for (let i = 0; i < this._windows.length; i++) {
            let overlay = this._windowOverlays[i];
            overlay.hide();
        }
    },

    _delayedWindowRepositioning: function() {
        if (this._windowIsZooming)
            return true;

        let [x, y, mask] = global.get_pointer();
        let wsWidth = this.actor.width * this.scale;
        let wsHeight = this.actor.height * this.scale;

        let pointerHasMoved = (this._cursorX != x && this._cursorY != y);
        let inWorkspace = (this.x < x && x < this.x + wsWidth &&
                           this.y < y && y < this.y + wsHeight);

        if (pointerHasMoved && inWorkspace) {
            // store current cursor position
            this._cursorX = x;
            this._cursorY = y;
            return true;
        }

        this.positionWindows(WindowPositionFlags.ANIMATE);
        return false;
    },

    showWindowsOverlays: function() {
        this._windowOverlaysGroup.show();
        this._fadeInAllOverlays();
    },

    hideWindowsOverlays: function() {
        this._windowOverlaysGroup.hide();
    },

    _windowRemoved : function(metaWorkspace, metaWin) {
        let win = metaWin.get_compositor_private();

        // find the position of the window in our list
        let index = this._lookupIndex (metaWin);

        if (index == -1)
            return;

        let clone = this._windows[index];

        this._windows.splice(index, 1);
        this._windowOverlays.splice(index, 1);

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

        this._repositionWindowsId = Mainloop.timeout_add(750,
            Lang.bind(this, this._delayedWindowRepositioning));
    },

    _windowAdded : function(metaWorkspace, metaWin) {
        if (this.leavingOverview)
            return;

        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            Mainloop.idle_add(Lang.bind(this,
                                        function () {
                                            if (this.actor && metaWin.get_compositor_private())
                                                this._windowAdded(metaWorkspace, metaWin);
                                            return false;
                                        }));
            return;
        }

        if (!this._isOverviewWindow(win))
            return;

        let clone = this._addWindowClone(win);

        if (win._overviewHint) {
            let x = (win._overviewHint.x - this.actor.x) / this.scale;
            let y = (win._overviewHint.y - this.actor.y) / this.scale;
            let scale = win._overviewHint.scale / this.scale;
            delete win._overviewHint;

            clone.actor.set_position (x, y);
            clone.actor.set_scale (scale, scale);
        }

        this.positionWindows(WindowPositionFlags.ANIMATE);
    },

    // check for maximized windows on the workspace
    hasMaximizedWindows: function() {
        for (let i = 0; i < this._windows.length; i++) {
            let metaWindow = this._windows[i].metaWindow;
            if (metaWindow.showing_on_its_workspace() &&
                metaWindow.maximized_horizontally &&
                metaWindow.maximized_vertically)
                return true;
        }
        return false;
    },

    // Animate the full-screen to Overview transition.
    zoomToOverview : function() {
        this.actor.set_position(this.x, this.y);
        this.actor.set_scale(this.scale, this.scale);

        // Position and scale the windows.
        if (Main.overview.animationInProgress)
            this.positionWindows(WindowPositionFlags.ANIMATE | WindowPositionFlags.ZOOM);
        else
            this.positionWindows(WindowPositionFlags.ZOOM);

        this._visible = true;
    },

    // Animates the return from Overview mode
    zoomFromOverview : function() {
        this.leavingOverview = true;

        this._hideAllOverlays();

        if (this._repositionWindowsId > 0) {
            Mainloop.source_remove(this._repositionWindowsId);
            this._repositionWindowsId = 0;
        }
        this._overviewHiddenId = Main.overview.connect('hidden', Lang.bind(this,
                                                                           this._doneLeavingOverview));

        // Position and scale the windows.
        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];

            clone.zoomFromOverview();

            if (clone.metaWindow.showing_on_its_workspace()) {
                Tweener.addTween(clone.actor,
                                 { x: clone.origX,
                                   y: clone.origY,
                                   scale_x: 1.0,
                                   scale_y: 1.0,
                                   workspace_relative: this,
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
                                   workspace_relative: this,
                                   time: Overview.ANIMATION_TIME,
                                   transition: 'easeOutQuad'
                                 });
            }
        }

        this._visible = false;
    },

    destroy : function() {
        this.actor.destroy();
    },

    _onDestroy: function(actor) {
        if (this._overviewHiddenId) {
            Main.overview.disconnect(this._overviewHiddenId);
            this._overviewHiddenId = 0;
        }
        Tweener.removeTweens(actor);

        this.metaWorkspace.disconnect(this._windowAddedId);
        this.metaWorkspace.disconnect(this._windowRemovedId);

        if (this._repositionWindowsId > 0)
            Mainloop.source_remove(this._repositionWindowsId);

        // Usually, the windows will be destroyed automatically with
        // their parent (this.actor), but we might have a zoomed window
        // which has been reparented to the stage - _windows[0] holds
        // the desktop window, which is never reparented
        for (let w = 0; w < this._windows.length; w++)
            this._windows[w].destroy();
        this._windows = [];
    },

    // Sets this.leavingOverview flag to false.
    _doneLeavingOverview : function() {
        this.leavingOverview = false;
    },

    // Tests if @win belongs to this workspaces
    _isMyWindow : function (win) {
        return win.get_workspace() == this.metaWorkspace.index() ||
            (win.get_meta_window() && win.get_meta_window().is_on_all_workspaces());
    },

    // Tests if @win should be shown in the Overview
    _isOverviewWindow : function (win) {
        let tracker = Shell.WindowTracker.get_default();
        return tracker.is_window_interesting(win.get_meta_window());
    },

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone : function(win) {
        let clone = new WindowClone(win);
        let overlay = new WindowOverlay(clone, this._windowOverlaysGroup);

        clone.connect('selected',
                      Lang.bind(this, this._onCloneSelected));
        clone.connect('drag-begin',
                      Lang.bind(this, function(clone) {
                          Main.overview.beginWindowDrag();
                          overlay.hide();
                      }));
        clone.connect('drag-end',
                      Lang.bind(this, function(clone) {
                          Main.overview.endWindowDrag();
                          overlay.show();
                      }));
        clone.connect('zoom-start',
                      Lang.bind(this, function() {
                          this._windowIsZooming = true;
                      }));
        clone.connect('zoom-end',
                      Lang.bind(this, function() {
                          this._windowIsZooming = false;
                      }));

        this.actor.add_actor(clone.actor);

        overlay.connect('show-close-button', Lang.bind(this, this._onShowOverlayClose));

        this._windows.push(clone);
        this._windowOverlays.push(overlay);

        return clone;
    },

    _onShowOverlayClose: function (windowOverlay) {
        for (let i = 0; i < this._windowOverlays.length; i++) {
            let overlay = this._windowOverlays[i];
            if (overlay == windowOverlay)
                continue;
            overlay.hideCloseButton();
        }
    },

    _computeWindowSlot : function(windowIndex, numberOfWindows) {
        if (numberOfWindows in POSITIONS)
            return POSITIONS[numberOfWindows][windowIndex];

        // If we don't have a predefined scheme for this window count,
        // arrange the windows in a grid pattern.
        let gridWidth = Math.ceil(Math.sqrt(numberOfWindows));
        let gridHeight = Math.ceil(numberOfWindows / gridWidth);

        let fraction = 0.95 * (1. / gridWidth);

        let xCenter = (.5 / gridWidth) + ((windowIndex) % gridWidth) / gridWidth;
        let yCenter = (.5 / gridHeight) + Math.floor((windowIndex / gridWidth)) / gridHeight;

        return [xCenter, yCenter, fraction];
    },

    _computeAllWindowSlots: function(totalWindows) {
        let slots = [];
        for (let i = 0; i < totalWindows; i++) {
            slots.push(this._computeWindowSlot(i, totalWindows));
        }
        return slots;
    },

    _onCloneSelected : function (clone, time) {
        Main.activateWindow(clone.metaWindow, time,
                            this.metaWorkspace.index());
    },

    _removeSelf : function(actor, event) {
        screen.remove_workspace(this.metaWorkspace, event.get_time());
        return true;
    },

    // Draggable target interface
    handleDragOver : function(source, actor, x, y, time) {
        if (source instanceof WindowClone)
            return DND.DragMotionResult.MOVE_DROP;
        if (source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    },

    acceptDrop : function(source, actor, x, y, time) {
        if (source instanceof WindowClone) {
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
            metaWindow.change_workspace_by_index(this.metaWorkspace.index(),
                                                 false, // don't create workspace
                                                 time);
            return true;
        } else if (source.shellWorkspaceLaunch) {
            this.metaWorkspace.activate(time);
            source.shellWorkspaceLaunch();
            return true;
        }

        return false;
    }
};

Signals.addSignalMethods(Workspace.prototype);

// Create a SpecialPropertyModifier to let us move windows in a
// straight line on the screen even though their containing workspace
// is also moving.
Tweener.registerSpecialPropertyModifier('workspace_relative', _workspaceRelativeModifier, _workspaceRelativeGet);

function _workspaceRelativeModifier(workspace) {
    let [startX, startY] = Main.overview.getPosition();
    let overviewPosX, overviewPosY, overviewScale;

    if (!workspace)
        return [];

    if (workspace.leavingOverview) {
        let [zoomedInX, zoomedInY] = Main.overview.getZoomedInPosition();
        overviewPosX = { begin: startX, end: zoomedInX };
        overviewPosY = { begin: startY, end: zoomedInY };
        overviewScale = { begin: Main.overview.getScale(),
                          end: Main.overview.getZoomedInScale() };
    } else {
        overviewPosX = { begin: startX, end: 0 };
        overviewPosY = { begin: startY, end: 0 };
        overviewScale = { begin: Main.overview.getScale(), end: 1 };
    }

    return [ { name: 'x',
               parameters: { workspacePos: workspace.x,
                             overviewPos: overviewPosX,
                             overviewScale: overviewScale } },
             { name: 'y',
               parameters: { workspacePos: workspace.y,
                             overviewPos: overviewPosY,
                             overviewScale: overviewScale } }
           ];
}

function _workspaceRelativeGet(begin, end, time, params) {
    let curOverviewPos = (1 - time) * params.overviewPos.begin +
                         time * params.overviewPos.end;
    let curOverviewScale = (1 - time) * params.overviewScale.begin +
                           time * params.overviewScale.end;

    // Calculate the screen position of the window.
    let screen = (1 - time) *
                 ((begin + params.workspacePos) * params.overviewScale.begin +
                  params.overviewPos.begin) +
                 time *
                 ((end + params.workspacePos) * params.overviewScale.end +
                 params.overviewPos.end);

    // Return the workspace coordinates.
    return (screen - curOverviewPos) / curOverviewScale - params.workspacePos;
}
