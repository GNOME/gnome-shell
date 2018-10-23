// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Atk = imports.gi.Atk;

const Animation = imports.ui.animation;
const Config = imports.misc.config;
const CtrlAltTab = imports.ui.ctrlAltTab;
const DND = imports.ui.dnd;
const Overview = imports.ui.overview;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const RemoteMenu = imports.ui.remoteMenu;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

var PANEL_ICON_SIZE = 16;
var APP_MENU_ICON_MARGIN = 0;

var BUTTON_DND_ACTIVATION_TIMEOUT = 250;

var SPINNER_ANIMATION_TIME = 1.0;

// To make sure the panel corners blend nicely with the panel,
// we draw background and borders the same way, e.g. drawing
// them as filled shapes from the outside inwards instead of
// using cairo stroke(). So in order to give the border the
// appearance of being drawn on top of the background, we need
// to blend border and background color together.
// For that purpose we use the following helper methods, taken
// from st-theme-node-drawing.c
function _norm(x) {
    return Math.round(x / 255);
}

function _over(srcColor, dstColor) {
    let src = _premultiply(srcColor);
    let dst = _premultiply(dstColor);
    let result = new Clutter.Color();

    result.alpha = src.alpha + _norm((255 - src.alpha) * dst.alpha);
    result.red = src.red + _norm((255 - src.alpha) * dst.red);
    result.green = src.green + _norm((255 - src.alpha) * dst.green);
    result.blue = src.blue + _norm((255 - src.alpha) * dst.blue);

    return _unpremultiply(result);
}

function _premultiply(color) {
    return new Clutter.Color({ red: _norm(color.red * color.alpha),
                               green: _norm(color.green * color.alpha),
                               blue: _norm(color.blue * color.alpha),
                               alpha: color.alpha });
};

function _unpremultiply(color) {
    if (color.alpha == 0)
        return new Clutter.Color();

    let red = Math.min((color.red * 255 + 127) / color.alpha, 255);
    let green = Math.min((color.green * 255 + 127) / color.alpha, 255);
    let blue = Math.min((color.blue * 255 + 127) / color.alpha, 255);
    return new Clutter.Color({ red: red, green: green,
                               blue: blue, alpha: color.alpha });
};

/**
 * AppMenuButton:
 *
 * This class manages the "application menu" component.  It tracks the
 * currently focused application.  However, when an app is launched,
 * this menu also handles startup notification for it.  So when we
 * have an active startup notification, we switch modes to display that.
 */
var AppMenuButton = new Lang.Class({
    Name: 'AppMenuButton',
    Extends: PanelMenu.Button,

    _init(panel) {
        this.parent(0.0, null, true);

        this.actor.accessible_role = Atk.Role.MENU;

        this._startingApps = [];

        this._menuManager = panel.menuManager;
        this._gtkSettings = Gtk.Settings.get_default();
        this._targetApp = null;
        this._appMenuNotifyId = 0;
        this._actionGroupNotifyId = 0;
        this._busyNotifyId = 0;

        let bin = new St.Bin({ name: 'appMenu' });
        bin.connect('style-changed', this._onStyleChanged.bind(this));
        this.actor.add_actor(bin);

        this.actor.bind_property("reactive", this.actor, "can-focus", 0);
        this.actor.reactive = false;

        this._container = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        bin.set_child(this._container);

        let textureCache = St.TextureCache.get_default();
        textureCache.connect('icon-theme-changed',
                             this._onIconThemeChanged.bind(this));

        this._iconBox = new St.Bin({ style_class: 'app-menu-icon' });
        this._container.add_actor(this._iconBox);

        this._label = new St.Label({ y_expand: true,
                                     y_align: Clutter.ActorAlign.CENTER });
        this._container.add_actor(this._label);
        this._arrow = PopupMenu.arrowIcon(St.Side.BOTTOM);
        this._container.add_actor(this._arrow);

        this._visible = this._gtkSettings.gtk_shell_shows_app_menu &&
                        !Main.overview.visible;
        if (!this._visible)
            this.actor.hide();
        this._overviewHidingId = Main.overview.connect('hiding', this._sync.bind(this));
        this._overviewShowingId = Main.overview.connect('showing', this._sync.bind(this));
        this._showsAppMenuId = this._gtkSettings.connect('notify::gtk-shell-shows-app-menu',
                                                         this._sync.bind(this));

        this._stop = true;

        this._spinner = null;

        let tracker = Shell.WindowTracker.get_default();
        let appSys = Shell.AppSystem.get_default();
        this._focusAppNotifyId =
            tracker.connect('notify::focus-app', this._focusAppChanged.bind(this));
        this._appStateChangedSignalId =
            appSys.connect('app-state-changed', this._onAppStateChanged.bind(this));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace', this._sync.bind(this));

        this._sync();
    },

    show() {
        if (this._visible)
            return;

        this._visible = true;
        this.actor.reactive = true;
        this.actor.show();
        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad' });
    },

    hide() {
        if (!this._visible)
            return;

        this._visible = false;
        this.actor.reactive = false;
        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete() {
                               this.actor.hide();
                           },
                           onCompleteScope: this });
    },

    _onStyleChanged(actor) {
        let node = actor.get_theme_node();
        let [success, icon] = node.lookup_url('spinner-image', false);
        if (!success || (this._spinnerIcon && this._spinnerIcon.equal(icon)))
            return;
        this._spinnerIcon = icon;
        this._spinner = new Animation.AnimatedIcon(this._spinnerIcon, PANEL_ICON_SIZE);
        this._container.add_actor(this._spinner.actor);
        this._spinner.actor.hide();
    },

    _syncIcon() {
        if (!this._targetApp)
            return;

        let icon = this._targetApp.create_icon_texture(PANEL_ICON_SIZE - APP_MENU_ICON_MARGIN);
        this._iconBox.set_child(icon);
    },

    _onIconThemeChanged() {
        if (this._iconBox.child == null)
            return;

        this._syncIcon();
    },

    stopAnimation() {
        if (this._stop)
            return;

        this._stop = true;

        if (this._spinner == null)
            return;

        Tweener.addTween(this._spinner.actor,
                         { opacity: 0,
                           time: SPINNER_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onCompleteScope: this,
                           onComplete() {
                               this._spinner.stop();
                               this._spinner.actor.opacity = 255;
                               this._spinner.actor.hide();
                           }
                         });
    },

    startAnimation() {
        this._stop = false;

        if (this._spinner == null)
            return;

        this._spinner.play();
        this._spinner.actor.show();
    },

    _onAppStateChanged(appSys, app) {
        let state = app.state;
        if (state != Shell.AppState.STARTING)
            this._startingApps = this._startingApps.filter(a => a != app);
        else if (state == Shell.AppState.STARTING)
            this._startingApps.push(app);
        // For now just resync on all running state changes; this is mainly to handle
        // cases where the focused window's application changes without the focus
        // changing.  An example case is how we map OpenOffice.org based on the window
        // title which is a dynamic property.
        this._sync();
    },

    _focusAppChanged() {
        let tracker = Shell.WindowTracker.get_default();
        let focusedApp = tracker.focus_app;
        if (!focusedApp) {
            // If the app has just lost focus to the panel, pretend
            // nothing happened; otherwise you can't keynav to the
            // app menu.
            if (global.stage.key_focus != null)
                return;
        }
        this._sync();
    },

    _findTargetApp() {
        let workspaceManager = global.workspace_manager;
        let workspace = workspaceManager.get_active_workspace();
        let tracker = Shell.WindowTracker.get_default();
        let focusedApp = tracker.focus_app;
        if (focusedApp && focusedApp.is_on_workspace(workspace))
            return focusedApp;

        for (let i = 0; i < this._startingApps.length; i++)
            if (this._startingApps[i].is_on_workspace(workspace))
                return this._startingApps[i];

        return null;
    },

    _sync() {
        let targetApp = this._findTargetApp();

        if (this._targetApp != targetApp) {
            if (this._appMenuNotifyId) {
                this._targetApp.disconnect(this._appMenuNotifyId);
                this._appMenuNotifyId = 0;
            }
            if (this._actionGroupNotifyId) {
                this._targetApp.disconnect(this._actionGroupNotifyId);
                this._actionGroupNotifyId = 0;
            }
            if (this._busyNotifyId) {
                this._targetApp.disconnect(this._busyNotifyId);
                this._busyNotifyId = 0;
            }

            this._targetApp = targetApp;

            if (this._targetApp) {
                this._appMenuNotifyId = this._targetApp.connect('notify::menu', this._sync.bind(this));
                this._actionGroupNotifyId = this._targetApp.connect('notify::action-group', this._sync.bind(this));
                this._busyNotifyId = this._targetApp.connect('notify::busy', this._sync.bind(this));
                this._label.set_text(this._targetApp.get_name());
                this.actor.set_accessible_name(this._targetApp.get_name());
            }
        }

        let shellShowsAppMenu = this._gtkSettings.gtk_shell_shows_app_menu;
        Meta.prefs_set_show_fallback_app_menu(!shellShowsAppMenu);

        let visible = (this._targetApp != null &&
                       shellShowsAppMenu &&
                       !Main.overview.visibleTarget);
        if (visible)
            this.show();
        else
            this.hide();

        let isBusy = (this._targetApp != null &&
                      (this._targetApp.get_state() == Shell.AppState.STARTING ||
                       this._targetApp.get_busy()));
        if (isBusy)
            this.startAnimation();
        else
            this.stopAnimation();

        this.actor.reactive = (visible && !isBusy);

        this._syncIcon();
        this._maybeSetMenu();
        this.emit('changed');
    },

    _maybeSetMenu() {
        let menu;

        if (this._targetApp == null) {
            menu = null;
        } else if (this._targetApp.action_group && this._targetApp.menu) {
            if (this.menu instanceof RemoteMenu.RemoteMenu &&
                this.menu.actionGroup == this._targetApp.action_group)
                return;

            menu = new RemoteMenu.RemoteMenu(this.actor, this._targetApp.menu, this._targetApp.action_group);
            menu.connect('activate', () => {
                let win = this._targetApp.get_windows()[0];
                win.check_alive(global.get_current_time());
            });

        } else {
            if (this.menu && this.menu.isDummyQuitMenu)
                return;

            // fallback to older menu
            menu = new PopupMenu.PopupMenu(this.actor, 0.0, St.Side.TOP, 0);
            menu.isDummyQuitMenu = true;
            menu.addAction(_("Quit"), () => {
                this._targetApp.request_quit();
            });
        }

        this.setMenu(menu);
        if (menu)
            this._menuManager.addMenu(menu);
    },

    destroy() {
        if (this._appStateChangedSignalId > 0) {
            let appSys = Shell.AppSystem.get_default();
            appSys.disconnect(this._appStateChangedSignalId);
            this._appStateChangedSignalId = 0;
        }
        if (this._focusAppNotifyId > 0) {
            let tracker = Shell.WindowTracker.get_default();
            tracker.disconnect(this._focusAppNotifyId);
            this._focusAppNotifyId = 0;
        }
        if (this._overviewHidingId > 0) {
            Main.overview.disconnect(this._overviewHidingId);
            this._overviewHidingId = 0;
        }
        if (this._overviewShowingId > 0) {
            Main.overview.disconnect(this._overviewShowingId);
            this._overviewShowingId = 0;
        }
        if (this._showsAppMenuId > 0) {
            this._gtkSettings.disconnect(this._showsAppMenuId);
            this._showsAppMenuId = 0;
        }
        if (this._switchWorkspaceNotifyId > 0) {
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
            this._switchWorkspaceNotifyId = 0;
        }

        this.parent();
    }
});

Signals.addSignalMethods(AppMenuButton.prototype);

var ActivitiesButton = new Lang.Class({
    Name: 'ActivitiesButton',
    Extends: PanelMenu.Button,

    _init() {
        this.parent(0.0, null, true);
        this.actor.accessible_role = Atk.Role.TOGGLE_BUTTON;

        this.actor.name = 'panelActivities';

        /* Translators: If there is no suitable word for "Activities"
           in your language, you can use the word for "Overview". */
        this._label = new St.Label({ text: _("Activities"),
                                     y_align: Clutter.ActorAlign.CENTER });
        this.actor.add_actor(this._label);

        this.actor.label_actor = this._label;

        this.actor.connect('captured-event', this._onCapturedEvent.bind(this));
        this.actor.connect_after('key-release-event', this._onKeyRelease.bind(this));

        Main.overview.connect('showing', () => {
            this.actor.add_style_pseudo_class('overview');
            this.actor.add_accessible_state (Atk.StateType.CHECKED);
        });
        Main.overview.connect('hiding', () => {
            this.actor.remove_style_pseudo_class('overview');
            this.actor.remove_accessible_state (Atk.StateType.CHECKED);
        });

        this._xdndTimeOut = 0;
    },

    handleDragOver(source, actor, x, y, time) {
        if (source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        if (this._xdndTimeOut != 0)
            Mainloop.source_remove(this._xdndTimeOut);
        this._xdndTimeOut = Mainloop.timeout_add(BUTTON_DND_ACTIVATION_TIMEOUT, () => {
            this._xdndToggleOverview(actor);
        });
        GLib.Source.set_name_by_id(this._xdndTimeOut, '[gnome-shell] this._xdndToggleOverview');

        return DND.DragMotionResult.CONTINUE;
    },

    _onCapturedEvent(actor, event) {
        if (event.type() == Clutter.EventType.BUTTON_PRESS ||
            event.type() == Clutter.EventType.TOUCH_BEGIN) {
            if (!Main.overview.shouldToggleByCornerOrButton())
                return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onEvent(actor, event) {
        this.parent(actor, event);

        if (event.type() == Clutter.EventType.TOUCH_END ||
            event.type() == Clutter.EventType.BUTTON_RELEASE)
            if (Main.overview.shouldToggleByCornerOrButton())
                Main.overview.toggle();

        return Clutter.EVENT_PROPAGATE;
    },

    _onKeyRelease(actor, event) {
        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_Return || symbol == Clutter.KEY_space) {
            if (Main.overview.shouldToggleByCornerOrButton())
                Main.overview.toggle();
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _xdndToggleOverview(actor) {
        let [x, y, mask] = global.get_pointer();
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);

        if (pickedActor == this.actor && Main.overview.shouldToggleByCornerOrButton())
            Main.overview.toggle();

        Mainloop.source_remove(this._xdndTimeOut);
        this._xdndTimeOut = 0;
        return GLib.SOURCE_REMOVE;
    }
});

var PanelCorner = new Lang.Class({
    Name: 'PanelCorner',

    _init(side) {
        this._side = side;

        this.actor = new St.DrawingArea({ style_class: 'panel-corner' });
        this.actor.connect('style-changed', this._styleChanged.bind(this));
        this.actor.connect('repaint', this._repaint.bind(this));
    },

    _findRightmostButton(container) {
        if (!container.get_children)
            return null;

        let children = container.get_children();

        if (!children || children.length == 0)
            return null;

        // Start at the back and work backward
        let index;
        for (index = children.length - 1; index >= 0; index--) {
            if (children[index].visible)
                break;
        }
        if (index < 0)
            return null;

        if (!(children[index].has_style_class_name('panel-menu')) &&
            !(children[index].has_style_class_name('panel-button')))
            return this._findRightmostButton(children[index]);

        return children[index];
    },

    _findLeftmostButton(container) {
        if (!container.get_children)
            return null;

        let children = container.get_children();

        if (!children || children.length == 0)
            return null;

        // Start at the front and work forward
        let index;
        for (index = 0; index < children.length; index++) {
            if (children[index].visible)
                break;
        }
        if (index == children.length)
            return null;

        if (!(children[index].has_style_class_name('panel-menu')) &&
            !(children[index].has_style_class_name('panel-button')))
            return this._findLeftmostButton(children[index]);

        return children[index];
    },

    setStyleParent(box) {
        let side = this._side;

        let rtlAwareContainer = box instanceof St.BoxLayout;
        if (rtlAwareContainer &&
            box.get_text_direction() == Clutter.TextDirection.RTL) {
            if (this._side == St.Side.LEFT)
                side = St.Side.RIGHT;
            else if (this._side == St.Side.RIGHT)
                side = St.Side.LEFT;
        }

        let button;
        if (side == St.Side.LEFT)
            button = this._findLeftmostButton(box);
        else if (side == St.Side.RIGHT)
            button = this._findRightmostButton(box);

        if (button) {
            if (this._button && this._buttonStyleChangedSignalId) {
                this._button.disconnect(this._buttonStyleChangedSignalId);
                this._button.style = null;
            }

            this._button = button;

            button.connect('destroy', () => {
                if (this._button == button) {
                    this._button = null;
                    this._buttonStyleChangedSignalId = 0;
                }
            });

            // Synchronize the locate button's pseudo classes with this corner
            this._buttonStyleChangedSignalId = button.connect('style-changed',
                actor => {
                    let pseudoClass = button.get_style_pseudo_class();
                    this.actor.set_style_pseudo_class(pseudoClass);
                });

            // The corner doesn't support theme transitions, so override
            // the .panel-button default
            button.style = 'transition-duration: 0ms';
        }
    },

    _repaint() {
        let node = this.actor.get_theme_node();

        let cornerRadius = node.get_length("-panel-corner-radius");
        let borderWidth = node.get_length('-panel-corner-border-width');

        let backgroundColor = node.get_color('-panel-corner-background-color');
        let borderColor = node.get_color('-panel-corner-border-color');

        let overlap = borderColor.alpha != 0;
        let offsetY = overlap ? 0 : borderWidth;

        let cr = this.actor.get_context();
        cr.setOperator(Cairo.Operator.SOURCE);

        cr.moveTo(0, offsetY);
        if (this._side == St.Side.LEFT)
            cr.arc(cornerRadius,
                   borderWidth + cornerRadius,
                   cornerRadius, Math.PI, 3 * Math.PI / 2);
        else
            cr.arc(0,
                   borderWidth + cornerRadius,
                   cornerRadius, 3 * Math.PI / 2, 2 * Math.PI);
        cr.lineTo(cornerRadius, offsetY);
        cr.closePath();

        let savedPath = cr.copyPath();

        let xOffsetDirection = this._side == St.Side.LEFT ? -1 : 1;
        let over = _over(borderColor, backgroundColor);
        Clutter.cairo_set_source_color(cr, over);
        cr.fill();

        if (overlap) {
            let offset = borderWidth;
            Clutter.cairo_set_source_color(cr, backgroundColor);

            cr.save();
            cr.translate(xOffsetDirection * offset, - offset);
            cr.appendPath(savedPath);
            cr.fill();
            cr.restore();
        }

        cr.$dispose();
    },

    _styleChanged() {
        let node = this.actor.get_theme_node();

        let cornerRadius = node.get_length("-panel-corner-radius");
        let borderWidth = node.get_length('-panel-corner-border-width');

        this.actor.set_size(cornerRadius, borderWidth + cornerRadius);
        this.actor.set_anchor_point(0, borderWidth);
    }
});

var AggregateLayout = new Lang.Class({
    Name: 'AggregateLayout',
    Extends: Clutter.BoxLayout,

    _init(params) {
        if (!params)
            params = {};
        params['orientation'] = Clutter.Orientation.VERTICAL;
        this.parent(params);

        this._sizeChildren = [];
    },

    addSizeChild(actor) {
        this._sizeChildren.push(actor);
        this.layout_changed();
    },

    vfunc_get_preferred_width(container, forHeight) {
        let themeNode = container.get_theme_node();
        let minWidth = themeNode.get_min_width();
        let natWidth = minWidth;

        for (let i = 0; i < this._sizeChildren.length; i++) {
            let child = this._sizeChildren[i];
            let [childMin, childNat] = child.get_preferred_width(forHeight);
            minWidth = Math.max(minWidth, childMin);
            natWidth = Math.max(minWidth, childNat);
        }
        return [minWidth, natWidth];
    }
});

var AggregateMenu = new Lang.Class({
    Name: 'AggregateMenu',
    Extends: PanelMenu.Button,

    _init() {
        this.parent(0.0, C_("System menu in the top bar", "System"), false);
        this.menu.actor.add_style_class_name('aggregate-menu');

        let menuLayout = new AggregateLayout();
        this.menu.box.set_layout_manager(menuLayout);

        this._indicators = new St.BoxLayout({ style_class: 'panel-status-indicators-box' });
        this.actor.add_child(this._indicators);

        if (Config.HAVE_NETWORKMANAGER) {
            this._network = new imports.ui.status.network.NMApplet();
        } else {
            this._network = null;
        }
        if (Config.HAVE_BLUETOOTH) {
            this._bluetooth = new imports.ui.status.bluetooth.Indicator();
        } else {
            this._bluetooth = null;
        }

        this._remoteAccess = new imports.ui.status.remoteAccess.RemoteAccessApplet();
        this._power = new imports.ui.status.power.Indicator();
        this._rfkill = new imports.ui.status.rfkill.Indicator();
        this._volume = new imports.ui.status.volume.Indicator();
        this._brightness = new imports.ui.status.brightness.Indicator();
        this._system = new imports.ui.status.system.Indicator();
        this._screencast = new imports.ui.status.screencast.Indicator();
        this._location = new imports.ui.status.location.Indicator();
        this._nightLight = new imports.ui.status.nightLight.Indicator();
        this._thunderbolt = new imports.ui.status.thunderbolt.Indicator();

        this._indicators.add_child(this._thunderbolt.indicators);
        this._indicators.add_child(this._screencast.indicators);
        this._indicators.add_child(this._location.indicators);
        this._indicators.add_child(this._nightLight.indicators);
        if (this._network) {
            this._indicators.add_child(this._network.indicators);
        }
        if (this._bluetooth) {
            this._indicators.add_child(this._bluetooth.indicators);
        }
        this._indicators.add_child(this._remoteAccess.indicators);
        this._indicators.add_child(this._rfkill.indicators);
        this._indicators.add_child(this._volume.indicators);
        this._indicators.add_child(this._power.indicators);
        this._indicators.add_child(PopupMenu.arrowIcon(St.Side.BOTTOM));

        this.menu.addMenuItem(this._volume.menu);
        this.menu.addMenuItem(this._brightness.menu);
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        if (this._network) {
            this.menu.addMenuItem(this._network.menu);
        }
        if (this._bluetooth) {
            this.menu.addMenuItem(this._bluetooth.menu);
        }
        this.menu.addMenuItem(this._remoteAccess.menu);
        this.menu.addMenuItem(this._location.menu);
        this.menu.addMenuItem(this._rfkill.menu);
        this.menu.addMenuItem(this._power.menu);
        this.menu.addMenuItem(this._nightLight.menu);
        this.menu.addMenuItem(this._system.menu);

        menuLayout.addSizeChild(this._location.menu.actor);
        menuLayout.addSizeChild(this._rfkill.menu.actor);
        menuLayout.addSizeChild(this._power.menu.actor);
        menuLayout.addSizeChild(this._system.menu.actor);
    },
});

const PANEL_ITEM_IMPLEMENTATIONS = {
    'activities': ActivitiesButton,
    'aggregateMenu': AggregateMenu,
    'appMenu': AppMenuButton,
    'dateMenu': imports.ui.dateMenu.DateMenuButton,
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'keyboard': imports.ui.status.keyboard.InputSourceIndicator,
};

var Panel = new Lang.Class({
    Name: 'Panel',

    _init() {
        this.actor = new Shell.GenericContainer({ name: 'panel',
                                                  reactive: true });
        this.actor._delegate = this;
        this.actor.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

        this._sessionStyle = null;

        this.statusArea = {};

        this.menuManager = new PopupMenu.PopupMenuManager(this);

        this._leftBox = new St.BoxLayout({ name: 'panelLeft' });
        this.actor.add_actor(this._leftBox);
        this._centerBox = new St.BoxLayout({ name: 'panelCenter' });
        this.actor.add_actor(this._centerBox);
        this._rightBox = new St.BoxLayout({ name: 'panelRight' });
        this.actor.add_actor(this._rightBox);

        this._leftCorner = new PanelCorner(St.Side.LEFT);
        this.actor.add_actor(this._leftCorner.actor);

        this._rightCorner = new PanelCorner(St.Side.RIGHT);
        this.actor.add_actor(this._rightCorner.actor);

        this.actor.connect('get-preferred-width', this._getPreferredWidth.bind(this));
        this.actor.connect('get-preferred-height', this._getPreferredHeight.bind(this));
        this.actor.connect('allocate', this._allocate.bind(this));
        this.actor.connect('button-press-event', this._onButtonPress.bind(this));
        this.actor.connect('touch-event', this._onButtonPress.bind(this));
        this.actor.connect('key-press-event', this._onKeyPress.bind(this));

        Main.overview.connect('showing', () => {
            this.actor.add_style_pseudo_class('overview');
            this._updateSolidStyle();
        });
        Main.overview.connect('hiding', () => {
            this.actor.remove_style_pseudo_class('overview');
            this._updateSolidStyle();
        });

        Main.layoutManager.panelBox.add(this.actor);
        Main.ctrlAltTabManager.addGroup(this.actor, _("Top Bar"), 'focus-top-bar-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.TOP });

        Main.sessionMode.connect('updated', this._updatePanel.bind(this));

        this._trackedWindows = new Map();
        global.window_group.connect('actor-added', this._onWindowActorAdded.bind(this));
        global.window_group.connect('actor-removed', this._onWindowActorRemoved.bind(this));
        global.window_manager.connect('switch-workspace', this._updateSolidStyle.bind(this));

        global.display.connect('workareas-changed', () => { this.actor.queue_relayout(); });
        this._updatePanel();
    },

    _onWindowActorAdded(container, metaWindowActor) {
        let signalIds = [];
        ['allocation-changed', 'notify::visible'].forEach(s => {
            signalIds.push(metaWindowActor.connect(s, this._updateSolidStyle.bind(this)));
        });
        this._trackedWindows.set(metaWindowActor, signalIds);
    },

    _onWindowActorRemoved(container, metaWindowActor) {
        this._trackedWindows.get(metaWindowActor).forEach(id => {
            metaWindowActor.disconnect(id);
        });
        this._trackedWindows.delete(metaWindowActor);
        this._updateSolidStyle();
    },

    _getPreferredWidth(actor, forHeight, alloc) {
        let primaryMonitor = Main.layoutManager.primaryMonitor;

        alloc.min_size = -1;

        if (primaryMonitor)
            alloc.natural_size = primaryMonitor.width;
        else
            alloc.natural_size = -1;
    },

    _getPreferredHeight(actor, forWidth, alloc) {
        // We don't need to implement this; it's forced by the CSS
        alloc.min_size = -1;
        alloc.natural_size = -1;
    },

    _allocate(actor, box, flags) {
        let allocWidth = box.x2 - box.x1;
        let allocHeight = box.y2 - box.y1;

        let [leftMinWidth, leftNaturalWidth] = this._leftBox.get_preferred_width(-1);
        let [centerMinWidth, centerNaturalWidth] = this._centerBox.get_preferred_width(-1);
        let [rightMinWidth, rightNaturalWidth] = this._rightBox.get_preferred_width(-1);

        let sideWidth, centerWidth;
        centerWidth = centerNaturalWidth;

        // get workspace area and center date entry relative to it
        let monitor = Main.layoutManager.findMonitorForActor(actor);
        let centerOffset = 0;
        if (monitor) {
            let workArea = Main.layoutManager.getWorkAreaForMonitor(monitor.index);
            centerOffset = 2 * (workArea.x - monitor.x) + workArea.width - monitor.width;
        }

        sideWidth = Math.max(0, (allocWidth - centerWidth + centerOffset) / 2);

        let childBox = new Clutter.ActorBox();

        childBox.y1 = 0;
        childBox.y2 = allocHeight;
        if (this.actor.get_text_direction() == Clutter.TextDirection.RTL) {
            childBox.x1 = Math.max(allocWidth - Math.min(Math.floor(sideWidth),
                                                         leftNaturalWidth),
                                   0);
            childBox.x2 = allocWidth;
        } else {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth),
                                   leftNaturalWidth);
        }
        this._leftBox.allocate(childBox, flags);

        childBox.x1 = Math.ceil(sideWidth);
        childBox.y1 = 0;
        childBox.x2 = childBox.x1 + centerWidth;
        childBox.y2 = allocHeight;
        this._centerBox.allocate(childBox, flags);

        childBox.y1 = 0;
        childBox.y2 = allocHeight;
        if (this.actor.get_text_direction() == Clutter.TextDirection.RTL) {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth),
                                   rightNaturalWidth);
        } else {
            childBox.x1 = Math.max(allocWidth - Math.min(Math.floor(sideWidth),
                                                         rightNaturalWidth),
                                   0);
            childBox.x2 = allocWidth;
        }
        this._rightBox.allocate(childBox, flags);

        let cornerMinWidth, cornerMinHeight;
        let cornerWidth, cornerHeight;

        [cornerMinWidth, cornerWidth] = this._leftCorner.actor.get_preferred_width(-1);
        [cornerMinHeight, cornerHeight] = this._leftCorner.actor.get_preferred_height(-1);
        childBox.x1 = 0;
        childBox.x2 = cornerWidth;
        childBox.y1 = allocHeight;
        childBox.y2 = allocHeight + cornerHeight;
        this._leftCorner.actor.allocate(childBox, flags);

        [cornerMinWidth, cornerWidth] = this._rightCorner.actor.get_preferred_width(-1);
        [cornerMinHeight, cornerHeight] = this._rightCorner.actor.get_preferred_height(-1);
        childBox.x1 = allocWidth - cornerWidth;
        childBox.x2 = allocWidth;
        childBox.y1 = allocHeight;
        childBox.y2 = allocHeight + cornerHeight;
        this._rightCorner.actor.allocate(childBox, flags);
    },

    _onButtonPress(actor, event) {
        if (Main.modalCount > 0)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_source() != actor)
            return Clutter.EVENT_PROPAGATE;

        let type = event.type();
        let isPress = type == Clutter.EventType.BUTTON_PRESS;
        if (!isPress && type != Clutter.EventType.TOUCH_BEGIN)
            return Clutter.EVENT_PROPAGATE;

        let button = isPress ? event.get_button() : -1;
        if (isPress && button != 1)
            return Clutter.EVENT_PROPAGATE;

        let focusWindow = global.display.focus_window;
        if (!focusWindow)
            return Clutter.EVENT_PROPAGATE;

        let dragWindow = focusWindow.is_attached_dialog() ? focusWindow.get_transient_for()
                                                          : focusWindow;
        if (!dragWindow)
            return Clutter.EVENT_PROPAGATE;

        let rect = dragWindow.get_frame_rect();
        let [stageX, stageY] = event.get_coords();

        let allowDrag = dragWindow.maximized_vertically &&
                        stageX > rect.x && stageX < rect.x + rect.width;

        if (!allowDrag)
            return Clutter.EVENT_PROPAGATE;

        global.display.begin_grab_op(dragWindow,
                                     Meta.GrabOp.MOVING,
                                     false, /* pointer grab */
                                     true, /* frame action */
                                     button,
                                     event.get_state(),
                                     event.get_time(),
                                     stageX, stageY);

        return Clutter.EVENT_STOP;
    },

    _onKeyPress(actor, event) {
        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_Escape) {
            global.display.focus_default_window(event.get_time());
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    _toggleMenu(indicator) {
        if (!indicator) // menu not supported by current session mode
            return;

        let menu = indicator.menu;
        if (!indicator.actor.reactive)
            return;

        menu.toggle();
        if (menu.isOpen)
            menu.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
    },

    toggleAppMenu() {
        this._toggleMenu(this.statusArea.appMenu);
    },

    toggleCalendar() {
        this._toggleMenu(this.statusArea.dateMenu);
    },

    closeCalendar() {
        let indicator = this.statusArea.dateMenu;
        if (!indicator) // calendar not supported by current session mode
            return;

        let menu = indicator.menu;
        if (!indicator.actor.reactive)
            return;

        menu.close();
    },

    set boxOpacity(value) {
        let isReactive = value > 0;

        this._leftBox.opacity = value;
        this._leftBox.reactive = isReactive;
        this._centerBox.opacity = value;
        this._centerBox.reactive = isReactive;
        this._rightBox.opacity = value;
        this._rightBox.reactive = isReactive;
    },

    get boxOpacity() {
        return this._leftBox.opacity;
    },

    _updatePanel() {
        let panel = Main.sessionMode.panel;
        this._hideIndicators();
        this._updateBox(panel.left, this._leftBox);
        this._updateBox(panel.center, this._centerBox);
        this._updateBox(panel.right, this._rightBox);

        if (panel.left.indexOf('dateMenu') != -1)
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.START;
        else if (panel.right.indexOf('dateMenu') != -1)
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.END;
        // Default to center if there is no dateMenu
        else
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.CENTER;

        this._updateSolidStyle();

        if (this._sessionStyle)
            this._removeStyleClassName(this._sessionStyle);

        this._sessionStyle = Main.sessionMode.panelStyle;
        if (this._sessionStyle)
            this._addStyleClassName(this._sessionStyle);

        if (this.actor.get_text_direction() == Clutter.TextDirection.RTL) {
            this._leftCorner.setStyleParent(this._rightBox);
            this._rightCorner.setStyleParent(this._leftBox);
        } else {
            this._leftCorner.setStyleParent(this._leftBox);
            this._rightCorner.setStyleParent(this._rightBox);
        }
    },

    _updateSolidStyle() {
        if (this.actor.has_style_pseudo_class('overview') || !Main.sessionMode.hasWindows) {
            this._removeStyleClassName('solid');
            return;
        }

        if (!Main.layoutManager.primaryMonitor)
            return;

        /* Get all the windows in the active workspace that are in the primary monitor and visible */
        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let windows = activeWorkspace.list_windows().filter(metaWindow => {
            return metaWindow.is_on_primary_monitor() &&
                   metaWindow.showing_on_its_workspace() &&
                   !metaWindow.is_hidden() &&
                   metaWindow.get_window_type() != Meta.WindowType.DESKTOP;
        });

        /* Check if at least one window is near enough to the panel */
        let [, panelTop] = this.actor.get_transformed_position();
        let panelBottom = panelTop + this.actor.get_height();
        let scale = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let isNearEnough = windows.some(metaWindow => {
            let verticalPosition = metaWindow.get_frame_rect().y;
            return verticalPosition < panelBottom + 5 * scale;
        });

        if (isNearEnough)
            this._addStyleClassName('solid');
        else
            this._removeStyleClassName('solid');

    },

    _hideIndicators() {
        for (let role in PANEL_ITEM_IMPLEMENTATIONS) {
            let indicator = this.statusArea[role];
            if (!indicator)
                continue;
            indicator.container.hide();
        }
    },

    _ensureIndicator(role) {
        let indicator = this.statusArea[role];
        if (!indicator) {
            let constructor = PANEL_ITEM_IMPLEMENTATIONS[role];
            if (!constructor) {
                // This icon is not implemented (this is a bug)
                return null;
            }
            indicator = new constructor(this);
            this.statusArea[role] = indicator;
        }
        return indicator;
    },

    _updateBox(elements, box) {
        let nChildren = box.get_n_children();

        for (let i = 0; i < elements.length; i++) {
            let role = elements[i];
            let indicator = this._ensureIndicator(role);
            if (indicator == null)
                continue;

            this._addToPanelBox(role, indicator, i + nChildren, box);
        }
    },

    _addToPanelBox(role, indicator, position, box) {
        let container = indicator.container;
        container.show();

        let parent = container.get_parent();
        if (parent)
            parent.remove_actor(container);


        box.insert_child_at_index(container, position);
        if (indicator.menu)
            this.menuManager.addMenu(indicator.menu);
        this.statusArea[role] = indicator;
        let destroyId = indicator.connect('destroy', emitter => {
            delete this.statusArea[role];
            emitter.disconnect(destroyId);
            container.destroy();
        });
        indicator.connect('menu-set', this._onMenuSet.bind(this));
        this._onMenuSet(indicator);
    },

    addToStatusArea(role, indicator, position, box) {
        if (this.statusArea[role])
            throw new Error('Extension point conflict: there is already a status indicator for role ' + role);

        if (!(indicator instanceof PanelMenu.Button))
            throw new TypeError('Status indicator must be an instance of PanelMenu.Button');

        position = position || 0;
        let boxes = {
            left: this._leftBox,
            center: this._centerBox,
            right: this._rightBox
        };
        let boxContainer = boxes[box] || this._rightBox;
        this.statusArea[role] = indicator;
        this._addToPanelBox(role, indicator, position, boxContainer);
        return indicator;
    },

    _addStyleClassName(className) {
        this.actor.add_style_class_name(className);
        this._rightCorner.actor.add_style_class_name(className);
        this._leftCorner.actor.add_style_class_name(className);
    },

    _removeStyleClassName(className) {
        this.actor.remove_style_class_name(className);
        this._rightCorner.actor.remove_style_class_name(className);
        this._leftCorner.actor.remove_style_class_name(className);
    },

    _onMenuSet(indicator) {
        if (!indicator.menu || indicator.menu.hasOwnProperty('_openChangedId'))
            return;

        indicator.menu._openChangedId = indicator.menu.connect('open-state-changed',
            (menu, isOpen) => {
                let boxAlignment;
                if (this._leftBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.START;
                else if (this._centerBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.CENTER;
                else if (this._rightBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.END;

                if (boxAlignment == Main.messageTray.bannerAlignment)
                    Main.messageTray.bannerBlocked = isOpen;
            });
    }
});
