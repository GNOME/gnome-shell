// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported UnlockDialog */

const { AccountsService, Atk, Clutter, Gdm, Gio,
        GnomeDesktop, GLib, GObject, Meta, Shell, St } = imports.gi;

const Background = imports.ui.background;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const AuthPrompt = imports.gdm.authPrompt;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';

const BLUR_BRIGHTNESS = 0.55;
const BLUR_RADIUS = 200;

const SUMMARY_ICON_SIZE = 48;

var NotificationsBox = GObject.registerClass({
    Signals: { 'wake-up-screen': {} },
}, class NotificationsBox extends St.BoxLayout {
    _init() {
        super._init({
            vertical: true,
            name: 'unlockDialogNotifications',
            style_class: 'unlock-dialog-notifications-container',
        });

        this._scrollView = new St.ScrollView({ hscrollbar_policy: St.PolicyType.NEVER });
        this._notificationBox = new St.BoxLayout({ vertical: true,
                                                   style_class: 'unlock-dialog-notifications-container' });
        this._scrollView.add_actor(this._notificationBox);

        this.add_child(this._scrollView);

        this._sources = new Map();
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source, true);
        });
        this._updateVisibility();

        this._sourceAddedId = Main.messageTray.connect('source-added', this._sourceAdded.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._sourceAddedId) {
            Main.messageTray.disconnect(this._sourceAddedId);
            this._sourceAddedId = 0;
        }

        let items = this._sources.entries();
        for (let [source, obj] of items)
            this._removeSource(source, obj);
    }

    _updateVisibility() {
        this._notificationBox.visible =
            this._notificationBox.get_children().some(a => a.visible);

        this.visible = this._notificationBox.visible;
    }

    _makeNotificationCountText(count, isChat) {
        if (isChat)
            return ngettext("%d new message", "%d new messages", count).format(count);
        else
            return ngettext("%d new notification", "%d new notifications", count).format(count);
    }

    _makeNotificationSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add_child(sourceActor);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add_child(textBox);

        let title = new St.Label({ text: source.title,
                                   style_class: 'unlock-dialog-notification-label' });
        textBox.add(title);

        let count = source.unseenCount;
        let countLabel = new St.Label({ text: this._makeNotificationCountText(count, source.isChat),
                                        style_class: 'unlock-dialog-notification-count-text' });
        textBox.add(countLabel);

        box.visible = count != 0;
        return [title, countLabel];
    }

    _makeNotificationDetailedSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        let sourceBin = new St.Bin({ child: sourceActor });
        box.add(sourceBin);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add_child(textBox);

        let title = new St.Label({ text: source.title,
                                   style_class: 'unlock-dialog-notification-label' });
        textBox.add(title);

        let visible = false;
        for (let i = 0; i < source.notifications.length; i++) {
            let n = source.notifications[i];

            if (n.acknowledged)
                continue;

            let body = '';
            if (n.bannerBodyText) {
                body = n.bannerBodyMarkup
                    ? n.bannerBodyText
                    : GLib.markup_escape_text(n.bannerBodyText, -1);
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
               source.narrowestPrivacyScope == MessageTray.PrivacyScope.SYSTEM;
    }

    _showSource(source, obj, box) {
        if (obj.detailed)
            [obj.titleLabel, obj.countLabel] = this._makeNotificationDetailedSource(source, box);
        else
            [obj.titleLabel, obj.countLabel] = this._makeNotificationSource(source, box);

        box.visible = obj.visible && (source.unseenCount > 0);
    }

    _sourceAdded(tray, source, initial) {
        let obj = {
            visible: source.policy.showInLockScreen,
            detailed: this._shouldShowDetails(source),
            sourceDestroyId: 0,
            sourceCountChangedId: 0,
            sourceTitleChangedId: 0,
            sourceUpdatedId: 0,
            sourceBox: null,
            titleLabel: null,
            countLabel: null,
        };

        obj.sourceBox = new St.BoxLayout({ style_class: 'unlock-dialog-notification-source',
                                           x_expand: true });
        this._showSource(source, obj, obj.sourceBox);
        this._notificationBox.add_child(obj.sourceBox);

        obj.sourceCountChangedId = source.connect('notify::count', () => {
            this._countChanged(source, obj);
        });
        obj.sourceTitleChangedId = source.connect('notify::title', () => {
            this._titleChanged(source, obj);
        });
        obj.policyChangedId = source.policy.connect('notify', (policy, pspec) => {
            if (pspec.name == 'show-in-lock-screen')
                this._visibleChanged(source, obj);
            else
                this._detailedChanged(source, obj);
        });
        obj.sourceDestroyId = source.connect('destroy', () => {
            this._onSourceDestroy(source, obj);
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
            if (obj.sourceBox.visible)
                this.emit('wake-up-screen');
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

        if (obj.detailed || oldDetailed != newDetailed) {
            // A new notification was pushed, or a previous notification was destroyed.
            // Give up, and build the list again.

            obj.sourceBox.destroy_all_children();
            obj.titleLabel = obj.countLabel = null;
            this._showSource(source, obj, obj.sourceBox);
        } else {
            let count = source.unseenCount;
            obj.countLabel.text = this._makeNotificationCountText(count, source.isChat);
        }

        obj.sourceBox.visible = obj.visible && (source.unseenCount > 0);

        this._updateVisibility();
        if (obj.sourceBox.visible)
            this.emit('wake-up-screen');
    }

    _visibleChanged(source, obj) {
        if (obj.visible == source.policy.showInLockScreen)
            return;

        obj.visible = source.policy.showInLockScreen;
        obj.sourceBox.visible = obj.visible && source.unseenCount > 0;

        this._updateVisibility();
        if (obj.sourceBox.visible)
            this.emit('wake-up-screen');
    }

    _detailedChanged(source, obj) {
        let newDetailed = this._shouldShowDetails(source);
        if (obj.detailed == newDetailed)
            return;

        obj.detailed = newDetailed;

        obj.sourceBox.destroy_all_children();
        obj.titleLabel = obj.countLabel = null;
        this._showSource(source, obj, obj.sourceBox);
    }

    _onSourceDestroy(source, obj) {
        this._removeSource(source, obj);
        this._updateVisibility();
    }

    _removeSource(source, obj) {
        obj.sourceBox.destroy();
        obj.sourceBox = obj.titleLabel = obj.countLabel = null;

        source.disconnect(obj.sourceDestroyId);
        source.disconnect(obj.sourceCountChangedId);
        source.disconnect(obj.sourceTitleChangedId);
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

        this.add_child(this._time);
        this.add_child(this._date);

        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', this._updateClock.bind(this));

        this._updateClock();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _updateClock() {
        this._time.text = this._wallClock.clock;

        let date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        let dateFormat = Shell.util_translate_time_string(N_("%A, %B %d"));
        this._date.text = date.toLocaleFormat(dateFormat);
    }

    _onDestroy() {
        this._wallClock.run_dispose();
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
            style_class: 'login-dialog',
            visible: false,
        });

        parentActor.add_child(this);

        // Background
        this._backgroundGroup = new Clutter.Actor();

        this.add_child(this._backgroundGroup);
        this.set_child_below_sibling(this._backgroundGroup, null);
        this._bgManagers = [];

        this._updateBackgrounds();
        Main.layoutManager.connect('monitors-changed', this._updateBackgrounds.bind(this));

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._promptBox = new St.BoxLayout({ vertical: true,
                                             x_align: Clutter.ActorAlign.CENTER,
                                             y_align: Clutter.ActorAlign.CENTER,
                                             x_expand: true,
                                             y_expand: true });
        this.add_child(this._promptBox);
        this._promptBox.add_constraint(new Layout.MonitorConstraint({ primary: true }));

        this._clock = new Clock();
        this._promptBox.add_child(this._clock);

        this._authPrompt = new AuthPrompt.AuthPrompt(new Gdm.Client(), AuthPrompt.AuthPromptMode.UNLOCK_ONLY);
        this._authPrompt.connect('failed', this._fail.bind(this));
        this._authPrompt.connect('cancelled', this._fail.bind(this));
        this._authPrompt.connect('reset', this._onReset.bind(this));
        this._authPrompt.setPasswordChar('\u25cf');
        this._authPrompt.nextButton.label = _("Unlock");

        this._promptBox.add_child(this._authPrompt);

        this.allowCancel = false;

        let screenSaverSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.screensaver' });
        if (screenSaverSettings.get_boolean('user-switch-enabled')) {
            let otherUserLabel = new St.Label({ text: _("Log in as another user"),
                                                style_class: 'login-dialog-not-listed-label' });
            this._otherUserButton = new St.Button({ style_class: 'login-dialog-not-listed-button',
                                                    can_focus: true,
                                                    child: otherUserLabel,
                                                    reactive: true });
            this._otherUserButton.connect('clicked', this._otherUserClicked.bind(this));
            this._promptBox.add_child(this._otherUserButton);
        } else {
            this._otherUserButton = null;
        }

        this._authPrompt.reset();
        this._updateSensitivity(true);

        Main.ctrlAltTabManager.addGroup(this, _("Unlock Window"), 'dialog-password-symbolic');

        // Notifications
        this._notificationsBox = new NotificationsBox();
        this._wakeUpScreenId = this._notificationsBox.connect('wake-up-screen', () => {
            this.emit('wake-up-screen');
        });
        this._promptBox.add_child(this._notificationsBox);

        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, this._escape.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _createBackground(monitorIndex) {
        let monitor = Main.layoutManager.monitors[monitorIndex];
        let widget = new St.Widget({ style_class: 'screen-shield-background',
                                     x: monitor.x,
                                     y: monitor.y,
                                     width: monitor.width,
                                     height: monitor.height });

        let bgManager = new Background.BackgroundManager({ container: widget,
                                                           monitorIndex,
                                                           controlPosition: false,
                                                           settingsSchema: SCREENSAVER_SCHEMA });

        this._bgManagers.push(bgManager);

        this._backgroundGroup.add_child(widget);

        widget.add_effect(new Shell.BlurEffect({
            brightness: BLUR_BRIGHTNESS,
            blur_radius: BLUR_RADIUS,
        }));
    }

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];
        this._backgroundGroup.destroy_all_children();

        for (let i = 0; i < Main.layoutManager.monitors.length; i++)
            this._createBackground(i);
    }

    _updateSensitivity(sensitive) {
        this._authPrompt.updateSensitivity(sensitive);

        if (this._otherUserButton) {
            this._otherUserButton.reactive = sensitive;
            this._otherUserButton.can_focus = sensitive;
        }
    }

    _fail() {
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
        if (this.allowCancel)
            this._authPrompt.cancel();
    }

    _otherUserClicked() {
        Gdm.goto_login_session_sync(null);

        this._authPrompt.cancel();
    }

    _onDestroy() {
        if (this._clock) {
            this._clock.destroy();
            this._clock = null;
        }

        if (this._notificationsBox) {
            this._notificationsBox.disconnect(this._wakeUpScreenId);
            this._notificationsBox.destroy();
            this._notificationsBox = null;
        }

        this.popModal();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }
    }

    cancel() {
        this._authPrompt.cancel();
    }

    addCharacter(unichar) {
        this._authPrompt.addCharacter(unichar);
    }

    finish(onComplete) {
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
        if (!Main.pushModal(this, modalParams))
            return false;

        this._isModal = true;

        return true;
    }

    popModal(timestamp) {
        if (this._isModal) {
            Main.popModal(this, timestamp);
            this._isModal = false;
        }
    }
});
