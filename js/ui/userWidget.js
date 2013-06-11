
// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// A widget showing the user avatar and name
const AccountsService = imports.gi.AccountsService;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;

const Params = imports.misc.params;

const AVATAR_ICON_SIZE = 64;

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.

const Avatar = new Lang.Class({
    Name: 'Avatar',

    _init: function(user, params) {
        this._user = user;
        params = Params.parse(params, { reactive: false,
                                        iconSize: AVATAR_ICON_SIZE,
                                        styleClass: 'framed-user-icon' });
        this._iconSize = params.iconSize;

        this.actor = new St.Bin({ style_class: params.styleClass,
                                  track_hover: params.reactive,
                                  reactive: params.reactive });
    },

    setSensitive: function(sensitive) {
        this.actor.can_focus = sensitive;
        this.actor.reactive = sensitive;
    },

    update: function() {
        let iconFile = this._user.get_icon_file();
        if (iconFile && !GLib.file_test(iconFile, GLib.FileTest.EXISTS))
            iconFile = null;

        if (iconFile) {
            let file = Gio.File.new_for_path(iconFile);
            this.actor.child = null;
            this.actor.style = 'background-image: url("%s");'.format(iconFile);
        } else {
            this.actor.style = null;
            this.actor.child = new St.Icon({ icon_name: 'avatar-default-symbolic',
                                             icon_size: this._iconSize });
        }
    }
});

const UserWidget = new Lang.Class({
    Name: 'UserWidget',

    _init: function(user) {
        this._user = user;

        this.actor = new St.BoxLayout({ style_class: 'user-widget',
                                        vertical: false });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._avatar = new Avatar(user);
        this.actor.add(this._avatar.actor,
                       { x_fill: true, y_fill: true });

        this._label = new St.Label({ style_class: 'user-widget-label' });
        this.actor.add(this._label,
                       { expand: true,
                         x_fill: true,
                         y_fill: false,
                         y_align: St.Align.MIDDLE });

        this._userLoadedId = this._user.connect('notify::is-loaded',
                                                Lang.bind(this, this._updateUser));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this, this._updateUser));
        if (this._user.is_loaded)
            this._updateUser();
    },

    _onDestroy: function() {
        if (this._userLoadedId != 0) {
            this._user.disconnect(this._userLoadedId);
            this._userLoadedId = 0;
        }

        if (this._userChangedId != 0) {
            this._user.disconnect(this._userChangedId);
            this._userChangedId = 0;
        }
    },

    _updateUser: function() {
        if (this._user.is_loaded)
            this._label.text = this._user.get_real_name();
        else
            this._label.text = '';

        this._avatar.update();
    }
});
