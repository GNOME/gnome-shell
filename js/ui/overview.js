// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Gdk = imports.gi.Gdk;

const Background = imports.ui.background;
const DND = imports.ui.dnd;
const LayoutManager = imports.ui.layout;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const OverviewControls = imports.ui.overviewControls;
const Panel = imports.ui.panel;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

// Time for initial animation going into Overview mode
var ANIMATION_TIME = 0.25;

// Must be less than ANIMATION_TIME, since we switch to
// or from the overview completely after ANIMATION_TIME,
// and don't want the shading animation to get cut off
var SHADE_ANIMATION_TIME = .20;

var DND_WINDOW_SWITCH_TIMEOUT = 750;

var OVERVIEW_ACTIVATION_TIMEOUT = 0.5;

var ShellInfo = new Lang.Class({
    Name: 'ShellInfo',

    _init() {
        this._source = null;
        this._undoCallback = null;
    },

    _onUndoClicked() {
        if (this._undoCallback)
            this._undoCallback();
        this._undoCallback = null;

        if (this._source)
            this._source.destroy();
    },

    setMessage(text, options) {
        options = Params.parse(options, { undoCallback: null,
                                          forFeedback: false
                                        });

        let undoCallback = options.undoCallback;
        let forFeedback = options.forFeedback;

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
            notification = this._source.notifications[0];
            notification.update(text, null, { clear: true });
        }

        this._undoCallback = undoCallback;
        if (undoCallback)
            notification.addAction(_("Undo"), this._onUndoClicked.bind(this));

        this._source.notify(notification);
    }
});

var Overview = new Lang.Class({
    Name: 'Overview',

    _init() {
        this._overviewCreated = false;
        this._initCalled = false;

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    },

    _createOverview() {
        if (this._overviewCreated)
            return;

        if (this.isDummy)
            return;

        this._overviewCreated = true;

        this._overview = new St.BoxLayout({ name: 'overview',
                                            /* Translators: This is the main view to select
                                               activities. See also note for "Activities" string. */
                                            accessible_name: _("Overview"),
                                            vertical: true });
        this._overview.add_constraint(new LayoutManager.MonitorConstraint({ primary: true }));
        this._overview._delegate = this;

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

        this.visible = false;           // animating to overview, in overview, animating out
        this._shown = false;            // show() and not hide()
        this._modal = false;            // have a modal grab
        this.animationInProgress = false;
        this.visibleTarget = false;

        // During transitions, we raise this to the top to avoid having the overview
        // area be reactive; it causes too many issues such as double clicks on
        // Dash elements, or mouseover handlers in the workspaces.
        this._coverPane = new Clutter.Actor({ opacity: 0,
                                              reactive: true });
        Main.layoutManager.overviewGroup.add_child(this._coverPane);
        this._coverPane.connect('event', () => Clutter.EVENT_STOP);

        Main.layoutManager.overviewGroup.add_child(this._overview);

        this._coverPane.hide();

        // XDND
        this._dragMonitor = {
            dragMotion: this._onDragMotion.bind(this)
        };


        Main.layoutManager.overviewGroup.connect('scroll-event',
                                                 this._onScrollEvent.bind(this));
        Main.xdndHandler.connect('drag-begin', this._onDragBegin.bind(this));
        Main.xdndHandler.connect('drag-end', this._onDragEnd.bind(this));

        global.screen.connect('restacked', this._onRestacked.bind(this));

        this._windowSwitchTimeoutId = 0;
        this._windowSwitchTimestamp = 0;
        this._lastActiveWorkspaceIndex = -1;
        this._lastHoveredWindow = null;
        this._needsFakePointerEvent = false;

        if (this._initCalled)
            this.init();
    },

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];

        for (let i = 0; i < Main.layoutManager.monitors.length; i++) {
            let bgManager = new Background.BackgroundManager({ container: this._backgroundGroup,
                                                               monitorIndex: i,
                                                               vignette: true });
            this._bgManagers.push(bgManager);
        }
    },

    _unshadeBackgrounds() {
        let backgrounds = this._backgroundGroup.get_children();
        for (let i = 0; i < backgrounds.length; i++) {
            Tweener.addTween(backgrounds[i],
                             { brightness: 1.0,
                               vignette_sharpness: 0.0,
                               time: SHADE_ANIMATION_TIME,
                               transition: 'easeOutQuad'
                             });
        }
    },

    _shadeBackgrounds() {
        let backgrounds = this._backgroundGroup.get_children();
        for (let i = 0; i < backgrounds.length; i++) {
            Tweener.addTween(backgrounds[i],
                             { brightness: Lightbox.VIGNETTE_BRIGHTNESS,
                               vignette_sharpness: Lightbox.VIGNETTE_SHARPNESS,
                               time: SHADE_ANIMATION_TIME,
                               transition: 'easeOutQuad'
                             });
        }
    },

    _sessionUpdated() {
        this.isDummy = !Main.sessionMode.hasOverview;
        this._createOverview();
    },

    // The members we construct that are implemented in JS might
    // want to access the overview as Main.overview to connect
    // signal handlers and so forth. So we create them after
    // construction in this init() method.
    init() {
        this._initCalled = true;

        if (this.isDummy)
            return;

        this._shellInfo = new ShellInfo();

        // Add a clone of the panel to the overview so spacing and such is
        // automatic
        this._panelGhost = new St.Bin({ child: new Clutter.Clone({ source: Main.panel.actor }),
                                        reactive: false,
                                        opacity: 0 });
        this._overview.add_actor(this._panelGhost);

        this._searchEntry = new St.Entry({ style_class: 'search-entry',
                                           /* Translators: this is the text displayed
                                              in the search entry when no search is
                                              active; it should not exceed ~30
                                              characters. */
                                           hint_text: _("Type to search…"),
                                           track_hover: true,
                                           can_focus: true });
        this._searchEntryBin = new St.Bin({ child: this._searchEntry,
                                            x_align: St.Align.MIDDLE });
        this._overview.add_actor(this._searchEntryBin);

        // Create controls
        this._controls = new OverviewControls.ControlsManager(this._searchEntry);
        this._dash = this._controls.dash;
        this.viewSelector = this._controls.viewSelector;

        // Add our same-line elements after the search entry
        this._overview.add(this._controls.actor, { y_fill: true, expand: true });

        // TODO - recalculate everything when desktop size changes
        this.dashIconSize = this._dash.iconSize;
        this._dash.connect('icon-size-changed', () => {
            this.dashIconSize = this._dash.iconSize;
        });

        Main.layoutManager.connect('monitors-changed', this._relayout.bind(this));
        this._relayout();
    },

    addSearchProvider(provider) {
        this.viewSelector.addSearchProvider(provider);
    },

    removeSearchProvider(provider) {
        this.viewSelector.removeSearchProvider(provider);
    },

    //
    // options:
    //  - undoCallback (function): the callback to be called if undo support is needed
    //  - forFeedback (boolean): whether the message is for direct feedback of a user action
    //
    setMessage(text, options) {
        if (this.isDummy)
            return;

        this._shellInfo.setMessage(text, options);
    },

    _onDragBegin() {
        this._inXdndDrag = true;

        DND.addDragMonitor(this._dragMonitor);
        // Remember the workspace we started from
        this._lastActiveWorkspaceIndex = global.screen.get_active_workspace_index();
    },

    _onDragEnd(time) {
        this._inXdndDrag = false;

        // In case the drag was canceled while in the overview
        // we have to go back to where we started and hide
        // the overview
        if (this._shown) {
            global.screen.get_workspace_by_index(this._lastActiveWorkspaceIndex).activate(time);
            this.hide();
        }
        this._resetWindowSwitchTimeout();
        this._lastHoveredWindow = null;
        DND.removeDragMonitor(this._dragMonitor);
        this.endItemDrag();
    },

    _resetWindowSwitchTimeout() {
        if (this._windowSwitchTimeoutId != 0) {
            Mainloop.source_remove(this._windowSwitchTimeoutId);
            this._windowSwitchTimeoutId = 0;
            this._needsFakePointerEvent = false;
        }
    },

    _fakePointerEvent() {
        let display = Gdk.Display.get_default();
        let deviceManager = display.get_device_manager();
        let pointer = deviceManager.get_client_pointer();
        let [screen, pointerX, pointerY] = pointer.get_position();

        pointer.warp(screen, pointerX, pointerY);
    },

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
            this._windowSwitchTimeoutId = Mainloop.timeout_add(DND_WINDOW_SWITCH_TIMEOUT,
                () => {
                    this._windowSwitchTimeoutId = 0;
                    this._needsFakePointerEvent = true;
                    Main.activateWindow(dragEvent.targetActor._delegate.metaWindow,
                                        this._windowSwitchTimestamp);
                    this.hide();
                    this._lastHoveredWindow = null;
                    return GLib.SOURCE_REMOVE;
                });
            GLib.Source.set_name_by_id(this._windowSwitchTimeoutId, '[gnome-shell] Main.activateWindow');
        }

        return DND.DragMotionResult.CONTINUE;
    },

    _onScrollEvent(actor, event) {
        this.emit('scroll-event', event);
        return Clutter.EVENT_PROPAGATE;
    },

    addAction(action) {
        if (this.isDummy)
            return;

        this._backgroundGroup.add_action(action);
    },

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
    },

    _relayout() {
        // To avoid updating the position and size of the workspaces
        // we just hide the overview. The positions will be updated
        // when it is next shown.
        this.hide();

        if (!Main.layoutManager.primaryMonitor)
            return;

        this._coverPane.set_position(0, 0);
        this._coverPane.set_size(global.screen_width, global.screen_height);

        this._updateBackgrounds();
    },

    _onRestacked() {
        let stack = global.get_window_actors();
        let stackIndices = {};

        for (let i = 0; i < stack.length; i++) {
            // Use the stable sequence for an integer to use as a hash key
            stackIndices[stack[i].get_meta_window().get_stable_sequence()] = i;
        }

        this.emit('windows-restacked', stackIndices);
    },

    beginItemDrag(source) {
        this.emit('item-drag-begin');
        this._inItemDrag = true;
    },

    cancelledItemDrag(source) {
        this.emit('item-drag-cancelled');
    },

    endItemDrag(source) {
        if (!this._inItemDrag)
            return;
        this.emit('item-drag-end');
        this._inItemDrag = false;
    },

    beginWindowDrag(window) {
        this.emit('window-drag-begin', window);
        this._inWindowDrag = true;
    },

    cancelledWindowDrag(window) {
        this.emit('window-drag-cancelled', window);
    },

    endWindowDrag(window) {
        if (!this._inWindowDrag)
            return;
        this.emit('window-drag-end', window);
        this._inWindowDrag = false;
    },

    focusSearch() {
        this.show();
        this._searchEntry.grab_key_focus();
    },

    fadeInDesktop() {
            this._desktopFade.opacity = 0;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 255,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad' });
    },

    fadeOutDesktop() {
        if (!this._desktopFade.get_n_children()) {
            let clone = this._getDesktopClone();
            if (!clone)
                return;

            this._desktopFade.add_child(clone);
        }

        this._desktopFade.opacity = 255;
        this._desktopFade.show();
        Tweener.addTween(this._desktopFade,
                         { opacity: 0,
                           time: ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
    },

    // Checks if the Activities button is currently sensitive to
    // clicks. The first call to this function within the
    // OVERVIEW_ACTIVATION_TIMEOUT time of the hot corner being
    // triggered will return false. This avoids opening and closing
    // the overview if the user both triggered the hot corner and
    // clicked the Activities button.
    shouldToggleByCornerOrButton() {
        if (this.animationInProgress)
            return false;
        if (this._inItemDrag || this._inWindowDrag)
            return false;
        if (this._activationTime == 0 ||
            GLib.get_monotonic_time() / GLib.USEC_PER_SEC - this._activationTime > OVERVIEW_ACTIVATION_TIMEOUT)
            return true;
        return false;
    },

    _syncGrab() {
        // We delay grab changes during animation so that when removing the
        // overview we don't have a problem with the release of a press/release
        // going to an application.
        if (this.animationInProgress)
            return true;

        if (this._shown) {
            let shouldBeModal = !this._inXdndDrag;
            if (shouldBeModal) {
                if (!this._modal) {
                    if (Main.pushModal(this._overview,
                                       { actionMode: Shell.ActionMode.OVERVIEW })) {
                        this._modal = true;
                    } else {
                        this.hide();
                        return false;
                    }
                }
            }
        } else {
            if (this._modal) {
                Main.popModal(this._overview);
                this._modal = false;
            }
        }
        return true;
    },

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
    },


    _animateVisible() {
        if (this.visible || this.animationInProgress)
            return;

        this.visible = true;
        this.animationInProgress = true;
        this.visibleTarget = true;
        this._activationTime = GLib.get_monotonic_time() / GLib.USEC_PER_SEC;

        Meta.disable_unredirect_for_screen(global.screen);
        this.viewSelector.show();

        this._overview.opacity = 0;
        Tweener.addTween(this._overview,
                         { opacity: 255,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._showDone,
                           onCompleteScope: this
                         });
        this._shadeBackgrounds();

        this._coverPane.raise_top();
        this._coverPane.show();
        this.emit('showing');
    },

    _showDone() {
        this.animationInProgress = false;
        this._desktopFade.hide();
        this._coverPane.hide();

        this.emit('shown');
        // Handle any calls to hide* while we were showing
        if (!this._shown)
            this._animateNotVisible();

        this._syncGrab();
        global.sync_pointer();
    },

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
            let button = (type == Clutter.EventType.BUTTON_PRESS ||
                          type == Clutter.EventType.BUTTON_RELEASE);
            let ctrl = (event.get_state() & Clutter.ModifierType.CONTROL_MASK) != 0;
            if (button && ctrl)
                return;
        }

        this._shown = false;

        this._animateNotVisible();
        this._syncGrab();
    },


    _animateNotVisible() {
        if (!this.visible || this.animationInProgress)
            return;

        this.animationInProgress = true;
        this.visibleTarget = false;

        this.viewSelector.animateFromOverview();

        // Make other elements fade out.
        Tweener.addTween(this._overview,
                         { opacity: 0,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });
        this._unshadeBackgrounds();

        this._coverPane.raise_top();
        this._coverPane.show();
        this.emit('hiding');
    },

    _hideDone() {
        // Re-enable unredirection
        Meta.enable_unredirect_for_screen(global.screen);

        this.viewSelector.hide();
        this._desktopFade.hide();
        this._coverPane.hide();

        this.visible = false;
        this.animationInProgress = false;

        this.emit('hidden');
        // Handle any calls to show* while we were hiding
        if (this._shown)
            this._animateVisible();
        else
            Main.layoutManager.hideOverview();

        this._syncGrab();

        // Fake a pointer event if requested
        if (this._needsFakePointerEvent) {
            this._fakePointerEvent();
            this._needsFakePointerEvent = false;
        }
    },

    toggle() {
        if (this.isDummy)
            return;

        if (this.visible)
            this.hide();
        else
            this.show();
    },

    getShowAppsButton() {
        return this._dash.showAppsButton;
    }
});
Signals.addSignalMethods(Overview.prototype);
