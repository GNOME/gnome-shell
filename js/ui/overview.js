// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Gdk = imports.gi.Gdk;

const Dash = imports.ui.dash;
const DND = imports.ui.dnd;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Panel = imports.ui.panel;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const ViewSelector = imports.ui.viewSelector;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

// Time for initial animation going into Overview mode
const ANIMATION_TIME = 0.25;

// XXX -- grab this automatically. Hard to do.
const DASH_MAX_WIDTH = 96;

const DND_WINDOW_SWITCH_TIMEOUT = 1250;

const GLSL_DIM_EFFECT_DECLARATIONS = '';
const GLSL_DIM_EFFECT_CODE = '\
   vec2 dist = cogl_tex_coord_in[0].xy - vec2(0.5, 0.5); \
   float elipse_radius = 0.5; \
   /* from https://bugzilla.gnome.org/show_bug.cgi?id=669798: \
      the alpha on the gradient goes from 165 at its darkest to 98 at its most transparent. */ \
   float y = 165.0 / 255.0; \
   float x = 98.0 / 255.0; \
   /* interpolate darkening value, based on distance from screen center */ \
   float val = min(length(dist), elipse_radius); \
   float a = mix(x, y, val / elipse_radius); \
   /* dim_factor varies from [1.0 -> 0.5] when overview is showing \
      We use it to smooth value, then we clamp it to valid color interval */ \
   a = clamp(a - cogl_color_in.r + 0.5, 0.0, 1.0); \
   /* We\'re blending between: color and black color (obviously omitted in the equation) */ \
   cogl_color_out.xyz = cogl_color_out.xyz * (1.0 - a); \
   cogl_color_out.a = 1.0;';

const ShellInfo = new Lang.Class({
    Name: 'ShellInfo',

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

    setMessage: function(text, options) {
        options = Params.parse(options, { undoCallback: null,
                                          forFeedback: false
                                        });

        let undoCallback = options.undoCallback;
        let forFeedback = options.forFeedback;

        if (this._source == null) {
            this._source = new MessageTray.SystemNotificationSource();
            this._source.connect('destroy', Lang.bind(this,
                function() {
                    this._source = null;
                }));
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
        if (undoCallback) {
            notification.addButton('system-undo', _("Undo"));
            notification.connect('action-invoked', Lang.bind(this, this._onUndoClicked));
        }

        this._source.notify(notification);
    }
});

const Overview = new Lang.Class({
    Name: 'Overview',

    _init: function() {
        this._overviewCreated = false;
        this._initCalled = false;

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _createOverview: function() {
        if (this._overviewCreated)
            return;

        if (this.isDummy)
            return;

        this._overviewCreated = true;

        // The main BackgroundActor is inside global.window_group which is
        // hidden when displaying the overview, so we create a new
        // one. Instances of this class share a single CoglTexture behind the
        // scenes which allows us to show the background with different
        // rendering options without duplicating the texture data.
        this._background = Meta.BackgroundActor.new_for_screen(global.screen);
        this._background.add_glsl_snippet(Meta.SnippetHook.FRAGMENT,
                                          GLSL_DIM_EFFECT_DECLARATIONS,
                                          GLSL_DIM_EFFECT_CODE,
                                          false);
        this._background.hide();
        global.overlay_group.add_actor(this._background);

        this._desktopFade = new St.Bin();
        global.overlay_group.add_actor(this._desktopFade);

        this._spacing = 0;

        /* Translators: This is the main view to select
           activities. See also note for "Activities" string. */
        this._group = new St.Widget({ name: 'overview',
                                      accessible_name: _("Overview"),
                                      reactive: true });
        this._group._delegate = this;
        this._group.connect('style-changed',
            Lang.bind(this, function() {
                let node = this._group.get_theme_node();
                let spacing = node.get_length('spacing');
                if (spacing != this._spacing) {
                    this._spacing = spacing;
                    this._relayout();
                }
            }));

        this._capturedEventId = 0;
        this._buttonPressId = 0;

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

        this._coverPane.hide();

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

        if (this._initCalled)
            this.init();
    },

    _sessionUpdated: function() {
        this.isDummy = !Main.sessionMode.hasOverview;
        this._createOverview();
    },

    // The members we construct that are implemented in JS might
    // want to access the overview as Main.overview to connect
    // signal handlers and so forth. So we create them after
    // construction in this init() method.
    init: function() {
        this._initCalled = true;

        if (this.isDummy)
            return;

        this._shellInfo = new ShellInfo();

        this._searchEntry = new St.Entry({ name: 'searchEntry',
                                           /* Translators: this is the text displayed
                                              in the search entry when no search is
                                              active; it should not exceed ~30
                                              characters. */
                                           hint_text: _("Type to search..."),
                                           track_hover: true,
                                           can_focus: true });
        this._group.add_actor(this._searchEntry);

        this._dash = new Dash.Dash();
        this._viewSelector = new ViewSelector.ViewSelector(this._searchEntry,
                                                           this._dash.showAppsButton);
        this._group.add_actor(this._viewSelector.actor);
        this._group.add_actor(this._dash.actor);

        // TODO - recalculate everything when desktop size changes
        this._dash.actor.add_constraint(this._viewSelector.constrainHeight);
        this.dashIconSize = this._dash.iconSize;
        this._dash.connect('icon-size-changed',
                           Lang.bind(this, function() {
                               this.dashIconSize = this._dash.iconSize;
                           }));

        // Translators: this is the name of the dock/favorites area on
        // the left of the overview
        Main.ctrlAltTabManager.addGroup(this._dash.actor, _("Dash"), 'user-bookmarks-symbolic');

        Main.layoutManager.connect('monitors-changed', Lang.bind(this, this._relayout));
        this._relayout();
    },

    addSearchProvider: function(provider) {
        this._viewSelector.addSearchProvider(provider);
    },

    removeSearchProvider: function(provider) {
        this._viewSelector.removeSearchProvider(provider);
    },

    //
    // options:
    //  - undoCallback (function): the callback to be called if undo support is needed
    //  - forFeedback (boolean): whether the message is for direct feedback of a user action
    //
    setMessage: function(text, options) {
        if (this.isDummy)
            return;

        this._shellInfo.setMessage(text, options);
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
        this._resetWindowSwitchTimeout();
        this._lastHoveredWindow = null;
        DND.removeDragMonitor(this._dragMonitor);
        this.endItemDrag();
    },

    _resetWindowSwitchTimeout: function() {
        if (this._windowSwitchTimeoutId != 0) {
            Mainloop.source_remove(this._windowSwitchTimeoutId);
            this._windowSwitchTimeoutId = 0;
            this._needsFakePointerEvent = false;
        }
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

    addAction: function(action) {
        if (this.isDummy)
            return;

        this._group.add_action(action);
    },

    _getDesktopClone: function() {
        let windows = global.get_window_actors().filter(function(w) {
            return w.meta_window.get_window_type() == Meta.WindowType.DESKTOP;
        });
        if (windows.length == 0)
            return null;

        let window = windows[0];
        let clone = new Clutter.Clone({ source: window.get_texture(),
                                        x: window.x, y: window.y });
        clone.source.connect('destroy', Lang.bind(this, function() {
            clone.destroy();
        }));
        return clone;
    },

    _relayout: function () {
        // To avoid updating the position and size of the workspaces
        // we just hide the overview. The positions will be updated
        // when it is next shown.
        this.hide();

        let primary = Main.layoutManager.primaryMonitor;
        let rtl = (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL);

        let contentY = Main.panel.actor.height;
        let contentHeight = primary.height - contentY - Main.messageTray.actor.height;

        this._group.set_position(primary.x, primary.y);
        this._group.set_size(primary.width, primary.height);

        this._coverPane.set_position(0, contentY);
        this._coverPane.set_size(primary.width, contentHeight);

        let searchWidth = this._searchEntry.get_width();
        let searchHeight = this._searchEntry.get_height();
        let searchX = (primary.width - searchWidth) / 2;
        let searchY = contentY + this._spacing;

        let dashWidth = DASH_MAX_WIDTH;
        let dashY = searchY + searchHeight + this._spacing;
        let dashX;
        if (rtl) {
            this._dash.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
            dashX = primary.width;
        } else {
            dashX = 0;
        }

        let viewX = rtl ? 0 : dashWidth + this._spacing;
        let viewY = searchY + searchHeight + this._spacing;
        let viewWidth = primary.width - dashWidth - this._spacing;
        let viewHeight = contentHeight - this._spacing - viewY;

        this._searchEntry.set_position(searchX, searchY);
        this._dash.actor.set_position(dashX, dashY);
        this._viewSelector.actor.set_position(viewX, viewY);
        this._viewSelector.actor.set_size(viewWidth, viewHeight);
    },

    //// Public methods ////

    beginItemDrag: function(source) {
        this.emit('item-drag-begin');
    },

    cancelledItemDrag: function(source) {
        this.emit('item-drag-cancelled');
    },

    endItemDrag: function(source) {
        this.emit('item-drag-end');
    },

    beginWindowDrag: function(source) {
        this.emit('window-drag-begin');
    },

    cancelledWindowDrag: function(source) {
        this.emit('window-drag-cancelled');
    },

    endWindowDrag: function(source) {
        this.emit('window-drag-end');
    },

    // show:
    //
    // Animates the overview visible and grabs mouse and keyboard input
    show : function() {
        if (this.isDummy)
            return;
        if (this._shown)
            return;
        this._shown = true;
        this._syncInputMode();
        if (!this._modal)
            return;
        this._animateVisible();
    },

    fadeInDesktop: function() {
            this._desktopFade.opacity = 0;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 255,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad' });
    },

    fadeOutDesktop: function() {
        if (!this._desktopFade.child)
            this._desktopFade.child = this._getDesktopClone();

        this._desktopFade.opacity = 255;
        this._desktopFade.show();
        Tweener.addTween(this._desktopFade,
                         { opacity: 0,
                           time: ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
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
        //
        // Disable unredirection while in the overview
        Meta.disable_unredirect_for_screen(global.screen);
        global.window_group.hide();
        this._group.show();
        this._background.show();
        this._viewSelector.show();

        this._group.opacity = 0;
        Tweener.addTween(this._group,
                         { opacity: 255,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._showDone,
                           onCompleteScope: this
                         });

        Tweener.addTween(this._background,
                         { dim_factor: 0.8,
                           time: ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });

        this._coverPane.raise_top();
        this._coverPane.show();
        this.emit('showing');
    },

    // showTemporarily:
    //
    // Animates the overview visible without grabbing mouse and keyboard input;
    // if show() has already been called, this has no immediate effect, but
    // will result in the overview not being hidden until hideTemporarily() is
    // called.
    showTemporarily: function() {
        if (this.isDummy)
            return;

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
        if (this.isDummy)
            return;

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
        if (this.isDummy)
            return;

        if (!this._shownTemporarily)
            return;

        if (!this._shown)
            this._animateNotVisible();

        this._shownTemporarily = false;
        this._syncInputMode();
    },

    toggle: function() {
        if (this.isDummy)
            return;

        if (this.visible)
            this.hide();
        else
            this.show();
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
                if (Main.pushModal(this._group,
                                   { keybindingMode: Main.KeybindingMode.OVERVIEW }))
                    this._modal = true;
                else
                    this.hide();
            }
        } else if (this._shownTemporarily) {
            if (this._modal) {
                Main.popModal(this._group);
                this._modal = false;
            }
            global.stage_input_mode = Shell.StageInputMode.FULLSCREEN;
        } else {
            if (this._modal) {
                Main.popModal(this._group);
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

        this._viewSelector.zoomFromOverview();

        // Make other elements fade out.
        Tweener.addTween(this._group,
                         { opacity: 0,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });

        Tweener.addTween(this._background,
                         { dim_factor: 1.0,
                           time: ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });

        this._coverPane.raise_top();
        this._coverPane.show();
        this.emit('hiding');
    },

    _showDone: function() {
        this.animationInProgress = false;
        this._desktopFade.hide();
        this._coverPane.hide();

        this.emit('shown');
        // Handle any calls to hide* while we were showing
        if (!this._shown && !this._shownTemporarily)
            this._animateNotVisible();

        this._syncInputMode();
        global.sync_pointer();
    },

    _hideDone: function() {
        // Re-enable unredirection
        Meta.enable_unredirect_for_screen(global.screen);

        global.window_group.show();

        this._viewSelector.hide();
        this._desktopFade.hide();
        this._background.hide();
        this._group.hide();

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        this._coverPane.hide();

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
});
Signals.addSignalMethods(Overview.prototype);
