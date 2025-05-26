import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import St from 'gi://St';

import * as Config from '../misc/config.js';
import * as CtrlAltTab from './ctrlAltTab.js';
import * as DND from './dnd.js';
import * as PopupMenu from './popupMenu.js';
import * as PanelMenu from './panelMenu.js';
import {QuickSettingsMenu, SystemIndicator} from './quickSettings.js';
import * as Main from './main.js';
import * as Util from '../misc/util.js';

import * as RemoteAccessStatus from './status/remoteAccess.js';
import * as PowerProfileStatus from './status/powerProfiles.js';
import * as RFKillStatus from './status/rfkill.js';
import * as CameraStatus from './status/camera.js';
import * as VolumeStatus from './status/volume.js';
import * as BrightnessStatus from './status/brightness.js';
import * as SystemStatus from './status/system.js';
import * as LocationStatus from './status/location.js';
import * as NightLightStatus from './status/nightLight.js';
import * as DarkModeStatus from './status/darkMode.js';
import * as DoNotDisturb from './status/doNotDisturb.js';
import * as BacklightStatus from './status/backlight.js';
import * as ThunderboltStatus from './status/thunderbolt.js';
import * as AutoRotateStatus from './status/autoRotate.js';
import * as BackgroundAppsStatus from './status/backgroundApps.js';

import {DateMenuButton} from './dateMenu.js';
import {ATIndicator} from './status/accessibility.js';
import {InputSourceIndicator} from './status/keyboard.js';
import {DwellClickIndicator} from './status/dwellClick.js';
import {ScreenRecordingIndicator, ScreenSharingIndicator} from './status/remoteAccess.js';

const BUTTON_DND_ACTIVATION_TIMEOUT = 250;

const N_QUICK_SETTINGS_COLUMNS = 2;

const INACTIVE_WORKSPACE_DOT_SCALE = 0.75;

const WorkspaceDot = GObject.registerClass({
    Properties: {
        'expansion': GObject.ParamSpec.double('expansion', null, null,
            GObject.ParamFlags.READWRITE,
            0.0, 1.0, 0.0),
        'width-multiplier': GObject.ParamSpec.double(
            'width-multiplier', null, null,
            GObject.ParamFlags.READWRITE,
            1.0, 10.0, 1.0),
    },
}, class WorkspaceDot extends Clutter.Actor {
    constructor(params = {}) {
        super({
            pivot_point: new Graphene.Point({x: 0.5, y: 0.5}),
            ...params,
        });

        this._dot = new St.Widget({
            style_class: 'workspace-dot',
            y_align: Clutter.ActorAlign.CENTER,
            pivot_point: new Graphene.Point({x: 0.5, y: 0.5}),
            request_mode: Clutter.RequestMode.WIDTH_FOR_HEIGHT,
        });
        this.add_child(this._dot);

        this.connect('notify::width-multiplier', () => this.queue_relayout());
        this.connect('notify::expansion', () => {
            this._updateVisuals();
            this.queue_relayout();
        });
        this._updateVisuals();

        this._destroying = false;
    }

    _updateVisuals() {
        const {expansion} = this;

        this._dot.set({
            opacity: Util.lerp(0.50, 1.0, expansion) * 255,
            scaleX: Util.lerp(INACTIVE_WORKSPACE_DOT_SCALE, 1.0, expansion),
            scaleY: Util.lerp(INACTIVE_WORKSPACE_DOT_SCALE, 1.0, expansion),
        });
    }

    vfunc_get_preferred_width(forHeight) {
        const factor = Util.lerp(1.0, this.widthMultiplier, this.expansion);
        return this._dot.get_preferred_width(forHeight).map(v => Math.round(v * factor));
    }

    vfunc_get_preferred_height(forWidth) {
        return this._dot.get_preferred_height(forWidth);
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        box.set_origin(0, 0);
        this._dot.allocate(box);
    }

    scaleIn() {
        this.set({
            scale_x: 0,
            scale_y: 0,
        });

        this.ease({
            duration: 500,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            scale_x: 1.0,
            scale_y: 1.0,
        });
    }

    scaleOutAndDestroy() {
        this._destroying = true;

        this.ease({
            duration: 500,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            scale_x: 0.0,
            scale_y: 0.0,
            onComplete: () => this.destroy(),
        });
    }

    get destroying() {
        return this._destroying;
    }
});

const WorkspaceIndicators = GObject.registerClass(
class WorkspaceIndicators extends St.BoxLayout {
    constructor() {
        super();

        this._workspacesAdjustment = Main.createWorkspacesAdjustment(this);
        this._workspacesAdjustment.connectObject(
            'notify::value', () => this._updateExpansion(),
            'notify::upper', () => this._recalculateDots(),
            this);

        for (let i = 0; i < this._workspacesAdjustment.upper; i++)
            this.insert_child_at_index(new WorkspaceDot(), i);
        this._updateExpansion();
    }

    _getActiveIndicators() {
        return [...this].filter(i => !i.destroying);
    }

    _recalculateDots() {
        const activeIndicators = this._getActiveIndicators();
        const nIndicators = activeIndicators.length;
        const targetIndicators = this._workspacesAdjustment.upper;

        let remaining = Math.abs(nIndicators - targetIndicators);
        while (remaining--) {
            if (nIndicators < targetIndicators) {
                const indicator = new WorkspaceDot();
                this.add_child(indicator);
                indicator.scaleIn();
            } else {
                const indicator = activeIndicators[nIndicators - remaining - 1];
                indicator.scaleOutAndDestroy();
            }
        }

        this._updateExpansion();
    }

    _updateExpansion() {
        const nIndicators = this._getActiveIndicators().length;
        const activeWorkspace = this._workspacesAdjustment.value;

        let widthMultiplier;
        if (nIndicators <= 2)
            widthMultiplier = 3.625;
        else if (nIndicators <= 5)
            widthMultiplier = 3.25;
        else
            widthMultiplier = 2.75;

        this.get_children().forEach((indicator, index) => {
            const distance = Math.abs(index - activeWorkspace);
            indicator.expansion = Math.clamp(1 - distance, 0, 1);
            indicator.widthMultiplier = widthMultiplier;
        });
    }
});

const ActivitiesButton = GObject.registerClass(
class ActivitiesButton extends PanelMenu.Button {
    _init() {
        super._init(0.0, null, true);

        this.set({
            name: 'panelActivities',
            accessible_role: Atk.Role.TOGGLE_BUTTON,
            /* Translators: If there is no suitable word for "Activities"
               in your language, you can use the word for "Overview". */
            accessible_name: _('Activities'),
        });

        this.add_child(new WorkspaceIndicators());

        Main.overview.connectObject('showing',
            () => this.add_style_pseudo_class('checked'),
            this);
        Main.overview.connectObject('hiding',
            () => this.remove_style_pseudo_class('checked'),
            this);

        this._xdndTimeOut = 0;
    }

    handleDragOver(source, _actor, _x, _y, _time) {
        if (source !== Main.xdndHandler)
            return DND.DragMotionResult.CONTINUE;

        if (this._xdndTimeOut !== 0)
            GLib.source_remove(this._xdndTimeOut);
        this._xdndTimeOut = GLib.timeout_add(GLib.PRIORITY_DEFAULT, BUTTON_DND_ACTIVATION_TIMEOUT, () => {
            this._xdndToggleOverview();
        });
        GLib.Source.set_name_by_id(this._xdndTimeOut, '[gnome-shell] this._xdndToggleOverview');

        return DND.DragMotionResult.CONTINUE;
    }

    vfunc_event(event) {
        if (event.type() === Clutter.EventType.TOUCH_END ||
            event.type() === Clutter.EventType.BUTTON_RELEASE) {
            if (Main.overview.shouldToggleByCornerOrButton())
                Main.overview.toggle();
        }

        return Main.wm.handleWorkspaceScroll(event);
    }

    vfunc_key_release_event(event) {
        let symbol = event.get_key_symbol();
        if (symbol === Clutter.KEY_Return || symbol === Clutter.KEY_space) {
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

        if (pickedActor === this && Main.overview.shouldToggleByCornerOrButton())
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

const QuickSettings = GObject.registerClass(
class QuickSettings extends PanelMenu.Button {
    constructor() {
        super(0.0, C_('System menu in the top bar', 'System'), true);

        this._indicators = new St.BoxLayout({
            style_class: 'panel-status-indicators-box',
        });
        this.add_child(this._indicators);

        this.setMenu(new QuickSettingsMenu(this, N_QUICK_SETTINGS_COLUMNS));

        this._setupIndicators().catch(error =>
            logError(error, 'Failed to setup quick settings'));
    }

    async _setupIndicators() {
        if (Config.HAVE_NETWORKMANAGER) {
            /** @type {import('./status/network.js')} */
            const NetworkStatus = await import('./status/network.js');

            this._network = new NetworkStatus.Indicator();
        } else {
            this._network = null;
        }

        if (Config.HAVE_BLUETOOTH) {
            /** @type {import('./status/bluetooth.js')} */
            const BluetoothStatus = await import('./status/bluetooth.js');

            this._bluetooth = new BluetoothStatus.Indicator();
        } else {
            this._bluetooth = null;
        }

        this._system = new SystemStatus.Indicator();
        this._camera = new CameraStatus.Indicator();
        this._volumeOutput = new VolumeStatus.OutputIndicator();
        this._volumeInput = new VolumeStatus.InputIndicator();
        this._brightness = new BrightnessStatus.Indicator();
        this._remoteAccess = new RemoteAccessStatus.RemoteAccessApplet();
        this._location = new LocationStatus.Indicator();
        this._thunderbolt = new ThunderboltStatus.Indicator();
        this._nightLight = new NightLightStatus.Indicator();
        this._darkMode = new DarkModeStatus.Indicator();
        this._doNotDisturb = new DoNotDisturb.Indicator();
        this._backlight = new BacklightStatus.Indicator();
        this._powerProfiles = new PowerProfileStatus.Indicator();
        this._rfkill = new RFKillStatus.Indicator();
        this._autoRotate = new AutoRotateStatus.Indicator();
        this._unsafeMode = new UnsafeModeIndicator();
        this._backgroundApps = new BackgroundAppsStatus.Indicator();

        // add privacy-related indicators before any external indicators
        let pos = 0;
        this._indicators.insert_child_at_index(this._remoteAccess, pos++);
        this._indicators.insert_child_at_index(this._camera, pos++);
        this._indicators.insert_child_at_index(this._volumeInput, pos++);
        this._indicators.insert_child_at_index(this._location, pos++);

        // append all other indicators
        this._indicators.add_child(this._brightness);
        this._indicators.add_child(this._thunderbolt);
        this._indicators.add_child(this._nightLight);
        if (this._network)
            this._indicators.add_child(this._network);
        this._indicators.add_child(this._darkMode);
        this._indicators.add_child(this._doNotDisturb);
        this._indicators.add_child(this._backlight);
        this._indicators.add_child(this._powerProfiles);
        if (this._bluetooth)
            this._indicators.add_child(this._bluetooth);
        this._indicators.add_child(this._rfkill);
        this._indicators.add_child(this._autoRotate);
        this._indicators.add_child(this._volumeOutput);
        this._indicators.add_child(this._unsafeMode);
        this._indicators.add_child(this._system);

        // add our quick settings items before any external ones
        const sibling = this.menu.getFirstItem();
        this._addItemsBefore(this._system.quickSettingsItems,
            sibling, N_QUICK_SETTINGS_COLUMNS);
        this._addItemsBefore(this._volumeOutput.quickSettingsItems,
            sibling, N_QUICK_SETTINGS_COLUMNS);
        this._addItemsBefore(this._volumeInput.quickSettingsItems,
            sibling, N_QUICK_SETTINGS_COLUMNS);
        this._addItemsBefore(this._brightness.quickSettingsItems,
            sibling, N_QUICK_SETTINGS_COLUMNS);

        this._addItemsBefore(this._camera.quickSettingsItems, sibling);
        this._addItemsBefore(this._remoteAccess.quickSettingsItems, sibling);
        this._addItemsBefore(this._thunderbolt.quickSettingsItems, sibling);
        this._addItemsBefore(this._location.quickSettingsItems, sibling);
        if (this._network)
            this._addItemsBefore(this._network.quickSettingsItems, sibling);
        if (this._bluetooth)
            this._addItemsBefore(this._bluetooth.quickSettingsItems, sibling);
        this._addItemsBefore(this._powerProfiles.quickSettingsItems, sibling);
        this._addItemsBefore(this._nightLight.quickSettingsItems, sibling);
        this._addItemsBefore(this._darkMode.quickSettingsItems, sibling);
        this._addItemsBefore(this._doNotDisturb.quickSettingsItems, sibling);
        this._addItemsBefore(this._backlight.quickSettingsItems, sibling);
        this._addItemsBefore(this._rfkill.quickSettingsItems, sibling);
        this._addItemsBefore(this._autoRotate.quickSettingsItems, sibling);
        this._addItemsBefore(this._unsafeMode.quickSettingsItems, sibling);

        // append background apps
        this._backgroundApps.quickSettingsItems.forEach(
            item => this.menu.addItem(item, N_QUICK_SETTINGS_COLUMNS));
    }

    _addItemsBefore(items, sibling, colSpan = 1) {
        items.forEach(item => this.menu.insertItemBefore(item, sibling, colSpan));
    }

    /**
     * Insert indicator and quick settings items at
     * appropriate positions
     *
     * @param {PanelMenu.Button} indicator
     * @param {number=} colSpan
     */
    addExternalIndicator(indicator, colSpan = 1) {
        // Insert before first non-privacy indicator if it exists
        let sibling = this._brightness ?? null;
        this._indicators.insert_child_below(indicator, sibling);

        // Insert before background apps if it exists
        sibling = this._backgroundApps?.quickSettingsItems?.at(-1) ?? null;
        this._addItemsBefore(indicator.quickSettingsItems, sibling, colSpan);
    }
});

const PANEL_ITEM_IMPLEMENTATIONS = {
    'activities': ActivitiesButton,
    'quickSettings': QuickSettings,
    'dateMenu': DateMenuButton,
    'a11y': ATIndicator,
    'keyboard': InputSourceIndicator,
    'dwellClick': DwellClickIndicator,
    'screenRecording': ScreenRecordingIndicator,
    'screenSharing': ScreenSharingIndicator,
};

export const Panel = GObject.registerClass(
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

        this._leftBox = new St.BoxLayout({name: 'panelLeft'});
        this.add_child(this._leftBox);
        this._centerBox = new St.BoxLayout({name: 'panelCenter'});
        this.add_child(this._centerBox);
        this._rightBox = new St.BoxLayout({name: 'panelRight'});
        this.add_child(this._rightBox);

        this.connect('button-press-event', this._onButtonPress.bind(this));
        this.connect('touch-event', this._onTouchEvent.bind(this));

        Main.overview.connectObject('showing',
            () => this.add_style_pseudo_class('overview'),
            this);
        Main.overview.connectObject('hiding',
            () => this.remove_style_pseudo_class('overview'),
            this);

        Main.layoutManager.panelBox.add_child(this);
        Main.ctrlAltTabManager.addGroup(this,
            _('Top Bar'), 'shell-focus-top-bar-symbolic',
            {sortGroup: CtrlAltTab.SortGroup.TOP});

        Main.sessionMode.connectObject('updated',
            this._updatePanel.bind(this),
            this);

        global.display.connectObject('workareas-changed',
            () => this.queue_relayout(),
            this);
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
        if (this.get_text_direction() === Clutter.TextDirection.RTL) {
            childBox.x1 = Math.max(
                allocWidth - Math.min(Math.floor(sideWidth), leftNaturalWidth),
                0);
            childBox.x2 = allocWidth;
        } else {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth), leftNaturalWidth);
        }
        this._leftBox.allocate(childBox);

        childBox.x1 = Math.ceil(sideWidth);
        childBox.y1 = 0;
        childBox.x2 = childBox.x1 + centerWidth;
        childBox.y2 = allocHeight;
        this._centerBox.allocate(childBox);

        childBox.y1 = 0;
        childBox.y2 = allocHeight;
        if (this.get_text_direction() === Clutter.TextDirection.RTL) {
            childBox.x1 = 0;
            childBox.x2 = Math.min(Math.floor(sideWidth), rightNaturalWidth);
        } else {
            childBox.x1 = Math.max(
                allocWidth - Math.min(Math.floor(sideWidth), rightNaturalWidth),
                0);
            childBox.x2 = allocWidth;
        }
        this._rightBox.allocate(childBox);
    }

    _tryDragWindow(event) {
        if (Main.modalCount > 0)
            return Clutter.EVENT_PROPAGATE;

        const backend = global.stage.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);

        const targetActor = global.stage.get_event_actor(event);
        if (targetActor !== this)
            return Clutter.EVENT_PROPAGATE;

        const [x, y] = event.get_coords();
        let dragWindow = this._getDraggableWindowForPosition(x);

        if (!dragWindow)
            return Clutter.EVENT_PROPAGATE;

        const positionHint = new Graphene.Point({x, y});
        return dragWindow.begin_grab_op(
            Meta.GrabOp.MOVING,
            sprite,
            event.get_time(),
            positionHint) ? Clutter.EVENT_STOP : Clutter.EVENT_PROPAGATE;
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

    vfunc_key_press_event(event) {
        let symbol = event.get_key_symbol();
        if (symbol === Clutter.KEY_Escape) {
            global.display.focus_default_window(event.get_time());
            return Clutter.EVENT_STOP;
        }

        return super.vfunc_key_press_event(event);
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

    toggleCalendar() {
        this._toggleMenu(this.statusArea.dateMenu);
    }

    toggleQuickSettings() {
        this._toggleMenu(this.statusArea.quickSettings);
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
            parent.remove_child(container);


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
        if (!indicator.menu || indicator.menu._openChangedConnected)
            return;

        this.menuManager.addMenu(indicator.menu);

        indicator.menu._openChangedConnected = true;
        indicator.menu.connectObject('open-state-changed',
            (menu, isOpen) => {
                let boxAlignment;
                if (this._leftBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.START;
                else if (this._centerBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.CENTER;
                else if (this._rightBox.contains(indicator.container))
                    boxAlignment = Clutter.ActorAlign.END;

                if (boxAlignment === Main.messageTray.bannerAlignment)
                    Main.messageTray.bannerBlocked = isOpen;
            }, this);
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
                   metaWindow.get_window_type() !== Meta.WindowType.DESKTOP &&
                   metaWindow.maximized_vertically &&
                   stageX > rect.x && stageX < rect.x + rect.width;
        });
    }
});
