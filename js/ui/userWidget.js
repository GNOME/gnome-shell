// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// A widget showing the user avatar and name

const Clutter = imports.gi.Clutter;
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
                                  reactive: params.reactive,
                                  width: this._iconSize,
                                  height: this._iconSize });
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

const UserWidgetLabel = new Lang.Class({
    Name: 'UserWidgetLabel',
    Extends: St.Widget,

    _init: function(user) {
        this.parent({ layout_manager: new Clutter.BinLayout() });

        this._user = user;

        this._realNameLabel = new St.Label({ style_class: 'user-widget-label',
                                             y_align: Clutter.ActorAlign.CENTER });
        this.add_child(this._realNameLabel);

        this._userNameLabel = new St.Label({ style_class: 'user-widget-label',
                                             y_align: Clutter.ActorAlign.CENTER });
        this.add_child(this._userNameLabel);

        this._currentLabel = null;

        this._userLoadedId = this._user.connect('notify::is-loaded', Lang.bind(this, this._updateUser));
        this._userChangedId = this._user.connect('changed', Lang.bind(this, this._updateUser));
        this._updateUser();

        // We can't override the destroy vfunc because that might be called during
        // object finalization, and we can't call any JS inside a GC finalize callback,
        // so we use a signal, that will be disconnected by GObject the first time
        // the actor is destroyed (which is guaranteed to be as part of a normal
        // destroy() call from JS, possibly from some ancestor)
        this.connect('destroy', Lang.bind(this, this._onDestroy));
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

    vfunc_allocate: function(box, flags) {
        this.set_allocation(box, flags);

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [minRealNameWidth, minRealNameHeight,
             natRealNameWidth, natRealNameHeight] = this._realNameLabel.get_preferred_size();

        let [minUserNameWidth, minUserNameHeight,
             natUserNameWidth, natUserNameHeight] = this._userNameLabel.get_preferred_size();

        if (natRealNameWidth <= availWidth)
            this._currentLabel = this._realNameLabel;
        else
            this._currentLabel = this._userNameLabel;

        let childBox = new Clutter.ActorBox();
        childBox.x1 = 0;
        childBox.y1 = 0;
        childBox.x2 = availWidth;
        childBox.y2 = availHeight;

        this._currentLabel.allocate(childBox, flags);
    },

    vfunc_paint: function() {
        this._currentLabel.paint();
    },

    _updateUser: function() {
        if (this._user.is_loaded) {
            this._realNameLabel.text = this._user.get_real_name();
            this._userNameLabel.text = this._user.get_user_name();
        } else {
            this._realNameLabel.text = '';
            this._userNameLabel.text = '';
        }
    },
});

const UserWidget = new Lang.Class({
    Name: 'UserWidget',

    _init: function(user) {
        this._user = user;

        this.actor = new St.BoxLayout({ style_class: 'user-widget',
                                        vertical: false });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._avatar = new Avatar(user);
        this.actor.add_child(this._avatar.actor);

        this._label = new UserWidgetLabel(user);
        this.actor.add_child(this._label);

        this._userLoadedId = this._user.connect('notify::is-loaded', Lang.bind(this, this._updateUser));
        this._userChangedId = this._user.connect('changed', Lang.bind(this, this._updateUser));
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
        this._avatar.update();
    }
});
