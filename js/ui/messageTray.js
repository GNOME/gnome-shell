/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Signals = imports.signals;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;

const MESSAGE_TRAY_TIMEOUT = 0.2;

const ICON_SIZE = 24;

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
        if (this._iconBox.child)
            this._iconBox.child.destroy();

        // Don't hide the notification if we are showing a new one.
        if (this._hideTimeoutId == 0)
            this.actor.hide();
    }
};

function Source(id, createIcon) {
    this._init(id, createIcon);
}

Source.prototype = {
    _init: function(id, createIcon) {
        this.id = id;
        if (createIcon)
            this.createIcon = createIcon;
    },

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon: function(size) {
        throw new Error('no implementation of createIcon in ' + this);
    },

    notify: function(text) {
        Main.notificationPopup.show(this.createIcon(ICON_SIZE), text);
    },

    clicked: function() {
        this.emit('clicked');
    },

    destroy: function() {
        this.emit('destroy');
    }
};
Signals.addSignalMethods(Source.prototype);

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
        this._sources = {};
        this._icons = {};
    },

    contains: function(source) {
        return this._sources.hasOwnProperty(source.id);
    },

    add: function(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.id);
            return;
        }

        let iconBox = new St.Bin({ reactive: true });
        iconBox.child = source.createIcon(ICON_SIZE);
        this._tray.insert_actor(iconBox, 0);
        this._icons[source.id] = iconBox;
        this._sources[source.id] = source;

        iconBox.connect('button-release-event', Lang.bind(this,
            function () {
                source.clicked();
            }));

        source.connect('destroy', Lang.bind(this,
            function () {
                this.remove(source);
            }));
    },

    remove: function(source) {
        if (!this.contains(source))
            return;

        this._tray.remove_actor(this._icons[source.id]);
        delete this._icons[source.id];
        delete this._sources[source.id];
    },

    getSource: function(id) {
        return this._sources[id];
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

