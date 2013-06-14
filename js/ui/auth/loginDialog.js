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

const AuthUtil = imports.ui.auth.util;
const Batch = imports.misc.batch;
const Fprint = imports.ui.auth.fingerprint;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const Tweener = imports.ui.tweener;
const UserAvatar = imports.ui.userAvatar;
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

        let layout = new St.BoxLayout({ vertical: false });
        this.actor = new St.Button({ style_class: 'login-dialog-user-list-item',
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                     can_focus: true,
                                     child: layout,
                                     reactive: true,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._userAvatar = new UserAvatar.UserAvatar(this.user,
                                                     { styleClass: 'login-dialog-user-list-item-icon' });
        layout.add(this._userAvatar.actor);
        let textLayout = new St.BoxLayout({ style_class: 'login-dialog-user-list-item-text-box',
                                            vertical:    true });
        layout.add(textLayout, { expand: true });

        this._nameLabel = new St.Label({ style_class: 'login-dialog-user-list-item-name' });
        this.actor.label_actor = this._nameLabel;
        textLayout.add(this._nameLabel,
                       { y_fill: false,
                         y_align: St.Align.MIDDLE,
                         expand: true });

        this._timedLoginIndicator = new St.Bin({ style_class: 'login-dialog-timed-login-indicator',
                                                 scale_x: 0 });
        textLayout.add(this._timedLoginIndicator,
                       { x_fill: true,
                         x_align: St.Align.MIDDLE,
                         y_fill: false,
                         y_align: St.Align.END });

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this._onUserChanged();
    },

    _onUserChanged: function() {
        this._nameLabel.set_text(this.user.get_real_name());
        this._userAvatar.update();
        this._updateLoggedIn();
    },

    syncStyleClasses: function() {
        this._updateLoggedIn();

        if (global.stage.get_key_focus() == this.actor)
            this.actor.add_style_pseudo_class('focus');
        else
            this.actor.remove_style_pseudo_class('focus');
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
            item.syncStyleClasses();
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

const SessionListItem = new Lang.Class({
    Name: 'SessionListItem',

    _init: function(id, name) {
        this.id = id;

        this.actor = new St.Button({ style_class: 'login-dialog-session-list-item',
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                     can_focus: true,
                                     reactive: true,
                                     x_fill: true,
                                     x_align: St.Align.START });

        this._box = new St.BoxLayout({ style_class: 'login-dialog-session-list-item-box' });

        this.actor.add_actor(this._box);
        this.actor.connect('clicked', Lang.bind(this, this._onClicked));

        this._dot = new St.DrawingArea({ style_class: 'login-dialog-session-list-item-dot' });
        this._dot.connect('repaint', Lang.bind(this, this._onRepaintDot));
        this._box.add_actor(this._dot);
        this.setShowDot(false);

        let label = new St.Label({ style_class: 'login-dialog-session-list-item-label',
                                   text: name });
        this.actor.label_actor = label;

        this._box.add_actor(label);
    },

    setShowDot: function(show) {
        if (show)
            this._dot.opacity = 255;
        else
            this._dot.opacity = 0;
    },

    _onRepaintDot: function(area) {
        let cr = area.get_context();
        let [width, height] = area.get_surface_size();
        let color = area.get_theme_node().get_foreground_color();

        cr.setSourceRGBA (color.red / 255,
                          color.green / 255,
                          color.blue / 255,
                          color.alpha / 255);
        cr.arc(width / 2, height / 2, width / 3, 0, 2 * Math.PI);
        cr.fill();
        cr.$dispose();
    },

    _onClicked: function() {
        this.emit('activate');
    }
});
Signals.addSignalMethods(SessionListItem.prototype);

const SessionList = new Lang.Class({
    Name: 'SessionList',

    _init: function() {
        this.actor = new St.Bin();

        this._box = new St.BoxLayout({ style_class: 'login-dialog-session-list',
                                       vertical: true});
        this.actor.child = this._box;

        this._button = new St.Button({ style_class: 'login-dialog-session-list-button',
                                       button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                       can_focus: true,
                                       x_fill: true,
                                       y_fill: true });
        let box = new St.BoxLayout();
        this._button.add_actor(box);

        this._triangle = new St.Label({ style_class: 'login-dialog-session-list-triangle',
                                        text: '\u25B8' });
        box.add_actor(this._triangle);

        let label = new St.Label({ style_class: 'login-dialog-session-list-label',
                                   text: _("Session…") });
        box.add_actor(label);

        this._button.connect('clicked',
                             Lang.bind(this, this._onClicked));
        this._box.add_actor(this._button);
        this._scrollView = new St.ScrollView({ style_class: 'login-dialog-session-list-scroll-view'});
        this._scrollView.set_policy(Gtk.PolicyType.NEVER,
                                    Gtk.PolicyType.AUTOMATIC);
        this._box.add_actor(this._scrollView);
        this._itemList = new St.BoxLayout({ style_class: 'login-dialog-session-item-list',
                                            vertical: true });
        this._scrollView.add_actor(this._itemList);
        this._scrollView.hide();
        this.isOpen = false;
        this._populate();
    },

    open: function() {
        if (this.isOpen)
            return;

        this._button.add_style_pseudo_class('open');
        this._scrollView.show();
        this._triangle.set_text('\u25BE');

        this.isOpen = true;
    },

    close: function() {
        if (!this.isOpen)
            return;

        this._button.remove_style_pseudo_class('open');
        this._scrollView.hide();
        this._triangle.set_text('\u25B8');

        this.isOpen = false;
    },

    _onClicked: function() {
        if (!this.isOpen)
            this.open();
        else
            this.close();
    },

    updateSensitivity: function(sensitive) {
        this._button.reactive = sensitive;
        this._button.can_focus = sensitive;

        for (let id in this._items)
            this._items[id].actor.reactive = sensitive;
    },

    setActiveSession: function(sessionId) {
         if (sessionId == this._activeSessionId)
             return;

         if (this._activeSessionId)
             this._items[this._activeSessionId].setShowDot(false);

         this._items[sessionId].setShowDot(true);
         this._activeSessionId = sessionId;

         this.emit('session-activated', this._activeSessionId);
    },

    _populate: function() {
        this._itemList.destroy_all_children();
        this._activeSessionId = null;
        this._items = {};

        let ids = Gdm.get_session_ids();
        ids.sort();

        if (ids.length <= 1) {
            this._box.hide();
            this._button.hide();
        } else {
            this._button.show();
            this._box.show();
        }

        for (let i = 0; i < ids.length; i++) {
            let [sessionName, sessionDescription] = Gdm.get_session_name_and_description(ids[i]);

            let item = new SessionListItem(ids[i], sessionName);
            this._itemList.add_actor(item.actor);
            this._items[ids[i]] = item;

            if (!this._activeSessionId)
                this.setActiveSession(ids[i]);

            item.connect('activate',
                         Lang.bind(this, function() {
                             this.setActiveSession(item.id);
                         }));
        }
    }
});
Signals.addSignalMethods(SessionList.prototype);

const LoginDialog = new Lang.Class({
    Name: 'LoginDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(parentActor) {
        this.parent({ shellReactive: true,
                      styleClass: 'login-dialog',
                      parentActor: parentActor,
                      keybindingMode: Shell.KeyBindingMode.LOGIN_SCREEN,
                      shouldFadeIn: false });
        this.connect('destroy',
                     Lang.bind(this, this._onDestroy));
        this.connect('opened',
                     Lang.bind(this, this._onOpened));

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
        this.contentLayout.add(this._userSelectionBox);

        this._bannerLabel = new St.Label({ style_class: 'login-dialog-banner',
                                           text: '' });
        this._userSelectionBox.add(this._bannerLabel);
        this._updateBanner();

        this._userList = new UserList();
        this._userSelectionBox.add(this._userList.actor,
                                   { expand: true,
                                     x_fill: true,
                                     y_fill: true });

        this.setInitialKeyFocus(this._userList.actor);

        this._promptBox = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                             vertical: true });
        this.contentLayout.add(this._promptBox,
                               { expand: true,
                                 x_fill: true,
                                 y_fill: true,
                                 x_align: St.Align.START });
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
                            { expand: true,
                              x_fill: true,
                              y_fill: false,
                              x_align: St.Align.START });

        this._promptMessage = new St.Label({ visible: false });
        this._promptBox.add(this._promptMessage, { x_fill: true });

        this._promptLoginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint-message' });
        this._promptLoginHint.hide();
        this._promptBox.add(this._promptLoginHint);

        this._signInButton = null;

        this._sessionList = new SessionList();
        this._sessionList.connect('session-activated',
                                  Lang.bind(this, function(list, sessionId) {
                                                this._greeter.call_select_session_sync (sessionId, null);
                                            }));

        this._promptBox.add(this._sessionList.actor,
                            { expand: true,
                              x_fill: false,
                              y_fill: true,
                              x_align: St.Align.START });
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
        this.backgroundStack.add_actor(this._logoBin);
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

    _verificationFailed: function() {
        this._promptEntry.text = '';

        this._updateSensitivity(true);
        this.setWorking(false);
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
        this.buttonLayout.visible = true;
        this.clearButtons();

        if (!this._disableUserList || this._verifyingUser)
            this.addButton({ action: Lang.bind(this, this.cancel),
                             label: _("Cancel"),
                             key: Clutter.Escape },
                           { expand: true,
                             x_fill: false,
                             y_fill: false,
                             x_align: St.Align.START,
                             y_align: St.Align.MIDDLE });
        this.placeSpinner({ expand: false,
                            x_fill: false,
                            y_fill: false,
                            x_align: St.Align.END,
                            y_align: St.Align.MIDDLE });
        this._signInButton = this.addButton({ action: Lang.bind(this, function() {
                                                          hold.release();
                                                      }),
                                              label: forSecret ? C_("button", "Sign In") : _("Next"),
                                              default: true },
                                            { expand: false,
                                              x_fill: false,
                                              y_fill: false,
                                              x_align: St.Align.END,
                                              y_align: St.Align.MIDDLE });

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
        this.setButtons([]);

        if (this._promptEntryTextChangedId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryTextChangedId);
            this._promptEntryTextChangedId = 0;
        }

        if (this._promptEntryActivateId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryActivateId);
            this._promptEntryActivateId = 0;
        }

        this.setWorking(false);
        this._promptBox.hide();
        this._promptLoginHint.hide();

        this._promptUser.set_child(null);

        this._updateSensitivity(true);
        this._promptEntry.set_text('');

        this._sessionList.close();
        this._promptLoginHint.hide();

        this.clearButtons();
        this._signInButton = null;
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
                         this.setWorking(true);
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
        Tweener.addTween(this.dialogLayout,
                         { opacity: 0,
                           time: _FADE_ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onUpdate: function() {
                               let children = Main.layoutManager.uiGroup.get_children();

                               for (let i = 0; i < children.length; i++) {
                                   if (children[i] != Main.layoutManager.screenShieldGroup)
                                       children[i].opacity = this.dialogLayout.opacity;
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

    _onOpened: function() {
        Main.ctrlAltTabManager.addGroup(this.dialogLayout,
                                        _("Login Window"),
                                        'dialog-password-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.MIDDLE });

    },

    close: function() {
        this.parent();

        Main.ctrlAltTabManager.removeGroup(this.dialogLayout);
    },

    addCharacter: function(unichar) {
        this._promptEntry.clutter_text.insert_unichar(unichar);
    },
});
