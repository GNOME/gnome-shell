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
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gdm = imports.gi.Gdm;

const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const GdmUtil = imports.gdm.util;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const Panel = imports.ui.panel;
const PanelMenu = imports.ui.panelMenu;
const Tweener = imports.ui.tweener;
const UserMenu = imports.ui.userMenu;

const _RESIZE_ANIMATION_TIME = 0.25;
const _SCROLL_ANIMATION_TIME = 0.5;
const _TIMED_LOGIN_IDLE_THRESHOLD = 5.0;
const _LOGO_ICON_HEIGHT = 16;

const WORK_SPINNER_ICON_SIZE = 24;
const WORK_SPINNER_ANIMATION_DELAY = 1.0;
const WORK_SPINNER_ANIMATION_TIME = 0.3;

let _loginDialog = null;

function _smoothlyResizeActor(actor, width, height) {
    let finalWidth;
    let finalHeight;

    if (width < 0)
        finalWidth = actor.width;
    else
        finalWidth = width;

    if (height < 0)
        finalHeight = actor.height;
    else
        finalHeight = height;

    actor.set_size(actor.width, actor.height);

    if (actor.width == finalWidth && actor.height == finalHeight)
        return null;

    let hold = new Batch.Hold();

    Tweener.addTween(actor,
                     { width: finalWidth,
                       height: finalHeight,
                       time: _RESIZE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: Lang.bind(this, function() {
                                       hold.release();
                                   })
                     });
    return hold;
}

const LogoMenuButton = new Lang.Class({
    Name: 'LogoMenuButton',
    Extends: PanelMenu.Button,

    _init: function() {
        this.parent(0.0, null, true);

        this._settings = new Gio.Settings({ schema: GdmUtil.LOGIN_SCREEN_SCHEMA });
        this._settings.connect('changed::' + GdmUtil.LOGO_KEY,
                               Lang.bind(this, this._updateLogo));

        this._iconBin = new St.Bin();
        this.actor.add_actor(this._iconBin);

        this._updateLogo();
    },

    _updateLogo: function() {
        let path = this._settings.get_string(GdmUtil.LOGO_KEY);
        let icon = null;

        if (path) {
            let file = Gio.file_new_for_path(path);
            let cache = St.TextureCache.get_default();
            icon = cache.load_uri_async(file.get_uri(), -1, _LOGO_ICON_HEIGHT);
        }
        this._iconBin.set_child(icon);
    }
});

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

        this._userAvatar = new UserMenu.UserAvatarWidget(this.user,
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

    _showItem: function(item) {
        let tasks = [function() {
                         return GdmUtil.fadeInActor(item.actor);
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        return batch.run();
    },

    _onItemActivated: function(activatedItem) {
        this.emit('activate', activatedItem);
    },

    giveUpWhitespace: function() {
        let container = this.actor.get_parent();

        container.child_set(this.actor, { expand: false });
    },

    takeOverWhitespace: function() {
        let container = this.actor.get_parent();

        container.child_set(this.actor, { expand: true });
    },

    pinInPlace: function() {
        this._box.set_size(this._box.width, this._box.height);
    },

    shrinkToNaturalHeight: function() {
        let oldWidth = this._box.width;
        let oldHeight = this._box.height;
        this._box.set_size(-1, -1);
        let [minHeight, naturalHeight] = this._box.get_preferred_height(-1);
        this._box.set_size(oldWidth, oldHeight);

        let batch = new Batch.ConsecutiveBatch(this,
                                               [function() {
                                                    return _smoothlyResizeActor(this._box, -1, naturalHeight);
                                                },

                                                function() {
                                                    this._box.set_size(-1, -1);
                                                }
                                               ]);

        return batch.run();
    },

    hideItemsExcept: function(exception) {
        let tasks = [];

        for (let userName in this._items) {
            let item = this._items[userName];

            item.actor.set_hover(false);
            item.actor.reactive = false;
            item.actor.can_focus = false;
            item.syncStyleClasses();
            item._timedLoginIndicator.scale_x = 0.;
            if (item != exception)
                tasks.push(function() {
                    return GdmUtil.fadeOutActor(item.actor);
                });
        }

        let batch = new Batch.ConsecutiveBatch(this,
                                               [function() {
                                                    return GdmUtil.fadeOutActor(this.actor.vscroll);
                                                },

                                                new Batch.ConcurrentBatch(this, tasks),

                                                function() {
                                                    this._box.remove_style_pseudo_class('expanded');
                                                }
                                               ]);

        return batch.run();
    },

    hideItems: function() {
        return this.hideItemsExcept(null);
    },

    _getExpandedHeight: function() {
        let hiddenActors = [];
        for (let userName in this._items) {
            let item = this._items[userName];
            if (!item.actor.visible) {
                item.actor.show();
                hiddenActors.push(item.actor);
            }
        }

        if (!this._box.visible) {
            this._box.show();
            hiddenActors.push(this._box);
        }

        this._box.set_size(-1, -1);
        let [minHeight, naturalHeight] = this._box.get_preferred_height(-1);

        for (let i = 0; i < hiddenActors.length; i++) {
            let actor = hiddenActors[i];
            actor.hide();
        }

        return naturalHeight;
    },

    showItems: function() {
        let tasks = [];

        for (let userName in this._items) {
            let item = this._items[userName];
            item.actor.sync_hover();
            item.actor.reactive = true;
            item.actor.can_focus = true;
            item.syncStyleClasses();
            tasks.push(function() {
                return this._showItem(item);
            });
        }

        let batch = new Batch.ConsecutiveBatch(this,
                                               [function() {
                                                    this.takeOverWhitespace();
                                                },

                                                function() {
                                                    let fullHeight = this._getExpandedHeight();
                                                    return _smoothlyResizeActor(this._box, -1, fullHeight);
                                                },

                                                function() {
                                                    this._box.add_style_pseudo_class('expanded');
                                                },

                                                new Batch.ConcurrentBatch(this, tasks),

                                                function() {
                                                    this.actor.set_size(-1, -1);
                                                },

                                                function() {
                                                    return GdmUtil.fadeInActor(this.actor.vscroll);
                                                }]);
        return batch.run();
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

        this._bannerLabel = new St.Label({ style_class: 'login-dialog-banner',
                                           text: '' });
        this.contentLayout.add(this._bannerLabel);
        this._updateBanner();

        this._titleLabel = new St.Label({ style_class: 'login-dialog-title',
                                          text: C_("title", "Sign In"),
                                          visible: false });

        this.contentLayout.add(this._titleLabel,
                              { y_fill: false,
                                y_align: St.Align.START });

        this._userList = new UserList();
        this.contentLayout.add(this._userList.actor,
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
        this._promptLabel = new St.Label({ style_class: 'login-dialog-prompt-label' });

        this._promptBox.add(this._promptLabel,
                            { expand: true,
                              x_fill: true,
                              y_fill: true,
                              x_align: St.Align.START });
        this._promptEntry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                           can_focus: true });
        this._promptEntryTextChangedId = 0;
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
        this._workSpinner = null;

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

        this.contentLayout.add(this._notListedButton,
                               { expand: false,
                                 x_align: St.Align.START,
                                 x_fill: true });

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
        let disableUserList = this._settings.get_boolean(GdmUtil.DISABLE_USER_LIST_KEY);

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
        let enabled = this._settings.get_boolean(GdmUtil.BANNER_MESSAGE_KEY);
        let text = this._settings.get_string(GdmUtil.BANNER_MESSAGE_TEXT_KEY);

        if (enabled && text) {
            this._bannerLabel.set_text(text);
            this._fadeInBanner();
        } else {
            this._fadeOutBanner();
        }
    },

    _reset: function() {
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
        this._setWorking(false);
    },

    _onDefaultSessionChanged: function(client, sessionId) {
        this._sessionList.setActiveSession(sessionId);
    },

    _showMessage: function(userVerifier, message, styleClass) {
        if (message) {
            this._promptMessage.text = message;
            this._promptMessage.styleClass = styleClass;
            GdmUtil.fadeInActor(this._promptMessage);
        } else {
            GdmUtil.fadeOutActor(this._promptMessage);
        }
    },

    _showLoginHint: function(verifier, message) {
        this._promptLoginHint.set_text(message)
        GdmUtil.fadeInActor(this._promptLoginHint);
    },

    _hideLoginHint: function() {
        GdmUtil.fadeOutActor(this._promptLoginHint);
        this._promptLoginHint.set_text('');
    },

    cancel: function() {
        this._userVerifier.cancel();
    },

    _fadeInPrompt: function() {
        let tasks = [function() {
                         return GdmUtil.fadeInActor(this._promptLabel);
                     },

                     function() {
                         return GdmUtil.fadeInActor(this._promptEntry);
                     },

                     function() {
                         // Show it with 0 opacity so we preallocate space for it
                         // in the event we need to fade in the message
                         this._promptLoginHint.opacity = 0;
                         this._promptLoginHint.show();
                     },

                     function() {
                         return GdmUtil.fadeInActor(this._promptBox);
                     },

                     function() {
                         if (this._user && this._user.is_logged_in())
                             return null;

                         if (!this._verifyingUser)
                             return null;

                         return GdmUtil.fadeInActor(this._sessionList.actor);
                     },

                     function() {
                         this._promptEntry.grab_key_focus();
                     }];

        this._sessionList.actor.hide();
        let batch = new Batch.ConcurrentBatch(this, tasks);
        return batch.run();
    },

    _showPrompt: function(forSecret) {
        let hold = new Batch.Hold();

        let tasks = [function() {
                         return this._fadeInPrompt();
                     },

                     function() {
                         this._prepareDialog(forSecret, hold);
                     },

                     hold];

        let batch = new Batch.ConcurrentBatch(this, tasks);

        return batch.run();
    },

    _prepareDialog: function(forSecret, hold) {
        this._workSpinner = new Panel.AnimatedIcon('process-working.svg', WORK_SPINNER_ICON_SIZE);
        this._workSpinner.actor.opacity = 0;
        this._workSpinner.actor.show();

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
        this.buttonLayout.add(this._workSpinner.actor,
                              { expand: false,
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

        let tasks = [function() {
                         this._setWorking(false);

                         return GdmUtil.fadeOutActor(this._promptBox);
                     },

                     function() {
                         this._promptLoginHint.hide();

                         this._updateSensitivity(true);
                         this._promptEntry.set_text('');

                         this.clearButtons();
                         this._workSpinner = null;
                         this._signInButton = null;
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);

        return batch.run();
    },

    _setWorking: function(working) {
        if (!this._workSpinner)
            return;

        if (working) {
            this._workSpinner.play();
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 255,
                               delay: WORK_SPINNER_ANIMATION_DELAY,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear'
                             });
        } else {
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 0,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear',
                               onCompleteScope: this,
                               onComplete: function() {
                                   this._workSpinner.stop();
                               }
                             });
        }
    },

    _askQuestion: function(verifier, serviceName, question, passwordChar) {
        this._promptLabel.set_text(question);

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

    _askForUsernameAndLogIn: function() {
        this._promptLabel.set_text(_("Username: "));
        this._promptEntry.set_text('');
        this._promptEntry.clutter_text.set_password_char('');

        let tasks = [this._showPrompt,

                     function() {
                         let userName = this._promptEntry.get_text();
                         this._promptEntry.reactive = false;
                         return this._beginVerificationForUser(userName);
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        return batch.run();
    },

    _onSessionOpened: function(client, serviceName) {
        this._greeter.call_start_session_when_ready_sync(serviceName, true, null);
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
                         if (!this.is_loaded) {
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

    _hideUserListAndLogIn: function() {
        let tasks = [function() {
                         return this._userList.hideItems();
                     },

                     function() {
                         return this._userList.giveUpWhitespace();
                     },

                     function() {
                         this._userList.actor.hide();
                     },

                     new Batch.ConcurrentBatch(this, [this._fadeOutTitleLabel,
                                                      this._fadeOutNotListedButton]),

                     function() {
                         return this._askForUsernameAndLogIn();
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        batch.run();
    },

    _showUserList: function() {
        let tasks = [this._hidePrompt,

                     new Batch.ConcurrentBatch(this, [this._fadeInTitleLabel,
                                                      this._fadeInNotListedButton]),

                     function() {
                         this._sessionList.close();
                         this._promptLoginHint.hide();
                         this._userList.actor.show();
                         this._userList.actor.opacity = 255;
                         return this._userList.showItems();
                     },

                     function() {
                         this._userList.actor.reactive = true;
                         this._userList.actor.grab_key_focus();
                     }];

        let batch = new Batch.ConsecutiveBatch(this, tasks);
        batch.run();
    },

    _fadeInBanner: function() {
        return GdmUtil.fadeInActor(this._bannerLabel);
    },

    _fadeOutBanner: function() {
        return GdmUtil.fadeOutActor(this._bannerLabel);
    },

    _fadeInTitleLabel: function() {
        return GdmUtil.fadeInActor(this._titleLabel);
    },

    _fadeOutTitleLabel: function() {
        return GdmUtil.fadeOutActor(this._titleLabel);
    },

    _fadeInNotListedButton: function() {
        return GdmUtil.fadeInActor(this._notListedButton);
    },

    _fadeOutNotListedButton: function() {
        return GdmUtil.fadeOutActor(this._notListedButton);
    },

    _beginVerificationForUser: function(userName) {
        let hold = new Batch.Hold();

        this._userVerifier.begin(userName, hold);
        this._verifyingUser = true;
        return hold;
    },

    _onUserListActivated: function(activatedItem) {
        let userName;

        let tasks = [function() {
                         this._userList.actor.reactive = false;
                         return this._userList.pinInPlace();
                     },

                     function() {
                         return this._userList.hideItemsExcept(activatedItem);
                     },

                     function() {
                         return this._userList.giveUpWhitespace();
                     },

                     new Batch.ConcurrentBatch(this, [this._fadeOutTitleLabel,
                                                      this._fadeOutNotListedButton]),

                     function() {
                         return this._userList.shrinkToNaturalHeight();
                     },

                     function() {
                         userName = activatedItem.user.get_user_name();

                         return this._beginVerificationForUser(userName);
                     }];

        this._user = activatedItem.user;

        let batch = new Batch.ConsecutiveBatch(this, tasks);
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

        // emitted in idle so caller doesn't have to explicitly check if
        // it's loaded immediately after construction
        // (since there's no way the caller could be listening for
        // 'loaded' yet)
        Mainloop.idle_add(Lang.bind(this, function() {
            this.emit('loaded');
            this.is_loaded = true;
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
    }
});
