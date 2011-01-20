/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Meta = imports.gi.Meta;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;
const Gdk = imports.gi.Gdk;

const AppDisplay = imports.ui.appDisplay;
const Dash = imports.ui.dash;
const DND = imports.ui.dnd;
const DocDisplay = imports.ui.docDisplay;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Panel = imports.ui.panel;
const PlaceDisplay = imports.ui.placeDisplay;
const Tweener = imports.ui.tweener;
const ViewSelector = imports.ui.viewSelector;
const WorkspacesView = imports.ui.workspacesView;

// Time for initial animation going into Overview mode
const ANIMATION_TIME = 0.25;

// We split the screen vertically between the dash and the view selector.
const DASH_SPLIT_FRACTION = 0.1;

const DND_WINDOW_SWITCH_TIMEOUT = 1250;

function ShellInfo() {
    this._init();
}

ShellInfo.prototype = {
    _init: function() {
        this._source = null;
        this._undoCallback = null;
    },

    _onUndoClicked: function() {
        if (this._undoCallback)
            this._undoCallback();
        this._undoCallback = null;

        if (this._source)
            this._source.destroy();
    },

    setMessage: function(text, undoCallback, undoLabel) {
        if (this._source == null) {
            this._source = new MessageTray.SystemNotificationSource();
            this._source.connect('destroy', Lang.bind(this,
                function() {
                    this._source = null;
                }));
            Main.messageTray.add(this._source);
        }

        let notification = this._source.notification;
        if (notification == null)
            notification = new MessageTray.Notification(this._source, text, null);
        else
            notification.update(text, null, { clear: true });

        notification.setTransient(true);

        this._undoCallback = undoCallback;
        if (undoCallback) {
            notification.addButton('system-undo',
                                   undoLabel ? undoLabel : _("Undo"));
            notification.connect('action-invoked',
                                 Lang.bind(this, this._onUndoClicked));
        }

        this._source.notify(notification);
    }
};

function Overview() {
    this._init();
}

Overview.prototype = {
    _init : function() {
        // The actual global.background_actor is inside global.window_group,
        // which is hidden when displaying the overview, so we display a clone.
        this._background = new Clutter.Clone({ source: global.background_actor });
        this._background.hide();
        global.overlay_group.add_actor(this._background);

        this._desktopFade = new St.Bin();
        global.overlay_group.add_actor(this._desktopFade);

        this._spacing = 0;

        this._group = new St.Group({ name: 'overview' });
        this._group._delegate = this;
        this._group.connect('style-changed',
            Lang.bind(this, function() {
                let node = this._group.get_theme_node();
                let spacing = node.get_length('spacing');
                if (spacing != this._spacing) {
                    this._spacing = spacing;
                    this.relayout();
                }
            }));

        this.shellInfo = new ShellInfo();

        this._workspacesDisplay = null;

        this.visible = false;           // animating to overview, in overview, animating out
        this._shown = false;            // show() and not hide()
        this._shownTemporarily = false; // showTemporarily() and not hideTemporarily()
        this._modal = false;            // have a modal grab
        this.animationInProgress = false;
        this._hideInProgress = false;

        // During transitions, we raise this to the top to avoid having the overview
        // area be reactive; it causes too many issues such as double clicks on
        // Dash elements, or mouseover handlers in the workspaces.
        this._coverPane = new Clutter.Rectangle({ opacity: 0,
                                                  reactive: true });
        this._group.add_actor(this._coverPane);
        this._coverPane.connect('event', Lang.bind(this, function (actor, event) { return true; }));


        this._group.hide();
        global.overlay_group.add_actor(this._group);

        this.viewSelector = new ViewSelector.ViewSelector();
        this._group.add_actor(this.viewSelector.actor);

        this._workspacesDisplay = new WorkspacesView.WorkspacesDisplay();
        this.viewSelector.addViewTab(_("Windows"), this._workspacesDisplay.actor);

        let appView = new AppDisplay.AllAppDisplay();
        this.viewSelector.addViewTab(_("Applications"), appView.actor);

        // Default search providers
        this.viewSelector.addSearchProvider(new AppDisplay.AppSearchProvider());
        this.viewSelector.addSearchProvider(new AppDisplay.PrefsSearchProvider());
        this.viewSelector.addSearchProvider(new PlaceDisplay.PlaceSearchProvider());
        this.viewSelector.addSearchProvider(new DocDisplay.DocSearchProvider());

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash.Dash();
        this._group.add_actor(this._dash.actor);
        this._dash.actor.add_constraint(this.viewSelector.constrainY);
        this._dash.actor.add_constraint(this.viewSelector.constrainHeight);

        this._coverPane.lower_bottom();

        // XDND
        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };

        Main.xdndHandler.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        Main.xdndHandler.connect('drag-end', Lang.bind(this, this._onDragEnd));

        this._windowSwitchTimeoutId = 0;
        this._windowSwitchTimestamp = 0;
        this._lastActiveWorkspaceIndex = -1;
        this._lastHoveredWindow = null;
        this._needsFakePointerEvent = false;

        this.workspaces = null;
    },

    _onDragBegin: function() {
        DND.addDragMonitor(this._dragMonitor);
        // Remember the workspace we started from
        this._lastActiveWorkspaceIndex = global.screen.get_active_workspace_index();
    },

    _onDragEnd: function(time) {
        // In case the drag was canceled while in the overview
        // we have to go back to where we started and hide
        // the overview
        if (this._shownTemporarily)  {
            global.screen.get_workspace_by_index(this._lastActiveWorkspaceIndex).activate(time);
            this.hideTemporarily();
        }
        this._lastHoveredWindow = null;
        DND.removeMonitor(this._dragMonitor);
    },

    _fakePointerEvent: function() {
        let display = Gdk.Display.get_default();
        let deviceManager = display.get_device_manager();
        let pointer = deviceManager.get_client_pointer();
        let [screen, pointerX, pointerY] = pointer.get_position();

        pointer.warp(screen, pointerX, pointerY);
    },

    _onDragMotion: function(dragEvent) {
        let targetIsWindow = dragEvent.targetActor &&
                             dragEvent.targetActor._delegate &&
                             dragEvent.targetActor._delegate.metaWindow;

        if (targetIsWindow &&
            dragEvent.targetActor._delegate.metaWindow == this._lastHoveredWindow)
            return DND.DragMotionResult.CONTINUE;

        this._lastHoveredWindow = null;

        if (this._windowSwitchTimeoutId != 0) {
            Mainloop.source_remove(this._windowSwitchTimeoutId);
            this._windowSwitchTimeoutId = 0;
            this._needsFakePointerEvent = false;
        }

        if (targetIsWindow) {
            this._lastHoveredWindow = dragEvent.targetActor._delegate.metaWindow;
            this._windowSwitchTimestamp = global.get_current_time();
            this._windowSwitchTimeoutId = Mainloop.timeout_add(DND_WINDOW_SWITCH_TIMEOUT,
                                            Lang.bind(this, function() {
                                                this._needsFakePointerEvent = true;
                                                Main.activateWindow(dragEvent.targetActor._delegate.metaWindow,
                                                                    this._windowSwitchTimestamp);
                                                this.hideTemporarily();
                                                this._lastHoveredWindow = null;
                                            }));
        }

        return DND.DragMotionResult.CONTINUE;
    },

    _getDesktopClone: function() {
        let windows = global.get_window_actors().filter(function(w) {
            return w.meta_window.get_window_type() == Meta.WindowType.DESKTOP;
        });
        if (windows.length == 0)
            return null;

        let clone = new Clutter.Clone({ source: windows[0].get_texture() });
        clone.source.connect('destroy', Lang.bind(this, function() {
            clone.destroy();
        }));
        return clone;
    },

    relayout: function () {
        let primary = global.get_primary_monitor();
        let rtl = (St.Widget.get_default_direction () == St.TextDirection.RTL);

        let contentY = Panel.PANEL_HEIGHT;
        let contentHeight = primary.height - contentY - Main.messageTray.actor.height;

        this._group.set_position(primary.x, primary.y);
        this._group.set_size(primary.width, primary.height);

        this._coverPane.set_position(0, contentY);
        this._coverPane.set_size(primary.width, contentHeight);

        let viewWidth = (1.0 - DASH_SPLIT_FRACTION) * primary.width - this._spacing;
        let viewHeight = contentHeight - 2 * this._spacing;
        let viewY = contentY + this._spacing;
        let viewX = rtl ? 0
                        : Math.floor(DASH_SPLIT_FRACTION * primary.width) + this._spacing;

        // Set the dash's x position - y is handled by a constraint
        let dashX;
        if (rtl) {
            this._dash.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
            dashX = primary.width;
        } else {
            dashX = 0;
        }
        this._dash.actor.set_x(dashX);

        this.viewSelector.actor.set_position(viewX, viewY);
        this.viewSelector.actor.set_size(viewWidth, viewHeight);
    },

    //// Public methods ////

    beginItemDrag: function(source) {
        this.emit('item-drag-begin');
    },

    endItemDrag: function(source) {
        this.emit('item-drag-end');
    },

    beginWindowDrag: function(source) {
        this.emit('window-drag-begin');
    },

    endWindowDrag: function(source) {
        this.emit('window-drag-end');
    },

    // Returns the scale the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInScale : function() {
        return 1 / this.workspaces.getScale();
    },

    // Returns the position the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInPosition : function() {
        let [posX, posY] = this.workspaces.getActiveWorkspacePosition();
        let scale = this.getZoomedInScale();

        return [- posX * scale, - posY * scale];
    },

    // Returns the current scale of the Overview.
    getScale : function() {
        return this.workspaces.actor.scaleX;
    },

    // Returns the current position of the Overview.
    getPosition : function() {
        return [this.workspaces.actor.x, this.workspaces.actor.y];
    },

    // show:
    //
    // Animates the overview visible and grabs mouse and keyboard input
    show : function() {
        if (this._shown)
            return;
        // Do this manually instead of using _syncInputMode, to handle failure
        if (!Main.pushModal(this.viewSelector.actor))
            return;
        this._modal = true;
        this._animateVisible();
        this._shown = true;
    },

    _animateVisible: function() {
        if (this.visible || this.animationInProgress)
            return;

        this.visible = true;
        this.animationInProgress = true;

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the Overview is displayed greatly
        // increases performance of the Overview especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the Overview rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();
        this._background.show();

        this.viewSelector.show();
        this._workspacesDisplay.show();
        this._dash.show();

        this.workspaces = this._workspacesDisplay.workspacesView;
        global.overlay_group.add_actor(this.workspaces.actor);

        if (!this._desktopFade.child)
            this._desktopFade.child = this._getDesktopClone();

        if (!this.workspaces.getActiveWorkspace().hasMaximizedWindows()) {
            this._desktopFade.opacity = 255;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 0,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad'
                             });
        }

        // Create a zoom out effect. First scale the workspaces view up and
        // position it so that the active workspace fills up the whole screen,
        // then transform it to its normal dimensions and position.
        // The opposite transition is used in hide().
        this.workspaces.actor.scaleX = this.workspaces.actor.scaleY = this.getZoomedInScale();
        [this.workspaces.actor.x, this.workspaces.actor.y] = this.getZoomedInPosition();
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.workspaces.actor,
                         { x: primary.x - this._group.x,
                           y: primary.y - this._group.y,
                           scaleX: 1,
                           scaleY: 1,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._showDone,
                           onCompleteScope: this
                          });

        // Make the other elements fade in.
        this._group.opacity = 0;
        Tweener.addTween(this._group,
                         { opacity: 255,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('showing');
    },

    // showTemporarily:
    //
    // Animates the overview visible without grabbing mouse and keyboard input;
    // if show() has already been called, this has no immediate effect, but
    // will result in the overview not being hidden until hideTemporarily() is
    // called.
    showTemporarily: function() {
        if (this._shownTemporarily)
            return;

        this._syncInputMode();
        this._animateVisible();
        this._shownTemporarily = true;
    },

    // hide:
    //
    // Reverses the effect of show()
    hide: function() {
        if (!this._shown)
            return;

        if (!this._shownTemporarily)
            this._animateNotVisible();

        this._shown = false;
        this._syncInputMode();
    },

    // hideTemporarily:
    //
    // Reverses the effect of showTemporarily()
    hideTemporarily: function() {
        if (!this._shownTemporarily)
            return;

        if (!this._shown)
            this._animateNotVisible();

        this._shownTemporarily = false;
        this._syncInputMode();
    },

    toggle: function() {
        if (this._shown)
            this.hide();
        else
            this.show();
    },

    /**
     * getWorkspacesForWindow:
     * @metaWindow: A #MetaWindow
     *
     * Returns the Workspaces object associated with the given window.
     * This method is not be accessible if the overview is not open
     * and will return %null.
     */
    getWorkspacesForWindow: function(metaWindow) {
        return this.workspaces;
    },

    //// Private methods ////

    _syncInputMode: function() {
        // We delay input mode changes during animation so that when removing the
        // overview we don't have a problem with the release of a press/release
        // going to an application.
        if (this.animationInProgress)
            return;

        if (this._shown) {
            if (!this._modal) {
                if (Main.pushModal(this._dash.actor))
                    this._modal = true;
                else
                    this.hide();
            }
        } else if (this._shownTemporarily) {
            if (this._modal) {
                Main.popModal(this._dash.actor);
                this._modal = false;
            }
            global.stage_input_mode = Shell.StageInputMode.FULLSCREEN;
        } else {
            if (this._modal) {
                Main.popModal(this._dash.actor);
                this._modal = false;
            }
            else if (global.stage_input_mode == Shell.StageInputMode.FULLSCREEN)
                global.stage_input_mode = Shell.StageInputMode.NORMAL;
        }
    },

    _animateNotVisible: function() {
        if (!this.visible || this.animationInProgress)
            return;

        this.animationInProgress = true;
        this._hideInProgress = true;

        if (!this.workspaces.getActiveWorkspace().hasMaximizedWindows()) {
            this._desktopFade.opacity = 0;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 255,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad' });
        }

        this.workspaces.hide();

        // Create a zoom in effect by transforming the workspaces view so that
        // the active workspace fills up the whole screen. The opposite
        // transition is used in show().
        let scale = this.getZoomedInScale();
        let [posX, posY] = this.getZoomedInPosition();
        Tweener.addTween(this.workspaces.actor,
                         { x: posX,
                           y: posY,
                           scaleX: scale,
                           scaleY: scale,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                          });

        // Make other elements fade out.
        Tweener.addTween(this._group,
                         { opacity: 0,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('hiding');
    },

    _showDone: function() {
        this.animationInProgress = false;
        this._desktopFade.hide();
        this._coverPane.lower_bottom();

        this.emit('shown');
        // Handle any calls to hide* while we were showing
        if (!this._shown && !this._shownTemporarily)
            this._animateNotVisible();

        this._syncInputMode();
    },

    _hideDone: function() {
        global.window_group.show();

        this.workspaces.destroy();
        this.workspaces = null;

        this._workspacesDisplay.hide();
        this.viewSelector.hide();
        this._dash.hide();

        this._desktopFade.hide();
        this._background.hide();
        this._group.hide();

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        this._coverPane.lower_bottom();

        this.emit('hidden');
        // Handle any calls to show* while we were hiding
        if (this._shown || this._shownTemporarily)
            this._animateVisible();

        this._syncInputMode();

        // Fake a pointer event if requested
        if (this._needsFakePointerEvent) {
            this._fakePointerEvent();
            this._needsFakePointerEvent = false;
        }
    }
};
Signals.addSignalMethods(Overview.prototype);
