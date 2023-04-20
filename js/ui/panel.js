// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Panel */

const { Atk, Clutter, GLib, GObject, Meta, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const { AppMenu } = imports.ui.appMenu;
const Config = imports.misc.config;
const CtrlAltTab = imports.ui.ctrlAltTab;
const DND = imports.ui.dnd;
const Overview = imports.ui.overview;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const {QuickSettingsMenu, SystemIndicator} = imports.ui.quickSettings;
const Main = imports.ui.main;

var PANEL_ICON_SIZE = 16;
var APP_MENU_ICON_MARGIN = 0;

var BUTTON_DND_ACTIVATION_TIMEOUT = 250;

const N_QUICK_SETTINGS_COLUMNS = 2;

/**
 * AppMenuButton:
 *
 * This class manages the "application menu" component.  It tracks the
 * currently focused application.  However, when an app is launched,
 * this menu also handles startup notification for it.  So when we
 * have an active startup notification, we switch modes to display that.
 */
var AppMenuButton = GObject.registerClass({
    Signals: { 'changed': {} },
}, class AppMenuButton extends PanelMenu.Button {
    _init(panel) {
        super._init(0.0, null, true);

        this.accessible_role = Atk.Role.MENU;

        this._startingApps = [];

        this._menuManager = panel.menuManager;
        this._targetApp = null;

        let bin = new St.Bin({ name: 'appMenu' });
        this.add_actor(bin);

        this.bind_property("reactive", this, "can-focus", 0);
        this.reactive = false;

        this._container = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        bin.set_child(this._container);

        let textureCache = St.TextureCache.get_default();
        textureCache.connect('icon-theme-changed',
                             this._onIconThemeChanged.bind(this));

        let iconEffect = new Clutter.DesaturateEffect();
        this._iconBox = new St.Bin({
            style_class: 'app-menu-icon',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._iconBox.add_effect(iconEffect);
        this._container.add_actor(this._iconBox);

        this._iconBox.connect('style-changed', () => {
            let themeNode = this._iconBox.get_theme_node();
            iconEffect.enabled = themeNode.get_icon_style() == St.IconStyle.SYMBOLIC;
        });

        this._label = new St.Label({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._container.add_actor(this._label);

        this._visible = !Main.overview.visible;
        if (!this._visible)
            this.hide();
        Main.overview.connectObject(
            'hiding', this._sync.bind(this),
            'showing', this._sync.bind(this), this);

        this._spinner = new Animation.Spinner(PANEL_ICON_SIZE, {
            animate: true,
            hideOnStop: true,
        });
        this._container.add_actor(this._spinner);

        let menu = new AppMenu(this);
        this.setMenu(menu);
        this._menuManager.addMenu(menu);

        Shell.WindowTracker.get_default().connectObject('notify::focus-app',
            this._focusAppChanged.bind(this), this);
        Shell.AppSystem.get_default().connectObject('app-state-changed',
            this._onAppStateChanged.bind(this), this);
        global.window_manager.connectObject('switch-workspace',
            this._sync.bind(this), this);

        this._sync();
    }

    fadeIn() {
        if (this._visible)
            return;

        this._visible = true;
        this.reactive = true;
        this.remove_all_transitions();
        this.ease({
            opacity: 255,
            duration: Overview.ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    fadeOut() {
        if (!this._visible)
            return;

        this._visible = false;
        this.reactive = false;
        this.remove_all_transitions();
        this.ease({
            opacity: 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: Overview.ANIMATION_TIME,
        });
    }

    _syncIcon(app) {
        const icon = app.create_icon_texture(PANEL_ICON_SIZE - APP_MENU_ICON_MARGIN);
        this._iconBox.set_child(icon);
    }

    _onIconThemeChanged() {
        if (this._iconBox.child == null)
            return;

        if (this._targetApp)
            this._syncIcon(this._targetApp);
    }

    stopAnimation() {
        this._spinner.stop();
    }

    startAnimation() {
        this._spinner.play();
    }

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
    }

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
    }

    _findTargetApp() {
        let workspaceManager = global.workspace_manager;
        let workspace = workspaceManager.get_active_workspace();
        let tracker = Shell.WindowTracker.get_default();
        let focusedApp = tracker.focus_app;
        if (focusedApp && focusedApp.is_on_workspace(workspace))
            return focusedApp;

        for (let i = 0; i < this._startingApps.length; i++) {
            if (this._startingApps[i].is_on_workspace(workspace))
                return this._startingApps[i];
        }

        return null;
    }

    _sync() {
        let targetApp = this._findTargetApp();

        if (this._targetApp != targetApp) {
            this._targetApp?.disconnectObject(this);

            this._targetApp = targetApp;

            if (this._targetApp) {
                this._targetApp.connectObject('notify::busy', this._sync.bind(this), this);
                this._label.set_text(this._targetApp.get_name());
                this.set_accessible_name(this._targetApp.get_name());

                this._syncIcon(this._targetApp);
            }
        }

        let visible = this._targetApp != null && !Main.overview.visibleTarget;
        if (visible)
            this.fadeIn();
        else
            this.fadeOut();

        let isBusy = this._targetApp != null &&
                      (this._targetApp.get_state() == Shell.AppState.STARTING ||
                       this._targetApp.get_busy());
        if (isBusy)
            this.startAnimation();
        else
            this.stopAnimation();

        this.reactive = visible && !isBusy;

        this.menu.setApp(this._targetApp);
        this.emit('changed');
    }
});

var ActivitiesButton = GObject.registerClass(
class ActivitiesButton extends PanelMenu.Button {
    _init() {
        super._init(0.0, null, true);
        this.accessible_role = Atk.Role.TOGGLE_BUTTON;

        this.name = 'panelActivities';

        /* Translators: If there is no suitable word for "Activities"
           in your language, you can use the word for "Overview". */
        this._label = new St.Label({
            text: _('Activities'),
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_actor(this._label);

        this.label_actor = this._label;

        Main.overview.connect('showing', () => {
            this.add_style_pseudo_class('overview');
            this.add_accessible_state(Atk.StateType.CHECKED);
        });
        Main.overview.connect('hiding', () => {
            this.remove_style_pseudo_class('overview');
            this.remove_accessible_state(Atk.StateType.CHECKED);
        });

        this._xdndTimeOut = 0;
    }

    handleDragOver(source, _actor, _x, _y, _time) {
        if (source != Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        if (this._xdndTimeOut != 0)
            GLib.source_remove(this._xdndTimeOut);
        this._xdndTimeOut = GLib.timeout_add(GLib.PRIORITY_DEFAULT, BUTTON_DND_ACTIVATION_TIMEOUT, () => {
            this._xdndToggleOverview();
        });
        GLib.Source.set_name_by_id(this._xdndTimeOut, '[gnome-shell] this._xdndToggleOverview');

        return DND.DragMotionResult.CONTINUE;
    }

    vfunc_event(event) {
        if (event.type() == Clutter.EventType.TOUCH_END ||
            event.type() == Clutter.EventType.BUTTON_RELEASE) {
            if (Main.overview.shouldToggleByCornerOrButton())
                Main.overview.toggle();
        }

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_key_release_event(keyEvent) {
        let symbol = keyEvent.keyval;
        if (symbol == Clutter.KEY_Return || symbol == Clutter.KEY_space) {
            if (Main.overview.shouldToggleByCornerOrButton()) {
                Main.overview.toggle();
                return Clutter.EVENT_STOP;
            }
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _xdndToggleOverview() {
        let [x, y] = global.get_pointer();
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);

        if (pickedActor == this && Main.overview.shouldToggleByCornerOrButton())
            Main.overview.toggle();

        GLib.source_remove(this._xdndTimeOut);
        this._xdndTimeOut = 0;
        return GLib.SOURCE_REMOVE;
    }
});

const UnsafeModeIndicator = GObject.registerClass(
class UnsafeModeIndicator extends SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'channel-insecure-symbolic';

        global.context.bind_property('unsafe-mode',
            this._indicator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
    }
});

var QuickSettings = GObject.registerClass(
class QuickSettings extends PanelMenu.Button {
    _init() {
        super._init(0.0, C_('System menu in the top bar', 'System'), true);

        this._indicators = new St.BoxLayout({
            style_class: 'panel-status-indicators-box',
        });
        this.add_child(this._indicators);

        this.setMenu(new QuickSettingsMenu(this, N_QUICK_SETTINGS_COLUMNS));

        if (Config.HAVE_NETWORKMANAGER)
            this._network = new imports.ui.status.network.Indicator();
        else
            this._network = null;

        if (Config.HAVE_BLUETOOTH)
            this._bluetooth = new imports.ui.status.bluetooth.Indicator();
        else
            this._bluetooth = null;

        this._system = new imports.ui.status.system.Indicator();
        this._volume = new imports.ui.status.volume.Indicator();
        this._brightness = new imports.ui.status.brightness.Indicator();
        this._remoteAccess = new imports.ui.status.remoteAccess.RemoteAccessApplet();
        this._location = new imports.ui.status.location.Indicator();
        this._thunderbolt = new imports.ui.status.thunderbolt.Indicator();
        this._nightLight = new imports.ui.status.nightLight.Indicator();
        this._darkMode = new imports.ui.status.darkMode.Indicator();
        this._powerProfiles = new imports.ui.status.powerProfiles.Indicator();
        this._rfkill = new imports.ui.status.rfkill.Indicator();
        this._autoRotate = new imports.ui.status.autoRotate.Indicator();
        this._unsafeMode = new UnsafeModeIndicator();
        this._backgroundApps = new imports.ui.status.backgroundApps.Indicator();

        this._indicators.add_child(this._brightness);
        this._indicators.add_child(this._remoteAccess);
        this._indicators.add_child(this._thunderbolt);
        this._indicators.add_child(this._location);
        this._indicators.add_child(this._nightLight);
        if (this._network)
            this._indicators.add_child(this._network);
        this._indicators.add_child(this._darkMode);
        this._indicators.add_child(this._powerProfiles);
        if (this._bluetooth)
            this._indicators.add_child(this._bluetooth);
        this._indicators.add_child(this._rfkill);
        this._indicators.add_child(this._autoRotate);
        this._indicators.add_child(this._volume);
        this._indicators.add_child(this._unsafeMode);
        this._indicators.add_child(this._system);

        this._addItems(this._system.quickSettingsItems, N_QUICK_SETTINGS_COLUMNS);
        this._addItems(this._volume.quickSettingsItems, N_QUICK_SETTINGS_COLUMNS);
        this._addItems(this._brightness.quickSettingsItems, N_QUICK_SETTINGS_COLUMNS);

        this._addItems(this._remoteAccess.quickSettingsItems);
        this._addItems(this._thunderbolt.quickSettingsItems);
        this._addItems(this._location.quickSettingsItems);
        if (this._network)
            this._addItems(this._network.quickSettingsItems);
        if (this._bluetooth)
            this._addItems(this._bluetooth.quickSettingsItems);
        this._addItems(this._powerProfiles.quickSettingsItems);
        this._addItems(this._nightLight.quickSettingsItems);
        this._addItems(this._darkMode.quickSettingsItems);
        this._addItems(this._rfkill.quickSettingsItems);
        this._addItems(this._autoRotate.quickSettingsItems);
        this._addItems(this._unsafeMode.quickSettingsItems);

        this._addItems(this._backgroundApps.quickSettingsItems, N_QUICK_SETTINGS_COLUMNS);
    }

    _addItems(items, colSpan = 1) {
        items.forEach(item => this.menu.addItem(item, colSpan));
    }
});

const PANEL_ITEM_IMPLEMENTATIONS = {
    'activities': ActivitiesButton,
    'appMenu': AppMenuButton,
    'quickSettings': QuickSettings,
    'dateMenu': imports.ui.dateMenu.DateMenuButton,
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'keyboard': imports.ui.status.keyboard.InputSourceIndicator,
    'dwellClick': imports.ui.status.dwellClick.DwellClickIndicator,
    'screenRecording': imports.ui.status.remoteAccess.ScreenRecordingIndicator,
    'screenSharing': imports.ui.status.remoteAccess.ScreenSharingIndicator,
};

var Panel = GObject.registerClass(
class Panel extends St.Widget {
    _init() {
        super._init({
            name: 'panel',
            reactive: true,
        });

        this.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

        this._sessionStyle = null;

        this.statusArea = {};

        this.menuManager = new PopupMenu.PopupMenuManager(this);

        this._leftBox = new St.BoxLayout({ name: 'panelLeft' });
        this.add_child(this._leftBox);
        this._centerBox = new St.BoxLayout({ name: 'panelCenter' });
        this.add_child(this._centerBox);
        this._rightBox = new St.BoxLayout({ name: 'panelRight' });
        this.add_child(this._rightBox);

        this.connect('button-press-event', this._onButtonPress.bind(this));
        this.connect('touch-event', this._onTouchEvent.bind(this));

        Main.overview.connect('showing', () => {
            this.add_style_pseudo_class('overview');
        });
        Main.overview.connect('hiding', () => {
            this.remove_style_pseudo_class('overview');
        });

        Main.layoutManager.panelBox.add(this);
        Main.ctrlAltTabManager.addGroup(this, _("Top Bar"), 'focus-top-bar-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.TOP });

        Main.sessionMode.connect('updated', this._updatePanel.bind(this));

        global.display.connect('workareas-changed', () => this.queue_relayout());
        this._updatePanel();
    }

    vfunc_get_preferred_width(_forHeight) {
        let primaryMonitor = Main.layoutManager.primaryMonitor;

        if (primaryMonitor)
            return [0, primaryMonitor.width];

        return [0,  0];
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        let allocWidth = box.x2 - box.x1;
        let allocHeight = box.y2 - box.y1;

        let [, leftNaturalWidth] = this._leftBox.get_preferred_width(-1);
        let [, centerNaturalWidth] = this._centerBox.get_preferred_width(-1);
        let [, rightNaturalWidth] = this._rightBox.get_preferred_width(-1);

        let sideWidth, centerWidth;
        centerWidth = centerNaturalWidth;

        // get workspace area and center date entry relative to it
        let monitor = Main.layoutManager.findMonitorForActor(this);
        let centerOffset = 0;
        if (monitor) {
            let workArea = Main.layoutManager.getWorkAreaForMonitor(monitor.index);
            centerOffset = 2 * (workArea.x - monitor.x) + workArea.width - monitor.width;
        }

        sideWidth = Math.max(0, (allocWidth - centerWidth + centerOffset) / 2);

        let childBox = new Clutter.ActorBox();

        childBox.y1 = 0;
        childBox.y2 = allocHeight;
        if (this.get_text_direction() == Clutter.TextDirection.RTL) {
            childBox.x1 = Math.max(allocWidth - Math.min(Math.floor(sideWidth),
                                                         leftNaturalWidth),
                                   0);
            childBox.x2 = allocWidth;
        } else {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth),
                                   leftNaturalWidth);
        }
        this._leftBox.allocate(childBox);

        childBox.x1 = Math.ceil(sideWidth);
        childBox.y1 = 0;
        childBox.x2 = childBox.x1 + centerWidth;
        childBox.y2 = allocHeight;
        this._centerBox.allocate(childBox);

        childBox.y1 = 0;
        childBox.y2 = allocHeight;
        if (this.get_text_direction() == Clutter.TextDirection.RTL) {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth),
                                   rightNaturalWidth);
        } else {
            childBox.x1 = Math.max(allocWidth - Math.min(Math.floor(sideWidth),
                                                         rightNaturalWidth),
                                   0);
            childBox.x2 = allocWidth;
        }
        this._rightBox.allocate(childBox);
    }

    _tryDragWindow(event) {
        if (Main.modalCount > 0)
            return Clutter.EVENT_PROPAGATE;

        const targetActor = global.stage.get_event_actor(event);
        if (targetActor !== this)
            return Clutter.EVENT_PROPAGATE;

        const [x, y_] = event.get_coords();
        let dragWindow = this._getDraggableWindowForPosition(x);

        if (!dragWindow)
            return Clutter.EVENT_PROPAGATE;

        return dragWindow.begin_grab_op(
            Meta.GrabOp.MOVING,
            event.get_device(),
            event.get_event_sequence(),
            event.get_time()) ? Clutter.EVENT_STOP : Clutter.EVENT_PROPAGATE;
    }

    _onButtonPress(actor, event) {
        if (event.get_button() !== Clutter.BUTTON_PRIMARY)
            return Clutter.EVENT_PROPAGATE;

        return this._tryDragWindow(event);
    }

    _onTouchEvent(actor, event) {
        if (event.type() !== Clutter.EventType.TOUCH_BEGIN)
            return Clutter.EVENT_PROPAGATE;

        return this._tryDragWindow(event);
    }

    vfunc_key_press_event(keyEvent) {
        let symbol = keyEvent.keyval;
        if (symbol == Clutter.KEY_Escape) {
            global.display.focus_default_window(keyEvent.time);
            return Clutter.EVENT_STOP;
        }

        return super.vfunc_key_press_event(keyEvent);
    }

    _toggleMenu(indicator) {
        if (!indicator || !indicator.mapped)
            return; // menu not supported by current session mode

        let menu = indicator.menu;
        if (!indicator.reactive)
            return;

        menu.toggle();
        if (menu.isOpen)
            menu.actor.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
    }

    _closeMenu(indicator) {
        if (!indicator || !indicator.mapped)
            return; // menu not supported by current session mode

        if (!indicator.reactive)
            return;

        indicator.menu.close();
    }

    toggleAppMenu() {
        this._toggleMenu(this.statusArea.appMenu);
    }

    toggleCalendar() {
        this._toggleMenu(this.statusArea.dateMenu);
    }

    closeCalendar() {
        this._closeMenu(this.statusArea.dateMenu);
    }

    closeQuickSettings() {
        this._closeMenu(this.statusArea.quickSettings);
    }

    set boxOpacity(value) {
        let isReactive = value > 0;

        this._leftBox.opacity = value;
        this._leftBox.reactive = isReactive;
        this._centerBox.opacity = value;
        this._centerBox.reactive = isReactive;
        this._rightBox.opacity = value;
        this._rightBox.reactive = isReactive;
    }

    get boxOpacity() {
        return this._leftBox.opacity;
    }

    _updatePanel() {
        let panel = Main.sessionMode.panel;
        this._hideIndicators();
        this._updateBox(panel.left, this._leftBox);
        this._updateBox(panel.center, this._centerBox);
        this._updateBox(panel.right, this._rightBox);

        if (panel.left.includes('dateMenu'))
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.START;
        else if (panel.right.includes('dateMenu'))
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.END;
        // Default to center if there is no dateMenu
        else
            Main.messageTray.bannerAlignment = Clutter.ActorAlign.CENTER;

        if (this._sessionStyle)
            this.remove_style_class_name(this._sessionStyle);

        this._sessionStyle = Main.sessionMode.panelStyle;
        if (this._sessionStyle)
            this.add_style_class_name(this._sessionStyle);
    }

    _hideIndicators() {
        for (let role in PANEL_ITEM_IMPLEMENTATIONS) {
            let indicator = this.statusArea[role];
            if (!indicator)
                continue;
            indicator.container.hide();
        }
    }

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
    }

    _updateBox(elements, box) {
        let nChildren = box.get_n_children();

        for (let i = 0; i < elements.length; i++) {
            let role = elements[i];
            let indicator = this._ensureIndicator(role);
            if (indicator == null)
                continue;

            this._addToPanelBox(role, indicator, i + nChildren, box);
        }
    }

    _addToPanelBox(role, indicator, position, box) {
        let container = indicator.container;
        container.show();

        let parent = container.get_parent();
        if (parent)
            parent.remove_actor(container);


        box.insert_child_at_index(container, position);
        this.statusArea[role] = indicator;
        let destroyId = indicator.connect('destroy', emitter => {
            delete this.statusArea[role];
            emitter.disconnect(destroyId);
        });
        indicator.connect('menu-set', this._onMenuSet.bind(this));
        this._onMenuSet(indicator);
    }

    addToStatusArea(role, indicator, position, box) {
        if (this.statusArea[role])
            throw new Error(`Extension point conflict: there is already a status indicator for role ${role}`);

        if (!(indicator instanceof PanelMenu.Button))
            throw new TypeError('Status indicator must be an instance of PanelMenu.Button');

        position ??= 0;
        let boxes = {
            left: this._leftBox,
            center: this._centerBox,
            right: this._rightBox,
        };
        let boxContainer = boxes[box] || this._rightBox;
        this.statusArea[role] = indicator;
        this._addToPanelBox(role, indicator, position, boxContainer);
        return indicator;
    }

    _onMenuSet(indicator) {
        if (!indicator.menu || indicator.menu._openChangedId)
            return;

        this.menuManager.addMenu(indicator.menu);

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

    _getDraggableWindowForPosition(stageX) {
        let workspaceManager = global.workspace_manager;
        const windows = workspaceManager.get_active_workspace().list_windows();
        const allWindowsByStacking =
            global.display.sort_windows_by_stacking(windows).reverse();

        return allWindowsByStacking.find(metaWindow => {
            let rect = metaWindow.get_frame_rect();
            return metaWindow.is_on_primary_monitor() &&
                   metaWindow.showing_on_its_workspace() &&
                   metaWindow.get_window_type() != Meta.WindowType.DESKTOP &&
                   metaWindow.maximized_vertically &&
                   stageX > rect.x && stageX < rect.x + rect.width;
        });
    }
});
