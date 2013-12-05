// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Tpl = imports.gi.TelepathyLogger;
const Tp = imports.gi.TelepathyGLib;

const History = imports.misc.history;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;

// See Notification.appendMessage
const SCROLLBACK_IMMEDIATE_TIME = 3 * 60; // 3 minutes
const SCROLLBACK_RECENT_TIME = 15 * 60; // 15 minutes
const SCROLLBACK_RECENT_LENGTH = 20;
const SCROLLBACK_IDLE_LENGTH = 5;

// See Source._displayPendingMessages
const SCROLLBACK_HISTORY_LINES = 10;

// See Notification._onEntryChanged
const COMPOSING_STOP_TIMEOUT = 5;

const CLOCK_FORMAT_KEY = 'clock-format';

const NotificationDirection = {
    SENT: 'chat-sent',
    RECEIVED: 'chat-received'
};

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

const TelepathyClient = new Lang.Class({
    Name: 'TelepathyClient',

    _init: function() {
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
        this._tpClient = new Shell.TpClient({ name: 'GnomeShell',
                                              account_manager: this._accountManager,
                                              uniquify_name: true });
        this._tpClient.set_observe_channels_func(
            Lang.bind(this, this._observeChannels));
        this._tpClient.set_approve_channels_func(
            Lang.bind(this, this._approveChannels));
        this._tpClient.set_handle_channels_func(
            Lang.bind(this, this._handleChannels));

        // Allow other clients (such as Empathy) to pre-empt our channels if
        // needed
        this._tpClient.set_delegated_channels_callback(
            Lang.bind(this, this._delegatedChannelsCb));
    },

    enable: function() {
        try {
            this._tpClient.register();
        } catch (e) {
            throw new Error('Couldn\'t register Telepathy client. Error: \n' + e);
        }

        if (!this._accountManager.is_prepared(Tp.AccountManager.get_feature_quark_core()))
            this._accountManager.prepare_async(null, Lang.bind(this, this._accountManagerPrepared));
    },

    disable: function() {
        this._tpClient.unregister();
    },

    _observeChannels: function(observer, account, conn, channels,
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

    _createChatSource: function(account, conn, channel, contact) {
        if (this._chatSources[channel.get_object_path()])
            return;

        let source = new ChatSource(account, conn, channel, contact, this._tpClient);

        this._chatSources[channel.get_object_path()] = source;
        source.connect('destroy', Lang.bind(this,
                       function() {
                           if (this._tpClient.is_handling_channel(channel)) {
                               // The chat box has been destroyed so it can't
                               // handle the channel any more.
                               channel.close_async(function(src, result) {
                                   channel.close_finish(result);
                               });
                           }

                           delete this._chatSources[channel.get_object_path()];
                       }));
    },

    _handleChannels: function(handler, account, conn, channels,
                              requests, user_action_time, context) {
        this._handlingChannels(account, conn, channels, true);
        context.accept();
    },

    _handlingChannels: function(account, conn, channels, notify) {
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

            if (notify && this._tpClient.is_handling_channel(channel)) {
                // We are already handling the channel, display the source
                let source = this._chatSources[channel.get_object_path()];
                if (source)
                    source.notify();
            }
        }
    },

    _approveChannels: function(approver, account, conn, channels,
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

    _approveTextChannel: function(account, conn, channel, dispatchOp, context) {
        let [targetHandle, targetHandleType] = channel.get_handle();

        if (targetHandleType == Tp.HandleType.CONTACT) {
            // Approve private text channels right away as we are going to handle it
            dispatchOp.claim_with_async(this._tpClient,
                                        Lang.bind(this, function(dispatchOp, result) {
                try {
                    dispatchOp.claim_with_finish(result);
                    this._handlingChannels(account, conn, [channel], false);
                } catch (err) {
                    throw new Error('Failed to Claim channel: ' + err);
                }}));

            context.accept();
        }
    },

    _delegatedChannelsCb: function(client, channels) {
        // Nothing to do as we don't make a distinction between observed and
        // handled channels.
    },

    _accountManagerPrepared: function(am, result) {
        am.prepare_finish(result);
    },
});

const ChatSource = new Lang.Class({
    Name: 'ChatSource',
    Extends: MessageTray.Source,

    _init: function(account, conn, channel, contact, client) {
        this._account = account;
        this._contact = contact;
        this._client = client;

        this.parent(contact.get_alias());

        this.isChat = true;
        this._pendingMessages = [];

        this._conn = conn;
        this._channel = channel;
        this._closedId = this._channel.connect('invalidated', Lang.bind(this, this._channelClosed));

        this._notification = new ChatNotification(this);
        this._notification.connect('clicked', Lang.bind(this, this.open));
        this._notification.setUrgency(MessageTray.Urgency.HIGH);
        this._notifyTimeoutId = 0;

        // We ack messages when the user expands the new notification or views the summary
        // notification, in which case the notification is also expanded.
        this._notification.connect('expanded', Lang.bind(this, this._ackMessages));

        this._presence = contact.get_presence_type();

        this._sentId = this._channel.connect('message-sent', Lang.bind(this, this._messageSent));
        this._receivedId = this._channel.connect('message-received', Lang.bind(this, this._messageReceived));
        this._pendingId = this._channel.connect('pending-message-removed', Lang.bind(this, this._pendingRemoved));

        this._notifyAliasId = this._contact.connect('notify::alias', Lang.bind(this, this._updateAlias));
        this._notifyAvatarId = this._contact.connect('notify::avatar-file', Lang.bind(this, this._updateAvatarIcon));
        this._presenceChangedId = this._contact.connect('presence-changed', Lang.bind(this, this._presenceChanged));

        // Add ourselves as a source.
        Main.messageTray.add(this);
        this.pushNotification(this._notification);

        this._getLogMessages();
    },

    buildRightClickMenu: function() {
        let item;

        let rightClickMenu = this.parent();
        item = new PopupMenu.PopupMenuItem('');
        item.actor.connect('notify::mapped', Lang.bind(this, function() {
            item.label.set_text(this.isMuted ? _("Unmute") : _("Mute"));
        }));
        item.connect('activate', Lang.bind(this, function() {
            this.setMuted(!this.isMuted);
            this.emit('done-displaying-content', false);
        }));
        rightClickMenu.add(item.actor);
        return rightClickMenu;
    },

    _createPolicy: function() {
        return new MessageTray.NotificationApplicationPolicy('empathy');
    },

    _updateAlias: function() {
        let oldAlias = this.title;
        let newAlias = this._contact.get_alias();

        if (oldAlias == newAlias)
            return;

        this.setTitle(newAlias);
        this._notification.appendAliasChange(oldAlias, newAlias);
    },

    getIcon: function() {
        let file = this._contact.get_avatar_file();
        if (file) {
            return new Gio.FileIcon({ file: file });
        } else {
            return new Gio.ThemedIcon({ name: 'avatar-default' });
        }
    },

    getSecondaryIcon: function() {
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

    _updateAvatarIcon: function() {
        this.iconUpdated();
        this._notification.update(this._notification.title, null, { customContent: true });
    },

    open: function() {
        if (this._client.is_handling_channel(this._channel)) {
            // We are handling the channel, try to pass it to Empathy
            this._client.delegate_channels_async([this._channel],
                                                 global.get_current_time(),
                                                 'org.freedesktop.Telepathy.Client.Empathy.Chat', null);
        } else {
            // We are not the handler, just ask to present the channel
            let dbus = Tp.DBusDaemon.dup();
            let cd = Tp.ChannelDispatcher.new(dbus);

            cd.present_channel_async(this._channel, global.get_current_time(), null);
        }
    },

    _getLogMessages: function() {
        let logManager = Tpl.LogManager.dup_singleton();
        let entity = Tpl.Entity.new_from_tp_contact(this._contact, Tpl.EntityType.CONTACT);

        logManager.get_filtered_events_async(this._account, entity,
                                             Tpl.EventTypeMask.TEXT, SCROLLBACK_HISTORY_LINES,
                                             null, Lang.bind(this, this._displayPendingMessages));
    },

    _displayPendingMessages: function(logManager, result) {
        let [success, events] = logManager.get_filtered_events_finish(result);

        let logMessages = events.map(makeMessageFromTplEvent);

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

    destroy: function(reason) {
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

        if (this._timestampTimeoutId)
            Mainloop.source_remove(this._timestampTimeoutId);

        this.parent(reason);
    },

    _channelClosed: function() {
        this.destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
    },

    /* All messages are new messages for Telepathy sources */
    get count() {
        return this._pendingMessages.length;
    },

    get indicatorCount() {
        return this.count;
    },

    get unseenCount() {
        return this.count;
    },

    get countVisible() {
        return this.count > 0;
    },

    _messageReceived: function(channel, message) {
        if (message.get_message_type() == Tp.ChannelTextMessageType.DELIVERY_REPORT)
            return;

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

    _notifyTimeout: function() {
        if (this._pendingMessages.length != 0)
            this.notify();

        this._notifyTimeoutId = 0;

        return GLib.SOURCE_REMOVE;
    },

    // This is called for both messages we send from
    // our client and other clients as well.
    _messageSent: function(channel, message, flags, token) {
        message = makeMessageFromTpMessage(message, NotificationDirection.SENT);
        this._notification.appendMessage(message);
    },

    notify: function() {
        this.parent(this._notification);
    },

    respond: function(text) {
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

    setChatState: function(state) {
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

    _presenceChanged: function (contact, presence, status, message) {
        let msg, title;

        title = GLib.markup_escape_text(this.title, -1);

        this._notification.update(this._notification.title, null, { customContent: true, secondaryGIcon: this.getSecondaryIcon() });

        if (message)
            msg += ' <i>(' + GLib.markup_escape_text(message, -1) + ')</i>';
    },

    _pendingRemoved: function(channel, message) {
        let idx = this._pendingMessages.indexOf(message);

        if (idx >= 0) {
            this._pendingMessages.splice(idx, 1);
            this.countUpdated();
        }
    },

    _ackMessages: function() {
        // Don't clear our messages here, tp-glib will send a
        // 'pending-message-removed' for each one.
        this._channel.ack_all_pending_messages_async(null);
    }
});

const ChatNotification = new Lang.Class({
    Name: 'ChatNotification',
    Extends: MessageTray.Notification,

    _init: function(source) {
        this.parent(source, source.title, null, { customContent: true, secondaryGIcon: source.getSecondaryIcon() });

        this._responseEntry = new St.Entry({ style_class: 'chat-response',
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

        this._createScrollArea();
        this._lastGroup = null;

        // Keep track of the bottom position for the current adjustment and
        // force a scroll to the bottom if things change while we were at the
        // bottom
        this._oldMaxScrollValue = this._scrollArea.vscroll.adjustment.value;
        this._scrollArea.add_style_class_name('chat-notification-scrollview');
        this._scrollArea.vscroll.adjustment.connect('changed', Lang.bind(this, function(adjustment) {
            if (adjustment.value == this._oldMaxScrollValue)
                this.scrollTo(St.Side.BOTTOM);
            this._oldMaxScrollValue = Math.max(adjustment.lower, adjustment.upper - adjustment.page_size);
        }));

        this._inputHistory = new History.HistoryManager({ entry: this._responseEntry.clutter_text });

        this._history = [];
        this._timestampTimeoutId = 0;
        this._composingTimeoutId = 0;
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
    appendMessage: function(message, noTimestamp) {
        let messageBody = GLib.markup_escape_text(message.text, -1);
        let styles = [message.direction];

        if (message.messageType == Tp.ChannelTextMessageType.ACTION) {
            let senderAlias = GLib.markup_escape_text(message.sender, -1);
            messageBody = '<i>%s</i> %s'.format(senderAlias, messageBody);
            styles.push('chat-action');
        }

        if (message.direction == NotificationDirection.RECEIVED) {
            this.update(this.source.title, messageBody, { customContent: true,
                                                          bannerMarkup: true });
        }

        let group = (message.direction == NotificationDirection.RECEIVED ?
                     'received' : 'sent');

        this._append({ body: messageBody,
                       group: group,
                       styles: styles,
                       timestamp: message.timestamp,
                       noTimestamp: noTimestamp });
    },

    _filterMessages: function() {
        if (this._history.length < 1)
            return;

        let lastMessageTime = this._history[0].time;
        let currentTime = (Date.now() / 1000);

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

        let groups = this._contentArea.get_children();
        for (let i = 0; i < groups.length; i++) {
            let group = groups[i];
            if (group.get_n_children() == 0)
                group.destroy();
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
     *  childProps: props to add the actor with.
     */
    _append: function(props) {
        let currentTime = (Date.now() / 1000);
        props = Params.parse(props, { body: null,
                                      group: null,
                                      styles: [],
                                      timestamp: currentTime,
                                      noTimestamp: false,
                                      childProps: null });

        // Reset the old message timeout
        if (this._timestampTimeoutId)
            Mainloop.source_remove(this._timestampTimeoutId);

        let highlighter = new MessageTray.URLHighlighter(props.body,
                                                         true,  // line wrap?
                                                         true); // allow markup?

        let body = highlighter.actor;

        let styles = props.styles;
        for (let i = 0; i < styles.length; i++)
            body.add_style_class_name(styles[i]);

        let group = props.group;
        if (group != this._lastGroup) {
            this._lastGroup = group;
            let emptyLine = new St.Label({ style_class: 'chat-empty-line' });
            this.addActor(emptyLine);
        }

        this._lastMessageBox = new St.BoxLayout({ vertical: false });
        this._lastMessageBox.add(body, props.childProps);
        this.addActor(this._lastMessageBox);

        let timestamp = props.timestamp;
        this._history.unshift({ actor: body, time: timestamp,
                                realMessage: group != 'meta' });

        if (!props.noTimestamp) {
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

    _formatTimestamp: function(date) {
        let now = new Date();

        var daysAgo = (now.getTime() - date.getTime()) / (24 * 60 * 60 * 1000);

        let format;

        let desktopSettings = new Gio.Settings({ schema: 'org.gnome.desktop.interface' });
        let clockFormat = desktopSettings.get_string(CLOCK_FORMAT_KEY);
        let hasAmPm = date.toLocaleFormat('%p') != '';

        if (clockFormat == '24h' || !hasAmPm) {
            // Show only the time if date is on today
            if(daysAgo < 1){
                /* Translators: Time in 24h format */
                format = _("%H\u2236%M");
            }
            // Show the word "Yesterday" and time if date is on yesterday
            else if(daysAgo <2){
                /* Translators: this is the word "Yesterday" followed by a
                 time string in 24h format. i.e. "Yesterday, 14:30" */
                // xgettext:no-c-format
                format = _("Yesterday, %H\u2236%M");
            }
            // Show a week day and time if date is in the last week
            else if (daysAgo < 7) {
                /* Translators: this is the week day name followed by a time
                 string in 24h format. i.e. "Monday, 14:30" */
                // xgettext:no-c-format
                format = _("%A, %H\u2236%M");

            } else if (date.getYear() == now.getYear()) {
                /* Translators: this is the month name and day number
                 followed by a time string in 24h format.
                 i.e. "May 25, 14:30" */
                // xgettext:no-c-format
                format = _("%B %d, %H\u2236%M");
            } else {
                /* Translators: this is the month name, day number, year
                 number followed by a time string in 24h format.
                 i.e. "May 25 2012, 14:30" */
                // xgettext:no-c-format
                format = _("%B %d %Y, %H\u2236%M");
            }
        } else {
            // Show only the time if date is on today
            if(daysAgo < 1){
                /* Translators: Time in 24h format */
                format = _("%l\u2236%M %p");
            }
            // Show the word "Yesterday" and time if date is on yesterday
            else if(daysAgo <2){
                /* Translators: this is the word "Yesterday" followed by a
                 time string in 12h format. i.e. "Yesterday, 2:30 pm" */
                // xgettext:no-c-format
                format = _("Yesterday, %l\u2236%M %p");
            }
            // Show a week day and time if date is in the last week
            else if (daysAgo < 7) {
                /* Translators: this is the week day name followed by a time
                 string in 12h format. i.e. "Monday, 2:30 pm" */
                // xgettext:no-c-format
                format = _("%A, %l\u2236%M %p");

            } else if (date.getYear() == now.getYear()) {
                /* Translators: this is the month name and day number
                 followed by a time string in 12h format.
                 i.e. "May 25, 2:30 pm" */
                // xgettext:no-c-format
                format = _("%B %d, %l\u2236%M %p");
            } else {
                /* Translators: this is the month name, day number, year
                 number followed by a time string in 12h format.
                 i.e. "May 25 2012, 2:30 pm"*/
                // xgettext:no-c-format
                format = _("%B %d %Y, %l\u2236%M %p");
            }
        }
        return date.toLocaleFormat(format);
    },

    appendTimestamp: function() {
        this._timestampTimeoutId = 0;

        let lastMessageTime = this._history[0].time;
        let lastMessageDate = new Date(lastMessageTime * 1000);

        let timeLabel = new St.Label({ text: this._formatTimestamp(lastMessageDate),
                                       style_class: 'chat-meta-message',
                                       x_expand: true,
                                       y_expand: true,
                                       x_align: Clutter.ActorAlign.END,
                                       y_align: Clutter.ActorAlign.END });

        this._lastMessageBox.add_actor(timeLabel);

        this._filterMessages();

        return GLib.SOURCE_REMOVE;
    },

    appendAliasChange: function(oldAlias, newAlias) {
        oldAlias = GLib.markup_escape_text(oldAlias, -1);
        newAlias = GLib.markup_escape_text(newAlias, -1);

        /* Translators: this is the other person changing their old IM name to their new
           IM name. */
        let message = '<i>' + _("%s is now known as %s").format(oldAlias, newAlias) + '</i>';

        let label = this._append({ body: message,
                                   group: 'meta',
                                   styles: ['chat-meta-message'] });

        this.update(newAlias, null, { customContent: true });

        this._filterMessages();
    },

    _onEntryActivated: function() {
        let text = this._responseEntry.get_text();
        if (text == '')
            return;

        this._inputHistory.addItem(text);

        // Telepathy sends out the Sent signal for us.
        // see Source._messageSent
        this._responseEntry.set_text('');
        this.source.respond(text);
    },

    _composingStopTimeout: function() {
        this._composingTimeoutId = 0;

        this.source.setChatState(Tp.ChannelChatState.PAUSED);

        return GLib.SOURCE_REMOVE;
    },

    _onEntryChanged: function() {
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
            this.source.setChatState(Tp.ChannelChatState.COMPOSING);

            this._composingTimeoutId = Mainloop.timeout_add_seconds(
                COMPOSING_STOP_TIMEOUT,
                Lang.bind(this, this._composingStopTimeout));
            GLib.Source.set_name_by_id(this._composingTimeoutId, '[gnome-shell] this._composingStopTimeout');
        } else {
            this.source.setChatState(Tp.ChannelChatState.ACTIVE);
        }
    }
});

const Component = TelepathyClient;
