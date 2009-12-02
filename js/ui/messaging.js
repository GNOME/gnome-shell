/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

const AVATAR_SIZE = 24;

const TELEPATHY = "org.freedesktop.Telepathy.";
const CONN = TELEPATHY + "Connection";
const CHANNEL = TELEPATHY + "Channel";
const CHANNELTEXT = CHANNEL + ".Type.Text";
const ACCOUNTMANAGER = TELEPATHY + 'AccountManager';

const ClientIface = {
    name: TELEPATHY + "Client",
    properties: [{ name: "Interfaces",
                   signature: "as",
                   access: "read" }]
};

const ClientObserverIface = {
    name: TELEPATHY + "Client.Observer",
    methods: [{ name: "ObserveChannels",
                inSignature: "ooa(oa{sv})oaoa{sv}",
                outSignature: "" }],
    properties: [{ name: "ObserverChannelFilter",
                   signature: "aa{sv}",
                   access: "read" }]
};

const ConnectionIface = {
    name: CONN,
    methods: [
        // This is deprecated, but the alternative requires building
        // another interface object...
        { name: "ListChannels",
          inSignature: "",
          outSignature: "a(osuu)"
        }
    ],
    signals: [
        { name: 'StatusChanged', inSignature: 'u' }
    ]
};

function Connection(path) {
    this._init(path);
};

Connection.prototype = {
    _init: function(path) {
        DBus.session.proxifyObject(this, nameify(path), path);
    }
};

DBus.proxifyPrototype(Connection.prototype, ConnectionIface);

const ConnectionAvatarsIface = {
    name: CONN + '.Interface.Avatars',
    methods: [
        { name: 'RequestAvatar',
          inSignature: 'u',
          outSignature: 'ays'
        }
    ]
};

function ConnectionAvatars(path) {
    this._init(path);
};

ConnectionAvatars.prototype = {
    _init: function(path) {
        DBus.session.proxifyObject(this, nameify(path), path);
    }
};

DBus.proxifyPrototype(ConnectionAvatars.prototype, ConnectionAvatarsIface);

const ChannelIface = {
    name: CHANNEL,
    properties: [
        { name: "TargetHandle",
          signature: "u",
          access: "read" },
        { name: "TargetID",
          signature: "s",
          access: "read" }
    ],
    signals: [
        { name: 'Closed', inSignature: '' }
    ]
};

function Channel(name, path) {
    this._init(name, path);
};

Channel.prototype = {
    _init: function(name, path) {
        DBus.session.proxifyObject(this, name, path);
    }
};

DBus.proxifyPrototype(Channel.prototype, ChannelIface);

const ChannelTextIface = {
    name: CHANNELTEXT,
    methods: [
        { name: 'ListPendingMessages',
          inSignature: 'b',
          outSignature: 'a(uuuuus)'
        }
    ],
    signals: [
        { name: 'Received', inSignature: 'uuuuus' }
    ]
};

function ChannelText(name, path) {
    this._init(name, path);
};

ChannelText.prototype = {
    _init: function(name, path) {
        DBus.session.proxifyObject(this, name, path);
    }
};

DBus.proxifyPrototype(ChannelText.prototype, ChannelTextIface);

const AccountManagerIface = {
    name: ACCOUNTMANAGER,

    properties: [{ name: "ValidAccounts",
                   signature: "ao",
                   access: "read" }]
};

function AccountManager() {
    this._init();
}

AccountManager.prototype = {
    _init: function() {
        DBus.session.proxifyObject(this,
                                   ACCOUNTMANAGER,
                                   pathify(ACCOUNTMANAGER));
    }
};

DBus.proxifyPrototype(AccountManager.prototype, AccountManagerIface);

const AccountIface = {
    name: 'org.freedesktop.Telepathy.Account',

    properties: [{ name: "Connection",
                   signature: "o",
                   access: "read" }]
};

function Account(name, path) {
    this._init(name, path);
}

Account.prototype = {
    _init: function(name, path) {
        DBus.session.proxifyObject(this, name, path);
    }
};

DBus.proxifyPrototype(Account.prototype, AccountIface);

let nameify = function(path) {
    return path.substr(1).replace('/', '.', 'g');
};

let pathify = function(name) {
    return '/' + name.replace('.', '/', 'g');
};

function Messaging() {
    this._init();
};

Messaging.prototype = {
    _init : function() {
        let name = TELEPATHY + "Client.GnomeShell";
        DBus.session.exportObject(pathify(name), this);
        DBus.session.acquire_name(name, DBus.SINGLE_INSTANCE,
            function(name){log("Acquired name " + name);},
            function(name){log("Lost name " + name);});

        this._conns = {};
        this._channels = {};

        // Acquire existing connections. (This wouldn't really be
        // needed if gnome-shell was only being started at the start
        // of a session, but it's very useful for making things
        // continue to work after restarting the shell.)
        let accountManager = new AccountManager();
        accountManager.GetRemote('ValidAccounts', Lang.bind(this, this._gotValidAccounts));
    },

    _gotValidAccounts: function(accounts, excp) {
        if (!accounts)
            return;

        for (let i = 0; i < accounts.length; i++) {
            let account = new Account(ACCOUNTMANAGER, accounts[i]);
            account.GetRemote('Connection', Lang.bind(this,
                function (conn_path, excp) {
                    if (!conn_path || conn_path == '/')
                        return;

                    let conn = new Connection(conn_path);
                    conn.ListChannelsRemote(Lang.bind(this,
                        function(channels, excp) {
                            if (!channels) {
                                log('no channels on ' + conn.getPath() + ': ' + excp);
                                return;
                            }
                            for (let i = 0; i < channels.length; i++) {
                                let [path, channel_type, handle_type, handle] = channels[i];
                                if (channel_type != CHANNELTEXT)
                                    continue;
                                if (this._channels[path])
                                    continue;

                                let connName = nameify(conn.getPath());
                                let channel = new Channel(connName, path);
                                channel.GetAllRemote(Lang.bind(this,
                                    function(props, excp) {
                                        this._addChannel(conn, path,
                                                         props['TargetHandle'],
                                                         props['TargetID']);
                                    }));
                            }
                        }));
                }));
        }
    },

    get Interfaces() {
        return [TELEPATHY + "Client.Observer"];
    },

    get ObserverChannelFilter() {
        return [
            { 'org.freedesktop.Telepathy.Channel.ChannelType': CHANNELTEXT }
        ];
    },

    ObserveChannels: function(account, conn_path, channels,
                              dispatch_operation, requests_satisfied,
                              observer_info) {
        let conn = new Connection(conn_path);
        let conn_name = nameify(conn_path);
        for (let i = 0; i < channels.length; i++) {
            let channelPath = channels[i][0];
            let props = channels[i][1];
            let targetHandle = props[CHANNEL + '.TargetHandle'];
            let targetId = props[CHANNEL + '.TargetID'];
            this._addChannel(conn, channelPath, targetHandle, targetId);
        }

        return [true];
    },

    _addChannel: function(conn, channelPath, targetHandle, targetId) {
        this._channels[channelPath] = new Source(conn, channelPath, targetHandle, targetId);
    }
};

DBus.conformExport(Messaging.prototype, ClientIface);
DBus.conformExport(Messaging.prototype, ClientObserverIface);

function Source(conn, channelPath, channel_props, targetId) {
    this._init(conn, channelPath, channel_props, targetId);
}

Source.prototype = {
    _init: function(conn, channelPath, targetHandle, targetId) {
        let connName = nameify(conn.getPath());
        this._channel = new Channel(connName, channelPath);
        this._closedId = this._channel.connect('Closed', Lang.bind(this, this._channelClosed));

        this._targetId = targetId;
        log('channel for ' + this._targetId + ' channelPath ' + channelPath);

        this._pendingMessages = null;

        this._avatar = null;
        this._avatarBytes = null;

        // FIXME: RequestAvatar is deprecated in favor of
        // RequestAvatars; but RequestAvatars provides no explicit
        // indication of "no avatar available", so there's no way we
        // can reliably wait for it to finish before displaying a
        // message. So we use RequestAvatar() instead.
        let connAv = new ConnectionAvatars(conn.getPath());
        connAv.RequestAvatarRemote(targetHandle, Lang.bind(this, this._gotAvatar));

        this._channelText = new ChannelText(connName, channelPath);
        this._receivedId = this._channelText.connect('Received', Lang.bind(this, this._receivedMessage));

        this._channelText.ListPendingMessagesRemote(false,
            Lang.bind(this, function(msgs, excp) {
                if (msgs) {
                    log('got pending messages for ' + this._targetId);
                    this._pendingMessages = msgs;
                    this._processPendingMessages();
                }
            }));
    },

    _gotAvatar: function(result, excp) {
        if (result) {
            this._avatarBytes = result[0];
            this._avatar = Shell.TextureCache.get_default().load_from_data(this._avatarBytes, this._avatarBytes.length, AVATAR_SIZE);
            log('got avatar for ' + this._targetId);
        } else {
            // fallback avatar (FIXME)
            this._avatar = Shell.TextureCache.get_default().load_icon_name("stock_person", AVATAR_SIZE);
            log('using default avatar for ' + this._targetId);
        }

        this._processPendingMessages();
    },

    _processPendingMessages: function() {
        if (!this._avatar || !this._pendingMessages)
            return;

        for (let i = 0; i < this._pendingMessages.length; i++)
            this._receivedMessage.apply(this, [this._channel].concat(this._pendingMessages[i]));
        this._pendingMessages = null;
    },

    _channelClosed: function() {
        log('Channel closed ' + this._targetId);
        this._channel.disconnect(this._closedId);
        this._channelText.disconnect(this._receivedId);
        Main.messageTray.remove(this._targetId);
    },

    _receivedMessage: function(channel, id, timestamp, sender,
                               type, flags, text) {
        log('Received: id ' + id + ', time ' + timestamp + ', sender ' + sender + ', type ' + type + ', flags ' + flags + ': ' + text);
        Main.notificationPopup.show(this._avatar, text);
        if (!Main.messageTray.contains(this._targetId)) {
            let avatarForMessageTray = null;
            if (this._avatarBytes)
                avatarForMessageTray = Shell.TextureCache.get_default().load_from_data(this._avatarBytes, this._avatarBytes.length, AVATAR_SIZE);
            else
                avatarForMessageTray = Shell.TextureCache.get_default().load_icon_name("stock_person", AVATAR_SIZE);
            Main.messageTray.add(this._targetId, avatarForMessageTray);
        }
    }
};
