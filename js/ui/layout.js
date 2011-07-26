/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const Meta = imports.gi.Meta;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;

const State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3
};

const HOT_CORNER_ACTIVATION_TIMEOUT = 0.5;

function LayoutManager() {
    this._init.apply(this, arguments);
}

LayoutManager.prototype = {
    _init: function () {
        this._rtl = (St.Widget.get_default_direction() == St.TextDirection.RTL);
        this.monitors = [];
        this.primaryMonitor = null;
        this.primaryIndex = -1;
        this._hotCorners = [];
        this.bottomBox = new Clutter.Group();
        this.topBox = new Clutter.Group({ clip_to_allocation: true });
        this.bottomBox.add_actor(this.topBox);

        global.screen.connect('monitors-changed', Lang.bind(this, this._monitorsChanged));
        this._updateMonitors();

        Main.connect('layout-initialized', Lang.bind(this, this._initChrome));

        Main.connect('main-ui-initialized', Lang.bind(this, this._finishInit));
    },

    _initChrome: function() {
        Main.chrome.addActor(this.bottomBox, { affectsStruts: false,
                                               visibleInFullscreen: true });
    },

    // _updateHotCorners needs access to Main.panel
    _finishInit: function() {
        this._updateHotCorners();

        this.topBox.height = Main.messageTray.actor.height;
        this.topBox.y = - Main.messageTray.actor.height;

        this._keyboardState = Main.keyboard.actor.visible ? State.SHOWN : State.HIDDEN;
        this._traySummoned = true;

        Main.keyboard.actor.connect('allocation-changed', Lang.bind(this, this._reposition));
    },

    _reposition: function () {
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function () { this._updateForKeyboard(); }));
    },

    _updateForKeyboard: function () {
        if (Tweener.isTweening(this.bottomBox))
            return;

        this.topBox.y = - Main.messageTray.actor.height;
        let bottom = this.bottomMonitor.y + this.bottomMonitor.height;
        if (this._keyboardState == State.SHOWN)
            this.bottomBox.y = bottom - Main.keyboard.actor.height;
        else {
            this.bottomBox.y = bottom;
            this._keyboardState = State.HIDDEN;
        }

    },

    updateForTray: function () {
        if (this._keyboardState == State.SHOWN) {
            if (this._traySummoned) {
                Main.messageTray.lock();
                this._traySummoned = false;
            }
            else {
                Main.messageTray.unlock();
                this._traySummoned = true;
            }
        }
        else {
            Main.messageTray.unlock();
            this._traySummoned = false;
        }
    },

    showKeyboard: function () {
        let bottom = this.bottomMonitor.y + this.bottomMonitor.height;
        // Keyboard height cannot be found directly since the first
        // call to this method may be when Keyboard.Keyboard() has
        // not returned; therefore the keyboard would be null
        Tweener.addTween(this.bottomBox,
                         { y: bottom - Main.keyboard.actor.height,
                           time: 0.5,
                           transition: 'easeOutQuad',
                         });
        this._keyboardState = State.SHOWN;
        this.updateForTray();
    },

    hideKeyboard: function () {
        let bottom = this.bottomMonitor.y + this.bottomMonitor.height;
        Tweener.addTween(this.bottomBox,
                         { y: bottom,
                           time: 0.5,
                           transition: 'easeOutQuad'
                         });
        this._keyboardState = State.HIDDEN;
        this.updateForTray();
    },

    // This is called by Main after everything else is constructed;
    // _updateHotCorners needs access to Main.panel, which didn't exist
    // yet when the LayoutManager was constructed.
    init: function() {
        global.screen.connect('monitors-changed', Lang.bind(this, this._monitorsChanged));
        this._updateHotCorners();
    },

    _updateMonitors: function() {
        let screen = global.screen;

        this.monitors = [];
        let nMonitors = screen.get_n_monitors();
        for (let i = 0; i < nMonitors; i++)
            this.monitors.push(screen.get_monitor_geometry(i));

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

        this.bottomBox.set_position(0, this.bottomMonitor.y + this.bottomMonitor.height);
        this.bottomBox.width = this.bottomMonitor.width;
    },

    _updateHotCorners: function() {
        // destroy old hot corners
        for (let i = 0; i < this._hotCorners.length; i++)
            this._hotCorners[i].destroy();
        this._hotCorners = [];

        // build new hot corners
        for (let i = 0; i < this.monitors.length; i++) {
            if (i == this.primaryIndex)
                continue;

            let monitor = this.monitors[i];
            let cornerX = this._rtl ? monitor.x + monitor.width : monitor.x;
            let cornerY = monitor.y;

            let haveTopLeftCorner = true;

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

            if (!haveTopLeftCorner)
                continue;

            let corner = new HotCorner();
            this._hotCorners.push(corner);
            corner.actor.set_position(cornerX, cornerY);
            Main.chrome.addActor(corner.actor, { affectsStruts: false });
        }
    },

    _monitorsChanged: function() {
        this._updateMonitors();
        this._updateHotCorners();

        this.emit('monitors-changed');
    },

    _isAboveOrBelowPrimary: function(monitor) {
        let primary = this.monitors[this.primaryIndex];
        let monitorLeft = monitor.x, monitorRight = monitor.x + monitor.width;
        let primaryLeft = primary.x, primaryRight = primary.x + primary.width;

        if ((monitorLeft >= primaryLeft && monitorLeft <= primaryRight) ||
            (monitorRight >= primaryLeft && monitorRight <= primaryRight) ||
            (primaryLeft >= monitorLeft && primaryLeft <= monitorRight) ||
            (primaryRight >= monitorLeft && primaryRight <= monitorRight))
            return true;

        return false;
    },

    get focusIndex() {
        let screen = global.screen;
        let display = screen.get_display();
        let focusWindow = display.focus_window;

        if (focusWindow) {
            let wrect = focusWindow.get_outer_rect();
            for (let i = 0; i < this.monitors.length; i++) {
                let monitor = this.monitors[i];

                if (monitor.x <= wrect.x && monitor.y <= wrect.y &&
                    monitor.x + monitor.width > wrect.x &&
                    monitor.y + monitor.height > wrect.y)
                    return i;
            }
        }

        return this.primaryIndex;
    },

    get focusMonitor() {
        return this.monitors[this.focusIndex];
    }
};
Signals.addSignalMethods(LayoutManager.prototype);


// HotCorner:
//
// This class manages a "hot corner" that can toggle switching to
// overview.
function HotCorner() {
    this._init();
}

HotCorner.prototype = {
    _init : function() {
        // We use this flag to mark the case where the user has entered the
        // hot corner and has not left both the hot corner and a surrounding
        // guard area (the "environs"). This avoids triggering the hot corner
        // multiple times due to an accidental jitter.
        this._entered = false;

        this.actor = new Clutter.Group({ name: 'hot-corner-environs',
                                         width: 3,
                                         height: 3,
                                         reactive: true });

        this._corner = new Clutter.Rectangle({ name: 'hot-corner',
                                               width: 1,
                                               height: 1,
                                               opacity: 0,
                                               reactive: true });
        this._corner._delegate = this;

        this.actor.add_actor(this._corner);

        if (St.Widget.get_default_direction() == St.TextDirection.RTL) {
            this._corner.set_position(this.actor.width - this._corner.width, 0);
            this.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
        } else {
            this._corner.set_position(0, 0);
        }

        this._activationTime = 0;

        this.actor.connect('leave-event',
                           Lang.bind(this, this._onEnvironsLeft));

        // Clicking on the hot corner environs should result in the
        // same behavior as clicking on the hot corner.
        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onCornerClicked));

        // In addition to being triggered by the mouse enter event,
        // the hot corner can be triggered by clicking on it. This is
        // useful if the user wants to undo the effect of triggering
        // the hot corner once in the hot corner.
        this._corner.connect('enter-event',
                             Lang.bind(this, this._onCornerEntered));
        this._corner.connect('button-release-event',
                             Lang.bind(this, this._onCornerClicked));
        this._corner.connect('leave-event',
                             Lang.bind(this, this._onCornerLeft));
    },

    destroy: function() {
        this.actor.destroy();
    },

    _addRipple : function(delay, time, startScale, startOpacity, finalScale, finalOpacity) {
        // We draw a ripple by using a source image and animating it scaling
        // outwards and fading away. We want the ripples to move linearly
        // or it looks unrealistic, but if the opacity of the ripple goes
        // linearly to zero it fades away too quickly, so we use Tweener's
        // 'onUpdate' to give a non-linear curve to the fade-away and make
        // it more visible in the middle section.

        let [x, y] = this._corner.get_transformed_position();
        let ripple = new St.BoxLayout({ style_class: 'ripple-box',
                                        opacity: 255 * Math.sqrt(startOpacity),
                                        scale_x: startScale,
                                        scale_y: startScale,
                                        x: x,
                                        y: y });
        ripple._opacity =  startOpacity;
        if (ripple.get_direction() == St.TextDirection.RTL)
            ripple.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
        Tweener.addTween(ripple, { _opacity: finalOpacity,
                                   scale_x: finalScale,
                                   scale_y: finalScale,
                                   delay: delay,
                                   time: time,
                                   transition: 'linear',
                                   onUpdate: function() { ripple.opacity = 255 * Math.sqrt(ripple._opacity); },
                                   onComplete: function() { ripple.destroy(); } });
        Main.uiGroup.add_actor(ripple);
    },

    rippleAnimation: function() {
        // Show three concentric ripples expanding outwards; the exact
        // parameters were found by trial and error, so don't look
        // for them to make perfect sense mathematically

        //              delay  time  scale opacity => scale opacity
        this._addRipple(0.0,   0.83,  0.25,  1.0,    1.5,  0.0);
        this._addRipple(0.05,  1.0,   0.0,   0.7,    1.25, 0.0);
        this._addRipple(0.35,  1.0,   0.0,   0.3,    1,    0.0);
    },

    handleDragOver: function(source, actor, x, y, time) {
        if (source != Main.xdndHandler)
            return;

        if (!Main.overview.visible && !Main.overview.animationInProgress) {
            this.rippleAnimation();
            Main.overview.showTemporarily();
            Main.overview.beginItemDrag(actor);
        }
    },

    _onCornerEntered : function() {
        if (!this._entered) {
            this._entered = true;
            if (!Main.overview.animationInProgress) {
                this._activationTime = Date.now() / 1000;

                this.rippleAnimation();
                Main.overview.toggle();
            }
        }
        return false;
    },

    _onCornerClicked : function() {
        if (this.shouldToggleOverviewOnClick())
            Main.overview.toggle();
        return true;
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
    },

    // Checks if the Activities button is currently sensitive to
    // clicks. The first call to this function within the
    // HOT_CORNER_ACTIVATION_TIMEOUT time of the hot corner being
    // triggered will return false. This avoids opening and closing
    // the overview if the user both triggered the hot corner and
    // clicked the Activities button.
    shouldToggleOverviewOnClick: function() {
        if (Main.overview.animationInProgress)
            return false;
        if (this._activationTime == 0 || Date.now() / 1000 - this._activationTime > HOT_CORNER_ACTIVATION_TIMEOUT)
            return true;
        return false;
    }
};
