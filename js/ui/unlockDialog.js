// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported UnlockDialog */

const { AccountsService, Atk, Clutter, Gdm, Gio, GLib,
        GnomeDesktop, GObject, Meta, Shell, St } = imports.gi;

const Layout = imports.ui.layout;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Signals = imports.signals;

const AuthPrompt = imports.gdm.authPrompt;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

var SUMMARY_ICON_SIZE = 24;

var Clock = class {
    constructor() {
        this.actor = new St.BoxLayout({
            style_class: 'screen-shield-clock',
            vertical: true,
        });

        this._time = new St.Label({ style_class: 'screen-shield-clock-time' });
        this._date = new St.Label({ style_class: 'screen-shield-clock-date' });

        this.actor.add(this._time, { x_align: St.Align.MIDDLE });
        this.actor.add(this._date, { x_align: St.Align.MIDDLE });

        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', this._updateClock.bind(this));

        this._updateClock();
    }

    _updateClock() {
        this._time.text = this._wallClock.clock;

        let date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        let dateFormat = Shell.util_translate_time_string(N_("%A, %B %d"));
        this._date.text = date.toLocaleFormat(dateFormat);
    }

    destroy() {
        this.actor.destroy();
        this._wallClock.run_dispose();
    }
};

var NotificationsBox = class {
    constructor() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        name: 'screenShieldNotifications',
                                        style_class: 'screen-shield-notifications-container' });

        this._scrollView = new St.ScrollView({ x_fill: false, x_align: St.Align.START,
                                               hscrollbar_policy: St.PolicyType.NEVER });
        this._notificationBox = new St.BoxLayout({ vertical: true,
                                                   style_class: 'screen-shield-notifications-container' });
        this._scrollView.add_actor(this._notificationBox);

        this.actor.add(this._scrollView, { x_fill: true, x_align: St.Align.START });

        this._sources = new Map();
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source, true);
        });
        this._updateVisibility();

        this._sourceAddedId = Main.messageTray.connect('source-added', this._sourceAdded.bind(this));
    }

    destroy() {
        if (this._sourceAddedId) {
            Main.messageTray.disconnect(this._sourceAddedId);
            this._sourceAddedId = 0;
        }

        let items = this._sources.entries();
        for (let [source, obj] of items) {
            this._removeSource(source, obj);
        }

        this.actor.destroy();
    }

    _updateVisibility() {
        this._notificationBox.visible =
            this._notificationBox.get_children().some(a => a.visible);

        this.actor.visible = this._notificationBox.visible;
    }

    _makeNotificationSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add(sourceActor, { y_fill: true });

        let title = new St.Label({
            text: source.title,
            style_class: 'screen-shield-notification-label',
            x_expand: true,
            y_align: Clutter.ActorAlign.START,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(title);

        let count = source.unseenCount;
        let countLabel = new St.Label({
            text: '%d'.format(count),
            style_class: 'screen-shield-notification-count-text',
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(countLabel);

        box.visible = count != 0;
        return [title, countLabel];
    }

    _makeNotificationDetailedSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        let sourceBin = new St.Bin({ y_align: St.Align.START,
                                     x_align: St.Align.START,
                                     child: sourceActor });
        box.add(sourceBin);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: false, y_align: St.Align.START });

        let title = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
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

            let label = new St.Label({ style_class: 'screen-shield-notification-count-text' });
            label.clutter_text.set_markup('<b>' + n.title + '</b> ' + body);
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
        if (obj.detailed) {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationDetailedSource(source, box);
        } else {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationSource(source, box);
        }

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

        obj.sourceBox = new St.BoxLayout({ style_class: 'screen-shield-notification-source',
                                           x_expand: true });
        this._showSource(source, obj, obj.sourceBox);
        this._notificationBox.add(obj.sourceBox, { x_fill: false, x_align: St.Align.START });

        obj.sourceCountChangedId = source.connect('count-updated', source => {
            this._countChanged(source, obj);
        });
        obj.sourceTitleChangedId = source.connect('title-changed', source => {
            this._titleChanged(source, obj);
        });
        obj.policyChangedId = source.policy.connect('policy-changed', (policy, key) => {
            if (key == 'show-in-lock-screen')
                this._visibleChanged(source, obj);
            else
                this._detailedChanged(source, obj);
        });
        obj.sourceDestroyId = source.connect('destroy', source => {
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
                }
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
            obj.countLabel.text = '%d'.format(count);
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
};
Signals.addSignalMethods(NotificationsBox.prototype);

var UnlockDialog = GObject.registerClass({
    Signals: { 'failed': {} },
}, class UnlockDialog extends St.Widget {
    _init(parentActor) {
        super._init({
            accessible_role: Atk.Role.WINDOW,
            style_class: 'login-dialog',
            layout_manager: new Clutter.BoxLayout(),
            visible: false,
        });

        this.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        parentActor.add_child(this);

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._mainBox = new St.BoxLayout({
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
            vertical: true,
        });
        this.add_child(this._mainBox);

        this._clock = new Clock();
        this._mainBox.add_child(this._clock.actor);

        this._authPrompt = new AuthPrompt.AuthPrompt(new Gdm.Client(), AuthPrompt.AuthPromptMode.UNLOCK_ONLY);
        this._authPrompt.connect('failed', this._fail.bind(this));
        this._authPrompt.connect('cancelled', this._fail.bind(this));
        this._authPrompt.connect('reset', this._onReset.bind(this));
        this._authPrompt.setPasswordChar('\u25cf');
        this._authPrompt.nextButton.label = _("Unlock");

        this._mainBox.add_child(this._authPrompt.actor);

        this.allowCancel = false;

        let screenSaverSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.screensaver' });
        if (screenSaverSettings.get_boolean('user-switch-enabled')) {
            let otherUserLabel = new St.Label({ text: _("Log in as another user"),
                                                style_class: 'login-dialog-not-listed-label' });
            this._otherUserButton = new St.Button({ style_class: 'login-dialog-not-listed-button',
                                                    can_focus: true,
                                                    child: otherUserLabel,
                                                    reactive: true,
                                                    x_align: St.Align.START,
                                                    x_fill: false });
            this._otherUserButton.connect('clicked', this._otherUserClicked.bind(this));
            this._mainBox.add_child(this._otherUserButton);
        } else {
            this._otherUserButton = null;
        }

        this._notificationsBox = new NotificationsBox();
        this._wakeUpScreenId = this._notificationsBox.connect('wake-up-screen', this._wakeUpScreen.bind(this));
        this._mainBox.add_child(this._notificationsBox.actor);

        this._authPrompt.reset();
        this._updateSensitivity(true);

        Main.ctrlAltTabManager.addGroup(this, _("Unlock Window"), 'dialog-password-symbolic');

        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, this._escape.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
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

        this._authPrompt.begin({ userName: userName });
    }

    _escape() {
        if (this.allowCancel)
            this._authPrompt.cancel();
    }

    _otherUserClicked() {
        Gdm.goto_login_session_sync(null);

        this._authPrompt.cancel();
    }

    _wakeUpScreen() {
        // FIXME
        //this._onUserBecameActive();
        this.emit('wake-up-screen');
    }

    _onDestroy() {
        this.popModal();

        if (this._notificationsBox) {
            this._notificationsBox.disconnect(this._wakeUpScreenId);
            this._notificationsBox.destroy();
            this._notificationsBox = null;
        }

        this._clock.destroy();
        this._clock = null;

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
