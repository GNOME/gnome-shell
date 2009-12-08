/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;

const MESSAGE_TRAY_TIMEOUT = 0.2;

function Notification() {
    this._init();
}

Notification.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: 'notification' });

        this._iconBox = new St.Bin();
        this.actor.add(this._iconBox);

        this._text = new St.Label();
        this.actor.add(this._text, { expand: true, x_fill: false, y_fill: false, y_align: St.Align.MIDDLE });

        Main.chrome.addActor(this.actor, { affectsStruts: false });

        let primary = global.get_primary_monitor();
        this.actor.y = primary.height;

        this._hideTimeoutId = 0;
    },

    show: function(icon, text) {
        let primary = global.get_primary_monitor();

        if (this._hideTimeoutId > 0) 
            Mainloop.source_remove(this._hideTimeoutId);
        this._hideTimeoutId = Mainloop.timeout_add(NOTIFICATION_TIMEOUT * 1000, Lang.bind(this, this.hide));

        this._iconBox.child = icon;
        this._text.text = text;

        this.actor.x = Math.round((primary.width - this.actor.width) / 2);
        this.actor.show();
        Tweener.addTween(this.actor,
                         { y: primary.height - this.actor.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    hide: function() {
        let primary = global.get_primary_monitor();
        this._hideTimeoutId = 0;

        Tweener.addTween(this.actor,
                         { y: primary.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this.hideComplete,
                           onCompleteScope: this
                         });
        return false;
    },

    hideComplete: function() {
        // We don't explicitly destroy the icon, since the caller may
        // still want it.
        this._iconBox.child = null;

        // Don't hide the notification if we are showing a new one.
        if (this._hideTimeoutId == 0)
            this.actor.hide();
    }
};

function MessageTray() {
    this._init();
}

MessageTray.prototype = {
    _init: function() {
        this.actor = new St.Bin({ name: 'message-tray',
                                  reactive: true,
                                  x_align: St.Align.END });
        Main.chrome.addActor(this.actor, { affectsStruts: false });

        let primary = global.get_primary_monitor();
        this.actor.x = 0;
        this.actor.y = primary.height - 1;

        this.actor.width = primary.width;

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onMessageTrayEntered));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onMessageTrayLeft));
        this._isShowing = false;
        this.actor.show();

        this._tray = new St.BoxLayout({ name: 'message-tray-inner' });
        this.actor.child = this._tray;
        this._tray.expand = true;
        this._icons = {};
    },

    contains: function(id) {
        return this._icons.hasOwnProperty(id);
    },

    add: function(id, icon) {
        if (this.contains(id))
            return;

        let iconBox = new St.Bin();
        iconBox.child = icon;
        this._tray.insert_actor(iconBox, 0);
        this._icons[id] = iconBox;
    },

    remove: function(id) {
        if (!this.contains(id))
            return;

        this._tray.remove_actor(this._icons[id]);
        this._icons[id].destroy();
        delete this._icons[id];
    },

    _onMessageTrayEntered: function() {
        if (this._hideTimeoutId > 0)
            Mainloop.source_remove(this._hideTimeoutId);

        if (this._isShowing)
            return;

        this._isShowing = true;
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.actor,
                         { y: primary.height - this.actor.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    _onMessageTrayLeft: function() {
        if (!this._isShowing)
            return;

        this._hideTimeoutId = Mainloop.timeout_add(MESSAGE_TRAY_TIMEOUT * 1000, Lang.bind(this, this._hide));
    },

    _hide: function() {
        this._isShowing = false;
        this._hideTimeoutId = 0;

        let primary = global.get_primary_monitor();

        Tweener.addTween(this.actor,
                         { y: primary.height - 1,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
        return false;
    }
};

