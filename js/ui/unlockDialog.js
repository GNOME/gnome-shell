import AccountsService from 'gi://AccountsService';
import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Gdm from 'gi://Gdm';
import Gio from 'gi://Gio';
import GnomeDesktop from 'gi://GnomeDesktop';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Background from './background.js';
import * as Layout from './layout.js';
import * as Main from './main.js';
import * as MessageTray from './messageTray.js';
import * as SwipeTracker from './swipeTracker.js';
import {formatDateWithCFormatString} from '../misc/dateUtils.js';
import {TimeLimitsState} from '../misc/timeLimitsManager.js';
import * as AuthMenuButton from '../gdm/authMenuButton.js';
import * as AuthPrompt from '../gdm/authPrompt.js';
import * as GdmConst from '../gdm/const.js';
import * as GdmUtil from '../gdm/util.js';
import {AuthPromptStatus} from '../gdm/authPrompt.js';
import {MprisSource} from './mpris.js';
import {MediaMessage} from './messageList.js';

const PRIMARY_UNLOCK_METHOD_SECTION_NAME = _('Unlock Options');

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

// The timeout before showing the unlock hint (in seconds)
const HINT_TIMEOUT = 4;

const CROSSFADE_TIME = 300;
const FADE_OUT_TRANSLATION = 200;
const FADE_OUT_SCALE = 0.3;

const BLUR_BRIGHTNESS = 0.65;
const BLUR_RADIUS = 90;

const FIXED_PROMPT_HEIGHT = 400;

const NotificationsBox = GObject.registerClass({
    Signals: {'wake-up-screen': {}},
}, class NotificationsBox extends St.BoxLayout {
    _init() {
        super._init({
            orientation: Clutter.Orientation.VERTICAL,
            name: 'unlockDialogNotifications',
        });

        this._notificationBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'unlock-dialog-notifications-container',
        });

        this._scrollView = new St.ScrollView({
            child: this._notificationBox,
        });
        this.add_child(this._scrollView);

        this._players = new Map();
        this._mediaSource = new MprisSource();
        this._mediaSource.connectObject(
            'player-added', (o, player) => this._addPlayer(player),
            'player-removed', (o, player) => this._removePlayer(player),
            this);
        this._mediaSource.players.forEach(player => {
            this._addPlayer(player);
        });

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.notifications',
        });

        this._sources = new Map();
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source, true);
        });
        this._updateVisibility();

        Main.messageTray.connectObject('source-added',
            this._sourceAdded.bind(this), this);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        const items = this._sources.entries();
        for (const [source, obj] of items)
            this._removeSource(source, obj);

        for (const player of this._players.keys())
            this._removePlayer(player);
    }

    _updateVisibility() {
        this._notificationBox.visible =
            this._notificationBox.get_children().some(a => a.visible);

        this.visible = this._notificationBox.visible;
    }

    _makeNotificationSource(source, box) {
        const iconActor = new St.Icon({
            style_class: 'unlock-dialog-notification-icon',
            fallback_icon_name: 'application-x-executable',
        });
        source.bind_property('icon', iconActor, 'gicon', GObject.BindingFlags.SYNC_CREATE);
        box.add_child(iconActor);

        const textBox = new St.BoxLayout({
            x_expand: true,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(textBox);

        const title = new St.Label({
            style_class: 'unlock-dialog-notification-label',
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
        });
        source.bind_property('title',
            title, 'text',
            GObject.BindingFlags.SYNC_CREATE);
        textBox.add_child(title);

        const count = source.unseenCount;
        const countLabel = new St.Label({
            text: `${count}`,
            visible: count > 1,
            style_class: 'unlock-dialog-notification-count-text',
        });
        textBox.add_child(countLabel);

        box.visible = count !== 0;
        return [title, countLabel];
    }

    _makeNotificationDetailedSource(source, box) {
        const iconActor = new St.Icon({
            style_class: 'unlock-dialog-notification-icon',
            fallback_icon_name: 'application-x-executable',
        });
        source.bind_property('icon', iconActor, 'gicon', GObject.BindingFlags.SYNC_CREATE);
        box.add_child(iconActor);

        const textBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
        });
        box.add_child(textBox);

        const title = new St.Label({
            style_class: 'unlock-dialog-notification-label',
        });
        source.bind_property_full('title',
            title, 'text',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, sourceVal) => [true, sourceVal?.replace(/\n/g, ' ') ?? ''],
            null);
        textBox.add_child(title);

        let visible = false;
        for (let i = 0; i < source.notifications.length; i++) {
            const n = source.notifications[i];

            if (n.acknowledged)
                continue;

            let body = '';
            if (n.body) {
                const bodyText = n.body.replace(/\n/g, ' ');
                body = n.useBodyMarkup
                    ? bodyText
                    : GLib.markup_escape_text(bodyText, -1);
            }

            const label = new St.Label({style_class: 'unlock-dialog-notification-count-text'});
            label.clutter_text.set_markup(`<b>${n.title}</b> ${body}`);
            textBox.add_child(label);

            visible = true;
        }

        box.visible = visible;
        return [title, null];
    }

    _shouldShowDetails(source) {
        return source.policy.detailsInLockScreen ||
               source.narrowestPrivacyScope === MessageTray.PrivacyScope.SYSTEM;
    }

    _updateSourceBoxStyle(source, obj, box) {
        const hasCriticalNotification =
            source.notifications.some(n => n.urgency === MessageTray.Urgency.CRITICAL);

        if (hasCriticalNotification !== obj.hasCriticalNotification) {
            obj.hasCriticalNotification = hasCriticalNotification;

            if (hasCriticalNotification)
                box.add_style_class_name('critical');
            else
                box.remove_style_class_name('critical');
        }
    }

    _showSource(source, obj, box) {
        if (obj.detailed)
            [obj.titleLabel, obj.countLabel] = this._makeNotificationDetailedSource(source, box);
        else
            [obj.titleLabel, obj.countLabel] = this._makeNotificationSource(source, box);

        box.visible = obj.visible && (source.unseenCount > 0);

        this._updateSourceBoxStyle(source, obj, box);
    }

    _wakeUpScreenForSource(source) {
        if (!this._settings.get_boolean('show-banners'))
            return;
        const obj = this._sources.get(source);
        if (obj?.sourceBox.visible)
            this.emit('wake-up-screen');
    }

    _addPlayer(player) {
        const message = new MediaMessage(player);
        this._players.set(player, message);
        this._notificationBox.insert_child_at_index(message, 0);
        this._updateVisibility();
    }

    _removePlayer(player) {
        const message = this._players.get(player);
        this._players.delete(player);
        message.destroy();
        this._updateVisibility();
    }

    _sourceAdded(tray, source, initial) {
        const obj = {
            visible: source.policy.showInLockScreen,
            detailed: this._shouldShowDetails(source),
            sourceBox: null,
            titleLabel: null,
            countLabel: null,
            hasCriticalNotification: false,
        };

        obj.sourceBox = new St.BoxLayout({
            style_class: 'unlock-dialog-notification-source',
            x_expand: true,
        });
        this._showSource(source, obj, obj.sourceBox);
        this._notificationBox.insert_child_at_index(obj.sourceBox, this._players.size);

        source.connectObject(
            'notify::count', () => this._countChanged(source, obj),
            'notify::title', () => this._titleChanged(source, obj),
            'destroy', () => {
                this._removeSource(source, obj);
                this._updateVisibility();
            }, this);
        obj.policyChangedId = source.policy.connect('notify', (policy, pspec) => {
            if (pspec.name === 'show-in-lock-screen')
                this._visibleChanged(source, obj);
            else
                this._detailedChanged(source, obj);
        });

        this._sources.set(source, obj);

        if (!initial) {
            // block scrollbars while animating, if they're not needed now
            const boxHeight = this._notificationBox.height;
            if (this._scrollView.height >= boxHeight)
                this._scrollView.vscrollbar_policy = St.PolicyType.NEVER;

            const widget = obj.sourceBox;
            const [, natHeight] = widget.get_preferred_height(-1);
            widget.height = 0;
            widget.ease({
                height: natHeight,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: 250,
                onComplete: () => {
                    this._scrollView.vscrollbar_policy = St.PolicyType.AUTOMATIC;
                    widget.set_height(-1);
                },
            });

            this._updateVisibility();
            this._wakeUpScreenForSource(source);
        }
    }

    _titleChanged(source, obj) {
        obj.titleLabel.text = source.title;
    }

    _countChanged(source, obj) {
        // A change in the number of notifications may change whether we show
        // details.
        const newDetailed = this._shouldShowDetails(source);
        const oldDetailed = obj.detailed;

        obj.detailed = newDetailed;

        if (obj.detailed || oldDetailed !== newDetailed) {
            // A new notification was pushed, or a previous notification was destroyed.
            // Give up, and build the list again.

            obj.sourceBox.destroy_all_children();
            obj.titleLabel = obj.countLabel = null;
            this._showSource(source, obj, obj.sourceBox);
        } else {
            const count = source.unseenCount;
            obj.countLabel.text = `${count}`;
            obj.countLabel.visible = count > 1;
        }

        obj.sourceBox.visible = obj.visible && (source.unseenCount > 0);

        this._updateVisibility();
        this._wakeUpScreenForSource(source);
    }

    _visibleChanged(source, obj) {
        if (obj.visible === source.policy.showInLockScreen)
            return;

        obj.visible = source.policy.showInLockScreen;
        obj.sourceBox.visible = obj.visible && source.unseenCount > 0;

        this._updateVisibility();
        this._wakeUpScreenForSource(source);
    }

    _detailedChanged(source, obj) {
        const newDetailed = this._shouldShowDetails(source);
        if (obj.detailed === newDetailed)
            return;

        obj.detailed = newDetailed;

        obj.sourceBox.destroy_all_children();
        obj.titleLabel = obj.countLabel = null;
        this._showSource(source, obj, obj.sourceBox);
    }

    _removeSource(source, obj) {
        obj.sourceBox.destroy();
        obj.sourceBox = obj.titleLabel = obj.countLabel = null;

        source.policy.disconnect(obj.policyChangedId);

        this._sources.delete(source);
    }
});

const Clock = GObject.registerClass(
class UnlockDialogClock extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'unlock-dialog-clock',
            orientation: Clutter.Orientation.VERTICAL,
        });

        this._time = new St.Label({
            style_class: 'unlock-dialog-clock-time',
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._date = new St.Label({
            style_class: 'unlock-dialog-clock-date',
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._hint = new St.Label({
            style_class: 'unlock-dialog-clock-hint',
            x_align: Clutter.ActorAlign.CENTER,
            opacity: 0,
        });

        this.add_child(this._time);
        this.add_child(this._date);
        this.add_child(this._hint);

        this._wallClock = new GnomeDesktop.WallClock({time_only: true});
        this._wallClock.connect('notify::clock', this._updateClock.bind(this));

        const backend = this.get_context().get_backend();
        this._seat = backend.get_default_seat();
        this._seat.connectObject('notify::touch-mode',
            this._updateHint.bind(this), this);

        this._monitorManager = global.backend.get_monitor_manager();
        this._monitorManager.connectObject('power-save-mode-changed',
            () => (this._hint.opacity = 0), this);

        this._idleMonitor = global.backend.get_core_idle_monitor();
        this._idleWatchId = this._idleMonitor.add_idle_watch(HINT_TIMEOUT * 1000, () => {
            this._hint.ease({
                opacity: 255,
                duration: CROSSFADE_TIME,
            });
        });

        this._updateClock();
        this._updateHint();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _updateClock() {
        this._time.text = this._wallClock.clock.trim();

        const date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        const dateFormat = Shell.util_translate_time_string(N_('%A %B %-d'));
        this._date.text = formatDateWithCFormatString(date, dateFormat);
    }

    _updateHint() {
        const authMechanism = this._selectedAuthMechanism;
        let text;

        if (authMechanism?.role === GdmConst.SMARTCARD_ROLE_NAME)
            text = _('Insert smartcard');
        else if (this._seat.touch_mode)
            text = _('Swipe up');
        else
            text = _('Click or press a key');

        this._hint.text = text;
    }

    _onDestroy() {
        this._wallClock.run_dispose();

        this._idleMonitor.remove_watch(this._idleWatchId);
    }
});

const UnlockDialogLayout = GObject.registerClass(
class UnlockDialogLayout extends Clutter.LayoutManager {
    _init(stack, notifications, authIndicatorButton, bottomButtonGroup) {
        super._init();

        this._stack = stack;
        this._notifications = notifications;
        this._authIndicatorButton = authIndicatorButton;
        this._bottomButtonGroup = bottomButtonGroup;
    }

    vfunc_get_preferred_width(container, forHeight) {
        return this._stack.get_preferred_width(forHeight);
    }

    vfunc_get_preferred_height(container, forWidth) {
        return this._stack.get_preferred_height(forWidth);
    }

    vfunc_allocate(container, box) {
        const [width, height] = box.get_size();

        const tenthOfHeight = height / 10.0;
        const centerY = height / 2.0;

        const [, , stackWidth, stackHeight] =
            this._stack.get_preferred_size();

        const [, , notificationsWidth, notificationsHeight] =
            this._notifications.get_preferred_size();

        const columnWidth = Math.max(stackWidth, notificationsWidth);

        const columnX1 = Math.floor((width - columnWidth) / 2.0);
        const actorBox = new Clutter.ActorBox();

        // Notifications
        const maxNotificationsHeight = Math.min(
            notificationsHeight,
            height - tenthOfHeight - stackHeight);

        actorBox.x1 = columnX1;
        actorBox.y1 = height - maxNotificationsHeight;
        actorBox.x2 = columnX1 + columnWidth;
        actorBox.y2 = actorBox.y1 + maxNotificationsHeight;

        this._notifications.allocate(actorBox);

        // Authentication Box
        const dialog = container.get_parent();
        let stackY;
        if (dialog._activePage === dialog._clock) {
            stackY = Math.min(
                Math.floor(centerY - stackHeight / 2.0),
                height - stackHeight - maxNotificationsHeight);
        } else {
            stackY = Math.min(
                Math.floor(centerY - FIXED_PROMPT_HEIGHT / 2.0),
                height - stackHeight - maxNotificationsHeight);
        }

        actorBox.x1 = columnX1;
        actorBox.y1 = stackY;
        actorBox.x2 = columnX1 + columnWidth;
        actorBox.y2 = stackY + stackHeight;

        this._stack.allocate(actorBox);

        // Auth Indicator button (left bottom)
        if (this._authIndicatorButton.visible) {
            const [, , natWidth, natHeight] =
                this._authIndicatorButton.get_preferred_size();

            const textDirection = this._authIndicatorButton.get_text_direction();
            if (textDirection === Clutter.TextDirection.RTL)
                actorBox.x1 = box.x2 - natWidth;
            else
                actorBox.x1 = box.x1;

            actorBox.y1 = box.y2 - natHeight;
            actorBox.x2 = actorBox.x1 + natWidth;
            actorBox.y2 = actorBox.y1 + natHeight;

            this._authIndicatorButton.allocate(actorBox);
        }

        // bottom button group, (has login options and switch user buttons) (right bottom)
        if (this._bottomButtonGroup.visible) {
            const [, , natWidth, natHeight] =
                this._bottomButtonGroup.get_preferred_size();

            const textDirection = this._bottomButtonGroup.get_text_direction();
            if (textDirection === Clutter.TextDirection.RTL)
                actorBox.x1 = box.x1;
            else
                actorBox.x1 = box.x2 - natWidth;

            actorBox.y1 = box.y2 - natHeight;
            actorBox.x2 = actorBox.x1 + natWidth;
            actorBox.y2 = actorBox.y1 + natHeight;

            this._bottomButtonGroup.allocate(actorBox);
        }
    }
});

export const UnlockDialog = GObject.registerClass({
    Signals: {
        'failed': {},
        'wake-up-screen': {},
    },
}, class UnlockDialog extends St.Widget {
    _init(parentActor) {
        super._init({
            accessible_role: Atk.Role.WINDOW,
            style_class: 'unlock-dialog',
            visible: false,
            reactive: true,
        });

        parentActor.add_child(this);

        this._gdmClient = new Gdm.Client();

        try {
            this._gdmClient.set_enabled_extensions([
                Gdm.UserVerifierChoiceList.interface_info().name,
            ]);
        } catch {
        }

        this._adjustment = new St.Adjustment({
            actor: this,
            lower: 0,
            upper: 2,
            page_size: 1,
            page_increment: 1,
        });
        this._adjustment.connect('notify::value', () => {
            this._setTransitionProgress(this._adjustment.value);
        });

        this._swipeTracker = new SwipeTracker.SwipeTracker(this,
            Clutter.Orientation.VERTICAL,
            Shell.ActionMode.UNLOCK_SCREEN,
            {
                name: 'UnlockDialog swipe tracker',
            });
        this._swipeTracker.connect('begin', this._swipeBegin.bind(this));
        this._swipeTracker.connect('update', this._swipeUpdate.bind(this));
        this._swipeTracker.connect('end', this._swipeEnd.bind(this));

        this.connect('scroll-event', (o, event) => {
            if (this._swipeTracker.canHandleScrollEvent(event))
                return Clutter.EVENT_PROPAGATE;

            const direction = event.get_scroll_direction();
            if (direction === Clutter.ScrollDirection.UP)
                this._showClock();
            else if (direction === Clutter.ScrollDirection.DOWN)
                this._showPrompt();
            return Clutter.EVENT_STOP;
        });

        this._activePage = null;

        const clickGesture = new Clutter.ClickGesture();
        clickGesture.connect('recognize', () => this._showPrompt());
        this.add_action(clickGesture);

        // Background
        this._backgroundGroup = new Clutter.Actor();
        this.add_child(this._backgroundGroup);

        this._bgManagers = [];

        const themeContext = St.ThemeContext.get_for_stage(global.stage);
        themeContext.connectObject('notify::scale-factor',
            () => this._updateBackgroundEffects(), this);

        this._updateBackgrounds();
        Main.layoutManager.connectObject('monitors-changed',
            this._updateBackgrounds.bind(this), this);

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        // Authentication & Clock stack
        this._stack = new Shell.Stack();

        this._promptBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
        });
        this._promptBox.set_pivot_point(0.5, 0.5);
        this._promptBox.hide();
        this._stack.add_child(this._promptBox);

        this._clock = new Clock();
        this._clock.set_pivot_point(0.5, 0.5);
        this._stack.add_child(this._clock);
        this._showClock();

        this.allowCancel = false;

        Main.ctrlAltTabManager.addGroup(this, _('Unlock Window'), 'dialog-password-symbolic');

        // Notifications
        this._notificationsBox = new NotificationsBox();
        this._notificationsBox.connect('wake-up-screen', () => this.emit('wake-up-screen'));

        this._bottomButtonGroup = new St.BoxLayout({
            style_class: 'login-dialog-bottom-button-group',
        });

        // Switch User button
        this._otherUserButton = new St.Button({
            style_class: 'login-dialog-button switch-user-button',
            accessible_name: _('Log in as another user'),
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: false,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.END,
            label: _('Switch User…'),
        });
        this._otherUserButton.set_pivot_point(0.5, 0.5);
        this._otherUserButton.connect('clicked', this._otherUserClicked.bind(this));
        this._bottomButtonGroup.add_child(this._otherUserButton);

        // Login Options button
        this._authMenuButton = new AuthMenuButton.AuthMenuButton({
            title: _('Login Options'),
            iconName: 'cog-wheel-symbolic',
        });
        this._authMenuButton.connect('active-item-changed', () => {
            const authMechanism = this._authMenuButton.getActiveItem();
            if (!authMechanism)
                return;

            this._selectAuthMechanism(authMechanism);
            this._authMenuButton.close();
        });
        this._authMenuButton.updateSensitivity(true);
        this._bottomButtonGroup.add_child(this._authMenuButton);

        // Auth Indicators
        this._authIndicatorButton = new AuthMenuButton.AuthMenuButtonIndicator({
            title: _('Background Authentication Methods'),
            animateVisibility: true,
        });
        this._authIndicatorButton.add_style_class_name('login-dialog-bottom-button-group');
        this._authIndicatorButton.set_pivot_point(0.5, 0.5);
        this._authIndicatorButton.updateSensitivity(true);

        this._screenSaverSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.screensaver'});

        this._screenSaverSettings.connectObject('changed::user-switch-enabled',
            this._updateUserSwitchVisibility.bind(this), this);

        this._lockdownSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.lockdown'});
        this._lockdownSettings.connect('changed::disable-user-switching',
            this._updateUserSwitchVisibility.bind(this));

        this._user.connectObject(
            'notify::is-loaded', () => this._updateUserSwitchVisibility(),
            'notify::has-multiple-users', () => this._updateUserSwitchVisibility(),
            this);

        this._updateUserSwitchVisibility();

        // When parental controls session limits are enabled, the screen will be
        // locked upon reaching the time limit. In those cases, tweak the lock screen,
        // so that the children cannot unlock without parental supervision.
        Main.timeLimitsManager.connectObject(
            'notify::state', () => this._updateAuthBlocked(),
            this);
        this._updateAuthBlocked();

        // Main Box
        const mainBox = new St.Widget();
        mainBox.add_constraint(new Layout.MonitorConstraint({primary: true}));
        mainBox.add_child(this._stack);
        mainBox.add_child(this._notificationsBox);
        mainBox.add_child(this._authIndicatorButton);
        mainBox.add_child(this._bottomButtonGroup);
        mainBox.layout_manager = new UnlockDialogLayout(
            this._stack,
            this._notificationsBox,
            this._authIndicatorButton,
            this._bottomButtonGroup);
        this.add_child(mainBox);

        this._idleMonitor = global.backend.get_core_idle_monitor();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, this._escape.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_key_press_event(event) {
        if (this._activePage === this._promptBox ||
            (this._promptBox && this._promptBox.visible))
            return Clutter.EVENT_PROPAGATE;

        const keyval = event.get_key_symbol();
        if (keyval === Clutter.KEY_Shift_L ||
            keyval === Clutter.KEY_Shift_R ||
            keyval === Clutter.KEY_Shift_Lock ||
            keyval === Clutter.KEY_Caps_Lock)
            return Clutter.EVENT_PROPAGATE;

        const unichar = event.get_key_unicode();

        this._showPrompt();

        if (GLib.unichar_isprint(unichar))
            this._authPrompt.startPreemptiveInput(unichar);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_captured_event(event) {
        if (Main.keyboard.maybeHandleEvent(event))
            return Clutter.EVENT_STOP;

        return Clutter.EVENT_PROPAGATE;
    }

    _selectAuthMechanism(authMechanism) {
        const oldMechanism = this._selectedAuthMechanism;

        if (authMechanism === oldMechanism)
            return;

        if (!this._authPrompt.selectMechanism(authMechanism)) {
            this._authMenuButton.setActiveItem(oldMechanism);
            return;
        }

        this._selectedAuthMechanism = authMechanism;
    }

    _createBackground(monitorIndex) {
        const monitor = Main.layoutManager.monitors[monitorIndex];
        const widget = new St.Widget({
            style_class: 'screen-shield-background',
            x: monitor.x,
            y: monitor.y,
            width: monitor.width,
            height: monitor.height,
            effect: new Shell.BlurEffect({name: 'blur'}),
        });

        const bgManager = new Background.BackgroundManager({
            container: widget,
            monitorIndex,
            controlPosition: false,
        });

        this._bgManagers.push(bgManager);

        this._backgroundGroup.add_child(widget);
    }

    _updateBackgroundEffects() {
        const themeContext = St.ThemeContext.get_for_stage(global.stage);

        for (const widget of this._backgroundGroup) {
            const effect = widget.get_effect('blur');

            if (effect) {
                effect.set({
                    brightness: BLUR_BRIGHTNESS,
                    radius: BLUR_RADIUS * themeContext.scale_factor,
                });
            }
        }
    }

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];
        this._backgroundGroup.destroy_all_children();

        for (let i = 0; i < Main.layoutManager.monitors.length; i++)
            this._createBackground(i);
        this._updateBackgroundEffects();
    }

    _ensureAuthPrompt() {
        if (!this._authPrompt) {
            this._authPrompt = new AuthPrompt.AuthPrompt(this._gdmClient,
                AuthPrompt.AuthPromptMode.UNLOCK_ONLY);
            this._authPrompt.connect('failed', this._fail.bind(this));
            this._authPrompt.connect('cancelled', this._fail.bind(this));
            this._authPrompt.connect('reset', this._onReset.bind(this));
            this._authPrompt.connect('loading', this._onLoading.bind(this));
            this._authPrompt.connect('mechanisms-changed', this._onMechanismsChanged.bind(this));
            this._promptBox.add_child(this._authPrompt);
        }

        const {verificationStatus} = this._authPrompt;
        switch (verificationStatus) {
        case AuthPromptStatus.NOT_VERIFYING:
        case AuthPromptStatus.VERIFICATION_CANCELLED:
        case AuthPromptStatus.VERIFICATION_FAILED:
            this._authPrompt.reset();
            this._authPrompt.updateSensitivity(
                {sensitive: verificationStatus === AuthPromptStatus.NOT_VERIFYING});
        }

        this._updateAuthBlocked();
    }

    _maybeDestroyAuthPrompt() {
        const focus = global.stage.key_focus;
        if (focus === null ||
            (this._authPrompt && this._authPrompt.contains(focus)) ||
            (this._otherUserButton && focus === this._otherUserButton))
            this.grab_key_focus();

        if (this._authPrompt) {
            this._authPrompt.destroy();
            this._authPrompt = null;
        }
    }

    _showClock() {
        if (this._activePage === this._clock)
            return;

        this._activePage = this._clock;

        this._adjustment.ease(0, {
            duration: CROSSFADE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._maybeDestroyAuthPrompt(),
        });
    }

    _showPrompt() {
        this._ensureAuthPrompt();

        if (this._activePage === this._promptBox)
            return;

        this._activePage = this._promptBox;

        this._adjustment.ease(1, {
            duration: CROSSFADE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _setTransitionProgress(progress) {
        this._promptBox.visible = progress > 0;
        this._clock.visible = progress < 1;

        this._otherUserButton.set({
            reactive: progress > 0,
            can_focus: progress > 0,
        });
        this._authIndicatorButton.set({
            opacity: 255 * progress,
            scale_x: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
            scale_y: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
        });
        this._updateUserSwitchVisibility();

        const {scaleFactor} = St.ThemeContext.get_for_stage(global.stage);

        this._promptBox.set({
            opacity: 255 * progress,
            scale_x: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
            scale_y: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
            translation_y: FADE_OUT_TRANSLATION * (1 - progress) * scaleFactor,
        });

        this._clock.set({
            opacity: 255 * (1 - progress),
            scale_x: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * (1 - progress),
            scale_y: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * (1 - progress),
            translation_y: -FADE_OUT_TRANSLATION * progress * scaleFactor,
        });

        this._bottomButtonGroup.set({
            opacity: 255 * progress,
            scale_x: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
            scale_y: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
        });
    }

    _fail() {
        this._showClock();
        this.emit('failed');
    }

    _onReset(authPrompt, resetType) {
        let userName;
        if (resetType !== AuthPrompt.ResetType.DONT_PROVIDE_USERNAME) {
            this._authPrompt.setUser(this._user);
            userName = this._userName;
        } else {
            userName = null;
        }

        this._authPrompt.begin({userName});
    }

    _onLoading(_authPrompt, isLoading) {
        this._authMenuButton.updateReactive(!isLoading);
    }

    _onMechanismsChanged(_authPrompt, mechanisms, selectedMechanism) {
        this._authMenuButton.clearItems({
            sectionName: PRIMARY_UNLOCK_METHOD_SECTION_NAME,
        });

        this._authIndicatorButton.clearItems();

        if (mechanisms.length === 0)
            return;

        for (const m of mechanisms) {
            if (GdmUtil.isSelectable(m)) {
                this._authMenuButton.addItem({
                    sectionName: PRIMARY_UNLOCK_METHOD_SECTION_NAME,
                    ...m,
                });
            } else {
                this._authIndicatorButton.addItem({
                    iconName: GdmUtil.getIconName(m),
                    description: this._getUnlockDescription(m),
                    ...m,
                });
            }
        }

        if (Object.keys(selectedMechanism).length > 0)
            this._authMenuButton.setActiveItem(selectedMechanism);

        this._authIndicatorButton.updateDescriptionLabel();
    }

    _getUnlockDescription(mechanism) {
        // This is only used for non selectable mechanisms.
        // Currently only fingerprint is non selectable
        switch (mechanism.role) {
        case GdmConst.FINGERPRINT_ROLE_NAME:
            return _('Unlock with fingerprint');
        default:
            throw new Error(`Failed getting unlock description: ${mechanism.role}`);
        }
    }

    _escape() {
        if (this._authPrompt && this.allowCancel)
            this._authPrompt.cancel();
    }

    _swipeBegin(tracker, monitor) {
        if (monitor !== Main.layoutManager.primaryIndex)
            return;

        this._adjustment.remove_transition('value');

        this._ensureAuthPrompt();

        const progress = this._adjustment.value;
        tracker.confirmSwipe(this._stack.height,
            [0, 1],
            progress,
            Math.round(progress));
    }

    _swipeUpdate(tracker, progress) {
        this._adjustment.value = progress;
    }

    _swipeEnd(tracker, duration, endProgress) {
        this._activePage = endProgress
            ? this._promptBox
            : this._clock;

        this._adjustment.ease(endProgress, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration,
            onComplete: () => {
                if (this._activePage === this._clock)
                    this._maybeDestroyAuthPrompt();
            },
        });
    }

    _otherUserClicked() {
        this._authPrompt.connectObject('destroy', () =>
            Gdm.goto_login_session_sync(null));
        this._authPrompt.cancel();
    }

    _onDestroy() {
        this.popModal();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }

        if (this._gdmClient) {
            this._gdmClient = null;
            delete this._gdmClient;
        }
    }

    _updateUserSwitchVisibility() {
        this._otherUserButton.visible = this._userManager.can_switch() &&
            this._userManager.has_multiple_users &&
            this._screenSaverSettings.get_boolean('user-switch-enabled') &&
            !this._lockdownSettings.get_boolean('disable-user-switching') &&
            this._promptBox.visible;
    }

    _updateAuthBlocked() {
        this._authPrompt?.setAuthBlocked(
            Main.timeLimitsManager.state === TimeLimitsState.LIMIT_REACHED);
    }

    cancel() {
        if (this._authPrompt)
            this._authPrompt.cancel();
    }

    finish(onComplete) {
        if (!this._authPrompt) {
            onComplete();
            return;
        }

        this._authPrompt.finish(onComplete);
    }

    open() {
        this.show();

        if (this._isModal)
            return true;

        const grab = Main.pushModal(Main.uiGroup,
            {actionMode: Shell.ActionMode.UNLOCK_SCREEN});
        this._grab = grab;
        this._isModal = true;

        return true;
    }

    activate() {
        this._showPrompt();
    }

    popModal() {
        if (this._isModal) {
            Main.popModal(this._grab);
            this._grab = null;
            this._isModal = false;
        }
    }
});
