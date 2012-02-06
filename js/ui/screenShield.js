// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;

const GnomeSession = imports.misc.gnomeSession;
const Lightbox = imports.ui.lightbox;
const LoginDialog = imports.gdm.loginDialog;
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

        this._group = new St.Widget({ x: 0,
                                      y: 0 });
        Main.uiGroup.add_actor(this._group);
        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.POSITION | Clutter.BindCoordinate.SIZE });
        this._group.add_constraint(constraint);
        this._group.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        this._group.connect('button-press-event', Lang.bind(this, this._onButtonPressEvent));
        this._lightbox = new Lightbox.Lightbox(this._group,
                                               { inhibitEvents: true, fadeInTime: 10, fadeFactor: 1 });
        this._background = Meta.BackgroundActor.new_for_screen(global.screen);
        this._background.hide();
        Main.uiGroup.add_actor(this._background);
    },

    _onStatusChanged: function(status) {
        log ("in _onStatusChanged");
        if (status == GnomeSession.PresenceStatus.IDLE) {
            log("session gone idle");
            this._group.reactive = true;
            Main.pushModal(this._group);
            this._lightbox.show();
        } else {
            let lightboxWasShown = this._lightbox.shown;
            log("this._lightbox.shown " + this._lightbox.shown);
            this._lightbox.hide();
            if (lightboxWasShown && this._settings.get_boolean(LOCK_ENABLED_KEY)) {
                this._background.show();
                this._background.raise_top();
            } else {
                this._popModal();
            }
        }
    },

    _popModal: function() {
        this._group.reactive = false;
        if (Main.isInModalStack(this._group))
            Main.popModal(this._group);
        this._background.hide();
    },

    _onKeyPressEvent: function(object, keyPressEvent) {
        log("in _onKeyPressEvent - lock is enabled: " + this._settings.get_boolean(LOCK_ENABLED_KEY));
        this._popModal();
    },

    _onButtonPressEvent: function(object, buttonPressEvent) {
        log("in _onButtonPressEvent - lock is enabled: " + this._settings.get_boolean(LOCK_ENABLED_KEY));
        this._popModal();
    },
});
