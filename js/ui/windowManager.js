// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const AltTab = imports.ui.altTab;
const Dialog = imports.ui.dialog;
const WorkspaceSwitcherPopup = imports.ui.workspaceSwitcherPopup;
const InhibitShortcutsDialog = imports.ui.inhibitShortcutsDialog;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const Tweener = imports.ui.tweener;
const WindowMenu = imports.ui.windowMenu;
const PadOsd = imports.ui.padOsd;
const EdgeDragAction = imports.ui.edgeDragAction;
const CloseDialog = imports.ui.closeDialog;
const SwitchMonitor = imports.ui.switchMonitor;

var SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';
var MINIMIZE_WINDOW_ANIMATION_TIME = 0.2;
var SHOW_WINDOW_ANIMATION_TIME = 0.15;
var DIALOG_SHOW_WINDOW_ANIMATION_TIME = 0.1;
var DESTROY_WINDOW_ANIMATION_TIME = 0.15;
var DIALOG_DESTROY_WINDOW_ANIMATION_TIME = 0.1;
var WINDOW_ANIMATION_TIME = 0.25;
var DIM_BRIGHTNESS = -0.3;
var DIM_TIME = 0.500;
var UNDIM_TIME = 0.250;

var ONE_SECOND = 1000; // in ms

const GSD_WACOM_BUS_NAME = 'org.gnome.SettingsDaemon.Wacom';
const GSD_WACOM_OBJECT_PATH = '/org/gnome/SettingsDaemon/Wacom';

const GsdWacomIface = '<node name="/org/gnome/SettingsDaemon/Wacom"> \
<interface name="org.gnome.SettingsDaemon.Wacom"> \
  <method name="SetGroupModeLED"> \
    <arg name="device_path" direction="in" type="s"/> \
    <arg name="group" direction="in" type="u"/> \
    <arg name="mode" direction="in" type="u"/> \
  </method> \
  <method name="SetOLEDLabels"> \
    <arg name="device_path" direction="in" type="s"/> \
    <arg name="labels" direction="in" type="as"/> \
  </method> \
  </interface> \
</node>';

const GsdWacomProxy = Gio.DBusProxy.makeProxyWrapper(GsdWacomIface);

var DisplayChangeDialog = new Lang.Class({
    Name: 'DisplayChangeDialog',
    Extends: ModalDialog.ModalDialog,

    _init(wm) {
        this.parent({ styleClass: 'prompt-dialog' });

        this._wm = wm;

        this._countDown = Meta.MonitorManager.get_display_configuration_timeout();

        let iconName = 'preferences-desktop-display-symbolic';
        let icon = new Gio.ThemedIcon({ name: iconName });
        let title = _("Do you want to keep these display settings?");
        let body = this._formatCountDown();

        this._content = new Dialog.MessageDialogContent({ icon, title, body });

        this.contentLayout.add(this._content,
                               { x_fill: true,
                                 y_fill: true });

        /* Translators: this and the following message should be limited in lenght,
           to avoid ellipsizing the labels.
        */
        this._cancelButton = this.addButton({ label: _("Revert Settings"),
                                              action: this._onFailure.bind(this),
                                              key: Clutter.Escape });
        this._okButton = this.addButton({ label:  _("Keep Changes"),
                                          action: this._onSuccess.bind(this),
                                          default: true });

        this._timeoutId = Mainloop.timeout_add(ONE_SECOND, this._tick.bind(this));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._tick');
    },

    close(timestamp) {
        if (this._timeoutId > 0) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        this.parent(timestamp);
    },

    _formatCountDown() {
        let fmt = ngettext("Settings changes will revert in %d second",
                           "Settings changes will revert in %d seconds");
        return fmt.format(this._countDown);
    },

    _tick() {
        this._countDown--;

        if (this._countDown == 0) {
            /* mutter already takes care of failing at timeout */
            this._timeoutId = 0;
            this.close();
            return GLib.SOURCE_REMOVE;
        }

        this._content.body = this._formatCountDown();
        return GLib.SOURCE_CONTINUE;
    },

    _onFailure() {
        this._wm.complete_display_change(false);
        this.close();
    },

    _onSuccess() {
        this._wm.complete_display_change(true);
        this.close();
    },
});

var WindowDimmer = new Lang.Class({
    Name: 'WindowDimmer',

    _init(actor) {
        this._brightnessEffect = new Clutter.BrightnessContrastEffect();
        actor.add_effect(this._brightnessEffect);
        this.actor = actor;
        this._enabled = true;
        this._dimFactor = 0.0;
        this._syncEnabled();
    },

    _syncEnabled() {
        this._brightnessEffect.enabled = (this._enabled && this._dimFactor > 0);
    },

    setEnabled(enabled) {
        this._enabled = enabled;
        this._syncEnabled();
    },

    set dimFactor(factor) {
        this._dimFactor = factor;
        this._brightnessEffect.set_brightness(factor * DIM_BRIGHTNESS);
        this._syncEnabled();
    },

    get dimFactor() {
        return this._dimFactor;
    }
});

function getWindowDimmer(actor) {
    let enabled = Meta.prefs_get_attach_modal_dialogs();
    if (actor._windowDimmer)
        actor._windowDimmer.setEnabled(enabled);

    if (enabled) {
        if (!actor._windowDimmer)
            actor._windowDimmer = new WindowDimmer(actor);
        return actor._windowDimmer;
    } else {
        return null;
    }
}

/*
 * When the last window closed on a workspace is a dialog or splash
 * screen, we assume that it might be an initial window shown before
 * the main window of an application, and give the app a grace period
 * where it can map another window before we remove the workspace.
 */
var LAST_WINDOW_GRACE_TIME = 1000;

var WorkspaceTracker = new Lang.Class({
    Name: 'WorkspaceTracker',

    _init(wm) {
        this._wm = wm;

        this._workspaces = [];
        this._checkWorkspacesId = 0;

        this._pauseWorkspaceCheck = false;

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('startup-sequence-changed', this._queueCheckWorkspaces.bind(this));

        global.screen.connect('notify::n-workspaces', this._nWorkspacesChanged.bind(this));
        global.window_manager.connect('switch-workspace', this._queueCheckWorkspaces.bind(this));

        global.screen.connect('window-entered-monitor', this._windowEnteredMonitor.bind(this));
        global.screen.connect('window-left-monitor', this._windowLeftMonitor.bind(this));
        global.screen.connect('restacked', this._windowsRestacked.bind(this));

        this._workspaceSettings = this._getWorkspaceSettings();
        this._workspaceSettings.connect('changed::dynamic-workspaces', this._queueCheckWorkspaces.bind(this));

        this._nWorkspacesChanged();
    },

    _getWorkspaceSettings() {
        let settings = global.get_overrides_settings();
        if (settings &&
            settings.settings_schema.list_keys().indexOf('dynamic-workspaces') > -1)
            return settings;
        return new Gio.Settings({ schema_id: 'org.gnome.mutter' });
    },

    blockUpdates() {
        this._pauseWorkspaceCheck = true;
    },

    unblockUpdates() {
        this._pauseWorkspaceCheck = false;
    },

    _checkWorkspaces() {
        let i;
        let emptyWorkspaces = [];

        if (!Meta.prefs_get_dynamic_workspaces()) {
            this._checkWorkspacesId = 0;
            return false;
        }

        // Update workspaces only if Dynamic Workspace Management has not been paused by some other function
        if (this._pauseWorkspaceCheck)
            return true;

        for (i = 0; i < this._workspaces.length; i++) {
            let lastRemoved = this._workspaces[i]._lastRemovedWindow;
            if ((lastRemoved &&
                 (lastRemoved.get_window_type() == Meta.WindowType.SPLASHSCREEN ||
                  lastRemoved.get_window_type() == Meta.WindowType.DIALOG ||
                  lastRemoved.get_window_type() == Meta.WindowType.MODAL_DIALOG)) ||
                this._workspaces[i]._keepAliveId)
                emptyWorkspaces[i] = false;
            else
                emptyWorkspaces[i] = true;
        }

        let sequences = Shell.WindowTracker.get_default().get_startup_sequences();
        for (i = 0; i < sequences.length; i++) {
            let index = sequences[i].get_workspace();
            if (index >= 0 && index <= global.screen.n_workspaces)
                emptyWorkspaces[index] = false;
        }

        let windows = global.get_window_actors();
        for (i = 0; i < windows.length; i++) {
            let actor = windows[i];
            let win = actor.get_meta_window();

            if (win.is_on_all_workspaces())
                continue;

            let workspaceIndex = win.get_workspace().index();
            emptyWorkspaces[workspaceIndex] = false;
        }

        // If we don't have an empty workspace at the end, add one
        if (!emptyWorkspaces[emptyWorkspaces.length -1]) {
            global.screen.append_new_workspace(false, global.get_current_time());
            emptyWorkspaces.push(false);
        }

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        emptyWorkspaces[activeWorkspaceIndex] = false;

        // Delete other empty workspaces; do it from the end to avoid index changes
        for (i = emptyWorkspaces.length - 2; i >= 0; i--) {
            if (emptyWorkspaces[i])
                global.screen.remove_workspace(this._workspaces[i], global.get_current_time());
        }

        this._checkWorkspacesId = 0;
        return false;
    },

    keepWorkspaceAlive(workspace, duration) {
        if (workspace._keepAliveId)
            Mainloop.source_remove(workspace._keepAliveId);

        workspace._keepAliveId = Mainloop.timeout_add(duration, () => {
            workspace._keepAliveId = 0;
            this._queueCheckWorkspaces();
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(workspace._keepAliveId, '[gnome-shell] this._queueCheckWorkspaces');
    },

    _windowRemoved(workspace, window) {
        workspace._lastRemovedWindow = window;
        this._queueCheckWorkspaces();
        let id = Mainloop.timeout_add(LAST_WINDOW_GRACE_TIME, () => {
            if (workspace._lastRemovedWindow == window) {
                workspace._lastRemovedWindow = null;
                this._queueCheckWorkspaces();
            }
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(id, '[gnome-shell] this._queueCheckWorkspaces');
    },

    _windowLeftMonitor(metaScreen, monitorIndex, metaWin) {
        // If the window left the primary monitor, that
        // might make that workspace empty
        if (monitorIndex == Main.layoutManager.primaryIndex)
            this._queueCheckWorkspaces();
    },

    _windowEnteredMonitor(metaScreen, monitorIndex, metaWin) {
        // If the window entered the primary monitor, that
        // might make that workspace non-empty
        if (monitorIndex == Main.layoutManager.primaryIndex)
            this._queueCheckWorkspaces();
    },

    _windowsRestacked() {
        // Figure out where the pointer is in case we lost track of
        // it during a grab. (In particular, if a trayicon popup menu
        // is dismissed, see if we need to close the message tray.)
        global.sync_pointer();
    },

    _queueCheckWorkspaces() {
        if (this._checkWorkspacesId == 0)
            this._checkWorkspacesId = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, this._checkWorkspaces.bind(this));
    },

    _nWorkspacesChanged() {
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return false;

        let lostWorkspaces = [];
        if (newNumWorkspaces > oldNumWorkspaces) {
            let w;

            // Assume workspaces are only added at the end
            for (w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._workspaces[w] = global.screen.get_workspace_by_index(w);

            for (w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                let workspace = this._workspaces[w];
                workspace._windowAddedId = workspace.connect('window-added', this._queueCheckWorkspaces.bind(this));
                workspace._windowRemovedId = workspace.connect('window-removed', this._windowRemoved.bind(this));
            }

        } else {
            // Assume workspaces are only removed sequentially
            // (e.g. 2,3,4 - not 2,4,7)
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let workspace = global.screen.get_workspace_by_index(w);
                if (this._workspaces[w] != workspace) {
                    removedIndex = w;
                    break;
                }
            }

            let lostWorkspaces = this._workspaces.splice(removedIndex, removedNum);
            lostWorkspaces.forEach(workspace => {
                workspace.disconnect(workspace._windowAddedId);
                workspace.disconnect(workspace._windowRemovedId);
            });
        }

        this._queueCheckWorkspaces();

        return false;
    }
});

var TilePreview = new Lang.Class({
    Name: 'TilePreview',

    _init() {
        this.actor = new St.Widget();
        global.window_group.add_actor(this.actor);

        this._reset();
        this._showing = false;
    },

    show(window, tileRect, monitorIndex) {
        let windowActor = window.get_compositor_private();
        if (!windowActor)
            return;

        global.window_group.set_child_below_sibling(this.actor, windowActor);

        if (this._rect && this._rect.equal(tileRect))
            return;

        let changeMonitor = (this._monitorIndex == -1 ||
                             this._monitorIndex != monitorIndex);

        this._monitorIndex = monitorIndex;
        this._rect = tileRect;

        let monitor = Main.layoutManager.monitors[monitorIndex];

        this._updateStyle(monitor);

        if (!this._showing || changeMonitor) {
            let monitorRect = new Meta.Rectangle({ x: monitor.x,
                                                   y: monitor.y,
                                                   width: monitor.width,
                                                   height: monitor.height });
            let [, rect] = window.get_frame_rect().intersect(monitorRect);
            this.actor.set_size(rect.width, rect.height);
            this.actor.set_position(rect.x, rect.y);
            this.actor.opacity = 0;
        }

        this._showing = true;
        this.actor.show();
        Tweener.addTween(this.actor,
                         { x: tileRect.x,
                           y: tileRect.y,
                           width: tileRect.width,
                           height: tileRect.height,
                           opacity: 255,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
    },

    hide() {
        if (!this._showing)
            return;

        this._showing = false;
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._reset.bind(this)
                         });
    },

    _reset() {
        this.actor.hide();
        this._rect = null;
        this._monitorIndex = -1;
    },

    _updateStyle(monitor) {
        let styles = ['tile-preview'];
        if (this._monitorIndex == Main.layoutManager.primaryIndex)
            styles.push('on-primary');
        if (this._rect.x == monitor.x)
            styles.push('tile-preview-left');
        if (this._rect.x + this._rect.width == monitor.x + monitor.width)
            styles.push('tile-preview-right');

        this.actor.style_class = styles.join(' ');
    }
});

var TouchpadWorkspaceSwitchAction = new Lang.Class({
    Name: 'TouchpadWorkspaceSwitchAction',

    _init(actor) {
        this._dx = 0;
        this._dy = 0;
        actor.connect('captured-event', this._handleEvent.bind(this));
    },

    _checkActivated() {
        const MOTION_THRESHOLD = 50;
        let allowedModes = Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW;
        let dir;

        if ((allowedModes & Main.actionMode) == 0)
            return;

        if (this._dy < -MOTION_THRESHOLD)
            dir = Meta.MotionDirection.DOWN;
        else if (this._dy > MOTION_THRESHOLD)
            dir = Meta.MotionDirection.UP;
        else if (this._dx < -MOTION_THRESHOLD)
            dir = Meta.MotionDirection.RIGHT;
        else if (this._dx > MOTION_THRESHOLD)
            dir = Meta.MotionDirection.LEFT;
        else
            return;

        this.emit('activated', dir);
    },

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 4)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.UPDATE) {
            let [dx, dy] = event.get_gesture_motion_delta();

            this._dx += dx;
            this._dy += dy;
        } else {
            if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END)
                this._checkActivated();

            this._dx = 0;
            this._dy = 0;
        }

        return Clutter.EVENT_STOP;
    }
});
Signals.addSignalMethods(TouchpadWorkspaceSwitchAction.prototype);

var WorkspaceSwitchAction = new Lang.Class({
    Name: 'WorkspaceSwitchAction',
    Extends: Clutter.SwipeAction,
    Signals: { 'activated': { param_types: [Meta.MotionDirection.$gtype] } },

    _init() {
        const MOTION_THRESHOLD = 50;

        this.parent();
        this.set_n_touch_points(4);
        this.set_threshold_trigger_distance(MOTION_THRESHOLD, MOTION_THRESHOLD);

        global.display.connect('grab-op-begin', () => {
            this.cancel();
        });
    },

    vfunc_gesture_prepare(actor) {
        let allowedModes = Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW;

        if (!this.parent(actor))
            return false;

        return (allowedModes & Main.actionMode);
    },

    vfunc_swept(actor, direction) {
        let dir;

        if (direction & Clutter.SwipeDirection.UP)
            dir = Meta.MotionDirection.DOWN;
        else if (direction & Clutter.SwipeDirection.DOWN)
            dir = Meta.MotionDirection.UP;
        else if (direction & Clutter.SwipeDirection.LEFT)
            dir = Meta.MotionDirection.RIGHT;
        else if (direction & Clutter.SwipeDirection.RIGHT)
            dir = Meta.MotionDirection.LEFT;

        this.emit('activated', dir);
    }
});

var AppSwitchAction = new Lang.Class({
    Name: 'AppSwitchAction',
    Extends: Clutter.GestureAction,
    Signals: { 'activated': {} },

    _init() {
        this.parent();
        this.set_n_touch_points(3);

        global.display.connect('grab-op-begin', () => {
            this.cancel();
        });
    },

    vfunc_gesture_prepare(action, actor) {
        if (Main.actionMode != Shell.ActionMode.NORMAL) {
            this.cancel();
            return false;
        }

        return this.get_n_current_points() <= 4;
    },

    vfunc_gesture_begin(action, actor) {
        // in milliseconds
        const LONG_PRESS_TIMEOUT = 250;

        let nPoints = this.get_n_current_points();
        let event = this.get_last_event (nPoints - 1);

        if (nPoints == 3)
            this._longPressStartTime = event.get_time();
        else if (nPoints == 4) {
            // Check whether the 4th finger press happens after a 3-finger long press,
            // this only needs to be checked on the first 4th finger press
            if (this._longPressStartTime != null &&
                event.get_time() < this._longPressStartTime + LONG_PRESS_TIMEOUT)
                this.cancel();
            else {
                this._longPressStartTime = null;
                this.emit('activated');
            }
        }

        return this.get_n_current_points() <= 4;
    },

    vfunc_gesture_progress(action, actor) {
        const MOTION_THRESHOLD = 30;

        if (this.get_n_current_points() == 3) {
            for (let i = 0; i < this.get_n_current_points(); i++) {
                let [startX, startY] = this.get_press_coords(i);
                let [x, y] = this.get_motion_coords(i);

                if (Math.abs(x - startX) > MOTION_THRESHOLD ||
                    Math.abs(y - startY) > MOTION_THRESHOLD)
                    return false;
            }

        }

        return true;
    }
});

var ResizePopup = new Lang.Class({
    Name: 'ResizePopup',

    _init() {
        this._widget = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._label = new St.Label({ style_class: 'resize-popup',
                                     x_align: Clutter.ActorAlign.CENTER,
                                     y_align: Clutter.ActorAlign.CENTER,
                                     x_expand: true, y_expand: true });
        this._widget.add_child(this._label);
        Main.uiGroup.add_actor(this._widget);
    },

    set(rect, displayW, displayH) {
        /* Translators: This represents the size of a window. The first number is
         * the width of the window and the second is the height. */
        let text = _("%d × %d").format(displayW, displayH);
        this._label.set_text(text);

        this._widget.set_position(rect.x, rect.y);
        this._widget.set_size(rect.width, rect.height);
    },

    destroy() {
        this._widget.destroy();
        this._widget = null;
    },
});

var WindowManager = new Lang.Class({
    Name: 'WindowManager',

    _init() {
        this._shellwm =  global.window_manager;

        this._minimizing = [];
        this._unminimizing = [];
        this._mapping = [];
        this._resizing = [];
        this._destroying = [];
        this._movingWindow = null;

        this._dimmedWindows = [];

        this._skippedActors = [];

        this._allowedKeybindings = {};

        this._isWorkspacePrepended = false;

        this._switchData = null;
        this._shellwm.connect('kill-switch-workspace', this._switchWorkspaceDone.bind(this));
        this._shellwm.connect('kill-window-effects', (shellwm, actor) => {
            this._minimizeWindowDone(shellwm, actor);
            this._mapWindowDone(shellwm, actor);
            this._destroyWindowDone(shellwm, actor);
            this._sizeChangeWindowDone(shellwm, actor);
        });

        this._shellwm.connect('switch-workspace', this._switchWorkspace.bind(this));
        this._shellwm.connect('show-tile-preview', this._showTilePreview.bind(this));
        this._shellwm.connect('hide-tile-preview', this._hideTilePreview.bind(this));
        this._shellwm.connect('show-window-menu', this._showWindowMenu.bind(this));
        this._shellwm.connect('minimize', this._minimizeWindow.bind(this));
        this._shellwm.connect('unminimize', this._unminimizeWindow.bind(this));
        this._shellwm.connect('size-change', this._sizeChangeWindow.bind(this));
        this._shellwm.connect('size-changed', this._sizeChangedWindow.bind(this));
        this._shellwm.connect('map', this._mapWindow.bind(this));
        this._shellwm.connect('destroy', this._destroyWindow.bind(this));
        this._shellwm.connect('filter-keybinding', this._filterKeybinding.bind(this));
        this._shellwm.connect('confirm-display-change', this._confirmDisplayChange.bind(this));
        this._shellwm.connect('create-close-dialog', this._createCloseDialog.bind(this));
        this._shellwm.connect('create-inhibit-shortcuts-dialog', this._createInhibitShortcutsDialog.bind(this));
        global.screen.connect('restacked', this._syncStacking.bind(this));

        this._workspaceSwitcherPopup = null;
        this._tilePreview = null;

        this.allowKeybinding('switch-to-session-1', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-2', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-3', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-4', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-5', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-6', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-7', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-8', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-9', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-10', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-11', Shell.ActionMode.ALL);
        this.allowKeybinding('switch-to-session-12', Shell.ActionMode.ALL);

        this.setCustomKeybindingHandler('switch-to-workspace-left',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-right',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-up',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-down',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-last',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-left',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-right',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-up',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-down',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-1',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-2',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-3',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-4',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-5',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-6',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-7',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-8',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-9',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-10',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-11',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-to-workspace-12',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-1',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-2',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-3',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-4',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-5',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-6',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-7',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-8',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-9',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-10',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-11',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-12',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('move-to-workspace-last',
                                        Shell.ActionMode.NORMAL,
                                        this._showWorkspaceSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-applications',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-group',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-applications-backward',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-group-backward',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-windows',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-windows-backward',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('cycle-windows',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('cycle-windows-backward',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('cycle-group',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('cycle-group-backward',
                                        Shell.ActionMode.NORMAL,
                                        this._startSwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-panels',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW |
                                        Shell.ActionMode.LOCK_SCREEN |
                                        Shell.ActionMode.UNLOCK_SCREEN |
                                        Shell.ActionMode.LOGIN_SCREEN,
                                        this._startA11ySwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-panels-backward',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW |
                                        Shell.ActionMode.LOCK_SCREEN |
                                        Shell.ActionMode.UNLOCK_SCREEN |
                                        Shell.ActionMode.LOGIN_SCREEN,
                                        this._startA11ySwitcher.bind(this));
        this.setCustomKeybindingHandler('switch-monitor',
                                        Shell.ActionMode.NORMAL |
                                        Shell.ActionMode.OVERVIEW,
                                        this._startSwitcher.bind(this));

        this.addKeybinding('pause-resume-tweens',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.NONE,
                           Shell.ActionMode.ALL,
                           this._toggleTweens.bind(this));

        this.addKeybinding('open-application-menu',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.NONE,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.POPUP,
                           this._toggleAppMenu.bind(this));

        this.addKeybinding('toggle-message-tray',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.NONE,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW |
                           Shell.ActionMode.POPUP,
                           this._toggleCalendar.bind(this));

        global.display.connect('show-resize-popup', this._showResizePopup.bind(this));
        global.display.connect('show-pad-osd', this._showPadOsd.bind(this));
        global.display.connect('show-osd', (display, monitorIndex, iconName, label) => {
            let icon = Gio.Icon.new_for_string(iconName);
            Main.osdWindowManager.show(monitorIndex, icon, label, null);
        });

        this._gsdWacomProxy = new GsdWacomProxy(Gio.DBus.session, GSD_WACOM_BUS_NAME,
                                                GSD_WACOM_OBJECT_PATH,
                                                (proxy, error) => {
                                                    if (error) {
                                                        log(error.message);
                                                        return;
                                                    }
                                                });

        global.display.connect('pad-mode-switch', (display, pad, group, mode) => {
            let labels = [];

            //FIXME: Fix num buttons
            for (let i = 0; i < 50; i++) {
                let str = display.get_pad_action_label(pad, Meta.PadActionType.BUTTON, i);
                labels.push(str ? str: '');
            }

            if (this._gsdWacomProxy) {
                this._gsdWacomProxy.SetOLEDLabelsRemote(pad.get_device_node(), labels);
                this._gsdWacomProxy.SetGroupModeLEDRemote(pad.get_device_node(), group, mode);
            }
        });

        Main.overview.connect('showing', () => {
            for (let i = 0; i < this._dimmedWindows.length; i++)
                this._undimWindow(this._dimmedWindows[i]);
        });
        Main.overview.connect('hiding', () => {
            for (let i = 0; i < this._dimmedWindows.length; i++)
                this._dimWindow(this._dimmedWindows[i]);
        });

        this._windowMenuManager = new WindowMenu.WindowMenuManager();

        if (Main.sessionMode.hasWorkspaces)
            this._workspaceTracker = new WorkspaceTracker(this);

        global.screen.override_workspace_layout(Meta.ScreenCorner.TOPLEFT,
                                                false, -1, 1);

        let gesture = new WorkspaceSwitchAction();
        gesture.connect('activated', this._actionSwitchWorkspace.bind(this));
        global.stage.add_action(gesture);

        // This is not a normal Clutter.GestureAction, doesn't need add_action()
        gesture = new TouchpadWorkspaceSwitchAction(global.stage);
        gesture.connect('activated', this._actionSwitchWorkspace.bind(this));

        gesture = new AppSwitchAction();
        gesture.connect('activated', this._switchApp.bind(this));
        global.stage.add_action(gesture);

        let mode = Shell.ActionMode.ALL & ~Shell.ActionMode.LOCK_SCREEN;
        gesture = new EdgeDragAction.EdgeDragAction(St.Side.BOTTOM, mode);
        gesture.connect('activated', () => {
            Main.keyboard.show(Main.layoutManager.bottomIndex);
        });
        global.stage.add_action(gesture);
    },

    _showPadOsd(display, device, settings, imagePath, editionMode, monitorIndex) {
        this._currentPadOsd = new PadOsd.PadOsd(device, settings, imagePath, editionMode, monitorIndex);
        this._currentPadOsd.connect('closed', () => { this._currentPadOsd = null });

        return this._currentPadOsd.actor;
    },

    _actionSwitchWorkspace(action, direction) {
            let newWs = global.screen.get_active_workspace().get_neighbor(direction);
            this.actionMoveWorkspace(newWs);
    },

    _lookupIndex(windows, metaWindow) {
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].metaWindow == metaWindow) {
                return i;
            }
        }
        return -1;
    },

    _switchApp() {
        let windows = global.get_window_actors().filter(actor => {
            let win = actor.metaWindow;
            return (!win.is_override_redirect() &&
                    win.located_on_workspace(global.screen.get_active_workspace()));
        });

        if (windows.length == 0)
            return;

        let focusWindow = global.display.focus_window;
        let nextWindow;

        if (focusWindow == null)
            nextWindow = windows[0].metaWindow;
        else {
            let index = this._lookupIndex (windows, focusWindow) + 1;

            if (index >= windows.length)
                index = 0;

            nextWindow = windows[index].metaWindow;
        }

        Main.activateWindow(nextWindow);
    },

    insertWorkspace(pos) {
        if (!Meta.prefs_get_dynamic_workspaces())
            return;

        global.screen.append_new_workspace(false, global.get_current_time());

        let windows = global.get_window_actors().map(a => a.meta_window);

        // To create a new workspace, we slide all the windows on workspaces
        // below us to the next workspace, leaving a blank workspace for us
        // to recycle.
        windows.forEach(window => {
            // If the window is attached to an ancestor, we don't need/want
            // to move it
            if (window.get_transient_for() != null)
                return;
            // Same for OR windows
            if (window.is_override_redirect())
                return;
            // Windows on workspaces below pos don't need moving
            let index = window.get_workspace().index();
            if (index < pos)
                return;
            window.change_workspace_by_index(index + 1, true);
        });

        // If the new workspace was inserted before the active workspace,
        // activate the workspace to which its windows went
        let activeIndex = global.screen.get_active_workspace_index();
        if (activeIndex >= pos) {
            let newWs = global.screen.get_workspace_by_index(activeIndex + 1);
            this._blockAnimations = true;
            newWs.activate(global.get_current_time());
            this._blockAnimations = false;
        }
    },


    keepWorkspaceAlive(workspace, duration) {
        if (!this._workspaceTracker)
            return;

        this._workspaceTracker.keepWorkspaceAlive(workspace, duration);
    },

    skipNextEffect(actor) {
        this._skippedActors.push(actor);
    },

    setCustomKeybindingHandler(name, modes, handler) {
        if (Meta.keybindings_set_custom_handler(name, handler))
            this.allowKeybinding(name, modes);
    },

    addKeybinding(name, settings, flags, modes, handler) {
        let action = global.display.add_keybinding(name, settings, flags, handler);
        if (action != Meta.KeyBindingAction.NONE)
            this.allowKeybinding(name, modes);
        return action;
    },

    removeKeybinding(name) {
        if (global.display.remove_keybinding(name))
            this.allowKeybinding(name, Shell.ActionMode.NONE);
    },

    allowKeybinding(name, modes) {
        this._allowedKeybindings[name] = modes;
    },

    _shouldAnimate() {
        return !(Main.overview.visible || this._blockAnimations);
    },

    _shouldAnimateActor(actor, types) {
        if (this._removeEffect(this._skippedActors, actor))
            return false;

        if (!this._shouldAnimate())
            return false;

        if (!actor.get_texture())
            return false;

        let type = actor.meta_window.get_window_type();
        return types.indexOf(type) >= 0;
    },

    _removeEffect(list, actor) {
        let idx = list.indexOf(actor);
        if (idx != -1) {
            list.splice(idx, 1);
            return true;
        }
        return false;
    },

    _minimizeWindow(shellwm, actor) {
        let types = [Meta.WindowType.NORMAL,
                     Meta.WindowType.MODAL_DIALOG,
                     Meta.WindowType.DIALOG];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_minimize(actor);
            return;
        }

        actor.set_scale(1.0, 1.0);

        this._minimizing.push(actor);

        if (actor.meta_window.is_monitor_sized()) {
            Tweener.addTween(actor,
                         { opacity: 0,
                           time: MINIMIZE_WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._minimizeWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [shellwm, actor],
                           onOverwrite: this._minimizeWindowOverwritten,
                           onOverwriteScope: this,
                           onOverwriteParams: [shellwm, actor]
                         });
        } else {
            let xDest, yDest, xScale, yScale;
            let [success, geom] = actor.meta_window.get_icon_geometry();
            if (success) {
                xDest = geom.x;
                yDest = geom.y;
                xScale = geom.width / actor.width;
                yScale = geom.height / actor.height;
            } else {
                let monitor = Main.layoutManager.monitors[actor.meta_window.get_monitor()];
                if (!monitor) {
                    this._minimizeWindowDone();
                    return;
                }
                xDest = monitor.x;
                yDest = monitor.y;
                if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
                    xDest += monitor.width;
                xScale = 0;
                yScale = 0;
            }

            Tweener.addTween(actor,
                             { scale_x: xScale,
                               scale_y: yScale,
                               x: xDest,
                               y: yDest,
                               time: MINIMIZE_WINDOW_ANIMATION_TIME,
                               transition: 'easeInExpo',
                               onComplete: this._minimizeWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._minimizeWindowOverwritten,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
        }
    },

    _minimizeWindowDone(shellwm, actor) {
        if (this._removeEffect(this._minimizing, actor)) {
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
            actor.set_opacity(255);
            actor.set_pivot_point(0, 0);

            shellwm.completed_minimize(actor);
        }
    },

    _minimizeWindowOverwritten(shellwm, actor) {
        if (this._removeEffect(this._minimizing, actor)) {
            shellwm.completed_minimize(actor);
        }
    },

    _unminimizeWindow(shellwm, actor) {
        let types = [Meta.WindowType.NORMAL,
                     Meta.WindowType.MODAL_DIALOG,
                     Meta.WindowType.DIALOG];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_unminimize(actor);
            return;
        }

        this._unminimizing.push(actor);

        if (actor.meta_window.is_monitor_sized()) {
            actor.opacity = 0;
            actor.set_scale(1.0, 1.0);
            Tweener.addTween(actor,
                         { opacity: 255,
                           time: MINIMIZE_WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._unminimizeWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [shellwm, actor],
                           onOverwrite: this._unminimizeWindowOverwritten,
                           onOverwriteScope: this,
                           onOverwriteParams: [shellwm, actor]
                         });
        } else {
            let [success, geom] = actor.meta_window.get_icon_geometry();
            if (success) {
                actor.set_position(geom.x, geom.y);
                actor.set_scale(geom.width / actor.width,
                                geom.height / actor.height);
            } else {
                let monitor = Main.layoutManager.monitors[actor.meta_window.get_monitor()];
                if (!monitor) {
                    actor.show();
                    this._unminimizeWindowDone();
                    return;
                }
                actor.set_position(monitor.x, monitor.y);
                if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
                    actor.x += monitor.width;
                actor.set_scale(0, 0);
            }

            let rect = actor.meta_window.get_frame_rect();
            let [xDest, yDest] = [rect.x, rect.y];

            actor.show();
            Tweener.addTween(actor,
                             { scale_x: 1.0,
                               scale_y: 1.0,
                               x: xDest,
                               y: yDest,
                               time: MINIMIZE_WINDOW_ANIMATION_TIME,
                               transition: 'easeInExpo',
                               onComplete: this._unminimizeWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._unminimizeWindowOverwritten,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
        }
    },

    _unminimizeWindowDone(shellwm, actor) {
        if (this._removeEffect(this._unminimizing, actor)) {
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
            actor.set_opacity(255);
            actor.set_pivot_point(0, 0);

            shellwm.completed_unminimize(actor);
        }
    },

    _unminimizeWindowOverwritten(shellwm, actor) {
        if (this._removeEffect(this._unminimizing, actor)) {
            shellwm.completed_unminimize(actor);
        }
    },

    _sizeChangeWindow(shellwm, actor, whichChange, oldFrameRect, oldBufferRect) {
        let types = [Meta.WindowType.NORMAL];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_size_change(actor);
            return;
        }

        if (oldFrameRect.width > 0 && oldFrameRect.height > 0)
            this._prepareAnimationInfo(shellwm, actor, oldFrameRect, whichChange);
        else
            shellwm.completed_size_change(actor);
    },

    _prepareAnimationInfo(shellwm, actor, oldFrameRect, change) {
        // Position a clone of the window on top of the old position,
        // while actor updates are frozen.
        let actorContent = Shell.util_get_content_for_window_actor(actor, oldFrameRect);
        let actorClone = new St.Widget({ content: actorContent });
        actorClone.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);
        actorClone.set_position(oldFrameRect.x, oldFrameRect.y);
        actorClone.set_size(oldFrameRect.width, oldFrameRect.height);
        Main.uiGroup.add_actor(actorClone);

        if (this._clearAnimationInfo(actor))
            this._shellwm.completed_size_change(actor);

        actor.__animationInfo = { clone: actorClone,
                                  oldRect: oldFrameRect };
    },

    _sizeChangedWindow(shellwm, actor) {
        if (!actor.__animationInfo)
            return;
        if (this._resizing.indexOf(actor) != -1)
            return;

        let actorClone = actor.__animationInfo.clone;
        let targetRect = actor.meta_window.get_frame_rect();
        let sourceRect = actor.__animationInfo.oldRect;

        let scaleX = targetRect.width / sourceRect.width;
        let scaleY = targetRect.height / sourceRect.height;

        this._resizing.push(actor);

        // Now scale and fade out the clone
        Tweener.addTween(actorClone,
                         { x: targetRect.x,
                           y: targetRect.y,
                           scale_x: scaleX,
                           scale_y: scaleY,
                           opacity: 0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });

        actor.translation_x = -targetRect.x + sourceRect.x;
        actor.translation_y = -targetRect.y + sourceRect.y;

        // Now set scale the actor to size it as the clone.
        actor.scale_x = 1 / scaleX;
        actor.scale_y = 1 / scaleY;

        // Scale it to its actual new size
        Tweener.addTween(actor,
                         { scale_x: 1.0,
                           scale_y: 1.0,
                           translation_x: 0,
                           translation_y: 0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._sizeChangeWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [shellwm, actor],
                           onOverwrite: this._sizeChangeWindowOverwritten,
                           onOverwriteScope: this,
                           onOverwriteParams: [shellwm, actor]
                         });

        // Now unfreeze actor updates, to get it to the new size.
        // It's important that we don't wait until the animation is completed to
        // do this, otherwise our scale will be applied to the old texture size.
        shellwm.completed_size_change(actor);
    },

    _clearAnimationInfo(actor) {
        if (actor.__animationInfo) {
            actor.__animationInfo.clone.destroy();
            delete actor.__animationInfo;
            return true;
        }
        return false;
    },

    _sizeChangeWindowDone(shellwm, actor) {
        if (this._removeEffect(this._resizing, actor)) {
            Tweener.removeTweens(actor);
            actor.scale_x = 1.0;
            actor.scale_y = 1.0;
            actor.translation_x = 0;
            actor.translation_y = 0;
            this._clearAnimationInfo(actor);
        }
    },

    _sizeChangeWindowOverwritten(shellwm, actor) {
        if (this._removeEffect(this._resizing, actor))
            this._clearAnimationInfo(actor);
    },

    _hasAttachedDialogs(window, ignoreWindow) {
        var count = 0;
        window.foreach_transient(win => {
            if (win != ignoreWindow &&
                win.is_attached_dialog() &&
                win.get_transient_for() == window) {
                count++;
                return false;
            }
            return true;
        });
        return count != 0;
    },

    _checkDimming(window, ignoreWindow) {
        let shouldDim = this._hasAttachedDialogs(window, ignoreWindow);

        if (shouldDim && !window._dimmed) {
            window._dimmed = true;
            this._dimmedWindows.push(window);
            this._dimWindow(window);
        } else if (!shouldDim && window._dimmed) {
            window._dimmed = false;
            this._dimmedWindows =
                this._dimmedWindows.filter(win => win != window);
            this._undimWindow(window);
        }
    },

    _dimWindow(window) {
        let actor = window.get_compositor_private();
        if (!actor)
            return;
        let dimmer = getWindowDimmer(actor);
        if (!dimmer)
            return;
        if (this._shouldAnimate())
            Tweener.addTween(dimmer,
                             { dimFactor: 1.0,
                               time: DIM_TIME,
                               transition: 'linear'
                             });
        else
            dimmer.dimFactor = 1.0;
    },

    _undimWindow(window) {
        let actor = window.get_compositor_private();
        if (!actor)
            return;
        let dimmer = getWindowDimmer(actor);
        if (!dimmer)
            return;
        if (this._shouldAnimate())
            Tweener.addTween(dimmer,
                             { dimFactor: 0.0,
                               time: UNDIM_TIME,
                               transition: 'linear' });
        else
            dimmer.dimFactor = 0.0;
    },

    _mapWindow(shellwm, actor) {
        actor._windowType = actor.meta_window.get_window_type();
        actor._notifyWindowTypeSignalId =
            actor.meta_window.connect('notify::window-type', () => {
                let type = actor.meta_window.get_window_type();
                if (type == actor._windowType)
                    return;
                if (type == Meta.WindowType.MODAL_DIALOG ||
                    actor._windowType == Meta.WindowType.MODAL_DIALOG) {
                    let parent = actor.get_meta_window().get_transient_for();
                    if (parent)
                        this._checkDimming(parent);
                }

                actor._windowType = type;
            });
        actor.meta_window.connect('unmanaged', window => {
            let parent = window.get_transient_for();
            if (parent)
                this._checkDimming(parent);
        });

        if (actor.meta_window.is_attached_dialog())
            this._checkDimming(actor.get_meta_window().get_transient_for());

        let types = [Meta.WindowType.NORMAL,
                     Meta.WindowType.DIALOG,
                     Meta.WindowType.MODAL_DIALOG];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_map(actor);
            return;
        }

        switch (actor._windowType) {
        case Meta.WindowType.NORMAL:
            actor.set_pivot_point(0.5, 1.0);
            actor.scale_x = 0.01;
            actor.scale_y = 0.05;
            actor.opacity = 0;
            actor.show();
            this._mapping.push(actor);

            Tweener.addTween(actor,
                             { opacity: 255,
                               scale_x: 1,
                               scale_y: 1,
                               time: SHOW_WINDOW_ANIMATION_TIME,
                               transition: 'easeOutExpo',
                               onComplete: this._mapWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._mapWindowOverwrite,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
            break;
        case Meta.WindowType.MODAL_DIALOG:
        case Meta.WindowType.DIALOG:
            actor.set_pivot_point(0.5, 0.5);
            actor.scale_y = 0;
            actor.opacity = 0;
            actor.show();
            this._mapping.push(actor);

            Tweener.addTween(actor,
                             { opacity: 255,
                               scale_x: 1,
                               scale_y: 1,
                               time: DIALOG_SHOW_WINDOW_ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: this._mapWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._mapWindowOverwrite,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
            break;
        default:
            shellwm.completed_map(actor);
            return;
        }
    },

    _mapWindowDone(shellwm, actor) {
        if (this._removeEffect(this._mapping, actor)) {
            Tweener.removeTweens(actor);
            actor.opacity = 255;
            actor.set_pivot_point(0, 0);
            actor.scale_y = 1;
            actor.scale_x = 1;
            actor.translation_y = 0;
            actor.translation_x = 0;
            shellwm.completed_map(actor);
        }
    },

    _mapWindowOverwrite(shellwm, actor) {
        if (this._removeEffect(this._mapping, actor)) {
            shellwm.completed_map(actor);
        }
    },

    _destroyWindow(shellwm, actor) {
        let window = actor.meta_window;
        if (actor._notifyWindowTypeSignalId) {
            window.disconnect(actor._notifyWindowTypeSignalId);
            actor._notifyWindowTypeSignalId = 0;
        }
        if (window._dimmed) {
            this._dimmedWindows =
                this._dimmedWindows.filter(win => win != window);
        }

        if (window.is_attached_dialog())
            this._checkDimming(window.get_transient_for(), window);

        let types = [Meta.WindowType.NORMAL,
                     Meta.WindowType.DIALOG,
                     Meta.WindowType.MODAL_DIALOG];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_destroy(actor);
            return;
        }

        switch (actor.meta_window.window_type) {
        case Meta.WindowType.NORMAL:
            actor.set_pivot_point(0.5, 0.5);
            this._destroying.push(actor);

            Tweener.addTween(actor,
                             { opacity: 0,
                               scale_x: 0.8,
                               scale_y: 0.8,
                               time: DESTROY_WINDOW_ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: this._destroyWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._destroyWindowDone,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
            break;
        case Meta.WindowType.MODAL_DIALOG:
        case Meta.WindowType.DIALOG:
            actor.set_pivot_point(0.5, 0.5);
            this._destroying.push(actor);

            if (window.is_attached_dialog()) {
                let parent = window.get_transient_for();
                actor._parentDestroyId = parent.connect('unmanaged', () => {
                    Tweener.removeTweens(actor);
                    this._destroyWindowDone(shellwm, actor);
                });
            }

            Tweener.addTween(actor,
                             { scale_y: 0,
                               time: DIALOG_DESTROY_WINDOW_ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: this._destroyWindowDone,
                               onCompleteScope: this,
                               onCompleteParams: [shellwm, actor],
                               onOverwrite: this._destroyWindowDone,
                               onOverwriteScope: this,
                               onOverwriteParams: [shellwm, actor]
                             });
            break;
        default:
            shellwm.completed_destroy(actor);
            return;
        }
    },

    _destroyWindowDone(shellwm, actor) {
        if (this._removeEffect(this._destroying, actor)) {
            let parent = actor.get_meta_window().get_transient_for();
            if (parent && actor._parentDestroyId) {
                parent.disconnect(actor._parentDestroyId);
                actor._parentDestroyId = 0;
            }
            shellwm.completed_destroy(actor);
        }
    },

    _filterKeybinding(shellwm, binding) {
        if (Main.actionMode == Shell.ActionMode.NONE)
            return true;

        // There's little sense in implementing a keybinding in mutter and
        // not having it work in NORMAL mode; handle this case generically
        // so we don't have to explicitly allow all builtin keybindings in
        // NORMAL mode.
        if (Main.actionMode == Shell.ActionMode.NORMAL &&
            binding.is_builtin())
            return false;

        return !(this._allowedKeybindings[binding.get_name()] & Main.actionMode);
    },

    _syncStacking() {
        if (this._switchData == null)
            return;

        // Update stacking of windows in inGroup (aka the workspace we are
        // switching to). Windows in outGroup are about to be hidden anyway,
        // so we just ignore them here.
        let windows = global.get_window_actors();
        let sibling = null;
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_parent() != this._switchData.inGroup)
                continue;

            this._switchData.inGroup.set_child_above_sibling(windows[i], sibling);
            sibling = windows[i];
        }
    },

    _switchWorkspace(shellwm, from, to, direction) {
        if (!Main.sessionMode.hasWorkspaces || !this._shouldAnimate()) {
            shellwm.completed_switch_workspace();
            return;
        }

        let windows = global.get_window_actors();

        /* @direction is the direction that the "camera" moves, so the
         * screen contents have to move one screen's worth in the
         * opposite direction.
         */
        let xDest = 0, yDest = 0;

        if (direction == Meta.MotionDirection.UP ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.UP_RIGHT)
                yDest = global.screen_height - Main.panel.actor.height;
        else if (direction == Meta.MotionDirection.DOWN ||
            direction == Meta.MotionDirection.DOWN_LEFT ||
            direction == Meta.MotionDirection.DOWN_RIGHT)
                yDest = -global.screen_height + Main.panel.actor.height;

        if (direction == Meta.MotionDirection.LEFT ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.DOWN_LEFT)
                xDest = global.screen_width;
        else if (direction == Meta.MotionDirection.RIGHT ||
                 direction == Meta.MotionDirection.UP_RIGHT ||
                 direction == Meta.MotionDirection.DOWN_RIGHT)
                xDest = -global.screen_width;

        let switchData = {};
        this._switchData = switchData;
        switchData.inGroup = new Clutter.Actor();
        switchData.outGroup = new Clutter.Actor();
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.windows = [];

        let wgroup = global.window_group;
        wgroup.add_actor(switchData.inGroup);
        wgroup.add_actor(switchData.outGroup);
        wgroup.add_actor(switchData.movingWindowBin);

        for (let i = 0; i < windows.length; i++) {
            let actor = windows[i];
            let window = actor.get_meta_window();

            if (!window.showing_on_its_workspace())
                continue;

            if (window.is_on_all_workspaces())
                continue;

            let record = { window: actor,
                           parent: actor.get_parent() };

            if (this._movingWindow && window == this._movingWindow) {
                switchData.movingWindow = record;
                switchData.windows.push(switchData.movingWindow);
                actor.reparent(switchData.movingWindowBin);
            } else if (window.get_workspace().index() == from) {
                switchData.windows.push(record);
                actor.reparent(switchData.outGroup);
            } else if (window.get_workspace().index() == to) {
                switchData.windows.push(record);
                actor.reparent(switchData.inGroup);
                actor.show();
            }
        }

        switchData.inGroup.set_position(-xDest, -yDest);
        switchData.inGroup.raise_top();

        switchData.movingWindowBin.raise_top();

        Tweener.addTween(switchData.outGroup,
                         { x: xDest,
                           y: yDest,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._switchWorkspaceDone,
                           onCompleteScope: this,
                           onCompleteParams: [shellwm]
                         });
        Tweener.addTween(switchData.inGroup,
                         { x: 0,
                           y: 0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
    },

    _switchWorkspaceDone(shellwm) {
        let switchData = this._switchData;
        if (!switchData)
            return;
        this._switchData = null;

        for (let i = 0; i < switchData.windows.length; i++) {
                let w = switchData.windows[i];
                if (w.window.is_destroyed()) // Window gone
                    continue;
                if (w.window.get_parent() == switchData.outGroup) {
                    w.window.reparent(w.parent);
                    w.window.hide();
                } else
                    w.window.reparent(w.parent);
        }
        Tweener.removeTweens(switchData.inGroup);
        Tweener.removeTweens(switchData.outGroup);
        switchData.inGroup.destroy();
        switchData.outGroup.destroy();
        switchData.movingWindowBin.destroy();

        if (this._movingWindow)
            this._movingWindow = null;

        shellwm.completed_switch_workspace();
    },

    _showTilePreview(shellwm, window, tileRect, monitorIndex) {
        if (!this._tilePreview)
            this._tilePreview = new TilePreview();
        this._tilePreview.show(window, tileRect, monitorIndex);
    },

    _hideTilePreview(shellwm) {
        if (!this._tilePreview)
            return;
        this._tilePreview.hide();
    },

    _showWindowMenu(shellwm, window, menu, rect) {
        this._windowMenuManager.showWindowMenuForWindow(window, menu, rect);
    },

    _startSwitcher(display, screen, window, binding) {
        let constructor = null;
        switch (binding.get_name()) {
            case 'switch-applications':
            case 'switch-applications-backward':
            case 'switch-group':
            case 'switch-group-backward':
                constructor = AltTab.AppSwitcherPopup;
                break;
            case 'switch-windows':
            case 'switch-windows-backward':
                constructor = AltTab.WindowSwitcherPopup;
                break;
            case 'cycle-windows':
            case 'cycle-windows-backward':
                constructor = AltTab.WindowCyclerPopup;
                break;
            case 'cycle-group':
            case 'cycle-group-backward':
                constructor = AltTab.GroupCyclerPopup;
                break;
            case 'switch-monitor':
                constructor = SwitchMonitor.SwitchMonitorPopup;
                break;
        }

        if (!constructor)
            return;

        /* prevent a corner case where both popups show up at once */
        if (this._workspaceSwitcherPopup != null)
            this._workspaceSwitcherPopup.destroy();

        let tabPopup = new constructor();

        if (!tabPopup.show(binding.is_reversed(), binding.get_name(), binding.get_mask()))
            tabPopup.destroy();
    },

    _startA11ySwitcher(display, screen, window, binding) {
        Main.ctrlAltTabManager.popup(binding.is_reversed(), binding.get_name(), binding.get_mask());
    },

    _toggleAppMenu(display, screen, window, event, binding) {
        Main.panel.toggleAppMenu();
    },

    _toggleCalendar(display, screen, window, event, binding) {
        Main.panel.toggleCalendar();
    },

    _toggleTweens() {
        this._tweensPaused = !this._tweensPaused;
        const OrigTweener = imports.tweener.tweener;
        if (this._tweensPaused)
            OrigTweener.pauseAllTweens();
        else
            OrigTweener.resumeAllTweens();
    },

    _showWorkspaceSwitcher(display, screen, window, binding) {
        if (!Main.sessionMode.hasWorkspaces)
            return;

        if (screen.n_workspaces == 1)
            return;

        let [action,,,target] = binding.get_name().split('-');
        let newWs;
        let direction;

        if (action == 'move') {
            // "Moving" a window to another workspace doesn't make sense when
            // it cannot be unstuck, and is potentially confusing if a new
            // workspaces is added at the start/end
            if (window.is_always_on_all_workspaces() ||
                (Meta.prefs_get_workspaces_only_on_primary() &&
                 window.get_monitor() != Main.layoutManager.primaryIndex))
              return;
        }

        if (target == 'last') {
            direction = Meta.MotionDirection.DOWN;
            newWs = screen.get_workspace_by_index(screen.n_workspaces - 1);
        } else if (isNaN(target)) {
            // Prepend a new workspace dynamically
            if (screen.get_active_workspace_index() == 0 &&
                action == 'move' && target == 'up' && this._isWorkspacePrepended == false) {
                this.insertWorkspace(0);
                this._isWorkspacePrepended = true;
            }

            direction = Meta.MotionDirection[target.toUpperCase()];
            newWs = screen.get_active_workspace().get_neighbor(direction);
        } else if (target > 0) {
            target--;
            newWs = screen.get_workspace_by_index(target);

            if (screen.get_active_workspace().index() > target)
                direction = Meta.MotionDirection.UP;
            else
                direction = Meta.MotionDirection.DOWN;
        }

        if (direction != Meta.MotionDirection.UP &&
            direction != Meta.MotionDirection.DOWN)
            return;

        if (action == 'switch')
            this.actionMoveWorkspace(newWs);
        else
            this.actionMoveWindow(window, newWs);

        if (!Main.overview.visible) {
            if (this._workspaceSwitcherPopup == null) {
                this._workspaceTracker.blockUpdates();
                this._workspaceSwitcherPopup = new WorkspaceSwitcherPopup.WorkspaceSwitcherPopup();
                this._workspaceSwitcherPopup.connect('destroy', () => {
                    this._workspaceTracker.unblockUpdates();
                    this._workspaceSwitcherPopup = null;
                    this._isWorkspacePrepended = false;
                });
            }
            this._workspaceSwitcherPopup.display(direction, newWs.index());
        }
    },

    actionMoveWorkspace(workspace) {
        if (!Main.sessionMode.hasWorkspaces)
            return;

        let activeWorkspace = global.screen.get_active_workspace();

        if (activeWorkspace != workspace)
            workspace.activate(global.get_current_time());
    },

    actionMoveWindow(window, workspace) {
        if (!Main.sessionMode.hasWorkspaces)
            return;

        let activeWorkspace = global.screen.get_active_workspace();

        if (activeWorkspace != workspace) {
            // This won't have any effect for "always sticky" windows
            // (like desktop windows or docks)

            this._movingWindow = window;
            window.change_workspace(workspace);

            global.display.clear_mouse_mode();
            workspace.activate_with_focus (window, global.get_current_time());
        }
    },

    _confirmDisplayChange() {
        let dialog = new DisplayChangeDialog(this._shellwm);
        dialog.open();
    },

    _createCloseDialog(shellwm, window) {
        return new CloseDialog.CloseDialog(window);
    },

    _createInhibitShortcutsDialog(shellwm, window) {
        return new InhibitShortcutsDialog.InhibitShortcutsDialog(window);
    },

    _showResizePopup(display, show, rect, displayW, displayH) {
        if (show) {
            if (!this._resizePopup)
                this._resizePopup = new ResizePopup();

            this._resizePopup.set(rect, displayW, displayH);
        } else {
            if (this._resizePopup) {
                this._resizePopup.destroy();
                this._resizePopup = null;
            }
        }
    },
});
