// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const ObjectManager = imports.misc.objectManager;

const _SMARTCARD_SERVICE_DBUS_NAME = "org.gnome.SettingsDaemon.Smartcard";

const SmartcardManagerIface = <interface name="org.gnome.SettingsDaemon.Smartcard.Manager">
  <method name="GetLoginToken">
      <arg name="token" type="o" direction="out"/>
  </method>
  <method name="GetInsertedTokens">
      <arg name="tokens" type="ao" direction="out"/>
  </method>
</interface>;

const SmartcardTokenIface = <interface name="org.gnome.SettingsDaemon.Smartcard.Token">
  <property name="Name" type="s" access="read"/>
  <property name="Driver" type="o" access="read"/>
  <property name="IsInserted" type="b" access="read"/>
  <property name="UsedToLogin" type="b" access="read"/>
</interface>;

const SmartcardDriverIface = <interface name="org.gnome.SettingsDaemon.Smartcard.Driver">
  <property name="Library" type="s" access="read"/>
  <property name="Description" type="s" access="read"/>
</interface>;

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
                                                                name: _SMARTCARD_SERVICE_DBUS_NAME,
                                                                objectPath: '/org/gnome/SettingsDaemon/Smartcard',
                                                                knownInterfaces: [ SmartcardManagerIface,
                                                                                   SmartcardTokenIface,
                                                                                   SmartcardDriverIface ],
                                                                onLoaded: Lang.bind(this, this._onLoaded) });
        this._insertedTokens = {};
        this._removedTokens = {};
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
        delete this._removedTokens[objectPath];

        if (token.IsInserted)
            this._insertedTokens[objectPath] = token;
        else
            this._removedTokens[objectPath] = token;

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
                                  this.emit('smartcard-inserted', token.Name);
                              } else {
                                  this.emit('smartcard-removed', token.Name);
                              }
                          }
                      }));

        // Emit a smartcard-inserted at startup if it's already plugged in
        if (token.IsInserted)
            this.emit('smartcard-inserted', token.Name);
    },

    _removeToken: function(token) {
        let objectPath = token.get_object_path();

        if (objectPath) {
            if (this._removedTokens[objectPath] == token) {
                delete this._removedTokens[objectPath];
            }

            if (this._insertedTokens[objectPath] == token) {
                delete this._insertedTokens[objectPath];
                this.emit('smartcard-removed', token.Name);
            }
        }

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
