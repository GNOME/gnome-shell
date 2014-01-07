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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

const AccountsService = imports.gi.AccountsService;
const Atk = imports.gi.Atk;
const Clutter = imports.gi.Clutter;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const AuthPrompt = imports.gdm.authPrompt;
const Batch = imports.gdm.batch;
const BoxPointer = imports.ui.boxpointer;
const CtrlAltTab = imports.ui.ctrlAltTab;
const GdmUtil = imports.gdm.util;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Realmd = imports.gdm.realmd;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

const _FADE_ANIMATION_TIME = 0.25;
const _SCROLL_ANIMATION_TIME = 0.5;
const _TIMED_LOGIN_IDLE_THRESHOLD = 5.0;
const _LOGO_ICON_HEIGHT = 48;

let _loginDialog = null;

const UserListItem = new Lang.Class({
    Name: 'UserListItem',

    _init: function(user) {
        this.user = user;
        this._userChangedId = this.user.connect('changed',
                                                 Lang.bind(this, this._onUserChanged));

        let layout = new St.BoxLayout({ vertical: true });
        this.actor = new St.Button({ style_class: 'login-dialog-user-list-item',
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                     can_focus: true,
                                     child: layout,
                                     reactive: true,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._userWidget = new UserWidget.UserWidget(this.user);
        layout.add(this._userWidget.actor);

        this._timedLoginIndicator = new St.Bin({ style_class: 'login-dialog-timed-login-indicator',
                                                 scale_x: 0 });
        layout.add(this._timedLoginIndicator);

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this._onUserChanged();
    },

    _onUserChanged: function() {
        this._updateLoggedIn();
    },

    _updateLoggedIn: function() {
        if (this.user.is_logged_in())
            this.actor.add_style_pseudo_class('logged-in');
        else
            this.actor.remove_style_pseudo_class('logged-in');
    },

    _onClicked: function() {
        this.emit('activate');
    },

    showTimedLoginIndicator: function(time) {
        let hold = new Batch.Hold();

        this.hideTimedLoginIndicator();
        Tweener.addTween(this._timedLoginIndicator,
                         { scale_x: 1.,
                           time: time,
                           transition: 'linear',
                           onComplete: function() {
                               hold.release();
                           },
                           onCompleteScope: this
                         });
        return hold;
    },

    hideTimedLoginIndicator: function() {
        Tweener.removeTweens(this._timedLoginIndicator);
        this._timedLoginIndicator.scale_x = 0.;
    }
});
Signals.addSignalMethods(UserListItem.prototype);

const UserList = new Lang.Class({
    Name: 'UserList',

    _init: function() {
        this.actor = new St.ScrollView({ style_class: 'login-dialog-user-list-view'});
        this.actor.set_policy(Gtk.PolicyType.NEVER,
                              Gtk.PolicyType.AUTOMATIC);

        this._box = new St.BoxLayout({ vertical: true,
                                       style_class: 'login-dialog-user-list',
                                       pseudo_class: 'expanded' });

        this.actor.add_actor(this._box);
        this._items = {};

        this.actor.connect('key-focus-in', Lang.bind(this, this._moveFocusToItems));
    },

    _moveFocusToItems: function() {
        let hasItems = Object.keys(this._items).length > 0;

        if (!hasItems)
            return;

        if (global.stage.get_key_focus() != this.actor)
            return;

        let focusSet = this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        if (!focusSet) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
                this._moveFocusToItems();
                return false;
            }));
        }
    },

    _onItemActivated: function(activatedItem) {
        this.emit('activate', activatedItem);
    },

    updateStyle: function(isExpanded) {
        let tasks = [];

        if (isExpanded)
            this._box.add_style_pseudo_class('expanded');
        else
            this._box.remove_style_pseudo_class('expanded');

        for (let userName in this._items) {
            let item = this._items[userName];
            item.actor.sync_hover();
        }
    },

    scrollToItem: function(item) {
        let box = item.actor.get_allocation_box();

        let adjustment = this.actor.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);
        Tweener.removeTweens(adjustment);
        Tweener.addTween (adjustment,
                          { value: value,
                            time: _SCROLL_ANIMATION_TIME,
                            transition: 'easeOutQuad' });
    },

    jumpToItem: function(item) {
        let box = item.actor.get_allocation_box();

        let adjustment = this.actor.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);

        adjustment.set_value(value);
    },

    getItemFromUserName: function(userName) {
        let item = this._items[userName];

        if (!item)
            return null;

        return item;
    },

    addUser: function(user) {
        if (!user.is_loaded)
            return;

        if (user.is_system_account())
            return;

        if (user.locked)
           return;

        let userName = user.get_user_name();

        if (!userName)
            return;

        this.removeUser(user);

        let item = new UserListItem(user);
        this._box.add(item.actor, { x_fill: true });

        this._items[userName] = item;

        item.connect('activate',
                     Lang.bind(this, this._onItemActivated));

        // Try to keep the focused item front-and-center
        item.actor.connect('key-focus-in',
                           Lang.bind(this,
                                     function() {
                                         this.scrollToItem(item);
                                     }));

        this._moveFocusToItems();

        this.emit('item-added', item);
    },

    removeUser: function(user) {
        if (!user.is_loaded)
            return;

        let userName = user.get_user_name();

        if (!userName)
            return;

        let item = this._items[userName];

        if (!item)
            return;

        item.actor.destroy();
        delete this._items[userName];
    }
});
Signals.addSignalMethods(UserList.prototype);

const SessionMenuButton = new Lang.Class({
    Name: 'SessionMenuButton',

    _init: function() {
        let gearIcon = new St.Icon({ icon_name: 'emblem-system-symbolic' });
        this._button = new St.Button({ style_class: 'login-dialog-session-list-button',
                                       reactive: true,
                                       track_hover: true,
                                       can_focus: true,
                                       accessible_name: _("Choose Session"),
                                       accessible_role: Atk.Role.MENU,
                                       child: gearIcon });

        this.actor = new St.Bin({ child: this._button });

        this._menu = new PopupMenu.PopupMenu(this._button, 0, St.Side.TOP);
        Main.uiGroup.add_actor(this._menu.actor);
        this._menu.actor.hide();

        this._menu.connect('open-state-changed',
                           Lang.bind(this, function(menu, isOpen) {
                                if (isOpen)
                                    this._button.add_style_pseudo_class('active');
                                else
                                    this._button.remove_style_pseudo_class('active');
                           }));

        this._manager = new PopupMenu.PopupMenuManager({ actor: this._button });
        this._manager.addMenu(this._menu);

        this._button.connect('clicked', Lang.bind(this, function() {
            this._menu.toggle();
        }));

        this._items = {};
        this._activeSessionId = null;
        this._populate();
    },

    updateSensitivity: function(sensitive) {
        this._button.reactive = sensitive;
        this._button.can_focus = sensitive;
        this._menu.close(BoxPointer.PopupAnimation.NONE);
    },

    _updateOrnament: function() {
        let itemIds = Object.keys(this._items);
        for (let i = 0; i < itemIds.length; i++) {
            if (itemIds[i] == this._activeSessionId)
                this._items[itemIds[i]].setOrnament(PopupMenu.Ornament.DOT);
            else
                this._items[itemIds[i]].setOrnament(PopupMenu.Ornament.NONE);
        }
    },

    setActiveSession: function(sessionId) {
         if (sessionId == this._activeSessionId)
             return;

         this._activeSessionId = sessionId;
         this._updateOrnament();

         this.emit('session-activated', this._activeSessionId);
    },

    close: function() {
        this._menu.close();
    },

    _populate: function() {
        let ids = Gdm.get_session_ids();
        ids.sort();

        if (ids.length <= 1) {
            this._button.hide();
            return;
        }

        for (let i = 0; i < ids.length; i++) {
            let [sessionName, sessionDescription] = Gdm.get_session_name_and_description(ids[i]);

            let id = ids[i];
            let item = new PopupMenu.PopupMenuItem(sessionName);
            this._menu.addMenuItem(item);
            this._items[id] = item;

            if (!this._activeSessionId)
                this.setActiveSession(id);

            item.connect('activate', Lang.bind(this, function() {
                this.setActiveSession(id);
            }));
        }
    }
});
Signals.addSignalMethods(SessionMenuButton.prototype);

const LoginDialog = new Lang.Class({
    Name: 'LoginDialog',

    _init: function(parentActor) {
        this.actor = new St.Widget({ accessible_role: Atk.Role.WINDOW,
                                     layout_manager: new Clutter.BinLayout(),
                                     style_class: 'login-dialog',
                                     visible: false });

        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        parentActor.add_child(this.actor);

        this._userManager = AccountsService.UserManager.get_default()
        let gdmClient = new Gdm.Client();

        if (GLib.getenv('GDM_GREETER_TEST') != '1') {
            this._greeter = gdmClient.get_greeter_sync(null);

            this._greeter.connect('default-session-name-changed',
                                  Lang.bind(this, this._onDefaultSessionChanged));

            this._greeter.connect('session-opened',
                                  Lang.bind(this, this._onSessionOpened));
            this._greeter.connect('timed-login-requested',
                                  Lang.bind(this, this._onTimedLoginRequested));
        }

        this._settings = new Gio.Settings({ schema: GdmUtil.LOGIN_SCREEN_SCHEMA });

        this._settings.connect('changed::' + GdmUtil.BANNER_MESSAGE_KEY,
                               Lang.bind(this, this._updateBanner));
        this._settings.connect('changed::' + GdmUtil.BANNER_MESSAGE_TEXT_KEY,
                               Lang.bind(this, this._updateBanner));
        this._settings.connect('changed::' + GdmUtil.DISABLE_USER_LIST_KEY,
                               Lang.bind(this, this._updateDisableUserList));
        this._settings.connect('changed::' + GdmUtil.LOGO_KEY,
                               Lang.bind(this, this._updateLogo));

        this._textureCache = St.TextureCache.get_default();
        this._textureCache.connect('texture-file-changed',
                                   Lang.bind(this, this._updateLogoTexture));

        this._userSelectionBox = new St.BoxLayout({ style_class: 'login-dialog-user-selection-box',
                                                    x_align: Clutter.ActorAlign.CENTER,
                                                    y_align: Clutter.ActorAlign.CENTER,
                                                    x_expand: true,
                                                    y_expand: true,
                                                    vertical: true,
                                                    visible: false });
        this.actor.add_child(this._userSelectionBox);

        this._bannerLabel = new St.Label({ style_class: 'login-dialog-banner',
                                           text: '' });
        this._userSelectionBox.add(this._bannerLabel);
        this._updateBanner();

        this._userList = new UserList();
        this._userSelectionBox.add(this._userList.actor,
                                   { expand: true,
                                     x_fill: true,
                                     y_fill: true });

        this._authPrompt = new AuthPrompt.AuthPrompt(gdmClient, AuthPrompt.AuthPromptMode.UNLOCK_OR_LOG_IN);
        this._authPrompt.connect('prompted', Lang.bind(this, this._onPrompted));
        this._authPrompt.connect('reset', Lang.bind(this, this._onReset));
        this._authPrompt.hide();
        this.actor.add_child(this._authPrompt.actor);

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

        this._notListedButton.connect('clicked', Lang.bind(this, this._hideUserListAskForUsernameAndBeginVerification));

        this._notListedButton.hide();

        this._userSelectionBox.add(this._notListedButton,
                                   { expand: false,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._logoBin = new St.Widget({ style_class: 'login-dialog-logo-bin',
                                        x_align: Clutter.ActorAlign.CENTER,
                                        y_align: Clutter.ActorAlign.END,
                                        x_expand: true,
                                        y_expand: true });
        this.actor.add_child(this._logoBin);
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


        this._sessionMenuButton = new SessionMenuButton();
        this._sessionMenuButton.connect('session-activated',
                                  Lang.bind(this, function(list, sessionId) {
                                                this._greeter.call_select_session_sync (sessionId, null);
                                            }));
        this._sessionMenuButton.actor.opacity = 0;
        this._sessionMenuButton.actor.show();
        this._authPrompt.addActorToDefaultButtonWell(this._sessionMenuButton.actor);

   },

    _updateDisableUserList: function() {
        let disableUserList = this._settings.get_boolean(GdmUtil.DISABLE_USER_LIST_KEY);

        if (disableUserList != this._disableUserList) {
            this._disableUserList = disableUserList;

            if (this._authPrompt.verificationStatus == AuthPrompt.AuthPromptStatus.NOT_VERIFYING)
                this._authPrompt.reset();
        }
    },

    _updateCancelButton: function() {
        let cancelVisible;

        // Hide the cancel button if the user list is disabled and we're asking for
        // a username
        if (this._authPrompt.verificationStatus == AuthPrompt.AuthPromptStatus.NOT_VERIFYING && this._disableUserList)
            cancelVisible = false;
        else
            cancelVisible = true;

        this._authPrompt.cancelButton.visible = cancelVisible;
    },

    _updateBanner: function() {
        let enabled = this._settings.get_boolean(GdmUtil.BANNER_MESSAGE_KEY);
        let text = this._settings.get_string(GdmUtil.BANNER_MESSAGE_TEXT_KEY);

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

        this._logoBin.destroy_all_children();
        if (this._logoFileUri)
            this._logoBin.add_child(this._textureCache.load_uri_async(this._logoFileUri,
                                                                      -1, _LOGO_ICON_HEIGHT));
    },

    _updateLogo: function() {
        let path = this._settings.get_string(GdmUtil.LOGO_KEY);

        this._logoFileUri = path ? Gio.file_new_for_path(path).get_uri() : null;
        this._updateLogoTexture(this._textureCache, this._logoFileUri);
    },

    _onPrompted: function() {
        this._sessionMenuButton.updateSensitivity(true);

        if (this._shouldShowSessionMenuButton())
            this._authPrompt.setActorInDefaultButtonWell(this._sessionMenuButton.actor);
        this._showPrompt();
    },

    _onReset: function(authPrompt, beginRequest) {
        this._sessionMenuButton.updateSensitivity(true);

        this._user = null;

        if (beginRequest == AuthPrompt.BeginRequestType.PROVIDE_USERNAME) {
            if (!this._disableUserList)
                this._showUserList();
            else
                this._hideUserListAskForUsernameAndBeginVerification();
        } else {
            this._hideUserListAndBeginVerification();
        }
    },

    _onDefaultSessionChanged: function(client, sessionId) {
        this._sessionMenuButton.setActiveSession(sessionId);
    },

    _shouldShowSessionMenuButton: function() {
        if (this._authPrompt.verificationStatus != AuthPrompt.AuthPromptStatus.VERIFYING &&
            this._authPrompt.verificationStatus != AuthPrompt.AuthPromptStatus.VERIFICATION_FAILED)
          return false;

        if (this._user && this._user.is_loaded && this._user.is_logged_in())
          return false;

        return true;
    },

    _showPrompt: function() {
        if (this._authPrompt.actor.visible)
            return;
        this._authPrompt.actor.opacity = 0;
        this._authPrompt.actor.show();
        Tweener.addTween(this._authPrompt.actor,
                         { opacity: 255,
                           time: _FADE_ANIMATION_TIME,
                           transition: 'easeOutQuad' });
    },

    _showRealmLoginHint: function(realmManager, hint) {
        if (!hint)
            return;

        hint = hint.replace(/%U/g, 'user');
        hint = hint.replace(/%D/g, 'DOMAIN');
        hint = hint.replace(/%[^UD]/g, '');

        // Translators: this message is shown below the username entry field
        // to clue the user in on how to login to the local network realm
        this._authPrompt.setMessage(_("(e.g., user or %s)").format(hint), GdmUtil.MessageType.HINT);
    },

    _askForUsernameAndBeginVerification: function() {
        this._authPrompt.setPasswordChar('');
        this._authPrompt.setQuestion(_("Username: "));

        let realmManager = new Realmd.Manager();
        let realmSignalId = realmManager.connect('login-format-changed',
                                                 Lang.bind(this, this._showRealmLoginHint));
        this._showRealmLoginHint(realmManager.loginFormat);

        let nextSignalId = this._authPrompt.connect('next',
                                                    Lang.bind(this, function() {
                                                        this._authPrompt.disconnect(nextSignalId);
                                                        this._authPrompt.updateSensitivity(false);
                                                        let answer = this._authPrompt.getAnswer();
                                                        this._user = this._userManager.get_user(answer);
                                                        this._authPrompt.clear();
                                                        this._authPrompt.startSpinning();
                                                        this._authPrompt.begin({ userName: answer });
                                                        this._updateCancelButton();

                                                        realmManager.disconnect(realmSignalId)
                                                        realmManager.release();
                                                    }));
        this._updateCancelButton();
        this._showPrompt();
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
                                   return GLib.SOURCE_REMOVE;
                               }));
                           },
                           onCompleteScope: this });
    },

    _onSessionOpened: function(client, serviceName) {
        this._authPrompt.finish(Lang.bind(this, function() {
            this._startSession(serviceName);
        }));
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
                                                                         return GLib.SOURCE_REMOVE;
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
                                    return Clutter.EVENT_PROPAGATE;

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

                                return Clutter.EVENT_PROPAGATE;
                             }));
    },

    _setUserListExpanded: function(expanded) {
        this._userList.updateStyle(expanded);
        this._userSelectionBox.visible = expanded;
    },

    _hideUserList: function() {
        this._setUserListExpanded(false);
        if (this._userSelectionBox.visible)
            GdmUtil.cloneAndFadeOutActor(this._userSelectionBox);
    },

    _hideUserListAskForUsernameAndBeginVerification: function() {
        this._hideUserList();
        this._askForUsernameAndBeginVerification();
    },

    _hideUserListAndBeginVerification: function() {
        this._hideUserList();
        this._authPrompt.begin();
    },

    _showUserList: function() {
        this._authPrompt.hide();
        this._sessionMenuButton.close();
        this._setUserListExpanded(true);
        this._notListedButton.show();
        this._userList.actor.grab_key_focus();
    },

    _beginVerificationForItem: function(item) {
        this._authPrompt.setUser(item.user);

        let userName = item.user.get_user_name();
        let hold = new Batch.Hold();

        this._authPrompt.begin({ userName: userName,
                                 hold: hold });
        return hold;
    },

    _onUserListActivated: function(activatedItem) {
        let tasks = [function() {
                         return GdmUtil.cloneAndFadeOutActor(this._userSelectionBox);
                     },
                     function() {
                         this._setUserListExpanded(false);
                     }];

        this._user = activatedItem.user;

        this._updateCancelButton();

        let batch = new Batch.ConcurrentBatch(this, [new Batch.ConsecutiveBatch(this, tasks),
                                                     this._beginVerificationForItem(activatedItem)]);
        batch.run();
    },

    _onDestroy: function() {
        if (this._userManagerLoadedId) {
            this._userManager.disconnect(this._userManagerLoadedId);
            this._userManagerLoadedId = 0;
        }
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

    open: function() {
        Main.ctrlAltTabManager.addGroup(this.actor,
                                        _("Login Window"),
                                        'dialog-password-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.MIDDLE });
        this._userList.actor.grab_key_focus();
        this.actor.show();
        this.actor.opacity = 0;

        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: 1,
                           transition: 'easeInQuad' });

        return true;
    },

    close: function() {
        Main.ctrlAltTabManager.removeGroup(this.dialogLayout);
    },

    cancel: function() {
        this._authPrompt.cancel();
    },

    addCharacter: function(unichar) {
        this._authPrompt.addCharacter(unichar);
    },

    finish: function(onComplete) {
        this._authPrompt.finish(onComplete);
    },
});
Signals.addSignalMethods(LoginDialog.prototype);
