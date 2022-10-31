// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WindowManager */

const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;

const AltTab = imports.ui.altTab;
const AppFavorites = imports.ui.appFavorites;
const Dialog = imports.ui.dialog;
const WorkspaceSwitcherPopup = imports.ui.workspaceSwitcherPopup;
const InhibitShortcutsDialog = imports.ui.inhibitShortcutsDialog;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const WindowMenu = imports.ui.windowMenu;
const PadOsd = imports.ui.padOsd;
const EdgeDragAction = imports.ui.edgeDragAction;
const CloseDialog = imports.ui.closeDialog;
const SwitchMonitor = imports.ui.switchMonitor;
const IBusManager = imports.misc.ibusManager;
const WorkspaceAnimation = imports.ui.workspaceAnimation;

const { loadInterfaceXML } = imports.misc.fileUtils;

var SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';
var MINIMIZE_WINDOW_ANIMATION_TIME = 400;
var MINIMIZE_WINDOW_ANIMATION_MODE = Clutter.AnimationMode.EASE_OUT_EXPO;
var SHOW_WINDOW_ANIMATION_TIME = 150;
var DIALOG_SHOW_WINDOW_ANIMATION_TIME = 100;
var DESTROY_WINDOW_ANIMATION_TIME = 150;
var DIALOG_DESTROY_WINDOW_ANIMATION_TIME = 100;
var WINDOW_ANIMATION_TIME = 250;
var SCROLL_TIMEOUT_TIME = 150;
var DIM_BRIGHTNESS = -0.3;
var DIM_TIME = 500;
var UNDIM_TIME = 250;
var APP_MOTION_THRESHOLD = 30;

var ONE_SECOND = 1000; // in ms

var MIN_NUM_WORKSPACES = 2;

const GSD_WACOM_BUS_NAME = 'org.gnome.SettingsDaemon.Wacom';
const GSD_WACOM_OBJECT_PATH = '/org/gnome/SettingsDaemon/Wacom';

const GsdWacomIface = loadInterfaceXML('org.gnome.SettingsDaemon.Wacom');
const GsdWacomProxy = Gio.DBusProxy.makeProxyWrapper(GsdWacomIface);

const WINDOW_DIMMER_EFFECT_NAME = "gnome-shell-window-dimmer";

Gio._promisify(Shell, 'util_start_systemd_unit');
Gio._promisify(Shell, 'util_stop_systemd_unit');

var DisplayChangeDialog = GObject.registerClass(
class DisplayChangeDialog extends ModalDialog.ModalDialog {
    _init(wm) {
        super._init();

        this._wm = wm;

        const monitorManager = global.backend.get_monitor_manager();
        this._countDown = monitorManager.get_display_configuration_timeout();

        // Translators: This string should be shorter than 30 characters
        let title = _('Keep these display settings?');
        let description = this._formatCountDown();

        this._content = new Dialog.MessageDialogContent({ title, description });
        this.contentLayout.add_child(this._content);

        /* Translators: this and the following message should be limited in length,
           to avoid ellipsizing the labels.
        */
        this._cancelButton = this.addButton({
            label: _('Revert Settings'),
            action: this._onFailure.bind(this),
            key: Clutter.KEY_Escape,
        });
        this._okButton = this.addButton({
            label: _('Keep Changes'),
            action: this._onSuccess.bind(this),
            default: true,
        });

        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, ONE_SECOND, this._tick.bind(this));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._tick');
    }

    close(timestamp) {
        if (this._timeoutId > 0) {
            GLib.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        super.close(timestamp);
    }

    _formatCountDown() {
        const fmt = ngettext(
            'Settings changes will revert in %d second',
            'Settings changes will revert in %d seconds',
            this._countDown);
        return fmt.format(this._countDown);
    }

    _tick() {
        this._countDown--;

        if (this._countDown == 0) {
            /* mutter already takes care of failing at timeout */
            this._timeoutId = 0;
            this.close();
            return GLib.SOURCE_REMOVE;
        }

        this._content.description = this._formatCountDown();
        return GLib.SOURCE_CONTINUE;
    }

    _onFailure() {
        this._wm.complete_display_change(false);
        this.close();
    }

    _onSuccess() {
        this._wm.complete_display_change(true);
        this.close();
    }
});

var WindowDimmer = GObject.registerClass(
class WindowDimmer extends Clutter.BrightnessContrastEffect {
    _init() {
        super._init({
            name: WINDOW_DIMMER_EFFECT_NAME,
            enabled: false,
        });
    }

    _syncEnabled(dimmed) {
        let animating = this.actor.get_transition(`@effects.${this.name}.brightness`) !== null;

        this.enabled = Meta.prefs_get_attach_modal_dialogs() && (animating || dimmed);
    }

    setDimmed(dimmed, animate) {
        let val = 127 * (1 + (dimmed ? 1 : 0) * DIM_BRIGHTNESS);
        let color = Clutter.Color.new(val, val, val, 255);

        this.actor.ease_property(`@effects.${this.name}.brightness`, color, {
            mode: Clutter.AnimationMode.LINEAR,
            duration: (dimmed ? DIM_TIME : UNDIM_TIME) * (animate ? 1 : 0),
            onStopped: () => this._syncEnabled(dimmed),
        });

        this._syncEnabled(dimmed);
    }
});

function getWindowDimmer(actor) {
    let effect = actor.get_effect(WINDOW_DIMMER_EFFECT_NAME);

    if (!effect) {
        effect = new WindowDimmer();
        actor.add_effect(effect);
    }
    return effect;
}

/*
 * When the last window closed on a workspace is a dialog or splash
 * screen, we assume that it might be an initial window shown before
 * the main window of an application, and give the app a grace period
 * where it can map another window before we remove the workspace.
 */
var LAST_WINDOW_GRACE_TIME = 1000;

var WorkspaceTracker = class {
    constructor(wm) {
        this._wm = wm;

        this._workspaces = [];
        this._checkWorkspacesId = 0;

        this._pauseWorkspaceCheck = false;

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('startup-sequence-changed', this._queueCheckWorkspaces.bind(this));

        let workspaceManager = global.workspace_manager;
        workspaceManager.connect('notify::n-workspaces',
                                 this._nWorkspacesChanged.bind(this));
        workspaceManager.connect('workspaces-reordered', () => {
            this._workspaces.sort((a, b) => a.index() - b.index());
        });
        global.window_manager.connect('switch-workspace',
                                      this._queueCheckWorkspaces.bind(this));

        global.display.connect('window-entered-monitor',
                               this._windowEnteredMonitor.bind(this));
        global.display.connect('window-left-monitor',
                               this._windowLeftMonitor.bind(this));

        this._workspaceSettings = new Gio.Settings({ schema_id: 'org.gnome.mutter' });
        this._workspaceSettings.connect('changed::dynamic-workspaces', this._queueCheckWorkspaces.bind(this));

        this._nWorkspacesChanged();
    }

    blockUpdates() {
        this._pauseWorkspaceCheck = true;
    }

    unblockUpdates() {
        this._pauseWorkspaceCheck = false;
    }

    _checkWorkspaces() {
        let workspaceManager = global.workspace_manager;
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
            if (index >= 0 && index <= workspaceManager.n_workspaces)
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
        if (!emptyWorkspaces[emptyWorkspaces.length - 1]) {
            workspaceManager.append_new_workspace(false, global.get_current_time());
            emptyWorkspaces.push(true);
        }

        // Enforce minimum number of workspaces
        while (emptyWorkspaces.length < MIN_NUM_WORKSPACES) {
            workspaceManager.append_new_workspace(false, global.get_current_time());
            emptyWorkspaces.push(true);
        }

        let lastIndex = emptyWorkspaces.length - 1;
        let lastEmptyIndex = emptyWorkspaces.lastIndexOf(false) + 1;
        let activeWorkspaceIndex = workspaceManager.get_active_workspace_index();
        emptyWorkspaces[activeWorkspaceIndex] = false;

        // Delete empty workspaces except for the last one; do it from the end
        // to avoid index changes
        for (i = lastIndex; i >= 0; i--) {
            if (workspaceManager.n_workspaces === MIN_NUM_WORKSPACES)
                break;
            if (emptyWorkspaces[i] && i != lastEmptyIndex)
                workspaceManager.remove_workspace(this._workspaces[i], global.get_current_time());
        }

        this._checkWorkspacesId = 0;
        return false;
    }

    keepWorkspaceAlive(workspace, duration) {
        if (workspace._keepAliveId)
            GLib.source_remove(workspace._keepAliveId);

        workspace._keepAliveId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, duration, () => {
            workspace._keepAliveId = 0;
            this._queueCheckWorkspaces();
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(workspace._keepAliveId, '[gnome-shell] this._queueCheckWorkspaces');
    }

    _windowRemoved(workspace, window) {
        workspace._lastRemovedWindow = window;
        this._queueCheckWorkspaces();
        let id = GLib.timeout_add(GLib.PRIORITY_DEFAULT, LAST_WINDOW_GRACE_TIME, () => {
            if (workspace._lastRemovedWindow == window) {
                workspace._lastRemovedWindow = null;
                this._queueCheckWorkspaces();
            }
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(id, '[gnome-shell] this._queueCheckWorkspaces');
    }

    _windowLeftMonitor(metaDisplay, monitorIndex, _metaWin) {
        // If the window left the primary monitor, that
        // might make that workspace empty
        if (monitorIndex == Main.layoutManager.primaryIndex)
            this._queueCheckWorkspaces();
    }

    _windowEnteredMonitor(metaDisplay, monitorIndex, _metaWin) {
        // If the window entered the primary monitor, that
        // might make that workspace non-empty
        if (monitorIndex == Main.layoutManager.primaryIndex)
            this._queueCheckWorkspaces();
    }

    _queueCheckWorkspaces() {
        if (this._checkWorkspacesId === 0) {
            const laters = global.compositor.get_laters();
            this._checkWorkspacesId =
                laters.add(Meta.LaterType.BEFORE_REDRAW, this._checkWorkspaces.bind(this));
        }
    }

    _nWorkspacesChanged() {
        let workspaceManager = global.workspace_manager;
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = workspaceManager.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return false;

        if (newNumWorkspaces > oldNumWorkspaces) {
            let w;

            // Assume workspaces are only added at the end
            for (w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._workspaces[w] = workspaceManager.get_workspace_by_index(w);

            for (w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._workspaces[w].connectObject(
                    'window-added', this._queueCheckWorkspaces.bind(this),
                    'window-removed', this._windowRemoved.bind(this), this);
            }
        } else {
            // Assume workspaces are only removed sequentially
            // (e.g. 2,3,4 - not 2,4,7)
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let workspace = workspaceManager.get_workspace_by_index(w);
                if (this._workspaces[w] != workspace) {
                    removedIndex = w;
                    break;
                }
            }

            let lostWorkspaces = this._workspaces.splice(removedIndex, removedNum);
            lostWorkspaces.forEach(workspace => workspace.disconnectObject(this));
        }

        this._queueCheckWorkspaces();

        return false;
    }
};

var TilePreview = GObject.registerClass(
class TilePreview extends St.Widget {
    _init() {
        super._init();
        global.window_group.add_actor(this);

        this._reset();
        this._showing = false;
    }

    open(window, tileRect, monitorIndex) {
        let windowActor = window.get_compositor_private();
        if (!windowActor)
            return;

        global.window_group.set_child_below_sibling(this, windowActor);

        if (this._rect && this._rect.equal(tileRect))
            return;

        let changeMonitor = this._monitorIndex == -1 ||
                             this._monitorIndex != monitorIndex;

        this._monitorIndex = monitorIndex;
        this._rect = tileRect;

        let monitor = Main.layoutManager.monitors[monitorIndex];

        this._updateStyle(monitor);

        if (!this._showing || changeMonitor) {
            const monitorRect = new Meta.Rectangle({
                x: monitor.x,
                y: monitor.y,
                width: monitor.width,
                height: monitor.height,
            });
            let [, rect] = window.get_frame_rect().intersect(monitorRect);
            this.set_size(rect.width, rect.height);
            this.set_position(rect.x, rect.y);
            this.opacity = 0;
        }

        this._showing = true;
        this.show();
        this.ease({
            x: tileRect.x,
            y: tileRect.y,
            width: tileRect.width,
            height: tileRect.height,
            opacity: 255,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    close() {
        if (!this._showing)
            return;

        this._showing = false;
        this.ease({
            opacity: 0,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._reset(),
        });
    }

    _reset() {
        this.hide();
        this._rect = null;
        this._monitorIndex = -1;
    }

    _updateStyle(monitor) {
        let styles = ['tile-preview'];
        if (this._monitorIndex == Main.layoutManager.primaryIndex)
            styles.push('on-primary');
        if (this._rect.x == monitor.x)
            styles.push('tile-preview-left');
        if (this._rect.x + this._rect.width == monitor.x + monitor.width)
            styles.push('tile-preview-right');

        this.style_class = styles.join(' ');
    }
});

var AppSwitchAction = GObject.registerClass({
    Signals: { 'activated': {} },
}, class AppSwitchAction extends Clutter.GestureAction {
    _init() {
        super._init();
        this.set_n_touch_points(3);
    }

    vfunc_gesture_prepare(_actor) {
        if (Main.actionMode != Shell.ActionMode.NORMAL) {
            this.cancel();
            return false;
        }

        return this.get_n_current_points() <= 4;
    }

    vfunc_gesture_begin(_actor) {
        // in milliseconds
        const LONG_PRESS_TIMEOUT = 250;

        let nPoints = this.get_n_current_points();
        let event = this.get_last_event(nPoints - 1);

        if (nPoints == 3) {
            this._longPressStartTime = event.get_time();
        } else if (nPoints == 4) {
            // Check whether the 4th finger press happens after a 3-finger long press,
            // this only needs to be checked on the first 4th finger press
            if (this._longPressStartTime != null &&
                event.get_time() < this._longPressStartTime + LONG_PRESS_TIMEOUT) {
                this.cancel();
            } else {
                this._longPressStartTime = null;
                this.emit('activated');
            }
        }

        return this.get_n_current_points() <= 4;
    }

    vfunc_gesture_progress(_actor) {
        if (this.get_n_current_points() == 3) {
            for (let i = 0; i < this.get_n_current_points(); i++) {
                let [startX, startY] = this.get_press_coords(i);
                let [x, y] = this.get_motion_coords(i);

                if (Math.abs(x - startX) > APP_MOTION_THRESHOLD ||
                    Math.abs(y - startY) > APP_MOTION_THRESHOLD)
                    return false;
            }
        }

        return true;
    }
});

var ResizePopup = GObject.registerClass(
class ResizePopup extends St.Widget {
    _init() {
        super._init({ layout_manager: new Clutter.BinLayout() });
        this._label = new St.Label({
            style_class: 'resize-popup',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
        });
        this.add_child(this._label);
        Main.uiGroup.add_actor(this);
    }

    set(rect, displayW, displayH) {
        /* Translators: This represents the size of a window. The first number is
         * the width of the window and the second is the height. */
        let text = _("%d × %d").format(displayW, displayH);
        this._label.set_text(text);

        this.set_position(rect.x, rect.y);
        this.set_size(rect.width, rect.height);
    }
});

var WindowManager = class {
    constructor() {
        this._shellwm =  global.window_manager;

        this._minimizing = new Set();
        this._unminimizing = new Set();
        this._mapping = new Set();
        this._resizing = new Set();
        this._resizePending = new Set();
        this._destroying = new Set();

        this._dimmedWindows = [];

        this._skippedActors = new Set();

        this._allowedKeybindings = {};

        this._isWorkspacePrepended = false;
        this._canScroll = true; // limiting scrolling speed

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

        this.addKeybinding('open-application-menu',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.POPUP,
                           this._toggleAppMenu.bind(this));

        this.addKeybinding('toggle-message-tray',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW |
                           Shell.ActionMode.POPUP,
                           this._toggleCalendar.bind(this));

        this.addKeybinding('switch-to-application-1',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-2',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-3',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-4',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-5',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-6',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-7',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-8',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        this.addKeybinding('switch-to-application-9',
                           new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                           Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                           Shell.ActionMode.NORMAL |
                           Shell.ActionMode.OVERVIEW,
                           this._switchToApplication.bind(this));

        global.stage.connect('scroll-event', (stage, event) => {
            const allowedModes = Shell.ActionMode.NORMAL;
            if ((allowedModes & Main.actionMode) === 0)
                return Clutter.EVENT_PROPAGATE;

            if (this._workspaceAnimation.canHandleScrollEvent(event))
                return Clutter.EVENT_PROPAGATE;

            if ((event.get_state() & global.display.compositor_modifiers) === 0)
                return Clutter.EVENT_PROPAGATE;

            return this.handleWorkspaceScroll(event);
        });

        global.display.connect('show-resize-popup', this._showResizePopup.bind(this));
        global.display.connect('show-pad-osd', this._showPadOsd.bind(this));
        global.display.connect('show-osd', (display, monitorIndex, iconName, label) => {
            let icon = Gio.Icon.new_for_string(iconName);
            Main.osdWindowManager.show(monitorIndex, icon, label, null);
        });

        this._gsdWacomProxy = new GsdWacomProxy(Gio.DBus.session, GSD_WACOM_BUS_NAME,
                                                GSD_WACOM_OBJECT_PATH,
                                                (proxy, error) => {
                                                    if (error)
                                                        log(error.message);
                                                });

        global.display.connect('pad-mode-switch', (display, pad, _group, _mode) => {
            let labels = [];

            // FIXME: Fix num buttons
            for (let i = 0; i < 50; i++) {
                let str = display.get_pad_action_label(pad, Meta.PadActionType.BUTTON, i);
                labels.push(str ?? '');
            }

            this._gsdWacomProxy?.SetOLEDLabelsAsync(
                pad.get_device_node(), labels).catch(logError);
        });

        global.display.connect('init-xserver', (display, task) => {
            IBusManager.getIBusManager().restartDaemon(['--xim']);

            this._startX11Services(task);

            return true;
        });
        global.display.connect('x11-display-closing', () => {
            if (!Meta.is_wayland_compositor())
                return;

            this._stopX11Services(null);

            IBusManager.getIBusManager().restartDaemon();
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

        let appSwitchAction = new AppSwitchAction();
        appSwitchAction.connect('activated', this._switchApp.bind(this));
        global.stage.add_action_full('app-switch', Clutter.EventPhase.CAPTURE, appSwitchAction);

        let mode = Shell.ActionMode.NORMAL;
        let topDragAction = new EdgeDragAction.EdgeDragAction(St.Side.TOP, mode);
        topDragAction.connect('activated',  () => {
            let currentWindow = global.display.focus_window;
            if (currentWindow)
                currentWindow.unmake_fullscreen();
        });

        let updateUnfullscreenGesture = () => {
            let currentWindow = global.display.focus_window;
            topDragAction.enabled = currentWindow && currentWindow.is_fullscreen();
        };

        global.display.connect('notify::focus-window', updateUnfullscreenGesture);
        global.display.connect('in-fullscreen-changed', updateUnfullscreenGesture);
        updateUnfullscreenGesture();

        global.stage.add_action_full('unfullscreen', Clutter.EventPhase.CAPTURE, topDragAction);

        this._workspaceAnimation =
            new WorkspaceAnimation.WorkspaceAnimationController();

        this._shellwm.connect('kill-switch-workspace', () => {
            this._workspaceAnimation.cancelSwitchAnimation();
            this._switchWorkspaceDone();
        });
    }

    async _startX11Services(task) {
        let status = true;
        try {
            await Shell.util_start_systemd_unit(
                'gnome-session-x11-services-ready.target', 'fail', null);
        } catch (e) {
            // Ignore NOT_SUPPORTED error, which indicates we are not systemd
            // managed and gnome-session will have taken care of everything
            // already.
            // Note that we do log cancellation from here.
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_SUPPORTED)) {
                log(`Error starting X11 services: ${e.message}`);
                status = false;
            }
        } finally {
            task.return_boolean(status);
        }
    }

    async _stopX11Services(cancellable) {
        try {
            await Shell.util_stop_systemd_unit(
                'gnome-session-x11-services.target', 'fail', cancellable);
        } catch (e) {
            // Ignore NOT_SUPPORTED error, which indicates we are not systemd
            // managed and gnome-session will have taken care of everything
            // already.
            // Note that we do log cancellation from here.
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_SUPPORTED))
                log(`Error stopping X11 services: ${e.message}`);
        }
    }

    _showPadOsd(display, device, settings, imagePath, editionMode, monitorIndex) {
        this._currentPadOsd = new PadOsd.PadOsd(device, settings, imagePath, editionMode, monitorIndex);
        this._currentPadOsd.connect('closed', () => (this._currentPadOsd = null));

        return this._currentPadOsd;
    }

    _lookupIndex(windows, metaWindow) {
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].metaWindow == metaWindow)
                return i;
        }
        return -1;
    }

    _switchApp() {
        let windows = global.get_window_actors().filter(actor => {
            let win = actor.metaWindow;
            let workspaceManager = global.workspace_manager;
            let activeWorkspace = workspaceManager.get_active_workspace();
            return !win.is_override_redirect() &&
                    win.located_on_workspace(activeWorkspace);
        });

        if (windows.length == 0)
            return;

        let focusWindow = global.display.focus_window;
        let nextWindow;

        if (focusWindow == null) {
            nextWindow = windows[0].metaWindow;
        } else {
            let index = this._lookupIndex(windows, focusWindow) + 1;

            if (index >= windows.length)
                index = 0;

            nextWindow = windows[index].metaWindow;
        }

        Main.activateWindow(nextWindow);
    }

    insertWorkspace(pos) {
        let workspaceManager = global.workspace_manager;

        if (!Meta.prefs_get_dynamic_workspaces())
            return;

        workspaceManager.append_new_workspace(false, global.get_current_time());

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
            // Sticky windows don't need moving, in fact moving would
            // unstick them
            if (window.on_all_workspaces)
                return;
            // Windows on workspaces below pos don't need moving
            let index = window.get_workspace().index();
            if (index < pos)
                return;
            window.change_workspace_by_index(index + 1, true);
        });

        // If the new workspace was inserted before the active workspace,
        // activate the workspace to which its windows went
        let activeIndex = workspaceManager.get_active_workspace_index();
        if (activeIndex >= pos) {
            let newWs = workspaceManager.get_workspace_by_index(activeIndex + 1);
            this._blockAnimations = true;
            newWs.activate(global.get_current_time());
            this._blockAnimations = false;
        }
    }

    keepWorkspaceAlive(workspace, duration) {
        if (!this._workspaceTracker)
            return;

        this._workspaceTracker.keepWorkspaceAlive(workspace, duration);
    }

    skipNextEffect(actor) {
        this._skippedActors.add(actor);
    }

    setCustomKeybindingHandler(name, modes, handler) {
        if (Meta.keybindings_set_custom_handler(name, handler))
            this.allowKeybinding(name, modes);
    }

    addKeybinding(name, settings, flags, modes, handler) {
        let action = global.display.add_keybinding(name, settings, flags, handler);
        if (action != Meta.KeyBindingAction.NONE)
            this.allowKeybinding(name, modes);
        return action;
    }

    removeKeybinding(name) {
        if (global.display.remove_keybinding(name))
            this.allowKeybinding(name, Shell.ActionMode.NONE);
    }

    allowKeybinding(name, modes) {
        this._allowedKeybindings[name] = modes;
    }

    _shouldAnimate() {
        const overviewOpen = Main.overview.visible && !Main.overview.closing;
        return !(overviewOpen || this._workspaceAnimation.gestureActive);
    }

    _shouldAnimateActor(actor, types) {
        if (this._skippedActors.delete(actor))
            return false;

        if (!this._shouldAnimate())
            return false;

        if (!actor.get_texture())
            return false;

        let type = actor.meta_window.get_window_type();
        return types.includes(type);
    }

    _minimizeWindow(shellwm, actor) {
        const types = [
            Meta.WindowType.NORMAL,
            Meta.WindowType.MODAL_DIALOG,
            Meta.WindowType.DIALOG,
        ];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_minimize(actor);
            return;
        }

        actor.set_scale(1.0, 1.0);

        this._minimizing.add(actor);

        if (actor.meta_window.is_monitor_sized()) {
            actor.ease({
                opacity: 0,
                duration: MINIMIZE_WINDOW_ANIMATION_TIME,
                mode: MINIMIZE_WINDOW_ANIMATION_MODE,
                onStopped: () => this._minimizeWindowDone(shellwm, actor),
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

            actor.ease({
                scale_x: xScale,
                scale_y: yScale,
                x: xDest,
                y: yDest,
                duration: MINIMIZE_WINDOW_ANIMATION_TIME,
                mode: MINIMIZE_WINDOW_ANIMATION_MODE,
                onStopped: () => this._minimizeWindowDone(shellwm, actor),
            });
        }
    }

    _minimizeWindowDone(shellwm, actor) {
        if (this._minimizing.delete(actor)) {
            actor.remove_all_transitions();
            actor.set_scale(1.0, 1.0);
            actor.set_opacity(255);
            actor.set_pivot_point(0, 0);

            shellwm.completed_minimize(actor);
        }
    }

    _unminimizeWindow(shellwm, actor) {
        const types = [
            Meta.WindowType.NORMAL,
            Meta.WindowType.MODAL_DIALOG,
            Meta.WindowType.DIALOG,
        ];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_unminimize(actor);
            return;
        }

        this._unminimizing.add(actor);

        if (actor.meta_window.is_monitor_sized()) {
            actor.opacity = 0;
            actor.set_scale(1.0, 1.0);
            actor.ease({
                opacity: 255,
                duration: MINIMIZE_WINDOW_ANIMATION_TIME,
                mode: MINIMIZE_WINDOW_ANIMATION_MODE,
                onStopped: () => this._unminimizeWindowDone(shellwm, actor),
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

            let rect = actor.meta_window.get_buffer_rect();
            let [xDest, yDest] = [rect.x, rect.y];

            actor.show();
            actor.ease({
                scale_x: 1,
                scale_y: 1,
                x: xDest,
                y: yDest,
                duration: MINIMIZE_WINDOW_ANIMATION_TIME,
                mode: MINIMIZE_WINDOW_ANIMATION_MODE,
                onStopped: () => this._unminimizeWindowDone(shellwm, actor),
            });
        }
    }

    _unminimizeWindowDone(shellwm, actor) {
        if (this._unminimizing.delete(actor)) {
            actor.remove_all_transitions();
            actor.set_scale(1.0, 1.0);
            actor.set_opacity(255);
            actor.set_pivot_point(0, 0);

            shellwm.completed_unminimize(actor);
        }
    }

    _sizeChangeWindow(shellwm, actor, whichChange, oldFrameRect, _oldBufferRect) {
        const types = [Meta.WindowType.NORMAL];
        const shouldAnimate =
            this._shouldAnimateActor(actor, types) &&
            oldFrameRect.width > 0 &&
            oldFrameRect.height > 0;

        if (shouldAnimate)
            this._prepareAnimationInfo(shellwm, actor, oldFrameRect, whichChange);
        else
            shellwm.completed_size_change(actor);
    }

    _prepareAnimationInfo(shellwm, actor, oldFrameRect, _change) {
        // Position a clone of the window on top of the old position,
        // while actor updates are frozen.
        let actorContent = actor.paint_to_content(oldFrameRect);
        let actorClone = new St.Widget({ content: actorContent });
        actorClone.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);
        actorClone.set_position(oldFrameRect.x, oldFrameRect.y);
        actorClone.set_size(oldFrameRect.width, oldFrameRect.height);

        actor.freeze();

        if (this._clearAnimationInfo(actor)) {
            log(`Old animationInfo removed from actor ${actor}`);
            this._shellwm.completed_size_change(actor);
        }

        actor.connectObject('destroy',
            () => this._clearAnimationInfo(actor), actorClone);

        this._resizePending.add(actor);
        actor.__animationInfo = {
            clone: actorClone,
            oldRect: oldFrameRect,
            frozen: true,
        };
    }

    _sizeChangedWindow(shellwm, actor) {
        if (!actor.__animationInfo)
            return;
        if (this._resizing.has(actor))
            return;

        let actorClone = actor.__animationInfo.clone;
        let targetRect = actor.meta_window.get_frame_rect();
        let sourceRect = actor.__animationInfo.oldRect;

        let scaleX = targetRect.width / sourceRect.width;
        let scaleY = targetRect.height / sourceRect.height;

        this._resizePending.delete(actor);
        this._resizing.add(actor);

        Main.uiGroup.add_child(actorClone);

        // Now scale and fade out the clone
        actorClone.ease({
            x: targetRect.x,
            y: targetRect.y,
            scale_x: scaleX,
            scale_y: scaleY,
            opacity: 0,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        actor.translation_x = -targetRect.x + sourceRect.x;
        actor.translation_y = -targetRect.y + sourceRect.y;

        // Now set scale the actor to size it as the clone.
        actor.scale_x = 1 / scaleX;
        actor.scale_y = 1 / scaleY;

        // Scale it to its actual new size
        actor.ease({
            scale_x: 1,
            scale_y: 1,
            translation_x: 0,
            translation_y: 0,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => this._sizeChangeWindowDone(shellwm, actor),
        });

        // ease didn't animate and cleared the info, we are done
        if (!actor.__animationInfo)
            return;

        // Now unfreeze actor updates, to get it to the new size.
        // It's important that we don't wait until the animation is completed to
        // do this, otherwise our scale will be applied to the old texture size.
        actor.thaw();
        actor.__animationInfo.frozen = false;
    }

    _clearAnimationInfo(actor) {
        if (actor.__animationInfo) {
            actor.__animationInfo.clone.destroy();
            if (actor.__animationInfo.frozen)
                actor.thaw();

            delete actor.__animationInfo;
            return true;
        }
        return false;
    }

    _sizeChangeWindowDone(shellwm, actor) {
        if (this._resizing.delete(actor)) {
            actor.remove_all_transitions();
            actor.scale_x = 1.0;
            actor.scale_y = 1.0;
            actor.translation_x = 0;
            actor.translation_y = 0;
            this._clearAnimationInfo(actor);
            this._shellwm.completed_size_change(actor);
        }

        if (this._resizePending.delete(actor)) {
            this._clearAnimationInfo(actor);
            this._shellwm.completed_size_change(actor);
        }
    }

    _checkDimming(window) {
        const shouldDim = window.has_attached_dialogs();

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
    }

    _dimWindow(window) {
        let actor = window.get_compositor_private();
        if (!actor)
            return;
        let dimmer = getWindowDimmer(actor);
        if (!dimmer)
            return;
        dimmer.setDimmed(true, this._shouldAnimate());
    }

    _undimWindow(window) {
        let actor = window.get_compositor_private();
        if (!actor)
            return;
        let dimmer = getWindowDimmer(actor);
        if (!dimmer)
            return;
        dimmer.setDimmed(false, this._shouldAnimate());
    }

    _waitForOverviewToHide() {
        if (!Main.overview.visible)
            return Promise.resolve();

        return new Promise(resolve => {
            const id = Main.overview.connect('hidden', () => {
                Main.overview.disconnect(id);
                resolve();
            });
        });
    }

    async _mapWindow(shellwm, actor) {
        actor._windowType = actor.meta_window.get_window_type();
        actor.meta_window.connectObject('notify::window-type', () => {
            let type = actor.meta_window.get_window_type();
            if (type === actor._windowType)
                return;
            if (type === Meta.WindowType.MODAL_DIALOG ||
                actor._windowType === Meta.WindowType.MODAL_DIALOG) {
                let parent = actor.get_meta_window().get_transient_for();
                if (parent)
                    this._checkDimming(parent);
            }

            actor._windowType = type;
        }, actor);
        actor.meta_window.connect('unmanaged', window => {
            let parent = window.get_transient_for();
            if (parent)
                this._checkDimming(parent);
        });

        if (actor.meta_window.is_attached_dialog())
            this._checkDimming(actor.get_meta_window().get_transient_for());

        const types = [
            Meta.WindowType.NORMAL,
            Meta.WindowType.DIALOG,
            Meta.WindowType.MODAL_DIALOG,
        ];
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
            this._mapping.add(actor);

            await this._waitForOverviewToHide();
            actor.ease({
                opacity: 255,
                scale_x: 1,
                scale_y: 1,
                duration: SHOW_WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_EXPO,
                onStopped: () => this._mapWindowDone(shellwm, actor),
            });
            break;
        case Meta.WindowType.MODAL_DIALOG:
        case Meta.WindowType.DIALOG:
            actor.set_pivot_point(0.5, 0.5);
            actor.scale_y = 0;
            actor.opacity = 0;
            actor.show();
            this._mapping.add(actor);

            await this._waitForOverviewToHide();
            actor.ease({
                opacity: 255,
                scale_x: 1,
                scale_y: 1,
                duration: DIALOG_SHOW_WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onStopped: () => this._mapWindowDone(shellwm, actor),
            });
            break;
        default:
            shellwm.completed_map(actor);
        }
    }

    _mapWindowDone(shellwm, actor) {
        if (this._mapping.delete(actor)) {
            actor.remove_all_transitions();
            actor.opacity = 255;
            actor.set_pivot_point(0, 0);
            actor.scale_y = 1;
            actor.scale_x = 1;
            actor.translation_y = 0;
            actor.translation_x = 0;
            shellwm.completed_map(actor);
        }
    }

    _destroyWindow(shellwm, actor) {
        let window = actor.meta_window;
        window.disconnectObject(actor);
        if (window._dimmed) {
            this._dimmedWindows =
                this._dimmedWindows.filter(win => win != window);
        }

        if (window.is_attached_dialog())
            this._checkDimming(window.get_transient_for());

        const types = [
            Meta.WindowType.NORMAL,
            Meta.WindowType.DIALOG,
            Meta.WindowType.MODAL_DIALOG,
        ];
        if (!this._shouldAnimateActor(actor, types)) {
            shellwm.completed_destroy(actor);
            return;
        }

        switch (actor.meta_window.window_type) {
        case Meta.WindowType.NORMAL:
            actor.set_pivot_point(0.5, 0.5);
            this._destroying.add(actor);

            actor.ease({
                opacity: 0,
                scale_x: 0.8,
                scale_y: 0.8,
                duration: DESTROY_WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onStopped: () => this._destroyWindowDone(shellwm, actor),
            });
            break;
        case Meta.WindowType.MODAL_DIALOG:
        case Meta.WindowType.DIALOG:
            actor.set_pivot_point(0.5, 0.5);
            this._destroying.add(actor);

            if (window.is_attached_dialog()) {
                let parent = window.get_transient_for();
                parent.connectObject('unmanaged', () => {
                    actor.remove_all_transitions();
                    this._destroyWindowDone(shellwm, actor);
                }, actor);
            }

            actor.ease({
                scale_y: 0,
                duration: DIALOG_DESTROY_WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onStopped: () => this._destroyWindowDone(shellwm, actor),
            });
            break;
        default:
            shellwm.completed_destroy(actor);
        }
    }

    _destroyWindowDone(shellwm, actor) {
        if (this._destroying.delete(actor)) {
            const parent = actor.get_meta_window()?.get_transient_for();
            parent?.disconnectObject(actor);
            shellwm.completed_destroy(actor);
        }
    }

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
    }

    _switchWorkspace(shellwm, from, to, direction) {
        if (!Main.sessionMode.hasWorkspaces || !this._shouldAnimate()) {
            shellwm.completed_switch_workspace();
            return;
        }

        this._switchInProgress = true;

        this._workspaceAnimation.animateSwitch(from, to, direction, () => {
            this._shellwm.completed_switch_workspace();
            this._switchInProgress = false;
        });
    }

    _switchWorkspaceDone() {
        if (!this._switchInProgress)
            return;

        this._shellwm.completed_switch_workspace();
        this._switchInProgress = false;
    }

    _showTilePreview(shellwm, window, tileRect, monitorIndex) {
        if (!this._tilePreview)
            this._tilePreview = new TilePreview();
        this._tilePreview.open(window, tileRect, monitorIndex);
    }

    _hideTilePreview() {
        if (!this._tilePreview)
            return;
        this._tilePreview.close();
    }

    _showWindowMenu(shellwm, window, menu, rect) {
        this._windowMenuManager.showWindowMenuForWindow(window, menu, rect);
    }

    _startSwitcher(display, window, binding) {
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
    }

    _startA11ySwitcher(display, window, binding) {
        Main.ctrlAltTabManager.popup(binding.is_reversed(), binding.get_name(), binding.get_mask());
    }

    _allowFavoriteShortcuts() {
        return Main.sessionMode.hasOverview;
    }

    _switchToApplication(display, window, binding) {
        if (!this._allowFavoriteShortcuts())
            return;

        let [, , , target] = binding.get_name().split('-');
        let apps = AppFavorites.getAppFavorites().getFavorites();
        let app = apps[target - 1];
        if (app) {
            Main.overview.hide();
            app.activate();
        }
    }

    _toggleAppMenu() {
        Main.panel.toggleAppMenu();
    }

    _toggleCalendar() {
        Main.panel.toggleCalendar();
    }

    _showWorkspaceSwitcher(display, window, binding) {
        let workspaceManager = display.get_workspace_manager();

        if (!Main.sessionMode.hasWorkspaces)
            return;

        if (workspaceManager.n_workspaces == 1)
            return;

        let [action,,, target] = binding.get_name().split('-');
        let newWs;
        let direction;
        let vertical = workspaceManager.layout_rows == -1;
        let rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;

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
            if (vertical)
                direction = Meta.MotionDirection.DOWN;
            else if (rtl)
                direction = Meta.MotionDirection.LEFT;
            else
                direction = Meta.MotionDirection.RIGHT;
            newWs = workspaceManager.get_workspace_by_index(workspaceManager.n_workspaces - 1);
        } else if (isNaN(target)) {
            // Prepend a new workspace dynamically
            let prependTarget;
            if (vertical)
                prependTarget = 'up';
            else if (rtl)
                prependTarget = 'right';
            else
                prependTarget = 'left';
            if (workspaceManager.get_active_workspace_index() === 0 &&
                action === 'move' && target === prependTarget &&
                this._isWorkspacePrepended === false) {
                this.insertWorkspace(0);
                this._isWorkspacePrepended = true;
            }

            direction = Meta.MotionDirection[target.toUpperCase()];
            newWs = workspaceManager.get_active_workspace().get_neighbor(direction);
        } else if ((target > 0) && (target <= workspaceManager.n_workspaces)) {
            target--;
            newWs = workspaceManager.get_workspace_by_index(target);

            if (workspaceManager.get_active_workspace_index() > target) {
                if (vertical)
                    direction = Meta.MotionDirection.UP;
                else if (rtl)
                    direction = Meta.MotionDirection.RIGHT;
                else
                    direction = Meta.MotionDirection.LEFT;
            } else {
                if (vertical) // eslint-disable-line no-lonely-if
                    direction = Meta.MotionDirection.DOWN;
                else if (rtl)
                    direction = Meta.MotionDirection.LEFT;
                else
                    direction = Meta.MotionDirection.RIGHT;
            }
        }

        if (workspaceManager.layout_rows == -1 &&
            direction != Meta.MotionDirection.UP &&
            direction != Meta.MotionDirection.DOWN)
            return;

        if (workspaceManager.layout_columns == -1 &&
            direction != Meta.MotionDirection.LEFT &&
            direction != Meta.MotionDirection.RIGHT)
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
            this._workspaceSwitcherPopup.display(newWs.index());
        }
    }

    actionMoveWorkspace(workspace) {
        if (!Main.sessionMode.hasWorkspaces)
            return;

        if (!workspace.active)
            workspace.activate(global.get_current_time());
    }

    actionMoveWindow(window, workspace) {
        if (!Main.sessionMode.hasWorkspaces)
            return;

        if (!workspace.active) {
            // This won't have any effect for "always sticky" windows
            // (like desktop windows or docks)

            this._workspaceAnimation.movingWindow = window;
            window.change_workspace(workspace);

            global.display.clear_mouse_mode();
            workspace.activate_with_focus(window, global.get_current_time());
        }
    }

    handleWorkspaceScroll(event) {
        if (!this._canScroll)
            return Clutter.EVENT_PROPAGATE;

        if (event.type() !== Clutter.EventType.SCROLL)
            return Clutter.EVENT_PROPAGATE;

        const direction = event.get_scroll_direction();
        if (direction === Clutter.ScrollDirection.SMOOTH)
            return Clutter.EVENT_PROPAGATE;

        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const rtl = Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
        const activeWs = workspaceManager.get_active_workspace();
        let ws;
        switch (direction) {
        case Clutter.ScrollDirection.UP:
            if (vertical)
                ws = activeWs.get_neighbor(Meta.MotionDirection.UP);
            else if (rtl)
                ws = activeWs.get_neighbor(Meta.MotionDirection.RIGHT);
            else
                ws = activeWs.get_neighbor(Meta.MotionDirection.LEFT);
            break;
        case Clutter.ScrollDirection.DOWN:
            if (vertical)
                ws = activeWs.get_neighbor(Meta.MotionDirection.DOWN);
            else if (rtl)
                ws = activeWs.get_neighbor(Meta.MotionDirection.LEFT);
            else
                ws = activeWs.get_neighbor(Meta.MotionDirection.RIGHT);
            break;
        case Clutter.ScrollDirection.LEFT:
            ws = activeWs.get_neighbor(Meta.MotionDirection.LEFT);
            break;
        case Clutter.ScrollDirection.RIGHT:
            ws = activeWs.get_neighbor(Meta.MotionDirection.RIGHT);
            break;
        default:
            return Clutter.EVENT_PROPAGATE;
        }
        this.actionMoveWorkspace(ws);

        this._canScroll = false;
        GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            SCROLL_TIMEOUT_TIME, () => {
                this._canScroll = true;
                return GLib.SOURCE_REMOVE;
            });

        return Clutter.EVENT_STOP;
    }

    _confirmDisplayChange() {
        let dialog = new DisplayChangeDialog(this._shellwm);
        dialog.open();
    }

    _createCloseDialog(shellwm, window) {
        return new CloseDialog.CloseDialog(window);
    }

    _createInhibitShortcutsDialog(shellwm, window) {
        return new InhibitShortcutsDialog.InhibitShortcutsDialog(window);
    }

    _showResizePopup(display, show, rect, displayW, displayH) {
        if (show) {
            if (!this._resizePopup)
                this._resizePopup = new ResizePopup();

            this._resizePopup.set(rect, displayW, displayH);
        } else {
            if (!this._resizePopup)
                return;

            this._resizePopup.destroy();
            this._resizePopup = null;
        }
    }
};
