// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported UnlockDialog */

const {
    AccountsService, Atk, Clutter, Gdm, Gio,
    GnomeDesktop, GLib, GObject, Shell, St,
} = imports.gi;

const Background = imports.ui.background;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const SwipeTracker = imports.ui.swipeTracker;

const AuthPrompt = imports.gdm.authPrompt;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

// The timeout before showing the unlock hint (in seconds)
const HINT_TIMEOUT = 4;

const CROSSFADE_TIME = 300;
const FADE_OUT_TRANSLATION = 200;
const FADE_OUT_SCALE = 0.3;

const BLUR_BRIGHTNESS = 0.65;
const BLUR_SIGMA = 45;

const SUMMARY_ICON_SIZE = 32;

var NotificationsBox = GObject.registerClass({
    Signals: { 'wake-up-screen': {} },
}, class NotificationsBox extends St.BoxLayout {
    _init() {
        super._init({
            vertical: true,
            name: 'unlockDialogNotifications',
        });

        this._scrollView = new St.ScrollView({ hscrollbar_policy: St.PolicyType.NEVER });
        this._notificationBox = new St.BoxLayout({
            vertical: true,
            style_class: 'unlock-dialog-notifications-container',
        });
        this._scrollView.add_actor(this._notificationBox);

        this.add_child(this._scrollView);

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
        let items = this._sources.entries();
        for (let [source, obj] of items)
            this._removeSource(source, obj);
    }

    _updateVisibility() {
        this._notificationBox.visible =
            this._notificationBox.get_children().some(a => a.visible);

        this.visible = this._notificationBox.visible;
    }

    _makeNotificationSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add_child(sourceActor);

        let textBox = new St.BoxLayout({
            x_expand: true,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(textBox);

        let title = new St.Label({
            text: source.title,
            style_class: 'unlock-dialog-notification-label',
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
        });
        textBox.add(title);

        let count = source.unseenCount;
        let countLabel = new St.Label({
            text: `${count}`,
            visible: count > 1,
            style_class: 'unlock-dialog-notification-count-text',
        });
        textBox.add(countLabel);

        box.visible = count !== 0;
        return [title, countLabel];
    }

    _makeNotificationDetailedSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        let sourceBin = new St.Bin({ child: sourceActor });
        box.add(sourceBin);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add_child(textBox);

        let title = new St.Label({
            text: source.title.replace(/\n/g, ' '),
            style_class: 'unlock-dialog-notification-label',
        });
        textBox.add(title);

        let visible = false;
        for (let i = 0; i < source.notifications.length; i++) {
            let n = source.notifications[i];

            if (n.acknowledged)
                continue;

            let body = '';
            if (n.bannerBodyText) {
                const bodyText = n.bannerBodyText.replace(/\n/g, ' ');
                body = n.bannerBodyMarkup
                    ? bodyText
                    : GLib.markup_escape_text(bodyText, -1);
            }

            let label = new St.Label({ style_class: 'unlock-dialog-notification-count-text' });
            label.clutter_text.set_markup(`<b>${n.title}</b> ${body}`);
            textBox.add(label);

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
        let hasCriticalNotification =
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

    _sourceAdded(tray, source, initial) {
        let obj = {
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
        this._notificationBox.add_child(obj.sourceBox);

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
            let boxHeight = this._notificationBox.height;
            if (this._scrollView.height >= boxHeight)
                this._scrollView.vscrollbar_policy = St.PolicyType.NEVER;

            let widget = obj.sourceBox;
            let [, natHeight] = widget.get_preferred_height(-1);
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
        let newDetailed = this._shouldShowDetails(source);
        let oldDetailed = obj.detailed;

        obj.detailed = newDetailed;

        if (obj.detailed || oldDetailed !== newDetailed) {
            // A new notification was pushed, or a previous notification was destroyed.
            // Give up, and build the list again.

            obj.sourceBox.destroy_all_children();
            obj.titleLabel = obj.countLabel = null;
            this._showSource(source, obj, obj.sourceBox);
        } else {
            let count = source.unseenCount;
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
        let newDetailed = this._shouldShowDetails(source);
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

var Clock = GObject.registerClass(
class UnlockDialogClock extends St.BoxLayout {
    _init() {
        super._init({ style_class: 'unlock-dialog-clock', vertical: true });

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

        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', this._updateClock.bind(this));

        this._seat = Clutter.get_default_backend().get_default_seat();
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
        this._time.text = this._wallClock.clock;

        let date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        let dateFormat = Shell.util_translate_time_string(N_('%A %B %-d'));
        this._date.text = date.toLocaleFormat(dateFormat);
    }

    _updateHint() {
        this._hint.text = this._seat.touch_mode
            ? _('Swipe up to unlock')
            : _('Click or press a key to unlock');
    }

    _onDestroy() {
        this._wallClock.run_dispose();

        this._idleMonitor.remove_watch(this._idleWatchId);
    }
});

var UnlockDialogLayout = GObject.registerClass(
class UnlockDialogLayout extends Clutter.LayoutManager {
    _init(stack, notifications, switchUserButton) {
        super._init();

        this._stack = stack;
        this._notifications = notifications;
        this._switchUserButton = switchUserButton;
    }

    vfunc_get_preferred_width(container, forHeight) {
        return this._stack.get_preferred_width(forHeight);
    }

    vfunc_get_preferred_height(container, forWidth) {
        return this._stack.get_preferred_height(forWidth);
    }

    vfunc_allocate(container, box) {
        let [width, height] = box.get_size();

        let tenthOfHeight = height / 10.0;
        let thirdOfHeight = height / 3.0;

        let [, , stackWidth, stackHeight] =
            this._stack.get_preferred_size();

        let [, , notificationsWidth, notificationsHeight] =
            this._notifications.get_preferred_size();

        let columnWidth = Math.max(stackWidth, notificationsWidth);

        let columnX1 = Math.floor((width - columnWidth) / 2.0);
        let actorBox = new Clutter.ActorBox();

        // Notifications
        let maxNotificationsHeight = Math.min(
            notificationsHeight,
            height - tenthOfHeight - stackHeight);

        actorBox.x1 = columnX1;
        actorBox.y1 = height - maxNotificationsHeight;
        actorBox.x2 = columnX1 + columnWidth;
        actorBox.y2 = actorBox.y1 + maxNotificationsHeight;

        this._notifications.allocate(actorBox);

        // Authentication Box
        let stackY = Math.min(
            thirdOfHeight,
            height - stackHeight - maxNotificationsHeight);

        actorBox.x1 = columnX1;
        actorBox.y1 = stackY;
        actorBox.x2 = columnX1 + columnWidth;
        actorBox.y2 = stackY + stackHeight;

        this._stack.allocate(actorBox);

        // Switch User button
        if (this._switchUserButton.visible) {
            let [, , natWidth, natHeight] =
                this._switchUserButton.get_preferred_size();

            const textDirection = this._switchUserButton.get_text_direction();
            if (textDirection === Clutter.TextDirection.RTL)
                actorBox.x1 = box.x1 + natWidth;
            else
                actorBox.x1 = box.x2 - (natWidth * 2);

            actorBox.y1 = box.y2 - (natHeight * 2);
            actorBox.x2 = actorBox.x1 + natWidth;
            actorBox.y2 = actorBox.y1 + natHeight;

            this._switchUserButton.allocate(actorBox);
        }
    }
});

var UnlockDialog = GObject.registerClass({
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
        } catch (e) {
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
            Shell.ActionMode.UNLOCK_SCREEN);
        this._swipeTracker.connect('begin', this._swipeBegin.bind(this));
        this._swipeTracker.connect('update', this._swipeUpdate.bind(this));
        this._swipeTracker.connect('end', this._swipeEnd.bind(this));

        this.connect('scroll-event', (o, event) => {
            if (this._swipeTracker.canHandleScrollEvent(event))
                return Clutter.EVENT_PROPAGATE;

            let direction = event.get_scroll_direction();
            if (direction === Clutter.ScrollDirection.UP)
                this._showClock();
            else if (direction === Clutter.ScrollDirection.DOWN)
                this._showPrompt();
            return Clutter.EVENT_STOP;
        });

        this._activePage = null;

        let tapAction = new Clutter.TapAction();
        tapAction.connect('tap', this._showPrompt.bind(this));
        this.add_action(tapAction);

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

        this._promptBox = new St.BoxLayout({ vertical: true });
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

        // Switch User button
        this._otherUserButton = new St.Button({
            style_class: 'login-dialog-button switch-user-button',
            accessible_name: _('Log in as another user'),
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: false,
            opacity: 0,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.END,
            icon_name: 'system-users-symbolic',
        });
        this._otherUserButton.set_pivot_point(0.5, 0.5);
        this._otherUserButton.connect('clicked', this._otherUserClicked.bind(this));

        this._screenSaverSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.screensaver' });

        this._screenSaverSettings.connectObject('changed::user-switch-enabled',
            this._updateUserSwitchVisibility.bind(this), this);

        this._lockdownSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });
        this._lockdownSettings.connect('changed::disable-user-switching',
            this._updateUserSwitchVisibility.bind(this));

        this._user.connectObject('notify::is-loaded',
            this._updateUserSwitchVisibility.bind(this), this);

        this._updateUserSwitchVisibility();

        // Main Box
        let mainBox = new St.Widget();
        mainBox.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        mainBox.add_child(this._stack);
        mainBox.add_child(this._notificationsBox);
        mainBox.add_child(this._otherUserButton);
        mainBox.layout_manager = new UnlockDialogLayout(
            this._stack,
            this._notificationsBox,
            this._otherUserButton);
        this.add_child(mainBox);

        this._idleMonitor = global.backend.get_core_idle_monitor();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, this._escape.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_key_press_event(keyEvent) {
        if (this._activePage === this._promptBox ||
            (this._promptBox && this._promptBox.visible))
            return Clutter.EVENT_PROPAGATE;

        const { keyval } = keyEvent;
        if (keyval === Clutter.KEY_Shift_L ||
            keyval === Clutter.KEY_Shift_R ||
            keyval === Clutter.KEY_Shift_Lock ||
            keyval === Clutter.KEY_Caps_Lock)
            return Clutter.EVENT_PROPAGATE;

        let unichar = keyEvent.unicode_value;

        this._showPrompt();

        if (GLib.unichar_isgraph(unichar))
            this._authPrompt.addCharacter(unichar);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_captured_event(event) {
        if (Main.keyboard.maybeHandleEvent(event))
            return Clutter.EVENT_STOP;

        return Clutter.EVENT_PROPAGATE;
    }

    _createBackground(monitorIndex) {
        let monitor = Main.layoutManager.monitors[monitorIndex];
        let widget = new St.Widget({
            style_class: 'screen-shield-background',
            x: monitor.x,
            y: monitor.y,
            width: monitor.width,
            height: monitor.height,
            effect: new Shell.BlurEffect({ name: 'blur' }),
        });

        let bgManager = new Background.BackgroundManager({
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
                    sigma: BLUR_SIGMA * themeContext.scale_factor,
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
            this._promptBox.add_child(this._authPrompt);
        }

        this._authPrompt.reset();
        this._authPrompt.updateSensitivity(true);
    }

    _maybeDestroyAuthPrompt() {
        let focus = global.stage.key_focus;
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

        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);

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

        this._otherUserButton.set({
            opacity: 255 * progress,
            scale_x: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
            scale_y: FADE_OUT_SCALE + (1 - FADE_OUT_SCALE) * progress,
        });
    }

    _fail() {
        this._showClock();
        this.emit('failed');
    }

    _onReset(authPrompt, beginRequest) {
        let userName;
        if (beginRequest == AuthPrompt.BeginRequestType.PROVIDE_USERNAME) {
            this._authPrompt.setUser(this._user);
            userName = this._userName;
        } else {
            userName = null;
        }

        this._authPrompt.begin({ userName });
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

        let progress = this._adjustment.value;
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
        Gdm.goto_login_session_sync(null);

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
            this._screenSaverSettings.get_boolean('user-switch-enabled') &&
            !this._lockdownSettings.get_boolean('disable-user-switching');
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

    open(timestamp) {
        this.show();

        if (this._isModal)
            return true;

        let modalParams = {
            timestamp,
            actionMode: Shell.ActionMode.UNLOCK_SCREEN,
        };
        let grab = Main.pushModal(Main.uiGroup, modalParams);
        if (grab.get_seat_state() !== Clutter.GrabState.ALL) {
            Main.popModal(grab);
            return false;
        }

        this._grab = grab;
        this._isModal = true;

        return true;
    }

    activate() {
        this._showPrompt();
    }

    popModal(timestamp) {
        if (this._isModal) {
            Main.popModal(this._grab, timestamp);
            this._grab = null;
            this._isModal = false;
        }
    }
});
