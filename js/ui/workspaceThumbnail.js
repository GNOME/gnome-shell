// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Background = imports.ui.background;
const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const WindowManager = imports.ui.windowManager;
const Workspace = imports.ui.workspace;
const WorkspacesView = imports.ui.workspacesView;

// The maximum size of a thumbnail is 1/10 the width and height of the screen
let MAX_THUMBNAIL_SCALE = 1/10.;

var RESCALE_ANIMATION_TIME = 0.2;
var SLIDE_ANIMATION_TIME = 0.2;

// When we create workspaces by dragging, we add a "cut" into the top and
// bottom of each workspace so that the user doesn't have to hit the
// placeholder exactly.
var WORKSPACE_CUT_SIZE = 10;

var WORKSPACE_KEEP_ALIVE_TIME = 100;

const OVERRIDE_SCHEMA = 'org.gnome.shell.overrides';

/* A layout manager that requests size only for primary_actor, but then allocates
   all using a fixed layout */
var PrimaryActorLayout = new Lang.Class({
    Name: 'PrimaryActorLayout',
    Extends: Clutter.FixedLayout,

    _init(primaryActor) {
        this.parent();

        this.primaryActor = primaryActor;
    },

    vfunc_get_preferred_width(container, forHeight) {
        return this.primaryActor.get_preferred_width(forHeight);
    },

    vfunc_get_preferred_height(container, forWidth) {
        return this.primaryActor.get_preferred_height(forWidth);
    },
});

var WindowClone = new Lang.Class({
    Name: 'WindowClone',

    _init (realWindow) {
        this.clone = new Clutter.Clone({ source: realWindow });

        /* Can't use a Shell.GenericContainer because of DND and reparenting... */
        this.actor = new Clutter.Actor({ layout_manager: new PrimaryActorLayout(this.clone),
                                         reactive: true });
        this.actor._delegate = this;
        this.actor.add_child(this.clone);
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;

        this.clone._updateId = this.metaWindow.connect('position-changed',
                                                       Lang.bind(this, this._onPositionChanged));
        this.clone._destroyId = this.realWindow.connect('destroy', Lang.bind(this, function() {
            // First destroy the clone and then destroy everything
            // This will ensure that we never see it in the _disconnectSignals loop
            this.clone.destroy();
            this.destroy();
        }));
        this._onPositionChanged();

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));
        this.actor.connect('touch-event',
                           Lang.bind(this, this._onTouchEvent));

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._draggable = DND.makeDraggable(this.actor,
                                            { restoreOnSuccess: true,
                                              dragActorMaxSize: Workspace.WINDOW_DND_SIZE,
                                              dragActorOpacity: Workspace.DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        this._draggable.connect('drag-cancelled', Lang.bind(this, this._onDragCancelled));
        this._draggable.connect('drag-end', Lang.bind(this, this._onDragEnd));
        this.inDrag = false;

        let iter = Lang.bind(this, function(win) {
            let actor = win.get_compositor_private();

            if (!actor)
                return false;
            if (!win.is_attached_dialog())
                return false;

            this._doAddAttachedDialog(win, actor);
            win.foreach_transient(iter);

            return true;
        });
        this.metaWindow.foreach_transient(iter);
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

    setStackAbove (actor) {
        this._stackAbove = actor;

        // Don't apply the new stacking now, it will be applied
        // when dragging ends and window are stacked again
        if (actor.inDrag)
            return;

        let actualAbove = this.getActualStackAbove();
        if (actualAbove == null)
            this.actor.lower_bottom();
        else
            this.actor.raise(actualAbove);
    },

    destroy () {
        this.actor.destroy();
    },

    addAttachedDialog(win) {
        this._doAddAttachedDialog(win, win.get_compositor_private());
    },

    _doAddAttachedDialog(metaDialog, realDialog) {
        let clone = new Clutter.Clone({ source: realDialog });
        this._updateDialogPosition(realDialog, clone);

        clone._updateId = metaDialog.connect('position-changed',
                                             Lang.bind(this, this._updateDialogPosition, clone));
        clone._destroyId = realDialog.connect('destroy', Lang.bind(this, function() {
            clone.destroy();
        }));
        this.actor.add_child(clone);
    },

    _updateDialogPosition(realDialog, cloneDialog) {
        let metaDialog = realDialog.meta_window;
        let dialogRect = metaDialog.get_frame_rect();
        let rect = this.metaWindow.get_frame_rect();

        cloneDialog.set_position(dialogRect.x - rect.x, dialogRect.y - rect.y);
    },

    _onPositionChanged() {
        let rect = this.metaWindow.get_frame_rect();
        this.actor.set_position(this.realWindow.x, this.realWindow.y);
    },

    _disconnectSignals() {
        this.actor.get_children().forEach(function(child) {
            let realWindow = child.source;

            realWindow.meta_window.disconnect(child._updateId);
            realWindow.disconnect(child._destroyId);
        });
    },

    _onDestroy() {
        this._disconnectSignals();

        this.actor._delegate = null;

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }

        this.disconnectAll();
    },

    _onButtonRelease  (actor, event) {
        this.emit('selected', event.get_time());

        return Clutter.EVENT_STOP;
    },

    _onTouchEvent  (actor, event) {
        if (event.type() != Clutter.EventType.TOUCH_END ||
            !global.display.is_pointer_emulating_sequence(event.get_event_sequence()))
            return Clutter.EVENT_PROPAGATE;

        this.emit('selected', event.get_time());
        return Clutter.EVENT_STOP;
    },

    _onDragBegin  (draggable, time) {
        this.inDrag = true;
        this.emit('drag-begin');
    },

    _onDragCancelled  (draggable, time) {
        this.emit('drag-cancelled');
    },

    _onDragEnd  (draggable, time, snapback) {
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


const ThumbnailState = {
    NEW   :         0,
    ANIMATING_IN :  1,
    NORMAL:         2,
    REMOVING :      3,
    ANIMATING_OUT : 4,
    ANIMATED_OUT :  5,
    COLLAPSING :    6,
    DESTROYED :     7
};

/**
 * @metaWorkspace: a #Meta.Workspace
 */
var WorkspaceThumbnail = new Lang.Class({
    Name: 'WorkspaceThumbnail',

    _init (metaWorkspace) {
        this.metaWorkspace = metaWorkspace;
        this.monitorIndex = Main.layoutManager.primaryIndex;

        this._removed = false;

        this.actor = new St.Widget({ clip_to_allocation: true,
                                     style_class: 'workspace-thumbnail' });
        this.actor._delegate = this;

        this._contents = new Clutter.Actor();
        this.actor.add_child(this._contents);

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._createBackground();

        let monitor = Main.layoutManager.primaryMonitor;
        this.setPorthole(monitor.x, monitor.y, monitor.width, monitor.height);

        let windows = global.get_window_actors().filter(Lang.bind(this, function(actor) {
            let win = actor.meta_window;
            return win.located_on_workspace(metaWorkspace);
        }));

        // Create clones for windows that should be visible in the Overview
        this._windows = [];
        this._allWindows = [];
        this._minimizedChangedIds = [];
        for (let i = 0; i < windows.length; i++) {
            let minimizedChangedId =
                windows[i].meta_window.connect('notify::minimized',
                                               Lang.bind(this,
                                                         this._updateMinimized));
            this._allWindows.push(windows[i].meta_window);
            this._minimizedChangedIds.push(minimizedChangedId);

            if (this._isMyWindow(windows[i]) && this._isOverviewWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        // Track window changes
        this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                          Lang.bind(this, this._windowAdded));
        this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                           Lang.bind(this, this._windowRemoved));
        this._windowEnteredMonitorId = global.screen.connect('window-entered-monitor',
                                                           Lang.bind(this, this._windowEnteredMonitor));
        this._windowLeftMonitorId = global.screen.connect('window-left-monitor',
                                                           Lang.bind(this, this._windowLeftMonitor));

        this.state = ThumbnailState.NORMAL;
        this._slidePosition = 0; // Fully slid in
        this._collapseFraction = 0; // Not collapsed
    },

    _createBackground() {
        this._bgManager = new Background.BackgroundManager({ monitorIndex: Main.layoutManager.primaryIndex,
                                                             container: this._contents,
                                                             vignette: false });
    },

    setPorthole(x, y, width, height) {
        this._portholeX = x;
        this._portholeY = y;
        this.actor.set_size(width, height);
        this._contents.set_position(-x, -y);
    },

    _lookupIndex (metaWindow) {
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].metaWindow == metaWindow) {
                return i;
            }
        }
        return -1;
    },

    syncStacking(stackIndices) {
        this._windows.sort(function (a, b) { return stackIndices[a.metaWindow.get_stable_sequence()] - stackIndices[b.metaWindow.get_stable_sequence()]; });

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            let metaWindow = clone.metaWindow;
            if (i == 0) {
                clone.setStackAbove(this._bgManager.backgroundActor);
            } else {
                let previousClone = this._windows[i - 1];
                clone.setStackAbove(previousClone.actor);
            }
        }
    },

    set slidePosition(slidePosition) {
        this._slidePosition = slidePosition;
        this.actor.queue_relayout();
    },

    get slidePosition() {
        return this._slidePosition;
    },

    set collapseFraction(collapseFraction) {
        this._collapseFraction = collapseFraction;
        this.actor.queue_relayout();
    },

    get collapseFraction() {
        return this._collapseFraction;
    },

    _doRemoveWindow (metaWin) {
        let win = metaWin.get_compositor_private();

        // find the position of the window in our list
        let index = this._lookupIndex (metaWin);

        if (index == -1)
            return;

        let clone = this._windows[index];
        this._windows.splice(index, 1);

        clone.destroy();
    },

    _doAddWindow (metaWin) {
        if (this._removed)
            return;

        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            let id = Mainloop.idle_add(Lang.bind(this,
                                       function () {
                                            if (!this._removed &&
                                                metaWin.get_compositor_private() &&
                                                metaWin.get_workspace() == this.metaWorkspace)
                                                this._doAddWindow(metaWin);
                                            return GLib.SOURCE_REMOVE;
                                        }));
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._doAddWindow');
            return;
        }

        if (this._allWindows.indexOf(metaWin) == -1) {
            let minimizedChangedId = metaWin.connect('notify::minimized',
                                                     Lang.bind(this,
                                                               this._updateMinimized));
            this._allWindows.push(metaWin);
            this._minimizedChangedIds.push(minimizedChangedId);
        }

        // We might have the window in our list already if it was on all workspaces and
        // now was moved to this workspace
        if (this._lookupIndex (metaWin) != -1)
            return;

        if (!this._isMyWindow(win))
            return;

        if (this._isOverviewWindow(win)) {
            this._addWindowClone(win);
        } else if (metaWin.is_attached_dialog()) {
            let parent = metaWin.get_transient_for();
            while (parent.is_attached_dialog())
                parent = metaWin.get_transient_for();

            let idx = this._lookupIndex (parent);
            if (idx < 0) {
                // parent was not created yet, it will take care
                // of the dialog when created
                return;
            }

            let clone = this._windows[idx];
            clone.addAttachedDialog(metaWin);
        }
    },

    _windowAdded (metaWorkspace, metaWin) {
        this._doAddWindow(metaWin);
    },

    _windowRemoved (metaWorkspace, metaWin) {
        let index = this._allWindows.indexOf(metaWin);
        if (index != -1) {
            metaWin.disconnect(this._minimizedChangedIds[index]);
            this._allWindows.splice(index, 1);
            this._minimizedChangedIds.splice(index, 1);
        }

        this._doRemoveWindow(metaWin);
    },

    _windowEnteredMonitor (metaScreen, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex) {
            this._doAddWindow(metaWin);
        }
    },

    _windowLeftMonitor (metaScreen, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex) {
            this._doRemoveWindow(metaWin);
        }
    },

    _updateMinimized(metaWin) {
        if (metaWin.minimized)
            this._doRemoveWindow(metaWin);
        else
            this._doAddWindow(metaWin);
    },

    destroy () {
        if (this.actor)
          this.actor.destroy();
    },

    workspaceRemoved () {
        if (this._removed)
            return;

        this._removed = true;

        this.metaWorkspace.disconnect(this._windowAddedId);
        this.metaWorkspace.disconnect(this._windowRemovedId);
        global.screen.disconnect(this._windowEnteredMonitorId);
        global.screen.disconnect(this._windowLeftMonitorId);

        for (let i = 0; i < this._allWindows.length; i++)
            this._allWindows[i].disconnect(this._minimizedChangedIds[i]);
    },

    _onDestroy(actor) {
        this.workspaceRemoved();

        if (this._bgManager) {
          this._bgManager.destroy();
          this._bgManager = null;
        }

        this._windows = [];
        this.actor = null;
    },

    // Tests if @actor belongs to this workspace and monitor
    _isMyWindow  (actor) {
        let win = actor.meta_window;
        return win.located_on_workspace(this.metaWorkspace) &&
            (win.get_monitor() == this.monitorIndex);
    },

    // Tests if @win should be shown in the Overview
    _isOverviewWindow  (win) {
        return !win.get_meta_window().skip_taskbar &&
               win.get_meta_window().showing_on_its_workspace();
    },

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone (win) {
        let clone = new WindowClone(win);

        clone.connect('selected',
                      Lang.bind(this, function(clone, time) {
                          this.activate(time);
                      }));
        clone.connect('drag-begin',
                      Lang.bind(this, function() {
                          Main.overview.beginWindowDrag(clone.metaWindow);
                      }));
        clone.connect('drag-cancelled',
                      Lang.bind(this, function() {
                          Main.overview.cancelledWindowDrag(clone.metaWindow);
                      }));
        clone.connect('drag-end',
                      Lang.bind(this, function() {
                          Main.overview.endWindowDrag(clone.metaWindow);
                      }));
        this._contents.add_actor(clone.actor);

        if (this._windows.length == 0)
            clone.setStackAbove(this._bgManager.backgroundActor);
        else
            clone.setStackAbove(this._windows[this._windows.length - 1].actor);

        this._windows.push(clone);

        return clone;
    },

    activate  (time) {
        if (this.state > ThumbnailState.NORMAL)
            return;

        // a click on the already current workspace should go back to the main view
        if (this.metaWorkspace == global.screen.get_active_workspace())
            Main.overview.hide();
        else
            this.metaWorkspace.activate(time);
    },

    // Draggable target interface used only by ThumbnailsBox
    handleDragOverInternal (source, time) {
        if (source == Main.xdndHandler) {
            this.metaWorkspace.activate(time);
            return DND.DragMotionResult.CONTINUE;
        }

        if (this.state > ThumbnailState.NORMAL)
            return DND.DragMotionResult.CONTINUE;

        if (source.realWindow && !this._isMyWindow(source.realWindow))
            return DND.DragMotionResult.MOVE_DROP;
        if (source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    },

    acceptDropInternal (source, time) {
        if (this.state > ThumbnailState.NORMAL)
            return false;

        if (source.realWindow) {
            let win = source.realWindow;
            if (this._isMyWindow(win))
                return false;

            let metaWindow = win.get_meta_window();

            // We need to move the window before changing the workspace, because
            // the move itself could cause a workspace change if the window enters
            // the primary monitor
            if (metaWindow.get_monitor() != this.monitorIndex)
                metaWindow.move_to_monitor(this.monitorIndex);

            metaWindow.change_workspace_by_index(this.metaWorkspace.index(), false);
            return true;
        } else if (source.shellWorkspaceLaunch) {
            source.shellWorkspaceLaunch({ workspace: this.metaWorkspace ? this.metaWorkspace.index() : -1,
                                          timestamp: time });
            return true;
        }

        return false;
    }
});

Signals.addSignalMethods(WorkspaceThumbnail.prototype);


var ThumbnailsBox = new Lang.Class({
    Name: 'ThumbnailsBox',

    _init() {
        this.actor = new Shell.GenericContainer({ reactive: true,
                                                  style_class: 'workspace-thumbnails',
                                                  request_mode: Clutter.RequestMode.WIDTH_FOR_HEIGHT });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor._delegate = this;

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator' });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._indicator = indicator;
        this.actor.add_actor(indicator);

        this._dropWorkspace = -1;
        this._dropPlaceholderPos = -1;
        this._dropPlaceholder = new St.Bin({ style_class: 'placeholder' });
        this.actor.add_actor(this._dropPlaceholder);
        this._spliceIndex = -1;

        this._targetScale = 0;
        this._scale = 0;
        this._pendingScaleUpdate = false;
        this._stateUpdateQueued = false;
        this._animatingIndicator = false;
        this._indicatorY = 0; // only used when _animatingIndicator is true

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this._thumbnails = [];

        this.actor.connect('button-press-event', function() { return Clutter.EVENT_STOP; });
        this.actor.connect('button-release-event', Lang.bind(this, this._onButtonRelease));
        this.actor.connect('touch-event', Lang.bind(this, this._onTouchEvent));

        Main.overview.connect('showing',
                              Lang.bind(this, this._createThumbnails));
        Main.overview.connect('hidden',
                              Lang.bind(this, this._destroyThumbnails));

        Main.overview.connect('item-drag-begin',
                              Lang.bind(this, this._onDragBegin));
        Main.overview.connect('item-drag-end',
                              Lang.bind(this, this._onDragEnd));
        Main.overview.connect('item-drag-cancelled',
                              Lang.bind(this, this._onDragCancelled));
        Main.overview.connect('window-drag-begin',
                              Lang.bind(this, this._onDragBegin));
        Main.overview.connect('window-drag-end',
                              Lang.bind(this, this._onDragEnd));
        Main.overview.connect('window-drag-cancelled',
                              Lang.bind(this, this._onDragCancelled));

        this._settings = new Gio.Settings({ schema_id: OVERRIDE_SCHEMA });
        this._settings.connect('changed::dynamic-workspaces',
            Lang.bind(this, this._updateSwitcherVisibility));

        Main.layoutManager.connect('monitors-changed', Lang.bind(this, function() {
            this._destroyThumbnails();
            if (Main.overview.visible)
                this._createThumbnails();
        }));
    },

    _updateSwitcherVisibility() {
        this.actor.visible =
            this._settings.get_boolean('dynamic-workspaces') ||
                global.screen.n_workspaces > 1;
    },

    _activateThumbnailAtPoint (stageX, stageY, time) {
        let [r, x, y] = this.actor.transform_stage_point(stageX, stageY);

        for (let i = 0; i < this._thumbnails.length; i++) {
            let thumbnail = this._thumbnails[i]
            let [w, h] = thumbnail.actor.get_transformed_size();
            if (y >= thumbnail.actor.y && y <= thumbnail.actor.y + h) {
                thumbnail.activate(time);
                break;
            }
        }
    },

    _onButtonRelease(actor, event) {
        let [stageX, stageY] = event.get_coords();
        this._activateThumbnailAtPoint(stageX, stageY, event.get_time());
        return Clutter.EVENT_STOP;
    },

    _onTouchEvent (actor, event) {
        if (event.type() == Clutter.EventType.TOUCH_END &&
            global.display.is_pointer_emulating_sequence(event.get_event_sequence())) {
            let [stageX, stageY] = event.get_coords();
            this._activateThumbnailAtPoint(stageX, stageY, event.get_time());
        }

        return Clutter.EVENT_STOP;
    },

    _onDragBegin() {
        this._dragCancelled = false;
        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };
        DND.addDragMonitor(this._dragMonitor);
    },

    _onDragEnd() {
        if (this._dragCancelled)
            return;

        this._endDrag();
    },

    _onDragCancelled() {
        this._dragCancelled = true;
        this._endDrag();
    },

    _endDrag() {
        this._clearDragPlaceholder();
        DND.removeDragMonitor(this._dragMonitor);
    },

    _onDragMotion(dragEvent) {
        if (!this.actor.contains(dragEvent.targetActor))
            this._onLeave();
        return DND.DragMotionResult.CONTINUE;
    },

    _onLeave() {
        this._clearDragPlaceholder();
    },

    _clearDragPlaceholder() {
        if (this._dropPlaceholderPos == -1)
            return;

        this._dropPlaceholderPos = -1;
        this.actor.queue_relayout();
    },

    // Draggable target interface
    handleDragOver (source, actor, x, y, time) {
        if (!source.realWindow && !source.shellWorkspaceLaunch && source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        let canCreateWorkspaces = Meta.prefs_get_dynamic_workspaces();
        let spacing = this.actor.get_theme_node().get_length('spacing');

        this._dropWorkspace = -1;
        let placeholderPos = -1;
        let targetBase;
        if (this._dropPlaceholderPos == 0)
            targetBase = this._dropPlaceholder.y;
        else
            targetBase = this._thumbnails[0].actor.y;
        let targetTop = targetBase - spacing - WORKSPACE_CUT_SIZE;
        let length = this._thumbnails.length;
        for (let i = 0; i < length; i ++) {
            // Allow the reorder target to have a 10px "cut" into
            // each side of the thumbnail, to make dragging onto the
            // placeholder easier
            let [w, h] = this._thumbnails[i].actor.get_transformed_size();
            let targetBottom = targetBase + WORKSPACE_CUT_SIZE;
            let nextTargetBase = targetBase + h + spacing;
            let nextTargetTop =  nextTargetBase - spacing - ((i == length - 1) ? 0: WORKSPACE_CUT_SIZE);

            // Expand the target to include the placeholder, if it exists.
            if (i == this._dropPlaceholderPos)
                targetBottom += this._dropPlaceholder.get_height();

            if (y > targetTop && y <= targetBottom && source != Main.xdndHandler && canCreateWorkspaces) {
                placeholderPos = i;
                break;
            } else if (y > targetBottom && y <= nextTargetTop) {
                this._dropWorkspace = i;
                break
            }

            targetBase = nextTargetBase;
            targetTop = nextTargetTop;
        }

        if (this._dropPlaceholderPos != placeholderPos) {
            this._dropPlaceholderPos = placeholderPos;
            this.actor.queue_relayout();
        }

        if (this._dropWorkspace != -1)
            return this._thumbnails[this._dropWorkspace].handleDragOverInternal(source, time);
        else if (this._dropPlaceholderPos != -1)
            return source.realWindow ? DND.DragMotionResult.MOVE_DROP : DND.DragMotionResult.COPY_DROP;
        else
            return DND.DragMotionResult.CONTINUE;
    },

    acceptDrop(source, actor, x, y, time) {
        if (this._dropWorkspace != -1) {
            return this._thumbnails[this._dropWorkspace].acceptDropInternal(source, time);
        } else if (this._dropPlaceholderPos != -1) {
            if (!source.realWindow && !source.shellWorkspaceLaunch)
                return false;

            let isWindow = !!source.realWindow;

            let newWorkspaceIndex;
            [newWorkspaceIndex, this._dropPlaceholderPos] = [this._dropPlaceholderPos, -1];
            this._spliceIndex = newWorkspaceIndex;

            Main.wm.insertWorkspace(newWorkspaceIndex);

            if (isWindow) {
                // Move the window to our monitor first if necessary.
                let thumbMonitor = this._thumbnails[newWorkspaceIndex].monitorIndex;
                if (source.metaWindow.get_monitor() != thumbMonitor)
                    source.metaWindow.move_to_monitor(thumbMonitor);
                source.metaWindow.change_workspace_by_index(newWorkspaceIndex, true);
            } else if (source.shellWorkspaceLaunch) {
                source.shellWorkspaceLaunch({ workspace: newWorkspaceIndex,
                                              timestamp: time });
                // This new workspace will be automatically removed if the application fails
                // to open its first window within some time, as tracked by Shell.WindowTracker.
                // Here, we only add a very brief timeout to avoid the _immediate_ removal of the
                // workspace while we wait for the startup sequence to load.
                Main.wm.keepWorkspaceAlive(global.screen.get_workspace_by_index(newWorkspaceIndex),
                                           WORKSPACE_KEEP_ALIVE_TIME);
            }

            // Start the animation on the workspace (which is actually
            // an old one which just became empty)
            let thumbnail = this._thumbnails[newWorkspaceIndex];
            this._setThumbnailState(thumbnail, ThumbnailState.NEW);
            thumbnail.slidePosition = 1;

            this._queueUpdateStates();

            return true;
        } else {
            return false;
        }
    },

    _createThumbnails() {
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
        this._syncStackingId =
            Main.overview.connect('windows-restacked',
                                  Lang.bind(this, this._syncStacking));

        this._targetScale = 0;
        this._scale = 0;
        this._pendingScaleUpdate = false;
        this._stateUpdateQueued = false;

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this.addThumbnails(0, global.screen.n_workspaces);

        this._updateSwitcherVisibility();
    },

    _destroyThumbnails() {
        if (this._switchWorkspaceNotifyId > 0) {
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
            this._switchWorkspaceNotifyId = 0;
        }
        if (this._nWorkspacesNotifyId > 0) {
            global.screen.disconnect(this._nWorkspacesNotifyId);
            this._nWorkspacesNotifyId = 0;
        }

        if (this._syncStackingId > 0) {
            Main.overview.disconnect(this._syncStackingId);
            this._syncStackingId = 0;
        }

        for (let w = 0; w < this._thumbnails.length; w++)
            this._thumbnails[w].destroy();
        this._thumbnails = [];
        this._porthole = null;
    },

    _workspacesChanged() {
        let validThumbnails = this._thumbnails.filter(function(t) {
            return t.state <= ThumbnailState.NORMAL;
        });
        let oldNumWorkspaces = validThumbnails.length;
        let newNumWorkspaces = global.screen.n_workspaces;
        let active = global.screen.get_active_workspace_index();

        if (newNumWorkspaces > oldNumWorkspaces) {
            this.addThumbnails(oldNumWorkspaces, newNumWorkspaces - oldNumWorkspaces);
        } else {
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let metaWorkspace = global.screen.get_workspace_by_index(w);
                if (this._thumbnails[w].metaWorkspace != metaWorkspace) {
                    removedIndex = w;
                    break;
                }
            }

            this.removeThumbnails(removedIndex, removedNum);
        }

        this._updateSwitcherVisibility();
    },

    addThumbnails(start, count) {
        if (!this._ensurePorthole())
            return;
        for (let k = start; k < start + count; k++) {
            let metaWorkspace = global.screen.get_workspace_by_index(k);
            let thumbnail = new WorkspaceThumbnail(metaWorkspace);
            thumbnail.setPorthole(this._porthole.x, this._porthole.y,
                                  this._porthole.width, this._porthole.height);
            this._thumbnails.push(thumbnail);
            this.actor.add_actor(thumbnail.actor);

            if (start > 0 && this._spliceIndex == -1) {
                // not the initial fill, and not splicing via DND
                thumbnail.state = ThumbnailState.NEW;
                thumbnail.slidePosition = 1; // start slid out
                this._haveNewThumbnails = true;
            } else {
                thumbnail.state = ThumbnailState.NORMAL;
            }

            this._stateCounts[thumbnail.state]++;
        }

        this._queueUpdateStates();

        // The thumbnails indicator actually needs to be on top of the thumbnails
        this._indicator.raise_top();

        // Clear the splice index, we got the message
        this._spliceIndex = -1;
    },

    removeThumbnails(start, count) {
        let currentPos = 0;
        for (let k = 0; k < this._thumbnails.length; k++) {
            let thumbnail = this._thumbnails[k];

            if (thumbnail.state > ThumbnailState.NORMAL)
                continue;

            if (currentPos >= start && currentPos < start + count) {
                thumbnail.workspaceRemoved();
                this._setThumbnailState(thumbnail, ThumbnailState.REMOVING);
            }

            currentPos++;
        }

        this._queueUpdateStates();
    },

    _syncStacking(overview, stackIndices) {
        for (let i = 0; i < this._thumbnails.length; i++)
            this._thumbnails[i].syncStacking(stackIndices);
    },

    set scale(scale) {
        this._scale = scale;
        this.actor.queue_relayout();
    },

    get scale() {
        return this._scale;
    },

    set indicatorY(indicatorY) {
        this._indicatorY = indicatorY;
        this.actor.queue_relayout();
    },

    get indicatorY() {
        return this._indicatorY;
    },

    _setThumbnailState(thumbnail, state) {
        this._stateCounts[thumbnail.state]--;
        thumbnail.state = state;
        this._stateCounts[thumbnail.state]++;
    },

    _iterateStateThumbnails(state, callback) {
        if (this._stateCounts[state] == 0)
            return;

        for (let i = 0; i < this._thumbnails.length; i++) {
            if (this._thumbnails[i].state == state)
                callback.call(this, this._thumbnails[i]);
        }
    },

    _tweenScale() {
        Tweener.addTween(this,
                         { scale: this._targetScale,
                           time: RESCALE_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._queueUpdateStates,
                           onCompleteScope: this });
    },

    _updateStates() {
        this._stateUpdateQueued = false;

        // If we are animating the indicator, wait
        if (this._animatingIndicator)
            return;

        // Then slide out any thumbnails that have been destroyed
        this._iterateStateThumbnails(ThumbnailState.REMOVING,
            function(thumbnail) {
                this._setThumbnailState(thumbnail, ThumbnailState.ANIMATING_OUT);

                Tweener.addTween(thumbnail,
                                 { slidePosition: 1,
                                   time: SLIDE_ANIMATION_TIME,
                                   transition: 'linear',
                                   onComplete() {
                                       this._setThumbnailState(thumbnail, ThumbnailState.ANIMATED_OUT);
                                       this._queueUpdateStates();
                                   },
                                   onCompleteScope: this
                                 });
            });

        // As long as things are sliding out, don't proceed
        if (this._stateCounts[ThumbnailState.ANIMATING_OUT] > 0)
            return;

        // Once that's complete, we can start scaling to the new size and collapse any removed thumbnails
        this._iterateStateThumbnails(ThumbnailState.ANIMATED_OUT,
            function(thumbnail) {
                this.actor.set_skip_paint(thumbnail.actor, true);
                this._setThumbnailState(thumbnail, ThumbnailState.COLLAPSING);
                Tweener.addTween(thumbnail,
                                 { collapseFraction: 1,
                                   time: RESCALE_ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete() {
                                       this._stateCounts[thumbnail.state]--;
                                       thumbnail.state = ThumbnailState.DESTROYED;

                                       let index = this._thumbnails.indexOf(thumbnail);
                                       this._thumbnails.splice(index, 1);
                                       thumbnail.destroy();

                                       this._queueUpdateStates();
                                   },
                                   onCompleteScope: this
                                 });
                });

        if (this._pendingScaleUpdate) {
            this._tweenScale();
            this._pendingScaleUpdate = false;
        }

        // Wait until that's done
        if (this._scale != this._targetScale || this._stateCounts[ThumbnailState.COLLAPSING] > 0)
            return;

        // And then slide in any new thumbnails
        this._iterateStateThumbnails(ThumbnailState.NEW,
            function(thumbnail) {
                this._setThumbnailState(thumbnail, ThumbnailState.ANIMATING_IN);
                Tweener.addTween(thumbnail,
                                 { slidePosition: 0,
                                   time: SLIDE_ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete() {
                                       this._setThumbnailState(thumbnail, ThumbnailState.NORMAL);
                                   },
                                   onCompleteScope: this
                                 });
            });
    },

    _queueUpdateStates() {
        if (this._stateUpdateQueued)
            return;

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                       Lang.bind(this, this._updateStates));

        this._stateUpdateQueued = true;
    },

    _getPreferredHeight(actor, forWidth, alloc) {
        // Note that for getPreferredWidth/Height we cheat a bit and skip propagating
        // the size request to our children because we know how big they are and know
        // that the actors aren't depending on the virtual functions being called.

        if (!this._ensurePorthole()) {
            alloc.min_size = -1;
            alloc.natural_size = -1;
            return;
        }

        let themeNode = this.actor.get_theme_node();

        let spacing = themeNode.get_length('spacing');
        let nWorkspaces = global.screen.n_workspaces;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        alloc.min_size = totalSpacing;
        alloc.natural_size = totalSpacing + nWorkspaces * this._porthole.height * MAX_THUMBNAIL_SCALE;
    },

    _getPreferredWidth(actor, forHeight, alloc) {
        if (!this._ensurePorthole()) {
            alloc.min_size = -1;
            alloc.natural_size = -1;
            return;
        }

        let themeNode = this.actor.get_theme_node();

        let spacing = this.actor.get_theme_node().get_length('spacing');
        let nWorkspaces = global.screen.n_workspaces;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        let avail = forHeight - totalSpacing;

        let scale = (avail / nWorkspaces) / this._porthole.height;
        scale = Math.min(scale, MAX_THUMBNAIL_SCALE);

        let width = Math.round(this._porthole.width * scale);
        alloc.min_size = width;
        alloc.natural_size = width;
    },

    // The "porthole" is the portion of the screen that we show in the
    // workspaces
    _ensurePorthole() {
        if (!Main.layoutManager.primaryMonitor)
            return false;

        if (!this._porthole)
            this._porthole = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);

        return true;
    },

    _allocate(actor, box, flags) {
        let rtl = (Clutter.get_default_text_direction () == Clutter.TextDirection.RTL);

        if (this._thumbnails.length == 0) // not visible
            return;

        let themeNode = this.actor.get_theme_node();

        let portholeWidth = this._porthole.width;
        let portholeHeight = this._porthole.height;
        let spacing = themeNode.get_length('spacing');

        // Compute the scale we'll need once everything is updated
        let nWorkspaces = global.screen.n_workspaces;
        let totalSpacing = (nWorkspaces - 1) * spacing;
        let avail = (box.y2 - box.y1) - totalSpacing;

        let newScale = (avail / nWorkspaces) / portholeHeight;
        newScale = Math.min(newScale, MAX_THUMBNAIL_SCALE);

        if (newScale != this._targetScale) {
            if (this._targetScale > 0) {
                // We don't do the tween immediately because we need to observe the ordering
                // in queueUpdateStates - if workspaces have been removed we need to slide them
                // out as the first thing.
                this._targetScale = newScale;
                this._pendingScaleUpdate = true;
            } else {
                this._targetScale = this._scale = newScale;
            }

            this._queueUpdateStates();
        }

        let thumbnailHeight = portholeHeight * this._scale;
        let thumbnailWidth = Math.round(portholeWidth * this._scale);
        let roundedHScale = thumbnailWidth / portholeWidth;

        let slideOffset; // X offset when thumbnail is fully slid offscreen
        if (rtl)
            slideOffset = - (thumbnailWidth + themeNode.get_padding(St.Side.LEFT));
        else
            slideOffset = thumbnailWidth + themeNode.get_padding(St.Side.RIGHT);

        let indicatorY1 = this._indicatorY;
        let indicatorY2;
        // when not animating, the workspace position overrides this._indicatorY
        let indicatorWorkspace = !this._animatingIndicator ? global.screen.get_active_workspace() : null;
        let indicatorThemeNode = this._indicator.get_theme_node();

        let indicatorTopFullBorder = indicatorThemeNode.get_padding(St.Side.TOP) + indicatorThemeNode.get_border_width(St.Side.TOP);
        let indicatorBottomFullBorder = indicatorThemeNode.get_padding(St.Side.BOTTOM) + indicatorThemeNode.get_border_width(St.Side.BOTTOM);
        let indicatorLeftFullBorder = indicatorThemeNode.get_padding(St.Side.LEFT) + indicatorThemeNode.get_border_width(St.Side.LEFT);
        let indicatorRightFullBorder = indicatorThemeNode.get_padding(St.Side.RIGHT) + indicatorThemeNode.get_border_width(St.Side.RIGHT);

        let y = box.y1;

        if (this._dropPlaceholderPos == -1) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
                this._dropPlaceholder.hide();
            }));
        }

        let childBox = new Clutter.ActorBox();

        for (let i = 0; i < this._thumbnails.length; i++) {
            let thumbnail = this._thumbnails[i];

            if (i > 0)
                y += spacing - Math.round(thumbnail.collapseFraction * spacing);

            let x1, x2;
            if (rtl) {
                x1 = box.x1 + slideOffset * thumbnail.slidePosition;
                x2 = x1 + thumbnailWidth;
            } else {
                x1 = box.x2 - thumbnailWidth + slideOffset * thumbnail.slidePosition;
                x2 = x1 + thumbnailWidth;
            }

            if (i == this._dropPlaceholderPos) {
                let [minHeight, placeholderHeight] = this._dropPlaceholder.get_preferred_height(-1);
                childBox.x1 = x1;
                childBox.x2 = x1 + thumbnailWidth;
                childBox.y1 = Math.round(y);
                childBox.y2 = Math.round(y + placeholderHeight);
                this._dropPlaceholder.allocate(childBox, flags);
                Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
                    this._dropPlaceholder.show();
                }));
                y += placeholderHeight + spacing;
            }

            // We might end up with thumbnailHeight being something like 99.33
            // pixels. To make this work and not end up with a gap at the bottom,
            // we need some thumbnails to be 99 pixels and some 100 pixels height;
            // we compute an actual scale separately for each thumbnail.
            let y1 = Math.round(y);
            let y2 = Math.round(y + thumbnailHeight);
            let roundedVScale = (y2 - y1) / portholeHeight;

            if (thumbnail.metaWorkspace == indicatorWorkspace) {
                indicatorY1 = y1;
                indicatorY2 = y2;
            }

            // Allocating a scaled actor is funny - x1/y1 correspond to the origin
            // of the actor, but x2/y2 are increased by the *unscaled* size.
            childBox.x1 = x1;
            childBox.x2 = x1 + portholeWidth;
            childBox.y1 = y1;
            childBox.y2 = y1 + portholeHeight;

            thumbnail.actor.set_scale(roundedHScale, roundedVScale);
            thumbnail.actor.allocate(childBox, flags);

            // We round the collapsing portion so that we don't get thumbnails resizing
            // during an animation due to differences in rounded, but leave the uncollapsed
            // portion unrounded so that non-animating we end up with the right total
            y += thumbnailHeight - Math.round(thumbnailHeight * thumbnail.collapseFraction);
        }

        if (rtl) {
            childBox.x1 = box.x1;
            childBox.x2 = box.x1 + thumbnailWidth;
        } else {
            childBox.x1 = box.x2 - thumbnailWidth;
            childBox.x2 = box.x2;
        }
        childBox.x1 -= indicatorLeftFullBorder;
        childBox.x2 += indicatorRightFullBorder;
        childBox.y1 = indicatorY1 - indicatorTopFullBorder;
        childBox.y2 = (indicatorY2 ? indicatorY2 : (indicatorY1 + thumbnailHeight)) + indicatorBottomFullBorder;
        this._indicator.allocate(childBox, flags);
    },

    _activeWorkspaceChanged(wm, from, to, direction) {
        let thumbnail;
        let activeWorkspace = global.screen.get_active_workspace();
        for (let i = 0; i < this._thumbnails.length; i++) {
            if (this._thumbnails[i].metaWorkspace == activeWorkspace) {
                thumbnail = this._thumbnails[i];
                break;
            }
        }

        this._animatingIndicator = true;
        let indicatorThemeNode = this._indicator.get_theme_node();
        let indicatorTopFullBorder = indicatorThemeNode.get_padding(St.Side.TOP) + indicatorThemeNode.get_border_width(St.Side.TOP);
        this.indicatorY = this._indicator.allocation.y1 + indicatorTopFullBorder;
        Tweener.addTween(this,
                         { indicatorY: thumbnail.actor.allocation.y1,
                           time: WorkspacesView.WORKSPACE_SWITCH_TIME,
                           transition: 'easeOutQuad',
                           onComplete() {
                               this._animatingIndicator = false;
                               this._queueUpdateStates();
                           },
                           onCompleteScope: this
                         });
    }
});
