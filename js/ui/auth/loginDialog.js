// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2011 Red Hat, Inc
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
 */

const AccountsService = imports.gi.AccountsService;
const Atk = imports.gi.Atk;
const Clutter = imports.gi.Clutter;
const CtrlAltTab = imports.ui.ctrlAltTab;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Realmd = imports.ui.auth.realmd;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gdm = imports.gi.Gdm;

const Animation = imports.ui.animation;
const AuthUtil = imports.ui.auth.util;
const Batch = imports.misc.batch;
const Fprint = imports.ui.auth.fingerprint;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const SessionList = imports.ui.auth.sessionList;
const Tweener = imports.ui.tweener;
const UserAvatar = imports.ui.userAvatar;
const UserList = imports.ui.auth.userList;
const UserWidget = imports.ui.userWidget;

const _FADE_ANIMATION_TIME = 0.25;
const _SCROLL_ANIMATION_TIME = 0.5;
const _WORK_SPINNER_ICON_SIZE = 24;
const _WORK_SPINNER_ANIMATION_DELAY = 1.0;
const _WORK_SPINNER_ANIMATION_TIME = 0.3;
const _TIMED_LOGIN_IDLE_THRESHOLD = 5.0;
const _LOGO_ICON_HEIGHT = 48;

let _loginDialog = null;

const LoginDialog = new Lang.Class({
    Name: 'LoginDialog',

    _init: function(parentActor) {
        this.actor = new St.Widget({ accessible_role: Atk.Role.WINDOW,
                                     style_class: 'login-dialog' });

        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        parentActor.add_child(this.actor);

        Main.ctrlAltTabManager.addGroup(this.actor,
                                        _("Login Window"),
                                        'dialog-password-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.MIDDLE });
        this._userManager = AccountsService.UserManager.get_default()
        this._greeterClient = new Gdm.Client();

        if (GLib.getenv('GDM_GREETER_TEST') != '1') {
            this._greeter = this._greeterClient.get_greeter_sync(null);

            this._greeter.connect('default-session-name-changed',
                                  Lang.bind(this, this._onDefaultSessionChanged));

            this._greeter.connect('session-opened',
                                  Lang.bind(this, this._onSessionOpened));
            this._greeter.connect('timed-login-requested',
                                  Lang.bind(this, this._onTimedLoginRequested));
        }

        this._userVerifier = new AuthUtil.ShellUserVerifier(this._greeterClient);
        this._userVerifier.connect('ask-question', Lang.bind(this, this._askQuestion));
        this._userVerifier.connect('show-message', Lang.bind(this, this._showMessage));
        this._userVerifier.connect('verification-failed', Lang.bind(this, this._verificationFailed));
        this._userVerifier.connect('reset', Lang.bind(this, this._reset));
        this._userVerifier.connect('show-login-hint', Lang.bind(this, this._showLoginHint));
        this._userVerifier.connect('hide-login-hint', Lang.bind(this, this._hideLoginHint));
        this._verifyingUser = false;

        this._settings = new Gio.Settings({ schema: AuthUtil.LOGIN_SCREEN_SCHEMA });

        this._settings.connect('changed::' + AuthUtil.BANNER_MESSAGE_KEY,
                               Lang.bind(this, this._updateBanner));
        this._settings.connect('changed::' + AuthUtil.BANNER_MESSAGE_TEXT_KEY,
                               Lang.bind(this, this._updateBanner));
        this._settings.connect('changed::' + AuthUtil.DISABLE_USER_LIST_KEY,
                               Lang.bind(this, this._updateDisableUserList));
        this._settings.connect('changed::' + AuthUtil.LOGO_KEY,
                               Lang.bind(this, this._updateLogo));

        this._textureCache = St.TextureCache.get_default();
        this._textureCache.connect('texture-file-changed',
                                   Lang.bind(this, this._updateLogoTexture));

        this._userSelectionBox = new St.BoxLayout({ style_class: 'login-dialog-user-selection-box',
                                                    vertical: true });

        this._userSelectionBox.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                            align_axis: Clutter.AlignAxis.BOTH,
                                                                            factor: 0.5 }));
        this.actor.add_child(this._userSelectionBox);

        this._bannerLabel = new St.Label({ style_class: 'login-dialog-banner',
                                           text: '' });
        this._userSelectionBox.add(this._bannerLabel);
        this._updateBanner();

        this._userList = new UserList.UserList();
        this._userSelectionBox.add(this._userList.actor,
                                   { expand: true,
                                     x_fill: true,
                                     y_fill: true });

        this._userList.actor.grab_key_focus();

        this._promptBox = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                             vertical: true });
        this._promptBox.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                     align_axis: Clutter.AlignAxis.BOTH,
                                                                     factor: 0.5 }));
        this.actor.add_child(this._promptBox);
        this._promptUser = new St.Bin({ x_fill: true,
                                        x_align: St.Align.START });
        this._promptBox.add(this._promptUser,
                            { x_align: St.Align.START,
                              x_fill: true,
                              y_fill: true,
                              expand: true });
        this._promptLabel = new St.Label({ style_class: 'login-dialog-prompt-label' });

        this._promptBox.add(this._promptLabel,
                            { expand: true,
                              x_fill: true,
                              y_fill: true,
                              x_align: St.Align.START });
        this._promptEntry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                           can_focus: true });
        this._promptEntryTextChangedId = 0;
        this._promptEntryActivateId = 0;
        this._promptBox.add(this._promptEntry,
                            { expand: false,
                              x_fill: false });

        this._promptMessage = new St.Label({ visible: false });
        this._promptBox.add(this._promptMessage, { x_fill: true });

        this._promptLoginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint-message' });
        this._promptLoginHint.hide();
        this._promptBox.add(this._promptLoginHint);

        this._sessionList = new SessionList.SessionList();
        this._sessionList.connect('session-activated',
                                  Lang.bind(this, function(list, sessionId) {
                                                this._greeter.call_select_session_sync (sessionId, null);
                                            }));

        this._promptBox.add(this._sessionList.actor,
                            { expand: true,
                              x_fill: false,
                              y_fill: true,
                              x_align: St.Align.START });

        this._buttonBox = new St.BoxLayout({ style_class: 'login-dialog-button-box',
                                             vertical: false });
        this._promptBox.add(this._buttonBox,
                            { expand:  true,
                              x_align: St.Align.MIDDLE,
                              y_align: St.Align.END });
        this._cancelButton = null;
        this._signInButton = null;

        this._promptBox.hide();

        // translators: this message is shown below the user list on the
        // login screen. It can be activated to reveal an entry for
        // manually entering the username.
        let notListedLabel = new St.Label({ text: _("Not listed?"),
                                            style_class: 'login-dialog-not-listed-label' });
        this._notListedButton = new St.Button({ style_class: 'login-dialog-not-listed-button',
                                                button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                                can_focus: true,
                                                child: notListedLabel,
                                                reactive: true,
                                                x_align: St.Align.START,
                                                x_fill: true });

        this._notListedButton.connect('clicked', Lang.bind(this, this._hideUserListAndLogIn));

        this._userSelectionBox.add(this._notListedButton,
                                   { expand: false,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._logoBin = new St.Bin({ style_class: 'login-dialog-logo-bin', y_expand: true });
        this._logoBin.set_y_align(Clutter.ActorAlign.END);
        this._logoBin.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                   align_axis: Clutter.AlignAxis.X_AXIS,
                                                                   factor: 0.5 }));
        this._logoBin.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                   align_axis: Clutter.AlignAxis.Y_AXIS,
                                                                   factor: 1.0 }));
        this.actor.add_actor(this._logoBin);
        this._updateLogo();

        if (!this._userManager.is_loaded)
            this._userManagerLoadedId = this._userManager.connect('notify::is-loaded',
                                                                  Lang.bind(this, function() {
                                                                      if (this._userManager.is_loaded) {
                                                                          this._loadUserList();
                                                                          this._userManager.disconnect(this._userManagerLoadedId);
                                                                          this._userManagerLoadedId = 0;
                                                                      }
                                                                  }));
        else
            this._loadUserList();

        this._userList.connect('activate',
                               Lang.bind(this, function(userList, item) {
                                   this._onUserListActivated(item);
                               }));

   },

    _updateDisableUserList: function() {
        let disableUserList = this._settings.get_boolean(AuthUtil.DISABLE_USER_LIST_KEY);

        // If this is the first time around, set initial focus
        if (this._disableUserList == undefined && disableUserList)
            this.setInitialKeyFocus(this._promptEntry);

        if (disableUserList != this._disableUserList) {
            this._disableUserList = disableUserList;

            if (!this._verifyingUser)
                this._reset();
        }
    },

    _updateBanner: function() {
        let enabled = this._settings.get_boolean(AuthUtil.BANNER_MESSAGE_KEY);
        let text = this._settings.get_string(AuthUtil.BANNER_MESSAGE_TEXT_KEY);

        if (enabled && text) {
            this._bannerLabel.set_text(text);
            this._bannerLabel.show();
        } else {
            this._bannerLabel.hide();
        }
    },

    _updateLogoTexture: function(cache, uri) {
        if (this._logoFileUri != uri)
            return;

        let icon = null;
        if (this._logoFileUri)
            icon = this._textureCache.load_uri_async(this._logoFileUri,
                                                     -1, _LOGO_ICON_HEIGHT);
        this._logoBin.set_child(icon);
    },

    _updateLogo: function() {
        let path = this._settings.get_string(AuthUtil.LOGO_KEY);

        this._logoFileUri = path ? Gio.file_new_for_path(path).get_uri() : null;
        this._updateLogoTexture(this._textureCache, this._logoFileUri);
    },

    _reset: function() {
        this._userVerifier.clear();

        this._updateSensitivity(true);
        this._promptMessage.hide();
        this._user = null;
        this._verifyingUser = false;

        if (this._disableUserList)
            this._hideUserListAndLogIn();
        else
            this._showUserList();
    },

    _setWorking: function(working) {
        if (!this._workSpinner)
            return;

        Tweener.removeTweens(this._workSpinner.actor);
        if (working) {
            this._workSpinner.play();
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 255,
                               delay: _WORK_SPINNER_ANIMATION_DELAY,
                               time: _WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear'
                             });
        } else {
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 0,
                               time: _WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear',
                               onCompleteScope: this,
                               onComplete: function() {
                                   if (this._workSpinner)
                                       this._workSpinner.stop();
                               }
                             });
        }
    },

    _verificationFailed: function() {
        this._promptEntry.text = '';

        this._updateSensitivity(true);
        this._setWorking(false);
    },

    _onDefaultSessionChanged: function(client, sessionId) {
        this._sessionList.setActiveSession(sessionId);
    },

    _showMessage: function(userVerifier, message, styleClass) {
        if (message) {
            this._promptMessage.text = message;
            this._promptMessage.styleClass = styleClass;
            this._promptMessage.show();
        } else {
            this._promptMessage.hide();
        }
    },

    _showLoginHint: function(verifier, message) {
        this._promptLoginHint.set_text(message)
        this._promptLoginHint.show();
        this._promptLoginHint.opacity = 255;
    },

    _hideLoginHint: function() {
        this._promptLoginHint.hide();
        this._promptLoginHint.set_text('');
    },

    cancel: function() {
        if (this._verifyingUser)
            this._userVerifier.cancel();
        else
            this._reset();
    },

    _showPrompt: function(forSecret) {
        this._sessionList.actor.hide();
        this._promptLabel.show();
        this._promptEntry.show();
        this._promptLoginHint.opacity = 0;
        this._promptLoginHint.show();
        this._promptBox.opacity = 0;
        this._promptBox.show();
        Tweener.addTween(this._promptBox,
                         { opacity: 255,
                           time: _FADE_ANIMATION_TIME,
                           transition: 'easeOutQuad' });

        if ((this._user && !this._user.is_logged_in()) || this._verifyingUser)
            this._sessionList.actor.show();

        this._promptEntry.grab_key_focus();

        let hold = new Batch.Hold();
        let tasks = [function() {
                         this._prepareDialog(forSecret, hold);
                     },

                     hold];

        let batch = new Batch.ConcurrentBatch(this, tasks);

        return batch.run();
    },

    _prepareDialog: function(forSecret, hold) {
        this._buttonBox.visible = true;
        this._buttonBox.destroy_all_children();

        if (!this._disableUserList || this._verifyingUser) {
            this._cancelButton = new St.Button({ style_class: 'modal-dialog-button',
                                                 button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                                 reactive: true,
                                                 can_focus: true,
                                                 label: _("Cancel") });
            this._cancelButton.connect('clicked',
                                       Lang.bind(this, function() {
                                           this.cancel();
                                       }));
            global.stage.connect('captured-event',
                                 Lang.bind(this, function(actor, event) {
                                    if (event.type() == Clutter.EventType.KEY_PRESS &&
                                        event.get_key_symbol() == Clutter.KEY_Escape) {
                                        this.cancel();
                                    }
                                 }));
            this._buttonBox.add(this._cancelButton,
                                { expand: true,
                                  x_fill: false,
                                  y_fill: false,
                                  x_align: St.Align.START,
                                  y_align: St.Align.END });
        }

        let spinnerIcon = global.datadir + '/theme/process-working.svg';
        this._workSpinner = new Animation.AnimatedIcon(spinnerIcon, _WORK_SPINNER_ICON_SIZE);
        this._workSpinner.actor.opacity = 0;
        this._workSpinner.actor.show();

        this._buttonBox.add(this._workSpinner.actor,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.MIDDLE });

        this._signInButton = new St.Button({ style_class: 'modal-dialog-button',
                                             button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                             reactive: true,
                                             can_focus: true,
                                             label: forSecret ? C_("button", "Sign In") : _("Next") });
        this._signInButton.connect('clicked',
                                   Lang.bind(this, function() {
                                       hold.release();
                                   }));
        this._signInButton.add_style_pseudo_class('default');
        this._buttonBox.add(this._signInButton,
                            { expand: true,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.END });

        this._updateSignInButtonSensitivity(this._promptEntry.text.length > 0);

        this._promptEntryTextChangedId =
            this._promptEntry.clutter_text.connect('text-changed',
                                                    Lang.bind(this, function() {
                                                        this._updateSignInButtonSensitivity(this._promptEntry.text.length > 0);
                                                    }));

         this._promptEntryActivateId =
             this._promptEntry.clutter_text.connect('activate', function() {
                 hold.release();
             });
    },

    _updateSensitivity: function(sensitive) {
        this._promptEntry.reactive = sensitive;
        this._promptEntry.clutter_text.editable = sensitive;
        this._sessionList.updateSensitivity(sensitive);
        this._updateSignInButtonSensitivity(sensitive);
    },

    _updateSignInButtonSensitivity: function(sensitive) {
        if (this._signInButton) {
            this._signInButton.reactive = sensitive;
            this._signInButton.can_focus = sensitive;
        }
    },

    _hidePrompt: function() {
        this._buttonBox.destroy_all_children();

        if (this._promptEntryTextChangedId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryTextChangedId);
            this._promptEntryTextChangedId = 0;
        }

        if (this._promptEntryActivateId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryActivateId);
            this._promptEntryActivateId = 0;
        }

        this._setWorking(false);
        this._promptBox.hide();
        this._promptLoginHint.hide();

        this._promptUser.set_child(null);

        this._updateSensitivity(true);
        this._promptEntry.set_text('');

        this._sessionList.close();
        this._promptLoginHint.hide();

        this._buttonBox.destroy_all_children();
        this._signInButton = null;
        this._cancelButton = null;
    },

    _askQuestion: function(verifier, serviceName, question, passwordChar) {
        this._promptLabel.set_text(question);

        this._updateSensitivity(true);
        this._promptEntry.set_text('');
        this._promptEntry.clutter_text.set_password_char(passwordChar);

        let tasks = [function() {
                         return this._showPrompt(!!passwordChar);
                     },

                     function() {
                         let text = this._promptEntry.get_text();
                         this._updateSensitivity(false);
                         this._setWorking(true);
                         this._userVerifier.answerQuery(serviceName, text);
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        return batch.run();
    },

    _showRealmLoginHint: function(realmManager, hint) {
        if (!hint)
            return;

        hint = hint.replace(/%U/g, 'user');
        hint = hint.replace(/%D/g, 'DOMAIN');
        hint = hint.replace(/%[^UD]/g, '');

        // Translators: this message is shown below the username entry field
        // to clue the user in on how to login to the local network realm
        this._showLoginHint(null, _("(e.g., user or %s)").format(hint));
    },

    _askForUsernameAndLogIn: function() {
        this._promptLabel.set_text(_("Username: "));
        this._promptEntry.set_text('');
        this._promptEntry.clutter_text.set_password_char('');

        let realmManager = new Realmd.Manager();
        let signalId = realmManager.connect('login-format-changed',
	                                        Lang.bind(this, this._showRealmLoginHint));
        this._showRealmLoginHint(realmManager.loginFormat);

        let tasks = [this._showPrompt,

                     function() {
                         let userName = this._promptEntry.get_text();
                         this._promptEntry.reactive = false;
                         return this._beginVerificationForUser(userName);
                     },

                     function() {
                         realmManager.disconnect(signalId)
                         realmManager.release();
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        return batch.run();
    },

    _startSession: function(serviceName) {
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: _FADE_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onUpdate: function() {
                               let children = Main.layoutManager.uiGroup.get_children();

                               for (let i = 0; i < children.length; i++) {
                                   if (children[i] != Main.layoutManager.screenShieldGroup)
                                       children[i].opacity = this.actor.opacity;
                               }
                           },
                           onUpdateScope: this,
                           onComplete: function() {
                               Mainloop.idle_add(Lang.bind(this, function() {
                                   this._greeter.call_start_session_when_ready_sync(serviceName, true, null);
                                   return false;
                               }));
                           },
                           onCompleteScope: this });
    },

    _onSessionOpened: function(client, serviceName) {
        if (!this._userVerifier.hasPendingMessages) {
            this._startSession(serviceName);
        } else {
            let signalId = this._userVerifier.connect('no-more-messages',
                                                      Lang.bind(this, function() {
                                                          this._userVerifier.disconnect(signalId);
                                                          this._startSession(serviceName);
                                                      }));
        }
    },

    _waitForItemForUser: function(userName) {
        let item = this._userList.getItemFromUserName(userName);

        if (item)
          return null;

        let hold = new Batch.Hold();
        let signalId = this._userList.connect('item-added',
                                              Lang.bind(this, function() {
                                                  let item = this._userList.getItemFromUserName(userName);

                                                  if (item)
                                                      hold.release();
                                              }));

        hold.connect('release', Lang.bind(this, function() {
                         this._userList.disconnect(signalId);
                     }));

        return hold;
    },

    _showTimedLoginAnimation: function() {
        this._timedLoginItem.actor.grab_key_focus();
        return this._timedLoginItem.showTimedLoginIndicator(this._timedLoginAnimationTime);
    },

    _blockTimedLoginUntilIdle: function() {
        // This blocks timed login from starting until a few
        // seconds after the user stops interacting with the
        // login screen.
        //
        // We skip this step if the timed login delay is very
        // short.
        if ((this._timedLoginDelay - _TIMED_LOGIN_IDLE_THRESHOLD) <= 0)
          return null;

        let hold = new Batch.Hold();

        this._timedLoginIdleTimeOutId = Mainloop.timeout_add_seconds(_TIMED_LOGIN_IDLE_THRESHOLD,
                                                                     function() {
                                                                         this._timedLoginAnimationTime -= _TIMED_LOGIN_IDLE_THRESHOLD;
                                                                         hold.release();
                                                                     });
        return hold;
    },

    _startTimedLogin: function(userName, delay) {
        this._timedLoginItem = null;
        this._timedLoginDelay = delay;
        this._timedLoginAnimationTime = delay;

        let tasks = [function() {
                         return this._waitForItemForUser(userName);
                     },

                     function() {
                         this._timedLoginItem = this._userList.getItemFromUserName(userName);
                     },

                     function() {
                         // If we're just starting out, start on the right
                         // item.
                         if (!this._userManager.is_loaded) {
                             this._userList.jumpToItem(this._timedLoginItem);
                         }
                     },

                     this._blockTimedLoginUntilIdle,

                     function() {
                         this._userList.scrollToItem(this._timedLoginItem);
                     },

                     this._showTimedLoginAnimation,

                     function() {
                         this._timedLoginBatch = null;
                         this._greeter.call_begin_auto_login_sync(userName, null);
                     }];

        this._timedLoginBatch = new Batch.ConsecutiveBatch(this, tasks);

        return this._timedLoginBatch.run();
    },

    _resetTimedLogin: function() {
        if (this._timedLoginBatch) {
            this._timedLoginBatch.cancel();
            this._timedLoginBatch = null;
        }

        if (this._timedLoginItem)
            this._timedLoginItem.hideTimedLoginIndicator();

        let userName = this._timedLoginItem.user.get_user_name();

        if (userName)
            this._startTimedLogin(userName, this._timedLoginDelay);
    },

    _onTimedLoginRequested: function(client, userName, seconds) {
        this._startTimedLogin(userName, seconds);

        global.stage.connect('captured-event',
                             Lang.bind(this, function(actor, event) {
                                if (this._timedLoginDelay == undefined)
                                    return false;

                                if (event.type() == Clutter.EventType.KEY_PRESS ||
                                    event.type() == Clutter.EventType.BUTTON_PRESS) {
                                    if (this._timedLoginBatch) {
                                        this._timedLoginBatch.cancel();
                                        this._timedLoginBatch = null;
                                    }
                                } else if (event.type() == Clutter.EventType.KEY_RELEASE ||
                                           event.type() == Clutter.EventType.BUTTON_RELEASE) {
                                    this._resetTimedLogin();
                                }

                                return false;
                             }));
    },

    _setUserListExpanded: function(expanded) {
        this._userList.updateStyle(expanded);
        this._userSelectionBox.visible = expanded;
    },

    _hideUserListAndLogIn: function() {
        this._setUserListExpanded(false);
        AuthUtil.cloneAndFadeOutActor(this._userSelectionBox);
        this._askForUsernameAndLogIn();
    },

    _showUserList: function() {
        this._hidePrompt();
        this._setUserListExpanded(true);
        this._userList.actor.grab_key_focus();
    },

    _beginVerificationForUser: function(userName) {
        let hold = new Batch.Hold();

        this._userVerifier.begin(userName, hold);
        this._verifyingUser = true;
        return hold;
    },

    _beginVerificationForItem: function(item) {
        let userWidget = new UserWidget.UserWidget(item.user);
        this._promptUser.set_child(userWidget.actor);

        let tasks = [function() {
                         let userName = item.user.get_user_name();
                         return this._beginVerificationForUser(userName);
                     }];
        let batch = new Batch.ConsecutiveBatch(this, tasks);
        return batch.run();
    },

    _onUserListActivated: function(activatedItem) {
        let tasks = [function() {
                         return AuthUtil.cloneAndFadeOutActor(this._userSelectionBox);
                     },
                     function() {
                         this._setUserListExpanded(false);
                     }];

        this._user = activatedItem.user;

        let batch = new Batch.ConcurrentBatch(this, [new Batch.ConsecutiveBatch(this, tasks),
                                                     this._beginVerificationForItem(activatedItem)]);
        batch.run();
    },

    _onDestroy: function() {
        if (this._userManagerLoadedId) {
            this._userManager.disconnect(this._userManagerLoadedId);
            this._userManagerLoadedId = 0;
        }

        Main.ctrlAltTabManager.removeGroup(this.actor);
    },

    _loadUserList: function() {
        let users = this._userManager.list_users();

        for (let i = 0; i < users.length; i++) {
            this._userList.addUser(users[i]);
        }

        this._updateDisableUserList();

        this._userManager.connect('user-added',
                                  Lang.bind(this, function(userManager, user) {
                                      this._userList.addUser(user);
                                  }));

        this._userManager.connect('user-removed',
                                  Lang.bind(this, function(userManager, user) {
                                      this._userList.removeUser(user);
                                  }));
    },

    addCharacter: function(unichar) {
        this._promptEntry.clutter_text.insert_unichar(unichar);
    },
});
