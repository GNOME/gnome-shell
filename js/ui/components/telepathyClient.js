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

        // Watch subscription requests and connection errors
        this._subscriptionSource = null;
        this._accountSource = null;

        // Workaround for gjs not supporting GPtrArray in signals.
        // See BGO bug #653941 for context.
        this._tpClient.set_contact_list_changed_func(
            Lang.bind(this, this._contactListChanged));

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

        this._accountManagerValidityChangedId = this._accountManager.connect('account-validity-changed',
                                                                             Lang.bind(this, this._accountValidityChanged));

        if (!this._accountManager.is_prepared(Tp.AccountManager.get_feature_quark_core()))
            this._accountManager.prepare_async(null, Lang.bind(this, this._accountManagerPrepared));
    },

    disable: function() {
        this._tpClient.unregister();
        this._accountManager.disconnect(this._accountManagerValidityChangedId);
        this._accountManagerValidityChangedId = 0;
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

    _displayRoomInvitation: function(conn, channel, dispatchOp, context) {
        // We can only approve the rooms if we have been invited to it
        let selfContact = channel.group_get_self_contact();
        if (selfContact == null) {
            context.fail(new Tp.Error({ code: Tp.Error.INVALID_ARGUMENT,
                                        message: 'Not invited to the room' }));
            return;
        }

        let [invited, inviter, reason, msg] = channel.group_get_local_pending_contact_info(selfContact);
        if (!invited) {
            context.fail(new Tp.Error({ code: Tp.Error.INVALID_ARGUMENT,
                                        message: 'Not invited to the room' }));
            return;
        }

        // FIXME: We don't have a 'chat room' icon (bgo #653737) use
        // system-users for now as Empathy does.
        let source = new ApproverSource(dispatchOp, _("Invitation"),
                                        Gio.icon_new_for_string('system-users'));
        Main.messageTray.add(source);

        let notif = new RoomInviteNotification(source, dispatchOp, channel, inviter);
        source.notify(notif);
        context.accept();
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
        else if (chanType == Tp.IFACE_CHANNEL_TYPE_CALL)
            this._approveCall(account, conn, channel, dispatchOp, context);
        else if (chanType == Tp.IFACE_CHANNEL_TYPE_FILE_TRANSFER)
            this._approveFileTransfer(account, conn, channel, dispatchOp, context);
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
        } else {
            this._displayRoomInvitation(conn, channel, dispatchOp, context);
        }
    },

    _approveCall: function(account, conn, channel, dispatchOp, context) {
        let isVideo = false;

        let props = channel.borrow_immutable_properties();

        if (props[Tp.PROP_CHANNEL_TYPE_CALL_INITIAL_VIDEO])
          isVideo = true;

        // We got the TpContact
        let source = new ApproverSource(dispatchOp, _("Call"), isVideo ?
                                        Gio.icon_new_for_string('camera-web') :
                                        Gio.icon_new_for_string('audio-input-microphone'));
        Main.messageTray.add(source);

        let notif = new AudioVideoNotification(source, dispatchOp, channel,
            channel.get_target_contact(), isVideo);
        source.notify(notif);
        context.accept();
    },

    _approveFileTransfer: function(account, conn, channel, dispatchOp, context) {
        // Use the icon of the file being transferred
        let gicon = Gio.content_type_get_icon(channel.get_mime_type());

        // We got the TpContact
        let source = new ApproverSource(dispatchOp, _("File Transfer"), gicon);
        Main.messageTray.add(source);

        let notif = new FileTransferNotification(source, dispatchOp, channel,
            channel.get_target_contact());
        source.notify(notif);
        context.accept();
    },

    _delegatedChannelsCb: function(client, channels) {
        // Nothing to do as we don't make a distinction between observed and
        // handled channels.
    },

    _accountManagerPrepared: function(am, result) {
        am.prepare_finish(result);

        let accounts = am.get_valid_accounts();
        for (let i = 0; i < accounts.length; i++) {
            this._accountValidityChanged(am, accounts[i], true);
        }
    },

    _accountValidityChanged: function(am, account, valid) {
        if (!valid)
            return;

        // It would be better to connect to "status-changed" but we cannot.
        // See discussion in https://bugzilla.gnome.org/show_bug.cgi?id=654159
        account.connect("notify::connection-status",
                        Lang.bind(this, this._accountConnectionStatusNotifyCb));

        account.connect('notify::connection',
                        Lang.bind(this, this._connectionChanged));
        this._connectionChanged(account);
    },

    _connectionChanged: function(account) {
        let conn = account.get_connection();
        if (conn == null)
            return;

        this._tpClient.grab_contact_list_changed(conn);
        if (conn.get_contact_list_state() == Tp.ContactListState.SUCCESS) {
            this._contactListChanged(conn, conn.dup_contact_list(), []);
        }
    },

    _contactListChanged: function(conn, added, removed) {
        for (let i = 0; i < added.length; i++) {
            let contact = added[i];

            contact.connect('subscription-states-changed',
                            Lang.bind(this, this._subscriptionStateChanged));
            this._subscriptionStateChanged(contact);
        }
    },

    _subscriptionStateChanged: function(contact) {
        if (contact.get_publish_state() != Tp.SubscriptionState.ASK)
            return;

        /* Implicitly accept publish requests if contact is already subscribed */
        if (contact.get_subscribe_state() == Tp.SubscriptionState.YES ||
            contact.get_subscribe_state() == Tp.SubscriptionState.ASK) {

            contact.authorize_publication_async(function(src, result) {
                src.authorize_publication_finish(result)});

            return;
        }

        /* Display notification to ask user to accept/reject request */
        let source = this._ensureAppSource();

        let notif = new SubscriptionRequestNotification(source, contact);
        source.notify(notif);
    },

    _accountConnectionStatusNotifyCb: function(account) {
        let connectionError = account.connection_error;

        if (account.connection_status != Tp.ConnectionStatus.DISCONNECTED ||
            connectionError == Tp.error_get_dbus_name(Tp.Error.CANCELLED)) {
            return;
        }

        let notif = this._accountNotifications[account.get_object_path()];
        if (notif)
            return;

        /* Display notification that account failed to connect */
        let source = this._ensureAppSource();

        notif = new AccountNotification(source, account, connectionError);
        this._accountNotifications[account.get_object_path()] = notif;
        notif.connect('destroy', Lang.bind(this, function() {
            delete this._accountNotifications[account.get_object_path()];
        }));
        source.notify(notif);
    },

    _ensureAppSource: function() {
        if (this._appSource == null) {
            this._appSource = new MessageTray.Source(_("Chat"), 'empathy');
            this._appSource.policy = new MessageTray.NotificationApplicationPolicy('empathy');

            Main.messageTray.add(this._appSource);
            this._appSource.connect('destroy', Lang.bind(this, function () {
                this._appSource = null;
            }));
        }

        return this._appSource;
    }
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
        this.setResident(true);

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

        this.updated();

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

        switch (clockFormat) {
            case '24h':
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
                break;
        default:
            /* explicit fall-through */
            case '12h':
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
                break;
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

const ApproverSource = new Lang.Class({
    Name: 'ApproverSource',
    Extends: MessageTray.Source,

    _init: function(dispatchOp, text, gicon) {
        this._gicon = gicon;

        this.parent(text);

        this._dispatchOp = dispatchOp;

        // Destroy the source if the channel dispatch operation is invalidated
        // as we can't approve any more.
        this._invalidId = dispatchOp.connect('invalidated',
                                             Lang.bind(this, function(domain, code, msg) {
            this.destroy();
        }));
    },

    _createPolicy: function() {
        return new MessageTray.NotificationApplicationPolicy('empathy');
    },

    destroy: function() {
        if (this._invalidId != 0) {
            this._dispatchOp.disconnect(this._invalidId);
            this._invalidId = 0;
        }

        this.parent();
    },

    getIcon: function() {
        return this._gicon;
    }
});

const RoomInviteNotification = new Lang.Class({
    Name: 'RoomInviteNotification',
    Extends: MessageTray.Notification,

    _init: function(source, dispatchOp, channel, inviter) {
        this.parent(source,
                    /* translators: argument is a room name like
                     * room@jabber.org for example. */
                    _("Invitation to %s").format(channel.get_identifier()),
                    null,
                    { customContent: true });
        this.setResident(true);

        /* translators: first argument is the name of a contact and the second
         * one the name of a room. "Alice is inviting you to join room@jabber.org
         * for example. */
        this.addBody(_("%s is inviting you to join %s").format(inviter.get_alias(), channel.get_identifier()));

        this.addAction(_("Decline"), Lang.bind(this, function() {
            dispatchOp.leave_channels_async(Tp.ChannelGroupChangeReason.NONE, '', function(src, result) {
                src.leave_channels_finish(result);
            });
            this.destroy();
        }));
        this.addAction(_("Accept"), Lang.bind(this, function() {
            dispatchOp.handle_with_time_async('', global.get_current_time(), function(src, result) {
                src.handle_with_time_finish(result);
            });
            this.destroy();
        }));
    }
});

// Audio Video
const AudioVideoNotification = new Lang.Class({
    Name: 'AudioVideoNotification',
    Extends: MessageTray.Notification,

    _init: function(source, dispatchOp, channel, contact, isVideo) {
        let title = '';

        if (isVideo)
             /* translators: argument is a contact name like Alice for example. */
            title = _("Video call from %s").format(contact.get_alias());
        else
             /* translators: argument is a contact name like Alice for example. */
            title = _("Call from %s").format(contact.get_alias());

        this.parent(source, title, null, { customContent: true });
        this.setResident(true);

        this.setUrgency(MessageTray.Urgency.CRITICAL);

        this.addAction(_("Decline"), Lang.bind(this, function() {
            dispatchOp.leave_channels_async(Tp.ChannelGroupChangeReason.NONE, '', function(src, result) {
                src.leave_channels_finish(result);
            });
            this.destroy();
        }));
        /* translators: this is a button label (verb), not a noun */
        this.addAction(_("Answer"), Lang.bind(this, function() {
            dispatchOp.handle_with_time_async('', global.get_current_time(), function(src, result) {
                src.handle_with_time_finish(result);
            });
            this.destroy();
        }));
    }
});

// File Transfer
const FileTransferNotification = new Lang.Class({
    Name: 'FileTransferNotification',
    Extends: MessageTray.Notification,

    _init: function(source, dispatchOp, channel, contact) {
        this.parent(source,
                    /* To translators: The first parameter is
                     * the contact's alias and the second one is the
                     * file name. The string will be something
                     * like: "Alice is sending you test.ogg"
                     */
                    _("%s is sending you %s").format(contact.get_alias(),
                                                     channel.get_filename()),
                    null,
                    { customContent: true });
        this.setResident(true);

        this.addAction(_("Decline"), Lang.bind(this, function() {
            dispatchOp.leave_channels_async(Tp.ChannelGroupChangeReason.NONE, '', function(src, result) {
                src.leave_channels_finish(result);
            });
            this.destroy();
        }));
        this.addAction(_("Accept"), Lang.bind(this, function() {
            dispatchOp.handle_with_time_async('', global.get_current_time(), function(src, result) {
                src.handle_with_time_finish(result);
            });
            this.destroy();
        }));
    }
});

// Subscription request
const SubscriptionRequestNotification = new Lang.Class({
    Name: 'SubscriptionRequestNotification',
    Extends: MessageTray.Notification,

    _init: function(source, contact) {
        this.parent(source,
                    /* To translators: The parameter is the contact's alias */
                    _("%s would like permission to see when you are online").format(contact.get_alias()),
                    null, { customContent: true });

        this._contact = contact;
        this._connection = contact.get_connection();

        let layout = new St.BoxLayout({ vertical: false });

        // Display avatar
        let iconBox = new St.Bin({ style_class: 'avatar-box' });
        iconBox._size = 48;

        let textureCache = St.TextureCache.get_default();
        let file = contact.get_avatar_file();

        if (file) {
            let uri = file.get_uri();
            let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
            iconBox.child = textureCache.load_uri_async(uri, iconBox._size, iconBox._size, scaleFactor);
        }
        else {
            iconBox.child = new St.Icon({ icon_name: 'avatar-default',
                                          icon_size: iconBox._size });
        }

        layout.add(iconBox);

        // subscription request message
        let label = new St.Label({ style_class: 'subscription-message',
                                   text: contact.get_publish_request() });

        layout.add(label);

        this.addActor(layout);

        this.addAction(_("Decline"), Lang.bind(this, function() {
            contact.remove_async(function(src, result) {
                src.remove_finish(result);
            });
        }));
        this.addAction(_("Accept"), Lang.bind(this, function() {
            // Authorize the contact and request to see his status as well
            contact.authorize_publication_async(function(src, result) {
                src.authorize_publication_finish(result);
            });

            contact.request_subscription_async('', function(src, result) {
                src.request_subscription_finish(result);
            });
        }));

        this._changedId = contact.connect('subscription-states-changed',
            Lang.bind(this, this._subscriptionStatesChangedCb));
        this._invalidatedId = this._connection.connect('invalidated',
            Lang.bind(this, this.destroy));
    },

    destroy: function() {
        if (this._changedId != 0) {
            this._contact.disconnect(this._changedId);
            this._changedId = 0;
        }

        if (this._invalidatedId != 0) {
            this._connection.disconnect(this._invalidatedId);
            this._invalidatedId = 0;
        }

        this.parent();
    },

    _subscriptionStatesChangedCb: function(contact, subscribe, publish, msg) {
        // Destroy the notification if the subscription request has been
        // answered
        if (publish != Tp.SubscriptionState.ASK)
            this.destroy();
    }
});

// Messages from empathy/libempathy/empathy-utils.c
// create_errors_to_message_hash()

/* Translator note: these should be the same messages that are
 * used in Empathy, so just copy and paste from there. */
let _connectionErrorMessages = {};
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.NETWORK_ERROR)]
  = _("Network error");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.AUTHENTICATION_FAILED)]
  = _("Authentication failed");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.ENCRYPTION_ERROR)]
  = _("Encryption error");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_NOT_PROVIDED)]
  = _("Certificate not provided");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_UNTRUSTED)]
  = _("Certificate untrusted");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_EXPIRED)]
  = _("Certificate expired");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_NOT_ACTIVATED)]
  = _("Certificate not activated");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_HOSTNAME_MISMATCH)]
  = _("Certificate hostname mismatch");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_FINGERPRINT_MISMATCH)]
  = _("Certificate fingerprint mismatch");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_SELF_SIGNED)]
  = _("Certificate self-signed");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CANCELLED)]
  = _("Status is set to offline");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.ENCRYPTION_NOT_AVAILABLE)]
  = _("Encryption is not available");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_INVALID)]
  = _("Certificate is invalid");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CONNECTION_REFUSED)]
  = _("Connection has been refused");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CONNECTION_FAILED)]
  = _("Connection can't be established");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CONNECTION_LOST)]
  = _("Connection has been lost");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.ALREADY_CONNECTED)]
  = _("This account is already connected to the server");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CONNECTION_REPLACED)]
  = _("Connection has been replaced by a new connection using the same resource");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.REGISTRATION_EXISTS)]
  = _("The account already exists on the server");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.SERVICE_BUSY)]
  = _("Server is currently too busy to handle the connection");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_REVOKED)]
  = _("Certificate has been revoked");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_INSECURE)]
  = _("Certificate uses an insecure cipher algorithm or is cryptographically weak");
_connectionErrorMessages[Tp.error_get_dbus_name(Tp.Error.CERT_LIMIT_EXCEEDED)]
  = _("The length of the server certificate, or the depth of the server certificate chain, exceed the limits imposed by the cryptography library");
_connectionErrorMessages['org.freedesktop.DBus.Error.NoReply']
  = _("Internal error");

const AccountNotification = new Lang.Class({
    Name: 'AccountNotification',
    Extends: MessageTray.Notification,

    _init: function(source, account, connectionError) {
        this.parent(source,
                    /* translators: argument is the account name, like
                     * name@jabber.org for example. */
                    _("Unable to connect to %s").format(account.get_display_name()),
                    this._getMessage(connectionError));

        this._account = account;

        this.addAction(_("View account"), Lang.bind(this, function() {
            let cmd = 'empathy-accounts --select-account=' +
                account.get_path_suffix();
            let app_info = Gio.app_info_create_from_commandline(cmd, null, 0);
            app_info.launch([], global.create_app_launch_context(0, -1));
        }));

        this._enabledId = account.connect('notify::enabled',
                                          Lang.bind(this, function() {
                                              if (!account.is_enabled())
                                                  this.destroy();
                                          }));

        this._invalidatedId = account.connect('invalidated',
                                              Lang.bind(this, this.destroy));

        this._connectionStatusId = account.connect('notify::connection-status',
            Lang.bind(this, function() {
                let status = account.connection_status;
                if (status == Tp.ConnectionStatus.CONNECTED) {
                    this.destroy();
                } else if (status == Tp.ConnectionStatus.DISCONNECTED) {
                    let connectionError = account.connection_error;

                    if (connectionError == Tp.error_get_dbus_name(Tp.Error.CANCELLED))
                        this.destroy();
                    else
                        this.update(this.title, this._getMessage(connectionError));
                }
            }));
    },

    _getMessage: function(connectionError) {
        let message;
        if (connectionError in _connectionErrorMessages) {
            message = _connectionErrorMessages[connectionError];
        } else {
            message = _("Unknown reason");
        }
        return message;
    },

    destroy: function() {
        if (this._enabledId != 0) {
            this._account.disconnect(this._enabledId);
            this._enabledId = 0;
        }

        if (this._invalidatedId != 0) {
            this._account.disconnect(this._invalidatedId);
            this._invalidatedId = 0;
        }

        if (this._connectionStatusId != 0) {
            this._account.disconnect(this._connectionStatusId);
            this._connectionStatusId = 0;
        }

        this.parent();
    }
});
const Component = TelepathyClient;
