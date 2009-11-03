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
    signals: [
        { name: 'StatusChanged', inSignature: 'u' }
    ]
};

const ConnectionAvatarsIface = {
    name: CONN + '.Interface.Avatars',
    methods: [
        { name: 'RequestAvatar',
          inSignature: 'u',
          outSignature: 'ays'
        }
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
DBus.proxifyPrototype(Connection.prototype, ConnectionAvatarsIface);

const ChannelIface = {
    name: CHANNEL,
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
        let conn = this._conns[conn_path];
        if (!conn) {
            conn = new Connection(conn_path);
            conn.connect('StatusChanged', Lang.bind(this, this._connectionStatusChanged));
        }

        let conn_name = nameify(conn_path);
        for (let i = 0; i < channels.length; i++) {
            new Source(conn, conn_name, channels[i][0], channels[i][1]);
        }

        return [true];
    },

    _connectionStatusChanged: function(connection, status) {
        if (status == Connection_Status.Disconnected) {
            delete this._conns[connection.getPath()];
        }
    }
};

DBus.conformExport(Messaging.prototype, ClientIface);
DBus.conformExport(Messaging.prototype, ClientObserverIface);

function Source(conn, conn_name, channel_path, channel_props) {
    this._init(conn, conn_name, channel_path, channel_props);
}

Source.prototype = {
    _init: function(conn, conn_name, channel_path, channel_props) {
        this._targetId = channel_props[CHANNEL + '.TargetID'];

        log('channel for ' + this._targetId);

	this._pendingMessages = null;

	// FIXME: RequestAvatar is deprecated in favor of
	// RequestAvatars; but RequestAvatars provides no explicit
	// indication of "no avatar available", so there's no way we
	// can reliably wait for it to finish before displaying a
	// message. So we use RequestAvatar() instead.
        let targethandle = channel_props[CHANNEL + '.TargetHandle'];
        conn.RequestAvatarRemote(targethandle, Lang.bind(this, this._gotAvatar));

        this._channel = new Channel(conn_name, channel_path);
        this._closedId = this._channel.connect('Closed', Lang.bind(this, this._channelClosed));

        this._channelText = new ChannelText(conn_name, channel_path);
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
            let bytes = result[0];
            this._avatar = Shell.TextureCache.get_default().load_from_data(bytes, bytes.length, AVATAR_SIZE, AVATAR_SIZE);
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
        log('closed');
        this._channel.disconnect(this._closedId);
        this._channelText.disconnect(this._receivedId);
    },

    _receivedMessage: function(channel, id, timestamp, sender,
                               type, flags, text) {
        log('Received: id ' + id + ', time ' + timestamp + ', sender ' + sender + ', type ' + type + ', flags ' + flags + ': ' + text);
        Main.notificationPopup.show(this._avatar, text);
    }
};
