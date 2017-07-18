const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const CtrlAltTab = imports.ui.ctrlAltTab;
const Lang = imports.lang;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const OverviewControls = imports.ui.overviewControls;
const Tweener = imports.ui.tweener;

const STANDARD_TRAY_ICON_IMPLEMENTATIONS = {
    'bluetooth-applet': 'bluetooth',
    'gnome-volume-control-applet': 'volume', // renamed to gnome-sound-applet
                                             // when moved to control center
    'gnome-sound-applet': 'volume',
    'nm-applet': 'network',
    'gnome-power-manager': 'battery',
    'keyboard': 'keyboard',
    'a11y-keyboard': 'a11y',
    'kbd-scrolllock': 'keyboard',
    'kbd-numlock': 'keyboard',
    'kbd-capslock': 'keyboard',
    'ibus-ui-gtk': 'keyboard'
};

// Offset of the original position from the bottom-right corner
const CONCEALED_WIDTH = 3;
const REVEAL_ANIMATION_TIME = 0.2;
const TEMP_REVEAL_TIME = 2;

const BARRIER_THRESHOLD = 70;
const BARRIER_TIMEOUT = 1000;

var LegacyTray = new Lang.Class({
    Name: 'LegacyTray',

    _init: function() {
        this.actor = new St.Widget({ clip_to_allocation: true,
                                     layout_manager: new Clutter.BinLayout() });
        let constraint = new Layout.MonitorConstraint({ primary: true,
                                                        work_area: true });
        this.actor.add_constraint(constraint);

        this._slideLayout = new OverviewControls.SlideLayout();
        this._slideLayout.translationX = 0;
        this._slideLayout.slideDirection = OverviewControls.SlideDirection.LEFT;

        this._slider = new St.Widget({ x_expand: true, y_expand: true,
                                       x_align: Clutter.ActorAlign.START,
                                       y_align: Clutter.ActorAlign.END,
                                       layout_manager: this._slideLayout });
        this.actor.add_actor(this._slider);
        this._slider.connect('notify::allocation', Lang.bind(this, this._syncBarrier));

        this._box = new St.BoxLayout({ style_class: 'legacy-tray' });
        this._slider.add_actor(this._box);

        this._concealHandle = new St.Button({ style_class: 'legacy-tray-handle',
                                              /* translators: 'Hide' is a verb */
                                              accessible_name: _("Hide tray"),
                                              can_focus: true });
        this._concealHandle.child = new St.Icon({ icon_name: 'go-previous-symbolic' });
        this._box.add_child(this._concealHandle);

        this._iconBox = new St.BoxLayout({ style_class: 'legacy-tray-icon-box' });
        this._box.add_actor(this._iconBox);

        this._revealHandle = new St.Button({ style_class: 'legacy-tray-handle' });
        this._revealHandle.child = new St.Icon({ icon_name: 'go-next-symbolic' });
        this._box.add_child(this._revealHandle);

        this._revealHandle.bind_property('visible',
                                         this._concealHandle, 'visible',
                                         GObject.BindingFlags.BIDIRECTIONAL |
                                         GObject.BindingFlags.INVERT_BOOLEAN);
        this._revealHandle.connect('notify::visible',
                                   Lang.bind(this, this._sync));
        this._revealHandle.connect('notify::hover',
                                   Lang.bind(this ,this._sync));
        this._revealHandle.connect('clicked', Lang.bind(this,
            function() {
                this._concealHandle.show();
            }));
        this._concealHandle.connect('clicked', Lang.bind(this,
            function() {
                this._revealHandle.show();
            }));

        this._horizontalBarrier = null;
        this._pressureBarrier = new Layout.PressureBarrier(BARRIER_THRESHOLD,
                                                           BARRIER_TIMEOUT,
                                                           Shell.ActionMode.NORMAL);
        this._pressureBarrier.connect('trigger', Lang.bind(this, function() {
            this._concealHandle.show();
        }));

        Main.layoutManager.addChrome(this.actor, { affectsInputRegion: false });
        Main.layoutManager.trackChrome(this._slider, { affectsInputRegion: true });
        Main.uiGroup.set_child_below_sibling(this.actor, Main.layoutManager.modalDialogGroup);
        Main.ctrlAltTabManager.addGroup(this.actor,
                                        _("Status Icons"), 'focus-legacy-systray-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.BOTTOM });

        this._trayManager = new Shell.TrayManager();
        this._trayIconAddedId = this._trayManager.connect('tray-icon-added', Lang.bind(this, this._onTrayIconAdded));
        this._trayIconRemovedId = this._trayManager.connect('tray-icon-removed', Lang.bind(this, this._onTrayIconRemoved));
        this._trayManager.manage_screen(global.screen, this.actor);

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                Tweener.removeTweens(this._slider);
                Tweener.addTween(this._slider, { opacity: 0,
                                                 time: Overview.ANIMATION_TIME,
                                                 transition: 'easeOutQuad' });
            }));
        Main.overview.connect('shown', Lang.bind(this, this._sync));
        Main.overview.connect('hiding', Lang.bind(this,
            function() {
                this._sync();
                Tweener.removeTweens(this._slider);
                Tweener.addTween(this._slider, { opacity: 255,
                                                 time: Overview.ANIMATION_TIME,
                                                 transition: 'easeOutQuad' });
            }));

        Main.layoutManager.connect('monitors-changed',
                                   Lang.bind(this, this._sync));
        global.screen.connect('in-fullscreen-changed',
                              Lang.bind(this, this._sync));
        Main.sessionMode.connect('updated', Lang.bind(this, this._sync));

        this._sync();
    },

    _onTrayIconAdded: function(tm, icon) {
        let wmClass = icon.wm_class ? icon.wm_class.toLowerCase() : '';
        if (STANDARD_TRAY_ICON_IMPLEMENTATIONS[wmClass] !== undefined)
            return;

        let button = new St.Button({ child: icon,
                                     style_class: 'legacy-tray-icon',
                                     button_mask: St.ButtonMask.ONE |
                                                  St.ButtonMask.TWO |
                                                  St.ButtonMask.THREE,
                                     can_focus: true,
                                     x_fill: true, y_fill: true });

        let app = Shell.WindowTracker.get_default().get_app_from_pid(icon.pid);
        if (!app)
            app = Shell.AppSystem.get_default().lookup_startup_wmclass(wmClass);
        if (!app)
            app = Shell.AppSystem.get_default().lookup_desktop_wmclass(wmClass);
        if (app)
            button.accessible_name = app.get_name();
        else
            button.accessible_name = icon.title;

        button.connect('clicked',
            function() {
                icon.click(Clutter.get_current_event());
            });
        button.connect('key-press-event',
            function() {
                icon.click(Clutter.get_current_event());
                return Clutter.EVENT_PROPAGATE;
            });
        button.connect('key-focus-in', Lang.bind(this,
            function() {
                this._concealHandle.show();
            }));

        this._iconBox.add_actor(button);

        if (!this._concealHandle.visible) {
            this._concealHandle.show();
            GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, TEMP_REVEAL_TIME,
                Lang.bind(this, function() {
                    this._concealHandle.hide();
                    return GLib.SOURCE_REMOVE;
                }));
        }
    },

    _onTrayIconRemoved: function(tm, icon) {
        if (!this.actor.contains(icon))
            return;

        icon.get_parent().destroy();
        this._sync();
    },

    _syncBarrier: function() {
        let rtl = (this._slider.get_text_direction() == Clutter.TextDirection.RTL);
        let [x, y] = this._slider.get_transformed_position();
        let [w, h] = this._slider.get_transformed_size();

        let x1 = Math.round(x);
        if (rtl)
            x1 += Math.round(w);

        let x2 = x1;
        let y1 = Math.round(y);
        let y2 = y1 + Math.round(h);

        if (this._horizontalBarrier &&
            this._horizontalBarrier.x1 == x1 &&
            this._horizontalBarrier.y1 == y1 &&
            this._horizontalBarrier.x2 == x2 &&
            this._horizontalBarrier.y2 == y2)
            return;

        this._unsetBarrier();

        let directions = (rtl ? Meta.BarrierDirection.NEGATIVE_X : Meta.BarrierDirection.POSITIVE_X);
        this._horizontalBarrier = new Meta.Barrier({ display: global.display,
                                                     x1: x1, x2: x2,
                                                     y1: y1, y2: y2,
                                                     directions: directions });
        this._pressureBarrier.addBarrier(this._horizontalBarrier);
    },

    _unsetBarrier: function() {
        if (this._horizontalBarrier == null)
            return;

        this._pressureBarrier.removeBarrier(this._horizontalBarrier);
        this._horizontalBarrier.destroy();
        this._horizontalBarrier = null;
    },

    _sync: function() {
        // FIXME: we no longer treat tray icons as notifications
        let allowed = Main.sessionMode.hasNotifications;
        let hasIcons = this._iconBox.get_n_children() > 0;
        let inOverview = Main.overview.visible && !Main.overview.animationInProgress;
        let inFullscreen = Main.layoutManager.primaryMonitor.inFullscreen;
        this.actor.visible = allowed && hasIcons && !inOverview && !inFullscreen;

        if (!hasIcons)
            this._concealHandle.hide();

        let targetSlide;
        if (this._concealHandle.visible) {
            targetSlide = 1.0;
        } else if (!hasIcons) {
            targetSlide = 0.0;
        } else {
            let [, boxWidth] = this._box.get_preferred_width(-1);
            let [, handleWidth] = this._revealHandle.get_preferred_width(-1);

            if (this._revealHandle.hover)
                targetSlide = handleWidth / boxWidth;
            else
                targetSlide = CONCEALED_WIDTH / boxWidth;
        }

        if (this.actor.visible) {
            Tweener.addTween(this._slideLayout,
                             { slideX: targetSlide,
                               time: REVEAL_ANIMATION_TIME,
                               transition: 'easeOutQuad' });
        } else {
            this._slideLayout.slideX = targetSlide;
            this._unsetBarrier();
        }
    }
});
