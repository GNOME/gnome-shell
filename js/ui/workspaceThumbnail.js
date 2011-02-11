/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const WorkspacesView = imports.ui.workspacesView;

// The maximum size of a thumbnail is 1/8 the width and height of the screen
let MAX_THUMBNAIL_SCALE = 1/8.;

function WindowClone(realWindow) {
    this._init(realWindow);
}

WindowClone.prototype = {
    _init : function(realWindow) {
        this.actor = new Clutter.Clone({ source: realWindow.get_texture(),
                                         reactive: true });
        this.actor._delegate = this;
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;

        this._positionChangedId = this.realWindow.connect('position-changed',
                                                          Lang.bind(this, this._onPositionChanged));
        this._realWindowDestroyedId = this.realWindow.connect('destroy',
                                                              Lang.bind(this, this._disconnectRealWindowSignals));
        this._onPositionChanged();

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._draggable = DND.makeDraggable(this.actor,
                                            { restoreOnSuccess: true,
                                              dragActorMaxSize: Workspace.WINDOW_DND_SIZE,
                                              dragActorOpacity: Workspace.DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        this._draggable.connect('drag-end', Lang.bind(this, this._onDragEnd));
        this.inDrag = false;
    },

    setStackAbove: function (actor) {
        this._stackAbove = actor;
        if (this._stackAbove == null)
            this.actor.lower_bottom();
        else
            this.actor.raise(this._stackAbove);
    },

    destroy: function () {
        this.actor.destroy();
    },

    _onPositionChanged: function() {
        let rect = this.metaWindow.get_outer_rect();
        this.actor.set_position(this.realWindow.x, this.realWindow.y);
    },

    _disconnectRealWindowSignals: function() {
        if (this._positionChangedId != 0) {
            this.realWindow.disconnect(this._positionChangedId);
            this._positionChangedId = 0;
        }

        if (this._realWindowDestroyedId != 0) {
            this.realWindow.disconnect(this._realWindowDestroyedId);
            this._realWindowDestroyedId = 0;
        }
    },

    _onDestroy: function() {
        this._disconnectRealWindowSignals();

        this.actor._delegate = null;

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }

        this.disconnectAll();
    },

    _onButtonRelease : function (actor, event) {
        this.emit('selected', event.get_time());

        return true;
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
        if (this.actor.get_parent() != null) {
            if (this._stackAbove == null)
                this.actor.lower_bottom();
            else
                this.actor.raise(this._stackAbove);
        }


        this.emit('drag-end');
    }
};
Signals.addSignalMethods(WindowClone.prototype);


/**
 * @metaWorkspace: a #Meta.Workspace
 */
function WorkspaceThumbnail(metaWorkspace) {
    this._init(metaWorkspace);
}

WorkspaceThumbnail.prototype = {
    _init : function(metaWorkspace) {
        this.metaWorkspace = metaWorkspace;

        this.actor = new St.Bin({ reactive: true,
                                  clip_to_allocation: true,
                                  style_class: 'workspace-thumbnail' });
        this.actor._delegate = this;

        this._group = new Clutter.Group();
        this.actor.add_actor(this._group);

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this.actor.connect('button-press-event', Lang.bind(this,
            function(actor, event) {
                return true;
            }));
        this.actor.connect('button-release-event', Lang.bind(this,
            function(actor, event) {
                this._activate();
                return true;
            }));

        this._background = new Clutter.Clone({ source: global.background_actor });
        this._group.add_actor(this._background);
        this._group.set_size(global.screen_width, global.screen_height);

        let windows = global.get_window_actors().filter(this._isMyWindow, this);

        // Create clones for windows that should be visible in the Overview
        this._windows = [];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        // Track window changes
        this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                          Lang.bind(this, this._windowAdded));
        this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                           Lang.bind(this, this._windowRemoved));
    },

    _lookupIndex: function (metaWindow) {
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].metaWindow == metaWindow) {
                return i;
            }
        }
        return -1;
    },

    syncStacking: function(stackIndices) {
        this._windows.sort(function (a, b) { return stackIndices[a.metaWindow.get_stable_sequence()] - stackIndices[b.metaWindow.get_stable_sequence()]; });

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            let metaWindow = clone.metaWindow;
            if (i == 0) {
                clone.setStackAbove(this._background);
            } else {
                let previousClone = this._windows[i - 1];
                clone.setStackAbove(previousClone.actor);
            }
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
        clone.destroy();
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
    },

    destroy : function() {
        this.actor.destroy();
    },

    _onDestroy: function(actor) {
        this.metaWorkspace.disconnect(this._windowAddedId);
        this.metaWorkspace.disconnect(this._windowRemovedId);

        this._windows = [];
        this.actor = null;
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

        clone.connect('selected',
                      Lang.bind(this, this._activate));
        clone.connect('drag-begin',
                      Lang.bind(this, function(clone) {
                          Main.overview.beginWindowDrag();
                      }));
        clone.connect('drag-end',
                      Lang.bind(this, function(clone) {
                          Main.overview.endWindowDrag();
                      }));
        this._group.add_actor(clone.actor);

        this._windows.push(clone);

        return clone;
    },

    _activate : function (clone, time) {
        // a click on the already current workspace should go back to the main view
        if (this.metaWorkspace == global.screen.get_active_workspace())
            Main.overview.hide();
        else
            this.metaWorkspace.activate(time);
    },

    // Draggable target interface
    handleDragOver : function(source, actor, x, y, time) {
        if (source.realWindow)
            return DND.DragMotionResult.MOVE_DROP;
        if (source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    },

    acceptDrop : function(source, actor, x, y, time) {
        if (source.realWindow) {
            let win = source.realWindow;
            if (this._isMyWindow(win))
                return false;

            let metaWindow = win.get_meta_window();
            metaWindow.change_workspace_by_index(this.metaWorkspace.index(),
                                                 false, // don't create workspace
                                                 time);
            return true;
        } else if (source.shellWorkspaceLaunch) {
            source.shellWorkspaceLaunch({ workspace: this.metaWorkspace,
                                          timestamp: time });
            return true;
        }

        return false;
    }
};

Signals.addSignalMethods(WorkspaceThumbnail.prototype);


function ThumbnailsBox() {
    this._init();
}

ThumbnailsBox.prototype = {
    _init: function() {
        this.actor = new Shell.GenericContainer({ style_class: 'workspace-thumbnails',
                                                  request_mode: Clutter.RequestMode.WIDTH_FOR_HEIGHT });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator',
                                     fixed_position_set: true });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._indicator = indicator;
        this.actor.add_actor(indicator);
        this._indicatorConstrained = false;

        this._thumbnails = [];
    },

    show: function() {
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));

        this.addThumbnails(0, global.screen.n_workspaces);
        this._constrainThumbnailIndicator();
    },

    hide: function() {
        this._unconstrainThumbnailIndicator();

        if (this._switchWorkspaceNotifyId > 0) {
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
            this._switchWorkspaceNotifyId = 0;
        }

        for (let w = 0; w < this._thumbnails.length; w++)
            this._thumbnails[w].destroy();
        this._thumbnails = [];
    },

    addThumbnails: function(start, count) {
        for (let k = start; k < start + count; k++) {
            let metaWorkspace = global.screen.get_workspace_by_index(k);
            let thumbnail = new WorkspaceThumbnail(metaWorkspace);
            this._thumbnails[k] = thumbnail;
            this.actor.add_actor(thumbnail.actor);
        }

        // The thumbnails indicator actually needs to be on top of the thumbnails
        this._indicator.raise_top();
    },

    removeThumbmails: function(start, count) {
        for (let k = start; k < start + count; k++)
            this._thumbnails[k].destroy();
        this._thumbnails.splice(start, count);

        // If we removed the current workspace, then metacity will have already
        // switched to an adjacent workspace. Leaving the animation we
        // started in response to that around will look funny because it's an
        // animation for the *old* workspace configuration. So, kill it.
        // If we animate the workspace removal in the future, we should animate
        // the indicator as part of that.
        Tweener.removeTweens(this._thumbnailIndicator);
        this._constrainThumbnailIndicator();
    },

    syncStacking: function(stackIndices) {
        for (let i = 0; i < this._thumbnails.length; i++)
            this._thumbnails[i].syncStacking(stackIndices);
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        // Note that for getPreferredWidth/Height we cheat a bit and skip propagating
        // the size request to our children because we know how big they are and know
        // that the actors aren't depending on the virtual functions being called.

        if (this._thumbnails.length == 0)
            return;

        let spacing = this.actor.get_theme_node().get_length('spacing');
        let totalSpacing = (this._thumbnails.length - 1) * spacing;

        alloc.min_size = totalSpacing;
        alloc.natural_size = totalSpacing + this._thumbnails.length * global.screen_height * MAX_THUMBNAIL_SCALE;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        if (this._thumbnails.length == 0)
            return;

        let spacing = this.actor.get_theme_node().get_length('spacing');
        let totalSpacing = (this._thumbnails.length - 1) * spacing;
        let avail = forHeight - totalSpacing;

        let scale = (avail / this._thumbnails.length) / global.screen_height;
        scale = Math.min(scale, MAX_THUMBNAIL_SCALE);

        alloc.min_size = alloc.natural_size = Math.round(global.screen_width * scale);
    },

    _allocate: function(actor, box, flags) {
        let screenHeight = global.screen_height;

        let spacing = this.actor.get_theme_node().get_length('spacing');
        let totalSpacing = (this._thumbnails.length - 1) * spacing;
        let avail = (box.y2 - box.y1) - totalSpacing;

        let scale = (avail / this._thumbnails.length) / screenHeight;
        scale = Math.min(scale, MAX_THUMBNAIL_SCALE);

        let thumbnailHeight = screenHeight * scale;

        let childBox = new Clutter.ActorBox();

        let indicatorWorkspace = this._indicatorConstrained ? global.screen.get_active_workspace() : null;
        let indicatorBox;

        // Allocating a scaled actor is funny - x1/y1 correspond to the origin
        // of the actor, but x2/y2 are increased by the *unscaled* size.
        childBox.x1 = box.x1;
        childBox.x2 = childBox.x1 + global.screen_width;

        let y = box.y1;

        for (let i = 0; i < this._thumbnails.length; i++) {
            if (i > 0)
                y += spacing + thumbnailHeight;

            // We might end up with thumbnailHeight being something like 99.33
            // pixels. To make this work and not end up with a gap at the bottom,
            // we need some thumbnails to be 99 pixels and some 100 pixels height;
            // we compute an actual scale separately for each thumbnail.
            let y1 = Math.round(y);
            let y2 = Math.round(y + thumbnailHeight);
            let roundedScale = (y2 - y1) / screenHeight;

            if (this._thumbnails[i].metaWorkspace == indicatorWorkspace) {
                let indicatorBox = new Clutter.ActorBox();
                indicatorBox.x1 = box.x1;
                indicatorBox.x2 = box.x2;
                indicatorBox.y1 = y1;
                indicatorBox.y2 = y2;

                this._indicator.allocate(indicatorBox, flags);
            }

            childBox.y1 = y1;
            childBox.y2 = childBox.y1 + screenHeight;

            this._thumbnails[i].actor.set_scale(roundedScale, roundedScale);
            this._thumbnails[i].actor.allocate(childBox, flags);
        }

        if (indicatorWorkspace == null)
            this._indicator.allocate_preferred_size(flags);
    },

    _constrainThumbnailIndicator: function() {
        this._indicatorConstrained = true;
        this.actor.queue_relayout();
    },

    _unconstrainThumbnailIndicator: function() {
        this._indicatorConstrained = false;
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        let active = global.screen.get_active_workspace_index();
        let thumbnail = this._thumbnails[active];

        this._unconstrainThumbnailIndicator();
        let oldAllocation = this._indicator.allocation;
        this._indicator.x = oldAllocation.x1;
        this._indicator.y = oldAllocation.y1;
        this._indicator.width = oldAllocation.x2 - oldAllocation.x1;
        this._indicator.height = oldAllocation.y2 - oldAllocation.y1;

        Tweener.addTween(this._indicator,
                         { x: thumbnail.actor.allocation.x1,
                           y: thumbnail.actor.allocation.y1,
                           time: WorkspacesView.WORKSPACE_SWITCH_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                                                 this._constrainThumbnailIndicator)
                         });
    }
};
