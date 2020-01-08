// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceThumbnail, ThumbnailsBox */

const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;

const Background = imports.ui.background;
const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Workspace = imports.ui.workspace;

// The maximum size of a thumbnail is 1/10 the width and height of the screen
let MAX_THUMBNAIL_SCALE = 1 / 10.;

var RESCALE_ANIMATION_TIME = 200;
var SLIDE_ANIMATION_TIME = 200;

// When we create workspaces by dragging, we add a "cut" into the top and
// bottom of each workspace so that the user doesn't have to hit the
// placeholder exactly.
var WORKSPACE_CUT_SIZE = 10;

var WORKSPACE_KEEP_ALIVE_TIME = 100;

var MUTTER_SCHEMA = 'org.gnome.mutter';

/* A layout manager that requests size only for primary_actor, but then allocates
   all using a fixed layout */
var PrimaryActorLayout = GObject.registerClass(
class PrimaryActorLayout extends Clutter.FixedLayout {
    _init(primaryActor) {
        super._init();

        this.primaryActor = primaryActor;
    }

    vfunc_get_preferred_width(container, forHeight) {
        return this.primaryActor.get_preferred_width(forHeight);
    }

    vfunc_get_preferred_height(container, forWidth) {
        return this.primaryActor.get_preferred_height(forWidth);
    }
});

var WindowClone = GObject.registerClass({
    Signals: {
        'drag-begin': {},
        'drag-cancelled': {},
        'drag-end': {},
        'selected': { param_types: [GObject.TYPE_UINT] },
    },
}, class WindowClone extends Clutter.Actor {
    _init(realWindow) {
        let clone = new Clutter.Clone({ source: realWindow });
        super._init({
            layout_manager: new PrimaryActorLayout(clone),
            reactive: true,
        });
        this._delegate = this;

        this.add_child(clone);
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;

        clone._updateId = this.realWindow.connect('notify::position',
                                                  this._onPositionChanged.bind(this));
        clone._destroyId = this.realWindow.connect('destroy', () => {
            // First destroy the clone and then destroy everything
            // This will ensure that we never see it in the _disconnectSignals loop
            clone.destroy();
            this.destroy();
        });
        this._onPositionChanged();

        this.connect('destroy', this._onDestroy.bind(this));

        this._draggable = DND.makeDraggable(this,
                                            { restoreOnSuccess: true,
                                              dragActorMaxSize: Workspace.WINDOW_DND_SIZE,
                                              dragActorOpacity: Workspace.DRAGGING_WINDOW_OPACITY });
        this._draggable.connect('drag-begin', this._onDragBegin.bind(this));
        this._draggable.connect('drag-cancelled', this._onDragCancelled.bind(this));
        this._draggable.connect('drag-end', this._onDragEnd.bind(this));
        this.inDrag = false;

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
    }

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
    }

    setStackAbove(actor) {
        this._stackAbove = actor;

        // Don't apply the new stacking now, it will be applied
        // when dragging ends and window are stacked again
        if (actor.inDrag)
            return;

        let parent = this.get_parent();
        let actualAbove = this.getActualStackAbove();
        if (actualAbove == null)
            parent.set_child_below_sibling(this, null);
        else
            parent.set_child_above_sibling(this, actualAbove);
    }

    addAttachedDialog(win) {
        this._doAddAttachedDialog(win, win.get_compositor_private());
    }

    _doAddAttachedDialog(metaDialog, realDialog) {
        let clone = new Clutter.Clone({ source: realDialog });
        this._updateDialogPosition(realDialog, clone);

        clone._updateId = realDialog.connect('notify::position', dialog => {
            this._updateDialogPosition(dialog, clone);
        });
        clone._destroyId = realDialog.connect('destroy', () => {
            clone.destroy();
        });
        this.add_child(clone);
    }

    _updateDialogPosition(realDialog, cloneDialog) {
        let metaDialog = realDialog.meta_window;
        let dialogRect = metaDialog.get_frame_rect();
        let rect = this.metaWindow.get_frame_rect();

        cloneDialog.set_position(dialogRect.x - rect.x, dialogRect.y - rect.y);
    }

    _onPositionChanged() {
        this.set_position(this.realWindow.x, this.realWindow.y);
    }

    _disconnectSignals() {
        this.get_children().forEach(child => {
            let realWindow = child.source;

            realWindow.disconnect(child._updateId);
            realWindow.disconnect(child._destroyId);
        });
    }

    _onDestroy() {
        this._disconnectSignals();

        this._delegate = null;

        if (this.inDrag) {
            this.emit('drag-end');
            this.inDrag = false;
        }
    }

    vfunc_button_press_event() {
        return Clutter.EVENT_STOP;
    }

    vfunc_button_release_event(buttonEvent) {
        this.emit('selected', buttonEvent.time);

        return Clutter.EVENT_STOP;
    }

    vfunc_touch_event(touchEvent) {
        if (touchEvent.type != Clutter.EventType.TOUCH_END ||
            !global.display.is_pointer_emulating_sequence(touchEvent.sequence))
            return Clutter.EVENT_PROPAGATE;

        this.emit('selected', touchEvent.time);
        return Clutter.EVENT_STOP;
    }

    _onDragBegin(_draggable, _time) {
        this.inDrag = true;
        this.emit('drag-begin');
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


        this.emit('drag-end');
    }
});


var ThumbnailState = {
    NEW:            0,
    ANIMATING_IN:   1,
    NORMAL:         2,
    REMOVING:       3,
    ANIMATING_OUT:  4,
    ANIMATED_OUT:   5,
    COLLAPSING:     6,
    DESTROYED:      7,
};

/**
 * @metaWorkspace: a #Meta.Workspace
 */
var WorkspaceThumbnail = GObject.registerClass({
    Properties: {
        'collapse-fraction': GObject.ParamSpec.double(
            'collapse-fraction', 'collapse-fraction', 'collapse-fraction',
            GObject.ParamFlags.READWRITE,
            0, 1, 0),
        'slide-position': GObject.ParamSpec.double(
            'slide-position', 'slide-position', 'slide-position',
            GObject.ParamFlags.READWRITE,
            0, 1, 0),
    },
}, class WorkspaceThumbnail extends St.Widget {
    _init(metaWorkspace) {
        super._init({
            clip_to_allocation: true,
            style_class: 'workspace-thumbnail',
        });
        this._delegate = this;

        this.metaWorkspace = metaWorkspace;
        this.monitorIndex = Main.layoutManager.primaryIndex;

        this._removed = false;

        this._contents = new Clutter.Actor();
        this.add_child(this._contents);

        this.connect('destroy', this._onDestroy.bind(this));

        this._createBackground();

        let workArea = Main.layoutManager.getWorkAreaForMonitor(this.monitorIndex);
        this.setPorthole(workArea.x, workArea.y, workArea.width, workArea.height);

        let windows = global.get_window_actors().filter(actor => {
            let win = actor.meta_window;
            return win.located_on_workspace(metaWorkspace);
        });

        // Create clones for windows that should be visible in the Overview
        this._windows = [];
        this._allWindows = [];
        this._minimizedChangedIds = [];
        for (let i = 0; i < windows.length; i++) {
            let minimizedChangedId =
                windows[i].meta_window.connect('notify::minimized',
                                               this._updateMinimized.bind(this));
            this._allWindows.push(windows[i].meta_window);
            this._minimizedChangedIds.push(minimizedChangedId);

            if (this._isMyWindow(windows[i]) && this._isOverviewWindow(windows[i]))
                this._addWindowClone(windows[i]);
        }

        // Track window changes
        this._windowAddedId = this.metaWorkspace.connect('window-added',
                                                         this._windowAdded.bind(this));
        this._windowRemovedId = this.metaWorkspace.connect('window-removed',
                                                           this._windowRemoved.bind(this));
        this._windowEnteredMonitorId = global.display.connect('window-entered-monitor',
                                                              this._windowEnteredMonitor.bind(this));
        this._windowLeftMonitorId = global.display.connect('window-left-monitor',
                                                           this._windowLeftMonitor.bind(this));

        this.state = ThumbnailState.NORMAL;
        this._slidePosition = 0; // Fully slid in
        this._collapseFraction = 0; // Not collapsed
    }

    _createBackground() {
        this._bgManager = new Background.BackgroundManager({ monitorIndex: Main.layoutManager.primaryIndex,
                                                             container: this._contents,
                                                             vignette: false });
    }

    setPorthole(x, y, width, height) {
        this.set_size(width, height);
        this._contents.set_position(-x, -y);
    }

    _lookupIndex(metaWindow) {
        return this._windows.findIndex(w => w.metaWindow == metaWindow);
    }

    syncStacking(stackIndices) {
        this._windows.sort((a, b) => {
            let indexA = stackIndices[a.metaWindow.get_stable_sequence()];
            let indexB = stackIndices[b.metaWindow.get_stable_sequence()];
            return indexA - indexB;
        });

        for (let i = 0; i < this._windows.length; i++) {
            let clone = this._windows[i];
            if (i == 0) {
                clone.setStackAbove(this._bgManager.backgroundActor);
            } else {
                let previousClone = this._windows[i - 1];
                clone.setStackAbove(previousClone);
            }
        }
    }

    // eslint-disable-next-line camelcase
    set slide_position(slidePosition) {
        if (this._slidePosition == slidePosition)
            return;
        this._slidePosition = slidePosition;
        this.notify('slide-position');
        this.queue_relayout();
    }

    // eslint-disable-next-line camelcase
    get slide_position() {
        return this._slidePosition;
    }

    // eslint-disable-next-line camelcase
    set collapse_fraction(collapseFraction) {
        if (this._collapseFraction == collapseFraction)
            return;
        this._collapseFraction = collapseFraction;
        this.notify('collapse-fraction');
        this.queue_relayout();
    }

    // eslint-disable-next-line camelcase
    get collapse_fraction() {
        return this._collapseFraction;
    }

    _doRemoveWindow(metaWin) {
        let clone = this._removeWindowClone(metaWin);
        if (clone)
            clone.destroy();
    }

    _doAddWindow(metaWin) {
        if (this._removed)
            return;

        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                if (!this._removed &&
                    metaWin.get_compositor_private() &&
                    metaWin.get_workspace() == this.metaWorkspace)
                    this._doAddWindow(metaWin);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._doAddWindow');
            return;
        }

        if (!this._allWindows.includes(metaWin)) {
            let minimizedChangedId = metaWin.connect('notify::minimized',
                                                     this._updateMinimized.bind(this));
            this._allWindows.push(metaWin);
            this._minimizedChangedIds.push(minimizedChangedId);
        }

        // We might have the window in our list already if it was on all workspaces and
        // now was moved to this workspace
        if (this._lookupIndex(metaWin) != -1)
            return;

        if (!this._isMyWindow(win))
            return;

        if (this._isOverviewWindow(win)) {
            this._addWindowClone(win);
        } else if (metaWin.is_attached_dialog()) {
            let parent = metaWin.get_transient_for();
            while (parent.is_attached_dialog())
                parent = parent.get_transient_for();

            let idx = this._lookupIndex(parent);
            if (idx < 0) {
                // parent was not created yet, it will take care
                // of the dialog when created
                return;
            }

            let clone = this._windows[idx];
            clone.addAttachedDialog(metaWin);
        }
    }

    _windowAdded(metaWorkspace, metaWin) {
        this._doAddWindow(metaWin);
    }

    _windowRemoved(metaWorkspace, metaWin) {
        let index = this._allWindows.indexOf(metaWin);
        if (index != -1) {
            metaWin.disconnect(this._minimizedChangedIds[index]);
            this._allWindows.splice(index, 1);
            this._minimizedChangedIds.splice(index, 1);
        }

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

    _updateMinimized(metaWin) {
        if (metaWin.minimized)
            this._doRemoveWindow(metaWin);
        else
            this._doAddWindow(metaWin);
    }

    workspaceRemoved() {
        if (this._removed)
            return;

        this._removed = true;

        this.metaWorkspace.disconnect(this._windowAddedId);
        this.metaWorkspace.disconnect(this._windowRemovedId);
        global.display.disconnect(this._windowEnteredMonitorId);
        global.display.disconnect(this._windowLeftMonitorId);

        for (let i = 0; i < this._allWindows.length; i++)
            this._allWindows[i].disconnect(this._minimizedChangedIds[i]);
    }

    _onDestroy() {
        this.workspaceRemoved();

        if (this._bgManager) {
            this._bgManager.destroy();
            this._bgManager = null;
        }

        this._windows = [];
    }

    // Tests if @actor belongs to this workspace and monitor
    _isMyWindow(actor) {
        let win = actor.meta_window;
        return win.located_on_workspace(this.metaWorkspace) &&
            (win.get_monitor() == this.monitorIndex);
    }

    // Tests if @win should be shown in the Overview
    _isOverviewWindow(win) {
        return !win.get_meta_window().skip_taskbar &&
               win.get_meta_window().showing_on_its_workspace();
    }

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone(win) {
        let clone = new WindowClone(win);

        clone.connect('selected', (o, time) => {
            this.activate(time);
        });
        clone.connect('drag-begin', () => {
            Main.overview.beginWindowDrag(clone.metaWindow);
        });
        clone.connect('drag-cancelled', () => {
            Main.overview.cancelledWindowDrag(clone.metaWindow);
        });
        clone.connect('drag-end', () => {
            Main.overview.endWindowDrag(clone.metaWindow);
        });
        clone.connect('destroy', () => {
            this._removeWindowClone(clone.metaWindow);
        });
        this._contents.add_actor(clone);

        if (this._windows.length == 0)
            clone.setStackAbove(this._bgManager.backgroundActor);
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

    activate(time) {
        if (this.state > ThumbnailState.NORMAL)
            return;

        // a click on the already current workspace should go back to the main view
        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        if (this.metaWorkspace == activeWorkspace)
            Main.overview.hide();
        else
            this.metaWorkspace.activate(time);
    }

    // Draggable target interface used only by ThumbnailsBox
    handleDragOverInternal(source, actor, time) {
        if (source == Main.xdndHandler) {
            this.metaWorkspace.activate(time);
            return DND.DragMotionResult.CONTINUE;
        }

        if (this.state > ThumbnailState.NORMAL)
            return DND.DragMotionResult.CONTINUE;

        if (source.realWindow && !this._isMyWindow(source.realWindow))
            return DND.DragMotionResult.MOVE_DROP;
        if (source.app && source.app.can_open_new_window())
            return DND.DragMotionResult.COPY_DROP;
        if (!source.app && source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    }

    acceptDropInternal(source, actor, time) {
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
        } else if (source.app && source.app.can_open_new_window()) {
            if (source.animateLaunchAtPos)
                source.animateLaunchAtPos(actor.x, actor.y);

            source.app.open_new_window(this.metaWorkspace.index());
            return true;
        } else if (!source.app && source.shellWorkspaceLaunch) {
            // While unused in our own drag sources, shellWorkspaceLaunch allows
            // extensions to define custom actions for their drag sources.
            source.shellWorkspaceLaunch({ workspace: this.metaWorkspace.index(),
                                          timestamp: time });
            return true;
        }

        return false;
    }
});


var ThumbnailsBox = GObject.registerClass({
    Properties: {
        'indicator-y': GObject.ParamSpec.double(
            'indicator-y', 'indicator-y', 'indicator-y',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
        'scale': GObject.ParamSpec.double(
            'scale', 'scale', 'scale',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
    },
}, class ThumbnailsBox extends St.Widget {
    _init(scrollAdjustment) {
        super._init({ reactive: true,
                      style_class: 'workspace-thumbnails',
                      request_mode: Clutter.RequestMode.WIDTH_FOR_HEIGHT });

        this._delegate = this;

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator' });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._indicator = indicator;
        this.add_actor(indicator);

        // The porthole is the part of the screen we're showing in the thumbnails
        this._porthole = { width: global.stage.width, height: global.stage.height,
                           x: global.stage.x, y: global.stage.y };

        this._dropWorkspace = -1;
        this._dropPlaceholderPos = -1;
        this._dropPlaceholder = new St.Bin({ style_class: 'placeholder' });
        this.add_actor(this._dropPlaceholder);
        this._spliceIndex = -1;

        this._targetScale = 0;
        this._scale = 0;
        this._pendingScaleUpdate = false;
        this._stateUpdateQueued = false;
        this._animatingIndicator = false;

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this._thumbnails = [];

        Main.overview.connect('showing',
                              this._createThumbnails.bind(this));
        Main.overview.connect('hidden',
                              this._destroyThumbnails.bind(this));

        Main.overview.connect('item-drag-begin',
                              this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end',
                              this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-cancelled',
                              this._onDragCancelled.bind(this));
        Main.overview.connect('window-drag-begin',
                              this._onDragBegin.bind(this));
        Main.overview.connect('window-drag-end',
                              this._onDragEnd.bind(this));
        Main.overview.connect('window-drag-cancelled',
                              this._onDragCancelled.bind(this));

        this._settings = new Gio.Settings({ schema_id: MUTTER_SCHEMA });
        this._settings.connect('changed::dynamic-workspaces',
            this._updateSwitcherVisibility.bind(this));

        Main.layoutManager.connect('monitors-changed', () => {
            this._destroyThumbnails();
            if (Main.overview.visible)
                this._createThumbnails();
        });

        global.display.connect('workareas-changed',
                               this._updatePorthole.bind(this));

        this._switchWorkspaceNotifyId = 0;
        this._nWorkspacesNotifyId = 0;
        this._syncStackingId = 0;
        this._workareasChangedId = 0;

        this._scrollAdjustment = scrollAdjustment;

        this._scrollAdjustment.connect('notify::value', adj => {
            let workspaceManager = global.workspace_manager;
            let activeIndex = workspaceManager.get_active_workspace_index();

            this._animatingIndicator = adj.value !== activeIndex;

            if (!this._animatingIndicator)
                this._queueUpdateStates();

            this.queue_relayout();
        });
    }

    _updateSwitcherVisibility() {
        let workspaceManager = global.workspace_manager;

        this.visible =
            this._settings.get_boolean('dynamic-workspaces') ||
                workspaceManager.n_workspaces > 1;
    }

    _activateThumbnailAtPoint(stageX, stageY, time) {
        let [r_, x_, y] = this.transform_stage_point(stageX, stageY);

        let thumbnail = this._thumbnails.find(t => {
            let [, h] = t.get_transformed_size();
            return y >= t.y && y <= t.y + h;
        });
        if (thumbnail)
            thumbnail.activate(time);
    }

    vfunc_button_release_event(buttonEvent) {
        let { x, y } = buttonEvent;
        this._activateThumbnailAtPoint(x, y, buttonEvent.time);
        return Clutter.EVENT_STOP;
    }

    vfunc_touch_event(touchEvent) {
        if (touchEvent.type == Clutter.EventType.TOUCH_END &&
            global.display.is_pointer_emulating_sequence(touchEvent.sequence)) {
            let { x, y } = touchEvent;
            this._activateThumbnailAtPoint(x, y, touchEvent.time);
        }

        return Clutter.EVENT_STOP;
    }

    _onDragBegin() {
        this._dragCancelled = false;
        this._dragMonitor = {
            dragMotion: this._onDragMotion.bind(this),
        };
        DND.addDragMonitor(this._dragMonitor);
    }

    _onDragEnd() {
        if (this._dragCancelled)
            return;

        this._endDrag();
    }

    _onDragCancelled() {
        this._dragCancelled = true;
        this._endDrag();
    }

    _endDrag() {
        this._clearDragPlaceholder();
        DND.removeDragMonitor(this._dragMonitor);
    }

    _onDragMotion(dragEvent) {
        if (!this.contains(dragEvent.targetActor))
            this._onLeave();
        return DND.DragMotionResult.CONTINUE;
    }

    _onLeave() {
        this._clearDragPlaceholder();
    }

    _clearDragPlaceholder() {
        if (this._dropPlaceholderPos == -1)
            return;

        this._dropPlaceholderPos = -1;
        this.queue_relayout();
    }

    // Draggable target interface
    handleDragOver(source, actor, x, y, time) {
        if (!source.realWindow &&
            (!source.app || !source.app.can_open_new_window()) &&
            (source.app || !source.shellWorkspaceLaunch) &&
            source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        let canCreateWorkspaces = Meta.prefs_get_dynamic_workspaces();
        let spacing = this.get_theme_node().get_length('spacing');

        this._dropWorkspace = -1;
        let placeholderPos = -1;
        let targetBase;
        if (this._dropPlaceholderPos == 0)
            targetBase = this._dropPlaceholder.y;
        else
            targetBase = this._thumbnails[0].y;
        let targetTop = targetBase - spacing - WORKSPACE_CUT_SIZE;
        let length = this._thumbnails.length;
        for (let i = 0; i < length; i++) {
            // Allow the reorder target to have a 10px "cut" into
            // each side of the thumbnail, to make dragging onto the
            // placeholder easier
            let [, h] = this._thumbnails[i].get_transformed_size();
            let targetBottom = targetBase + WORKSPACE_CUT_SIZE;
            let nextTargetBase = targetBase + h + spacing;
            let nextTargetTop =  nextTargetBase - spacing - (i == length - 1 ? 0 : WORKSPACE_CUT_SIZE);

            // Expand the target to include the placeholder, if it exists.
            if (i == this._dropPlaceholderPos)
                targetBottom += this._dropPlaceholder.get_height();

            if (y > targetTop && y <= targetBottom && source != Main.xdndHandler && canCreateWorkspaces) {
                placeholderPos = i;
                break;
            } else if (y > targetBottom && y <= nextTargetTop) {
                this._dropWorkspace = i;
                break;
            }

            targetBase = nextTargetBase;
            targetTop = nextTargetTop;
        }

        if (this._dropPlaceholderPos != placeholderPos) {
            this._dropPlaceholderPos = placeholderPos;
            this.queue_relayout();
        }

        if (this._dropWorkspace != -1)
            return this._thumbnails[this._dropWorkspace].handleDragOverInternal(source, actor, time);
        else if (this._dropPlaceholderPos != -1)
            return source.realWindow ? DND.DragMotionResult.MOVE_DROP : DND.DragMotionResult.COPY_DROP;
        else
            return DND.DragMotionResult.CONTINUE;
    }

    acceptDrop(source, actor, x, y, time) {
        if (this._dropWorkspace != -1) {
            return this._thumbnails[this._dropWorkspace].acceptDropInternal(source, actor, time);
        } else if (this._dropPlaceholderPos != -1) {
            if (!source.realWindow &&
                (!source.app || !source.app.can_open_new_window()) &&
                (source.app || !source.shellWorkspaceLaunch))
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
            } else if (source.app && source.app.can_open_new_window()) {
                if (source.animateLaunchAtPos)
                    source.animateLaunchAtPos(actor.x, actor.y);

                source.app.open_new_window(newWorkspaceIndex);
            } else if (!source.app && source.shellWorkspaceLaunch) {
                // While unused in our own drag sources, shellWorkspaceLaunch allows
                // extensions to define custom actions for their drag sources.
                source.shellWorkspaceLaunch({ workspace: newWorkspaceIndex,
                                              timestamp: time });
            }

            if (source.app || (!source.app && source.shellWorkspaceLaunch)) {
                // This new workspace will be automatically removed if the application fails
                // to open its first window within some time, as tracked by Shell.WindowTracker.
                // Here, we only add a very brief timeout to avoid the _immediate_ removal of the
                // workspace while we wait for the startup sequence to load.
                let workspaceManager = global.workspace_manager;
                Main.wm.keepWorkspaceAlive(workspaceManager.get_workspace_by_index(newWorkspaceIndex),
                                           WORKSPACE_KEEP_ALIVE_TIME);
            }

            // Start the animation on the workspace (which is actually
            // an old one which just became empty)
            let thumbnail = this._thumbnails[newWorkspaceIndex];
            this._setThumbnailState(thumbnail, ThumbnailState.NEW);
            thumbnail.slide_position = 1;

            this._queueUpdateStates();

            return true;
        } else {
            return false;
        }
    }

    _createThumbnails() {
        let workspaceManager = global.workspace_manager;

        this._nWorkspacesNotifyId =
            workspaceManager.connect('notify::n-workspaces',
                                     this._workspacesChanged.bind(this));
        this._workspacesReorderedId =
            workspaceManager.connect('workspaces-reordered', () => {
                this._thumbnails.sort((a, b) => {
                    return a.metaWorkspace.index() - b.metaWorkspace.index();
                });
                this.queue_relayout();
            });
        this._syncStackingId =
            Main.overview.connect('windows-restacked',
                                  this._syncStacking.bind(this));

        this._targetScale = 0;
        this._scale = 0;
        this._pendingScaleUpdate = false;
        this._stateUpdateQueued = false;

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this.addThumbnails(0, workspaceManager.n_workspaces);

        this._updateSwitcherVisibility();
    }

    _destroyThumbnails() {
        if (this._thumbnails.length == 0)
            return;

        if (this._nWorkspacesNotifyId > 0) {
            let workspaceManager = global.workspace_manager;
            workspaceManager.disconnect(this._nWorkspacesNotifyId);
            this._nWorkspacesNotifyId = 0;
        }
        if (this._workspacesReorderedId > 0) {
            let workspaceManager = global.workspace_manager;
            workspaceManager.disconnect(this._workspacesReorderedId);
            this._workspacesReorderedId = 0;
        }

        if (this._syncStackingId > 0) {
            Main.overview.disconnect(this._syncStackingId);
            this._syncStackingId = 0;
        }

        for (let w = 0; w < this._thumbnails.length; w++)
            this._thumbnails[w].destroy();
        this._thumbnails = [];
    }

    _workspacesChanged() {
        let validThumbnails =
            this._thumbnails.filter(t => t.state <= ThumbnailState.NORMAL);
        let workspaceManager = global.workspace_manager;
        let oldNumWorkspaces = validThumbnails.length;
        let newNumWorkspaces = workspaceManager.n_workspaces;

        if (newNumWorkspaces > oldNumWorkspaces) {
            this.addThumbnails(oldNumWorkspaces, newNumWorkspaces - oldNumWorkspaces);
        } else {
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let metaWorkspace = workspaceManager.get_workspace_by_index(w);
                if (this._thumbnails[w].metaWorkspace != metaWorkspace) {
                    removedIndex = w;
                    break;
                }
            }

            this.removeThumbnails(removedIndex, removedNum);
        }

        this._updateSwitcherVisibility();
    }

    addThumbnails(start, count) {
        let workspaceManager = global.workspace_manager;

        for (let k = start; k < start + count; k++) {
            let metaWorkspace = workspaceManager.get_workspace_by_index(k);
            let thumbnail = new WorkspaceThumbnail(metaWorkspace);
            thumbnail.setPorthole(this._porthole.x, this._porthole.y,
                                  this._porthole.width, this._porthole.height);
            this._thumbnails.push(thumbnail);
            this.add_actor(thumbnail);

            if (start > 0 && this._spliceIndex == -1) {
                // not the initial fill, and not splicing via DND
                thumbnail.state = ThumbnailState.NEW;
                thumbnail.slide_position = 1; // start slid out
                this._haveNewThumbnails = true;
            } else {
                thumbnail.state = ThumbnailState.NORMAL;
            }

            this._stateCounts[thumbnail.state]++;
        }

        this._queueUpdateStates();

        // The thumbnails indicator actually needs to be on top of the thumbnails
        this.set_child_above_sibling(this._indicator, null);

        // Clear the splice index, we got the message
        this._spliceIndex = -1;
    }

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
    }

    _syncStacking(overview, stackIndices) {
        for (let i = 0; i < this._thumbnails.length; i++)
            this._thumbnails[i].syncStacking(stackIndices);
    }

    set scale(scale) {
        if (this._scale == scale)
            return;

        this._scale = scale;
        this.notify('scale');
        this.queue_relayout();
    }

    get scale() {
        return this._scale;
    }

    _setThumbnailState(thumbnail, state) {
        this._stateCounts[thumbnail.state]--;
        thumbnail.state = state;
        this._stateCounts[thumbnail.state]++;
    }

    _iterateStateThumbnails(state, callback) {
        if (this._stateCounts[state] == 0)
            return;

        for (let i = 0; i < this._thumbnails.length; i++) {
            if (this._thumbnails[i].state == state)
                callback.call(this, this._thumbnails[i]);
        }
    }

    _updateStates() {
        this._stateUpdateQueued = false;

        // If we are animating the indicator, wait
        if (this._animatingIndicator)
            return;

        // Then slide out any thumbnails that have been destroyed
        this._iterateStateThumbnails(ThumbnailState.REMOVING, thumbnail => {
            this._setThumbnailState(thumbnail, ThumbnailState.ANIMATING_OUT);

            thumbnail.ease_property('slide-position', 1, {
                duration: SLIDE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
                onComplete: () => {
                    this._setThumbnailState(thumbnail, ThumbnailState.ANIMATED_OUT);
                    this._queueUpdateStates();
                },
            });
        });

        // As long as things are sliding out, don't proceed
        if (this._stateCounts[ThumbnailState.ANIMATING_OUT] > 0)
            return;

        // Once that's complete, we can start scaling to the new size and collapse any removed thumbnails
        this._iterateStateThumbnails(ThumbnailState.ANIMATED_OUT, thumbnail => {
            this._setThumbnailState(thumbnail, ThumbnailState.COLLAPSING);
            thumbnail.ease_property('collapse-fraction', 1, {
                duration: RESCALE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._stateCounts[thumbnail.state]--;
                    thumbnail.state = ThumbnailState.DESTROYED;

                    let index = this._thumbnails.indexOf(thumbnail);
                    this._thumbnails.splice(index, 1);
                    thumbnail.destroy();

                    this._queueUpdateStates();
                },
            });
        });

        if (this._pendingScaleUpdate) {
            this.ease_property('scale', this._targetScale, {
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: RESCALE_ANIMATION_TIME,
                onComplete: () => this._queueUpdateStates(),
            });
            this._pendingScaleUpdate = false;
        }

        // Wait until that's done
        if (this._scale != this._targetScale || this._stateCounts[ThumbnailState.COLLAPSING] > 0)
            return;

        // And then slide in any new thumbnails
        this._iterateStateThumbnails(ThumbnailState.NEW, thumbnail => {
            this._setThumbnailState(thumbnail, ThumbnailState.ANIMATING_IN);
            thumbnail.ease_property('slide-position', 0, {
                duration: SLIDE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._setThumbnailState(thumbnail, ThumbnailState.NORMAL);
                },
            });
        });
    }

    _queueUpdateStates() {
        if (this._stateUpdateQueued)
            return;

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                       this._updateStates.bind(this));

        this._stateUpdateQueued = true;
    }

    vfunc_get_preferred_height(_forWidth) {
        // Note that for getPreferredWidth/Height we cheat a bit and skip propagating
        // the size request to our children because we know how big they are and know
        // that the actors aren't depending on the virtual functions being called.
        let workspaceManager = global.workspace_manager;
        let themeNode = this.get_theme_node();

        let spacing = themeNode.get_length('spacing');
        let nWorkspaces = workspaceManager.n_workspaces;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        let naturalHeight = totalSpacing + nWorkspaces * this._porthole.height * MAX_THUMBNAIL_SCALE;

        return themeNode.adjust_preferred_height(totalSpacing, naturalHeight);
    }

    vfunc_get_preferred_width(forHeight) {
        let workspaceManager = global.workspace_manager;
        let themeNode = this.get_theme_node();

        forHeight = themeNode.adjust_for_height(forHeight);

        let spacing = themeNode.get_length('spacing');
        let nWorkspaces = workspaceManager.n_workspaces;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        let avail = forHeight - totalSpacing;

        let scale = (avail / nWorkspaces) / this._porthole.height;
        scale = Math.min(scale, MAX_THUMBNAIL_SCALE);

        let width = Math.round(this._porthole.width * scale);

        return themeNode.adjust_preferred_width(width, width);
    }

    _updatePorthole() {
        if (!Main.layoutManager.primaryMonitor) {
            this._porthole = { width: global.stage.width, height: global.stage.height,
                               x: global.stage.x, y: global.stage.y };
        } else {
            this._porthole = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        }

        this.queue_relayout();
    }

    vfunc_allocate(box, flags) {
        this.set_allocation(box, flags);

        let rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;

        if (this._thumbnails.length == 0) // not visible
            return;

        let workspaceManager = global.workspace_manager;
        let themeNode = this.get_theme_node();

        box = themeNode.get_content_box(box);

        let portholeWidth = this._porthole.width;
        let portholeHeight = this._porthole.height;
        let spacing = themeNode.get_length('spacing');

        // Compute the scale we'll need once everything is updated
        let nWorkspaces = workspaceManager.n_workspaces;
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
            slideOffset = -(thumbnailWidth + themeNode.get_padding(St.Side.LEFT));
        else
            slideOffset = thumbnailWidth + themeNode.get_padding(St.Side.RIGHT);

        let indicatorValue = this._scrollAdjustment.value;
        let indicatorUpperWs = Math.ceil(indicatorValue);
        let indicatorLowerWs = Math.floor(indicatorValue);

        let indicatorLowerY1 = 0;
        let indicatorLowerY2 = 0;
        let indicatorUpperY1 = 0;
        let indicatorUpperY2 = 0;

        let indicatorThemeNode = this._indicator.get_theme_node();
        let indicatorTopFullBorder = indicatorThemeNode.get_padding(St.Side.TOP) + indicatorThemeNode.get_border_width(St.Side.TOP);
        let indicatorBottomFullBorder = indicatorThemeNode.get_padding(St.Side.BOTTOM) + indicatorThemeNode.get_border_width(St.Side.BOTTOM);
        let indicatorLeftFullBorder = indicatorThemeNode.get_padding(St.Side.LEFT) + indicatorThemeNode.get_border_width(St.Side.LEFT);
        let indicatorRightFullBorder = indicatorThemeNode.get_padding(St.Side.RIGHT) + indicatorThemeNode.get_border_width(St.Side.RIGHT);

        let y = box.y1;

        if (this._dropPlaceholderPos == -1) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._dropPlaceholder.hide();
            });
        }

        let childBox = new Clutter.ActorBox();

        for (let i = 0; i < this._thumbnails.length; i++) {
            let thumbnail = this._thumbnails[i];

            if (i > 0)
                y += spacing - Math.round(thumbnail.collapse_fraction * spacing);

            let x1, x2;
            if (rtl) {
                x1 = box.x1 + slideOffset * thumbnail.slide_position;
                x2 = x1 + thumbnailWidth;
            } else {
                x1 = box.x2 - thumbnailWidth + slideOffset * thumbnail.slide_position;
                x2 = x1 + thumbnailWidth;
            }

            if (i == this._dropPlaceholderPos) {
                let [, placeholderHeight] = this._dropPlaceholder.get_preferred_height(-1);
                childBox.x1 = x1;
                childBox.x2 = x2;
                childBox.y1 = Math.round(y);
                childBox.y2 = Math.round(y + placeholderHeight);
                this._dropPlaceholder.allocate(childBox, flags);
                Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                    this._dropPlaceholder.show();
                });
                y += placeholderHeight + spacing;
            }

            // We might end up with thumbnailHeight being something like 99.33
            // pixels. To make this work and not end up with a gap at the bottom,
            // we need some thumbnails to be 99 pixels and some 100 pixels height;
            // we compute an actual scale separately for each thumbnail.
            let y1 = Math.round(y);
            let y2 = Math.round(y + thumbnailHeight);
            let roundedVScale = (y2 - y1) / portholeHeight;

            if (i === indicatorUpperWs) {
                indicatorUpperY1 = y1;
                indicatorUpperY2 = y2;
            }
            if (i === indicatorLowerWs) {
                indicatorLowerY1 = y1;
                indicatorLowerY2 = y2;
            }

            // Allocating a scaled actor is funny - x1/y1 correspond to the origin
            // of the actor, but x2/y2 are increased by the *unscaled* size.
            childBox.x1 = x1;
            childBox.x2 = x1 + portholeWidth;
            childBox.y1 = y1;
            childBox.y2 = y1 + portholeHeight;

            thumbnail.set_scale(roundedHScale, roundedVScale);
            thumbnail.allocate(childBox, flags);

            // We round the collapsing portion so that we don't get thumbnails resizing
            // during an animation due to differences in rounded, but leave the uncollapsed
            // portion unrounded so that non-animating we end up with the right total
            y += thumbnailHeight - Math.round(thumbnailHeight * thumbnail.collapse_fraction);
        }

        if (rtl) {
            childBox.x1 = box.x1;
            childBox.x2 = box.x1 + thumbnailWidth;
        } else {
            childBox.x1 = box.x2 - thumbnailWidth;
            childBox.x2 = box.x2;
        }
        let indicatorY1 = indicatorLowerY1 +
            (indicatorUpperY1 - indicatorLowerY1) * (indicatorValue % 1);
        let indicatorY2 = indicatorLowerY2 +
            (indicatorUpperY2 - indicatorLowerY2) * (indicatorValue % 1);

        childBox.x1 -= indicatorLeftFullBorder;
        childBox.x2 += indicatorRightFullBorder;
        childBox.y1 = indicatorY1 - indicatorTopFullBorder;
        childBox.y2 = indicatorY2 + indicatorBottomFullBorder;
        this._indicator.allocate(childBox, flags);
    }
});
