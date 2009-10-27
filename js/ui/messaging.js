const DBus = imports.dbus;
const Shell = imports.gi.Shell;

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
    name: CONN
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
        log('observing ' + conn_path);
        let conn = new Connection(conn_path);
        let conn_name = conn_path.substr(1).replace('/','.','g');

        for (var i = 0; i < channels.length; i++) {
            let path = channels[i][0];
            let props = channels[i][1];

            let targethandle = props[CHANNEL + '.TargetHandle'];
            let targetid = props[CHANNEL + '.TargetID'];

            // conn.RequestAvatarRemote(targethandle,
            //     function(result, excp) {
            //         log("called for " + targetid);
            //         let avatar;
            //         if (result) {
            //             let bytes = result[0];
            //             avatar = Shell.TextureCache.get_default().load_from_data(bytes, bytes.length, -1, TRAY_HEIGHT);
            //         } else {
            //             // fallback avatar
            //             avatar = Shell.TextureCache.get_default().load_icon_name("stock_person", TRAY_HEIGHT);
            //         }
	    // 	});

            let channel = new Channel(conn_name, path);
            let id = channel.connect('Closed',
		function(emitter) {
                    log('closed');
                    channel.disconnect(id);
                });

            let text = new ChannelText(conn_name, path);
	    text.connect('Received',
		function(chan, id, timestamp, sender, type, flags, text) {
		    log('Received: id ' + id + ', time ' + timestamp + ', sender ' + sender + ', type ' + type + ', flags ' + flags + ': ' + text);
		});
        }

        return [true];
    }
};

DBus.conformExport(Messaging.prototype, ClientIface);
DBus.conformExport(Messaging.prototype, ClientObserverIface);
