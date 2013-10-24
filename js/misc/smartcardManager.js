// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const ObjectManager = imports.misc.objectManager;

const SmartcardTokenIface = '<node> \
<interface name="org.gnome.SettingsDaemon.Smartcard.Token"> \
  <property name="Name" type="s" access="read"/> \
  <property name="Driver" type="o" access="read"/> \
  <property name="IsInserted" type="b" access="read"/> \
  <property name="UsedToLogin" type="b" access="read"/> \
</interface> \
</node>';

let _smartcardManager = null;

function getSmartcardManager() {
    if (_smartcardManager == null)
        _smartcardManager = new SmartcardManager();

    return _smartcardManager;
}

const SmartcardManager = new Lang.Class({
    Name: 'SmartcardManager',
    _init: function() {
        this._objectManager = new ObjectManager.ObjectManager({ connection: Gio.DBus.session,
                                                                name: "org.gnome.SettingsDaemon.Smartcard",
                                                                objectPath: '/org/gnome/SettingsDaemon/Smartcard',
                                                                knownInterfaces: [ SmartcardTokenIface ],
                                                                onLoaded: Lang.bind(this, this._onLoaded) });
        this._insertedTokens = {};
        this._loginToken = null;
    },

    _onLoaded: function() {
        let tokens = this._objectManager.getProxiesForInterface('org.gnome.SettingsDaemon.Smartcard.Token');

        for (let i = 0; i < tokens.length; i++)
            this._addToken(tokens[i]);

        this._objectManager.connect('interface-added', Lang.bind(this, function(objectManager, interfaceName, proxy) {
            if (interfaceName == 'org.gnome.SettingsDaemon.Smartcard.Token')
                this._addToken(proxy);
        }));

        this._objectManager.connect('interface-removed', Lang.bind(this, function(objectManager, interfaceName, proxy) {
            if (interfaceName == 'org.gnome.SettingsDaemon.Smartcard.Token')
                this._removeToken(proxy);
        }));
    },

    _updateToken: function(token) {
        let objectPath = token.get_object_path();

        delete this._insertedTokens[objectPath];

        if (token.IsInserted)
            this._insertedTokens[objectPath] = token;

        if (token.UsedToLogin)
            this._loginToken = token;
    },

    _addToken: function(token) {
        this._updateToken(token);

        token.connect('g-properties-changed',
                      Lang.bind(this, function(proxy, properties) {
                          if ('IsInserted' in properties.deep_unpack()) {
                              this._updateToken(token);

                              if (token.IsInserted) {
                                  this.emit('smartcard-inserted', token);
                              } else {
                                  this.emit('smartcard-removed', token);
                              }
                          }
                      }));

        // Emit a smartcard-inserted at startup if it's already plugged in
        if (token.IsInserted)
            this.emit('smartcard-inserted', token);
    },

    _removeToken: function(token) {
        let objectPath = token.get_object_path();

        if (this._insertedTokens[objectPath] == token) {
            delete this._insertedTokens[objectPath];
            this.emit('smartcard-removed', token);
        }

        if (this._loginToken == token)
            this._loginToken = null;

        token.disconnectAll();
    },

    hasInsertedTokens: function() {
        return Object.keys(this._insertedTokens).length > 0;
    },

    hasInsertedLoginToken: function() {
        if (!this._loginToken)
            return false;

        if (!this._loginToken.IsInserted)
            return false;

        return true;
    }

});
Signals.addSignalMethods(SmartcardManager.prototype);
