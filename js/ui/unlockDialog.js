// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported UnlockDialog */

const { AccountsService, Atk, Clutter, Gdm, Gio,
        GnomeDesktop, GLib, GObject, Meta, Shell, St } = imports.gi;

const Layout = imports.ui.layout;
const Main = imports.ui.main;

const AuthPrompt = imports.gdm.authPrompt;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

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

        this._promptBox = new St.BoxLayout({ vertical: true,
                                             x_align: Clutter.ActorAlign.CENTER,
                                             y_align: Clutter.ActorAlign.CENTER,
                                             x_expand: true,
                                             y_expand: true });
        this.add_child(this._promptBox);

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

        this.popModal();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }
    }

    cancel() {
        this._authPrompt.cancel();

        this.destroy();
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
