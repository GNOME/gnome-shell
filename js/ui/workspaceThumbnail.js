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

// Fraction of original screen size for thumbnails
let THUMBNAIL_SCALE = 1/8.;

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

        this._group.set_size(THUMBNAIL_SCALE * global.screen_width, THUMBNAIL_SCALE * global.screen_height);
        this._group.set_scale(THUMBNAIL_SCALE, THUMBNAIL_SCALE);

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
        this.actor = new St.BoxLayout({ vertical: true,
                                        style_class: 'workspace-thumbnails' });

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator',
                                     fixed_position_set: true });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._indicator = indicator;
        this.actor.add(indicator);
        this._indicatorConstraints = [];
        this._indicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.POSITION }));
        this._indicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.SIZE }));
        this._indicatorConstraints.forEach(function(constraint) {
                                               indicator.add_constraint(constraint);
                                           });

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
            this.actor.add(thumbnail.actor);
        }

        // The thumbnails indicator actually needs to be on top of the thumbnails, but
        // there is also something more subtle going on as well - actors in a StBoxLayout
        // are allocated from bottom to top (start to end), and we need the
        // thumnail indicator to be allocated after the actors it is constrained to.
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

    _constrainThumbnailIndicator: function() {
        let active = global.screen.get_active_workspace_index();
        let thumbnail = this._thumbnails[active];

        this._indicatorConstraints.forEach(function(constraint) {
                                               constraint.set_source(thumbnail.actor);
                                               constraint.set_enabled(true);
                                           });
    },

    _unconstrainThumbnailIndicator: function() {
        this._indicatorConstraints.forEach(function(constraint) {
                                               constraint.set_enabled(false);
                                           });
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
