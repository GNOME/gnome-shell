/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Workspace = imports.ui.workspace;

// Fraction of original screen size for thumbnails
let THUMBNAIL_SCALE = 1/8.;

function WindowClone(realWindow) {
    this._init(realWindow);
}

WindowClone.prototype = {
    _init : function(realWindow) {
        this.actor = new Clutter.Clone({ source: realWindow.get_texture(),
                                         clip_to_allocation: true,
                                         reactive: true,
                                         x: realWindow.x,
                                         y: realWindow.y });
        this.actor._delegate = this;
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;
        this.metaWindow._delegate = this;

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

        this._selected = false;
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

    _onDestroy: function() {
        this.metaWindow._delegate = null;
        this.actor._delegate = null;

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }

        this.disconnectAll();
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
        // When dragging a window, we use this slot for reserve space.
        this.metaWorkspace = metaWorkspace;

        this.actor = new St.Bin({ reactive: true,
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
                this.metaWorkspace.activate(event.get_time());
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
                      Lang.bind(this, this._onCloneSelected));
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

    _onCloneSelected : function (clone, time) {
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
            this.metaWorkspace.activate(time);
            source.shellWorkspaceLaunch();
            return true;
        }

        return false;
    }
};

Signals.addSignalMethods(WorkspaceThumbnail.prototype);
