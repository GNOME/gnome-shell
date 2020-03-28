/* exported Dock, State, Mode */

const { Clutter, Gio, GLib, GObject, Gtk, Meta, Shell, St } = imports.gi;

const Main = imports.ui.main;
const Dash = imports.ui.dash;
const OverviewControls = imports.ui.overviewControls;
const ViewSelector = imports.ui.viewSelector;
const Layout = imports.ui.layout;

var State = {
    HIDDEN: 0,
    SHOWING: 1,
    SHOWN: 2,
    HIDING: 3,
};

var Mode = {
    DEFAULT: 0,
    AUTOHIDE: 1,
    FIXED: 2,
};

var DOCK_PRESSURE_THRESHOLD = 100; // ms
var DOCK_PRESSURE_TIMEOUT = 1000; // ms

var DOCK_ANIMATION_DURATION = 0.2; // seconds
var DOCK_SHOW_DELAY = 0.25; // seconds
var DOCK_HIDE_DELAY = 0.2; // seconds

var Dock = GObject.registerClass({
    Signals: {
        showing: {},
        hiding: {},
    },
}, class Dock extends St.Bin {
    _init(params) {
        const { monitorIndex } = params;
        this._monitorIndex = monitorIndex;
        this._dockSettings = new Gio.Settings({ schema_id: 'org.gnome.shell.dock' });

        this._dockMode = this._dockSettings.get_enum('mode');
        // Update dock on settings changes
        this._bindSettingsChanges();

        // Used to ignore hover events while auto-hiding
        this._ignoreHover = false;
        this._saveIgnoreHover = null;

        // Initial dock state.
        this._dockState = State.HIDDEN;

        // Get the monitor object for this dock.
        this._monitor = Main.layoutManager.monitors[this._monitorIndex];

        // Pressure barrier state
        this._canUsePressure = false;
        this._pressureBarrier = null;
        this._barrier = null;

        this._removeBarrierTimeoutId = 0;

        // Fallback autohide detection
        this._dockEdge = null;

        // Get icon size settings.
        const fixedIconSize = this._dockSettings.get_boolean('fixed-icon-size');
        const iconSize = this._dockSettings.get_int('icon-size');
        const iconSizes = this._getIconSizes();

        // Create a new dash.
        this.dash = new Dash.Dash({
            monitorIndex,
            fixedIconSize,
            iconSizes,
            iconSize,
        });

        super._init({
            name: 'dock',
            reactive: false,
        });

        this._box = new St.BoxLayout({
            name: 'dockbox',
            reactive: true,
            track_hover: true,
        });

        this._box.connect('notify::hover', this._hoverChanged.bind(this));
        this.dash.connect('menu-closed', () => {
            this._box.sync_hover();
        });

        global.display.connect('workareas-changed', this._reallocate.bind(this));
        global.display.connect('in-fullscreen-changed', this._updateBarrier.bind(this));

        // Only initialize signals when the overview isn't a dummy.
        if (!Main.overview.isDummy) {
            Main.overview.connect('item-drag-begin', this._onDragStart.bind(this));
            Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));
            Main.overview.connect('item-drag-cancelled', this._onDragEnd.bind(this));
            Main.overview.connect('showing', this._onOverviewShowing.bind(this));
            Main.overview.connect('hiding', this._onOverviewHiding.bind(this));
        }

        let id = this.connect_after('paint', () => {
            this.disconnect(id);

            this._updateDashVisibility();

            // Setup barriers.
            this._updatePressureBarrier();
            this._updateBarrier();

            // Setup the fallback autohide detection as needed.
            this._setupFallbackEdgeIfNeeded();
        });

        this._box.add_actor(this.dash);
        this._box.x_expand = true;

        this._slider = new OverviewControls.DashSlider();
        this._slider.add_actor(this._box);

        this.dash.connect('icon-size-changed', this._animateIn.bind(this));

        this.set_child(this._slider);

        Main.uiGroup.add_child(this);

        if (Main.uiGroup.contains(global.top_window_group))
            Main.uiGroup.set_child_below_sibling(this, global.top_window_group);

        this._updateTracking();

        // Create and apply height constraint to the dash. It's controlled by this.height
        let constraint = new Clutter.BindConstraint({
            source: this,
            coordinate: Clutter.BindCoordinate.HEIGHT,
        });
        this.dash.add_constraint(constraint);

        // Set initial allocation based on work area.
        this._reallocate();
    }

    _bindSettingsChanges() {
        const settings = this._dockSettings;

        ['changed::icon-size', 'changed::fixed-icon-size'].forEach(signal =>
            settings.connect(signal,
                () => {
                    const fixed = settings.get_boolean('fixed-icon-size');
                    const size = settings.get_int('icon-size');
                    this.dash.setIconSize(size, fixed);
                },
            ));

        settings.connect('changed::icon-sizes',
            () => {
                const sizes = this._getIconSizes();
                this.dash.setIconSizes(sizes);
            });

        settings.connect('changed::mode',
            () => {
                this._dockMode = settings.get_enum('mode');

                this._updateTracking();
                this._updateDashVisibility();
                this._updateBarrier();
            });

        settings.connect('changed::extend', this._reallocate.bind(this));
        settings.connect('changed::height', this._reallocate.bind(this));
    }

    _getIconSizes() {
        const iconSizesVariant = this._dockSettings.get_value('icon-sizes');
        const n = iconSizesVariant.n_children();
        const iconSizes = [];

        for (let i = 0; i < n; i++) {
            const val = iconSizesVariant.get_child_value(i).get_int32();
            iconSizes.push(val);
        }

        return iconSizes;
    }

    _updateDashVisibility() {
        // Ignore if overview is visible.
        if (Main.overview.visibleTarget)
            return;

        // If auto-hiding check that the dash should still be visible
        if (this._dockMode === Mode.AUTOHIDE) {
            this._ignoreHover = false;

            global.sync_pointer();

            if (this._box.hover)
                this._animateIn();
            else
                this._animateOut();

        } else if (this._dockMode === Mode.FIXED) {
            this._animateIn();
        } else {
            this._animateOut();
        }
    }

    _onOverviewShowing() {
        this._ignoreHover = true;
        this._removeTransitions();
        this._animateIn();
    }

    _onOverviewHiding() {
        this._ignoreHover = false;
        this._updateDashVisibility();
    }

    _hoverChanged() {
        if (!this._ignoreHover && this._dockMode === Mode.AUTOHIDE) {
            if (this._box.hover)
                this.slideIn();
            else
                this.slideOut();

        }
    }

    slideIn() {
        if (this._dockState === State.HIDDEN || this._dockState === State.HIDING) {
            if (this._dockState === State.HIDING)
                this._removeTransitions();

            // Prevent 'double' animations which can cause visual quirks with autohide.
            if (this._dockState !== State.SHOWING) {
                this.emit('showing');
                this._animateIn();
            }
        }
    }

    slideOut() {
        if (this._dockState === State.SHOWN || this._dockState === State.SHOWING) {
            let delay = DOCK_HIDE_DELAY;

            // If the dock is already animating in, wait until it is finished.
            if (this._dockState === State.SHOWING)
                delay += DOCK_ANIMATION_DURATION;

            this.emit('hiding');
            this._animateOut(delay);
        }
    }

    _animateIn(delay = 0) {
        this._dockState = State.SHOWING;

        this._slider.slideIn(DOCK_ANIMATION_DURATION, delay, () => {
            this._dockState = State.SHOWN;

            if (this._removeBarrierTimeoutId > 0)
                GLib.source_remove(this._removeBarrierTimeoutId);

            // Only schedule a remove timeout if the barrier exists.
            if (this._barrier)
                this._removeBarrierTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 100, this._removeBarrier.bind(this));
        });
    }

    _animateOut(delay = 0) {
        this._dockState = State.HIDING;

        this._slider.slideOut(DOCK_ANIMATION_DURATION, delay, () => {
            this._dockState = State.HIDDEN;

            // Remove queued barrier removal if any
            if (this._removeBarrierTimeoutId > 0)
                GLib.source_remove(this._removeBarrierTimeoutId);

            this._updateBarrier();
        });
    }

    _setupFallbackEdgeIfNeeded() {
        this._canUsePressure = global.display.supports_extended_barriers();

        if (this._dockEdge) {
            this._dockEdge.destroy();
            this._dockEdge = null;
        }

        if (!this._canUsePressure) {
            log('Dock is using fallback edge detection.');

            const workArea = Main.layoutManager.getWorkAreaForMonitor(this._monitorIndex);
            const height = workArea.height - 3;
            this._dockEdge = new Clutter.Actor({
                name: 'dock-edge',
                width: 1,
                height,
                opacity: 0,
                reactive: true,
            });

            Main.layoutManager.addChrome(this._dockEdge);

            if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL) {
                this._dockEdge.set_position(workArea.width - this._dockEdge.width, 0);
                this.set_anchor_point_from_gravity(Clutter.Gravity.EAST);
            } else {
                this._dockEdge.set_position(0, 1);
            }

            this._dockEdge.connect('enter-event', () => {
                if (!this._dockEntered) {
                    this._dockEntered = true;
                    if (!this._monitor.inFullscreen)
                        this._onPressureSensed();

                }
                return Clutter.EVENT_PROPAGATE;
            });

            this._dockEdge.connect('leave-event', (_, event) => {
                if (event.get_related() !== this._dockEdge)
                    this._dockEntered = false;

                return Clutter.EVENT_STOP;
            });
        }
    }

    _updatePressureBarrier() {
        this._canUsePressure = global.display.supports_extended_barriers();

        // Remove existing barriers

        if (this._pressureBarrier) {
            this._pressureBarrier.destroy();
            this._pressureBarrier = null;
        }

        if (this._barrier) {
            this._barrier.destroy();
            this._barrier = null;
        }

        if (this._canUsePressure) {
            this._pressureBarrier = new Layout.PressureBarrier(DOCK_PRESSURE_THRESHOLD, DOCK_PRESSURE_TIMEOUT + DOCK_SHOW_DELAY * 1000, // 1 second plus the delay to show.
                Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW);
            this._pressureBarrier.connect('trigger', () => {
                if (this._monitor.inFullscreen)
                    return;
                this._onPressureSensed();
            });
        }
    }

    _onPressureSensed() {
        if (Main.overview.visibleTarget)
            return;

        // After any animations have completed, check that the mouse hasn't left the
        // dock area.
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, DOCK_ANIMATION_DURATION * 1000, () => {
            let [x, y] = global.get_pointer();
            const computedX = this.x + this._slider.x;
            const computedY = this._monitor.y + this._monitor.height;

            if (x > computedX ||
                x < this._monitor.x ||
                y < this._monitor.y ||
                y > computedY) {
                this._hoverChanged();
                return GLib.SOURCE_REMOVE;
            } else {
                return GLib.SOURCE_CONTINUE;
            }
        });

        this.slideIn();
    }

    _removeBarrier() {
        if (this._barrier) {
            if (this._pressureBarrier)
                this._pressureBarrier.removeBarrier(this._barrier);

            this._barrier.destroy();
            this._barrier = null;
        }
        this._removeBarrierTimeoutId = 0;
        return false;
    }

    _updateBarrier() {
        // Remove existing barrier
        this._removeBarrier();

        // The barrier should not be set in fullscreen.
        if (this._monitor.inFullscreen)
            return;

        // Reset the barrier to default variables.
        if (this._pressureBarrier) {
            this._pressureBarrier._reset();
            this._pressureBarrier._isTriggered = false;
        }

        if (this._canUsePressure && this._dockMode === Mode.AUTOHIDE) {
            let workArea = Main.layoutManager.getWorkAreaForMonitor(this._monitor.index);

            let x1 = this._monitor.x + 1;
            let x2 = x1;
            let y1 = workArea.y + 1;
            // Avoid conflicting with the hot corner by subtracting 1px;
            let y2 = workArea.y + workArea.height - 1;
            let direction = Meta.BarrierDirection.POSITIVE_X;

            if (this._pressureBarrier && this._dockState === State.HIDDEN) {
                this._barrier = new Meta.Barrier({
                    display: global.display,
                    x1,
                    x2,
                    y1,
                    y2,
                    directions: direction,
                });

                this._pressureBarrier.addBarrier(this._barrier);
            }
        }
    }

    _updateTracking() {
        Main.layoutManager._untrackActor(this._slider);
        Main.layoutManager._untrackActor(this);

        if (this._dockMode === Mode.FIXED) {
            // Setting trackFullscreen directly on the slider
            // causes issues when full-screening some windows.
            Main.layoutManager._trackActor(this, {
                affectsInputRegion: false,
                trackFullscreen: true,
            });
            Main.layoutManager._trackActor(this._slider, { affectsStruts: true });
        } else {
            Main.layoutManager._trackActor(this._slider);
        }
    }

    _reallocate() {
        // Ensure all height-related variables are updated.
        this._updateDashVisibility();

        let workArea = Main.layoutManager.getWorkAreaForMonitor(this._monitorIndex);

        let extendHeight = this._dockSettings.get_boolean('extend');
        let height = extendHeight ? 1 : this._dockSettings.get_double('height');

        if (height < 0 || height > 1)
            height = 0.95;

        this.height = Math.round(height * workArea.height);
        this.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_WEST);
        this.x = this._monitor.x;
        this.y = workArea.y + Math.round(((1 - height) / 2) * workArea.height);

        if (extendHeight) {
            this.dash._container.set_height(this.height);
            this.add_style_class_name('extended');
        } else {
            this.dash._container.set_height(-1);
            this.remove_style_class_name('extended');
        }
    }

    _removeTransitions() {
        this._slider.remove_all_transitions();
    }

    _onDragStart() {
        this._saveIgnoreHover = this._ignoreHover;
        this._ignoreHover = true;
        this._animateIn();
    }

    _onDragEnd() {
        if (this._saveIgnoreHover !== null)
            this._ignoreHover = this._saveIgnoreHover;

        this._saveIgnoreHover = null;
        this._box.sync_hover();

        if (Main.overview.slideInn)
            this._pageChanged();

    }

    _pageChanged() {
        const activePage = Main.overview.viewSelector.getActivePage();
        const showDash = activePage === ViewSelector.ViewPage.WINDOWS ||
            activePage === ViewSelector.ViewPage.APPS;

        if (showDash)
            this._animateIn();
        else
            this._animateOut();

    }

    _onAccessibilityFocus() {
        this._box.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        this._animateIn();
    }
});
