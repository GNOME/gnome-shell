/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;

const MESSAGE_TRAY_TIMEOUT = 0.2;

const ICON_SIZE = 24;

function Notification(icon, text) {
    this._init(icon, text);
}

Notification.prototype = {
    _init: function(icon, text) {
        this.icon = icon;
        this.text = text;
    }
}

function NotificationBox() {
    this._init();
}

NotificationBox.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: 'notification' });

        this._iconBox = new St.Bin();
        this.actor.add(this._iconBox);

        this._textBox = new Shell.GenericContainer();
        this._textBox.connect('get-preferred-width', Lang.bind(this, this._textBoxGetPreferredWidth));
        this._textBox.connect('get-preferred-height', Lang.bind(this, this._textBoxGetPreferredHeight));
        this._textBox.connect('allocate', Lang.bind(this, this._textBoxAllocate));
        this.actor.add(this._textBox, { expand: true, x_fill: false, y_fill: false, y_align: St.Align.MIDDLE });

        this._text = new St.Label();
        this._text.clutter_text.line_wrap = true;
        this._text.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._textBox.add_actor(this._text);
    },

    _textBoxGetPreferredWidth: function (actor, forHeight, alloc) {
        let [min, nat] = this._text.get_preferred_width(forHeight);

        alloc.min_size = alloc.nat_size = Math.min(nat, global.screen_width / 2);
    },

    _textBoxGetPreferredHeight: function (actor, forWidth, alloc) {
        // St.BoxLayout passes -1 for @forWidth, which isn't what we want.
        let prefWidth = {};
        this._textBoxGetPreferredWidth(this._textBox, -1, prefWidth);
        [alloc.min_size, alloc.nat_size] = this._text.get_preferred_height(prefWidth.nat_size);
        log('for width ' + prefWidth.nat_size + ', height ' + alloc.nat_size);
    },

    _textBoxAllocate: function (actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        childBox.x1 = childBox.y1 = 0;
        childBox.x2 = box.x2 - box.x1;
        childBox.y2 = box.y2 - box.y1;
        this._text.allocate(childBox, flags);
    },

    setContent: function(notification) {
        this._iconBox.child = notification.icon;

        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        let markup = notification.text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, "&lt;$1");
        this._text.clutter_text.set_markup(markup);
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
        Main.messageTray.showNotification(new Notification(this.createIcon(ICON_SIZE), text));
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
        this.actor = new St.BoxLayout({ name: 'message-tray',
                                        reactive: true });

        let primary = global.get_primary_monitor();
        this.actor.x = 0;
        this.actor.y = primary.height - 1;

        this.actor.width = primary.width;

        this._summaryBin = new St.Bin({ x_align: St.Align.END });
        this.actor.add(this._summaryBin, { expand: true });
        this._summaryBin.hide();

        this._notificationBox = new NotificationBox();
        this._notificationQueue = [];
        this.actor.add(this._notificationBox.actor);
        this._notificationBox.actor.hide();

        Main.chrome.addActor(this.actor, { affectsStruts: false });

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onMessageTrayEntered));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onMessageTrayLeft));
        this._isShowing = false;
        this.actor.show();

        this._summary = new St.BoxLayout({ name: 'summary-mode' });
        this._summaryBin.child = this._summary;

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
        this._summary.insert_actor(iconBox, 0);
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

        this._summary.remove_actor(this._icons[source.id]);
        delete this._icons[source.id];
        delete this._sources[source.id];
    },

    getSource: function(id) {
        return this._sources[id];
    },

    _onMessageTrayEntered: function() {
        // Don't hide the message tray after a timeout if the user has moved the mouse over it.
        // We might have a timeout in place if the user moved the mouse away from the message tray for a very short period of time
        // or if we are showing a notification.
        if (this._hideTimeoutId > 0)
            Mainloop.source_remove(this._hideTimeoutId);

        if (this._isShowing)
            return;

        // If the message tray was not already showing, we'll show it in the summary mode.
        this._summaryBin.show();
        this._show();
    },

    _onMessageTrayLeft: function() {
        if (!this._isShowing)
            return;

        // We wait just a little before hiding the message tray in case the user will quickly move the mouse back over it.
        this._hideTimeoutId = Mainloop.timeout_add(MESSAGE_TRAY_TIMEOUT * 1000, Lang.bind(this, this._hide));
    },

    _show: function() {
        this._isShowing = true;
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.actor,
                         { y: primary.height - this.actor.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    _hide: function() {
        this._hideTimeoutId = 0;

        let primary = global.get_primary_monitor();

        Tweener.addTween(this.actor,
                         { y: primary.height - 1,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._hideComplete,
                           onCompleteScope: this
                         });
        return false;
    },

    _hideComplete: function() {
        this._isShowing = false;
        this._summaryBin.hide();
        this._notificationBox.actor.hide();
        if (this._notificationQueue.length > 0)
            this.showNotification(this._notificationQueue.shift());
    },

    showNotification: function(notification) {
        if (this._isShowing) {
            this._notificationQueue.push(notification);
            return;
        }

        this._notificationBox.setContent(notification);

        this._notificationBox.actor.x = Math.round((this.actor.width - this._notificationBox.actor.width) / 2);
        this._notificationBox.actor.show();

        // Because we set up the timeout before we do the animation, we add ANIMATION_TIME to NOTIFICATION_TIMEOUT, so that
        // NOTIFICATION_TIMEOUT represents the time the notifiation is fully shown.
        this._hideTimeoutId = Mainloop.timeout_add((NOTIFICATION_TIMEOUT + ANIMATION_TIME) * 1000, Lang.bind(this, this._hide));

        this._show();
     }
};
