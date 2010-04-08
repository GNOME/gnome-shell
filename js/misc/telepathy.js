/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;

// D-Bus utils; should eventually move to gjs.
// https://bugzilla.gnome.org/show_bug.cgi?id=610859

function makeProxyClass(iface) {
    let constructor = function() { this._init.apply(this, arguments); };

    constructor.prototype._init = function(bus, name, path) {
        bus.proxifyObject(this, name, path);
    };

    DBus.proxifyPrototype(constructor.prototype, iface);
    return constructor;
}

function nameToPath(name) {
    return '/' + name.replace('.', '/', 'g');
};

function pathToName(path) {
    if (path[0] != '/')
	throw new Error('not a D-Bus path: ' + path);
    return path.substr(1).replace('/', '.', 'g');
};

// This is tp_escape_as_identifier() from telepathy-glib
function escapeAsIdentifier(name) {
    if (!name)
        return '_';

    // first char is replaced with _XX if it's non-alpha,
    // later chars are replaced with _XX if they're non-alphanumeric
    if (name.length == 1) {
        return name.replace(/[^a-zA-Z]/, _hexEscape);
    } else {
        return (name[0].replace(/[^a-zA-Z]/, _hexEscape) +
                name.substring(1).replace(/[^a-zA-Z0-9]/g, _hexEscape));
    }
}

function _hexEscape(ch) {
    return '_' + ch.charCodeAt(0).toString(16);
}

// Telepathy D-Bus interface definitions. Note that most of these are
// incomplete, and only cover the methods/properties/signals that
// we're currently using.

const TELEPATHY = 'org.freedesktop.Telepathy';

const CLIENT_NAME = TELEPATHY + '.Client';
const ClientIface = {
    name: CLIENT_NAME,
    properties: [
        { name: 'Interfaces',
          signature: 'as',
          access: 'read' }
    ]
};

const CLIENT_APPROVER_NAME = TELEPATHY + '.Client.Approver';
const ClientApproverIface = {
    name: CLIENT_APPROVER_NAME,
    methods: [
        { name: 'AddDispatchOperation',
          inSignature: 'a(oa{sv})oa{sv}',
          outSignature: '' }
    ],
    properties: [
        { name: 'ApproverChannelFilter',
          signature: 'aa{sv}',
          access: 'read' }
    ]
};

const CLIENT_HANDLER_NAME = TELEPATHY + '.Client.Handler';
const ClientHandlerIface = {
    name: CLIENT_HANDLER_NAME,
    methods: [
        { name: 'HandleChannels',
          inSignature: 'ooa(oa{sv})aota{sv}',
          outSignature: '' }
    ],
    properties: [
        { name: 'HandlerChannelFilter',
          signature: 'aa{sv}',
          access: 'read' }
    ]
};

const CLIENT_OBSERVER_NAME = TELEPATHY + '.Client.Observer';
const ClientObserverIface = {
    name: CLIENT_OBSERVER_NAME,
    methods: [
        { name: 'ObserveChannels',
          inSignature: 'ooa(oa{sv})oaoa{sv}',
          outSignature: '' }
    ],
    properties: [
        { name: 'ObserverChannelFilter',
          signature: 'aa{sv}',
          access: 'read' }
    ]
};

const CHANNEL_DISPATCH_OPERATION_NAME = TELEPATHY + '.ChannelDispatchOperation';
const ChannelDispatchOperationIface = {
    name: CHANNEL_DISPATCH_OPERATION_NAME,
    methods: [
        { name: 'HandleWith',
          inSignature: 's',
          outSignature: '' },
        { name: 'Claim',
          inSignature: '',
          outSignature: '' }
    ]
};
let ChannelDispatchOperation = makeProxyClass(ChannelDispatchOperationIface);

const CONNECTION_NAME = TELEPATHY + '.Connection';
const ConnectionIface = {
    name: CONNECTION_NAME,
    signals: [
        { name: 'StatusChanged',
          inSignature: 'uu' }
    ]
};
let Connection = makeProxyClass(ConnectionIface);

const ConnectionStatus = {
    CONNECTED:    0,
    CONNECTING:   1,
    DISCONNECTED: 2
};

const CONNECTION_ALIASING_NAME = CONNECTION_NAME + '.Interface.Aliasing';
const ConnectionAliasingIface = {
    name: CONNECTION_ALIASING_NAME,
    methods: [
        { name: 'RequestAliases',
          inSignature: 'au',
          outSignature: 'as'
        }
    ],
    signals: [
        { name: 'AliasesChanged',
          inSignature: 'a(us)' }
    ]
};
let ConnectionAliasing = makeProxyClass(ConnectionAliasingIface);

const CONNECTION_AVATARS_NAME = CONNECTION_NAME + '.Interface.Avatars';
const ConnectionAvatarsIface = {
    name: CONNECTION_AVATARS_NAME,
    methods: [
        { name: 'GetKnownAvatarTokens',
          inSignature: 'au',
          outSignature: 'a{us}'
        },
        { name: 'RequestAvatars',
          inSignature: 'au',
          outSignature: ''
        }
    ],
    signals: [
        { name: 'AvatarRetrieved',
          inSignature: 'usays'
        },
        { name: 'AvatarUpdated',
          inSignature: 'us'
        }
    ]
};
let ConnectionAvatars = makeProxyClass(ConnectionAvatarsIface);

const CONNECTION_REQUESTS_NAME = CONNECTION_NAME + '.Interface.Requests';
const ConnectionRequestsIface = {
    name: CONNECTION_REQUESTS_NAME,
    methods: [
        { name: 'CreateChannel',
          inSignature: 'a{sv}',
          outSignature: 'oa{sv}'
        },
        { name: 'EnsureChannel',
          inSignature: 'a{sv}',
          outSignature: 'boa{sv}'
        }
    ],
    properties: [
        { name: 'Channels',
          signature: 'a(oa{sv})',
          access: 'read' }
    ],
    signals: [
        { name: 'NewChannels',
          inSignature: 'a(oa{sv})'
        },
        { name: 'ChannelClosed',
          inSignature: 'o'
        }
    ]
};
let ConnectionRequests = makeProxyClass(ConnectionRequestsIface);

const HandleType = {
    NONE:    0,
    CONTACT: 1,
    ROOM:    2,
    LIST:    3,
    GROUP:   4
};

const CHANNEL_NAME = TELEPATHY + '.Channel';
const ChannelIface = {
    name: CHANNEL_NAME,
    signals: [
        { name: 'Closed',
          inSignature: '' }
    ]
};
let Channel = makeProxyClass(ChannelIface);

const CHANNEL_TEXT_NAME = CHANNEL_NAME + '.Type.Text';
const ChannelTextIface = {
    name: CHANNEL_TEXT_NAME,
    methods: [
        { name: 'ListPendingMessages',
          inSignature: 'b',
          outSignature: 'a(uuuuus)'
        },
        { name: 'AcknowledgePendingMessages',
          inSignature: 'au',
          outSignature: ''
        },
        { name: 'Send',
          inSignature: 'us',
          outSignature: ''
        }
    ],
    signals: [
        { name: 'Received',
          inSignature: 'uuuuus' }
    ]
};
let ChannelText = makeProxyClass(ChannelTextIface);

const ChannelTextMessageType = {
    NORMAL: 0,
    ACTION: 1,
    NOTICE: 2,
    AUTO_REPLY: 3,
    DELIVERY_REPORT: 4
};

const ACCOUNT_MANAGER_NAME = TELEPATHY + '.AccountManager';
const AccountManagerIface = {
    name: ACCOUNT_MANAGER_NAME,
    properties: [
        { name: 'ValidAccounts',
          signature: 'ao',
          access: 'read' }
    ]
};
let AccountManager = makeProxyClass(AccountManagerIface);

const ACCOUNT_NAME = TELEPATHY + '.Account';
const AccountIface = {
    name: ACCOUNT_NAME,
    properties: [
        { name: 'Connection',
          signature: 'o',
          access: 'read' }
    ]
};
let Account = makeProxyClass(AccountIface);

const CHANNEL_DISPATCHER_NAME = TELEPATHY + '.ChannelDispatcher';
const ChannelDispatcherIface = {
    name: CHANNEL_DISPATCHER_NAME,
    methods: [
        { name: 'EnsureChannel',
          inSignature: 'oa{sv}xs',
          outSignature: 'o' }
    ]
};
let ChannelDispatcher = makeProxyClass(ChannelDispatcherIface);

const CHANNEL_REQUEST_NAME = TELEPATHY + '.ChannelRequest';
const ChannelRequestIface = {
    name: CHANNEL_REQUEST_NAME,
    methods: [
        { name: 'Proceed',
          inSignature: '',
          outSignature: '' }
    ],
    signals: [
        { name: 'Failed',
          signature: 'ss' },
        { name: 'Succeeded',
          signature: '' }
    ]
};
let ChannelRequest = makeProxyClass(ChannelRequestIface);
