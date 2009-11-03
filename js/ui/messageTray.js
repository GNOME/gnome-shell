/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;

function Notification() {
    this._init();
}

Notification.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: 'notification' });

        this._iconBox = new St.Bin();
        this.actor.add(this._iconBox);

        this._text = new St.Label();
        this.actor.add(this._text, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });

        // Directly adding the actor to Main.chrome.actor is a hack to
        // work around the fact that there is no way to add an actor that
        // affects the input region but not the shape.
        // See: https://bugzilla.gnome.org/show_bug.cgi?id=597044
        Main.chrome.actor.add_actor(this.actor);
        Main.chrome.addInputRegionActor(this.actor);

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
