/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Tp = imports.gi.TelepathyGLib;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Telepathy = imports.misc.telepathy;


// See Notification.appendMessage
const SCROLLBACK_IMMEDIATE_TIME = 60; // 1 minute
const SCROLLBACK_RECENT_TIME = 15 * 60; // 15 minutes
const SCROLLBACK_RECENT_LENGTH = 20;
const SCROLLBACK_IDLE_LENGTH = 5;

const NotificationDirection = {
    SENT: 'chat-sent',
    RECEIVED: 'chat-received'
};

let contactFeatures = [Tp.ContactFeature.ALIAS,
                        Tp.ContactFeature.AVATAR_DATA,
                        Tp.ContactFeature.PRESENCE];

// This is GNOME Shell's implementation of the Telepathy 'Client'
// interface. Specifically, the shell is a Telepathy 'Observer', which
// lets us see messages even if they belong to another app (eg,
// Empathy).

function Client() {
    this._init();
};

Client.prototype = {
    _init : function() {
        this._accounts = {};
        this._sources = {};

        // Set up a SimpleObserver, which will call _observeChannels whenever a
        // channel matching its filters is detected.
        // The second argument, recover, means _observeChannels will be run
        // for any existing channel as well.
        let dbus = Tp.DBusDaemon.dup();
        this._observer = Tp.SimpleObserver.new(dbus, true, 'GnomeShell', true,
                                              Lang.bind(this, this._observeChannels));

        // We only care about single-user text-based chats
        let props = {};
        props[Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE] = Tp.IFACE_CHANNEL_TYPE_TEXT;
        props[Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE] = Tp.HandleType.CONTACT;
        this._observer.add_observer_filter(props);

        try {
            this._observer.register();
        } catch (e) {
            throw new Error('Couldn\'t register SimpleObserver. Error: \n' + e);
        }
    },

    _observeChannels: function(observer, account, conn, channels,
                               dispatchOp, requests, context) {
        let connPath = conn.get_object_path();

        let len = channels.length;
        for (let i = 0; i < len; i++) {
            let channel = channels[i];
            let [targetHandle, targetHandleType] = channel.get_handle();

            /* Only observe contact text channels */
            if ((!(channel instanceof Tp.TextChannel)) ||
               targetHandleType != Tp.HandleType.CONTACT)
               continue;

            if (this._sources[connPath + ':' + targetHandle])
                continue;

            let source = new Source(account, conn, chan);

            this._sources[connPath + ':' + targetHandle] = source;
            source.connect('destroy', Lang.bind(this,
                function() {
                    delete this._sources[connPath + ':' + targetHandle];
                }));

            /* Request a TpContact */
            Shell.get_tp_contacts(conn, 1, [targetHandle],
                    contactFeatures.length, contactFeatures,
                    Lang.bind(this,  function (connection, contacts, failed) {
                        if (contacts.length < 1)
                            return;

                        /* We got the TpContact */
                        if (this._sources[connPath + ':' + targetHandle])
                            return;

                        let source = new Source(account, conn, chan, contacts[0]);

                        this._sources[connPath + ':' + targetHandle] = source;
                        source.connect('destroy', Lang.bind(this,
                            function() {
                                delete this._sources[connPath + ':' + targetHandle];
                            }));
                    }));
        }

        // Allow dbus method to return
        context.accept();
    }
};

function Source(account, conn, channel, contact) {
    this._init(account, conn, channel, contact);
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init: function(account, conn, channel, contact) {
        MessageTray.Source.prototype._init.call(this, channel.get_identifier());

        this.isChat = true;

        this._account = account;
        this._contact = contact;

        this._conn = conn;
        this._channel = channel;
        this._closedId = this._channel.connect('invalidated', Lang.bind(this, this._channelClosed));

        this._updateAlias();

        this._notification = new Notification(this);
        this._notification.setUrgency(MessageTray.Urgency.HIGH);

        this._presence = contact.get_presence_type();

        this._channelText = new Telepathy.ChannelText(DBus.session, conn.get_bus_name(), channel.get_object_path());
        this._sentId = this._channelText.connect('Sent', Lang.bind(this, this._messageSent));
        this._receivedId = this._channelText.connect('Received', Lang.bind(this, this._messageReceived));

        this._channelText.ListPendingMessagesRemote(false, Lang.bind(this, this._gotPendingMessages));

        this._setSummaryIcon(this.createNotificationIcon());

        this._notifyAliasId = this._contact.connect('notify::alias', Lang.bind(this, this._updateAlias));
        this._notifyAvatarId = this._contact.connect('notify::avatar-file', Lang.bind(this, this._updateAvatarIcon));
        this._presenceChangedId = this._contact.connect('presence-changed', Lang.bind(this, this._presenceChanged));
    },

    _updateAlias: function() {
        this.title = this._contact.get_alias();
    },

    createNotificationIcon: function() {
        this._iconBox = new St.Bin({ style_class: 'avatar-box' });
        this._iconBox._size = this.ICON_SIZE;

        this._updateAvatarIcon();

        return this._iconBox;
    },

    _updateAvatarIcon: function() {
        let textureCache = St.TextureCache.get_default();
        let file = this._contact.get_avatar_file();

        if (file) {
            let uri = file.get_uri();
            this._iconBox.child = textureCache.load_uri_async(uri, this._iconBox._size, this._iconBox._size);
        } else {
            this._iconBox.child = new St.Icon({ icon_name: 'stock_person',
                                                icon_type: St.IconType.FULLCOLOR,
                                                icon_size: this._iconBox._size });
        }
    },

    _notificationClicked: function(notification) {
        let props = {};
        props[Tp.PROP_CHANNEL_CHANNEL_TYPE] = Tp.IFACE_CHANNEL_TYPE_TEXT;
        [props[Tp.PROP_CHANNEL_TARGET_HANDLE], props[Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE]] = this._channel.get_handle();

        let req = Tp.AccountChannelRequest.new(this._account, props, global.get_current_time());

        req.ensure_channel_async('', null, null);
    },

    _gotPendingMessages: function(msgs, err) {
        if (!msgs)
            return;

        for (let i = 0; i < msgs.length; i++)
            this._messageReceived.apply(this, [this._channel].concat(msgs[i]));
    },

    _channelClosed: function() {
        this._channel.disconnect(this._closedId);
        this._channelText.disconnect(this._receivedId);
        this._channelText.disconnect(this._sentId);

        this._contact.disconnect(this._notifyAliasId);
        this._contact.disconnect(this._notifyAvatarId);
        this._contact.disconnect(this._presenceChangedId);
        this.destroy();
    },

    _messageReceived: function(channel, id, timestamp, sender,
                               type, flags, text) {
        this._notification.appendMessage(text, timestamp, NotificationDirection.RECEIVED);
        this.notify();
    },

    // This is called for both messages we send from
    // our client and other clients as well.
    _messageSent: function(channel, timestamp, type, text) {
        this._notification.appendMessage(text, timestamp, NotificationDirection.SENT);
    },

    notify: function() {
        if (!Main.messageTray.contains(this))
            Main.messageTray.add(this);

        MessageTray.Source.prototype.notify.call(this, this._notification);
    },

    respond: function(text) {
        this._channelText.SendRemote(Tp.ChannelTextMessageType.NORMAL, text);
    },

    _presenceChanged: function (contact, presence, type, status, message) {
        let msg, shouldNotify, title;

        if (this._presence == presence)
          return;

        title = GLib.markup_escape_text(this.title, -1);

        if (presence == Tp.ConnectionPresenceType.AVAILABLE) {
            msg = _("%s is online.").format(title);
            shouldNotify = (this._presence == Tp.ConnectionPresenceType.OFFLINE);
        } else if (presence == Tp.ConnectionPresenceType.OFFLINE ||
                   presence == Tp.ConnectionPresenceType.EXTENDED_AWAY) {
            presence = Tp.ConnectionPresenceType.OFFLINE;
            msg = _("%s is offline.").format(title);
            shouldNotify = (this._presence != Tp.ConnectionPresenceType.OFFLINE);
        } else if (presence == Tp.ConnectionPresenceType.AWAY) {
            msg = _("%s is away.").format(title);
            shouldNotify = false;
        } else if (presence == Tp.ConnectionPresenceType.BUSY) {
            msg = _("%s is busy.").format(title);
            shouldNotify = false;
        } else
            return;

        this._presence = presence;

        if (message)
            msg += ' <i>(' + GLib.markup_escape_text(message, -1) + ')</i>';

        this._notification.appendPresence(msg, shouldNotify);
        if (shouldNotify)
            this.notify();
    }
};

function Notification(source) {
    this._init(source);
}

Notification.prototype = {
    __proto__:  MessageTray.Notification.prototype,

    _init: function(source) {
        MessageTray.Notification.prototype._init.call(this, source, source.title, null, { customContent: true });
        this.setResident(true);

        this._responseEntry = new St.Entry({ style_class: 'chat-response',
                                             can_focus: true });
        this._responseEntry.clutter_text.connect('activate', Lang.bind(this, this._onEntryActivated));
        this.setActionArea(this._responseEntry);

        this._history = [];
        this._timestampTimeoutId = 0;
    },

    appendMessage: function(text, timestamp, direction) {
        this.update(this.source.title, text, { customContent: true });
        this._append(text, direction, timestamp);
    },

    _append: function(text, style, timestamp) {
        let currentTime = (Date.now() / 1000);
        if (!timestamp)
            timestamp = currentTime;
        let lastMessageTime = -1;
        if (this._history.length > 0)
            lastMessageTime = this._history[0].time;

        // Reset the old message timeout
        if (this._timestampTimeoutId)
            Mainloop.source_remove(this._timestampTimeoutId);

        let body = this.addBody(text);
        body.add_style_class_name(style);
        this.scrollTo(St.Side.BOTTOM);

        this._history.unshift({ actor: body, time: timestamp, realMessage: true });

        if (timestamp < currentTime - SCROLLBACK_IMMEDIATE_TIME)
            this._appendTimestamp();
        else
            // Schedule a new timestamp in SCROLLBACK_IMMEDIATE_TIME
            // from the timestamp of the message.
            this._timestampTimeoutId = Mainloop.timeout_add_seconds(
                SCROLLBACK_IMMEDIATE_TIME - (currentTime - timestamp),
                Lang.bind(this, this._appendTimestamp));

        if (this._history.length > 1) {
            // Keep the scrollback from growing too long. If the most
            // recent message (before the one we just added) is within
            // SCROLLBACK_RECENT_TIME, we will keep
            // SCROLLBACK_RECENT_LENGTH previous messages. Otherwise
            // we'll keep SCROLLBACK_IDLE_LENGTH messages.

            let maxLength = (lastMessageTime < currentTime - SCROLLBACK_RECENT_TIME) ?
                SCROLLBACK_IDLE_LENGTH : SCROLLBACK_RECENT_LENGTH;
            let filteredHistory = this._history.filter(function(item) { return item.realMessage });
            if (filteredHistory.length > maxLength) {
                let lastMessageToKeep = filteredHistory[maxLength];
                let expired = this._history.splice(this._history.indexOf(lastMessageToKeep));
                for (let i = 0; i < expired.length; i++)
                    expired[i].actor.destroy();
            }
        }
    },

    _appendTimestamp: function() {
        let lastMessageTime = this._history[0].time;
        let lastMessageDate = new Date(lastMessageTime * 1000);

        /* Translators: this is a time format string followed by a date.
           If applicable, replace %X with a strftime format valid for your
           locale, without seconds. */
        // xgettext:no-c-format
        let timeLabel = this.addBody(lastMessageDate.toLocaleFormat(_("Sent at %X on %A")), false, { expand: true, x_fill: false, x_align: St.Align.END });
        timeLabel.add_style_class_name('chat-meta-message');
        this._history.unshift({ actor: timeLabel, time: lastMessageTime, realMessage: false });

        this._timestampTimeoutId = 0;
        return false;
    },

    appendPresence: function(text, asTitle) {
        if (asTitle)
            this.update(text, null, { customContent: true, titleMarkup: true });
        else
            this.update(this.source.title, null, { customContent: true });
        let label = this.addBody(text, true);
        label.add_style_class_name('chat-meta-message');
        this._history.unshift({ actor: label, time: (Date.now() / 1000), realMessage: false});
    },

    _onEntryActivated: function() {
        let text = this._responseEntry.get_text();
        if (text == '')
            return;

        // Telepathy sends out the Sent signal for us.
        // see Source._messageSent
        this._responseEntry.set_text('');
        this.source.respond(text);
    }
};
