/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * Copyright 2010 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
*
 * Author: David Zeuthen <davidz@redhat.com>
 */

const Lang = imports.lang;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;
const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Pango = imports.gi.Pango;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const Mainloop = imports.mainloop;
const Polkit = imports.gi.Polkit;
const PolkitAgent = imports.gi.PolkitAgent;

const ModalDialog = imports.ui.modalDialog;

function AuthenticationDialog(message, cookie, userNames) {
    this._init(message, cookie, userNames);
}

AuthenticationDialog.prototype = {
    __proto__: ModalDialog.ModalDialog.prototype,

    _init: function(message, cookie, userNames) {
        ModalDialog.ModalDialog.prototype._init.call(this, { styleClass: 'polkit-dialog' });

        this.message = message;
        this.userNames = userNames;

        let mainContentBox = new St.BoxLayout({ style_class: 'polkit-dialog-main-layout',
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

        let messageBox = new St.BoxLayout({ style_class: 'polkit-dialog-message-layout',
                                            vertical: true });
        mainContentBox.add(messageBox,
                           { y_align: St.Align.START });

        this._subjectLabel = new St.Label({ style_class: 'polkit-dialog-headline',
                                            text: _('Authentication Required') });

        messageBox.add(this._subjectLabel,
                       { y_fill:  false,
                         y_align: St.Align.START });

        this._descriptionLabel = new St.Label({ style_class: 'polkit-dialog-description',
                                                text: message });
        this._descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._descriptionLabel.clutter_text.line_wrap = true;

        messageBox.add(this._descriptionLabel,
                       { y_fill:  true,
                         y_align: St.Align.START });

        if (userNames.length > 1) {
            log('polkitAuthenticationAgent: Received ' + userNames.length +
                ' identities that can be used for authentication. Only ' +
                'considering the first one.');
        }

        let userName = userNames[0];

        this._user = Gdm.UserManager.ref_default().get_user(userName);
        let userRealName = this._user.get_real_name()
        this._userLoadedId = this._user.connect('notify::is_loaded',
                                                Lang.bind(this, this._onUserChanged));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this, this._onUserChanged));

        // Special case 'root'
        if (userName == 'root')
            userRealName = _('Administrator');

        // Work around Gdm.UserManager returning an empty string for the real name
        if (userRealName.length == 0)
            userRealName = userName;

        let userBox = new St.BoxLayout({ style_class: 'polkit-dialog-user-layout',
                                         vertical: false });
        messageBox.add(userBox);

        this._userIcon = new St.Icon();
        this._userIcon.hide();
        userBox.add(this._userIcon,
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

        this._onUserChanged();

        this._passwordBox = new St.BoxLayout({ vertical: false });
        messageBox.add(this._passwordBox);
        this._passwordLabel = new St.Label(({ style_class: 'polkit-dialog-password-label' }));
        this._passwordBox.add(this._passwordLabel);
        this._passwordEntry = new St.Entry({ style_class: 'polkit-dialog-password-entry',
                                             text: _(''),
                                             can_focus: true});
        this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onEntryActivate));
        this._passwordBox.add(this._passwordEntry,
                              {expand: true });
        this._passwordBox.hide();

        this._errorBox = new St.BoxLayout({ style_class: 'polkit-dialog-error-box' });
        messageBox.add(this._errorBox);
        let errorIcon = new St.Icon({ icon_name: 'dialog-error',
                                      icon_size: 24,
                                      style_class: 'polkit-dialog-error-icon' });
        this._errorBox.add(errorIcon, { y_align: St.Align.MIDDLE });
        this._errorMessage = new St.Label({ style_class: 'polkit-dialog-error-label' });
        this._errorMessage.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessage.clutter_text.line_wrap = true;
        this._errorBox.add(this._errorMessage, { expand: true,
                                                 y_align: St.Align.MIDDLE,
                                                 y_fill: true });
        this._errorBox.hide();

        this._infoBox = new St.BoxLayout({ style_class: 'polkit-dialog-info-box' });
        messageBox.add(this._infoBox);
        let infoIcon = new St.Icon({ icon_name: 'dialog-information',
                                     icon_size: 24,
                                     style_class: 'polkit-dialog-info-icon' });
        this._infoBox.add(infoIcon, { y_align: St.Align.MIDDLE });
        this._infoMessage = new St.Label({ style_class: 'polkit-dialog-info-label'});
        this._infoMessage.clutter_text.line_wrap = true;
        this._infoBox.add(this._infoMessage, { expand: true,
                                               y_align: St.Align.MIDDLE,
                                               y_fill: true });
        this._infoBox.hide();

        this.setButtons([{ label: _('Cancel'),
                           action: Lang.bind(this, this.cancel),
                           key:    Clutter.Escape
                         },
                         { label:  _('Authenticate'),
                           action: Lang.bind(this, this._onAuthenticateButtonPressed)
                         }]);

        this._doneEmitted = false;

        this._identityToAuth = Polkit.UnixUser.new_for_name(userName);
        this._cookie = cookie;

        this._session = new PolkitAgent.Session({ identity: this._identityToAuth,
                                                  cookie: this._cookie });
        this._session.connect('completed', Lang.bind(this, this._onSessionCompleted));
        this._session.connect('request', Lang.bind(this, this._onSessionRequest));
        this._session.connect('show-error', Lang.bind(this, this._onSessionShowError));
        this._session.connect('show-info', Lang.bind(this, this._onSessionShowInfo));
        this.connect('opened',
                     Lang.bind(this, function() {
                         this._session.initiate();
                     }));
    },

    _emitDone: function(keepVisible) {
        if (!this._doneEmitted) {
            this._doneEmitted = true;
            this.emit('done', keepVisible);
        }
    },

    _onEntryActivate: function() {
        let response = this._passwordEntry.get_text();
        this._session.response(response);
        // When the user responds, dismiss already shown info and
        // error texts (if any)
        this._errorBox.hide();
        this._infoBox.hide();
    },

    _onAuthenticateButtonPressed: function() {
        this._onEntryActivate();
    },

    _onSessionCompleted: function(session, gainedAuthorization) {
        this._passwordBox.hide();
        this._emitDone(!gainedAuthorization);
    },

    _onSessionRequest: function(session, request, echo_on) {
        // Cheap localization trick
        if (request == 'Password:')
            this._passwordLabel.set_text(_('Password:'));
        else
            this._passwordLabel.set_text(request);

        if (echo_on)
            this._passwordEntry.clutter_text.set_password_char('');
        else
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE

        this._passwordBox.show();
        this._passwordEntry.set_text('');
        this._passwordEntry.grab_key_focus();
    },

    _onSessionShowError: function(session, text) {
        this._passwordEntry.set_text('');
        this._errorMessage.set_text(text);
        this._errorBox.show();
    },

    _onSessionShowInfo: function(session, text) {
        this._passwordEntry.set_text('');
        this._infoMessage.set_text(text);
        this._infoBox.show();
    },

    destroySession: function() {
        if (this._session) {
            this._session.cancel();
            this._session = null;
        }
    },

    _onUserChanged: function() {
        if (this._user.is_loaded) {
            let iconFileName = this._user.get_icon_file();
            let iconFile = Gio.file_new_for_path(iconFileName);
            let icon;
            if (iconFile.query_exists(null)) {
                icon = new Gio.FileIcon({file: iconFile});
            } else {
                icon = new Gio.ThemedIcon({name: 'avatar-default'});
            }
            this._userIcon.set_gicon (icon);
            this._userIcon.show();
        }
    },

    cancel: function() {
        this.close(global.get_current_time());
        this._emitDone(false);
    },

};
Signals.addSignalMethods(AuthenticationDialog.prototype);

function AuthenticationAgent() {
    this._init();
}

AuthenticationAgent.prototype = {
    _init: function() {
        this._native = new Shell.PolkitAuthenticationAgent();
        this._native.connect('initiate', Lang.bind(this, this._onInitiate));
        this._native.connect('cancel', Lang.bind(this, this._onCancel));
        this._currentDialog = null;
        this._isCompleting = false;
    },

    _onInitiate: function(nativeAgent, actionId, message, iconName, cookie, userNames) {
        this._currentDialog = new AuthenticationDialog(message, cookie, userNames);
        if (!this._currentDialog.open(global.get_current_time())) {
            // This can fail if e.g. unable to get input grab
            //
            // In an ideal world this wouldn't happen (because the
            // Shell is in complete control of the session) but that's
            // just not how things work right now.
            //
            // We could add retrying if this turns out to be a problem
            log('polkitAuthenticationAgent: Failed to show modal dialog');
            this._currentDialog.destroySession();
            this._currentDialog = null;
            this._native.complete()
        } else {
            this._currentDialog.connect('done', Lang.bind(this, this._onDialogDone));
        }
    },

    _onCancel: function(nativeAgent) {
        this._completeRequest(false);
    },

    _onDialogDone: function(dialog, keepVisible) {
        this._completeRequest(keepVisible);
    },

    _reallyCompleteRequest: function() {
        this._currentDialog.close();
        this._currentDialog.destroySession();
        this._currentDialog = null;
        this._isCompleting = false;

        this._native.complete()
    },

    _completeRequest: function(keepVisible) {
        if (this._isCompleting)
            return;

        this._isCompleting = true;

        if (keepVisible) {
            // Give the user 2 seconds to read 'Authentication Failure' before
            // dismissing the dialog
            Mainloop.timeout_add(2000,
                                 Lang.bind(this,
                                           function() {
                                               this._reallyCompleteRequest();
                                           }));
        } else {
            this._reallyCompleteRequest();
        }
    }
}

function init() {
    let agent = new AuthenticationAgent();
}
