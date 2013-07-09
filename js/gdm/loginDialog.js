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

const Animation = imports.ui.animation;
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
const _DEFAULT_BUTTON_WELL_ICON_SIZE = 24;
const _DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1.0;
const _DEFAULT_BUTTON_WELL_ANIMATION_TIME = 0.3;
const _TIMED_LOGIN_IDLE_THRESHOLD = 5.0;
const _LOGO_ICON_HEIGHT = 48;

let _loginDialog = null;

const DefaultButtonWellMode = {
    NONE: 0,
    SESSION_MENU_BUTTON: 1,
    SPINNER: 2
};

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

        this._userAvatar = new UserWidget.Avatar(this.user,
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

        let subtitle = new PopupMenu.PopupMenuItem(_("Session"), { style_class: 'popup-subtitle-menu-item',
                                                                   reactive: false });
        this._menu.addMenuItem(subtitle);

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
                                     style_class: 'login-dialog',
                                     visible: false });

        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        parentActor.add_child(this.actor);

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

        this._userVerifier = new GdmUtil.ShellUserVerifier(this._greeterClient);
        this._userVerifier.connect('ask-question', Lang.bind(this, this._askQuestion));
        this._userVerifier.connect('show-message', Lang.bind(this, this._showMessage));
        this._userVerifier.connect('verification-failed', Lang.bind(this, this._verificationFailed));
        this._userVerifier.connect('reset', Lang.bind(this, this._reset));
        this._userVerifier.connect('show-login-hint', Lang.bind(this, this._showLoginHint));
        this._userVerifier.connect('hide-login-hint', Lang.bind(this, this._hideLoginHint));
        this._verifyingUser = false;

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
                                                    vertical: true });
        this._userSelectionBox.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                            align_axis: Clutter.AlignAxis.BOTH,
                                                                            factor: 0.5 }));
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

        this._promptBox = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                             vertical: true });

        this._promptBox.connect('button-press-event',
                                 Lang.bind(this, function(actor, event) {
                                    if (event.get_key_symbol() == Clutter.KEY_Escape) {
                                        this.cancel();
                                    }
                                 }));

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
                            { expand: true,
                              x_fill: true,
                              y_fill: false,
                              x_align: St.Align.START });

        this._promptEntry.grab_key_focus();

        this._promptMessage = new St.Label({ visible: false });
        this._promptBox.add(this._promptMessage, { x_fill: true });

        this._promptLoginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint-message' });
        this._promptLoginHint.hide();
        this._promptBox.add(this._promptLoginHint);

        this._buttonBox = new St.BoxLayout({ style_class: 'modal-dialog-button-box',
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
        this._notListedButton.hide();

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

        this._defaultButtonWell = new St.Widget();
        this._defaultButtonWellMode = DefaultButtonWellMode.NONE;

        this._sessionMenuButton = new SessionMenuButton();
        this._sessionMenuButton.connect('session-activated',
                                  Lang.bind(this, function(list, sessionId) {
                                                this._greeter.call_select_session_sync (sessionId, null);
                                            }));
        this._sessionMenuButton.actor.opacity = 0;
        this._sessionMenuButton.actor.show();
        this._defaultButtonWell.add_child(this._sessionMenuButton.actor);

        let spinnerIcon = global.datadir + '/theme/process-working.svg';
        this._workSpinner = new Animation.AnimatedIcon(spinnerIcon, _DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._workSpinner.actor.opacity = 0;
        this._workSpinner.actor.show();

        this._defaultButtonWell.add_child(this._workSpinner.actor);
        this._sessionMenuButton.actor.add_constraint(new Clutter.AlignConstraint({ source: this._workSpinner.actor,
                                                                                   align_axis: Clutter.AlignAxis.BOTH,
                                                                                   factor: 0.5 }));
   },

    _updateDisableUserList: function() {
        let disableUserList = this._settings.get_boolean(GdmUtil.DISABLE_USER_LIST_KEY);

        if (disableUserList != this._disableUserList) {
            this._disableUserList = disableUserList;

            if (!this._verifyingUser)
                this._reset();
        }
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

        let icon = null;
        if (this._logoFileUri)
            icon = this._textureCache.load_uri_async(this._logoFileUri,
                                                     -1, _LOGO_ICON_HEIGHT);
        this._logoBin.set_child(icon);
    },

    _updateLogo: function() {
        let path = this._settings.get_string(GdmUtil.LOGO_KEY);

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

    _getActorForDefaultButtonWellMode: function(mode) {
        let actor;

        if (mode == DefaultButtonWellMode.NONE)
            actor = null;
        else if (mode == DefaultButtonWellMode.SPINNER)
            actor = this._workSpinner.actor;
        else if (mode == DefaultButtonWellMode.SESSION_MENU_BUTTON)
            actor = this._sessionMenuButton.actor;

        return actor;
    },

    _setDefaultButtonWellMode: function(mode, immediately) {
        if (this._defaultButtonWellMode == DefaultButtonWellMode.NONE &&
            mode == DefaultButtonWellMode.NONE)
            return;

        let oldActor = this._getActorForDefaultButtonWellMode(this._defaultButtonWellMode);

        if (oldActor)
            Tweener.removeTweens(oldActor);

        let actor = this._getActorForDefaultButtonWellMode(mode);

        if (this._defaultButtonWellMode != mode && oldActor) {
            if (immediately)
                oldActor.opacity = 0;
            else
                Tweener.addTween(oldActor,
                                 { opacity: 0,
                                   time: _DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: _DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear',
                                   onCompleteScope: this,
                                   onComplete: function() {
                                       if (mode == DefaultButtonWellMode.SPINNER) {
                                           if (this._workSpinner)
                                               this._workSpinner.stop();
                                       }
                                   }
                                 });

        }

        if (actor) {
            if (mode == DefaultButtonWellMode.SPINNER)
                this._workSpinner.play();

            if (immediately)
                actor.opacity = 255;
            else
                Tweener.addTween(actor,
                                 { opacity: 255,
                                   time: _DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: _DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear' });
        }

        this._defaultButtonWellMode = mode;
    },

    _verificationFailed: function() {
        this._promptEntry.text = '';

        this._updateSensitivity(true);
        this._setDefaultButtonWellMode(DefaultButtonWellMode.NONE, true);
    },

    _onDefaultSessionChanged: function(client, sessionId) {
        this._sessionMenuButton.setActiveSession(sessionId);
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

    _shouldShowSessionMenuButton: function() {
        if (this._verifyingUser)
          return true;

        if (!this._user)
          return false;

        if (this._user.is_logged_in)
          return false;

        return true;
    },

    _showPrompt: function(forSecret) {
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

        if (this._shouldShowSessionMenuButton())
            this._setDefaultButtonWellMode(DefaultButtonWellMode.SESSION_MENU_BUTTON, true);
        else
            this._setDefaultButtonWellMode(DefaultButtonWellMode.NONE, true);

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
        this._buttonBox.remove_all_children();

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
            this._buttonBox.add(this._cancelButton,
                                { expand: false,
                                  x_fill: false,
                                  y_fill: false,
                                  x_align: St.Align.START,
                                  y_align: St.Align.END });
        }

        this._buttonBox.add(this._defaultButtonWell,
                            { expand: true,
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
                            { expand: false,
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
        this._sessionMenuButton.updateSensitivity(sensitive);
        this._updateSignInButtonSensitivity(sensitive);
    },

    _updateSignInButtonSensitivity: function(sensitive) {
        if (this._signInButton) {
            this._signInButton.reactive = sensitive;
            this._signInButton.can_focus = sensitive;
        }
    },

    _hidePrompt: function() {
        if (this._promptEntryTextChangedId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryTextChangedId);
            this._promptEntryTextChangedId = 0;
        }

        if (this._promptEntryActivateId > 0) {
            this._promptEntry.clutter_text.disconnect(this._promptEntryActivateId);
            this._promptEntryActivateId = 0;
        }

        this._setDefaultButtonWellMode(DefaultButtonWellMode.NONE, true);
        this._promptBox.hide();
        this._promptLoginHint.hide();

        this._promptUser.set_child(null);

        this._updateSensitivity(true);
        this._promptEntry.set_text('');

        this._sessionMenuButton.close();
        this._promptLoginHint.hide();

        this._buttonBox.remove_all_children();
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
                         this._setDefaultButtonWellMode(DefaultButtonWellMode.SPINNER, false);
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
        GdmUtil.cloneAndFadeOutActor(this._userSelectionBox);
        this._askForUsernameAndLogIn();
    },

    _showUserList: function() {
        this._hidePrompt();
        this._setUserListExpanded(true);
        this._notListedButton.show();
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
                         return GdmUtil.cloneAndFadeOutActor(this._userSelectionBox);
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

    open: function() {
        Main.ctrlAltTabManager.addGroup(this.actor,
                                        _("Login Window"),
                                        'dialog-password-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.MIDDLE });
        this._userList.actor.grab_key_focus();
        this.actor.show();

        return true;
    },

    close: function() {
        Main.ctrlAltTabManager.removeGroup(this.dialogLayout);
    },

    addCharacter: function(unichar) {
        this._promptEntry.clutter_text.insert_unichar(unichar);
    },
});
Signals.addSignalMethods(LoginDialog.prototype);
