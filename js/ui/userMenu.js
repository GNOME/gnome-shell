// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported UserMenu */

const { AccountsService, Clutter, GLib,
    Gio, Pango, Shell, St } = imports.gi;

const AppActivation = imports.ui.appActivation;
const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const UserWidget = imports.ui.userWidget;

const USER_ICON_SIZE = 34;

const ONLINE_ACCOUNTS_TEXT = _('Social Accounts');
const ONLINE_ACCOUNTS_PANEL_LAUNCHER = 'gnome-online-accounts-panel.desktop';

const SETTINGS_TEXT = _('Settings');
const SETTINGS_LAUNCHER = 'gnome-control-center.desktop';

const USER_ACCOUNTS_PANEL_LAUNCHER = 'gnome-user-accounts-panel.desktop';

const FEEDBACK_TEXT = _('Give Us Feedback');
const FEEDBACK_LAUNCHER = "eos-link-feedback.desktop";

const HELP_CENTER_TEXT = _('Help');
const HELP_CENTER_LAUNCHER = 'org.gnome.Yelp.desktop';

const UserAccountSection = class extends PopupMenu.PopupMenuSection {
    constructor(user) {
        super();

        // User account's icon
        this.userIconItem = new PopupMenu.PopupBaseMenuItem({
            reactive: false,
            can_focus: false,
        });
        this.userIconItem.set({
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });

        this._user = user;
        this._avatar = new UserWidget.Avatar(this._user, {
            reactive: true,
            styleClass: 'user-menu-avatar',
        });
        this._avatar.x_align = Clutter.ActorAlign.CENTER;

        let iconButton = new St.Button({ child: this._avatar });
        this.userIconItem.add_child(iconButton);

        iconButton.connect('clicked', () => {
            if (Main.sessionMode.allowSettings)
                this.userIconItem.activate(null);
        });

        this.userIconItem.connect('notify::sensitive', () => {
            this._avatar.setSensitive(this.userIconItem.getSensitive);
        });
        this.addMenuItem(this.userIconItem);

        // User account's name
        this.userLabelItem = new PopupMenu.PopupBaseMenuItem({
            reactive: false,
            can_focus: false,
        });
        this._label = new St.Label({ style_class: 'user-menu-name' });
        this._label.clutter_text.set({
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            ellipsize: Pango.EllipsizeMode.NONE,
            line_wrap: true,
        });
        this.userLabelItem.add_child(this._label);
        this.addMenuItem(this.userLabelItem);

        // We need to monitor the session to know when to enable the user avatar
        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        this.userIconItem.setSensitive(Main.sessionMode.allowSettings);
    }

    update() {
        this._avatar.update();

        if (this._user.is_loaded)
            this._label.set_text(this._user.get_real_name());
        else
            this._label.set_text('');
    }
};

var UserMenu = class {
    constructor() {
        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());

        this._user.connect('notify::is-loaded', this._updateUser.bind(this));
        this._user.connect('changed', this._updateUser.bind(this));

        this._createPanelIcon();
        this._createPopupMenu();

        this._updateUser();
    }

    _createPanelIcon() {
        this.panelBox = new St.BoxLayout({
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._panelAvatar = new UserWidget.Avatar(this._user, {
            iconSize: USER_ICON_SIZE,
            styleClass: 'user-menu-button-icon',
            reactive: true,
        });
        this.panelBox.add_actor(this._panelAvatar);
    }

    _createPopupMenu() {
        this.menu = new PopupMenu.PopupMenuSection();

        this._accountSection = new UserAccountSection(this._user);
        this._accountSection.userIconItem.connect('activate', () => {
            this._launchApplication(USER_ACCOUNTS_PANEL_LAUNCHER);
        });

        this.menu.addMenuItem(this._accountSection);
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        let menuItemsSection = new PopupMenu.PopupMenuSection();
        menuItemsSection.box.style_class = 'user-menu-items';

        // Control Center
        let gicon = new Gio.ThemedIcon({ name: 'applications-system-symbolic' });
        this._settingsItem = menuItemsSection.addAction(SETTINGS_TEXT, () => {
            this._launchApplication(SETTINGS_LAUNCHER);
        }, gicon);

        // Social
        gicon = new Gio.ThemedIcon({ name: 'user-available-symbolic' });
        menuItemsSection.addSettingsAction(ONLINE_ACCOUNTS_TEXT, ONLINE_ACCOUNTS_PANEL_LAUNCHER, gicon);

        // Feedback
        let iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/feedback-symbolic.svg');
        gicon = new Gio.FileIcon({ file: iconFile });
        menuItemsSection.addAction(FEEDBACK_TEXT, () => {
            this._launchApplication(FEEDBACK_LAUNCHER);
        }, gicon);
        this.menu.addMenuItem(menuItemsSection);

        // Help center
        iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/endless-help-symbolic.svg');
        gicon = new Gio.FileIcon({ file: iconFile });
        menuItemsSection.addAction(HELP_CENTER_TEXT, () => {
            this._launchApplication(HELP_CENTER_LAUNCHER);
        }, gicon);
        this.menu.addMenuItem(menuItemsSection);

        // We need to monitor the session to know when to show/hide the settings item
        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _launchApplication(launcherName) {
        this.menu.close(BoxPointer.PopupAnimation.NONE);
        Main.overview.hide();

        let app = Shell.AppSystem.get_default().lookup_app(launcherName);
        let context = new AppActivation.AppActivationContext(app);
        context.activate();
    }

    _updateUser() {
        this._panelAvatar.update();
        this._accountSection.update();
    }

    _sessionUpdated() {
        this._settingsItem.visible = Main.sessionMode.allowSettings;
    }
};
