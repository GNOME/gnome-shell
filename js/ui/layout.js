// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Background = imports.ui.background;
const BackgroundMenu = imports.ui.backgroundMenu;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const STARTUP_ANIMATION_TIME = 0.5;
const KEYBOARD_ANIMATION_TIME = 0.15;
const BACKGROUND_FADE_ANIMATION_TIME = 1.0;
const DEFAULT_BACKGROUND_COLOR = Clutter.Color.from_pixel(0x2e3436ff);

// The message tray takes this much pressure
// in the pressure barrier at once to release it.
const MESSAGE_TRAY_PRESSURE_THRESHOLD = 250; // pixels
const MESSAGE_TRAY_PRESSURE_TIMEOUT = 1000; // ms

const HOT_CORNER_PRESSURE_THRESHOLD = 100; // pixels
const HOT_CORNER_PRESSURE_TIMEOUT = 1000; // ms

function isPopupMetaWindow(actor) {
    switch(actor.meta_window.get_window_type()) {
    case Meta.WindowType.DROPDOWN_MENU:
    case Meta.WindowType.POPUP_MENU:
    case Meta.WindowType.COMBO:
        return true;
    default:
        return false;
    }
}

const MonitorConstraint = new Lang.Class({
    Name: 'MonitorConstraint',
    Extends: Clutter.Constraint,
    Properties: {'primary': GObject.ParamSpec.boolean('primary', 
                                                      'Primary', 'Track primary monitor',
                                                      GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                                      false),
                 'index': GObject.ParamSpec.int('index',
                                                'Monitor index', 'Track specific monitor',
                                                GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                                -1, 64, -1)},

    _init: function(props) {
        this._primary = false;
        this._index = -1;

        this.parent(props);
    },

    get primary() {
        return this._primary;
    },

    set primary(v) {
        if (v)
            this._index = -1;
        this._primary = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('primary');
    },

    get index() {
        return this._index;
    },

    set index(v) {
        this._primary = false;
        this._index = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('index');
    },

    vfunc_set_actor: function(actor) {
        if (actor) {
            if (!this._monitorsChangedId) {
                this._monitorsChangedId = Main.layoutManager.connect('monitors-changed', Lang.bind(this, function() {
                    this.actor.queue_relayout();
                }));
            }
        } else {
            if (this._monitorsChangedId)
                Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;
        }

        this.parent(actor);
    },

    vfunc_update_allocation: function(actor, actorBox) {
        if (!this._primary && this._index < 0)
            return;

        let monitor;
        if (this._primary) {
            monitor = Main.layoutManager.primaryMonitor;
        } else {
            let index = Math.min(this._index, Main.layoutManager.monitors.length - 1);
            monitor = Main.layoutManager.monitors[index];
        }

        actorBox.init_rect(monitor.x, monitor.y, monitor.width, monitor.height);
    }
});

const Monitor = new Lang.Class({
    Name: 'Monitor',

    _init: function(index, geometry) {
        this.index = index;
        this.x = geometry.x;
        this.y = geometry.y;
        this.width = geometry.width;
        this.height = geometry.height;
    },

    get inFullscreen() {
        return global.screen.get_monitor_in_fullscreen(this.index);
    }
})

const defaultParams = {
    trackFullscreen: false,
    affectsStruts: false,
};

const LayoutManager = new Lang.Class({
    Name: 'LayoutManager',

    _init: function () {
        this._rtl = (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL);
        this.monitors = [];
        this.primaryMonitor = null;
        this.primaryIndex = -1;
        this.hotCorners = [];

        this._keyboardIndex = -1;
        this._rightPanelBarrier = null;
        this._trayBarrier = null;

        this._inOverview = false;
        this._updateRegionIdle = 0;

        this._trackedActors = [];
        this._topActors = [];
        this._isPopupWindowVisible = false;
        this._startingUp = true;

        // Normally, the stage is always covered so Clutter doesn't need to clear
        // it; however it becomes visible during the startup animation
        // See the comment below for a longer explanation
        global.stage.color = DEFAULT_BACKGROUND_COLOR;

        // Set up stage hierarchy to group all UI actors under one container.
        this.uiGroup = new Shell.GenericContainer({ name: 'uiGroup' });
        this.uiGroup.connect('allocate',
                        function (actor, box, flags) {
                            let children = actor.get_children();
                            for (let i = 0; i < children.length; i++)
                                children[i].allocate_preferred_size(flags);
                        });
        this.uiGroup.connect('get-preferred-width',
                        function(actor, forHeight, alloc) {
                            let width = global.stage.width;
                            [alloc.min_size, alloc.natural_size] = [width, width];
                        });
        this.uiGroup.connect('get-preferred-height',
                        function(actor, forWidth, alloc) {
                            let height = global.stage.height;
                            [alloc.min_size, alloc.natural_size] = [height, height];
                        });

        global.stage.remove_actor(global.window_group);
        this.uiGroup.add_actor(global.window_group);

        global.stage.add_child(this.uiGroup);

        this.overviewGroup = new St.Widget({ name: 'overviewGroup',
                                             visible: false });
        this.addChrome(this.overviewGroup);

        this.screenShieldGroup = new St.Widget({ name: 'screenShieldGroup',
                                                 visible: false,
                                                 clip_to_allocation: true,
                                                 layout_manager: new Clutter.BinLayout(),
                                               });
        this.addChrome(this.screenShieldGroup);

        this.panelBox = new St.BoxLayout({ name: 'panelBox',
                                           vertical: true });
        this.addChrome(this.panelBox, { affectsStruts: true,
                                        trackFullscreen: true });
        this.panelBox.connect('allocation-changed',
                              Lang.bind(this, this._panelBoxChanged));

        this.trayBox = new St.Widget({ name: 'trayBox',
                                       layout_manager: new Clutter.BinLayout() }); 
        this.addChrome(this.trayBox);
        this._setupTrayPressure();

        this.keyboardBox = new St.BoxLayout({ name: 'keyboardBox',
                                              reactive: true,
                                              track_hover: true });
        this.addChrome(this.keyboardBox);
        this._keyboardHeightNotifyId = 0;

        global.stage.remove_actor(global.top_window_group);
        this.uiGroup.add_actor(global.top_window_group);

        this._backgroundGroup = new Meta.BackgroundGroup();
        global.window_group.add_child(this._backgroundGroup);
        this._backgroundGroup.lower_bottom();
        this._bgManagers = [];

        // Need to update struts on new workspaces when they are added
        global.screen.connect('notify::n-workspaces',
                              Lang.bind(this, this._queueUpdateRegions));
        global.screen.connect('restacked',
                              Lang.bind(this, this._windowsRestacked));
        global.screen.connect('monitors-changed',
                              Lang.bind(this, this._monitorsChanged));
        global.screen.connect('in-fullscreen-changed',
                              Lang.bind(this, this._updateFullscreen));
        this._monitorsChanged();
    },

    // This is called by Main after everything else is constructed
    init: function() {
        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));

        this._loadBackground();
    },

    showOverview: function() {
        this.overviewGroup.show();

        this._inOverview = true;
        this._updateVisibility();
        this._updateRegions();
    },

    hideOverview: function() {
        this.overviewGroup.hide();

        this._inOverview = false;
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _sessionUpdated: function() {
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _updateMonitors: function() {
        let screen = global.screen;

        this.monitors = [];
        let nMonitors = screen.get_n_monitors();
        for (let i = 0; i < nMonitors; i++)
            this.monitors.push(new Monitor(i, screen.get_monitor_geometry(i)));

        if (nMonitors == 1) {
            this.primaryIndex = this.bottomIndex = 0;
        } else {
            // If there are monitors below the primary, then we need
            // to split primary from bottom.
            this.primaryIndex = this.bottomIndex = screen.get_primary_monitor();
            for (let i = 0; i < this.monitors.length; i++) {
                let monitor = this.monitors[i];
                if (this._isAboveOrBelowPrimary(monitor)) {
                    if (monitor.y > this.monitors[this.bottomIndex].y)
                        this.bottomIndex = i;
                }
            }
        }
        this.primaryMonitor = this.monitors[this.primaryIndex];
        this.bottomMonitor = this.monitors[this.bottomIndex];
    },

    _updateHotCorners: function() {
        // destroy old hot corners
        this.hotCorners.forEach(function(corner) {
            if (corner)
                corner.destroy();
        });
        this.hotCorners = [];

        let size = this.panelBox.height;

        // build new hot corners
        for (let i = 0; i < this.monitors.length; i++) {
            let monitor = this.monitors[i];
            let cornerX = this._rtl ? monitor.x + monitor.width : monitor.x;
            let cornerY = monitor.y;

            let haveTopLeftCorner = true;

            if (i != this.primaryIndex) {
                // Check if we have a top left (right for RTL) corner.
                // I.e. if there is no monitor directly above or to the left(right)
                let besideX = this._rtl ? monitor.x + 1 : cornerX - 1;
                let besideY = cornerY;
                let aboveX = cornerX;
                let aboveY = cornerY - 1;

                for (let j = 0; j < this.monitors.length; j++) {
                    if (i == j)
                        continue;
                    let otherMonitor = this.monitors[j];
                    if (besideX >= otherMonitor.x &&
                        besideX < otherMonitor.x + otherMonitor.width &&
                        besideY >= otherMonitor.y &&
                        besideY < otherMonitor.y + otherMonitor.height) {
                        haveTopLeftCorner = false;
                        break;
                    }
                    if (aboveX >= otherMonitor.x &&
                        aboveX < otherMonitor.x + otherMonitor.width &&
                        aboveY >= otherMonitor.y &&
                        aboveY < otherMonitor.y + otherMonitor.height) {
                        haveTopLeftCorner = false;
                        break;
                    }
                }
            }

            if (haveTopLeftCorner) {
                let corner = new HotCorner(this, monitor, cornerX, cornerY);
                corner.setBarrierSize(size);
                this.hotCorners.push(corner);
            } else {
                this.hotCorners.push(null);
            }
        }

        this.emit('hot-corners-changed');
    },

    _createBackground: function(monitorIndex) {
        let bgManager = new Background.BackgroundManager({ container: this._backgroundGroup,
                                                           layoutManager: this,
                                                           monitorIndex: monitorIndex });
        BackgroundMenu.addBackgroundMenu(bgManager.background.actor);

        bgManager.connect('changed', Lang.bind(this, function() {
                              BackgroundMenu.addBackgroundMenu(bgManager.background.actor);
                          }));

        this._bgManagers[monitorIndex] = bgManager;

        return bgManager.background;
    },

    _createSecondaryBackgrounds: function() {
        for (let i = 0; i < this.monitors.length; i++) {
            if (i != this.primaryIndex) {
                let background = this._createBackground(i);

                background.actor.opacity = 0;
                Tweener.addTween(background.actor,
                                 { opacity: 255,
                                   time: BACKGROUND_FADE_ANIMATION_TIME,
                                   transition: 'easeOutQuad' });
            }
        }
    },

    _createPrimaryBackground: function() {
        this._createBackground(this.primaryIndex);
    },

    _updateBackgrounds: function() {
        let i;
        for (i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];

        if (Main.sessionMode.isGreeter)
            return;

        if (this._startingUp)
            return;

        for (let i = 0; i < this.monitors.length; i++) {
            this._createBackground(i);
        }
    },

    _updateBoxes: function() {
        this.screenShieldGroup.set_position(0, 0);
        this.screenShieldGroup.set_size(global.screen_width, global.screen_height);

        this.panelBox.set_position(this.primaryMonitor.x, this.primaryMonitor.y);
        this.panelBox.set_size(this.primaryMonitor.width, -1);

        if (this.keyboardIndex < 0)
            this.keyboardIndex = this.primaryIndex;

        this.trayBox.set_position(this.bottomMonitor.x,
                                  this.bottomMonitor.y + this.bottomMonitor.height);
        this.trayBox.set_size(this.bottomMonitor.width, -1);
    },

    _panelBoxChanged: function() {
        this._updatePanelBarrier();

        let size = this.panelBox.height;
        this.hotCorners.forEach(function(corner) {
            if (corner)
                corner.setBarrierSize(size);
        });
    },

    _updatePanelBarrier: function() {
        if (this._rightPanelBarrier) {
            this._rightPanelBarrier.destroy();
            this._rightPanelBarrier = null;
        }

        if (this.panelBox.height) {
            let primary = this.primaryMonitor;

            this._rightPanelBarrier = new Meta.Barrier({ display: global.display,
                                                         x1: primary.x + primary.width, y1: primary.y,
                                                         x2: primary.x + primary.width, y2: primary.y + this.panelBox.height,
                                                         directions: Meta.BarrierDirection.NEGATIVE_X });
        }
    },

    _setupTrayPressure: function() {
        this._trayPressure = new PressureBarrier(MESSAGE_TRAY_PRESSURE_THRESHOLD,
                                                 MESSAGE_TRAY_PRESSURE_TIMEOUT,
                                                 Shell.KeyBindingMode.NORMAL |
                                                 Shell.KeyBindingMode.OVERVIEW);
        this._trayPressure.setEventFilter(this._trayBarrierEventFilter);
        this._trayPressure.connect('trigger', function(barrier) {
            if (Main.layoutManager.bottomMonitor.inFullscreen)
                return;

            Main.messageTray.openTray();
        });
    },

    _updateTrayBarrier: function() {
        let monitor = this.bottomMonitor;

        if (this._trayBarrier) {
            this._trayPressure.removeBarrier(this._trayBarrier);
            this._trayBarrier.destroy();
            this._trayBarrier = null;
        }

        this._trayBarrier = new Meta.Barrier({ display: global.display,
                                               x1: monitor.x, x2: monitor.x + monitor.width,
                                               y1: monitor.y + monitor.height, y2: monitor.y + monitor.height,
                                               directions: Meta.BarrierDirection.NEGATIVE_Y });
        this._trayPressure.addBarrier(this._trayBarrier);
    },

    _trayBarrierEventFilter: function(event) {
        // Throw out all events where the pointer was grabbed by another
        // client, as the client that grabbed the pointer expects to have
        // complete control over it
        if (event.grabbed && Main.modalCount == 0)
            return true;

        return false;
    },

    _monitorsChanged: function() {
        this._updateMonitors();
        this._updateBoxes();
        this._updateTrayBarrier();
        this._updateHotCorners();
        this._updateBackgrounds();
        this._updateFullscreen();
        this._updateVisibility();
        this._queueUpdateRegions();

        this.emit('monitors-changed');
    },

    _isAboveOrBelowPrimary: function(monitor) {
        let primary = this.monitors[this.primaryIndex];
        let monitorLeft = monitor.x, monitorRight = monitor.x + monitor.width;
        let primaryLeft = primary.x, primaryRight = primary.x + primary.width;

        if ((monitorLeft >= primaryLeft && monitorLeft < primaryRight) ||
            (monitorRight > primaryLeft && monitorRight <= primaryRight) ||
            (primaryLeft >= monitorLeft && primaryLeft < monitorRight) ||
            (primaryRight > monitorLeft && primaryRight <= monitorRight))
            return true;

        return false;
    },

    get currentMonitor() {
        let index = global.screen.get_current_monitor();
        return this.monitors[index];
    },

    get keyboardMonitor() {
        return this.monitors[this.keyboardIndex];
    },

    get focusIndex() {
        let i = Main.layoutManager.primaryIndex;

        if (global.stage.key_focus != null)
            i = this.findIndexForActor(global.stage.key_focus);
        else if (global.display.focus_window != null)
            i = global.display.focus_window.get_monitor();
        return i;
    },

    get focusMonitor() {
        return this.monitors[this.focusIndex];
    },

    set keyboardIndex(v) {
        this._keyboardIndex = v;
        this.keyboardBox.set_position(this.keyboardMonitor.x,
                                      this.keyboardMonitor.y + this.keyboardMonitor.height);
        this.keyboardBox.set_size(this.keyboardMonitor.width, -1);
    },

    get keyboardIndex() {
        return this._keyboardIndex;
    },

    _loadBackground: function() {
        this._systemBackground = new Background.SystemBackground();
        this._systemBackground.actor.hide();

        global.stage.insert_child_below(this._systemBackground.actor, null);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._systemBackground.actor.add_constraint(constraint);

        let signalId = this._systemBackground.connect('loaded', Lang.bind(this, function() {
            this._systemBackground.disconnect(signalId);
            this._systemBackground.actor.show();
            global.stage.show();

            this._prepareStartupAnimation();
        }));
    },

    // Startup Animations
    //
    // We have two different animations, depending on whether we're a greeter
    // or a normal session.
    //
    // In the greeter, we want to animate the panel from the top, and smoothly
    // fade the login dialog on top of whatever plymouth left on screen which
    // we get as a still frame background before drawing anything else.
    //
    // Here we just have the code to animate the panel, and fade up the background.
    // The login dialog animation is handled by modalDialog.js
    //
    // When starting a normal user session, we want to grow it out of the middle
    // of the screen.
    //
    // Usually, we don't want to paint the stage background color because the
    // MetaBackgroundActor inside global.window_group covers the entirety of the
    // screen. So, we set no_clear_hint at the end of the animation.

    _prepareStartupAnimation: function() {
        // During the initial transition, add a simple actor to block all events,
        // so they don't get delivered to X11 windows that have been transformed.
        this._coverPane = new Clutter.Actor({ opacity: 0,
                                              width: global.screen_width,
                                              height: global.screen_height,
                                              reactive: true });
        this.addChrome(this._coverPane);

        if (Main.sessionMode.isGreeter) {
            this.panelBox.translation_y = -this.panelBox.height;
        } else {
            this._createPrimaryBackground();

            // We need to force an update of the regions now before we scale
            // the UI group to get the coorect allocation for the struts.
            this._updateRegions();

            this.trayBox.hide();
            this.keyboardBox.hide();

            let monitor = this.primaryMonitor;
            let x = monitor.x + monitor.width / 2.0;
            let y = monitor.y + monitor.height / 2.0;

            this.uiGroup.set_pivot_point(x / global.screen_width,
                                         y / global.screen_height);
            this.uiGroup.scale_x = this.uiGroup.scale_y = 0.5;
            this.uiGroup.opacity = 0;
            global.window_group.set_clip(monitor.x, monitor.y, monitor.width, monitor.height);
        }

        this.emit('startup-prepared');

        // We're mostly prepared for the startup animation
        // now, but since a lot is going on asynchronously
        // during startup, let's defer the startup animation
        // until the event loop is uncontended and idle.
        // This helps to prevent us from running the animation
        // when the system is bogged down
        GLib.idle_add(GLib.PRIORITY_LOW, Lang.bind(this, function() {
            this._startupAnimation();
            return false;
        }));
    },

    _startupAnimation: function() {
        if (Main.sessionMode.isGreeter)
            this._startupAnimationGreeter();
        else
            this._startupAnimationSession();
    },

    _startupAnimationGreeter: function() {
        Tweener.addTween(this.panelBox,
                         { translation_y: 0,
                           time: STARTUP_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._startupAnimationComplete,
                           onCompleteScope: this });
    },

    _startupAnimationSession: function() {
        Tweener.addTween(this.uiGroup,
                         { scale_x: 1,
                           scale_y: 1,
                           opacity: 255,
                           time: STARTUP_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._startupAnimationComplete,
                           onCompleteScope: this });
    },

    _startupAnimationComplete: function() {
        // At this point, the UI group is covering everything, so
        // we no longer need to clear the stage
        global.stage.no_clear_hint = true;

        this._coverPane.destroy();
        this._coverPane = null;

        this._systemBackground.actor.destroy();
        this._systemBackground = null;

        this._startingUp = false;

        this.trayBox.show();
        this.keyboardBox.show();

        if (!Main.sessionMode.isGreeter) {
            this._createSecondaryBackgrounds();
            global.window_group.remove_clip();
        }

        this._queueUpdateRegions();

        this.emit('startup-complete');
    },

    showKeyboard: function () {
        Tweener.addTween(this.keyboardBox,
                         { anchor_y: this.keyboardBox.height,
                           time: KEYBOARD_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._showKeyboardComplete,
                           onCompleteScope: this
                         });
        this.emit('keyboard-visible-changed', true);
    },

    _showKeyboardComplete: function() {
        // Poke Chrome to update the input shape; it doesn't notice
        // anchor point changes
        this._updateRegions();

        this._keyboardHeightNotifyId = this.keyboardBox.connect('notify::height', Lang.bind(this, function () {
            this.keyboardBox.anchor_y = this.keyboardBox.height;
        }));
    },

    hideKeyboard: function (immediate) {
        if (this._keyboardHeightNotifyId) {
            this.keyboardBox.disconnect(this._keyboardHeightNotifyId);
            this._keyboardHeightNotifyId = 0;
        }
        Tweener.addTween(this.keyboardBox,
                         { anchor_y: 0,
                           time: immediate ? 0 : KEYBOARD_ANIMATION_TIME,
                           transition: 'easeInQuad',
                           onComplete: this._hideKeyboardComplete,
                           onCompleteScope: this
                         });

        this.emit('keyboard-visible-changed', false);
    },

    _hideKeyboardComplete: function() {
        this._updateRegions();
    },

    // addChrome:
    // @actor: an actor to add to the chrome
    // @params: (optional) additional params
    //
    // Adds @actor to the chrome, and extends the input region
    // to include it. Changes in @actor's size, position, and
    // visibility will automatically result in appropriate changes
    // to the input region.
    //
    // If %affectsStruts in @params is %true (and @actor is along a
    // screen edge), then @actor's size and position will also affect
    // the window manager struts. Changes to @actor's visibility will
    // NOT affect whether or not the strut is present, however.
    //
    // If %trackFullscreen in @params is %true, the actor's visibility
    // will be bound to the presence of fullscreen windows on the same
    // monitor (it will be hidden whenever a fullscreen window is visible,
    // and shown otherwise)
    addChrome: function(actor, params) {
        this.uiGroup.add_actor(actor);
        if (this.uiGroup.contains(global.top_window_group))
            this.uiGroup.set_child_below_sibling(actor, global.top_window_group);
        this._trackActor(actor, params);
    },

    // trackChrome:
    // @actor: a descendant of the chrome to begin tracking
    // @params: parameters describing how to track @actor
    //
    // Tells the chrome to track @actor, which must be a descendant
    // of an actor added via addChrome(). This can be used to extend the
    // struts or input region to cover specific children.
    //
    // @params can have any of the same values as in addChrome(),
    // though some possibilities don't make sense. By default, @actor has
    // the same params as its chrome ancestor.
    trackChrome: function(actor, params) {
        let ancestor = actor.get_parent();
        let index = this._findActor(ancestor);
        while (ancestor && index == -1) {
            ancestor = ancestor.get_parent();
            index = this._findActor(ancestor);
        }
        if (!ancestor)
            throw new Error('actor is not a descendent of a chrome actor');

        let ancestorData = this._trackedActors[index];
        if (!params)
            params = {};
        // We can't use Params.parse here because we want to drop
        // the extra values like ancestorData.actor
        for (let prop in defaultParams) {
            if (!params.hasOwnProperty(prop))
                params[prop] = ancestorData[prop];
        }

        this._trackActor(actor, params);
    },

    // untrackChrome:
    // @actor: an actor previously tracked via trackChrome()
    //
    // Undoes the effect of trackChrome()
    untrackChrome: function(actor) {
        this._untrackActor(actor);
    },

    // removeChrome:
    // @actor: a chrome actor
    //
    // Removes @actor from the chrome
    removeChrome: function(actor) {
        this.uiGroup.remove_actor(actor);
        this._untrackActor(actor);
    },

    _findActor: function(actor) {
        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (actorData.actor == actor)
                return i;
        }
        return -1;
    },

    _trackActor: function(actor, params) {
        if (this._findActor(actor) != -1)
            throw new Error('trying to re-track existing chrome actor');

        let actorData = Params.parse(params, defaultParams);
        actorData.actor = actor;
        actorData.isToplevel = actor.get_parent() == this.uiGroup;
        actorData.visibleId = actor.connect('notify::visible',
                                            Lang.bind(this, this._queueUpdateRegions));
        actorData.allocationId = actor.connect('notify::allocation',
                                               Lang.bind(this, this._queueUpdateRegions));
        actorData.parentSetId = actor.connect('parent-set',
                                              Lang.bind(this, this._actorReparented));
        // Note that destroying actor will unset its parent, so we don't
        // need to connect to 'destroy' too.

        this._trackedActors.push(actorData);
        this._queueUpdateRegions();
    },

    _untrackActor: function(actor) {
        let i = this._findActor(actor);

        if (i == -1)
            return;
        let actorData = this._trackedActors[i];

        this._trackedActors.splice(i, 1);
        actor.disconnect(actorData.visibleId);
        actor.disconnect(actorData.allocationId);
        actor.disconnect(actorData.parentSetId);

        this._queueUpdateRegions();
    },

    _actorReparented: function(actor, oldParent) {
        let newParent = actor.get_parent();
        if (!newParent) {
            this._untrackActor(actor);
        } else {
            let i = this._findActor(actor);
            let actorData = this._trackedActors[i];
            actorData.isToplevel = (newParent == this.uiGroup);
        }
    },

    _updateVisibility: function() {
        let windowsVisible = Main.sessionMode.hasWindows && !this._inOverview;

        global.window_group.visible = windowsVisible;
        global.top_window_group.visible = windowsVisible;

        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i], visible;
            if (!actorData.trackFullscreen)
                continue;
            if (!actorData.isToplevel)
                continue;

            if (!windowsVisible)
                visible = true;
            else if (this.findMonitorForActor(actorData.actor).inFullscreen)
                visible = false;
            else
                visible = true;
            actorData.actor.visible = visible;
        }
    },

    getWorkAreaForMonitor: function(monitorIndex) {
        // Assume that all workspaces will have the same
        // struts and pick the first one.
        let ws = global.screen.get_workspace_by_index(0);
        return ws.get_work_area_for_monitor(monitorIndex);
    },

    // This call guarantees that we return some monitor to simplify usage of it
    // In practice all tracked actors should be visible on some monitor anyway
    findIndexForActor: function(actor) {
        let [x, y] = actor.get_transformed_position();
        let [w, h] = actor.get_transformed_size();
        let rect = new Meta.Rectangle({ x: x, y: y, width: w, height: h });
        return global.screen.get_monitor_index_for_rect(rect);
    },

    findMonitorForActor: function(actor) {
        return this.monitors[this.findIndexForActor(actor)];
    },

    _queueUpdateRegions: function() {
        if (Main.sessionMode.isGreeter)
            return;

        if (this._startingUp)
            return;

        if (!this._updateRegionIdle)
            this._updateRegionIdle = Mainloop.idle_add(Lang.bind(this, this._updateRegions),
                                                       Meta.PRIORITY_BEFORE_REDRAW);
    },

    _getWindowActorsForWorkspace: function(workspace) {
        return global.get_window_actors().filter(function (actor) {
            let win = actor.meta_window;
            return win.located_on_workspace(workspace);
        });
    },

    _updateFullscreen: function() {
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _windowsRestacked: function() {
        let changed = false;

        if (this._isPopupWindowVisible != global.top_window_group.get_children().some(isPopupMetaWindow))
            changed = true;

        if (changed) {
            this._updateVisibility();
            this._queueUpdateRegions();
        }
    },

    _updateRegions: function() {
        let rects = [], struts = [], i;

        if (this._updateRegionIdle) {
            Mainloop.source_remove(this._updateRegionIdle);
            delete this._updateRegionIdle;
        }

        let isPopupMenuVisible = global.top_window_group.get_children().some(isPopupMetaWindow);
        let wantsInputRegion = !isPopupMenuVisible;

        for (i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (!wantsInputRegion && !actorData.affectsStruts)
                continue;

            let [x, y] = actorData.actor.get_transformed_position();
            let [w, h] = actorData.actor.get_transformed_size();
            x = Math.round(x);
            y = Math.round(y);
            w = Math.round(w);
            h = Math.round(h);

            if (wantsInputRegion && actorData.actor.get_paint_visibility())
                rects.push(new Meta.Rectangle({ x: x, y: y, width: w, height: h }));

            if (actorData.affectsStruts) {
                // Limit struts to the size of the screen
                let x1 = Math.max(x, 0);
                let x2 = Math.min(x + w, global.screen_width);
                let y1 = Math.max(y, 0);
                let y2 = Math.min(y + h, global.screen_height);

                // NetWM struts are not really powerful enought to handle
                // a multi-monitor scenario, they only describe what happens
                // around the outer sides of the full display region. However
                // it can describe a partial region along each side, so
                // we can support having the struts only affect the
                // primary monitor. This should be enough as we only have
                // chrome affecting the struts on the primary monitor so
                // far.
                //
                // Metacity wants to know what side of the screen the
                // strut is considered to be attached to. If the actor is
                // only touching one edge, or is touching the entire
                // border of the primary monitor, then it's obvious which
                // side to call it. If it's in a corner, we pick a side
                // arbitrarily. If it doesn't touch any edges, or it spans
                // the width/height across the middle of the screen, then
                // we don't create a strut for it at all.
                let side;
                let primary = this.primaryMonitor;
                if (x1 <= primary.x && x2 >= primary.x + primary.width) {
                    if (y1 <= primary.y)
                        side = Meta.Side.TOP;
                    else if (y2 >= primary.y + primary.height)
                        side = Meta.Side.BOTTOM;
                    else
                        continue;
                } else if (y1 <= primary.y && y2 >= primary.y + primary.height) {
                    if (x1 <= 0)
                        side = Meta.Side.LEFT;
                    else if (x2 >= primary.x + primary.width)
                        side = Meta.Side.RIGHT;
                    else
                        continue;
                } else if (x1 <= 0)
                    side = Meta.Side.LEFT;
                else if (y1 <= 0)
                    side = Meta.Side.TOP;
                else if (x2 >= global.screen_width)
                    side = Meta.Side.RIGHT;
                else if (y2 >= global.screen_height)
                    side = Meta.Side.BOTTOM;
                else
                    continue;

                // Ensure that the strut rects goes all the way to the screen edge,
                // as this really what mutter expects.
                switch (side) {
                case Meta.Side.TOP:
                    y1 = 0;
                    break;
                case Meta.Side.BOTTOM:
                    y2 = global.screen_height;
                    break;
                case Meta.Side.LEFT:
                    x1 = 0;
                    break;
                case Meta.Side.RIGHT:
                    x2 = global.screen_width;
                    break;
                }

                let strutRect = new Meta.Rectangle({ x: x1, y: y1, width: x2 - x1, height: y2 - y1});
                let strut = new Meta.Strut({ rect: strutRect, side: side });
                struts.push(strut);
            }
        }

        global.set_stage_input_region(rects);
        this._isPopupWindowVisible = isPopupMenuVisible;

        let screen = global.screen;
        for (let w = 0; w < screen.n_workspaces; w++) {
            let workspace = screen.get_workspace_by_index(w);
            workspace.set_builtin_struts(struts);
        }

        return false;
    }
});
Signals.addSignalMethods(LayoutManager.prototype);


// HotCorner:
//
// This class manages a "hot corner" that can toggle switching to
// overview.
const HotCorner = new Lang.Class({
    Name: 'HotCorner',

    _init : function(layoutManager, monitor, x, y) {
        // We use this flag to mark the case where the user has entered the
        // hot corner and has not left both the hot corner and a surrounding
        // guard area (the "environs"). This avoids triggering the hot corner
        // multiple times due to an accidental jitter.
        this._entered = false;

        this._monitor = monitor;

        this._x = x;
        this._y = y;

        this._setupFallbackCornerIfNeeded(layoutManager);

        this._pressureBarrier = new PressureBarrier(HOT_CORNER_PRESSURE_THRESHOLD,
                                                    HOT_CORNER_PRESSURE_TIMEOUT,
                                                    Shell.KeyBindingMode.NORMAL |
                                                    Shell.KeyBindingMode.OVERVIEW);
        this._pressureBarrier.connect('trigger', Lang.bind(this, this._toggleOverview));

        // Cache the three ripples instead of dynamically creating and destroying them.
        this._ripple1 = new St.BoxLayout({ style_class: 'ripple-box', opacity: 0, visible: false });
        this._ripple2 = new St.BoxLayout({ style_class: 'ripple-box', opacity: 0, visible: false });
        this._ripple3 = new St.BoxLayout({ style_class: 'ripple-box', opacity: 0, visible: false });

        layoutManager.uiGroup.add_actor(this._ripple1);
        layoutManager.uiGroup.add_actor(this._ripple2);
        layoutManager.uiGroup.add_actor(this._ripple3);
    },

    setBarrierSize: function(size) {
        if (this._verticalBarrier) {
            this._pressureBarrier.removeBarrier(this._verticalBarrier);
            this._verticalBarrier.destroy();
            this._verticalBarrier = null;
        }

        if (this._horizontalBarrier) {
            this._pressureBarrier.removeBarrier(this._horizontalBarrier);
            this._horizontalBarrier.destroy();
            this._horizontalBarrier = null;
        }

        if (size > 0) {
            if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL) {
                this._verticalBarrier = new Meta.Barrier({ display: global.display,
                                                           x1: this._x, x2: this._x, y1: this._y, y2: this._y + size,
                                                           directions: Meta.BarrierDirection.NEGATIVE_X });
                this._horizontalBarrier = new Meta.Barrier({ display: global.display,
                                                             x1: this._x - size, x2: this._x, y1: this._y, y2: this._y,
                                                             directions: Meta.BarrierDirection.POSITIVE_Y });
            } else {
                this._verticalBarrier = new Meta.Barrier({ display: global.display,
                                                           x1: this._x, x2: this._x, y1: this._y, y2: this._y + size,
                                                           directions: Meta.BarrierDirection.POSITIVE_X });
                this._horizontalBarrier = new Meta.Barrier({ display: global.display,
                                                             x1: this._x, x2: this._x + size, y1: this._y, y2: this._y,
                                                             directions: Meta.BarrierDirection.POSITIVE_Y });
            }

            this._pressureBarrier.addBarrier(this._verticalBarrier);
            this._pressureBarrier.addBarrier(this._horizontalBarrier);
        }
    },

    _setupFallbackCornerIfNeeded: function(layoutManager) {
        if (!global.display.supports_extended_barriers()) {
            this.actor = new Clutter.Actor({ name: 'hot-corner-environs',
                                             x: this._x, y: this._y,
                                             width: 3,
                                             height: 3,
                                             reactive: true });

            this._corner = new Clutter.Actor({ name: 'hot-corner',
                                               width: 1,
                                               height: 1,
                                               opacity: 0,
                                               reactive: true });
            this._corner._delegate = this;

            this.actor.add_child(this._corner);
            layoutManager.addChrome(this.actor);

            if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL) {
                this._corner.set_position(this.actor.width - this._corner.width, 0);
                this.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
            } else {
                this._corner.set_position(0, 0);
            }

            this.actor.connect('leave-event',
                               Lang.bind(this, this._onEnvironsLeft));

            this._corner.connect('enter-event',
                                 Lang.bind(this, this._onCornerEntered));
            this._corner.connect('leave-event',
                                 Lang.bind(this, this._onCornerLeft));
        }
    },

    destroy: function() {
        this.setBarrierSize(0);
        this._pressureBarrier.destroy();
        this._pressureBarrier = null;

        if (this.actor)
            this.actor.destroy();
    },

    _animRipple : function(ripple, delay, time, startScale, startOpacity, finalScale) {
        // We draw a ripple by using a source image and animating it scaling
        // outwards and fading away. We want the ripples to move linearly
        // or it looks unrealistic, but if the opacity of the ripple goes
        // linearly to zero it fades away too quickly, so we use Tweener's
        // 'onUpdate' to give a non-linear curve to the fade-away and make
        // it more visible in the middle section.

        ripple._opacity = startOpacity;

        if (ripple.get_text_direction() == Clutter.TextDirection.RTL)
            ripple.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);

        ripple.visible = true;
        ripple.opacity = 255 * Math.sqrt(startOpacity);
        ripple.scale_x = ripple.scale_y = startScale;

        ripple.x = this._x;
        ripple.y = this._y;

        Tweener.addTween(ripple, { _opacity: 0,
                                   scale_x: finalScale,
                                   scale_y: finalScale,
                                   delay: delay,
                                   time: time,
                                   transition: 'linear',
                                   onUpdate: function() { ripple.opacity = 255 * Math.sqrt(ripple._opacity); },
                                   onComplete: function() { ripple.visible = false; } });
    },

    _rippleAnimation: function() {
        // Show three concentric ripples expanding outwards; the exact
        // parameters were found by trial and error, so don't look
        // for them to make perfect sense mathematically

        //                              delay  time  scale opacity => scale
        this._animRipple(this._ripple1, 0.0,   0.83,  0.25,  1.0,     1.5);
        this._animRipple(this._ripple2, 0.05,  1.0,   0.0,   0.7,     1.25);
        this._animRipple(this._ripple3, 0.35,  1.0,   0.0,   0.3,     1);
    },

    _toggleOverview: function() {
        if (this._monitor.inFullscreen)
            return;

        if (Main.overview.shouldToggleByCornerOrButton()) {
            this._rippleAnimation();
            Main.overview.toggle();
        }
    },

    handleDragOver: function(source, actor, x, y, time) {
        if (source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        this._toggleOverview();

        return DND.DragMotionResult.CONTINUE;
    },

    _onCornerEntered : function() {
        if (!this._entered) {
            this._entered = true;
            this._toggleOverview();
        }
        return false;
    },

    _onCornerLeft : function(actor, event) {
        if (event.get_related() != this.actor)
            this._entered = false;
        // Consume event, otherwise this will confuse onEnvironsLeft
        return true;
    },

    _onEnvironsLeft : function(actor, event) {
        if (event.get_related() != this._corner)
            this._entered = false;
        return false;
    }
});

const PressureBarrier = new Lang.Class({
    Name: 'PressureBarrier',

    _init: function(threshold, timeout, keybindingMode) {
        this._threshold = threshold;
        this._timeout = timeout;
        this._keybindingMode = keybindingMode;
        this._barriers = [];
        this._eventFilter = null;

        this._isTriggered = false;
        this._reset();
    },

    addBarrier: function(barrier) {
        barrier._pressureHitId = barrier.connect('hit', Lang.bind(this, this._onBarrierHit));
        barrier._pressureLeftId = barrier.connect('left', Lang.bind(this, this._onBarrierLeft));

        this._barriers.push(barrier);
    },

    _disconnectBarrier: function(barrier) {
        barrier.disconnect(barrier._pressureHitId);
        barrier.disconnect(barrier._pressureLeftId);
    },

    removeBarrier: function(barrier) {
        this._disconnectBarrier(barrier);
        this._barriers.splice(this._barriers.indexOf(barrier), 1);
    },

    destroy: function() {
        this._barriers.forEach(Lang.bind(this, this._disconnectBarrier));
        this._barriers = [];
    },

    setEventFilter: function(filter) {
        this._eventFilter = filter;
    },

    _reset: function() {
        this._barrierEvents = [];
        this._currentPressure = 0;
        this._lastTime = 0;
    },

    _isHorizontal: function(barrier) {
        return barrier.y1 == barrier.y2;
    },

    _getDistanceAcrossBarrier: function(barrier, event) {
        if (this._isHorizontal(barrier))
            return Math.abs(event.dy);
        else
            return Math.abs(event.dx);
    },

    _getDistanceAlongBarrier: function(barrier, event) {
        if (this._isHorizontal(barrier))
            return Math.abs(event.dx);
        else
            return Math.abs(event.dy);
    },

    _trimBarrierEvents: function() {
        // Events are guaranteed to be sorted in time order from
        // oldest to newest, so just look for the first old event,
        // and then chop events after that off.
        let i = 0;
        let threshold = this._lastTime - this._timeout;

        while (i < this._barrierEvents.length) {
            let [time, distance] = this._barrierEvents[i];
            if (time >= threshold)
                break;
            i++;
        }

        let firstNewEvent = i;

        for (i = 0; i < firstNewEvent; i++) {
            let [time, distance] = this._barrierEvents[i];
            this._currentPressure -= distance;
        }

        this._barrierEvents = this._barrierEvents.slice(firstNewEvent);
    },

    _onBarrierLeft: function(barrier, event) {
        this._reset();
        this._isTriggered = false;
    },

    _trigger: function() {
        this._isTriggered = true;
        this.emit('trigger');
        this._reset();
    },

    _onBarrierHit: function(barrier, event) {
        // If we've triggered the barrier, wait until the pointer has the
        // left the barrier hitbox until we trigger it again.
        if (this._isTriggered)
            return;

        if (this._eventFilter && this._eventFilter(event))
            return;

        // Throw out all events not in the proper keybinding mode
        if (!(this._keybindingMode & Main.keybindingMode))
            return;

        let slide = this._getDistanceAlongBarrier(barrier, event);
        let distance = this._getDistanceAcrossBarrier(barrier, event);

        if (distance >= this._threshold) {
            this._trigger();
            return;
        }

        // Throw out events where the cursor is move more
        // along the axis of the barrier than moving with
        // the barrier.
        if (slide > distance)
            return;

        this._lastTime = event.time;

        this._trimBarrierEvents();
        distance = Math.min(15, distance);

        this._barrierEvents.push([event.time, distance]);
        this._currentPressure += distance;

        if (this._currentPressure >= this._threshold)
            this._trigger();
    }
});
Signals.addSignalMethods(PressureBarrier.prototype);
