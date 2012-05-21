// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;

const GnomeSession = imports.misc.gnomeSession;
const Lightbox = imports.ui.lightbox;
const UnlockDialog = imports.ui.unlockDialog;
const Main = imports.ui.main;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';

/**
 * To test screen shield, make sure to kill gnome-screensaver.
 *
 * If you are setting org.gnome.desktop.session.idle-delay directly in dconf,
 * rather than through System Settings, you also need to set
 * org.gnome.settings-daemon.plugins.power.sleep-display-ac and
 * org.gnome.settings-daemon.plugins.power.sleep-display-battery to the same value.
 * This will ensure that the screen blanks at the right time when it fades out.
 * https://bugzilla.gnome.org/show_bug.cgi?id=668703 explains the dependance.
 */
const ScreenShield = new Lang.Class({
    Name: 'ScreenShield',

    _init: function() {
        this._presence = new GnomeSession.Presence(Lang.bind(this, function(proxy, error) {
            if (error) {
                logError(error, 'Error while reading gnome-session presence');
                return;
            }

            this._onStatusChanged(proxy.status);
        }));
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._onStatusChanged(status);
        }));

        this._settings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });

        this._isModal = false;
        this._isLocked = false;
        this._group = new St.Widget({ x: 0,
                                      y: 0 });
        Main.uiGroup.add_actor(this._group);
        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.POSITION | Clutter.BindCoordinate.SIZE });
        this._group.add_constraint(constraint);

        this._lightbox = new Lightbox.Lightbox(this._group,
                                               { inhibitEvents: true, fadeInTime: 10, fadeFactor: 1 });
        this._background = Meta.BackgroundActor.new_for_screen(global.screen);
        this._background.hide();
        Main.uiGroup.add_actor(this._background);
    },

    _onStatusChanged: function(status) {
        if (status == GnomeSession.PresenceStatus.IDLE) {
            if (this._dialog) {
                this._dialog.cancel();
                this._dialog = null;
            }

            this._group.reactive = true;
            if (!this._isModal) {
                Main.pushModal(this._group);
                this._isModal = true;
            }

            if (!this._isLocked)
                this._lightbox.show();
        } else {
            let lightboxWasShown = this._lightbox.shown;
            this._lightbox.hide();

            let shouldLock = lightboxWasShown && this._settings.get_boolean(LOCK_ENABLED_KEY);
            if (shouldLock || this._isLocked) {
                this.lock();
            } else if (this._isModal) {
                this._popModal();
            }
        }
    },

    _popModal: function() {
        this._group.reactive = false;
        Main.popModal(this._group);

        this._background.hide();

        this._isModal = false;
        this._isLocked = false;

        this.emit('lock-status-changed', false);
    },

    _showUnlockDialog: function() {
        if (this._dialog)
            return;

        this._dialog = new UnlockDialog.UnlockDialog();
        this._dialog.connect('failed', Lang.bind(this, this._onUnlockFailed));
        this._dialog.connect('unlocked', Lang.bind(this, this._onUnlockSucceded));

        if (!this._dialog.open(global.get_current_time())) {
            log('Could not open unlock dialog: failed to acquire grab');

            // and then? best we can do is to autounlock, although that's potentially
            // a security issue
            this._onUnlockSucceded();
        }

        this._dialog._group.raise_top();
    },

    _onUnlockFailed: function() {
        // for now, on failure we just destroy the dialog and create a new
        // one (this is what gnome-screensaver does)
        // in the future, we may want to go back to the lock screen instead

        this._dialog.destroy();
        this._dialog = null;

        this._showUnlockDialog();
    },

    _onUnlockSucceded: function() {
        this._dialog.destroy();
        this._dialog = null;

        this._popModal();
    },

    get locked() {
        return this._isLocked;
    },

    lock: function() {
        if (!this._isModal) {
            Main.pushModal(this.actor);
            this._isModal = true;
        }

        let wasLocked = this._isLocked;
        this._isLocked = true;
        this.actor.show();

        this._showUnlockDialog();

        if (!wasLocked)
            this.emit('lock-status-changed', true);
    }
});
Signals.addSignalMethods(ScreenShield.prototype);
