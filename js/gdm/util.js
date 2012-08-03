// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;

const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const PASSWORD_SERVICE_NAME = 'gdm-password';
const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
const FADE_ANIMATION_TIME = 0.16;

const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
const BANNER_MESSAGE_KEY = 'banner-message-enable';
const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';

const LOGO_KEY = 'logo';

function fadeInActor(actor) {
    if (actor.opacity == 255 && actor.visible)
        return null;

    let hold = new Batch.Hold();
    actor.show();
    let [minHeight, naturalHeight] = actor.get_preferred_height(-1);

    actor.opacity = 0;
    actor.set_height(0);
    Tweener.addTween(actor,
                     { opacity: 255,
                       height: naturalHeight,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.set_height(-1);
                           hold.release();
                       },
                     });

    return hold;
}

function fadeOutActor(actor) {
    if (!actor.visible || actor.opacity == 0) {
        actor.opacity = 0;
        actor.hide();
        return null;
    }

    let hold = new Batch.Hold();
    Tweener.addTween(actor,
                     { opacity: 0,
                       height: 0,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.hide();
                           this.set_height(-1);
                           hold.release();
                       },
                     });
    return hold;
}

const ShellUserVerifier = new Lang.Class({
    Name: 'ShellUserVerifier',

    _init: function(client, params) {
        params = Params.parse(params, { reauthenticationOnly: false });
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;

        this._settings = new Gio.Settings({ schema: LOGIN_SCREEN_SCHEMA });

        this._cancellable = new Gio.Cancellable();

        this._fprintManager = new Fprint.FprintManager();
        this._checkForFingerprintReader();
    },

    begin: function(userName, hold) {
        this._hold = hold;
        this._userName = userName;

        if (userName) {
            // If possible, reauthenticate an already running session,
            // so any session specific credentials get updated appropriately
            this._client.open_reauthentication_channel(userName, this._cancellable,
                                                       Lang.bind(this, this._reauthenticationChannelOpened));
        } else {
            this._client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
        }
    },

    cancel: function() {
        this._cancellable.cancel();

        if (this._userVerifier)
            this._userVerifier.call_cancel_sync(null);
    },

    clear: function() {
        this._cancellable.cancel();

        if (this._userVerifier) {
            this._userVerifier.run_dispose();
            this._userVerifier = null;
        }
    },

    answerQuery: function(serviceName, answer) {
        this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
    },

    _checkForFingerprintReader: function() {
        this._haveFingerprintReader = false;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY))
            return;

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable, Lang.bind(this,
            function(device, error) {
                if (!error && device)
                    this._haveFingerprintReader = true;
            }));
    },

    _reauthenticationChannelOpened: function(client, result) {
        try {
            this._userVerifier = client.open_reauthentication_channel_finish(result);
            this._connectSignals();
            this._beginVerification();

            this._hold.release();
        } catch (e) {
            if (this._reauthOnly) {
                logError(e, 'Failed to open reauthentication channel');

                this._hold.release();
                this.emit('verification-failed');

                return;
            }

            // If there's no session running, or it otherwise fails, then fall back
            // to performing verification from this login session
            client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
        }
    },

    _userVerifierGot: function(client, result) {
        this._userVerifier = client.get_user_verifier_finish(result);
        this._connectSignals();
        this._beginVerification();

        this._hold.release();
    },

    _connectSignals: function() {
        this._userVerifier.connect('info', Lang.bind(this, this._onInfo));
        this._userVerifier.connect('problem', Lang.bind(this, this._onProblem));
        this._userVerifier.connect('info-query', Lang.bind(this, this._onInfoQuery));
        this._userVerifier.connect('secret-info-query', Lang.bind(this, this._onSecretInfoQuery));
        this._userVerifier.connect('conversation-stopped', Lang.bind(this, this._onConversationStopped));
        this._userVerifier.connect('reset', Lang.bind(this, this._onReset));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
    },

    _beginVerification: function() {
        this._hold.acquire();

        if (this._userName) {
            this._userVerifier.call_begin_verification_for_user(PASSWORD_SERVICE_NAME,
                                                                this._userName,
                                                                this._cancellable,
                                                                Lang.bind(this, function(obj, result) {
                obj.call_begin_verification_for_user_finish(result);
                this._hold.release();
            }));

            if (this._haveFingerprintReader) {
                this._hold.acquire();

                this._userVerifier.call_begin_verification_for_user(FINGERPRINT_SERVICE_NAME,
                                                                    this._userName,
                                                                    this._cancellable,
                                                                    Lang.bind(this, function(obj, result) {
                    obj.call_begin_verification_for_user_finish(result);
                    this._hold.release();
                }));
            }
        } else {
            this._userVerifier.call_begin_verification(PASSWORD_SERVICE_NAME,
                                                       this._cancellable,
                                                       Lang.bind(this, function(obj, result) {
                obj.call_begin_verification_finish(result);
                this._hold.release();
            }));
        }
    },

    _onInfo: function(client, serviceName, info) {
        // We don't display fingerprint messages, because they
        // have words like UPEK in them. Instead we use the messages
        // as a cue to display our own message.
        if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader) {
            this.emit('show-fingerprint-prompt');
        } else if (serviceName == PASSWORD_SERVICE_NAME) {
            Main.notifyError(info);
        }
    },

    _onProblem: function(client, serviceName, problem) {
        // we don't want to show auth failed messages to
        // users who haven't enrolled their fingerprint.
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;
        Main.notifyError(problem);
    },

    _onInfoQuery: function(client, serviceName, question) {
        // We only expect questions to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        this.emit('ask-question', serviceName, question, '');
    },

    _onSecretInfoQuery: function(client, serviceName, secretQuestion) {
        // We only expect secret requests to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        this.emit('ask-question', serviceName, secretQuestion, '\u25cf');
    },

    _onReset: function() {
        this._userVerifier.run_dispose();
        this._userVerifier = null;

        this._checkForFingerprintReader();

        this.emit('reset');
    },

    _onVerificationComplete: function() {
        this.emit('verification-complete');
    },

    _onConversationStopped: function(client, serviceName) {
        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (serviceName == PASSWORD_SERVICE_NAME) {
            this.emit('verification-failed');
        } else if (serviceName == FINGERPRINT_SERVICE_NAME) {
            this.emit('hide-fingerprint-prompt');
        }
    },
});
Signals.addSignalMethods(ShellUserVerifier.prototype);
