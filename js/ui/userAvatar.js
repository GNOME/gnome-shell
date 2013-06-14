// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const St = imports.gi.St;

const Params = imports.misc.params;

const DIALOG_ICON_SIZE = 64;

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.

const UserAvatar = new Lang.Class({
    Name: 'UserAvatar',

    _init: function(user, params) {
        this._user = user;
        params = Params.parse(params, { reactive: false,
                                        iconSize: DIALOG_ICON_SIZE,
                                        styleClass: 'status-chooser-user-icon' });
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
