/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Gdm = imports.gi.Gdm;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Panel = imports.ui.panel;

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.

const SIDEBAR_VISIBLE_KEY = 'sidebar/visible';

function StatusMenu() {
    this._init();
}

StatusMenu.prototype = {
    _init: function() {
        this._gdm = Gdm.UserManager.ref_default();
        this._user = this._gdm.get_user(GLib.get_user_name());
        this._presence = new GnomeSessionPresence();

        this.actor = new St.BoxLayout({ name: 'statusMenu' });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._iconBox = new St.Bin();
        this.actor.add(this._iconBox, { y_align: St.Align.MIDDLE });

        let textureCache = Shell.TextureCache.get_default();
        // FIXME: these icons are all wrong (likewise in createSubMenu)
        this._availableIcon = textureCache.load_icon_name('gtk-yes', 16);
        this._busyIcon = textureCache.load_icon_name('gtk-no', 16);
        this._invisibleIcon = textureCache.load_icon_name('gtk-close', 16);
        this._idleIcon = textureCache.load_icon_name('gtk-media-pause', 16);

        this._presence.connect('StatusChanged', Lang.bind(this, this._updatePresenceIcon));
        this._presence.getStatus(Lang.bind(this, this._updatePresenceIcon));

        this._name = new St.Label({ text: this._user.get_real_name() });
        this.actor.add(this._name, { expand: true, y_align: St.Align.MIDDLE });
        this._userNameChangedId = this._user.connect('notify::display-name', Lang.bind(this, this._updateUserName));

        this._createSubMenu();
        this._gdm.connect('users-loaded', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-added', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-removed', Lang.bind(this, this._updateSwitchUser));
    },

    _onDestroy: function() {
        this._user.disconnect(this._userNameChangedId);
    },

    _updateUserName: function() {
        this._name.set_text(this._user.get_real_name());
    },

    _updateSwitchUser: function() {
        let users = this._gdm.list_users();
        if (users.length > 1)
            this._loginScreenItem.show();
        else
            this._loginScreenItem.hide();
    },

    _updatePresenceIcon: function(presence, status) {
        if (status == GnomeSessionPresenceStatus.AVAILABLE)
            this._iconBox.child = this._availableIcon;
        else if (status == GnomeSessionPresenceStatus.BUSY)
            this._iconBox.child = this._busyIcon;
        else if (status == GnomeSessionPresenceStatus.INVISIBLE)
            this._iconBox.child = this._invisibleIcon;
        else
            this._iconBox.child = this._idleIcon;
    },

    // The menu

    _createImageMenuItem: function(label, iconName, forceIcon) {
        let image = new Gtk.Image();
        let item = new Gtk.ImageMenuItem({ label: label,
                                           image: image,
                                           always_show_image: forceIcon == true });
        item.connect('style-set', Lang.bind(this,
            function() {
                image.set_from_icon_name(iconName, Gtk.IconSize.MENU);
            }));

        return item;
    },

    _createSubMenu: function() {
        this._menu = new Gtk.Menu();
        this._menu.connect('deactivate', Lang.bind(this, function() { this.emit('deactivated'); }));

        let item;

        item = this._createImageMenuItem(_('Available'), 'gtk-yes', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSessionPresenceStatus.AVAILABLE));
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Busy'), 'gtk-no', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSessionPresenceStatus.BUSY));
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Invisible'), 'gtk-close', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSessionPresenceStatus.INVISIBLE));
        this._menu.append(item);
        item.show();

        item = new Gtk.SeparatorMenuItem();
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Account Information...'), 'user-info');
        item.connect('activate', Lang.bind(this, this._onAccountInformationActivate));
        this._menu.append(item);
        item.show();

        let gconf = Shell.GConf.get_default();
        item = new Gtk.CheckMenuItem({ label: _('Sidebar'),
                                       active: gconf.get_boolean(SIDEBAR_VISIBLE_KEY) });
        item.connect('activate', Lang.bind(this,
            function() {
                gconf.set_boolean(SIDEBAR_VISIBLE_KEY, this._sidebarItem.active);
            }));
        this._menu.append(item);
        item.show();
        this._sidebarItem = item;

        item = this._createImageMenuItem(_('System Preferences...'), 'preferences-desktop');
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        this._menu.append(item);
        item.show();

        item = new Gtk.SeparatorMenuItem();
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Lock Screen'), 'system-lock-screen');
        item.connect('activate', Lang.bind(this, this._onLockScreenActivate));
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Switch User'), 'system-users');
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this._menu.append(item);
        item.show();
        this._loginScreenItem = item;

        item = this._createImageMenuItem(_('Log Out...'), 'system-log-out');
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this._menu.append(item);
        item.show();

        item = this._createImageMenuItem(_('Shut Down...'), 'system-shutdown');
        item.connect('activate', Lang.bind(this, this._onShutDownActivate));
        this._menu.append(item);
        item.show();
    },

    _setPresenceStatus: function(item, status) {
        this._presence.setStatus(status);
    },

    _onAccountInformationActivate: function() {
        this._spawn(['gnome-about-me']);
    },

    _onPreferencesActivate: function() {
        this._spawn(['gnome-control-center']);
    },

    _onLockScreenActivate: function() {
        this._spawn(['gnome-screensaver-command', '--lock']);
    },

    _onLoginScreenActivate: function() {
        this._gdm.goto_login_session();
        this._onLockScreenActivate();
    },

    _onQuitSessionActivate: function() {
        this._spawn(['gnome-session-save', '--logout-dialog']);
    },

    _onShutDownActivate: function() {
        this._spawn(['gnome-session-save', '--shutdown-dialog']);
    },

    _spawn: function(args) {
        // FIXME: once Shell.Process gets support for signalling
        // errors we should pop up an error dialog or something here
        // on failure
        let p = new Shell.Process({'args' : args});
        p.run();
    },

    // shell_status_menu_toggle:
    // @event: event causing the toggle
    //
    // If the menu is not currently up, pops it up. Otherwise, hides it.
    // Popping up may fail if another grab is already active; check with
    // isActive().
    toggle: function(event) {
        if (this._menu.visible)
            this._menu.popdown();
        else {
            // We don't want to overgrab a Mutter grab with the grab
            // that GTK+ uses on menus.
            if (global.display_is_grabbed())
                return;

            let [menuWidth, menuHeight] = this._menu.get_size_request ();

            let panel;
            for (panel = this.actor; panel; panel = panel.get_parent()) {
                if (panel._delegate instanceof Panel.Panel)
                    break;
            }

            let [panelX, panelY] = panel.get_transformed_position();
            let [panelWidth, panelHeight] = panel.get_transformed_size();

            let menuX = Math.round(panelX + panelWidth - menuWidth);
            let menuY = Math.round(panelY + panelHeight);

            Shell.popup_menu(this._menu, event.get_button(), event.get_time(),
                             menuX, menuY);
        }
    },
    
    //  isActive:
    //  
    //  Gets whether the menu is currently popped up
    //  
    //  Return value: %true if the menu is currently popped up
    isActive: function() {
        return this._menu.visible;
    }
};
Signals.addSignalMethods(StatusMenu.prototype);


const GnomeSessionPresenceIface = {
    name: 'org.gnome.SessionManager.Presence',
    methods: [{ name: 'SetStatus',
                inSignature: 'u' }],
    properties: [{ name: 'status',
                   signature: 'u',
                   access: 'readwrite' }],
    signals: [{ name: 'StatusChanged',
                inSignature: 'u' }]
};

const GnomeSessionPresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3
};

function GnomeSessionPresence() {
    this._init();
}

GnomeSessionPresence.prototype = {
    _init: function() {
        DBus.session.proxifyObject(this, 'org.gnome.SessionManager', '/org/gnome/SessionManager/Presence', this);
        this.connect('StatusChanged', Lang.bind(this, function (proxy, status) { this.status = status; }));
    },

    getStatus: function(callback) {
        this.GetRemote('status', Lang.bind(this,
            function(status, ex) {
                if (!ex)
                    callback(this, status);
            }));
    },

    setStatus: function(status) {
        this.SetStatusRemote(status);
    }
};
DBus.proxifyPrototype(GnomeSessionPresence.prototype, GnomeSessionPresenceIface);

