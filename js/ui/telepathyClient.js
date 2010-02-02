/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Telepathy = imports.misc.telepathy;

let avatarManager;

// This is GNOME Shell's implementation of the Telepathy "Client"
// interface. Specifically, the shell is a Telepathy "Approver", which
// lets us control the routing of incoming messages, a "Handler",
// which lets us receive and respond to messages, and an "Observer",
// which lets us see messages even if they belong to another app (eg,
// a conversation started from within Empathy).

function Client() {
    this._init();
};

Client.prototype = {
    _init : function() {
        let name = Telepathy.CLIENT_NAME + '.GnomeShell';
        DBus.session.exportObject(Telepathy.nameToPath(name), this);
        DBus.session.acquire_name(name, DBus.SINGLE_INSTANCE,
                                  function (name) { /* FIXME: acquired */ },
                                  function (name) { /* FIXME: lost */ });

        this._channels = {};

        avatarManager = new AvatarManager();

        // Acquire existing connections. (Needed to make things work
        // through a restart.)
        let accountManager = new Telepathy.AccountManager(DBus.session,
                                                          Telepathy.ACCOUNT_MANAGER_NAME,
                                                          Telepathy.nameToPath(Telepathy.ACCOUNT_MANAGER_NAME));
        accountManager.GetRemote('ValidAccounts', Lang.bind(this, this._gotValidAccounts));
    },

    _gotValidAccounts: function(accounts, err) {
        if (!accounts)
            return;

        for (let i = 0; i < accounts.length; i++) {
            let account = new Telepathy.Account(DBus.session,
                                                Telepathy.ACCOUNT_MANAGER_NAME,
                                                accounts[i]);
            account.GetRemote('Connection', Lang.bind(this,
                function (connPath, err) {
                    if (!connPath || connPath == '/')
                        return;

                    let connReq = new Telepathy.ConnectionRequests(DBus.session,
                                                                   Telepathy.pathToName(connPath),
                                                                   connPath);
                    connReq.GetRemote('Channels', Lang.bind(this,
                        function(channels, err) {
                            if (!channels)
                                return;
                            this._addChannels(connPath, channels);
                        }));
                }));
        }
    },

    get Interfaces() {
        return [ Telepathy.CLIENT_APPROVER_NAME,
                 Telepathy.CLIENT_HANDLER_NAME,
                 Telepathy.CLIENT_OBSERVER_NAME ];
    },

    get ApproverChannelFilter() {
        return [
            { 'org.freedesktop.Telepathy.Channel.ChannelType': Telepathy.CHANNEL_TEXT_NAME }
        ];
    },

    AddDispatchOperation: function(channels, dispatchOperationPath, properties) {
        let sender = DBus.getCurrentMessageContext().sender;
        let op = new Telepathy.ChannelDispatchOperation(DBus.session, sender,
                                                        dispatchOperationPath);
        op.ClaimRemote();
    },

    get HandlerChannelFilter() {
        return [
            { 'org.freedesktop.Telepathy.Channel.ChannelType': Telepathy.CHANNEL_TEXT_NAME }
        ];
    },

    HandleChannels: function(account, connPath, channels,
                             requestsSatisfied, userActionTime,
                             handlerInfo) {
        this._addChannels(connPath, channels);
    },

    get ObserverChannelFilter() {
        return [
            { 'org.freedesktop.Telepathy.Channel.ChannelType': Telepathy.CHANNEL_TEXT_NAME }
        ];
    },

    ObserveChannels: function(account, connPath, channels,
                              dispatchOperation, requestsSatisfied,
                              observerInfo) {
        this._addChannels(connPath, channels);
    },

    _addChannels: function(connPath, channelDetailsList) {
        for (let i = 0; i < channelDetailsList.length; i++) {
            let [channelPath, props] = channelDetailsList[i];
            if (this._channels[channelPath])
                continue;

            let channelType = props[Telepathy.CHANNEL_NAME + '.ChannelType'];
            if (channelType != Telepathy.CHANNEL_TEXT_NAME)
                continue;

            let targetHandle = props[Telepathy.CHANNEL_NAME + '.TargetHandle'];
            let targetHandleType = props[Telepathy.CHANNEL_NAME + '.TargetHandleType'];
            let targetId = props[Telepathy.CHANNEL_NAME + '.TargetID'];

            let source = new Source(connPath, channelPath,
                                    targetHandle, targetHandleType, targetId);
            this._channels[channelPath] = source;
            source.connect('destroy', Lang.bind(this,
                function() {
                    delete this._channels[channelPath];
                }));
        }
    }
};
DBus.conformExport(Client.prototype, Telepathy.ClientIface);
DBus.conformExport(Client.prototype, Telepathy.ClientApproverIface);
DBus.conformExport(Client.prototype, Telepathy.ClientHandlerIface);
DBus.conformExport(Client.prototype, Telepathy.ClientObserverIface);


function AvatarManager() {
    this._init();
};

AvatarManager.prototype = {
    _init: function() {
        this._connections = {};
    },

    _addConnection: function(conn) {
        if (this._connections[conn.getPath()])
            return this._connections[conn.getPath()];

        let info = {};

        // avatarData[handle] describes the icon for @handle:
        // either the string 'default', meaning to use the default
        // avatar, or an array of bytes containing, eg, PNG data.
        info.avatarData = {};

        // icons[handle] is an array of the icon actors currently
        // being displayed for @handle. These will be updated
        // automatically if @handle's avatar changes.
        info.icons = {};

        info.connectionAvatars = new Telepathy.ConnectionAvatars(DBus.session,
                                                                 conn.getBusName(),
                                                                 conn.getPath());
        info.updatedId = info.connectionAvatars.connect(
            'AvatarUpdated', Lang.bind(this, this._avatarUpdated));
        info.retrievedId = info.connectionAvatars.connect(
            'AvatarRetrieved', Lang.bind(this, this._avatarRetrieved));

        info.statusChangedId = conn.connect('StatusChanged', Lang.bind(this,
            function (status, reason) {
                if (status == Telepathy.ConnectionStatus.DISCONNECTED)
                    this._removeConnection(conn);
            }));

        this._connections[conn.getPath()] = info;
        return info;
    },

    _removeConnection: function(conn) {
        let info = this._connections[conn.getPath()];
        if (!info)
            return;

        conn.disconnect(info.statusChangedId);
        info.connectionAvatars.disconnect(info.updatedId);
        info.connectionAvatars.disconnect(info.retrievedId);

        delete this._connections[conn.getPath()];
    },

    _avatarUpdated: function(conn, handle, token) {
        let info = this._connections[conn.getPath()];
        if (!info)
            return;

        if (!info.avatarData[handle]) {
            // This would only happen if either (a) the initial
            // RequestAvatars() call hasn't returned yet, or (b)
            // Telepathy is informing us about avatars we didn't ask
            // about. Either way, we don't have to do anything here.
            return;
        }

        if (token == '') {
            // Invoke the next async callback in the chain, telling
            // it to use the default image.
            this._avatarRetrieved(conn, handle, token, 'default', null);
        } else {
            // In this case, @token is some sort of UUID. Telepathy
            // expects us to cache avatar images to disk and use the
            // tokens to figure out when we already have the right
            // images cached. But we don't do that, we just
            // ignore @token and request the image unconditionally.
            info.connectionAvatars.RequestAvatarsRemote([handle]);
        }
    },

    _createIcon: function(iconData, size) {
        let textureCache = St.TextureCache.get_default();
        if (iconData == 'default')
            return textureCache.load_icon_name('stock_person', size);
        else
            return textureCache.load_from_data(iconData, iconData.length, size);
    },

    _avatarRetrieved: function(conn, handle, token, avatarData, mimeType) {
        let info = this._connections[conn.getPath()];
        if (!info)
            return;

        info.avatarData[handle] = avatarData;
        if (!info.icons[handle])
            return;

        for (let i = 0; i < info.icons[handle].length; i++) {
            let iconBox = info.icons[handle][i];
            let size = iconBox.child.height;
            iconBox.child = this._createIcon(avatarData, size);
        }
    },

    createAvatar: function(conn, handle, size) {
        let iconBox = new St.Bin({ style_class: 'avatar-box' });

        let info = this._connections[conn.getPath()];
        if (!info)
            info = this._addConnection(conn);

        if (!info.icons[handle])
            info.icons[handle] = [];
        info.icons[handle].push(iconBox);

        iconBox.connect('destroy', Lang.bind(this,
            function() {
                let i = info.icons[handle].indexOf(iconBox);
                if (i != -1)
                    info.icons[handle].splice(i, 1);
            }));

        let avatarData = info.avatarData[handle];
        if (avatarData) {
            iconBox.child = this._createIcon(avatarData, size);
            return iconBox;
        }

        // Fill in the default icon and then asynchronously load
        // the real avatar.
        iconBox.child = this._createIcon('default', size);
        info.connectionAvatars.GetKnownAvatarTokensRemote([handle], Lang.bind(this,
            function (tokens, err) {
                if (tokens && tokens[handle])
                    info.connectionAvatars.RequestAvatarsRemote([handle]);
                else
                    info.avatarData[handle] = 'default';
            }));

        return iconBox;
    }
};


function Source(connPath, channelPath, targetHandle, targetHandleType, targetId) {
    this._init(connPath, channelPath, targetHandle, targetHandleType, targetId);
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init: function(connPath, channelPath, targetHandle, targetHandleType, targetId) {
        MessageTray.Source.prototype._init.call(this, targetId);

        let connName = Telepathy.pathToName(connPath);
        this._conn = new Telepathy.Connection(DBus.session, connName, connPath);
        this._channel = new Telepathy.Channel(DBus.session, connName, channelPath);
        this._closedId = this._channel.connect('Closed', Lang.bind(this, this._channelClosed));

        this._targetHandle = targetHandle;
        this._targetId = targetId;

        this.name = this._targetId;
        if (targetHandleType == Telepathy.HandleType.CONTACT) {
            let aliasing = new Telepathy.ConnectionAliasing(DBus.session, connName, connPath);
            aliasing.RequestAliasesRemote([this._targetHandle], Lang.bind(this,
                function (aliases, err) {
                    if (aliases && aliases.length)
                        this.name = aliases[0];
                }));
        }

        this._channelText = new Telepathy.ChannelText(DBus.session, connName, channelPath);
        this._receivedId = this._channelText.connect('Received', Lang.bind(this, this._receivedMessage));

        this._channelText.ListPendingMessagesRemote(false, Lang.bind(this, this._gotPendingMessages));
    },

    createIcon: function(size) {
        return avatarManager.createAvatar(this._conn, this._targetHandle, size);
    },

    _gotPendingMessages: function(msgs, err) {
        if (!msgs)
            return;

        for (let i = 0; i < msgs.length; i++)
            this._receivedMessage.apply(this, [this._channel].concat(msgs[i]));
    },

    _channelClosed: function() {
        this._channel.disconnect(this._closedId);
        this._channelText.disconnect(this._receivedId);
        this.destroy();
    },

    _receivedMessage: function(channel, id, timestamp, sender,
                               type, flags, text) {
        if (!Main.messageTray.contains(this))
            Main.messageTray.add(this);

        let notification = new Notification(this._targetId, this, text);
        this.notify(notification);

        this._channelText.AcknowledgePendingMessagesRemote([id]);
    }
};

function Notification(id, source, text) {
    this._init(id, source, text);
}

Notification.prototype = {
    __proto__:  MessageTray.Notification.prototype,

    _init: function(id, source, text) {
        MessageTray.Notification.prototype._init.call(this, id, source, source.name, text, true);
    }
};
