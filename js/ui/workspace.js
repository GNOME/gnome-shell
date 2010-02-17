/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gdk = imports.gi.Gdk;
const Gtk = imports.gi.Gtk;
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

const FRAME_COLOR = new Clutter.Color();
FRAME_COLOR.from_pixel(0xffffffff);

const SCROLL_SCALE_AMOUNT = 100 / 5;

const ZOOM_OVERLAY_FADE_TIME = 0.15;

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

// Spacing between workspaces. At the moment, the same spacing is used
// in both zoomed-in and zoomed-out views; this is slightly
// metaphor-breaking, but the alternatives are also weird.
const GRID_SPACING = 15;
const FRAME_SIZE = GRID_SPACING / 3;

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

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onEnter));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onLeave));
        this._havePointer = false;

        this._draggable = DND.makeDraggable(this.actor);
        this._draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        this._draggable.connect('drag-end', Lang.bind(this, this._onDragEnd));
        this._inDrag = false;

        this._zooming = false;
    },

    setStackAbove: function (actor) {
        this._stackAbove = actor;
        if (this._inDrag || this._zooming)
            // We'll fix up the stack after the drag/zooming
            return;
        this.actor.raise(this._stackAbove);
    },

    destroy: function () {
        this.actor.destroy();
    },

    _onEnter: function (actor, event) {
        // If the user drags faster than we can follow, he'll end up
        // leaving the window temporarily and then re-entering it
        if (this._inDrag)
            return;

        this._havePointer = true;
    },

    _onLeave: function (actor, event) {
        // If the user drags faster than we can follow, he'll end up
        // leaving the window temporarily and then re-entering it
        if (this._inDrag)
            return;

        this._havePointer = false;

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

        this._zoomLightbox = new Lightbox.Lightbox(global.stage, false);

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
        this.actor.raise(this._stackAbove);

        [this.actor.x, this.actor.y]             = this._zoomLocalOrig.getPosition();
        [this.actor.scale_x, this.actor.scale_y] = this._zoomLocalOrig.getScale();

        this._zoomLightbox.destroy();

        this._zoomLocalPosition  = undefined;
        this._zoomLocalScale     = undefined;
        this._zoomGlobalPosition = undefined;
        this._zoomGlobalScale    = undefined;
        this._zoomTargetPosition = undefined;
        this._zoomStep           = undefined;
        this._zoomLightbox       = undefined;
    },

    _onButtonRelease : function (actor, event) {
        this.emit('selected', event.get_time());
    },

    _onDragBegin : function (draggable, time) {
        this._inDrag = true;
        this.emit('drag-begin');
    },

    _onDragEnd : function (draggable, time, snapback) {
        this._inDrag = false;

        // Most likely, the clone is going to move away from the
        // pointer now. But that won't cause a leave-event, so
        // do this by hand. Of course, if the window only snaps
        // back a short distance, this might be wrong, but it's
        // better to have the label mysteriously missing than
        // mysteriously present
        this._havePointer = false;

        // We may not have a parent if DnD completed successfully, in
        // which case our clone will shortly be destroyed and replaced
        // with a new one on the target workspace.
        if (this.actor.get_parent() != null)
            this.actor.raise(this._stackAbove);

        this.emit('drag-end');
    }
};

Signals.addSignalMethods(WindowClone.prototype);


function DesktopClone(window) {
    this._init(window);
}

DesktopClone.prototype = {
    _init : function(window) {
        this.actor = new Clutter.Group({ reactive: true });

        let background = new Clutter.Clone({ source: Main.background.source });
        this.actor.add_actor(background);

        if (window) {
            this._desktop = new Clutter.Clone({ source: window.get_texture() });
            this.actor.add_actor(this._desktop);
            this._desktop.hide();
        } else {
            this._desktop = null;
        }

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));
    },

    zoomFromOverview: function(fadeInIcons) {
        if (this._desktop == null)
            return;

        if (fadeInIcons) {
            this._desktop.opacity = 0;
            this._desktop.show();
            Tweener.addTween(this._desktop,
                             { opacity: 255,
                               time: Overview.ANIMATION_TIME,
                               transition: "easeOutQuad" });
        }
    },

    zoomToOverview: function(fadeOutIcons) {
        if (this._desktop == null)
            return;

        if (fadeOutIcons) {
            this._desktop.opacity = 255;
            this._desktop.show();
            Tweener.addTween(this._desktop,
                             { opacity: 0,
                               time: Overview.ANIMATION_TIME,
                               transition: "easeOutQuad",
                               onComplete: Lang.bind(this,
                                   function() {
                                       this._desktop.hide();
                                   })
                             });
        } else {
            this._desktop.hide();
        }
    },

    _onButtonRelease : function (actor, event) {
        this.emit('selected', event.get_time());
    }
};

Signals.addSignalMethods(DesktopClone.prototype);


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

        let title = new St.Label({ style_class: "window-caption",
                                   text: metaWindow.title });
        title.connect('style-changed',
                      Lang.bind(this, this._onStyleChanged));
        title.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        title._spacing = 0;

        this._updateCaptionId = metaWindow.connect('notify::title',
            Lang.bind(this, function(w) {
                this.title.text = w.title;
            }));

        let button = new St.Bin({ style_class: "window-close",
                                  reactive: true });
        button.connect('style-changed',
                       Lang.bind(this, this._onStyleChanged));
        button._overlap = 0;

        windowClone.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        windowClone.actor.connect('enter-event',
                                  Lang.bind(this, this._onEnter));
        windowClone.actor.connect('leave-event',
                                  Lang.bind(this, this._onLeave));

        this._idleToggleCloseId = 0;
        button.connect('button-release-event',
                       Lang.bind(this, this._closeWindow));

        this._windowAddedId = 0;
        windowClone.connect('zoom-start', Lang.bind(this, this.hide));
        windowClone.connect('zoom-end', Lang.bind(this, this.show));

        button.hide();

        this.title = title;
        this.closeButton = button;

        parentActor.add_actor(this.title);
        parentActor.add_actor(this.closeButton);
    },

    hide: function() {
        this.closeButton.hide();
        this.title.hide();
    },

    show: function() {
        let [child, x, y, mask] = Gdk.Screen.get_default().get_root_window().get_pointer();
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
                          time: Overview.ANIMATION_TIME,
                          transition: "easeOutQuad" });
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

    _closeWindow: function(actor, event) {
        let metaWindow = this._windowClone.metaWindow;
        this._workspace = metaWindow.get_workspace();

        this._windowAddedId = this._workspace.connect('window-added',
                                                      Lang.bind(this,
                                                                this._onWindowAdded));

        metaWindow.delete(event.get_time());
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
        let [child, x, y, mask] = Gdk.Screen.get_default().get_root_window().get_pointer();
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

        let [success, len] = titleNode.get_length('-shell-caption-spacing',
                                                  false);
        if (success)
            this.title._spacing = len;

        let closeNode = this.closeButton.get_theme_node();

        [success, len] = closeNode.get_length('-shell-close-overlap',
                                              false);
        if (success)
            this.closeButton._overlap = len;

        this._parentActor.queue_relayout();
    }
};

Signals.addSignalMethods(WindowOverlay.prototype);

const WindowPositionFlags = {
    ZOOM: 1 << 0,
    ANIMATE: 1 << 1
};

/**
 * @workspaceNum: Workspace index
 * @parentActor: The actor which will be the parent of this workspace;
 *               we need this in order to add chrome such as the icons
 *               on top of the windows without having them be scaled.
 */
function Workspace(workspaceNum, parentActor) {
    this._init(workspaceNum, parentActor);
}

Workspace.prototype = {
    _init : function(workspaceNum, parentActor) {
        this.workspaceNum = workspaceNum;
        this._windowOverlaysGroup = new Clutter.Group();
        // Without this the drop area will be overlapped.
        this._windowOverlaysGroup.set_size(0, 0);

        this._metaWorkspace = global.screen.get_workspace_by_index(workspaceNum);

        parentActor.add_actor(this._windowOverlaysGroup);
        this._parentActor = parentActor;

        this.actor = new Clutter.Group();
        this.actor._delegate = this;
        // Auto-sizing is unreliable in the presence of ClutterClone, so rather than
        // implicitly counting on the workspace actor to be sized to the size of the
        // included desktop actor clone, set the size explicitly to the screen size.
        // See http://bugzilla.openedhand.com/show_bug.cgi?id=1755
        this.actor.width = global.screen_width;
        this.actor.height = global.screen_height;
        this.scale = 1.0;

        let windows = global.get_windows().filter(this._isMyWindow, this);

        // Find the desktop window
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_window_type() == Meta.WindowType.DESKTOP) {
                this._desktop = new DesktopClone(windows[i]);
                break;
            }
        }
        // If there wasn't one, fake it
        if (!this._desktop)
            this._desktop = new DesktopClone();

        this._desktop.connect('selected',
                              Lang.bind(this,
                                        function(clone, time) {
                                            this._metaWorkspace.activate(time);
                                            Main.overview.hide();
                                        }));
        this.actor.add_actor(this._desktop.actor);

        // Create clones for remaining windows that should be
        // visible in the Overview
        this._windows = [this._desktop];
        this._windowOverlays = [ null ];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        // A filter for what windows we display
        this._showOnlyWindows = null;

        // Track window changes
        this._windowAddedId = this._metaWorkspace.connect('window-added',
                                                          Lang.bind(this, this._windowAdded));
        this._windowRemovedId = this._metaWorkspace.connect('window-removed',
                                                            Lang.bind(this, this._windowRemoved));

        this._visible = false;

        this._frame = null;

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
        if (showLightbox) {
            this._lightbox = new Lightbox.Lightbox(this.actor, false);
        } else {
            this._lightbox.destroy();
            this._lightbox = null;
        }
        if (this._frame) {
            this._frame.set_opacity(showLightbox ? 150 : 255);
        }
    },

    /**
     * setHighlightWindow:
     * @metaWindow: A #MetaWindow
     *
     * Draw the user's attention to the given window @metaWindow.
     */
    setHighlightWindow: function (metaWindow) {
        let actor;
        if (metaWindow != null) {
            let clone = this.lookupCloneForMetaWindow(metaWindow);
            actor = clone.actor;
        }
        this._lightbox.highlight(actor);
    },

    // Mark the workspace selected/not-selected
    setSelected : function(selected) {
        // Don't draw a frame if we only have one workspace
        if (selected && global.screen.n_workspaces > 1) {
            if (this._frame)
                return;

            // FIXME: do something cooler-looking using clutter-cairo
            this._frame = new Clutter.Rectangle({ color: FRAME_COLOR });
            this.actor.add_actor(this._frame);
            this._frame.set_position(this._desktop.actor.x - FRAME_SIZE / this.actor.scale_x,
                                     this._desktop.actor.y - FRAME_SIZE / this.actor.scale_y);
            this._frame.set_size(this._desktop.actor.width + 2 * FRAME_SIZE / this.actor.scale_x,
                                 this._desktop.actor.height + 2 * FRAME_SIZE / this.actor.scale_y);
            this._frame.lower_bottom();

            this._framePosHandler = this.actor.connect('notify::scale-x', Lang.bind(this, this._updateFramePosition));
        } else {
            if (!this._frame)
                return;
            this.actor.disconnect(this._framePosHandler);
            this._frame.destroy();
            this._frame = null;
        }
    },

    _updateFramePosition : function() {
        this._frame.set_position(this._desktop.actor.x - FRAME_SIZE / this.actor.scale_x,
                                 this._desktop.actor.y - FRAME_SIZE / this.actor.scale_y);
        this._frame.set_size(this._desktop.actor.width + 2 * FRAME_SIZE / this.actor.scale_x,
                             this._desktop.actor.height + 2 * FRAME_SIZE / this.actor.scale_y);
    },

    _isCloneVisible: function(clone) {
        return this._showOnlyWindows == null || (clone.metaWindow in this._showOnlyWindows);
    },

    /**
     * _getVisibleClones:
     *
     * Returns a list WindowClone objects where the clone isn't filtered
     * out by any application filter.  The clone for the desktop is excluded.
     * The returned array will always be newly allocated; it is not in any
     * defined order, and thus it's convenient to call .sort() with your
     * choice of sorting function.
     */
    _getVisibleClones: function() {
        let visible = [];

        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];

            if (!this._isCloneVisible(clone))
                continue;

            visible.push(clone);
        }
        return visible;
    },

    _getVisibleWindows: function() {
        return this._getVisibleClones().map(function (clone) { return clone.metaWindow; });
    },

    _resetCloneVisibility: function () {
        for (let i = 1; i < this._windows.length; i++) {
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
     * @metaWindow: A #MetaWindow
     * @slot: An element of #POSITIONS
     * @slotGeometry: Layout of @slot
     *
     * Returns a number corresponding to how much perceived motion
     * would be involved in moving the window to the given slot.
     * Currently this is the square of the distance between the
     * centers.
     */
    _computeWindowMotion: function (metaWindow, slot, slotGeometry) {
        let rect = new Meta.Rectangle();
        metaWindow.get_outer_rect(rect);

        let [slotX, slotY, slotWidth, slotHeight] = slotGeometry;
        let distanceSquared;
        let xDelta, yDelta;

        xDelta = (rect.x + rect.width / 2) - (slotX + slotWidth / 2);
        yDelta = (rect.y + rect.height / 2) - (slotY + slotHeight / 2);
        distanceSquared = xDelta * xDelta + yDelta * yDelta;

        return distanceSquared;
    },

    /**
     * _orderWindowsPermutations:
     *
     * Iterate over all permutations of the windows, and determine the
     * permutation which has the least total motion.
     */
    _orderWindowsPermutations: function (windows, slots, slotGeometries) {
        let minimumMotionPermutation = null;
        let minimumMotion = -1;
        let permIndex = 0;
        this._forEachPermutations(windows, Lang.bind(this, function (permutation) {
            let motion = 0;
            for (let i = 0; i < permutation.length; i++) {
                let metaWindow = permutation[i];
                let slot = slots[i];
                let slotAbsGeometry = slotGeometries[i];

                let delta = this._computeWindowMotion(metaWindow, slot, slotAbsGeometry);

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
    _orderWindowsGreedy: function(windows, slots, slotGeometries) {
        let result = [];
        let slotIndex = 0;
        // Copy since we mutate below
        let windowCopy = windows.concat();
        for (let i = 0; i < slots.length; i++) {
            let slot = slots[i];
            let slotGeometry = slotGeometries[i];
            let minimumMotionIndex = -1;
            let minimumMotion = -1;
            for (let j = 0; j < windowCopy.length; j++) {
                let metaWindow = windowCopy[j];
                let delta = this._computeWindowMotion(metaWindow, slot, slotGeometry);
                if (minimumMotionIndex == -1 || delta < minimumMotion) {
                    minimumMotionIndex = j;
                    minimumMotion = delta;
                }
            }
            result.push(windowCopy[minimumMotionIndex]);
            windowCopy.splice(minimumMotionIndex, 1);
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
    _orderWindowsByMotionAndStartup: function(windows, slots) {
        windows.sort(function(w1, w2) {
            return w2.get_stable_sequence() - w1.get_stable_sequence();
        });
        let slotGeometries = slots.map(Lang.bind(this, this._getSlotAbsoluteGeometry));
        if (windows.length <= POSITIONING_PERMUTATIONS_MAX)
            return this._orderWindowsPermutations(windows, slots, slotGeometries);
        else
            return this._orderWindowsGreedy(windows, slots, slotGeometries);
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
     * _getSlotAbsoluteGeometry:
     * @slot: A layout slot
     *
     * Returns: the screen coordiantes [x, y, width, height]
     * of a given window layout slot.
     */
    _getSlotAbsoluteGeometry: function(slot) {
        let [x, y, width, height] = this._getSlotRelativeGeometry(slot);
        return [ this.gridX + x, this.gridY + y,
                 this.scale * width, this.scale * height];
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

        let [buttonOuterHeight, captionHeight] = this._windowOverlays[1].chromeHeights();
        buttonOuterHeight /= this.scale;
        captionHeight /= this.scale;
        let buttonOuterWidth = this._windowOverlays[1].chromeWidth() / this.scale;

        let desiredWidth = global.screen_width * fraction;
        let desiredHeight = global.screen_height * fraction;
        let scale = Math.min((desiredWidth - buttonOuterWidth) / rect.width,
                             (desiredHeight - buttonOuterHeight - captionHeight) / rect.height,
                             1.0 / this.scale);

        x = xCenter - 0.5 * scale * rect.width;
        y = y + height - rect.height * scale - captionHeight;

        return [x, y, scale];
    },

    /**
     * positionWindows:
     * @flags:
     *  ZOOM - workspace is moving at the same time and we need to take that into account.
     *  ANIMATE - Indicates that we need animate changing position.
     */
    positionWindows : function(flags) {
        let totalVisible = 0;

        let visibleWindows = this._getVisibleWindows();

        let workspaceZooming = flags & WindowPositionFlags.ZOOM;
        let animate = flags & WindowPositionFlags.ANIMATE;

        // Start the animations
        let slots = this._computeAllWindowSlots(visibleWindows.length);
        visibleWindows = this._orderWindowsByMotionAndStartup(visibleWindows, slots);

        for (let i = 0; i < visibleWindows.length; i++) {
            let slot = slots[i];
            let metaWindow = visibleWindows[i];
            let mainIndex = this._lookupIndex(metaWindow);
            let clone = metaWindow._delegate;
            let overlay = this._windowOverlays[mainIndex];

            let [x, y, scale] = this._computeWindowRelativeLayout(metaWindow, slot);

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
                                        transition: "easeInQuad"
                                      });
                }

                Tweener.addTween(clone.actor,
                                 { x: x,
                                   y: y,
                                   scale_x: scale,
                                   scale_y: scale,
                                   workspace_relative: workspaceZooming ? this : null,
                                   time: Overview.ANIMATION_TIME,
                                   transition: "easeOutQuad",
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
        // This is a little messy and complicated because when we
        // start the fade-in we may not have done the final positioning
        // of the workspaces. (Tweener doesn't necessarily finish
        // all animations before calling onComplete callbacks.)
        // So we need to manually compute where the window will
        // be after the workspace animation finishes.
        let [cloneX, cloneY] = clone.actor.get_position();
        let [cloneWidth, cloneHeight] = clone.actor.get_size();
        cloneX = this.gridX + this.scale * cloneX;
        cloneY = this.gridY + this.scale * cloneY;
        cloneWidth = this.scale * clone.actor.scale_x * cloneWidth;
        cloneHeight = this.scale * clone.actor.scale_y * cloneHeight;

        overlay.updatePositions(cloneX, cloneY, cloneWidth, cloneHeight);
        overlay.fadeIn();
    },

    _fadeInAllOverlays: function() {
        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];
            let overlay = this._windowOverlays[i];
            if (this._showOnlyWindows != null && !(clone.metaWindow in this._showOnlyWindows))
                continue;
            this._fadeInWindowOverlay(clone, overlay);
        }
    },

    _hideAllOverlays: function() {
        for (let i = 1; i< this._windows.length; i++) {
            let overlay = this._windowOverlays[i];
            overlay.hide();
        }
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

        this.positionWindows(WindowPositionFlags.ANIMATE);
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
    _haveMaximizedWindows: function() {
        for (let i = 1; i < this._windows.length; i++) {
            let metaWindow = this._windows[i].metaWindow;
            if (metaWindow.showing_on_its_workspace() &&
                metaWindow.maximized_horizontally &&
                metaWindow.maximized_vertically)
                return true;
        }
        return false;
    },

    // Animate the full-screen to Overview transition.
    zoomToOverview : function(animate) {
        this.actor.set_position(this.gridX, this.gridY);
        this.actor.set_scale(this.scale, this.scale);

        // Position and scale the windows.
        if (animate)
            this.positionWindows(WindowPositionFlags.ANIMATE | WindowPositionFlags.ZOOM);
        else
            this.positionWindows(WindowPositionFlags.ZOOM);

        let active = global.screen.get_active_workspace_index();
        let fadeInIcons = (animate &&
                           active == this.workspaceNum &&
                           !this._haveMaximizedWindows());
        this._desktop.zoomToOverview(fadeInIcons);

        this._visible = true;
    },

    // Animates the return from Overview mode
    zoomFromOverview : function() {
        this.leavingOverview = true;

        this._hideAllOverlays();

        Main.overview.connect('hidden', Lang.bind(this,
                                                 this._doneLeavingOverview));

        // Position and scale the windows.
        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];

            if (clone.metaWindow.showing_on_its_workspace()) {
                Tweener.addTween(clone.actor,
                                 { x: clone.origX,
                                   y: clone.origY,
                                   scale_x: 1.0,
                                   scale_y: 1.0,
                                   workspace_relative: this,
                                   time: Overview.ANIMATION_TIME,
                                   opacity: 255,
                                   transition: "easeOutQuad"
                                 });
            } else {
                // The window is hidden, make it shrink and fade it out
                Tweener.addTween(clone.actor,
                                 { scale_x: 0,
                                   scale_y: 0,
                                   opacity: 0,
                                   workspace_relative: this,
                                   time: Overview.ANIMATION_TIME,
                                   transition: "easeOutQuad"
                                 });
            }
        }

        let active = global.screen.get_active_workspace_index();
        let fadeOutIcons = (active == this.workspaceNum &&
                            !this._haveMaximizedWindows());
        this._desktop.zoomFromOverview(fadeOutIcons);

        this._visible = false;
    },

    // Animates grid shrinking/expanding when a row or column
    // of workspaces is added or removed
    resizeToGrid : function (oldScale) {
        this._hideAllOverlays();
        Tweener.addTween(this.actor,
                         { x: this.gridX,
                           y: this.gridY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overview.ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: Lang.bind(this, this._fadeInAllOverlays)
                         });
    },

    // Animates the addition of a new (empty) workspace
    slideIn : function(oldScale) {
        if (this.gridCol > this.gridRow) {
            this.actor.set_position(global.screen_width, this.gridY);
            this.actor.set_scale(oldScale, oldScale);
        } else {
            this.actor.set_position(this.gridX, global.screen_height);
            this.actor.set_scale(this.scale, this.scale);
        }
        Tweener.addTween(this.actor,
                         { x: this.gridX,
                           y: this.gridY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overview.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        this._visible = true;
    },

    // Animates the removal of a workspace
    slideOut : function(onComplete) {
        let destX = this.actor.x, destY = this.actor.y;

        this._hideAllOverlays();

        if (this.gridCol > this.gridRow)
            destX = global.screen_width;
        else
            destY = global.screen_height;
        Tweener.addTween(this.actor,
                         { x: destX,
                           y: destY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overview.ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: onComplete
                         });

        this._visible = false;

        // Don't let the user try to select this workspace as it's
        // making its exit.
        this._desktop.reactive = false;
    },

    destroy : function() {
        Tweener.removeTweens(this.actor);
        this.actor.destroy();
        this.actor = null;
        this._windowOverlaysGroup.destroy();

        this._metaWorkspace.disconnect(this._windowAddedId);
        this._metaWorkspace.disconnect(this._windowRemovedId);
    },

    // Sets this.leavingOverview flag to false.
    _doneLeavingOverview : function() {
        this.leavingOverview = false;
    },

    // Tests if @win belongs to this workspaces
    _isMyWindow : function (win) {
        return win.get_workspace() == this.workspaceNum ||
            (win.get_meta_window() && win.get_meta_window().is_on_all_workspaces());
    },

    // Tests if @win should be shown in the Overview
    _isOverviewWindow : function (win) {
        let tracker = Shell.WindowTracker.get_default()
        return tracker.is_window_interesting(win.get_meta_window());
    },

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone : function(win) {
        let clone = new WindowClone(win);
        let overlay = new WindowOverlay(clone, this._windowOverlaysGroup);

        clone.connect('selected',
                      Lang.bind(this, this._onCloneSelected));
        clone.connect('drag-begin',
                      Lang.bind(this, function() {
                          overlay.hide();
                      }));
        clone.connect('drag-end',
                      Lang.bind(this, function() {
                          overlay.show();
                      }));

        this.actor.add_actor(clone.actor);

        overlay.connect('show-close-button', Lang.bind(this, this._onShowOverlayClose));

        this._windows.push(clone);
        this._windowOverlays.push(overlay);

        return clone;
    },

    _onShowOverlayClose: function (windowOverlay) {
        for (let i = 1; i < this._windowOverlays.length; i++) {
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
        Main.activateWindow(clone.metaWindow, time);
    },

    _removeSelf : function(actor, event) {
        let screen = global.screen;
        let workspace = screen.get_workspace_by_index(this.workspaceNum);

        screen.remove_workspace(workspace, event.get_time());
        return true;
    },

    // Draggable target interface
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
            metaWindow.change_workspace_by_index(this.workspaceNum,
                                                 false, // don't create workspace
                                                 time);
            return true;
        } else if (source.shellWorkspaceLaunch) {
            this._metaWorkspace.activate(time);
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
               parameters: { workspacePos: workspace.gridX,
                             overviewPos: overviewPosX,
                             overviewScale: overviewScale } },
             { name: 'y',
               parameters: { workspacePos: workspace.gridY,
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
