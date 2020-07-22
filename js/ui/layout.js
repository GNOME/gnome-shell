// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported MonitorConstraint, LayoutManager */

const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const Background = imports.ui.background;
const BackgroundMenu = imports.ui.backgroundMenu;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Ripples = imports.ui.ripples;

var STARTUP_ANIMATION_TIME = 500;
var KEYBOARD_ANIMATION_TIME = 150;
var BACKGROUND_FADE_ANIMATION_TIME = 1000;

var HOT_CORNER_PRESSURE_THRESHOLD = 100; // pixels
var HOT_CORNER_PRESSURE_TIMEOUT = 1000; // ms

function isPopupMetaWindow(actor) {
    switch (actor.meta_window.get_window_type()) {
    case Meta.WindowType.DROPDOWN_MENU:
    case Meta.WindowType.POPUP_MENU:
    case Meta.WindowType.COMBO:
        return true;
    default:
        return false;
    }
}

var MonitorConstraint = GObject.registerClass({
    Properties: {
        'primary': GObject.ParamSpec.boolean('primary',
                                             'Primary', 'Track primary monitor',
                                             GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                             false),
        'index': GObject.ParamSpec.int('index',
                                       'Monitor index', 'Track specific monitor',
                                       GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                       -1, 64, -1),
        'work-area': GObject.ParamSpec.boolean('work-area',
                                               'Work-area', 'Track monitor\'s work-area',
                                               GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                               false),
    },
}, class MonitorConstraint extends Clutter.Constraint {
    _init(props) {
        this._primary = false;
        this._index = -1;
        this._workArea = false;

        super._init(props);
    }

    get primary() {
        return this._primary;
    }

    set primary(v) {
        if (v)
            this._index = -1;
        this._primary = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('primary');
    }

    get index() {
        return this._index;
    }

    set index(v) {
        this._primary = false;
        this._index = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('index');
    }

    // eslint-disable-next-line camelcase
    get work_area() {
        return this._workArea;
    }

    // eslint-disable-next-line camelcase
    set work_area(v) {
        if (v == this._workArea)
            return;
        this._workArea = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('work-area');
    }

    vfunc_set_actor(actor) {
        if (actor) {
            if (!this._monitorsChangedId) {
                this._monitorsChangedId =
                    Main.layoutManager.connect('monitors-changed', () => {
                        this.actor.queue_relayout();
                    });
            }

            if (!this._workareasChangedId) {
                this._workareasChangedId =
                    global.display.connect('workareas-changed', () => {
                        if (this._workArea)
                            this.actor.queue_relayout();
                    });
            }
        } else {
            if (this._monitorsChangedId)
                Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;

            if (this._workareasChangedId)
                global.display.disconnect(this._workareasChangedId);
            this._workareasChangedId = 0;
        }

        super.vfunc_set_actor(actor);
    }

    vfunc_update_allocation(actor, actorBox) {
        if (!this._primary && this._index < 0)
            return;

        if (!Main.layoutManager.primaryMonitor)
            return;

        let index;
        if (this._primary)
            index = Main.layoutManager.primaryIndex;
        else
            index = Math.min(this._index, Main.layoutManager.monitors.length - 1);

        let rect;
        if (this._workArea) {
            let workspaceManager = global.workspace_manager;
            let ws = workspaceManager.get_workspace_by_index(0);
            rect = ws.get_work_area_for_monitor(index);
        } else {
            rect = Main.layoutManager.monitors[index];
        }

        actorBox.init_rect(rect.x, rect.y, rect.width, rect.height);
    }
});

var Monitor = class Monitor {
    constructor(index, geometry, geometryScale) {
        this.index = index;
        this.x = geometry.x;
        this.y = geometry.y;
        this.width = geometry.width;
        this.height = geometry.height;
        this.geometry_scale = geometryScale;
    }

    get inFullscreen() {
        return global.display.get_monitor_in_fullscreen(this.index);
    }
};

const UiActor = GObject.registerClass(
class UiActor extends St.Widget {
    vfunc_get_preferred_width(_forHeight) {
        let width = global.stage.width;
        return [width, width];
    }

    vfunc_get_preferred_height(_forWidth) {
        let height = global.stage.height;
        return [height, height];
    }
});

const defaultParams = {
    trackFullscreen: false,
    affectsStruts: false,
    affectsInputRegion: true,
};

var LayoutManager = GObject.registerClass({
    Signals: { 'hot-corners-changed': {},
               'startup-complete': {},
               'startup-prepared': {},
               'monitors-changed': {},
               'system-modal-opened': {},
               'keyboard-visible-changed': { param_types: [GObject.TYPE_BOOLEAN] } },
}, class LayoutManager extends GObject.Object {
    _init() {
        super._init();

        this._rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;
        this.monitors = [];
        this.primaryMonitor = null;
        this.primaryIndex = -1;
        this.hotCorners = [];

        this._keyboardIndex = -1;
        this._rightPanelBarrier = null;

        this._inOverview = false;
        this._updateRegionIdle = 0;

        this._trackedActors = [];
        this._topActors = [];
        this._isPopupWindowVisible = false;
        this._startingUp = true;
        this._pendingLoadBackground = false;

        // Set up stage hierarchy to group all UI actors under one container.
        this.uiGroup = new UiActor({ name: 'uiGroup' });
        this.uiGroup.set_flags(Clutter.ActorFlags.NO_LAYOUT);

        global.stage.add_child(this.uiGroup);

        global.stage.remove_actor(global.window_group);
        this.uiGroup.add_actor(global.window_group);

        // Using addChrome() to add actors to uiGroup will position actors
        // underneath the top_window_group.
        // To insert actors at the top of uiGroup, we use addTopChrome() or
        // add the actor directly using uiGroup.add_actor().
        global.stage.remove_actor(global.top_window_group);
        this.uiGroup.add_actor(global.top_window_group);

        this.overviewGroup = new St.Widget({ name: 'overviewGroup',
                                             visible: false,
                                             reactive: true });
        this.addChrome(this.overviewGroup);

        this.screenShieldGroup = new St.Widget({
            name: 'screenShieldGroup',
            visible: false,
            clip_to_allocation: true,
            layout_manager: new Clutter.BinLayout(),
        });
        this.addChrome(this.screenShieldGroup);

        this.panelBox = new St.BoxLayout({ name: 'panelBox',
                                           vertical: true });
        this.addChrome(this.panelBox, { affectsStruts: true,
                                        trackFullscreen: true });
        this.panelBox.connect('notify::allocation',
                              this._panelBoxChanged.bind(this));

        this.modalDialogGroup = new St.Widget({ name: 'modalDialogGroup',
                                                layout_manager: new Clutter.BinLayout() });
        this.uiGroup.add_actor(this.modalDialogGroup);

        this.keyboardBox = new St.BoxLayout({ name: 'keyboardBox',
                                              reactive: true,
                                              track_hover: true });
        this.addTopChrome(this.keyboardBox);
        this._keyboardHeightNotifyId = 0;

        // A dummy actor that tracks the mouse or text cursor, based on the
        // position and size set in setDummyCursorGeometry.
        this.dummyCursor = new St.Widget({ width: 0, height: 0, opacity: 0 });
        this.uiGroup.add_actor(this.dummyCursor);

        let feedbackGroup = Meta.get_feedback_group_for_display(global.display);
        global.stage.remove_actor(feedbackGroup);
        this.uiGroup.add_actor(feedbackGroup);

        this._backgroundGroup = new Meta.BackgroundGroup();
        global.window_group.add_child(this._backgroundGroup);
        global.window_group.set_child_below_sibling(this._backgroundGroup, null);
        this._bgManagers = [];

        this._interfaceSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.interface',
        });

        this._interfaceSettings.connect('changed::enable-hot-corners',
                                        this._updateHotCorners.bind(this));

        // Need to update struts on new workspaces when they are added
        let workspaceManager = global.workspace_manager;
        workspaceManager.connect('notify::n-workspaces',
                                 this._queueUpdateRegions.bind(this));

        let display = global.display;
        display.connect('restacked',
                        this._windowsRestacked.bind(this));
        display.connect('in-fullscreen-changed',
                        this._updateFullscreen.bind(this));

        let monitorManager = Meta.MonitorManager.get();
        monitorManager.connect('monitors-changed',
                               this._monitorsChanged.bind(this));
        this._monitorsChanged();
    }

    // This is called by Main after everything else is constructed
    init() {
        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));

        this._loadBackground();
    }

    showOverview() {
        this.overviewGroup.show();

        this._inOverview = true;
        this._updateVisibility();
    }

    hideOverview() {
        this.overviewGroup.hide();

        this._inOverview = false;
        this._updateVisibility();
    }

    _sessionUpdated() {
        this._updateVisibility();
        this._queueUpdateRegions();
    }

    _updateMonitors() {
        let display = global.display;

        this.monitors = [];
        let nMonitors = display.get_n_monitors();
        for (let i = 0; i < nMonitors; i++) {
            this.monitors.push(new Monitor(i,
                                           display.get_monitor_geometry(i),
                                           display.get_monitor_scale(i)));
        }

        if (nMonitors == 0) {
            this.primaryIndex = this.bottomIndex = -1;
        } else if (nMonitors == 1) {
            this.primaryIndex = this.bottomIndex = 0;
        } else {
            // If there are monitors below the primary, then we need
            // to split primary from bottom.
            this.primaryIndex = this.bottomIndex = display.get_primary_monitor();
            for (let i = 0; i < this.monitors.length; i++) {
                let monitor = this.monitors[i];
                if (this._isAboveOrBelowPrimary(monitor)) {
                    if (monitor.y > this.monitors[this.bottomIndex].y)
                        this.bottomIndex = i;
                }
            }
        }
        if (this.primaryIndex != -1) {
            this.primaryMonitor = this.monitors[this.primaryIndex];
            this.bottomMonitor = this.monitors[this.bottomIndex];

            if (this._pendingLoadBackground) {
                this._loadBackground();
                this._pendingLoadBackground = false;
            }
        } else {
            this.primaryMonitor = null;
            this.bottomMonitor = null;
        }
    }

    _updateHotCorners() {
        // destroy old hot corners
        this.hotCorners.forEach(corner => {
            if (corner)
                corner.destroy();
        });
        this.hotCorners = [];

        if (!this._interfaceSettings.get_boolean('enable-hot-corners')) {
            this.emit('hot-corners-changed');
            return;
        }

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
    }

    _addBackgroundMenu(bgManager) {
        BackgroundMenu.addBackgroundMenu(bgManager.backgroundActor, this);
    }

    _createBackgroundManager(monitorIndex) {
        let bgManager = new Background.BackgroundManager({ container: this._backgroundGroup,
                                                           layoutManager: this,
                                                           monitorIndex });

        bgManager.connect('changed', this._addBackgroundMenu.bind(this));
        this._addBackgroundMenu(bgManager);

        return bgManager;
    }

    _showSecondaryBackgrounds() {
        for (let i = 0; i < this.monitors.length; i++) {
            if (i != this.primaryIndex) {
                let backgroundActor = this._bgManagers[i].backgroundActor;
                backgroundActor.show();
                backgroundActor.opacity = 0;
                backgroundActor.ease({
                    opacity: 255,
                    duration: BACKGROUND_FADE_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            }
        }
    }

    _waitLoaded(bgManager) {
        return new Promise(resolve => {
            const id = bgManager.connect('loaded', () => {
                bgManager.disconnect(id);
                resolve();
            });
        });
    }

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];

        if (Main.sessionMode.isGreeter)
            return Promise.resolve();

        for (let i = 0; i < this.monitors.length; i++) {
            let bgManager = this._createBackgroundManager(i);
            this._bgManagers.push(bgManager);

            if (i != this.primaryIndex && this._startingUp)
                bgManager.backgroundActor.hide();
        }

        return Promise.all(this._bgManagers.map(this._waitLoaded));
    }

    _updateKeyboardBox() {
        this.keyboardBox.set_position(this.keyboardMonitor.x,
                                      this.keyboardMonitor.y + this.keyboardMonitor.height);
        this.keyboardBox.set_size(this.keyboardMonitor.width, -1);
    }

    _updateBoxes() {
        this.screenShieldGroup.set_position(0, 0);
        this.screenShieldGroup.set_size(global.screen_width, global.screen_height);

        if (!this.primaryMonitor)
            return;

        this.panelBox.set_position(this.primaryMonitor.x, this.primaryMonitor.y);
        this.panelBox.set_size(this.primaryMonitor.width, -1);

        this.keyboardIndex = this.primaryIndex;
    }

    _panelBoxChanged() {
        this._updatePanelBarrier();

        let size = this.panelBox.height;
        this.hotCorners.forEach(corner => {
            if (corner)
                corner.setBarrierSize(size);
        });
    }

    _updatePanelBarrier() {
        if (this._rightPanelBarrier) {
            this._rightPanelBarrier.destroy();
            this._rightPanelBarrier = null;
        }

        if (!this.primaryMonitor)
            return;

        if (this.panelBox.height) {
            let primary = this.primaryMonitor;

            this._rightPanelBarrier = new Meta.Barrier({ display: global.display,
                                                         x1: primary.x + primary.width, y1: primary.y,
                                                         x2: primary.x + primary.width, y2: primary.y + this.panelBox.height,
                                                         directions: Meta.BarrierDirection.NEGATIVE_X });
        }
    }

    _monitorsChanged() {
        this._updateMonitors();
        this._updateBoxes();
        this._updateHotCorners();
        this._updateBackgrounds();
        this._updateFullscreen();
        this._updateVisibility();
        this._queueUpdateRegions();

        this.emit('monitors-changed');
    }

    _isAboveOrBelowPrimary(monitor) {
        let primary = this.monitors[this.primaryIndex];
        let monitorLeft = monitor.x, monitorRight = monitor.x + monitor.width;
        let primaryLeft = primary.x, primaryRight = primary.x + primary.width;

        if ((monitorLeft >= primaryLeft && monitorLeft < primaryRight) ||
            (monitorRight > primaryLeft && monitorRight <= primaryRight) ||
            (primaryLeft >= monitorLeft && primaryLeft < monitorRight) ||
            (primaryRight > monitorLeft && primaryRight <= monitorRight))
            return true;

        return false;
    }

    get currentMonitor() {
        let index = global.display.get_current_monitor();
        return this.monitors[index];
    }

    get keyboardMonitor() {
        return this.monitors[this.keyboardIndex];
    }

    get focusIndex() {
        let i = Main.layoutManager.primaryIndex;

        if (global.stage.key_focus != null)
            i = this.findIndexForActor(global.stage.key_focus);
        else if (global.display.focus_window != null)
            i = global.display.focus_window.get_monitor();
        return i;
    }

    get focusMonitor() {
        if (this.focusIndex < 0)
            return null;
        return this.monitors[this.focusIndex];
    }

    set keyboardIndex(v) {
        this._keyboardIndex = v;
        this._updateKeyboardBox();
    }

    get keyboardIndex() {
        return this._keyboardIndex;
    }

    _loadBackground() {
        if (!this.primaryMonitor) {
            this._pendingLoadBackground = true;
            return;
        }
        this._systemBackground = new Background.SystemBackground();
        this._systemBackground.hide();

        global.stage.insert_child_below(this._systemBackground, null);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._systemBackground.add_constraint(constraint);

        let signalId = this._systemBackground.connect('loaded', () => {
            this._systemBackground.disconnect(signalId);

            // We're mostly prepared for the startup animation
            // now, but since a lot is going on asynchronously
            // during startup, let's defer the startup animation
            // until the event loop is uncontended and idle.
            // This helps to prevent us from running the animation
            // when the system is bogged down
            const id = GLib.idle_add(GLib.PRIORITY_LOW, () => {
                this._systemBackground.show();
                global.stage.show();
                this._prepareStartupAnimation();
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] Startup Animation');
        });
    }

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

    async _prepareStartupAnimation() {
        // During the initial transition, add a simple actor to block all events,
        // so they don't get delivered to X11 windows that have been transformed.
        this._coverPane = new Clutter.Actor({ opacity: 0,
                                              width: global.screen_width,
                                              height: global.screen_height,
                                              reactive: true });
        this.addChrome(this._coverPane);

        if (Meta.is_restart()) {
            // On restart, we don't do an animation. Force an update of the
            // regions immediately so that maximized windows restore to the
            // right size taking struts into account.
            this._updateRegions();
        } else if (Main.sessionMode.isGreeter) {
            this.panelBox.translation_y = -this.panelBox.height;
        } else {
            // We need to force an update of the regions now before we scale
            // the UI group to get the correct allocation for the struts.
            this._updateRegions();

            this.keyboardBox.hide();

            let monitor = this.primaryMonitor;
            let x = monitor.x + monitor.width / 2.0;
            let y = monitor.y + monitor.height / 2.0;

            this.uiGroup.set_pivot_point(x / global.screen_width,
                                         y / global.screen_height);
            this.uiGroup.scale_x = this.uiGroup.scale_y = 0.75;
            this.uiGroup.opacity = 0;
            global.window_group.set_clip(monitor.x, monitor.y, monitor.width, monitor.height);

            await this._updateBackgrounds();
        }

        this.emit('startup-prepared');

        this._startupAnimation();
    }

    _startupAnimation() {
        if (Meta.is_restart())
            this._startupAnimationComplete();
        else if (Main.sessionMode.isGreeter)
            this._startupAnimationGreeter();
        else
            this._startupAnimationSession();
    }

    _startupAnimationGreeter() {
        this.panelBox.ease({
            translation_y: 0,
            duration: STARTUP_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._startupAnimationComplete(),
        });
    }

    _startupAnimationSession() {
        this.uiGroup.ease({
            scale_x: 1,
            scale_y: 1,
            opacity: 255,
            duration: STARTUP_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._startupAnimationComplete(),
        });
    }

    _startupAnimationComplete() {
        this._coverPane.destroy();
        this._coverPane = null;

        this._systemBackground.destroy();
        this._systemBackground = null;

        this._startingUp = false;

        this.keyboardBox.show();

        if (!Main.sessionMode.isGreeter) {
            this._showSecondaryBackgrounds();
            global.window_group.remove_clip();
        }

        this._queueUpdateRegions();

        this.emit('startup-complete');
    }

    showKeyboard() {
        this.keyboardBox.show();
        this.keyboardBox.ease({
            translation_y: -this.keyboardBox.height,
            opacity: 255,
            duration: KEYBOARD_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._showKeyboardComplete();
            },
        });
        this.emit('keyboard-visible-changed', true);
    }

    _showKeyboardComplete() {
        // Poke Chrome to update the input shape; it doesn't notice
        // anchor point changes
        this._updateRegions();

        this._keyboardHeightNotifyId = this.keyboardBox.connect('notify::height', () => {
            this.keyboardBox.translation_y = -this.keyboardBox.height;
        });
    }

    hideKeyboard(immediate) {
        if (this._keyboardHeightNotifyId) {
            this.keyboardBox.disconnect(this._keyboardHeightNotifyId);
            this._keyboardHeightNotifyId = 0;
        }
        this.keyboardBox.ease({
            translation_y: 0,
            opacity: 0,
            duration: immediate ? 0 : KEYBOARD_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
            onComplete: () => {
                this._hideKeyboardComplete();
            },
        });

        this.emit('keyboard-visible-changed', false);
    }

    _hideKeyboardComplete() {
        this.keyboardBox.hide();
        this._updateRegions();
    }

    // setDummyCursorGeometry:
    //
    // The cursor dummy is a standard widget commonly used for popup
    // menus and box pointers to track, as the box pointer API only
    // tracks actors. If you want to pop up a menu based on where the
    // user clicked, or where the text cursor is, the cursor dummy
    // is what you should use. Given that the menu should not track
    // the actual mouse pointer as it moves, you need to call this
    // function before you show the menu to ensure it is at the right
    // position and has the right size.
    setDummyCursorGeometry(x, y, w, h) {
        this.dummyCursor.set_position(Math.round(x), Math.round(y));
        this.dummyCursor.set_size(Math.round(w), Math.round(h));
    }

    // addChrome:
    // @actor: an actor to add to the chrome
    // @params: (optional) additional params
    //
    // Adds @actor to the chrome, and (unless %affectsInputRegion in
    // @params is %false) extends the input region to include it.
    // Changes in @actor's size, position, and visibility will
    // automatically result in appropriate changes to the input
    // region.
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
    addChrome(actor, params) {
        this.uiGroup.add_actor(actor);
        if (this.uiGroup.contains(global.top_window_group))
            this.uiGroup.set_child_below_sibling(actor, global.top_window_group);
        this._trackActor(actor, params);
    }

    // addTopChrome:
    // @actor: an actor to add to the chrome
    // @params: (optional) additional params
    //
    // Like addChrome(), but adds @actor above all windows, including popups.
    addTopChrome(actor, params) {
        this.uiGroup.add_actor(actor);
        this._trackActor(actor, params);
    }

    // trackChrome:
    // @actor: a descendant of the chrome to begin tracking
    // @params: parameters describing how to track @actor
    //
    // Tells the chrome to track @actor. This can be used to extend the
    // struts or input region to cover specific children.
    //
    // @params can have any of the same values as in addChrome(),
    // though some possibilities don't make sense. By default, @actor has
    // the same params as its chrome ancestor.
    trackChrome(actor, params = {}) {
        let ancestor = actor.get_parent();
        let index = this._findActor(ancestor);
        while (ancestor && index == -1) {
            ancestor = ancestor.get_parent();
            index = this._findActor(ancestor);
        }

        let ancestorData = ancestor
            ? this._trackedActors[index]
            : defaultParams;
        // We can't use Params.parse here because we want to drop
        // the extra values like ancestorData.actor
        for (let prop in defaultParams) {
            if (!Object.prototype.hasOwnProperty.call(params, prop))
                params[prop] = ancestorData[prop];
        }

        this._trackActor(actor, params);
    }

    // untrackChrome:
    // @actor: an actor previously tracked via trackChrome()
    //
    // Undoes the effect of trackChrome()
    untrackChrome(actor) {
        this._untrackActor(actor);
    }

    // removeChrome:
    // @actor: a chrome actor
    //
    // Removes @actor from the chrome
    removeChrome(actor) {
        this.uiGroup.remove_actor(actor);
        this._untrackActor(actor);
    }

    _findActor(actor) {
        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (actorData.actor == actor)
                return i;
        }
        return -1;
    }

    _trackActor(actor, params) {
        if (this._findActor(actor) != -1)
            throw new Error('trying to re-track existing chrome actor');

        let actorData = Params.parse(params, defaultParams);
        actorData.actor = actor;
        actorData.visibleId = actor.connect('notify::visible',
                                            this._queueUpdateRegions.bind(this));
        actorData.allocationId = actor.connect('notify::allocation',
                                               this._queueUpdateRegions.bind(this));
        actorData.destroyId = actor.connect('destroy',
                                            this._untrackActor.bind(this));
        // Note that destroying actor will unset its parent, so we don't
        // need to connect to 'destroy' too.

        this._trackedActors.push(actorData);
        this._updateActorVisibility(actorData);
        this._queueUpdateRegions();
    }

    _untrackActor(actor) {
        let i = this._findActor(actor);

        if (i == -1)
            return;
        let actorData = this._trackedActors[i];

        this._trackedActors.splice(i, 1);
        actor.disconnect(actorData.visibleId);
        actor.disconnect(actorData.allocationId);
        actor.disconnect(actorData.destroyId);

        this._queueUpdateRegions();
    }

    _updateActorVisibility(actorData) {
        if (!actorData.trackFullscreen)
            return;

        let monitor = this.findMonitorForActor(actorData.actor);
        actorData.actor.visible = !(global.window_group.visible &&
                                    monitor &&
                                    monitor.inFullscreen);
    }

    _updateVisibility() {
        let windowsVisible = Main.sessionMode.hasWindows && !this._inOverview;

        global.window_group.visible = windowsVisible;
        global.top_window_group.visible = windowsVisible;

        this._trackedActors.forEach(this._updateActorVisibility.bind(this));
    }

    getWorkAreaForMonitor(monitorIndex) {
        // Assume that all workspaces will have the same
        // struts and pick the first one.
        let workspaceManager = global.workspace_manager;
        let ws = workspaceManager.get_workspace_by_index(0);
        return ws.get_work_area_for_monitor(monitorIndex);
    }

    // This call guarantees that we return some monitor to simplify usage of it
    // In practice all tracked actors should be visible on some monitor anyway
    findIndexForActor(actor) {
        let [x, y] = actor.get_transformed_position();
        let [w, h] = actor.get_transformed_size();
        let rect = new Meta.Rectangle({ x, y, width: w, height: h });
        return global.display.get_monitor_index_for_rect(rect);
    }

    findMonitorForActor(actor) {
        let index = this.findIndexForActor(actor);
        if (index >= 0 && index < this.monitors.length)
            return this.monitors[index];
        return null;
    }

    _queueUpdateRegions() {
        if (this._startingUp)
            return;

        if (!this._updateRegionIdle) {
            this._updateRegionIdle = Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                                                    this._updateRegions.bind(this));
        }
    }

    _getWindowActorsForWorkspace(workspace) {
        return global.get_window_actors().filter(actor => {
            let win = actor.meta_window;
            return win.located_on_workspace(workspace);
        });
    }

    _updateFullscreen() {
        this._updateVisibility();
        this._queueUpdateRegions();
    }

    _windowsRestacked() {
        let changed = false;

        if (this._isPopupWindowVisible != global.top_window_group.get_children().some(isPopupMetaWindow))
            changed = true;

        if (changed) {
            this._updateVisibility();
            this._queueUpdateRegions();
        }
    }

    _updateRegions() {
        if (this._updateRegionIdle) {
            Meta.later_remove(this._updateRegionIdle);
            delete this._updateRegionIdle;
        }

        // No need to update when we have a modal.
        if (Main.modalCount > 0)
            return GLib.SOURCE_REMOVE;

        let rects = [], struts = [], i;
        let isPopupMenuVisible = global.top_window_group.get_children().some(isPopupMetaWindow);
        let wantsInputRegion = !isPopupMenuVisible;

        for (i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (!(actorData.affectsInputRegion && wantsInputRegion) && !actorData.affectsStruts)
                continue;

            let [x, y] = actorData.actor.get_transformed_position();
            let [w, h] = actorData.actor.get_transformed_size();
            x = Math.round(x);
            y = Math.round(y);
            w = Math.round(w);
            h = Math.round(h);

            if (actorData.affectsInputRegion && wantsInputRegion && actorData.actor.get_paint_visibility())
                rects.push(new Meta.Rectangle({ x, y, width: w, height: h }));

            let monitor = null;
            if (actorData.affectsStruts)
                monitor = this.findMonitorForActor(actorData.actor);

            if (monitor) {
                // Limit struts to the size of the screen
                let x1 = Math.max(x, 0);
                let x2 = Math.min(x + w, global.screen_width);
                let y1 = Math.max(y, 0);
                let y2 = Math.min(y + h, global.screen_height);

                // Metacity wants to know what side of the monitor the
                // strut is considered to be attached to. First, we find
                // the monitor that contains the strut. If the actor is
                // only touching one edge, or is touching the entire
                // border of that monitor, then it's obvious which side
                // to call it. If it's in a corner, we pick a side
                // arbitrarily. If it doesn't touch any edges, or it
                // spans the width/height across the middle of the
                // screen, then we don't create a strut for it at all.

                let side;
                if (x1 <= monitor.x && x2 >= monitor.x + monitor.width) {
                    if (y1 <= monitor.y)
                        side = Meta.Side.TOP;
                    else if (y2 >= monitor.y + monitor.height)
                        side = Meta.Side.BOTTOM;
                    else
                        continue;
                } else if (y1 <= monitor.y && y2 >= monitor.y + monitor.height) {
                    if (x1 <= monitor.x)
                        side = Meta.Side.LEFT;
                    else if (x2 >= monitor.x + monitor.width)
                        side = Meta.Side.RIGHT;
                    else
                        continue;
                } else if (x1 <= monitor.x) {
                    side = Meta.Side.LEFT;
                } else if (y1 <= monitor.y) {
                    side = Meta.Side.TOP;
                } else if (x2 >= monitor.x + monitor.width) {
                    side = Meta.Side.RIGHT;
                } else if (y2 >= monitor.y + monitor.height) {
                    side = Meta.Side.BOTTOM;
                } else {
                    continue;
                }

                let strutRect = new Meta.Rectangle({ x: x1, y: y1, width: x2 - x1, height: y2 - y1 });
                let strut = new Meta.Strut({ rect: strutRect, side });
                struts.push(strut);
            }
        }

        global.set_stage_input_region(rects);
        this._isPopupWindowVisible = isPopupMenuVisible;

        let workspaceManager = global.workspace_manager;
        for (let w = 0; w < workspaceManager.n_workspaces; w++) {
            let workspace = workspaceManager.get_workspace_by_index(w);
            workspace.set_builtin_struts(struts);
        }

        return GLib.SOURCE_REMOVE;
    }

    modalEnded() {
        // We don't update the stage input region while in a modal,
        // so queue an update now.
        this._queueUpdateRegions();
    }
});


// HotCorner:
//
// This class manages a "hot corner" that can toggle switching to
// overview.
var HotCorner = GObject.registerClass(
class HotCorner extends Clutter.Actor {
    _init(layoutManager, monitor, x, y) {
        super._init();

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
                                                    Shell.ActionMode.NORMAL |
                                                    Shell.ActionMode.OVERVIEW);
        this._pressureBarrier.connect('trigger', this._toggleOverview.bind(this));

        let px = 0.0;
        let py = 0.0;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL) {
            px = 1.0;
            py = 0.0;
        }

        this._ripples = new Ripples.Ripples(px, py, 'ripple-box');
        this._ripples.addTo(layoutManager.uiGroup);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    setBarrierSize(size) {
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
    }

    _setupFallbackCornerIfNeeded(layoutManager) {
        if (!global.display.supports_extended_barriers()) {
            this.set({
                name: 'hot-corner-environs',
                x: this._x,
                y: this._y,
                width: 3,
                height: 3,
                reactive: true,
            });

            this._corner = new Clutter.Actor({ name: 'hot-corner',
                                               width: 1,
                                               height: 1,
                                               opacity: 0,
                                               reactive: true });
            this._corner._delegate = this;

            this.add_child(this._corner);
            layoutManager.addChrome(this);

            if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL) {
                this._corner.set_position(this.width - this._corner.width, 0);
                this.set_pivot_point(1.0, 0.0);
                this.translation_x = -this.width;
            } else {
                this._corner.set_position(0, 0);
            }

            this._corner.connect('enter-event',
                                 this._onCornerEntered.bind(this));
            this._corner.connect('leave-event',
                                 this._onCornerLeft.bind(this));
        }
    }

    _onDestroy() {
        this.setBarrierSize(0);
        this._pressureBarrier.destroy();
        this._pressureBarrier = null;

        this._ripples.destroy();
    }

    _toggleOverview() {
        if (this._monitor.inFullscreen && !Main.overview.visible)
            return;

        if (Main.overview.shouldToggleByCornerOrButton()) {
            Main.overview.toggle();
            if (Main.overview.animationInProgress)
                this._ripples.playAnimation(this._x, this._y);
        }
    }

    handleDragOver(source, _actor, _x, _y, _time) {
        if (source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        this._toggleOverview();

        return DND.DragMotionResult.CONTINUE;
    }

    _onCornerEntered() {
        if (!this._entered) {
            this._entered = true;
            this._toggleOverview();
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _onCornerLeft(actor, event) {
        if (event.get_related() != this)
            this._entered = false;
        // Consume event, otherwise this will confuse onEnvironsLeft
        return Clutter.EVENT_STOP;
    }

    vfunc_leave_event(crossingEvent) {
        if (crossingEvent.related != this._corner)
            this._entered = false;
        return Clutter.EVENT_PROPAGATE;
    }
});

var PressureBarrier = class PressureBarrier {
    constructor(threshold, timeout, actionMode) {
        this._threshold = threshold;
        this._timeout = timeout;
        this._actionMode = actionMode;
        this._barriers = [];
        this._eventFilter = null;

        this._isTriggered = false;
        this._reset();
    }

    addBarrier(barrier) {
        barrier._pressureHitId = barrier.connect('hit', this._onBarrierHit.bind(this));
        barrier._pressureLeftId = barrier.connect('left', this._onBarrierLeft.bind(this));

        this._barriers.push(barrier);
    }

    _disconnectBarrier(barrier) {
        barrier.disconnect(barrier._pressureHitId);
        barrier.disconnect(barrier._pressureLeftId);
    }

    removeBarrier(barrier) {
        this._disconnectBarrier(barrier);
        this._barriers.splice(this._barriers.indexOf(barrier), 1);
    }

    destroy() {
        this._barriers.forEach(this._disconnectBarrier.bind(this));
        this._barriers = [];
    }

    setEventFilter(filter) {
        this._eventFilter = filter;
    }

    _reset() {
        this._barrierEvents = [];
        this._currentPressure = 0;
        this._lastTime = 0;
    }

    _isHorizontal(barrier) {
        return barrier.y1 == barrier.y2;
    }

    _getDistanceAcrossBarrier(barrier, event) {
        if (this._isHorizontal(barrier))
            return Math.abs(event.dy);
        else
            return Math.abs(event.dx);
    }

    _getDistanceAlongBarrier(barrier, event) {
        if (this._isHorizontal(barrier))
            return Math.abs(event.dx);
        else
            return Math.abs(event.dy);
    }

    _trimBarrierEvents() {
        // Events are guaranteed to be sorted in time order from
        // oldest to newest, so just look for the first old event,
        // and then chop events after that off.
        let i = 0;
        let threshold = this._lastTime - this._timeout;

        while (i < this._barrierEvents.length) {
            let [time, distance_] = this._barrierEvents[i];
            if (time >= threshold)
                break;
            i++;
        }

        let firstNewEvent = i;

        for (i = 0; i < firstNewEvent; i++) {
            let [time_, distance] = this._barrierEvents[i];
            this._currentPressure -= distance;
        }

        this._barrierEvents = this._barrierEvents.slice(firstNewEvent);
    }

    _onBarrierLeft(barrier, _event) {
        barrier._isHit = false;
        if (this._barriers.every(b => !b._isHit)) {
            this._reset();
            this._isTriggered = false;
        }
    }

    _trigger() {
        this._isTriggered = true;
        this.emit('trigger');
        this._reset();
    }

    _onBarrierHit(barrier, event) {
        barrier._isHit = true;

        // If we've triggered the barrier, wait until the pointer has the
        // left the barrier hitbox until we trigger it again.
        if (this._isTriggered)
            return;

        if (this._eventFilter && this._eventFilter(event))
            return;

        // Throw out all events not in the proper keybinding mode
        if (!(this._actionMode & Main.actionMode))
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
};
Signals.addSignalMethods(PressureBarrier.prototype);
