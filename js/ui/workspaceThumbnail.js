// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceThumbnail, ThumbnailsBox */

const { Clutter, Gio, GLib, GObject, Graphene, Meta, Shell, St } = imports.gi;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const { TransientSignalHolder } = imports.misc.signalTracker;
const Util = imports.misc.util;
const Workspace = imports.ui.workspace;

const NUM_WORKSPACES_THRESHOLD = 2;

// The maximum size of a thumbnail is 5% the width and height of the screen
var MAX_THUMBNAIL_SCALE = 0.05;

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

        this.realWindow.connectObject(
            'notify::position', this._onPositionChanged.bind(this),
            'destroy', () => {
                // First destroy the clone and then destroy everything
                // This will ensure that we never see it in the _disconnectSignals loop
                clone.destroy();
                this.destroy();
            }, this);
        this._onPositionChanged();

        this.connect('destroy', this._onDestroy.bind(this));

        this._draggable = DND.makeDraggable(this, {
            restoreOnSuccess: true,
            dragActorMaxSize: Workspace.WINDOW_DND_SIZE,
            dragActorOpacity: Workspace.DRAGGING_WINDOW_OPACITY,
        });
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

        realDialog.connectObject(
            'notify::position', dialog => this._updateDialogPosition(dialog, clone),
            'destroy', () => clone.destroy(), this);
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

    _onDestroy() {
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
    EXPANDING:      1,
    EXPANDED:       2,
    ANIMATING_IN:   3,
    NORMAL:         4,
    REMOVING:       5,
    ANIMATING_OUT:  6,
    ANIMATED_OUT:   7,
    COLLAPSING:     8,
    DESTROYED:      9,
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
    _init(metaWorkspace, monitorIndex) {
        super._init({
            clip_to_allocation: true,
            style_class: 'workspace-thumbnail',
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });
        this._delegate = this;

        this.metaWorkspace = metaWorkspace;
        this.monitorIndex = monitorIndex;

        this._removed = false;

        this._viewport = new Clutter.Actor();
        this.add_child(this._viewport);

        this._contents = new Clutter.Actor();
        this._viewport.add_child(this._contents);

        this.connect('destroy', this._onDestroy.bind(this));

        let workArea = Main.layoutManager.getWorkAreaForMonitor(this.monitorIndex);
        this.setPorthole(workArea.x, workArea.y, workArea.width, workArea.height);

        let windows = global.get_window_actors().filter(actor => {
            let win = actor.meta_window;
            return win.located_on_workspace(metaWorkspace);
        });

        // Create clones for windows that should be visible in the Overview
        this._windows = [];
        this._allWindows = [];
        for (let i = 0; i < windows.length; i++) {
            windows[i].meta_window.connectObject('notify::minimized',
                this._updateMinimized.bind(this), this);
            this._allWindows.push(windows[i].meta_window);

            if (this._isMyWindow(windows[i]) && this._isOverviewWindow(windows[i]))
                this._addWindowClone(windows[i]);
        }

        // Track window changes
        this.metaWorkspace.connectObject(
            'window-added', this._windowAdded.bind(this),
            'window-removed', this._windowRemoved.bind(this), this);
        global.display.connectObject(
            'window-entered-monitor', this._windowEnteredMonitor.bind(this),
            'window-left-monitor', this._windowLeftMonitor.bind(this), this);

        this.state = ThumbnailState.NORMAL;
        this._slidePosition = 0; // Fully slid in
        this._collapseFraction = 0; // Not collapsed
    }

    setPorthole(x, y, width, height) {
        this._viewport.set_size(width, height);
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

        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];
            const previousClone = this._windows[i - 1];
            clone.setStackAbove(previousClone);
        }
    }

    set slidePosition(slidePosition) {
        if (this._slidePosition == slidePosition)
            return;

        const scale = Util.lerp(1, 0.75, slidePosition);
        this.set_scale(scale, scale);
        this.opacity = Util.lerp(255, 0, slidePosition);

        this._slidePosition = slidePosition;
        this.notify('slide-position');
        this.queue_relayout();
    }

    get slidePosition() {
        return this._slidePosition;
    }

    set collapseFraction(collapseFraction) {
        if (this._collapseFraction == collapseFraction)
            return;
        this._collapseFraction = collapseFraction;
        this.notify('collapse-fraction');
        this.queue_relayout();
    }

    get collapseFraction() {
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
            metaWin.connectObject('notify::minimized',
                this._updateMinimized.bind(this), this);
            this._allWindows.push(metaWin);
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
            metaWin.disconnectObject(this);
            this._allWindows.splice(index, 1);
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

        this.metaWorkspace.disconnectObject(this);
        global.display.disconnectObject(this);
        this._allWindows.forEach(w => w.disconnectObject(this));
    }

    _onDestroy() {
        this.workspaceRemoved();
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

        if (this._windows.length > 0)
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
        if (this.metaWorkspace.active)
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

        if (source.metaWindow &&
            !this._isMyWindow(source.metaWindow.get_compositor_private()))
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

        if (source.metaWindow) {
            let win = source.metaWindow.get_compositor_private();
            if (this._isMyWindow(win))
                return false;

            let metaWindow = win.get_meta_window();
            Main.moveWindowToMonitorAndWorkspace(metaWindow,
                this.monitorIndex, this.metaWorkspace.index());
            return true;
        } else if (source.app && source.app.can_open_new_window()) {
            if (source.animateLaunchAtPos)
                source.animateLaunchAtPos(actor.x, actor.y);

            source.app.open_new_window(this.metaWorkspace.index());
            return true;
        } else if (!source.app && source.shellWorkspaceLaunch) {
            // While unused in our own drag sources, shellWorkspaceLaunch allows
            // extensions to define custom actions for their drag sources.
            source.shellWorkspaceLaunch({
                workspace: this.metaWorkspace.index(),
                timestamp: time,
            });
            return true;
        }

        return false;
    }

    setScale(scaleX, scaleY) {
        this._viewport.set_scale(scaleX, scaleY);
    }
});


var ThumbnailsBox = GObject.registerClass({
    Properties: {
        'expand-fraction': GObject.ParamSpec.double(
            'expand-fraction', 'expand-fraction', 'expand-fraction',
            GObject.ParamFlags.READWRITE,
            0, 1, 1),
        'scale': GObject.ParamSpec.double(
            'scale', 'scale', 'scale',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
        'should-show': GObject.ParamSpec.boolean(
            'should-show', 'should-show', 'should-show',
            GObject.ParamFlags.READABLE,
            true),
    },
}, class ThumbnailsBox extends St.Widget {
    _init(scrollAdjustment, monitorIndex) {
        super._init({
            style_class: 'workspace-thumbnails',
            reactive: true,
            x_align: Clutter.ActorAlign.CENTER,
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });

        this._delegate = this;

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator' });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._indicator = indicator;
        this.add_actor(indicator);

        this._monitorIndex = monitorIndex;

        this._dropWorkspace = -1;
        this._dropPlaceholderPos = -1;
        this._dropPlaceholder = new St.Bin({ style_class: 'placeholder' });
        this.add_actor(this._dropPlaceholder);
        this._spliceIndex = -1;

        this._targetScale = 0;
        this._scale = 0;
        this._expandFraction = 1;
        this._updateStateId = 0;
        this._pendingScaleUpdate = false;
        this._animatingIndicator = false;

        this._shouldShow = true;

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this._thumbnails = [];

        Main.overview.connectObject(
            'showing', () => this._createThumbnails(),
            'hidden', () => this._destroyThumbnails(),
            'item-drag-begin', () => this._onDragBegin(),
            'item-drag-end', () => this._onDragEnd(),
            'item-drag-cancelled', () => this._onDragCancelled(),
            'window-drag-begin', () => this._onDragBegin(),
            'window-drag-end', () => this._onDragEnd(),
            'window-drag-cancelled', () => this._onDragCancelled(), this);

        this._settings = new Gio.Settings({ schema_id: MUTTER_SCHEMA });
        this._settings.connect('changed::dynamic-workspaces',
            () => this._updateShouldShow());
        this._updateShouldShow();

        Main.layoutManager.connectObject('monitors-changed', () => {
            this._destroyThumbnails();
            if (Main.overview.visible)
                this._createThumbnails();
        }, this);

        // The porthole is the part of the screen we're showing in the thumbnails
        global.display.connectObject('workareas-changed',
            () => this._updatePorthole(), this);
        this._updatePorthole();

        this.connect('notify::visible', () => {
            if (!this.visible)
                this._queueUpdateStates();
        });
        this.connect('destroy', () => this._onDestroy());

        this._scrollAdjustment = scrollAdjustment;
        this._scrollAdjustment.connectObject('notify::value',
            () => this._updateIndicator(), this);
    }

    setMonitorIndex(monitorIndex) {
        this._monitorIndex = monitorIndex;
    }

    _onDestroy() {
        this._destroyThumbnails();
        this._unqueueUpdateStates();

        if (this._settings)
            this._settings.run_dispose();
        this._settings = null;
    }

    _updateShouldShow() {
        const { nWorkspaces } = global.workspace_manager;
        const shouldShow = this._settings.get_boolean('dynamic-workspaces')
            ? nWorkspaces > NUM_WORKSPACES_THRESHOLD
            : nWorkspaces > 1;

        if (this._shouldShow === shouldShow)
            return;

        this._shouldShow = shouldShow;
        this.notify('should-show');
    }

    _updateIndicator() {
        const { value } = this._scrollAdjustment;
        const { workspaceManager } = global;
        const activeIndex = workspaceManager.get_active_workspace_index();

        this._animatingIndicator = value !== activeIndex;

        if (!this._animatingIndicator)
            this._queueUpdateStates();

        this.queue_relayout();
    }

    _activateThumbnailAtPoint(stageX, stageY, time) {
        const [r_, x] = this.transform_stage_point(stageX, stageY);

        const thumbnail = this._thumbnails.find(t => x >= t.x && x <= t.x + t.width);
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

    _getPlaceholderTarget(index, spacing, rtl) {
        const workspace = this._thumbnails[index];

        let targetX1;
        let targetX2;

        if (rtl) {
            const baseX = workspace.x + workspace.width;
            targetX1 = baseX - WORKSPACE_CUT_SIZE;
            targetX2 = baseX + spacing + WORKSPACE_CUT_SIZE;
        } else {
            targetX1 = workspace.x - spacing - WORKSPACE_CUT_SIZE;
            targetX2 = workspace.x + WORKSPACE_CUT_SIZE;
        }

        if (index === 0) {
            if (rtl)
                targetX2 -= spacing + WORKSPACE_CUT_SIZE;
            else
                targetX1 += spacing + WORKSPACE_CUT_SIZE;
        }

        if (index === this._dropPlaceholderPos) {
            const placeholderWidth = this._dropPlaceholder.get_width() + spacing;
            if (rtl)
                targetX2 += placeholderWidth;
            else
                targetX1 -= placeholderWidth;
        }

        return [targetX1, targetX2];
    }

    _withinWorkspace(x, index, rtl) {
        const length = this._thumbnails.length;
        const workspace = this._thumbnails[index];

        let workspaceX1 = workspace.x + WORKSPACE_CUT_SIZE;
        let workspaceX2 = workspace.x + workspace.width - WORKSPACE_CUT_SIZE;

        if (index === length - 1) {
            if (rtl)
                workspaceX1 -= WORKSPACE_CUT_SIZE;
            else
                workspaceX2 += WORKSPACE_CUT_SIZE;
        }

        return x > workspaceX1 && x <= workspaceX2;
    }

    // Draggable target interface
    handleDragOver(source, actor, x, y, time) {
        if (!source.metaWindow &&
            (!source.app || !source.app.can_open_new_window()) &&
            (source.app || !source.shellWorkspaceLaunch) &&
            source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        const rtl = Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
        let canCreateWorkspaces = Meta.prefs_get_dynamic_workspaces();
        let spacing = this.get_theme_node().get_length('spacing');

        this._dropWorkspace = -1;
        let placeholderPos = -1;
        let length = this._thumbnails.length;
        for (let i = 0; i < length; i++) {
            const index = rtl ? length - i - 1 : i;

            if (canCreateWorkspaces && source !== Main.xdndHandler) {
                const [targetStart, targetEnd] =
                    this._getPlaceholderTarget(index, spacing, rtl);

                if (x > targetStart && x <= targetEnd) {
                    placeholderPos = index;
                    break;
                }
            }

            if (this._withinWorkspace(x, index, rtl)) {
                this._dropWorkspace = index;
                break;
            }
        }

        if (this._dropPlaceholderPos != placeholderPos) {
            this._dropPlaceholderPos = placeholderPos;
            this.queue_relayout();
        }

        if (this._dropWorkspace != -1)
            return this._thumbnails[this._dropWorkspace].handleDragOverInternal(source, actor, time);
        else if (this._dropPlaceholderPos != -1)
            return source.metaWindow ? DND.DragMotionResult.MOVE_DROP : DND.DragMotionResult.COPY_DROP;
        else
            return DND.DragMotionResult.CONTINUE;
    }

    acceptDrop(source, actor, x, y, time) {
        if (this._dropWorkspace != -1) {
            return this._thumbnails[this._dropWorkspace].acceptDropInternal(source, actor, time);
        } else if (this._dropPlaceholderPos != -1) {
            if (!source.metaWindow &&
                (!source.app || !source.app.can_open_new_window()) &&
                (source.app || !source.shellWorkspaceLaunch))
                return false;

            let isWindow = !!source.metaWindow;

            let newWorkspaceIndex;
            [newWorkspaceIndex, this._dropPlaceholderPos] = [this._dropPlaceholderPos, -1];
            this._spliceIndex = newWorkspaceIndex;

            Main.wm.insertWorkspace(newWorkspaceIndex);

            if (isWindow) {
                // Move the window to our monitor first if necessary.
                let thumbMonitor = this._thumbnails[newWorkspaceIndex].monitorIndex;
                Main.moveWindowToMonitorAndWorkspace(source.metaWindow,
                    thumbMonitor, newWorkspaceIndex, true);
            } else if (source.app && source.app.can_open_new_window()) {
                if (source.animateLaunchAtPos)
                    source.animateLaunchAtPos(actor.x, actor.y);

                source.app.open_new_window(newWorkspaceIndex);
            } else if (!source.app && source.shellWorkspaceLaunch) {
                // While unused in our own drag sources, shellWorkspaceLaunch allows
                // extensions to define custom actions for their drag sources.
                source.shellWorkspaceLaunch({
                    workspace: newWorkspaceIndex,
                    timestamp: time,
                });
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
            thumbnail.collapse_fraction = 1;

            this._queueUpdateStates();

            return true;
        } else {
            return false;
        }
    }

    _createThumbnails() {
        if (this._thumbnails.length > 0)
            return;

        const { workspaceManager } = global;
        this._transientSignalHolder = new TransientSignalHolder(this);
        workspaceManager.connectObject(
            'notify::n-workspaces', this._workspacesChanged.bind(this),
            'active-workspace-changed', () => this._updateIndicator(),
            'workspaces-reordered', () => {
                this._thumbnails.sort((a, b) => {
                    return a.metaWorkspace.index() - b.metaWorkspace.index();
                });
                this.queue_relayout();
            }, this._transientSignalHolder);
        Main.overview.connectObject('windows-restacked',
            this._syncStacking.bind(this), this._transientSignalHolder);

        this._targetScale = 0;
        this._scale = 0;
        this._pendingScaleUpdate = false;
        this._unqueueUpdateStates();

        this._stateCounts = {};
        for (let key in ThumbnailState)
            this._stateCounts[ThumbnailState[key]] = 0;

        this.addThumbnails(0, workspaceManager.n_workspaces);

        this._updateShouldShow();
    }

    _destroyThumbnails() {
        if (this._thumbnails.length == 0)
            return;

        this._transientSignalHolder.destroy();
        delete this._transientSignalHolder;

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

        this._updateShouldShow();
    }

    addThumbnails(start, count) {
        let workspaceManager = global.workspace_manager;

        for (let k = start; k < start + count; k++) {
            let metaWorkspace = workspaceManager.get_workspace_by_index(k);
            let thumbnail = new WorkspaceThumbnail(metaWorkspace, this._monitorIndex);
            thumbnail.setPorthole(this._porthole.x, this._porthole.y,
                                  this._porthole.width, this._porthole.height);
            this._thumbnails.push(thumbnail);
            this.add_actor(thumbnail);

            if (this._shouldShow && start > 0 && this._spliceIndex === -1) {
                // not the initial fill, and not splicing via DND
                thumbnail.state = ThumbnailState.NEW;
                thumbnail.slide_position = 1; // start slid out
                thumbnail.collapse_fraction = 1; // start fully collapsed
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
        this._updateStateId = 0;

        // If we are animating the indicator, wait
        if (this._animatingIndicator)
            return;

        // Likewise if we are in the process of hiding
        if (!this._shouldShow && this.visible)
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

        // Once that's complete, we can start scaling to the new size,
        // collapse any removed thumbnails and expand added ones
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

        this._iterateStateThumbnails(ThumbnailState.NEW, thumbnail => {
            this._setThumbnailState(thumbnail, ThumbnailState.EXPANDING);
            thumbnail.ease_property('collapse-fraction', 0, {
                duration: SLIDE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._setThumbnailState(thumbnail, ThumbnailState.EXPANDED);
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
        if (this._scale !== this._targetScale ||
            this._stateCounts[ThumbnailState.COLLAPSING] > 0 ||
            this._stateCounts[ThumbnailState.EXPANDING] > 0)
            return;

        // And then slide in any new thumbnails
        this._iterateStateThumbnails(ThumbnailState.EXPANDED, thumbnail => {
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
        if (this._updateStateId > 0)
            return;

        const laters = global.compositor.get_laters();
        this._updateStateId = laters.add(
            Meta.LaterType.BEFORE_REDRAW, () => this._updateStates());
    }

    _unqueueUpdateStates() {
        if (this._updateStateId) {
            const laters = global.compositor.get_laters();
            laters.remove(this._updateStateId);
        }
        this._updateStateId = 0;
    }

    vfunc_get_preferred_height(forWidth) {
        let themeNode = this.get_theme_node();

        forWidth = themeNode.adjust_for_width(forWidth);

        let spacing = themeNode.get_length('spacing');
        let nWorkspaces = this._thumbnails.length;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        const avail = forWidth - totalSpacing;

        let scale = (avail / nWorkspaces) / this._porthole.width;
        scale = Math.min(scale, MAX_THUMBNAIL_SCALE);

        const height = Math.round(this._porthole.height * scale);
        return themeNode.adjust_preferred_height(height, height);
    }

    vfunc_get_preferred_width(_forHeight) {
        // Note that for getPreferredHeight/Width we cheat a bit and skip propagating
        // the size request to our children because we know how big they are and know
        // that the actors aren't depending on the virtual functions being called.
        let themeNode = this.get_theme_node();

        let spacing = themeNode.get_length('spacing');
        let nWorkspaces = this._thumbnails.length;
        let totalSpacing = (nWorkspaces - 1) * spacing;

        const naturalWidth = this._thumbnails.reduce((accumulator, thumbnail, index) => {
            let workspaceSpacing = 0;

            if (index > 0)
                workspaceSpacing += spacing / 2;
            if (index < this._thumbnails.length - 1)
                workspaceSpacing += spacing / 2;

            const progress = 1 - thumbnail.collapse_fraction;
            const width = (this._porthole.width * MAX_THUMBNAIL_SCALE + workspaceSpacing) * progress;
            return accumulator + width;
        }, 0);

        return themeNode.adjust_preferred_width(totalSpacing, naturalWidth);
    }

    _updatePorthole() {
        if (!Main.layoutManager.monitors[this._monitorIndex]) {
            const { x, y, width, height } = global.stage;
            this._porthole = { x, y, width, height };
        } else {
            this._porthole =
                Main.layoutManager.getWorkAreaForMonitor(this._monitorIndex);
        }

        this.queue_relayout();
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        let rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;

        if (this._thumbnails.length == 0) // not visible
            return;

        let themeNode = this.get_theme_node();
        box = themeNode.get_content_box(box);

        const portholeWidth = this._porthole.width;
        const portholeHeight = this._porthole.height;
        const spacing = themeNode.get_length('spacing');

        const nWorkspaces = this._thumbnails.length;

        // Compute the scale we'll need once everything is updated,
        // unless we are currently transitioning
        if (this._expandFraction === 1) {
            const totalSpacing = (nWorkspaces - 1) * spacing;
            const availableWidth = (box.get_width() - totalSpacing) / nWorkspaces;

            const hScale = availableWidth / portholeWidth;
            const vScale = box.get_height() / portholeHeight;
            const newScale = Math.min(hScale, vScale);

            if (newScale !== this._targetScale) {
                if (this._targetScale > 0) {
                    // We don't ease immediately because we need to observe the
                    // ordering in queueUpdateStates - if workspaces have been
                    // removed we need to slide them out as the first thing.
                    this._targetScale = newScale;
                    this._pendingScaleUpdate = true;
                } else {
                    this._targetScale = this._scale = newScale;
                }

                this._queueUpdateStates();
            }
        }

        const ratio = portholeWidth / portholeHeight;
        const thumbnailFullHeight = Math.round(portholeHeight * this._scale);
        const thumbnailWidth = Math.round(thumbnailFullHeight * ratio);
        const thumbnailHeight = thumbnailFullHeight * this._expandFraction;
        const roundedVScale = thumbnailHeight / portholeHeight;

        // We always request size for MAX_THUMBNAIL_SCALE, distribute
        // space evently if we use smaller thumbnails
        const extraWidth =
            (MAX_THUMBNAIL_SCALE * portholeWidth - thumbnailWidth) * nWorkspaces;
        box.x1 += Math.round(extraWidth / 2);
        box.x2 -= Math.round(extraWidth / 2);

        let indicatorValue = this._scrollAdjustment.value;
        let indicatorUpperWs = Math.ceil(indicatorValue);
        let indicatorLowerWs = Math.floor(indicatorValue);

        let indicatorLowerX1 = 0;
        let indicatorLowerX2 = 0;
        let indicatorUpperX1 = 0;
        let indicatorUpperX2 = 0;

        let indicatorThemeNode = this._indicator.get_theme_node();
        let indicatorTopFullBorder = indicatorThemeNode.get_padding(St.Side.TOP) + indicatorThemeNode.get_border_width(St.Side.TOP);
        let indicatorBottomFullBorder = indicatorThemeNode.get_padding(St.Side.BOTTOM) + indicatorThemeNode.get_border_width(St.Side.BOTTOM);
        let indicatorLeftFullBorder = indicatorThemeNode.get_padding(St.Side.LEFT) + indicatorThemeNode.get_border_width(St.Side.LEFT);
        let indicatorRightFullBorder = indicatorThemeNode.get_padding(St.Side.RIGHT) + indicatorThemeNode.get_border_width(St.Side.RIGHT);

        let x = box.x1;

        if (this._dropPlaceholderPos == -1) {
            this._dropPlaceholder.allocate_preferred_size(
                ...this._dropPlaceholder.get_position());

            const laters = global.compositor.get_laters();
            laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._dropPlaceholder.hide();
            });
        }

        let childBox = new Clutter.ActorBox();

        for (let i = 0; i < this._thumbnails.length; i++) {
            const thumbnail = this._thumbnails[i];
            if (i > 0)
                x += spacing - Math.round(thumbnail.collapse_fraction * spacing);

            const y1 = box.y1;
            const y2 = y1 + thumbnailHeight;

            if (i === this._dropPlaceholderPos) {
                const [, placeholderWidth] = this._dropPlaceholder.get_preferred_width(-1);
                childBox.y1 = y1;
                childBox.y2 = y2;

                if (rtl) {
                    childBox.x2 = box.x2 - Math.round(x);
                    childBox.x1 = box.x2 - Math.round(x + placeholderWidth);
                } else {
                    childBox.x1 = Math.round(x);
                    childBox.x2 = Math.round(x + placeholderWidth);
                }

                this._dropPlaceholder.allocate(childBox);

                const laters = global.compositor.get_laters();
                laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                    this._dropPlaceholder.show();
                });
                x += placeholderWidth + spacing;
            }

            // We might end up with thumbnailWidth being something like 99.33
            // pixels. To make this work and not end up with a gap at the end,
            // we need some thumbnails to be 99 pixels and some 100 pixels width;
            // we compute an actual scale separately for each thumbnail.
            const x1 = Math.round(x);
            const x2 = Math.round(x + thumbnailWidth);
            const roundedHScale = (x2 - x1) / portholeWidth;

            // Allocating a scaled actor is funny - x1/y1 correspond to the origin
            // of the actor, but x2/y2 are increased by the *unscaled* size.
            if (rtl) {
                childBox.x2 = box.x2 - x1;
                childBox.x1 = box.x2 - (x1 + thumbnailWidth);
            } else {
                childBox.x1 = x1;
                childBox.x2 = x1 + thumbnailWidth;
            }
            childBox.y1 = y1;
            childBox.y2 = y1 + thumbnailHeight;

            thumbnail.setScale(roundedHScale, roundedVScale);
            thumbnail.allocate(childBox);

            if (i === indicatorUpperWs) {
                indicatorUpperX1 = childBox.x1;
                indicatorUpperX2 = childBox.x2;
            }
            if (i === indicatorLowerWs) {
                indicatorLowerX1 = childBox.x1;
                indicatorLowerX2 = childBox.x2;
            }

            // We round the collapsing portion so that we don't get thumbnails resizing
            // during an animation due to differences in rounded, but leave the uncollapsed
            // portion unrounded so that non-animating we end up with the right total
            x += thumbnailWidth - Math.round(thumbnailWidth * thumbnail.collapse_fraction);
        }

        childBox.y1 = box.y1;
        childBox.y2 = box.y1 + thumbnailHeight;

        const indicatorX1 = indicatorLowerX1 +
            (indicatorUpperX1 - indicatorLowerX1) * (indicatorValue % 1);
        const indicatorX2 = indicatorLowerX2 +
            (indicatorUpperX2 - indicatorLowerX2) * (indicatorValue % 1);

        childBox.x1 = indicatorX1 - indicatorLeftFullBorder;
        childBox.x2 = indicatorX2 + indicatorRightFullBorder;
        childBox.y1 -= indicatorTopFullBorder;
        childBox.y2 += indicatorBottomFullBorder;
        this._indicator.allocate(childBox);
    }

    get shouldShow() {
        return this._shouldShow;
    }

    set expandFraction(expandFraction) {
        if (this._expandFraction === expandFraction)
            return;
        this._expandFraction = expandFraction;
        this.notify('expand-fraction');
        this.queue_relayout();
    }

    get expandFraction() {
        return this._expandFraction;
    }
});
