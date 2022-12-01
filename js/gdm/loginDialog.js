// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported LoginDialog */
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

const {
    AccountsService, Atk, Clutter, Gdm, Gio,
    GLib, GObject, Meta, Pango, Shell, St,
} = imports.gi;

const AuthPrompt = imports.gdm.authPrompt;
const Batch = imports.gdm.batch;
const BoxPointer = imports.ui.boxpointer;
const CtrlAltTab = imports.ui.ctrlAltTab;
const GdmUtil = imports.gdm.util;
const Layout = imports.ui.layout;
const LoginManager = imports.misc.loginManager;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Realmd = imports.gdm.realmd;
const UserWidget = imports.ui.userWidget;

const _FADE_ANIMATION_TIME = 250;
const _SCROLL_ANIMATION_TIME = 500;
const _TIMED_LOGIN_IDLE_THRESHOLD = 5.0;

var UserListItem = GObject.registerClass({
    Signals: { 'activate': {} },
}, class UserListItem extends St.Button {
    _init(user) {
        let layout = new St.BoxLayout({
            vertical: true,
        });
        super._init({
            style_class: 'login-dialog-user-list-item',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            x_expand: true,
            child: layout,
            reactive: true,
        });

        this.user = user;
        this.user.connectObject('changed', this._onUserChanged.bind(this), this);

        this.connect('notify::hover', () => {
            this._setSelected(this.hover);
        });

        this._userWidget = new UserWidget.UserWidget(this.user);
        layout.add(this._userWidget);

        this._userWidget.bind_property('label-actor', this, 'label-actor',
                                       GObject.BindingFlags.SYNC_CREATE);

        this._timedLoginIndicator = new St.Bin({
            style_class: 'login-dialog-timed-login-indicator',
            scale_x: 0,
            visible: false,
        });
        layout.add(this._timedLoginIndicator);

        this._onUserChanged();
    }

    vfunc_key_focus_in() {
        super.vfunc_key_focus_in();
        this._setSelected(true);
    }

    vfunc_key_focus_out() {
        super.vfunc_key_focus_out();
        this._setSelected(false);
    }

    _onUserChanged() {
        this._updateLoggedIn();
    }

    _updateLoggedIn() {
        if (this.user.is_logged_in())
            this.add_style_pseudo_class('logged-in');
        else
            this.remove_style_pseudo_class('logged-in');
    }

    vfunc_clicked() {
        this.emit('activate');
    }

    _setSelected(selected) {
        if (selected) {
            this.add_style_pseudo_class('selected');
            this.grab_key_focus();
        } else {
            this.remove_style_pseudo_class('selected');
        }
    }

    showTimedLoginIndicator(time) {
        let hold = new Batch.Hold();

        this.hideTimedLoginIndicator();

        this._timedLoginIndicator.visible = true;

        let startTime = GLib.get_monotonic_time();

        this._timedLoginTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 33,
            () => {
                let currentTime = GLib.get_monotonic_time();
                let elapsedTime = (currentTime - startTime) / GLib.USEC_PER_SEC;
                this._timedLoginIndicator.scale_x = elapsedTime / time;
                if (elapsedTime >= time) {
                    this._timedLoginTimeoutId = 0;
                    hold.release();
                    return GLib.SOURCE_REMOVE;
                }

                return GLib.SOURCE_CONTINUE;
            });

        GLib.Source.set_name_by_id(this._timedLoginTimeoutId, '[gnome-shell] this._timedLoginTimeoutId');

        return hold;
    }

    hideTimedLoginIndicator() {
        if (this._timedLoginTimeoutId) {
            GLib.source_remove(this._timedLoginTimeoutId);
            this._timedLoginTimeoutId = 0;
        }

        this._timedLoginIndicator.visible = false;
        this._timedLoginIndicator.scale_x = 0.;
    }
});

var UserList = GObject.registerClass({
    Signals: {
        'activate': { param_types: [UserListItem.$gtype] },
        'item-added': { param_types: [UserListItem.$gtype] },
    },
}, class UserList extends St.ScrollView {
    _init() {
        super._init({
            style_class: 'login-dialog-user-list-view',
            x_expand: true,
            y_expand: true,
        });
        this.set_policy(St.PolicyType.NEVER,
                        St.PolicyType.AUTOMATIC);

        this._box = new St.BoxLayout({
            vertical: true,
            style_class: 'login-dialog-user-list',
            pseudo_class: 'expanded',
        });

        this.add_actor(this._box);
        this._items = {};
    }

    vfunc_key_focus_in() {
        super.vfunc_key_focus_in();
        this._moveFocusToItems();
    }

    _moveFocusToItems() {
        let hasItems = Object.keys(this._items).length > 0;

        if (!hasItems)
            return;

        if (global.stage.get_key_focus() != this)
            return;

        let focusSet = this.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
        if (!focusSet) {
            const laters = global.compositor.get_laters();
            laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._moveFocusToItems();
                return false;
            });
        }
    }

    _onItemActivated(activatedItem) {
        this.emit('activate', activatedItem);
    }

    updateStyle(isExpanded) {
        if (isExpanded)
            this._box.add_style_pseudo_class('expanded');
        else
            this._box.remove_style_pseudo_class('expanded');

        for (let userName in this._items) {
            let item = this._items[userName];
            item.sync_hover();
        }
    }

    scrollToItem(item) {
        let box = item.get_allocation_box();

        let adjustment = this.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);
        adjustment.ease(value, {
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: _SCROLL_ANIMATION_TIME,
        });
    }

    jumpToItem(item) {
        let box = item.get_allocation_box();

        let adjustment = this.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);

        adjustment.set_value(value);
    }

    getItemFromUserName(userName) {
        let item = this._items[userName];

        if (!item)
            return null;

        return item;
    }

    containsUser(user) {
        return this._items[user.get_user_name()] != null;
    }

    addUser(user) {
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
        this._box.add_child(item);

        this._items[userName] = item;

        item.connect('activate', this._onItemActivated.bind(this));

        // Try to keep the focused item front-and-center
        item.connect('key-focus-in', () => this.scrollToItem(item));

        this._moveFocusToItems();

        this.emit('item-added', item);
    }

    removeUser(user) {
        if (!user.is_loaded)
            return;

        let userName = user.get_user_name();

        if (!userName)
            return;

        let item = this._items[userName];

        if (!item)
            return;

        item.destroy();
        delete this._items[userName];
    }

    numItems() {
        return Object.keys(this._items).length;
    }
});

var SessionMenuButton = GObject.registerClass({
    Signals: { 'session-activated': { param_types: [GObject.TYPE_STRING] } },
}, class SessionMenuButton extends St.Bin {
    _init() {
        let button = new St.Button({
            style_class: 'login-dialog-button login-dialog-session-list-button',
            icon_name: 'emblem-system-symbolic',
            reactive: true,
            track_hover: true,
            can_focus: true,
            accessible_name: _("Choose Session"),
            accessible_role: Atk.Role.MENU,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        super._init({ child: button });
        this._button = button;

        this._menu = new PopupMenu.PopupMenu(this._button, 0, St.Side.BOTTOM);
        Main.uiGroup.add_actor(this._menu.actor);
        this._menu.actor.hide();

        this._menu.connect('open-state-changed', (menu, isOpen) => {
            if (isOpen)
                this._button.add_style_pseudo_class('active');
            else
                this._button.remove_style_pseudo_class('active');
        });

        this._manager = new PopupMenu.PopupMenuManager(this._button,
                                                       { actionMode: Shell.ActionMode.NONE });
        this._manager.addMenu(this._menu);

        this._button.connect('clicked', () => this._menu.toggle());

        this._items = {};
        this._activeSessionId = null;
        this._populate();
    }

    updateSensitivity(sensitive) {
        this._button.reactive = sensitive;
        this._button.can_focus = sensitive;
        this.opacity = sensitive ? 255 : 0;
        this._menu.close(BoxPointer.PopupAnimation.NONE);
    }

    _updateOrnament() {
        let itemIds = Object.keys(this._items);
        for (let i = 0; i < itemIds.length; i++) {
            if (itemIds[i] == this._activeSessionId)
                this._items[itemIds[i]].setOrnament(PopupMenu.Ornament.DOT);
            else
                this._items[itemIds[i]].setOrnament(PopupMenu.Ornament.NONE);
        }
    }

    setActiveSession(sessionId) {
        if (sessionId == this._activeSessionId)
            return;

        this._activeSessionId = sessionId;
        this._updateOrnament();
    }

    close() {
        this._menu.close();
    }

    _populate() {
        let ids = Gdm.get_session_ids();
        ids.sort();

        if (ids.length <= 1) {
            this._button.hide();
            return;
        }

        for (let i = 0; i < ids.length; i++) {
            let [sessionName, sessionDescription_] = Gdm.get_session_name_and_description(ids[i]);

            let id = ids[i];
            let item = new PopupMenu.PopupMenuItem(sessionName);
            this._menu.addMenuItem(item);
            this._items[id] = item;

            item.connect('activate', () => {
                this.setActiveSession(id);
                this.emit('session-activated', this._activeSessionId);
            });
        }
    }
});

var LoginDialog = GObject.registerClass({
    Signals: {
        'failed': {},
        'wake-up-screen': {},
    },
}, class LoginDialog extends St.Widget {
    _init(parentActor) {
        super._init({ style_class: 'login-dialog', visible: false });

        this.get_accessible().set_role(Atk.Role.WINDOW);

        this.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this.connect('destroy', this._onDestroy.bind(this));
        parentActor.add_child(this);

        this._userManager = AccountsService.UserManager.get_default();
        this._gdmClient = new Gdm.Client();

        try {
            this._gdmClient.set_enabled_extensions([Gdm.UserVerifierChoiceList.interface_info().name]);
        } catch (e) {
        }

        this._settings = new Gio.Settings({ schema_id: GdmUtil.LOGIN_SCREEN_SCHEMA });

        this._settings.connect(`changed::${GdmUtil.BANNER_MESSAGE_KEY}`,
                               this._updateBanner.bind(this));
        this._settings.connect(`changed::${GdmUtil.BANNER_MESSAGE_TEXT_KEY}`,
                               this._updateBanner.bind(this));
        this._settings.connect(`changed::${GdmUtil.DISABLE_USER_LIST_KEY}`,
                               this._updateDisableUserList.bind(this));
        this._settings.connect(`changed::${GdmUtil.LOGO_KEY}`,
                               this._updateLogo.bind(this));

        this._textureCache = St.TextureCache.get_default();
        this._textureCache.connectObject('texture-file-changed',
            this._updateLogoTexture.bind(this), this);

        this._userSelectionBox = new St.BoxLayout({
            style_class: 'login-dialog-user-selection-box',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            vertical: true,
            visible: false,
        });
        this.add_child(this._userSelectionBox);

        this._userList = new UserList();
        this._userSelectionBox.add_child(this._userList);

        this._authPrompt = new AuthPrompt.AuthPrompt(this._gdmClient, AuthPrompt.AuthPromptMode.UNLOCK_OR_LOG_IN);
        this._authPrompt.connect('prompted', this._onPrompted.bind(this));
        this._authPrompt.connect('reset', this._onReset.bind(this));
        this._authPrompt.hide();
        this.add_child(this._authPrompt);

        // translators: this message is shown below the user list on the
        // login screen. It can be activated to reveal an entry for
        // manually entering the username.
        let notListedLabel = new St.Label({
            text: _("Not listed?"),
            style_class: 'login-dialog-not-listed-label',
        });
        this._notListedButton = new St.Button({
            style_class: 'login-dialog-not-listed-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: notListedLabel,
            reactive: true,
            x_align: Clutter.ActorAlign.START,
            label_actor: notListedLabel,
        });

        this._notListedButton.connect('clicked', this._hideUserListAskForUsernameAndBeginVerification.bind(this));

        this._notListedButton.hide();

        this._userSelectionBox.add_child(this._notListedButton);

        this._bannerView = new St.ScrollView({
            style_class: 'login-dialog-banner-view',
            opacity: 0,
            vscrollbar_policy: St.PolicyType.AUTOMATIC,
            hscrollbar_policy: St.PolicyType.NEVER,
        });
        this.add_child(this._bannerView);

        let bannerBox = new St.BoxLayout({ vertical: true });

        this._bannerView.add_actor(bannerBox);
        this._bannerLabel = new St.Label({
            style_class: 'login-dialog-banner',
            text: '',
        });
        this._bannerLabel.clutter_text.line_wrap = true;
        this._bannerLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        bannerBox.add_child(this._bannerLabel);
        this._updateBanner();

        this._sessionMenuButton = new SessionMenuButton();
        this._sessionMenuButton.connect('session-activated',
            (list, sessionId) => {
                this._greeter.call_select_session_sync(sessionId, null);
            });
        this._sessionMenuButton.opacity = 0;
        this._sessionMenuButton.show();
        this.add_child(this._sessionMenuButton);

        this._logoBin = new St.Widget({
            style_class: 'login-dialog-logo-bin',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.END,
        });
        this._logoBin.connect('resource-scale-changed', () => {
            this._updateLogoTexture(this._textureCache, this._logoFile);
        });
        this.add_child(this._logoBin);
        this._updateLogo();

        this._userList.connect('activate', (userList, item) => {
            this._onUserListActivated(item);
        });

        this._disableUserList = undefined;
        this._userListLoaded = false;

        this._realmManager = new Realmd.Manager();
        this._realmManager.connectObject('login-format-changed',
            this._showRealmLoginHint.bind(this), this);

        this._getGreeterSessionProxy();

        // If the user list is enabled, it should take key focus; make sure the
        // screen shield is initialized first to prevent it from stealing the
        // focus later
        Main.layoutManager.connectObject('startup-complete',
            this._updateDisableUserList.bind(this), this);
    }

    _getBannerAllocation(dialogBox) {
        let actorBox = new Clutter.ActorBox();

        let [, , natWidth, natHeight] = this._bannerView.get_preferred_size();
        let centerX = dialogBox.x1 + (dialogBox.x2 - dialogBox.x1) / 2;

        actorBox.x1 = Math.floor(centerX - natWidth / 2);
        actorBox.y1 = dialogBox.y1 + Main.layoutManager.panelBox.height;
        actorBox.x2 = actorBox.x1 + natWidth;
        actorBox.y2 = actorBox.y1 + natHeight;

        return actorBox;
    }

    _getLogoBinAllocation(dialogBox) {
        let actorBox = new Clutter.ActorBox();

        let [, , natWidth, natHeight] = this._logoBin.get_preferred_size();
        let centerX = dialogBox.x1 + (dialogBox.x2 - dialogBox.x1) / 2;

        actorBox.x1 = Math.floor(centerX - natWidth / 2);
        actorBox.y1 = dialogBox.y2 - natHeight;
        actorBox.x2 = actorBox.x1 + natWidth;
        actorBox.y2 = actorBox.y1 + natHeight;

        return actorBox;
    }

    _getSessionMenuButtonAllocation(dialogBox) {
        let actorBox = new Clutter.ActorBox();

        let [, , natWidth, natHeight] = this._sessionMenuButton.get_preferred_size();

        if (this.get_text_direction() === Clutter.TextDirection.RTL)
            actorBox.x1 = dialogBox.x1 + natWidth;
        else
            actorBox.x1 = dialogBox.x2 - (natWidth * 2);

        actorBox.y1 = dialogBox.y2 - (natHeight * 2);
        actorBox.x2 = actorBox.x1 + natWidth;
        actorBox.y2 = actorBox.y1 + natHeight;

        return actorBox;
    }

    _getCenterActorAllocation(dialogBox, actor) {
        let actorBox = new Clutter.ActorBox();

        let [, , natWidth, natHeight] = actor.get_preferred_size();
        let centerX = dialogBox.x1 + (dialogBox.x2 - dialogBox.x1) / 2;
        let centerY = dialogBox.y1 + (dialogBox.y2 - dialogBox.y1) / 2;

        natWidth = Math.min(natWidth, dialogBox.x2 - dialogBox.x1);
        natHeight = Math.min(natHeight, dialogBox.y2 - dialogBox.y1);

        actorBox.x1 = Math.floor(centerX - natWidth / 2);
        actorBox.y1 = Math.floor(centerY - natHeight / 2);
        actorBox.x2 = actorBox.x1 + natWidth;
        actorBox.y2 = actorBox.y1 + natHeight;

        return actorBox;
    }

    vfunc_allocate(dialogBox) {
        this.set_allocation(dialogBox);

        let themeNode = this.get_theme_node();
        dialogBox = themeNode.get_content_box(dialogBox);

        let dialogWidth = dialogBox.x2 - dialogBox.x1;
        let dialogHeight = dialogBox.y2 - dialogBox.y1;

        // First find out what space the children require
        let bannerAllocation = null;
        let bannerHeight = 0;
        if (this._bannerView.visible) {
            bannerAllocation = this._getBannerAllocation(dialogBox);
            bannerHeight = bannerAllocation.y2 - bannerAllocation.y1;
        }

        let authPromptAllocation = null;
        let authPromptWidth = 0;
        if (this._authPrompt.visible) {
            authPromptAllocation = this._getCenterActorAllocation(dialogBox, this._authPrompt);
            authPromptWidth = authPromptAllocation.x2 - authPromptAllocation.x1;
        }

        let userSelectionAllocation = null;
        let userSelectionHeight = 0;
        if (this._userSelectionBox.visible) {
            userSelectionAllocation = this._getCenterActorAllocation(dialogBox, this._userSelectionBox);
            userSelectionHeight = userSelectionAllocation.y2 - userSelectionAllocation.y1;
        }

        let logoAllocation = null;
        let logoHeight = 0;
        if (this._logoBin.visible) {
            logoAllocation = this._getLogoBinAllocation(dialogBox);
            logoHeight = logoAllocation.y2 - logoAllocation.y1;
        }

        let sessionMenuButtonAllocation = null;
        if (this._sessionMenuButton.visible)
            sessionMenuButtonAllocation = this._getSessionMenuButtonAllocation(dialogBox);

        // Then figure out if we're overly constrained and need to
        // try a different layout, or if we have what extra space we
        // can hand out
        if (bannerAllocation) {
            let bannerSpace;

            if (authPromptAllocation)
                bannerSpace = authPromptAllocation.y1 - bannerAllocation.y1;
            else
                bannerSpace = 0;

            let leftOverYSpace = bannerSpace - bannerHeight;

            if (leftOverYSpace > 0) {
                // First figure out how much left over space is up top
                let leftOverTopSpace = leftOverYSpace / 2;

                // Then, shift the banner into the middle of that extra space
                let yShift = Math.floor(leftOverTopSpace / 2);

                bannerAllocation.y1 += yShift;
                bannerAllocation.y2 += yShift;
            } else {
                // Then figure out how much space there would be if we switched to a
                // wide layout with banner on one side and authprompt on the other.
                let leftOverXSpace = dialogWidth - authPromptWidth;

                // In a wide view, half of the available space goes to the banner,
                // and the other half goes to the margins.
                let wideBannerWidth = leftOverXSpace / 2;
                let wideSpacing  = leftOverXSpace - wideBannerWidth;

                // If we do go with a wide layout, we need there to be at least enough
                // space for the banner and the auth prompt to be the same width,
                // so it doesn't look unbalanced.
                if (authPromptWidth > 0 && wideBannerWidth > authPromptWidth) {
                    let centerX = dialogBox.x1 + dialogWidth / 2;
                    let centerY = dialogBox.y1 + dialogHeight / 2;

                    // A small portion of the spacing goes down the center of the
                    // screen to help delimit the two columns of the wide view
                    let centerGap = wideSpacing / 8;

                    // place the banner along the left edge of the center margin
                    bannerAllocation.x2 = Math.floor(centerX - centerGap / 2);
                    bannerAllocation.x1 = Math.floor(bannerAllocation.x2 - wideBannerWidth);

                    // figure out how tall it would like to be and try to accommodate
                    // but don't let it get too close to the logo
                    let [, wideBannerHeight] = this._bannerView.get_preferred_height(wideBannerWidth);

                    let maxWideHeight = dialogHeight - 3 * logoHeight;
                    wideBannerHeight = Math.min(maxWideHeight, wideBannerHeight);
                    bannerAllocation.y1 = Math.floor(centerY - wideBannerHeight / 2);
                    bannerAllocation.y2 = bannerAllocation.y1 + wideBannerHeight;

                    // place the auth prompt along the right edge of the center margin
                    authPromptAllocation.x1 = Math.floor(centerX + centerGap / 2);
                    authPromptAllocation.x2 = authPromptAllocation.x1 + authPromptWidth;
                } else {
                    // If we aren't going to do a wide view, then we need to limit
                    // the height of the banner so it will present scrollbars

                    // First figure out how much space there is without the banner
                    leftOverYSpace += bannerHeight;

                    // Then figure out how much of that space is up top
                    let availableTopSpace = Math.floor(leftOverYSpace / 2);

                    // Then give all of that space to the banner
                    bannerAllocation.y2 = bannerAllocation.y1 + availableTopSpace;
                }
            }
        } else if (userSelectionAllocation) {
            // Grow the user list to fill the space
            let leftOverYSpace = dialogHeight - userSelectionHeight - logoHeight;

            if (leftOverYSpace > 0) {
                let topExpansion = Math.floor(leftOverYSpace / 2);
                let bottomExpansion = topExpansion;

                userSelectionAllocation.y1 -= topExpansion;
                userSelectionAllocation.y2 += bottomExpansion;
            }
        }

        // Finally hand out the allocations
        if (bannerAllocation)
            this._bannerView.allocate(bannerAllocation);

        if (authPromptAllocation)
            this._authPrompt.allocate(authPromptAllocation);

        if (userSelectionAllocation)
            this._userSelectionBox.allocate(userSelectionAllocation);

        if (logoAllocation)
            this._logoBin.allocate(logoAllocation);

        if (sessionMenuButtonAllocation)
            this._sessionMenuButton.allocate(sessionMenuButtonAllocation);
    }

    _ensureUserListLoaded() {
        if (!this._userManager.is_loaded) {
            this._userManager.connectObject('notify::is-loaded',
                () => {
                    if (this._userManager.is_loaded) {
                        this._userManager.disconnectObject(this);
                        this._loadUserList();
                    }
                });
        } else {
            let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, this._loadUserList.bind(this));
            GLib.Source.set_name_by_id(id, '[gnome-shell] _loadUserList');
        }
    }

    _updateDisableUserList() {
        let disableUserList = this._settings.get_boolean(GdmUtil.DISABLE_USER_LIST_KEY);

        // Disable user list when there are no users.
        if (this._userListLoaded && this._userList.numItems() == 0)
            disableUserList = true;

        if (disableUserList != this._disableUserList) {
            this._disableUserList = disableUserList;

            if (this._authPrompt.verificationStatus == AuthPrompt.AuthPromptStatus.NOT_VERIFYING)
                this._authPrompt.reset();

            if (this._disableUserList && this._timedLoginUserListHold)
                this._timedLoginUserListHold.release();
        }
    }

    _updateCancelButton() {
        let cancelVisible;

        // Hide the cancel button if the user list is disabled and we're asking for
        // a username
        if (this._authPrompt.verificationStatus == AuthPrompt.AuthPromptStatus.NOT_VERIFYING && this._disableUserList)
            cancelVisible = false;
        else
            cancelVisible = true;

        this._authPrompt.cancelButton.visible = cancelVisible;
    }

    _updateBanner() {
        let enabled = this._settings.get_boolean(GdmUtil.BANNER_MESSAGE_KEY);
        let text = this._settings.get_string(GdmUtil.BANNER_MESSAGE_TEXT_KEY);

        if (enabled && text) {
            this._bannerLabel.set_text(text);
            this._bannerLabel.show();
        } else {
            this._bannerLabel.hide();
        }
    }

    _fadeInBannerView() {
        this._bannerView.show();
        this._bannerView.ease({
            opacity: 255,
            duration: _FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _hideBannerView() {
        this._bannerView.remove_all_transitions();
        this._bannerView.opacity = 0;
        this._bannerView.hide();
    }

    _updateLogoTexture(cache, file) {
        if (this._logoFile && !this._logoFile.equal(file))
            return;

        this._logoBin.destroy_all_children();
        const resourceScale = this._logoBin.get_resource_scale();
        if (this._logoFile) {
            let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
            this._logoBin.add_child(this._textureCache.load_file_async(this._logoFile,
                                                                       -1, -1,
                                                                       scaleFactor,
                                                                       resourceScale));
        }
    }

    _updateLogo() {
        let path = this._settings.get_string(GdmUtil.LOGO_KEY);

        this._logoFile = path ? Gio.file_new_for_path(path) : null;
        this._updateLogoTexture(this._textureCache, this._logoFile);
    }

    _onPrompted() {
        const showSessionMenu = this._shouldShowSessionMenuButton();

        this._sessionMenuButton.updateSensitivity(showSessionMenu);
        this._sessionMenuButton.visible = showSessionMenu;
        this._showPrompt();
    }

    _resetGreeterProxy() {
        if (GLib.getenv('GDM_GREETER_TEST') != '1') {
            if (this._greeter)
                this._greeter.run_dispose();

            this._greeter = this._gdmClient.get_greeter_sync(null);

            this._greeter.connectObject(
                'default-session-name-changed', this._onDefaultSessionChanged.bind(this),
                'session-opened', this._onSessionOpened.bind(this),
                'timed-login-requested', this._onTimedLoginRequested.bind(this), this);
        }
    }

    _onReset(authPrompt, beginRequest) {
        this._resetGreeterProxy();
        this._sessionMenuButton.updateSensitivity(true);

        const previousUser = this._user;
        this._user = null;

        if (this._nextSignalId) {
            this._authPrompt.disconnect(this._nextSignalId);
            this._nextSignalId = 0;
        }

        if (previousUser && beginRequest === AuthPrompt.BeginRequestType.REUSE_USERNAME) {
            this._user = previousUser;
            this._authPrompt.setUser(this._user);
            this._authPrompt.begin({ userName: previousUser.get_user_name() });
        } else if (beginRequest === AuthPrompt.BeginRequestType.PROVIDE_USERNAME) {
            if (!this._disableUserList)
                this._showUserList();
            else
                this._hideUserListAskForUsernameAndBeginVerification();
        } else {
            this._hideUserListAndBeginVerification();
        }
    }

    _onDefaultSessionChanged(client, sessionId) {
        this._sessionMenuButton.setActiveSession(sessionId);
    }

    _shouldShowSessionMenuButton() {
        if (this._authPrompt.verificationStatus != AuthPrompt.AuthPromptStatus.VERIFYING &&
            this._authPrompt.verificationStatus != AuthPrompt.AuthPromptStatus.VERIFICATION_FAILED)
            return false;

        if (this._user && this._user.is_loaded && this._user.is_logged_in())
            return false;

        return true;
    }

    _showPrompt() {
        if (this._authPrompt.visible)
            return;
        this._authPrompt.opacity = 0;
        this._authPrompt.show();
        this._authPrompt.ease({
            opacity: 255,
            duration: _FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
        this._fadeInBannerView();
    }

    _showRealmLoginHint(realmManager, hint) {
        if (!hint)
            return;

        hint = hint.replace(/%U/g, 'user');
        hint = hint.replace(/%D/g, 'DOMAIN');
        hint = hint.replace(/%[^UD]/g, '');

        // Translators: this message is shown below the username entry field
        // to clue the user in on how to login to the local network realm
        this._authPrompt.setMessage(_("(e.g., user or %s)").format(hint), GdmUtil.MessageType.HINT);
    }

    _askForUsernameAndBeginVerification() {
        this._authPrompt.setUser(null);
        this._authPrompt.setQuestion(_('Username'));

        this._showRealmLoginHint(this._realmManager.loginFormat);

        if (this._nextSignalId)
            this._authPrompt.disconnect(this._nextSignalId);
        this._nextSignalId = this._authPrompt.connect('next',
            () => {
                this._authPrompt.disconnect(this._nextSignalId);
                this._nextSignalId = 0;
                this._authPrompt.updateSensitivity(false);
                let answer = this._authPrompt.getAnswer();
                this._user = this._userManager.get_user(answer);
                this._authPrompt.clear();
                this._authPrompt.begin({ userName: answer });
                this._updateCancelButton();
            });
        this._updateCancelButton();

        this._sessionMenuButton.updateSensitivity(false);
        this._authPrompt.updateSensitivity(true);
        this._showPrompt();
    }

    _bindOpacity() {
        this._bindings = Main.layoutManager.uiGroup.get_children()
            .filter(c => c != Main.layoutManager.screenShieldGroup)
            .map(c => this.bind_property('opacity', c, 'opacity', 0));
    }

    _unbindOpacity() {
        this._bindings.forEach(b => b.unbind());
    }

    _loginScreenSessionActivated() {
        if (this.opacity == 255 && this._authPrompt.verificationStatus == AuthPrompt.AuthPromptStatus.NOT_VERIFYING)
            return;

        if (this._authPrompt.verificationStatus !== AuthPrompt.AuthPromptStatus.NOT_VERIFYING)
            this._authPrompt.reset();

        this._bindOpacity();
        this.ease({
            opacity: 255,
            duration: _FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._unbindOpacity(),
        });
    }

    async _getGreeterSessionProxy() {
        const loginManager = LoginManager.getLoginManager();
        this._greeterSessionProxy = await loginManager.getCurrentSessionProxy();
        this._greeterSessionProxy?.connectObject('g-properties-changed', (proxy, properties) => {
            const activeChanged = !!properties.lookup_value('Active', null);
            if (activeChanged && proxy.Active)
                this._loginScreenSessionActivated();
        }, this);
    }

    _startSession(serviceName) {
        this._bindOpacity();
        this.ease({
            opacity: 0,
            duration: _FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._greeter.call_start_session_when_ready_sync(serviceName, true, null);
                this._unbindOpacity();
            },
        });
    }

    _onSessionOpened(client, serviceName) {
        this._authPrompt.finish(() => this._startSession(serviceName));
    }

    _waitForItemForUser(userName) {
        let item = this._userList.getItemFromUserName(userName);

        if (item)
            return null;

        let hold = new Batch.Hold();
        let signalId = this._userList.connect('item-added',
            () => {
                item = this._userList.getItemFromUserName(userName);

                if (item)
                    hold.release();
            });

        hold.connect('release', () => this._userList.disconnect(signalId));

        return hold;
    }

    _blockTimedLoginUntilIdle() {
        let hold = new Batch.Hold();

        this._timedLoginIdleTimeOutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, _TIMED_LOGIN_IDLE_THRESHOLD,
            () => {
                this._timedLoginIdleTimeOutId = 0;
                hold.release();
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._timedLoginIdleTimeOutId, '[gnome-shell] this._timedLoginIdleTimeOutId');
        return hold;
    }

    _startTimedLogin(userName, delay) {
        let firstRun = true;

        // Cancel execution of old batch
        if (this._timedLoginBatch) {
            this._timedLoginBatch.cancel();
            this._timedLoginBatch = null;
            firstRun = false;
        }

        // Reset previous idle-timeout
        if (this._timedLoginIdleTimeOutId) {
            GLib.source_remove(this._timedLoginIdleTimeOutId);
            this._timedLoginIdleTimeOutId = 0;
        }

        let loginItem = null;
        let animationTime;

        let tasks = [
            () => {
                if (this._disableUserList)
                    return null;

                this._timedLoginUserListHold = this._waitForItemForUser(userName);
                return this._timedLoginUserListHold;
            },

            () => {
                this._timedLoginUserListHold = null;

                if (this._disableUserList)
                    loginItem = this._authPrompt;
                else
                    loginItem = this._userList.getItemFromUserName(userName);

                // If there is an animation running on the item, reset it.
                loginItem.hideTimedLoginIndicator();
            },

            () => {
                if (this._disableUserList)
                    return;

                // If we're just starting out, start on the right item.
                if (!this._userManager.is_loaded)
                    this._userList.jumpToItem(loginItem);
            },

            () => {
                // This blocks the timed login animation until a few
                // seconds after the user stops interacting with the
                // login screen.

                // We skip this step if the timed login delay is very short.
                if (delay > _TIMED_LOGIN_IDLE_THRESHOLD) {
                    animationTime = delay - _TIMED_LOGIN_IDLE_THRESHOLD;
                    return this._blockTimedLoginUntilIdle();
                } else {
                    animationTime = delay;
                    return null;
                }
            },

            () => {
                if (this._disableUserList)
                    return;

                // If idle timeout is done, make sure the timed login indicator is shown
                if (delay > _TIMED_LOGIN_IDLE_THRESHOLD &&
                    this._authPrompt.visible)
                    this._authPrompt.cancel();

                if (delay > _TIMED_LOGIN_IDLE_THRESHOLD || firstRun) {
                    this._userList.scrollToItem(loginItem);
                    loginItem.grab_key_focus();
                }
            },

            () => loginItem.showTimedLoginIndicator(animationTime),

            () => {
                this._timedLoginBatch = null;
                this._greeter.call_begin_auto_login_sync(userName, null);
            },
        ];

        this._timedLoginBatch = new Batch.ConsecutiveBatch(this, tasks);

        return this._timedLoginBatch.run();
    }

    _onTimedLoginRequested(client, userName, seconds) {
        if (this._timedLoginBatch)
            return;

        this._startTimedLogin(userName, seconds);

        // Restart timed login on user interaction
        global.stage.connect('captured-event', (actor, event) => {
            if (event.type() == Clutter.EventType.KEY_PRESS ||
                event.type() == Clutter.EventType.BUTTON_PRESS)
                this._startTimedLogin(userName, seconds);

            return Clutter.EVENT_PROPAGATE;
        });
    }

    _setUserListExpanded(expanded) {
        this._userList.updateStyle(expanded);
        this._userSelectionBox.visible = expanded;
    }

    _hideUserList() {
        this._setUserListExpanded(false);
        if (this._userSelectionBox.visible)
            GdmUtil.cloneAndFadeOutActor(this._userSelectionBox);
    }

    _hideUserListAskForUsernameAndBeginVerification() {
        this._hideUserList();
        this._askForUsernameAndBeginVerification();
    }

    _hideUserListAndBeginVerification() {
        this._hideUserList();
        this._authPrompt.begin();
    }

    _showUserList() {
        this._ensureUserListLoaded();
        this._authPrompt.hide();
        this._hideBannerView();
        this._sessionMenuButton.close();
        this._sessionMenuButton.hide();
        this._setUserListExpanded(true);
        this._notListedButton.show();
        this._userList.grab_key_focus();
    }

    _beginVerificationForItem(item) {
        this._authPrompt.setUser(item.user);

        let userName = item.user.get_user_name();
        let hold = new Batch.Hold();

        this._authPrompt.begin({ userName, hold });
        return hold;
    }

    _onUserListActivated(activatedItem) {
        this._user = activatedItem.user;

        this._updateCancelButton();

        const batch = new Batch.ConcurrentBatch(this, [
            GdmUtil.cloneAndFadeOutActor(this._userSelectionBox),
            this._beginVerificationForItem(activatedItem),
        ]);
        batch.run();
    }

    _onDestroy() {
        if (this._settings) {
            this._settings.run_dispose();
            this._settings = null;
        }
        this._greeter = null;
        this._greeterSessionProxy = null;
        this._realmManager?.release();
        this._realmManager = null;
    }

    _loadUserList() {
        if (this._userListLoaded)
            return GLib.SOURCE_REMOVE;

        this._userListLoaded = true;

        let users = this._userManager.list_users();

        for (let i = 0; i < users.length; i++)
            this._userList.addUser(users[i]);

        this._updateDisableUserList();

        this._userManager.connectObject(
            'user-added', (userManager, user) => {
                this._userList.addUser(user);
                this._updateDisableUserList();
            },
            'user-removed', (userManager, user) => {
                this._userList.removeUser(user);
                this._updateDisableUserList();
            },
            'user-changed', (userManager, user) => {
                if (this._userList.containsUser(user) && user.locked)
                    this._userList.removeUser(user);
                else if (!this._userList.containsUser(user) && !user.locked)
                    this._userList.addUser(user);
                this._updateDisableUserList();
            }, this);

        return GLib.SOURCE_REMOVE;
    }

    activate() {
        this._userList.grab_key_focus();
        this.show();
    }

    open() {
        Main.ctrlAltTabManager.addGroup(this,
                                        _("Login Window"),
                                        'dialog-password-symbolic',
                                        { sortGroup: CtrlAltTab.SortGroup.MIDDLE });
        this.activate();

        this.opacity = 0;

        this._grab = Main.pushModal(global.stage, { actionMode: Shell.ActionMode.LOGIN_SCREEN });

        this.ease({
            opacity: 255,
            duration: 1000,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
        });

        return true;
    }

    close() {
        Main.popModal(this._grab);
        this._grab = null;
        Main.ctrlAltTabManager.removeGroup(this);
    }

    cancel() {
        this._authPrompt.cancel();
    }

    addCharacter(_unichar) {
        // Don't allow type ahead at the login screen
    }

    finish(onComplete) {
        this._authPrompt.finish(onComplete);
    }
});
