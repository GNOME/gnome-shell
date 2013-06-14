
// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// A widget showing the user avatar and name
const AccountsService = imports.gi.AccountsService;
const Lang = imports.lang;
const St = imports.gi.St;

const UserAvatar = imports.ui.userAvatar;

const UserWidget = new Lang.Class({
    Name: 'UserWidget',

    _init: function(user) {
        this._user = user;

        this.actor = new St.BoxLayout({ style_class: 'user-widget',
                                        vertical: false });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._avatar = new UserAvatar.UserAvatar(user);
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
