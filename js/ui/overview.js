// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Overview */

const { Clutter, GLib, GObject, Meta, Pango, Shell, St } = imports.gi;
const Signals = imports.signals;

const Background = imports.ui.background;
const DND = imports.ui.dnd;
const LayoutManager = imports.ui.layout;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const OverviewControls = imports.ui.overviewControls;
const Params = imports.misc.params;
const ViewSelector = imports.ui.viewSelector;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

// Time for initial animation going into Overview mode
var ANIMATION_TIME = 250;

// Must be less than ANIMATION_TIME, since we switch to
// or from the overview completely after ANIMATION_TIME,
// and don't want the shading animation to get cut off
var SHADE_ANIMATION_TIME = 200;

var DND_WINDOW_SWITCH_TIMEOUT = 750;

var OVERVIEW_ACTIVATION_TIMEOUT = 0.5;

var NO_WINDOWS_OPEN_DIALOG_TIMEOUT = 2000; // ms

var ShellInfo = class {
    constructor() {
        this._source = null;
        this._undoCallback = null;
        this._destroyCallback = null;
    }

    _onDestroy() {
        if (this._destroyCallback)
            this._destroyCallback();

        this._destroyCallback = null;
    }

    _onUndoClicked() {
        if (this._undoCallback)
            this._undoCallback();
        this._undoCallback = null;

        if (this._source)
            this._source.destroy();
    }

    setMessage(text, options) {
        options = Params.parse(options, {
            undoCallback: null,
            forFeedback: false,
            destroyCallback: null,
        });

        let undoCallback = options.undoCallback;
        let forFeedback = options.forFeedback;
        let destroyCallback = options.destroyCallback;

        if (this._source == null) {
            this._source = new MessageTray.SystemNotificationSource();
            this._source.connect('destroy', () => {
                this._source = null;
            });
            Main.messageTray.add(this._source);
        }

        let notification = null;
        if (this._source.notifications.length == 0) {
            notification = new MessageTray.Notification(this._source, text, null);
            notification.setTransient(true);
            notification.setForFeedback(forFeedback);
        } else {
            // as we reuse the notification, ensure that the previous _destroyCallback() is called
            if (this._destroyCallback)
                this._destroyCallback();

            notification = this._source.notifications[0];
            notification.update(text, null, { clear: true });
        }

        this._destroyCallback = destroyCallback;
        notification.connect('destroy', this._onDestroy.bind(this));

        this._undoCallback = undoCallback;
        if (undoCallback)
            notification.addAction(_("Undo"), this._onUndoClicked.bind(this));

        this._source.showNotification(notification);
    }
};

var NoWindowsDialog = GObject.registerClass(
class NoWindowsDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({
            styleClass: 'prompt-dialog',
            shellReactive: true,
            destroyOnClose: false,
        });

        this._timeoutId = 0;

        let descriptionLabel = new St.Label({
            style_class: 'prompt-dialog-headline headline',
            text: _('No apps are open'),
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        descriptionLabel.clutter_text.line_wrap = true;
        descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        this.contentLayout.add_child(descriptionLabel);

        this.connect('key-press-event', () => {
            this.close(global.get_current_time());
            return Clutter.EVENT_PROPAGATE;
        });
    }

    popup() {
        if (this._timeoutId !== 0)
            GLib.source_remove(this._timeoutId);

        this._timeoutId =
            GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                NO_WINDOWS_OPEN_DIALOG_TIMEOUT,
                () => {
                    this.popdown();
                    return GLib.SOURCE_REMOVE;
                });
        this.open(global.get_current_time());
    }

    popdown() {
        if (this._timeoutId !== 0) {
            GLib.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }
        this.close(global.get_current_time());
    }
});

var OverviewActor = GObject.registerClass(
class OverviewActor extends St.BoxLayout {
    _init() {
        super._init({
            name: 'overview',
            /* Translators: This is the main view to select
                activities. See also note for "Activities" string. */
            accessible_name: _("Overview"),
            vertical: true,
        });
    }

    init() {
        this.add_constraint(new LayoutManager.MonitorConstraint({ primary: true }));

        this._controls = new OverviewControls.ControlsManager();

        // Add our same-line elements after the search entry
        this.add_child(this._controls);

        // Add a clone of the panel to the overview so spacing and such is
        // automatic
        let panelGhost = new St.Bin({
            child: new Clutter.Clone({ source: Main.panel }),
            reactive: false,
            opacity: 0,
        });
        this.add_actor(panelGhost);
    }

    get searchEntry() {
        return this._controls.viewSelector.searchEntry;
    }

    get viewSelector() {
        return this._controls.viewSelector;
    }
});

var Overview = class {
    constructor() {
        this._initCalled = false;
        this._visible = false;

        this._noWindowsDialog = new NoWindowsDialog();

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    get viewSelector() {
        return this._overview.viewSelector;
    }

    get animationInProgress() {
        return this._animationInProgress;
    }

    get visible() {
        return this._visible;
    }

    get visibleTarget() {
        return this._visibleTarget;
    }

    _createOverview() {
        if (this._overview)
            return;

        if (this.isDummy)
            return;

        // The main Background actors are inside global.window_group which are
        // hidden when displaying the overview, so we create a new
        // one. Instances of this class share a single CoglTexture behind the
        // scenes which allows us to show the background with different
        // rendering options without duplicating the texture data.
        this._backgroundGroup = new Meta.BackgroundGroup({ reactive: true });
        Main.layoutManager.overviewGroup.add_child(this._backgroundGroup);
        this._bgManagers = [];

        this._desktopFade = new St.Widget();
        Main.layoutManager.overviewGroup.add_child(this._desktopFade);

        this._activationTime = 0;

        this._visible = false;          // animating to overview, in overview, animating out
        this._shown = false;            // show() and not hide()
        this._toggleToHidden = false;   // Whether to hide the overview when either toggle function is called
        this._targetPage = null;        // do we have a target page to animate to?
        this._modal = false;            // have a modal grab
        this._animationInProgress = false;
        this._visibleTarget = false;

        // During transitions, we raise this to the top to avoid having the overview
        // area be reactive; it causes too many issues such as double clicks on
        // Dash elements, or mouseover handlers in the workspaces.
        this._coverPane = new Clutter.Actor({ opacity: 0,
                                              reactive: true });
        Main.layoutManager.overviewGroup.add_child(this._coverPane);
        this._coverPane.connect('event', () => Clutter.EVENT_STOP);
        this._coverPane.hide();

        // XDND
        this._dragMonitor = {
            dragMotion: this._onDragMotion.bind(this),
        };


        Main.layoutManager.overviewGroup.connect('scroll-event',
                                                 this._onScrollEvent.bind(this));
        Main.xdndHandler.connect('drag-begin', this._onDragBegin.bind(this));
        Main.xdndHandler.connect('drag-end', this._onDragEnd.bind(this));

        global.display.connect('restacked', this._onRestacked.bind(this));

        this._windowSwitchTimeoutId = 0;
        this._windowSwitchTimestamp = 0;
        this._lastActiveWorkspaceIndex = -1;
        this._lastHoveredWindow = null;

        if (this._initCalled)
            this.init();
    }

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];

        for (let i = 0; i < Main.layoutManager.monitors.length; i++) {
            let bgManager = new Background.BackgroundManager({ container: this._backgroundGroup,
                                                               monitorIndex: i });
            this._bgManagers.push(bgManager);
        }
    }

    _sessionUpdated() {
        const { hasOverview } = Main.sessionMode;
        if (!hasOverview)
            this.hide();

        this.isDummy = !hasOverview;
        this._createOverview();
    }

    // The members we construct that are implemented in JS might
    // want to access the overview as Main.overview to connect
    // signal handlers and so forth. So we create them after
    // construction in this init() method.
    init() {
        this._initCalled = true;

        if (this.isDummy)
            return;

        // this._allMonitorsGroup is a simple actor that covers all monitors,
        // used to install actions that apply to all monitors
        this._allMonitorsGroup = new Clutter.Actor({
            reactive: true,
            x_expand: true,
            y_expand: true,
        });

        this._overview = new OverviewActor();
        this._overview.init();
        this._overview._delegate = this;
        this._allMonitorsGroup.add_actor(this._overview);

        Main.layoutManager.overviewGroup.add_child(this._allMonitorsGroup);

        this._shellInfo = new ShellInfo();

        this.viewSelector.connect('page-changed', this._onPageChanged.bind(this));
        Main.layoutManager.connect('startup-prepared', this._onStartupPrepared.bind(this));
        Main.layoutManager.connect('monitors-changed', this._relayout.bind(this));
        this._relayoutNoHide();
    }

    addSearchProvider(provider) {
        this.viewSelector.addSearchProvider(provider);
    }

    removeSearchProvider(provider) {
        this.viewSelector.removeSearchProvider(provider);
    }

    //
    // options:
    //  - undoCallback (function): the callback to be called if undo support is needed
    //  - forFeedback (boolean): whether the message is for direct feedback of a user action
    //
    setMessage(text, options) {
        if (this.isDummy)
            return;

        this._shellInfo.setMessage(text, options);
    }

    _onPageChanged() {
        this._toggleToHidden = false;

        // SideComponent hooks on this signal but can't connect directly to
        // viewSelector since it won't be created at the time the component
        // is enabled, so rely on the overview and re-issue it from here.
        this.emit('page-changed');
    }

    _onDragBegin() {
        this._inXdndDrag = true;

        DND.addDragMonitor(this._dragMonitor);
        // Remember the workspace we started from
        let workspaceManager = global.workspace_manager;
        this._lastActiveWorkspaceIndex = workspaceManager.get_active_workspace_index();
    }

    _onDragEnd(time) {
        this._inXdndDrag = false;

        // In case the drag was canceled while in the overview
        // we have to go back to where we started and hide
        // the overview
        if (this._shown) {
            let workspaceManager = global.workspace_manager;
            workspaceManager.get_workspace_by_index(this._lastActiveWorkspaceIndex).activate(time);
            this.hide();
        }
        this._resetWindowSwitchTimeout();
        this._lastHoveredWindow = null;
        DND.removeDragMonitor(this._dragMonitor);
        this.endItemDrag();
    }

    _resetWindowSwitchTimeout() {
        if (this._windowSwitchTimeoutId != 0) {
            GLib.source_remove(this._windowSwitchTimeoutId);
            this._windowSwitchTimeoutId = 0;
        }
    }

    _onDragMotion(dragEvent) {
        let targetIsWindow = dragEvent.targetActor &&
                             dragEvent.targetActor._delegate &&
                             dragEvent.targetActor._delegate.metaWindow &&
                             !(dragEvent.targetActor._delegate instanceof WorkspaceThumbnail.WindowClone);

        this._windowSwitchTimestamp = global.get_current_time();

        if (targetIsWindow &&
            dragEvent.targetActor._delegate.metaWindow == this._lastHoveredWindow)
            return DND.DragMotionResult.CONTINUE;

        this._lastHoveredWindow = null;

        this._resetWindowSwitchTimeout();

        if (targetIsWindow) {
            this._lastHoveredWindow = dragEvent.targetActor._delegate.metaWindow;
            this._windowSwitchTimeoutId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                DND_WINDOW_SWITCH_TIMEOUT,
                () => {
                    this._windowSwitchTimeoutId = 0;
                    Main.activateWindow(dragEvent.targetActor._delegate.metaWindow,
                                        this._windowSwitchTimestamp);
                    this.hide();
                    this._lastHoveredWindow = null;
                    return GLib.SOURCE_REMOVE;
                });
            GLib.Source.set_name_by_id(this._windowSwitchTimeoutId, '[gnome-shell] Main.activateWindow');
        }

        return DND.DragMotionResult.CONTINUE;
    }

    _onScrollEvent(actor, event) {
        this.emit('scroll-event', event);
        return Clutter.EVENT_PROPAGATE;
    }

    addAction(action, isPrimary = true) {
        if (this.isDummy)
            return;

        if (isPrimary)
            this._overview.add_action(action);
        else
            this._allMonitorsGroup.add_action(action);
    }

    _getDesktopClone() {
        let windows = global.get_window_actors().filter(
            w => w.meta_window.get_window_type() == Meta.WindowType.DESKTOP
        );
        if (windows.length == 0)
            return null;

        let window = windows[0];
        let clone = new Clutter.Clone({ source: window,
                                        x: window.x, y: window.y });
        clone.source.connect('destroy', () => {
            clone.destroy();
        });
        return clone;
    }

    _relayout() {
        // To avoid updating the position and size of the workspaces
        // we just hide the overview. The positions will be updated
        // when it is next shown.
        this.hide();

        this._relayoutNoHide();
    }

    _relayoutNoHide() {
        this._coverPane.set_position(0, 0);
        this._coverPane.set_size(global.screen_width, global.screen_height);

        this._updateBackgrounds();
    }

    _onRestacked() {
        let stack = global.get_window_actors();
        let stackIndices = {};

        for (let i = 0; i < stack.length; i++) {
            // Use the stable sequence for an integer to use as a hash key
            stackIndices[stack[i].get_meta_window().get_stable_sequence()] = i;
        }

        this.emit('windows-restacked', stackIndices);
    }

    beginItemDrag(_source) {
        this.emit('item-drag-begin');
        this._inItemDrag = true;
    }

    cancelledItemDrag(_source) {
        this.emit('item-drag-cancelled');
    }

    endItemDrag(_source) {
        if (!this._inItemDrag)
            return;
        this.emit('item-drag-end');
        this._inItemDrag = false;
    }

    beginWindowDrag(window) {
        this.emit('window-drag-begin', window);
        this._inWindowDrag = true;
    }

    cancelledWindowDrag(window) {
        this.emit('window-drag-cancelled', window);
    }

    endWindowDrag(window) {
        if (!this._inWindowDrag)
            return;
        this.emit('window-drag-end', window);
        this._inWindowDrag = false;
    }

    focusSearch() {
        this.show();
        this.viewSelector.focusSearch();
    }

    _showOrSwitchPage(page) {
        if (this.visible) {
            this.viewSelector.setActivePage(page);
        } else {
            this._targetPage = page;
            this.show();
        }
    }

    showApps() {
        if (this.isDummy)
            return;

        this._showOrSwitchPage(ViewSelector.ViewPage.APPS);
    }

    showWindows() {
        if (this.isDummy)
            return;

        this._showOrSwitchPage(ViewSelector.ViewPage.WINDOWS);
    }

    _onStartupPrepared() {
        this.showApps();
    }

    fadeInDesktop() {
        this._desktopFade.opacity = 0;
        this._desktopFade.show();
        this._desktopFade.ease({
            opacity: 255,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: ANIMATION_TIME,
        });
    }

    fadeOutDesktop() {
        if (!this._desktopFade.get_n_children()) {
            let clone = this._getDesktopClone();
            if (!clone)
                return;

            this._desktopFade.add_child(clone);
        }

        this._desktopFade.opacity = 255;
        this._desktopFade.show();
        this._desktopFade.ease({
            opacity: 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: ANIMATION_TIME,
        });
    }

    toggleApps() {
        if (this.isDummy)
            return;

        if (!this.visible ||
            this.viewSelector.getActivePage() !== ViewSelector.ViewPage.APPS) {
            this.showApps();
            return;
        }

        if (!Main.workspaceMonitor.hasActiveWindows) {
            this._noWindowsDialog.popup();
            return;
        }

        if (!Main.workspaceMonitor.hasVisibleWindows) {
            // There are active windows but all of them are hidden, so activate
            // the most recently used one before hiding the overview.
            let appSystem = Shell.AppSystem.get_default();
            let runningApps = appSystem.get_running();
            if (runningApps.length > 0)
                runningApps[0].activate();
        }

        // Toggle to the currently open window
        this.hide();
    }

    toggleWindows() {
        if (this.isDummy)
            return;

        if (!this.visible) {
            this.showWindows();
            return;
        }

        if (!Main.workspaceMonitor.hasActiveWindows) {
            this._noWindowsDialog.popup();
            return;
        }

        if (this.viewSelector.getActivePage() !== ViewSelector.ViewPage.WINDOWS) {
            this.showWindows();
            return;
        }

        if (!this._toggleToHidden) {
            this.showApps();
            return;
        }

        if (!Main.workspaceMonitor.hasVisibleWindows) {
            // There are active windows but all of them are
            // hidden, so we get back to show the icons grid.
            this.showApps();
            return;
        }

        // Toggle to the currently open window
        this.hide();
    }

    // Checks if the Activities button is currently sensitive to
    // clicks. The first call to this function within the
    // OVERVIEW_ACTIVATION_TIMEOUT time of the hot corner being
    // triggered will return false. This avoids opening and closing
    // the overview if the user both triggered the hot corner and
    // clicked the Activities button.
    shouldToggleByCornerOrButton() {
        if (this._animationInProgress)
            return false;
        if (this._inItemDrag || this._inWindowDrag)
            return false;
        if (!this._activationTime ||
            GLib.get_monotonic_time() / GLib.USEC_PER_SEC - this._activationTime > OVERVIEW_ACTIVATION_TIMEOUT)
            return true;
        return false;
    }

    _syncGrab() {
        // We delay grab changes during animation so that when removing the
        // overview we don't have a problem with the release of a press/release
        // going to an application.
        if (this._animationInProgress)
            return true;

        if (this._shown) {
            let shouldBeModal = !this._inXdndDrag;
            if (shouldBeModal && !this._modal) {
                let actionMode = Shell.ActionMode.OVERVIEW;
                if (Main.pushModal(this._overview, { actionMode })) {
                    this._modal = true;
                } else {
                    this.hide();
                    return false;
                }
            }
        } else {
            // eslint-disable-next-line no-lonely-if
            if (this._modal) {
                Main.popModal(this._overview);
                this._modal = false;
            }
        }
        return true;
    }

    // show:
    //
    // Animates the overview visible and grabs mouse and keyboard input
    show() {
        if (this.isDummy)
            return;
        if (this._shown)
            return;
        this._shown = true;

        if (!this._syncGrab())
            return;

        Main.layoutManager.showOverview();
        this._animateVisible();
    }


    _animateVisible() {
        if (this._visible || this._animationInProgress)
            return;

        this._visible = true;
        this._animationInProgress = true;
        this._visibleTarget = true;
        this._activationTime = GLib.get_monotonic_time() / GLib.USEC_PER_SEC;

        Meta.disable_unredirect_for_display(global.display);

        if (!this._targetPage)
            this._targetPage = ViewSelector.ViewPage.WINDOWS;

        this.viewSelector.show(this._targetPage);
        this._targetPage = null;

        // Since the overview is just becoming visible, we should toggle back
        // the hidden state
        this._toggleToHidden = true;

        this._overview.opacity = 0;
        this._overview.ease({
            opacity: 255,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: ANIMATION_TIME,
            onComplete: () => this._showDone(),
        });

        Main.layoutManager.overviewGroup.set_child_above_sibling(
            this._coverPane, null);
        this._coverPane.show();
        this.emit('showing');
    }

    _showDone() {
        this._animationInProgress = false;
        this._desktopFade.hide();
        this._coverPane.hide();

        this.emit('shown');
        // Handle any calls to hide* while we were showing
        if (!this._shown)
            this._animateNotVisible();

        this._syncGrab();
        global.sync_pointer();
    }

    // hide:
    //
    // Reverses the effect of show()
    hide() {
        if (this.isDummy)
            return;

        if (!this._shown)
            return;

        let event = Clutter.get_current_event();
        if (event) {
            let type = event.type();
            let button = type == Clutter.EventType.BUTTON_PRESS ||
                          type == Clutter.EventType.BUTTON_RELEASE;
            let ctrl = (event.get_state() & Clutter.ModifierType.CONTROL_MASK) != 0;
            if (button && ctrl)
                return;
        }

        this._shown = false;

        // Hide the 'No windows dialog' in case it is open
        this._noWindowsDialog.popdown();

        this._animateNotVisible();
        this._syncGrab();
    }

    _animateNotVisible() {
        if (!this._visible || this._animationInProgress)
            return;

        this._animationInProgress = true;
        this._visibleTarget = false;

        this.viewSelector.animateFromOverview();

        // Make other elements fade out.
        this._overview.ease({
            opacity: 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: ANIMATION_TIME,
            onComplete: () => this._hideDone(),
        });

        Main.layoutManager.overviewGroup.set_child_above_sibling(
            this._coverPane, null);
        this._coverPane.show();
        this.emit('hiding');
    }

    _hideDone() {
        // Re-enable unredirection
        Meta.enable_unredirect_for_display(global.display);

        this.viewSelector.hide();
        this._desktopFade.hide();
        this._coverPane.hide();

        this._visible = false;
        this._animationInProgress = false;

        this.emit('hidden');
        // Handle any calls to show* while we were hiding
        if (this._shown)
            this._animateVisible();
        else
            Main.layoutManager.hideOverview();

        this._syncGrab();
    }

    toggle() {
        if (this.isDummy)
            return;

        if (this._visible)
            this.hide();
        else
            this.show();
    }

    getActivePage() {
        return this.viewSelector.getActivePage();
    }

    get searchEntry() {
        return this._overview.searchEntry;
    }
};
Signals.addSignalMethods(Overview.prototype);
