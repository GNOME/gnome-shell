// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const GdmGreeter = imports.gi.GdmGreeter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const ModalDialog = imports.ui.modalDialog;

const Fprint = imports.gdm.fingerprint;
const GdmLoginDialog = imports.gdm.loginDialog;

function _fadeInActor(actor) {
    if (actor.opacity == 255 && actor.visible)
        return;

    actor.show();
    let [minHeight, naturalHeight] = actor.get_preferred_height(-1);

    actor.opacity = 0;
    actor.set_height(0);
    Tweener.addTween(actor,
                     { opacity: 255,
                       height: naturalHeight,
                       time: _FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.set_height(-1);
                       },
                     });
}

function _fadeOutActor(actor) {
    if (!actor.visible || actor.opacity == 0) {
        actor.opacity = 0;
        actor.hide();
    }

    Tweener.addTween(actor,
                     { opacity: 0,
                       height: 0,
                       time: _FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.hide();
                           this.set_height(-1);
                       },
                     });
}

// A widget showing the user avatar and name
const UserWidget = new Lang.Class({
    Name: 'UserWidget',

    _init: function(user) {
        this._user = user;

        this.actor = new St.BoxLayout({ style_class: 'status-chooser',
                                        vertical: false,
                                        reactive: false
                                      });

        this._iconBin = new St.Bin({ style_class: 'status-chooser-user-icon' });
        this.actor.add(this._iconBin,
                       { x_fill: true,
                         y_fill: true });

        this._label = new St.Label({ style_class: 'login-dialog-prompt-label',
                                     // FIXME:
                                     style: 'text-align: right' });
        this.actor.add(this._label,
                      { expand: true,
                        x_fill: true,
                        y_fill: true
                      });

        this._userLoadedId = this._user.connect('notify::is-loaded',
                                                Lang.bind(this,
                                                          this._updateUser));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this,
                                                           this._updateUser));
        this.actor.connect('notify::mapped', Lang.bind(this, function() {
            if (this.actor.mapped)
                this._updateUser();
        }));
    },

    destroy: function() {
        // clean up signal handlers
        if (this._userLoadedId != 0) {
            this._user.disconnect(this._userLoadedId);
            this._userLoadedId = 0;
        }

        if (this._userChangedId != 0) {
            this._user.disconnect(this._userChangedId);
            this._userChangedId = 0;
        }

        this.actor.destroy();
    },

    _updateUser: function() {
        let iconFile = null;
        if (this._user.is_loaded) {
            this._label.text = this._user.get_real_name();
            iconFile = this._user.get_icon_file();
            if (!GLib.file_test(iconFile, GLib.FileTest.EXISTS))
                iconFile = null;
        } else {
            this._label.text = "";
        }

        if (iconFile)
            this._setIconFromFile(iconFile);
        else
            this._setIconFromName('avatar-default');
    },

    // XXX: a GFileIcon instead?
    _setIconFromFile: function(iconFile) {
        this._iconBin.set_style('background-image: url("' + iconFile + '");' +
                                'background-size: contain;');
        this._iconBin.child = null;
    },

    _setIconFromName: function(iconName) {
        this._iconBin.set_style(null);

        if (iconName != null) {
            let icon = new St.Icon({ icon_name: iconName,
                                     icon_type: St.IconType.SYMBOLIC,
                                     icon_size: DIALOG_ICON_SIZE });
            this._iconBin.child = icon;
            this._iconBin.show();
        } else {
            this._iconBin.child = null;
            this._iconBin.hide();
        }
    }
});

const UnlockDialog = new Lang.Class({
    Name: 'UnlockDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function() {
        this.parent({ shellReactive: true,
                      styleClass: 'login-dialog' });

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._greeterClient = GdmGreeter.Server.new_for_display_sync(null, null);

        this._greeterClient.call_start_conversation_sync(GdmLoginDialog.PASSWORD_SERVICE_NAME, null);

        this._greeterClient.connect('reset',
                                    Lang.bind(this, this._onReset));
        this._greeterClient.connect('ready',
                                    Lang.bind(this, this._onReady));
        this._greeterClient.connect('info',
                                    Lang.bind(this, this._onInfo));
        this._greeterClient.connect('problem',
                                    Lang.bind(this, this._onProblem));
        this._greeterClient.connect('info-query',
                                    Lang.bind(this, this._onInfoQuery));
        this._greeterClient.connect('secret-info-query',
                                    Lang.bind(this, this._onSecretInfoQuery));
        this._greeterClient.connect('session-opened',
                                    Lang.bind(this, this._onSessionOpened));
        this._greeterClient.connect('conversation-stopped',
                                    Lang.bind(this, this._onConversationStopped));

        this._fprintManager = new Fprint.FprintManager();
        this._startFingerprintConversationIfNeeded();

        this._userWidget = new UserWidget(this._user);
        this.contentLayout.add_actor(this._userWidget.actor);

        this._promptLayout = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                                vertical: false
                                              });

        this._promptLabel = new St.Label({ style_class: 'login-dialog-prompt-label' });
        this._promptLayout.add(this._promptLabel,
                            { expand: false,
                              x_fill: true,
                              y_fill: true,
                              x_align: St.Align.START });

        this._promptEntry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                           can_focus: true });
        this._promptLayout.add(this._promptEntry,
                            { expand: true,
                              x_fill: true,
                              y_fill: false,
                              x_align: St.Align.START });

        this.contentLayout.add_actor(this._promptLayout);

        // Translators: this message is shown below the password entry field
        // to indicate the user can swipe their finger instead
        this._promptFingerprintMessage = new St.Label({ text: _("(or swipe finger)"),
                                                        style_class: 'login-dialog-prompt-fingerprint-message' });
        this._promptFingerprintMessage.hide();
        this.contentLayout.add_actor(this._promptFingerprintMessage);
        
        this._okButton = { label: _("Unlock"),
                           action: Lang.bind(this, this._doUnlock),
                           key: Clutter.KEY_Return,
                         };
        this.setButtons([this._okButton]);

        this._updateOkButton(true);
    },

    _updateOkButton: function(sensitive) {
        this._okButton.button.reactive = sensitive;
        this._okButton.button.can_focus = sensitive;
        if (sensitive)
            this._okButton.button.remove_style_pseudo_class('disabled');
        else
            this._okButton.button.add_style_pseudo_class('disabled');
    },

    _onReset: function() {
        // I'm not sure this is emitted for external greeters...

        this._greeterClient.call_start_conversation_sync(GdmLoginDialog.PASSWORD_SERVICE_NAME, null);
        this._startFingerprintConversationIfNeeded();
    },

   _startFingerprintConversationIfNeeded: function() {
       this._haveFingerprintReader = false;

       // FIXME: the greeter has a GSettings key for disabling fingerprint auth

       this._fprintManager.GetDefaultDeviceRemote(Lang.bind(this,
           function(device, error) {
               if (!error && device)
                   this._haveFingerprintReader = true;

               if (this._haveFingerprintReader)
                   this._greeterClient.call_start_conversation_sync(GdmLoginDialog.FINGERPRINT_SERVICE_NAME, null);
           }));
    },

    _onReady: function(greeter, serviceName) {
        greeter.call_begin_verification_for_user_sync(serviceName, this._userName, null);
    },

    _onInfo: function(greeter, serviceName, info) {
        // We don't display fingerprint messages, because they
        // have words like UPEK in them. Instead we use the messages
        // as a cue to display our own message.
        if (serviceName == GdmLoginDialog.FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader &&
            (!this._promptFingerprintMessage.visible ||
             this._promptFingerprintMessage.opacity != 255)) {

            _fadeInActor(this._promptFingerprintMessage);
            return;
        }

        if (serviceName != GdmLoginDialog.PASSWORD_SERVICE_NAME)
            return;
        Main.notify(info);
    },

    _onProblem: function(client, serviceName, problem) {
        // we don't want to show auth failed messages to
        // users who haven't enrolled their fingerprint.
        if (serviceName != GdmLoginDialog.PASSWORD_SERVICE_NAME)
            return;
        Main.notifyError(problem);
    },

    _onInfoQuery: function(client, serviceName, question) {
        // We only expect questions to come from the main auth service
        if (serviceName != GdmLoginDialog.PASSWORD_SERVICE_NAME)
            return;

        this._promptLabel.text = question;
        this._promptEntry.text = '';
        this._promptEntry.clutter_text.set_password_char('');

        this._currentQuery = serviceName;
        this._updateOkButton(true);
    },

    _onSecretInfoQuery: function(client, serviceName, secretQuestion) {
        // We only expect secret requests to come from the main auth service
        if (serviceName != GdmLoginDialog.PASSWORD_SERVICE_NAME)
            return;

        this._promptLabel.text = secretQuestion;
        this._promptEntry.text = '';
        this._promptEntry.clutter_text.set_password_char('\u25cf');

        this._currentQuery = serviceName;
        this._updateOkButton(true);
    },

    _doUnlock: function() {
        if (!this._currentQuery)
            return;

        let query = this._currentQuery;
        this._currentQuery = null;

        this._updateOkButton(false);

        this._greeterClient.call_answer_query_sync(query, this._promptEntry.text, null);
    },

    _onConversationStopped: function(client, serviceName) {
        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (serviceName == GdmLoginDialog.PASSWORD_SERVICE_NAME) {
            this._greeterClient.call_cancel_sync(null);
            this.emit('failed');
        } else if (serviceName == GdmLoginDialog.FINGERPRINT_SERVICE_NAME) {
            _fadeOutActor(this._promptFingerprintMessage);
        }
    },

    _onSessionOpened: function(client, serviceName) {
        // For external greeters, SessionOpened means we succeded
        // in the authentication process

        // Close the greeter proxy
        this._greeterClient.run_dispose();
        this._greeterClient = null;

        this.emit('unlocked');
    },

    destroy: function() {
        if (this._greeterClient) {
            this._greeterClient.run_dispose();
            this._greeterClient = null;
        }

        this.parent();
    },

    cancel: function() {
        this._greeterClient.call_cancel_sync(null);

        this.destroy();
    },
});
