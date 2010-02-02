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
const SUMMARY_TIMEOUT = 1;

const MESSAGE_TRAY_TIMEOUT = 0.2;

const ICON_SIZE = 24;

const MessageTrayState = {
    HIDDEN: 0, // entire message tray is hidden
    NOTIFICATION: 1, // notifications are visible
    SUMMARY: 2, // summary is visible
    TRAY_ONLY: 3 // neither notifiations nor summary are visible, only tray
};

function Notification(icon, text, source) {
    this._init(icon, text, source);
}

Notification.prototype = {
    _init: function(icon, text, source) {
        this.icon = icon;
        this.text = text;
        this.source = source;
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

        this._text = new St.Label();
        this.actor.add(this._text, { expand: true, x_fill: false, y_fill: false, y_align: St.Align.MIDDLE });

        this.notification = null;
    },

    setContent: function(notification) {
        this.notification = notification;

        this._iconBox.child = notification.icon;

        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        let markup = notification.text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, "&lt;$1");
        this._text.clutter_text.set_markup(markup);

        let primary = global.get_primary_monitor();
        this.actor.x = Math.round((primary.width - this.actor.width) / 2);
    }
};

function Source(id, createIcon) {
    this._init(id, createIcon);
}

Source.prototype = {
    _init: function(id, createIcon) {
        this.id = id;
        this.text = null;
        if (createIcon)
            this.createIcon = createIcon;
    },

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon: function(size) {
        throw new Error('no implementation of createIcon in ' + this);
    },

    notify: function(text) {
        this.text = text;
        this.emit('notify');
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

        global.connect('screen-size-changed',
                       Lang.bind(this, this._setSizePosition));
        this._setSizePosition();

        this._summaryBin = new St.Bin({ x_align: St.Align.END });
        this.actor.add(this._summaryBin, { expand: true });
        this._summaryBin.hide();

        this._notificationBox = new NotificationBox();
        this._notificationQueue = [];
        this.actor.add(this._notificationBox.actor);
        this._notificationBox.actor.hide();

        this._summaryBin.connect('notify::allocation', Lang.bind(this,
            function() {
                let primary = global.get_primary_monitor();
                this._summaryBin.x = primary.x + primary.width - this._summaryBin.width;
            }));

        Main.chrome.addActor(this.actor, { affectsStruts: false });

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onMessageTrayEntered));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onMessageTrayLeft));
        this._state = MessageTrayState.HIDDEN;
        this.actor.show();

        this._summary = new St.BoxLayout({ name: 'summary-mode' });
        this._summaryBin.child = this._summary;

        this._sources = {};
        this._icons = {};
    },

    _setSizePosition: function() {
        let primary = global.get_primary_monitor();
        this.actor.x = primary.x;
        this.actor.y = primary.y + primary.height - 1;

        this.actor.width = primary.width;
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

        source.connect('notify', Lang.bind(this, this._onNotify));

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

        // remove all notifications with this source from the queue
        let newNotificationQueue = [];
        for (let i = 0; i < this._notificationQueue.length; i++) {
            if (this._notificationQueue[i].source != source)
                newNotificationQueue.push(this._notificationQueue[i]);
        }
        this._notificationQueue = newNotificationQueue;

        // Update state if we are showing a notification from the removed source
        if (this._state == MessageTrayState.NOTIFICATION &&
            this._notificationBox.notification.source == source)
            this._updateState();

        this._summary.remove_actor(this._icons[source.id]);
        delete this._icons[source.id];
        delete this._sources[source.id];
    },

    getSource: function(id) {
        return this._sources[id];
    },

    _onNotify: function(source) {
        let notification = new Notification(source.createIcon(ICON_SIZE), source.text, source)
        this._notificationQueue.push(notification);

        if (this._state == MessageTrayState.HIDDEN)
	    this._updateState();
    },

    _onMessageTrayEntered: function() {
        // Don't hide the message tray after a timeout if the user has moved
        // the mouse over it.
        // We might have a timeout in place if the user moved the mouse away
        // from the message tray for a very short period of time or if we are
        // showing a notification.
        if (this._updateTimeoutId > 0)
            Mainloop.source_remove(this._updateTimeoutId);

        if (this._state == MessageTrayState.HIDDEN)
            this._updateState();
    },

    _onMessageTrayLeft: function() {
        if (this._state == MessageTrayState.HIDDEN)
            return;

        // We wait just a little before hiding the message tray in case the
        // user will quickly move the mouse back over it.
        let timeout = MESSAGE_TRAY_TIMEOUT * 1000;
        this._updateTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._updateState));
    },

    // As tray, notification box and summary view are all animated separately,
    // but dependant on each other's states, it appears less confusing
    // handling all transitions in a state machine rather than spread out
    // over different event handlers.
    //
    // State changes are triggered when
    // - a notification arrives (see _onNotify())
    // - the mouse enters the tray (see _onMessageTrayEntered())
    // - the mouse leaves the tray (see _onMessageTrayLeft())
    // - a timeout expires (usually set up in a previous invocation of this function)
    _updateState: function() {
        if (this._updateTimeoutId > 0)
            Mainloop.source_remove(this._updateTimeoutId);

        this._updateTimeoutId = 0;
        let timeout = -1;

        switch (this._state) {
        case MessageTrayState.HIDDEN:
            if (this._notificationQueue.length > 0) {
                this._showNotification();
                this._showTray();
                this._state = MessageTrayState.NOTIFICATION;
                // Because we set up the timeout before we do the animation,
                // we add ANIMATION_TIME to NOTIFICATION_TIMEOUT, so that
                // NOTIFICATION_TIMEOUT represents the time the notifiation
                // is fully shown.
                timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
	    } else {
                this._showSummary();
                this._showTray();
                this._state = MessageTrayState.SUMMARY;
            }
            break;
        case MessageTrayState.NOTIFICATION:
            if (this._notificationQueue.length > 0) {
                this._hideNotification();
                this._state = MessageTrayState.TRAY_ONLY;
                timeout = ANIMATION_TIME * 1000;
            } else {
                this._hideNotification();
                this._showSummary();
                this._state = MessageTrayState.SUMMARY;
                timeout = (ANIMATION_TIME + SUMMARY_TIMEOUT) * 1000;
            }
	    break;
        case MessageTrayState.SUMMARY:
            if (this._notificationQueue.length > 0) {
                this._hideSummary();
                this._showNotification();
                this._state = MessageTrayState.NOTIFICATION;
                timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
            } else {
                this._hideSummary();
                this._hideTray();
                this._state = MessageTrayState.HIDDEN;
            }
            break;
        case MessageTrayState.TRAY_ONLY:
            this._showNotification();
            this._state = MessageTrayState.NOTIFICATION;
            timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
            break;
        }

        if (timeout > -1)
            this._updateTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._updateState));
    },

    _showTray: function() {
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.actor,
                         { y: primary.y + primary.height - this.actor.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    _hideTray: function() {
        let primary = global.get_primary_monitor();

        Tweener.addTween(this.actor,
                         { y: primary.y + primary.height - 1,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
        return false;
    },

    _showNotification: function() {
        this._notificationBox.setContent(this._notificationQueue.shift());

        let notification = this._notificationBox.actor;
        let primary = global.get_primary_monitor();
        notification.opacity = 0;
        notification.y = primary.y + this.actor.height;
        notification.show();
        let futureY = primary.y + this.actor.height - notification.height;

        Tweener.addTween(notification,
                         { y: futureY,
                           opacity: 255,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
    },

    _hideNotification: function() {
        let notification = this._notificationBox.actor;

        Tweener.addTween(notification,
                         { y: notification.y + notification.height,
                           opacity: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: Lang.bind(this, function() {
                               notification.hide();
                           })});
    },

    _showSummary: function() {
        let primary = global.get_primary_monitor();
        this._summaryBin.opacity = 0;
        this._summaryBin.y = this.actor.height;
        this._summaryBin.show();
        Tweener.addTween(this._summaryBin,
                         { y: primary.y + this.actor.height - this._summaryBin.height,
                           opacity: 255,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
    },

    _hideSummary: function() {
        Tweener.addTween(this._summaryBin,
                         { y: this._summaryBin.y + this._summaryBin.height,
                           opacity: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: Lang.bind(this, function() {
                               this._summaryBin.hide();
                           })});
    }
};
