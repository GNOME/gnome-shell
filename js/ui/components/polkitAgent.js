// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Pango = imports.gi.Pango;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Mainloop = imports.mainloop;
const Polkit = imports.gi.Polkit;
const PolkitAgent = imports.gi.PolkitAgent;

const Components = imports.ui.components;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;

const DIALOG_ICON_SIZE = 48;

const AuthenticationDialog = new Lang.Class({
    Name: 'AuthenticationDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(actionId, message, cookie, userNames) {
        this.parent({ styleClass: 'prompt-dialog' });

        this.actionId = actionId;
        this.message = message;
        this.userNames = userNames;
        this._wasDismissed = false;

        let mainContentBox = new St.BoxLayout({ style_class: 'prompt-dialog-main-layout',
                                                vertical: false });
        this.contentLayout.add(mainContentBox,
                               { x_fill: true,
                                 y_fill: true });

        let icon = new St.Icon({ icon_name: 'dialog-password-symbolic' });
        mainContentBox.add(icon,
                           { x_fill:  true,
                             y_fill:  false,
                             x_align: St.Align.END,
                             y_align: St.Align.START });

        let messageBox = new St.BoxLayout({ style_class: 'prompt-dialog-message-layout',
                                            vertical: true });
        mainContentBox.add(messageBox,
                           { expand: true, y_align: St.Align.START });

        this._subjectLabel = new St.Label({ style_class: 'prompt-dialog-headline',
                                            text: _("Authentication Required") });

        messageBox.add(this._subjectLabel,
                       { x_fill: false,
                         y_fill:  false,
                         x_align: St.Align.START,
                         y_align: St.Align.START });

        this._descriptionLabel = new St.Label({ style_class: 'prompt-dialog-description',
                                                text: message });
        this._descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._descriptionLabel.clutter_text.line_wrap = true;

        messageBox.add(this._descriptionLabel,
                       { x_fill: false,
                         y_fill:  true,
                         x_align: St.Align.START,
                         y_align: St.Align.START });

        if (userNames.length > 1) {
            log('polkitAuthenticationAgent: Received ' + userNames.length +
                ' identities that can be used for authentication. Only ' +
                'considering one.');
        }

        let userName = GLib.get_user_name();
        if (userNames.indexOf(userName) < 0)
            userName = 'root';
        if (userNames.indexOf(userName) < 0)
            userName = userNames[0];

        this._user = AccountsService.UserManager.get_default().get_user(userName);
        let userRealName = this._user.get_real_name()
        this._userLoadedId = this._user.connect('notify::is_loaded',
                                                Lang.bind(this, this._onUserChanged));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this, this._onUserChanged));

        // Special case 'root'
        let userIsRoot = false;
        if (userName == 'root') {
            userIsRoot = true;
            userRealName = _("Administrator");
        }

        if (userIsRoot) {
            let userLabel = new St.Label(({ style_class: 'polkit-dialog-user-root-label',
                                            text: userRealName }));
            messageBox.add(userLabel, { x_fill: false,
                                        x_align: St.Align.START });
        } else {
            let userBox = new St.BoxLayout({ style_class: 'polkit-dialog-user-layout',
                                             vertical: false });
            messageBox.add(userBox);
            this._userAvatar = new UserWidget.Avatar(this._user,
                                                     { iconSize: DIALOG_ICON_SIZE,
                                                       styleClass: 'polkit-dialog-user-icon' });
            this._userAvatar.actor.hide();
            userBox.add(this._userAvatar.actor,
                        { x_fill:  true,
                          y_fill:  false,
                          x_align: St.Align.END,
                          y_align: St.Align.START });
            let userLabel = new St.Label(({ style_class: 'polkit-dialog-user-label',
                                            text: userRealName }));
            userBox.add(userLabel,
                        { x_fill:  true,
                          y_fill:  false,
                          x_align: St.Align.END,
                          y_align: St.Align.MIDDLE });
        }

        this._onUserChanged();

        this._passwordBox = new St.BoxLayout({ vertical: false, style_class: 'prompt-dialog-password-box' });
        messageBox.add(this._passwordBox);
        this._passwordLabel = new St.Label(({ style_class: 'prompt-dialog-password-label' }));
        this._passwordBox.add(this._passwordLabel, { y_fill: false, y_align: St.Align.MIDDLE });
        this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                             text: "",
                                             can_focus: true});
        ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
        this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onEntryActivate));
        this._passwordBox.add(this._passwordEntry,
                              { expand: true });
        this.setInitialKeyFocus(this._passwordEntry);
        this._passwordBox.hide();

        this._errorMessageLabel = new St.Label({ style_class: 'prompt-dialog-error-label' });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        messageBox.add(this._errorMessageLabel, { x_fill: false, x_align: St.Align.START });
        this._errorMessageLabel.hide();

        this._infoMessageLabel = new St.Label({ style_class: 'prompt-dialog-info-label' });
        this._infoMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._infoMessageLabel.clutter_text.line_wrap = true;
        messageBox.add(this._infoMessageLabel);
        this._infoMessageLabel.hide();

        /* text is intentionally non-blank otherwise the height is not the same as for
         * infoMessage and errorMessageLabel - but it is still invisible because
         * gnome-shell.css sets the color to be transparent
         */
        this._nullMessageLabel = new St.Label({ style_class: 'prompt-dialog-null-label',
                                                text: 'abc'});
        this._nullMessageLabel.add_style_class_name('hidden');
        this._nullMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._nullMessageLabel.clutter_text.line_wrap = true;
        messageBox.add(this._nullMessageLabel);
        this._nullMessageLabel.show();

        this._cancelButton = this.addButton({ label: _("Cancel"),
                                              action: Lang.bind(this, this.cancel),
                                              key: Clutter.Escape },
                                            { expand: true, x_fill: false, x_align: St.Align.START });
        this.placeSpinner({ expand: false,
                            x_fill: false,
                            y_fill: false,
                            x_align: St.Align.END,
                            y_align: St.Align.MIDDLE });
        this._okButton = this.addButton({ label:  _("Authenticate"),
                                          action: Lang.bind(this, this._onAuthenticateButtonPressed),
                                          default: true },
                                        { expand: false, x_fill: false, x_align: St.Align.END });

        this._doneEmitted = false;

        this._identityToAuth = Polkit.UnixUser.new_for_name(userName);
        this._cookie = cookie;
    },

    performAuthentication: function() {
        this.destroySession();
        this._session = new PolkitAgent.Session({ identity: this._identityToAuth,
                                                  cookie: this._cookie });
        this._session.connect('completed', Lang.bind(this, this._onSessionCompleted));
        this._session.connect('request', Lang.bind(this, this._onSessionRequest));
        this._session.connect('show-error', Lang.bind(this, this._onSessionShowError));
        this._session.connect('show-info', Lang.bind(this, this._onSessionShowInfo));
        this._session.initiate();
    },

    _ensureOpen: function() {
        // NOTE: ModalDialog.open() is safe to call if the dialog is
        // already open - it just returns true without side-effects
        if (!this.open(global.get_current_time())) {
            // This can fail if e.g. unable to get input grab
            //
            // In an ideal world this wouldn't happen (because the
            // Shell is in complete control of the session) but that's
            // just not how things work right now.
            //
            // One way to make this happen is by running 'sleep 3;
            // pkexec bash' and then opening a popup menu.
            //
            // We could add retrying if this turns out to be a problem

            log('polkitAuthenticationAgent: Failed to show modal dialog.' +
                ' Dismissing authentication request for action-id ' + this.actionId +
                ' cookie ' + this._cookie);
            this._emitDone(true);
        }
    },

    _emitDone: function(dismissed) {
        if (!this._doneEmitted) {
            this._doneEmitted = true;
            this.emit('done', dismissed);
        }
    },

    _updateSensitivity: function(sensitive) {
        this._passwordEntry.reactive = sensitive;
        this._passwordEntry.clutter_text.editable = sensitive;

        this._okButton.can_focus = sensitive;
        this._okButton.reactive = sensitive;
        this.setWorking(!sensitive);
    },

    _onEntryActivate: function() {
        let response = this._passwordEntry.get_text();
        this._updateSensitivity(false);
        this._session.response(response);
        // When the user responds, dismiss already shown info and
        // error texts (if any)
        this._errorMessageLabel.hide();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.show();
    },

    _onAuthenticateButtonPressed: function() {
        this._onEntryActivate();
    },

    _onSessionCompleted: function(session, gainedAuthorization) {
        if (this._completed || this._doneEmitted)
            return;

        this._completed = true;

        /* Yay, all done */
        if (gainedAuthorization) {
            this._emitDone(false);

        } else {
            /* Unless we are showing an existing error message from the PAM
             * module (the PAM module could be reporting the authentication
             * error providing authentication-method specific information),
             * show "Sorry, that didn't work. Please try again."
             */
            if (!this._errorMessageLabel.visible && !this._wasDismissed) {
                /* Translators: "that didn't work" refers to the fact that the
                 * requested authentication was not gained; this can happen
                 * because of an authentication error (like invalid password),
                 * for instance. */
                this._errorMessageLabel.set_text(_("Sorry, that didn\'t work. Please try again."));
                this._errorMessageLabel.show();
                this._infoMessageLabel.hide();
                this._nullMessageLabel.hide();
            }

            /* Try and authenticate again */
            this.performAuthentication();
        }
    },

    _onSessionRequest: function(session, request, echo_on) {
        // Cheap localization trick
        if (request == 'Password:' || request == 'Password: ')
            this._passwordLabel.set_text(_("Password:"));
        else
            this._passwordLabel.set_text(request);

        if (echo_on)
            this._passwordEntry.clutter_text.set_password_char('');
        else
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE

        this._passwordBox.show();
        this._passwordEntry.set_text('');
        this._passwordEntry.grab_key_focus();
        this._updateSensitivity(true);
        this._ensureOpen();
    },

    _onSessionShowError: function(session, text) {
        this._passwordEntry.set_text('');
        this._errorMessageLabel.set_text(text);
        this._errorMessageLabel.show();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    },

    _onSessionShowInfo: function(session, text) {
        this._passwordEntry.set_text('');
        this._infoMessageLabel.set_text(text);
        this._infoMessageLabel.show();
        this._errorMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    },

    destroySession: function() {
        if (this._session) {
            if (!this._completed)
                this._session.cancel();
            this._completed = false;
            this._session = null;
        }
    },

    _onUserChanged: function() {
        if (this._user.is_loaded && this._userAvatar) {
            this._userAvatar.update();
            this._userAvatar.actor.show();
        }
    },

    cancel: function() {
        this._wasDismissed = true;
        this.close(global.get_current_time());
        this._emitDone(true);
    },
});
Signals.addSignalMethods(AuthenticationDialog.prototype);

const AuthenticationAgent = new Lang.Class({
    Name: 'AuthenticationAgent',

    _init: function() {
        this._currentDialog = null;
        this._handle = null;
        this._native = new Shell.PolkitAuthenticationAgent();
        this._native.connect('initiate', Lang.bind(this, this._onInitiate));
        this._native.connect('cancel', Lang.bind(this, this._onCancel));
    },

    enable: function() {
        try {
            this._native.register();
        } catch(e) {
            log('Failed to register AuthenticationAgent');
        }
    },

    disable: function() {
        try {
            this._native.unregister();
        } catch(e) {
            log('Failed to unregister AuthenticationAgent');
        }
    },

    _onInitiate: function(nativeAgent, actionId, message, iconName, cookie, userNames) {
        this._currentDialog = new AuthenticationDialog(actionId, message, cookie, userNames);

        // We actually don't want to open the dialog until we know for
        // sure that we're going to interact with the user. For
        // example, if the password for the identity to auth is blank
        // (which it will be on a live CD) then there will be no
        // conversation at all... of course, we don't *know* that
        // until we actually try it.
        //
        // See https://bugzilla.gnome.org/show_bug.cgi?id=643062 for more
        // discussion.

        this._currentDialog.connect('done', Lang.bind(this, this._onDialogDone));
        this._currentDialog.performAuthentication();
    },

    _onCancel: function(nativeAgent) {
        this._completeRequest(false);
    },

    _onDialogDone: function(dialog, dismissed) {
        this._completeRequest(dismissed);
    },

    _completeRequest: function(dismissed) {
        this._currentDialog.close();
        this._currentDialog.destroySession();
        this._currentDialog = null;

        this._native.complete(dismissed);
    },
});

const Component = AuthenticationAgent;
