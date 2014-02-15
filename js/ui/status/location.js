// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;

const PanelMenu = imports.ui.panelMenu;

var GeoclueIface = '<node> \
  <interface name="org.freedesktop.GeoClue2.Manager"> \
    <property name="InUse" type="b" access="read"/> \
  </interface> \
</node>';

const GeoclueManager = Gio.DBusProxy.makeProxyWrapper(GeoclueIface);

const Indicator = new Lang.Class({
    Name: 'LocationIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'find-location-symbolic';
        this._sync();

        this._watchId = Gio.bus_watch_name(Gio.BusType.SYSTEM,
                                           'org.freedesktop.GeoClue2',
                                           0,
                                           Lang.bind(this, this._onGeoclueAppeared),
                                           Lang.bind(this, this._onGeoclueVanished));
    },

    _sync: function() {
        if (this._proxy == null) {
            this._indicator.visible = false;
            return;
        }

        this._indicator.visible = this._proxy.InUse;
    },

    _onGeoclueAppeared: function() {
        new GeoclueManager(Gio.DBus.system,
                           'org.freedesktop.GeoClue2',
                           '/org/freedesktop/GeoClue2/Manager',
                           Lang.bind(this, this._onProxyReady));
    },

    _onProxyReady: function (proxy, error) {
        if (error != null) {
            log (error.message);
            return;
        }

        this._proxy = proxy;
        this._proxy.connect('g-properties-changed', Lang.bind(this, this._sync));

        this._sync();
    },

    _onGeoclueVanished: function() {
        this._proxy = null;

        this._sync();
    }
});
