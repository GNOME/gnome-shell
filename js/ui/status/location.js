// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;

const Shell = imports.gi.Shell;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const LOCATION_SCHEMA = 'org.gnome.shell.location';
const MAX_ACCURACY_LEVEL = 'max-accuracy-level';

var GeoclueIface = '<node> \
  <interface name="org.freedesktop.GeoClue2.Manager"> \
    <property name="InUse" type="b" access="read"/> \
    <property name="AvailableAccuracyLevel" type="u" access="read"/> \
    <method name="AddAgent"> \
      <arg name="id" type="s" direction="in"/> \
    </method> \
  </interface> \
</node>';

const GeoclueManager = Gio.DBusProxy.makeProxyWrapper(GeoclueIface);

var AgentIface = '<node> \
  <interface name="org.freedesktop.GeoClue2.Agent"> \
    <property name="MaxAccuracyLevel" type="u" access="read"/> \
    <method name="AuthorizeApp"> \
      <arg name="desktop_id" type="s" direction="in"/> \
      <arg name="req_accuracy_level" type="u" direction="in"/> \
      <arg name="authorized" type="b" direction="out"/> \
      <arg name="allowed_accuracy_level" type="u" direction="out"/> \
    </method> \
  </interface> \
</node>';

const AccuracyLevel = {
    NONE: 0,
    COUNTRY: 1,
    CITY: 4,
    STREET: 6,
    EXACT: 8,
};

const Indicator = new Lang.Class({
    Name: 'LocationIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._settings = new Gio.Settings({ schema: LOCATION_SCHEMA });
        this._settings.connect('changed::' + MAX_ACCURACY_LEVEL,
                               Lang.bind(this, this._onMaxAccuracyLevelChanged));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'find-location-symbolic';

        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Location"), true);
        this._item.icon.icon_name = 'find-location-symbolic';

        var credentials = new Gio.Credentials();
        var uid = credentials.get_unix_user();
        this._agent = Gio.DBusExportedObject.wrapJSObject(AgentIface, this);
        this._agent.export(Gio.DBus.system,
                          '/org/freedesktop/GeoClue2/Agent/' + uid);

        this._item.status.text = _("On");
        this._onOffAction = this._item.menu.addAction(_("Turn Off"), Lang.bind(this, this._onOnOffAction));

        this._accurateItem = new PopupMenu.PopupMenuItem(_("Accurate (GPS + Network)"), false);
        this._accurateItem.connect('activate', Lang.bind(this, this._onAccurateItemActivated));
        this._item.menu.addMenuItem(this._accurateItem);

        this._powerSavingItem = new PopupMenu.PopupMenuItem(_("Power Saving (Network Only)"), false);
        this._powerSavingItem.connect('activate', Lang.bind(this, this._onPowerSavingItemActivated));
        this._item.menu.addMenuItem(this._powerSavingItem);

        this._offItem = new PopupMenu.PopupMenuItem(_("Off"), false);
        this._offItem.connect('activate', Lang.bind(this, this._onOffItemActivated));
        this._item.menu.addMenuItem(this._offItem);

        this.menu.addMenuItem(this._item);

        this._watchId = Gio.bus_watch_name(Gio.BusType.SYSTEM,
                                           'org.freedesktop.GeoClue2',
                                           0,
                                           Lang.bind(this, this._connectToGeoclue),
                                           Lang.bind(this, this._onGeoclueVanished));
        this._onMaxAccuracyLevelChanged();
        this._connectToGeoclue();
    },

    get MaxAccuracyLevel() {
        return this._getMaxAccuracyLevel();
    },

    // We (and geoclue) have currently no way to reliably identifying apps so
    // for now, lets just authorize all apps as long as they provide a valid
    // desktop ID. We also ensure they don't get more accuracy than global max.
    AuthorizeApp: function(desktop_id, reqAccuracyLevel) {
        var appSystem = Shell.AppSystem.get_default();
        var app = appSystem.lookup_app(desktop_id + ".desktop");
        if (app == null) {
            return [false, 0];
        }

        let allowedAccuracyLevel = clamp(reqAccuracyLevel, 0, this._getMaxAccuracyLevel());
        return [true, allowedAccuracyLevel];
    },

    _syncIndicator: function() {
        if (this._proxy == null) {
            this._indicator.visible = false;
            return;
        }

        this._indicator.visible = this._proxy.InUse;
    },

    _connectToGeoclue: function() {
        if (this._proxy != null || this._connecting)
            return false;

        this._connecting = true;
        new GeoclueManager(Gio.DBus.system,
                           'org.freedesktop.GeoClue2',
                           '/org/freedesktop/GeoClue2/Manager',
                           Lang.bind(this, this._onProxyReady));
        return true;
    },

    _onProxyReady: function(proxy, error) {
        if (error != null) {
            log(error.message);
            this._connecting = false;
            return;
        }

        this._proxy = proxy;
        this._propertiesChangedId = this._proxy.connect('g-properties-changed',
                                                        Lang.bind(this, this._onGeocluePropsChanged));

        this._updateMenu();
        this._syncIndicator();

        this._proxy.AddAgentRemote('gnome-shell', Lang.bind(this, this._onAgentRegistered));
    },

    _onAgentRegistered: function(result, error) {
        this._connecting = false;
        this._notifyMaxAccuracyLevel();

        if (error != null)
            log(error.message);
    },

    _onGeoclueVanished: function() {
        if (this._propertiesChangedId) {
            this._proxy.disconnect(this._propertiesChangedId);
            this._propertiesChangedId = 0;
        }
        this._proxy = null;

        this._syncIndicator();
    },

    _onOnOffAction: function() {
        if (this._getMaxAccuracyLevel() == 0)
            this._settings.set_enum(MAX_ACCURACY_LEVEL, this._availableAccuracyLevel);
        else
            this._settings.set_enum(MAX_ACCURACY_LEVEL, AccuracyLevel.NONE);
    },

    _onAccurateItemActivated: function() {
        this._settings.set_enum(MAX_ACCURACY_LEVEL, AccuracyLevel.EXACT);
    },

    _onPowerSavingItemActivated: function() {
        this._settings.set_enum(MAX_ACCURACY_LEVEL, AccuracyLevel.STREET);
    },

    _onOffItemActivated: function() {
        this._settings.set_enum(MAX_ACCURACY_LEVEL, AccuracyLevel.NONE);
    },

    _onMaxAccuracyLevelChanged: function() {
        let maxAccuracyLevel = this._getMaxAccuracyLevel();
        if (this._availableAccuracyLevel < AccuracyLevel.EXACT) {
            if (maxAccuracyLevel == 0) {
                this._item.status.text = _("Off");
                this._onOffAction.label.text = _("Turn On");
            } else {
                this._item.status.text = _("On");
                this._onOffAction.label.text = _("Turn Off");
            }
        } else {
            if (maxAccuracyLevel == 0) {
                this._item.status.text = _("Off");
                this._offItem.setOrnament(PopupMenu.Ornament.DOT);
                this._accurateItem.setOrnament(PopupMenu.Ornament.NONE);
                this._powerSavingItem.setOrnament(PopupMenu.Ornament.NONE);
            } else if (maxAccuracyLevel < AccuracyLevel.EXACT) {
                this._item.status.text = _("Power Saving");
                this._powerSavingItem.setOrnament(PopupMenu.Ornament.DOT);
                this._accurateItem.setOrnament(PopupMenu.Ornament.NONE);
                this._offItem.setOrnament(PopupMenu.Ornament.NONE);
            } else {
                this._item.status.text = _("Accurate");
                this._accurateItem.setOrnament(PopupMenu.Ornament.DOT);
                this._offItem.setOrnament(PopupMenu.Ornament.NONE);
                this._powerSavingItem.setOrnament(PopupMenu.Ornament.NONE);
            }
        }

        // Gotta ensure geoclue is up and we are registered as agent to it
        // before we emit the notify for this property change.
        if (!this._connectToGeoclue())
            this._notifyMaxAccuracyLevel();
    },

    _getMaxAccuracyLevel: function() {
        return this._settings.get_enum(MAX_ACCURACY_LEVEL);
    },

    _notifyMaxAccuracyLevel: function() {
        let variant = new GLib.Variant('u', this._getMaxAccuracyLevel());
        this._agent.emit_property_changed('MaxAccuracyLevel', variant);
    },

    _updateMenu: function() {
        this._availableAccuracyLevel = this._proxy.AvailableAccuracyLevel;
        this.menu.actor.visible = (this._availableAccuracyLevel != 0);
        if (this._availableAccuracyLevel == 0)
            return;

        if (this._availableAccuracyLevel < AccuracyLevel.EXACT) {
            this._onOffAction.actor.show();
            this._accurateItem.actor.hide();
            this._powerSavingItem.actor.hide();
            this._offItem.actor.hide();
        } else {
            this._onOffAction.actor.hide();
            this._accurateItem.actor.show();
            this._powerSavingItem.actor.show();
            this._offItem.actor.show();
        }

        this._onMaxAccuracyLevelChanged();
    },

    _onGeocluePropsChanged: function(proxy, properties) {
        let unpacked = properties.deep_unpack();
        if ("InUse" in unpacked)
            this._syncIndicator();
        if ("AvailableAccuracyLevel" in unpacked)
            this._updateMenu();
    }
});

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}
