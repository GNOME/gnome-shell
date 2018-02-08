// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const St = imports.gi.St;

var Tpl = null;
var Tp = null;
try {
    Tpl = imports.gi.TelepathyLogger;
    Tp = imports.gi.TelepathyGLib;
} catch(e) {
    log('Telepathy is not available, chat integration will be disabled.');
}

const History = imports.misc.history;
const Main = imports.ui.main;
const MessageList = imports.ui.messageList;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

const HAVE_TP = (Tp != null && Tpl != null);

// See Notification.appendMessage
var SCROLLBACK_IMMEDIATE_TIME = 3 * 60; // 3 minutes
var SCROLLBACK_RECENT_TIME = 15 * 60; // 15 minutes
var SCROLLBACK_RECENT_LENGTH = 20;
var SCROLLBACK_IDLE_LENGTH = 5;

// See Source._displayPendingMessages
var SCROLLBACK_HISTORY_LINES = 10;

// See Notification._onEntryChanged
var COMPOSING_STOP_TIMEOUT = 5;

var CHAT_EXPAND_LINES = 12;

var NotificationDirection = {
    SENT: 'chat-sent',
    RECEIVED: 'chat-received'
};

var N_ = function(s) { return s; };

function makeMessageFromTpMessage(tpMessage, direction) {
    let [text, flags] = tpMessage.to_text();

    let timestamp = tpMessage.get_sent_timestamp();
    if (timestamp == 0)
        timestamp = tpMessage.get_received_timestamp();

    return {
        messageType: tpMessage.get_message_type(),
        text: text,
        sender: tpMessage.sender.alias,
        timestamp: timestamp,
        direction: direction
    };
}


function makeMessageFromTplEvent(event) {
    let sent = event.get_sender().get_entity_type() == Tpl.EntityType.SELF;
    let direction = sent ? NotificationDirection.SENT : NotificationDirection.RECEIVED;

    return {
        messageType: event.get_message_type(),
        text: event.get_message(),
        sender: event.get_sender().get_alias(),
        timestamp: event.get_timestamp(),
        direction: direction
    };
}

var TelepathyComponent = new Lang.Class({
    Name: 'TelepathyComponent',

    _init() {
        this._client = null;

        if (!HAVE_TP)
            return; // Telepathy isn't available

        this._client = new TelepathyClient();
    },

    enable() {
        if (!this._client)
            return;

        try {
            this._client.register();
        } catch (e) {
            throw new Error('Couldn\'t register Telepathy client. Error: \n' + e);
        }

        if (!this._client.account_manager.is_prepared(Tp.AccountManager.get_feature_quark_core()))
            this._client.account_manager.prepare_async(null, null);
    },

    disable() {
        if (!this._client)
            return;

        this._client.unregister();
    }
});

var TelepathyClient = HAVE_TP ? new Lang.Class({
    Name: 'TelepathyClient',
    Extends: Tp.BaseClient,

    _init() {
        // channel path -> ChatSource
        this._chatSources = {};
        this._chatState = Tp.ChannelChatState.ACTIVE;

        // account path -> AccountNotification
        this._accountNotifications = {};

        // Define features we want
        this._accountManager = Tp.AccountManager.dup();
        let factory = this._accountManager.get_factory();
        factory.add_account_features([Tp.Account.get_feature_quark_connection()]);
        factory.add_connection_features([Tp.Connection.get_feature_quark_contact_list()]);
        factory.add_channel_features([Tp.Channel.get_feature_quark_contacts()]);
        factory.add_contact_features([Tp.ContactFeature.ALIAS,
                                      Tp.ContactFeature.AVATAR_DATA,
                                      Tp.ContactFeature.PRESENCE,
                                      Tp.ContactFeature.SUBSCRIPTION_STATES]);

        // Set up a SimpleObserver, which will call _observeChannels whenever a
        // channel matching its filters is detected.
        // The second argument, recover, means _observeChannels will be run
        // for any existing channel as well.
        this.parent({ name: 'GnomeShell',
                      account_manager: this._accountManager,
                      uniquify_name: true });

        // We only care about single-user text-based chats
        let filter = {};
        filter[Tp.PROP_CHANNEL_CHANNEL_TYPE] = Tp.IFACE_CHANNEL_TYPE_TEXT;
        filter[Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE] = Tp.HandleType.CONTACT;

        this.set_observer_recover(true);
        this.add_observer_filter(filter);
        this.add_approver_filter(filter);
        this.add_handler_filter(filter);

        // Allow other clients (such as Empathy) to pre-empt our channels if
        // needed
        this.set_delegated_channels_callback(
            Lang.bind(this, this._delegatedChannelsCb));
    },

    vfunc_observe_channels(account, conn, channels,
                                     dispatchOp, requests, context) {
        let len = channels.length;
        for (let i = 0; i < len; i++) {
            let channel = channels[i];
            let [targetHandle, targetHandleType] = channel.get_handle();

            if (channel.get_invalidated())
              continue;

            /* Only observe contact text channels */
            if ((!(channel instanceof Tp.TextChannel)) ||
               targetHandleType != Tp.HandleType.CONTACT)
               continue;

            this._createChatSource(account, conn, channel, channel.get_target_contact());
        }

        context.accept();
    },

    _createChatSource(account, conn, channel, contact) {
        if (this._chatSources[channel.get_object_path()])
            return;

        let source = new ChatSource(account, conn, channel, contact, this);

        this._chatSources[channel.get_object_path()] = source;
        source.connect('destroy', Lang.bind(this,
                       function() {
                           delete this._chatSources[channel.get_object_path()];
                       }));
    },

    vfunc_handle_channels(account, conn, channels, requests,
                                    user_action_time, context) {
        this._handlingChannels(account, conn, channels, true);
        context.accept();
    },

    _handlingChannels(account, conn, channels, notify) {
        let len = channels.length;
        for (let i = 0; i < len; i++) {
            let channel = channels[i];

            // We can only handle text channel, so close any other channel
            if (!(channel instanceof Tp.TextChannel)) {
                channel.close_async(null);
                continue;
            }

            if (channel.get_invalidated())
              continue;

            // 'notify' will be true when coming from an actual HandleChannels
            // call, and not when from a successful Claim call. The point is
            // we don't want to notify for a channel we just claimed which
            // has no new messages (for example, a new channel which only has
            // a delivery notification). We rely on _displayPendingMessages()
            // and _messageReceived() to notify for new messages.

            // But we should still notify from HandleChannels because the
            // Telepathy spec states that handlers must foreground channels
            // in HandleChannels calls which are already being handled.

            if (notify && this.is_handling_channel(channel)) {
                // We are already handling the channel, display the source
                let source = this._chatSources[channel.get_object_path()];
                if (source)
                    source.notify();
            }
        }
    },

    vfunc_add_dispatch_operation(account, conn, channels,
                                           dispatchOp, context) {
        let channel = channels[0];
        let chanType = channel.get_channel_type();

        if (channel.get_invalidated()) {
            context.fail(new Tp.Error({ code: Tp.Error.INVALID_ARGUMENT,
                                        message: 'Channel is invalidated' }));
            return;
        }

        if (chanType == Tp.IFACE_CHANNEL_TYPE_TEXT)
            this._approveTextChannel(account, conn, channel, dispatchOp, context);
        else
            context.fail(new Tp.Error({ code: Tp.Error.INVALID_ARGUMENT,
                                        message: 'Unsupported channel type' }));
    },

    _approveTextChannel(account, conn, channel, dispatchOp, context) {
        let [targetHandle, targetHandleType] = channel.get_handle();

        if (targetHandleType != Tp.HandleType.CONTACT) {
            context.fail(new Tp.Error({ code: Tp.Error.INVALID_ARGUMENT,
                                        message: 'Unsupported handle type' }));
            return;
        }

        // Approve private text channels right away as we are going to handle it
        dispatchOp.claim_with_async(this, Lang.bind(this, function(dispatchOp, result) {
            try {
                dispatchOp.claim_with_finish(result);
                this._handlingChannels(account, conn, [channel], false);
            } catch (err) {
                log('Failed to Claim channel: ' + err);
            }
        }));

        context.accept();
    },

    _delegatedChannelsCb(client, channels) {
        // Nothing to do as we don't make a distinction between observed and
        // handled channels.
    },
}) : null;

var ChatSource = new Lang.Class({
    Name: 'ChatSource',
    Extends: MessageTray.Source,

    _init(account, conn, channel, contact, client) {
        this._account = account;
        this._contact = contact;
        this._client = client;

        this.parent(contact.get_alias());

        this.isChat = true;
        this._pendingMessages = [];

        this._conn = conn;
        this._channel = channel;
        this._closedId = this._channel.connect('invalidated', Lang.bind(this, this._channelClosed));

        this._notifyTimeoutId = 0;

        this._presence = contact.get_presence_type();

        this._sentId = this._channel.connect('message-sent', Lang.bind(this, this._messageSent));
        this._receivedId = this._channel.connect('message-received', Lang.bind(this, this._messageReceived));
        this._pendingId = this._channel.connect('pending-message-removed', Lang.bind(this, this._pendingRemoved));

        this._notifyAliasId = this._contact.connect('notify::alias', Lang.bind(this, this._updateAlias));
        this._notifyAvatarId = this._contact.connect('notify::avatar-file', Lang.bind(this, this._updateAvatarIcon));
        this._presenceChangedId = this._contact.connect('presence-changed', Lang.bind(this, this._presenceChanged));

        // Add ourselves as a source.
        Main.messageTray.add(this);

        this._getLogMessages();
    },

    _ensureNotification() {
        if (this._notification)
            return;

        this._notification = new ChatNotification(this);
        this._notification.connect('activated', Lang.bind(this, this.open));
        this._notification.connect('updated', Lang.bind(this,
            function() {
                if (this._banner && this._banner.expanded)
                    this._ackMessages();
            }));
        this._notification.connect('destroy', Lang.bind(this,
            function() {
                this._notification = null;
            }));
        this.pushNotification(this._notification);
    },

    _createPolicy() {
        if (this._account.protocol_name == 'irc')
            return new MessageTray.NotificationApplicationPolicy('org.gnome.Polari');
        return new MessageTray.NotificationApplicationPolicy('empathy');
    },

    createBanner() {
        this._banner = new ChatNotificationBanner(this._notification);

        // We ack messages when the user expands the new notification
        let id = this._banner.connect('expanded', Lang.bind(this, this._ackMessages));
        this._banner.actor.connect('destroy', Lang.bind(this,
            function() {
                this._banner.disconnect(id);
                this._banner = null;
            }));

        return this._banner;
    },

    _updateAlias() {
        let oldAlias = this.title;
        let newAlias = this._contact.get_alias();

        if (oldAlias == newAlias)
            return;

        this.setTitle(newAlias);
        if (this._notification)
            this._notification.appendAliasChange(oldAlias, newAlias);
    },

    getIcon() {
        let file = this._contact.get_avatar_file();
        if (file) {
            return new Gio.FileIcon({ file: file });
        } else {
            return new Gio.ThemedIcon({ name: 'avatar-default' });
        }
    },

    getSecondaryIcon() {
        let iconName;
        let presenceType = this._contact.get_presence_type();

        switch (presenceType) {
            case Tp.ConnectionPresenceType.AVAILABLE:
                iconName = 'user-available';
                break;
            case Tp.ConnectionPresenceType.BUSY:
                iconName = 'user-busy';
                break;
            case Tp.ConnectionPresenceType.OFFLINE:
                iconName = 'user-offline';
                break;
            case Tp.ConnectionPresenceType.HIDDEN:
                iconName = 'user-invisible';
                break;
            case Tp.ConnectionPresenceType.AWAY:
                iconName = 'user-away';
                break;
            case Tp.ConnectionPresenceType.EXTENDED_AWAY:
                iconName = 'user-idle';
                break;
            default:
                iconName = 'user-offline';
       }
       return new Gio.ThemedIcon({ name: iconName });
    },

    _updateAvatarIcon() {
        this.iconUpdated();
        if (this._notifiction)
            this._notification.update(this._notification.title,
                                      this._notification.bannerBodyText,
                                      { gicon: this.getIcon() });
    },

    open() {
        Main.overview.hide();
        Main.panel.closeCalendar();

        if (this._client.is_handling_channel(this._channel)) {
            // We are handling the channel, try to pass it to Empathy or Polari
            // (depending on the channel type)
            // We don't check if either app is availble - mission control will
            // fallback to something else if activation fails

            let target;
            if (this._channel.connection.protocol_name == 'irc')
                target = 'org.freedesktop.Telepathy.Client.Polari';
            else
                target = 'org.freedesktop.Telepathy.Client.Empathy.Chat';
            this._client.delegate_channels_async([this._channel], global.get_current_time(), target, null);
        } else {
            // We are not the handler, just ask to present the channel
            let dbus = Tp.DBusDaemon.dup();
            let cd = Tp.ChannelDispatcher.new(dbus);

            cd.present_channel_async(this._channel, global.get_current_time(), null);
        }
    },

    _getLogMessages() {
        let logManager = Tpl.LogManager.dup_singleton();
        let entity = Tpl.Entity.new_from_tp_contact(this._contact, Tpl.EntityType.CONTACT);

        logManager.get_filtered_events_async(this._account, entity,
                                             Tpl.EventTypeMask.TEXT, SCROLLBACK_HISTORY_LINES,
                                             null, Lang.bind(this, this._displayPendingMessages));
    },

    _displayPendingMessages(logManager, result) {
        let [success, events] = logManager.get_filtered_events_finish(result);

        let logMessages = events.map(makeMessageFromTplEvent);
        this._ensureNotification();

        let pendingTpMessages = this._channel.get_pending_messages();
        let pendingMessages = [];

        for (let i = 0; i < pendingTpMessages.length; i++) {
            let message = pendingTpMessages[i];

            if (message.get_message_type() == Tp.ChannelTextMessageType.DELIVERY_REPORT)
                continue;

            pendingMessages.push(makeMessageFromTpMessage(message, NotificationDirection.RECEIVED));

            this._pendingMessages.push(message);
        }

        this.countUpdated();

        let showTimestamp = false;

        for (let i = 0; i < logMessages.length; i++) {
            let logMessage = logMessages[i];
            let isPending = false;

            // Skip any log messages that are also in pendingMessages
            for (let j = 0; j < pendingMessages.length; j++) {
                let pending = pendingMessages[j];
                if (logMessage.timestamp == pending.timestamp && logMessage.text == pending.text) {
                    isPending = true;
                    break;
                }
            }

            if (!isPending) {
                showTimestamp = true;
                this._notification.appendMessage(logMessage, true, ['chat-log-message']);
            }
        }

        if (showTimestamp)
            this._notification.appendTimestamp();

        for (let i = 0; i < pendingMessages.length; i++)
            this._notification.appendMessage(pendingMessages[i], true);

        if (pendingMessages.length > 0)
            this.notify();
    },

    destroy(reason) {
        if (this._client.is_handling_channel(this._channel)) {
            this._ackMessages();
            // The chat box has been destroyed so it can't
            // handle the channel any more.
            this._channel.close_async(function(channel, result) {
                channel.close_finish(result);
            });
        } else {
            // Don't indicate any unread messages when the notification
            // that represents them has been destroyed.
            this._pendingMessages = [];
            this.countUpdated();
        }

        // Keep source alive while the channel is open
        if (reason != MessageTray.NotificationDestroyedReason.SOURCE_CLOSED)
            return;

        if (this._destroyed)
            return;

        this._destroyed = true;
        this._channel.disconnect(this._closedId);
        this._channel.disconnect(this._receivedId);
        this._channel.disconnect(this._pendingId);
        this._channel.disconnect(this._sentId);

        this._contact.disconnect(this._notifyAliasId);
        this._contact.disconnect(this._notifyAvatarId);
        this._contact.disconnect(this._presenceChangedId);

        this.parent(reason);
    },

    _channelClosed() {
        this.destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
    },

    /* All messages are new messages for Telepathy sources */
    get count() {
        return this._pendingMessages.length;
    },

    get unseenCount() {
        return this.count;
    },

    get countVisible() {
        return this.count > 0;
    },

    _messageReceived(channel, message) {
        if (message.get_message_type() == Tp.ChannelTextMessageType.DELIVERY_REPORT)
            return;

        this._ensureNotification();
        this._pendingMessages.push(message);
        this.countUpdated();

        message = makeMessageFromTpMessage(message, NotificationDirection.RECEIVED);
        this._notification.appendMessage(message);

        // Wait a bit before notifying for the received message, a handler
        // could ack it in the meantime.
        if (this._notifyTimeoutId != 0)
            Mainloop.source_remove(this._notifyTimeoutId);
        this._notifyTimeoutId = Mainloop.timeout_add(500,
            Lang.bind(this, this._notifyTimeout));
        GLib.Source.set_name_by_id(this._notifyTimeoutId, '[gnome-shell] this._notifyTimeout');
    },

    _notifyTimeout() {
        if (this._pendingMessages.length != 0)
            this.notify();

        this._notifyTimeoutId = 0;

        return GLib.SOURCE_REMOVE;
    },

    // This is called for both messages we send from
    // our client and other clients as well.
    _messageSent(channel, message, flags, token) {
        this._ensureNotification();
        message = makeMessageFromTpMessage(message, NotificationDirection.SENT);
        this._notification.appendMessage(message);
    },

    notify() {
        this.parent(this._notification);
    },

    respond(text) {
        let type;
        if (text.slice(0, 4) == '/me ') {
            type = Tp.ChannelTextMessageType.ACTION;
            text = text.slice(4);
        } else {
            type = Tp.ChannelTextMessageType.NORMAL;
        }

        let msg = Tp.ClientMessage.new_text(type, text);
        this._channel.send_message_async(msg, 0, Lang.bind(this, function (src, result) {
            this._channel.send_message_finish(result); 
        }));
    },

    setChatState(state) {
        // We don't want to send COMPOSING every time a letter is typed into
        // the entry. We send the state only when it changes. Telepathy/Empathy
        // might change it behind our back if the user is using both
        // gnome-shell's entry and the Empathy conversation window. We could
        // keep track of it with the ChatStateChanged signal but it is good
        // enough right now.
        if (state != this._chatState) {
          this._chatState = state;
          this._channel.set_chat_state_async(state, null);
        }
    },

    _presenceChanged (contact, presence, status, message) {
        if (this._notification)
            this._notification.update(this._notification.title,
                                      this._notification.bannerBodyText,
                                      { secondaryGIcon: this.getSecondaryIcon() });
    },

    _pendingRemoved(channel, message) {
        let idx = this._pendingMessages.indexOf(message);

        if (idx >= 0) {
            this._pendingMessages.splice(idx, 1);
            this.countUpdated();
        }

        if (this._pendingMessages.length == 0 &&
            this._banner && !this._banner.expanded)
            this._banner.hide();
    },

    _ackMessages() {
        // Don't clear our messages here, tp-glib will send a
        // 'pending-message-removed' for each one.
        this._channel.ack_all_pending_messages_async(null);
    }
});

var ChatNotification = new Lang.Class({
    Name: 'ChatNotification',
    Extends: MessageTray.Notification,

    _init(source) {
        this.parent(source, source.title, null,
                    { secondaryGIcon: source.getSecondaryIcon() });
        this.setUrgency(MessageTray.Urgency.HIGH);
        this.setResident(true);

        this.messages = [];
        this._timestampTimeoutId = 0;
    },

    destroy(reason) {
        if (this._timestampTimeoutId)
            Mainloop.source_remove(this._timestampTimeoutId);
        this._timestampTimeoutId = 0;
        this.parent(reason);
    },

    /**
     * appendMessage:
     * @message: An object with the properties:
     *   text: the body of the message,
     *   messageType: a #Tp.ChannelTextMessageType,
     *   sender: the name of the sender,
     *   timestamp: the time the message was sent
     *   direction: a #NotificationDirection
     * 
     * @noTimestamp: Whether to add a timestamp. If %true, no timestamp
     *   will be added, regardless of the difference since the
     *   last timestamp
     */
    appendMessage(message, noTimestamp) {
        let messageBody = GLib.markup_escape_text(message.text, -1);
        let styles = [message.direction];

        if (message.messageType == Tp.ChannelTextMessageType.ACTION) {
            let senderAlias = GLib.markup_escape_text(message.sender, -1);
            messageBody = '<i>%s</i> %s'.format(senderAlias, messageBody);
            styles.push('chat-action');
        }

        if (message.direction == NotificationDirection.RECEIVED)
            this.update(this.source.title, messageBody,
                        { datetime: GLib.DateTime.new_from_unix_local (message.timestamp),
                          bannerMarkup: true });

        let group = (message.direction == NotificationDirection.RECEIVED ?
                     'received' : 'sent');

        this._append({ body: messageBody,
                       group: group,
                       styles: styles,
                       timestamp: message.timestamp,
                       noTimestamp: noTimestamp });
    },

    _filterMessages() {
        if (this.messages.length < 1)
            return;

        let lastMessageTime = this.messages[0].timestamp;
        let currentTime = (Date.now() / 1000);

        // Keep the scrollback from growing too long. If the most
        // recent message (before the one we just added) is within
        // SCROLLBACK_RECENT_TIME, we will keep
        // SCROLLBACK_RECENT_LENGTH previous messages. Otherwise
        // we'll keep SCROLLBACK_IDLE_LENGTH messages.

        let maxLength = (lastMessageTime < currentTime - SCROLLBACK_RECENT_TIME) ?
            SCROLLBACK_IDLE_LENGTH : SCROLLBACK_RECENT_LENGTH;

        let filteredHistory = this.messages.filter(function(item) { return item.realMessage });
        if (filteredHistory.length > maxLength) {
            let lastMessageToKeep = filteredHistory[maxLength];
            let expired = this.messages.splice(this.messages.indexOf(lastMessageToKeep));
            for (let i = 0; i < expired.length; i++)
                this.emit('message-removed', expired[i]);
        }
    },

    /**
     * _append:
     * @props: An object with the properties:
     *  body: The text of the message.
     *  group: The group of the message, one of:
     *         'received', 'sent', 'meta'.
     *  styles: Style class names for the message to have.
     *  timestamp: The timestamp of the message.
     *  noTimestamp: suppress timestamp signal?
     */
    _append(props) {
        let currentTime = (Date.now() / 1000);
        props = Params.parse(props, { body: null,
                                      group: null,
                                      styles: [],
                                      timestamp: currentTime,
                                      noTimestamp: false });

        // Reset the old message timeout
        if (this._timestampTimeoutId)
            Mainloop.source_remove(this._timestampTimeoutId);
        this._timestampTimeoutId = 0;

        let message = { realMessage: props.group != 'meta',
                        showTimestamp: false };
        Lang.copyProperties(props, message);
        delete message.noTimestamp;

        this.messages.unshift(message);
        this.emit('message-added', message);

        if (!props.noTimestamp) {
            let timestamp = props.timestamp;
            if (timestamp < currentTime - SCROLLBACK_IMMEDIATE_TIME) {
                this.appendTimestamp();
            } else {
                // Schedule a new timestamp in SCROLLBACK_IMMEDIATE_TIME
                // from the timestamp of the message.
                this._timestampTimeoutId = Mainloop.timeout_add_seconds(
                    SCROLLBACK_IMMEDIATE_TIME - (currentTime - timestamp),
                    Lang.bind(this, this.appendTimestamp));
                GLib.Source.set_name_by_id(this._timestampTimeoutId, '[gnome-shell] this.appendTimestamp');
            }
        }

        this._filterMessages();
    },

    appendTimestamp() {
        this._timestampTimeoutId = 0;

        this.messages[0].showTimestamp = true;
        this.emit('timestamp-changed', this.messages[0]);

        this._filterMessages();

        return GLib.SOURCE_REMOVE;
    },

    appendAliasChange(oldAlias, newAlias) {
        oldAlias = GLib.markup_escape_text(oldAlias, -1);
        newAlias = GLib.markup_escape_text(newAlias, -1);

        /* Translators: this is the other person changing their old IM name to their new
           IM name. */
        let message = '<i>' + _("%s is now known as %s").format(oldAlias, newAlias) + '</i>';

        this._append({ body: message,
                       group: 'meta',
                       styles: ['chat-meta-message'] });

        this._filterMessages();
    }
});

var ChatLineBox = new Lang.Class({
    Name: 'ChatLineBox',
    Extends: St.BoxLayout,

    vfunc_get_preferred_height(forWidth) {
        let [, natHeight] = this.parent(forWidth);
        return [natHeight, natHeight];
    }
});

var ChatNotificationBanner = new Lang.Class({
    Name: 'ChatNotificationBanner',
    Extends: MessageTray.NotificationBanner,

    _init(notification) {
        this.parent(notification);

        this._responseEntry = new St.Entry({ style_class: 'chat-response',
                                             x_expand: true,
                                             can_focus: true });
        this._responseEntry.clutter_text.connect('activate', Lang.bind(this, this._onEntryActivated));
        this._responseEntry.clutter_text.connect('text-changed', Lang.bind(this, this._onEntryChanged));
        this.setActionArea(this._responseEntry);

        this._responseEntry.clutter_text.connect('key-focus-in', Lang.bind(this, function() {
            this.focused = true;
        }));
        this._responseEntry.clutter_text.connect('key-focus-out', Lang.bind(this, function() {
            this.focused = false;
            this.emit('unfocused');
        }));

        this._scrollArea = new St.ScrollView({ style_class: 'chat-scrollview vfade',
                                               vscrollbar_policy: Gtk.PolicyType.AUTOMATIC,
                                               hscrollbar_policy: Gtk.PolicyType.NEVER,
                                               visible: this.expanded });
        this._contentArea = new St.BoxLayout({ style_class: 'chat-body',
                                               vertical: true });
        this._scrollArea.add_actor(this._contentArea);

        this.setExpandedBody(this._scrollArea);
        this.setExpandedLines(CHAT_EXPAND_LINES);

        this._lastGroup = null;

        // Keep track of the bottom position for the current adjustment and
        // force a scroll to the bottom if things change while we were at the
        // bottom
        this._oldMaxScrollValue = this._scrollArea.vscroll.adjustment.value;
        this._scrollArea.vscroll.adjustment.connect('changed', Lang.bind(this, function(adjustment) {
            if (adjustment.value == this._oldMaxScrollValue)
                this.scrollTo(St.Side.BOTTOM);
            this._oldMaxScrollValue = Math.max(adjustment.lower, adjustment.upper - adjustment.page_size);
        }));

        this._inputHistory = new History.HistoryManager({ entry: this._responseEntry.clutter_text });

        this._composingTimeoutId = 0;

        this._messageActors = new Map();

        this._messageAddedId = this.notification.connect('message-added',
            Lang.bind(this, function(n, message) {
                this._addMessage(message);
            }));
        this._messageRemovedId = this.notification.connect('message-removed',
            Lang.bind(this, function(n, message) {
                let actor = this._messageActors.get(message);
                if (this._messageActors.delete(message))
                    actor.destroy();
            }));
        this._timestampChangedId = this.notification.connect('timestamp-changed',
            Lang.bind(this, function(n, message) {
                this._updateTimestamp(message);
            }));

        for (let i = this.notification.messages.length - 1; i >= 0; i--)
            this._addMessage(this.notification.messages[i]);
    },

    _onDestroy() {
        this.parent();
        this.notification.disconnect(this._messageAddedId);
        this.notification.disconnect(this._messageRemovedId);
        this.notification.disconnect(this._timestampChangedId);
    },

    scrollTo(side) {
        let adjustment = this._scrollArea.vscroll.adjustment;
        if (side == St.Side.TOP)
            adjustment.value = adjustment.lower;
        else if (side == St.Side.BOTTOM)
            adjustment.value = adjustment.upper;
    },

    hide() {
        this.emit('done-displaying');
    },

    _addMessage(message) {
        let highlighter = new MessageList.URLHighlighter(message.body, true, true);
        let body = highlighter.actor;

        let styles = message.styles;
        for (let i = 0; i < styles.length; i++)
            body.add_style_class_name(styles[i]);

        let group = message.group;
        if (group != this._lastGroup) {
            this._lastGroup = group;
            body.add_style_class_name('chat-new-group');
        }

        let lineBox = new ChatLineBox();
        lineBox.add(body);
        this._contentArea.add_actor(lineBox);
        this._messageActors.set(message, lineBox);

        this._updateTimestamp(message);
    },

    _updateTimestamp(message) {
        let actor = this._messageActors.get(message);
        if (!actor)
            return;

        while (actor.get_n_children() > 1)
            actor.get_child_at_index(1).destroy();

        if (message.showTimestamp) {
            let lastMessageTime = message.timestamp;
            let lastMessageDate = new Date(lastMessageTime * 1000);

            let timeLabel = Util.createTimeLabel(lastMessageDate);
            timeLabel.style_class = 'chat-meta-message';
            timeLabel.x_expand = timeLabel.y_expand = true;
            timeLabel.x_align = timeLabel.y_align = Clutter.ActorAlign.END;

            actor.add_actor(timeLabel);
        }
    },

    _onEntryActivated() {
        let text = this._responseEntry.get_text();
        if (text == '')
            return;

        this._inputHistory.addItem(text);

        // Telepathy sends out the Sent signal for us.
        // see Source._messageSent
        this._responseEntry.set_text('');
        this.notification.source.respond(text);
    },

    _composingStopTimeout() {
        this._composingTimeoutId = 0;

        this.notification.source.setChatState(Tp.ChannelChatState.PAUSED);

        return GLib.SOURCE_REMOVE;
    },

    _onEntryChanged() {
        let text = this._responseEntry.get_text();

        // If we're typing, we want to send COMPOSING.
        // If we empty the entry, we want to send ACTIVE.
        // If we've stopped typing for COMPOSING_STOP_TIMEOUT
        //    seconds, we want to send PAUSED.

        // Remove composing timeout.
        if (this._composingTimeoutId > 0) {
            Mainloop.source_remove(this._composingTimeoutId);
            this._composingTimeoutId = 0;
        }

        if (text != '') {
            this.notification.source.setChatState(Tp.ChannelChatState.COMPOSING);

            this._composingTimeoutId = Mainloop.timeout_add_seconds(
                COMPOSING_STOP_TIMEOUT,
                Lang.bind(this, this._composingStopTimeout));
            GLib.Source.set_name_by_id(this._composingTimeoutId, '[gnome-shell] this._composingStopTimeout');
        } else {
            this.notification.source.setChatState(Tp.ChannelChatState.ACTIVE);
        }
    }
});

var Component = TelepathyComponent;
