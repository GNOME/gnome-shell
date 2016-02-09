// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const ModalDialog = imports.ui.modalDialog;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const LOCATION_SCHEMA = 'org.gnome.system.location';
const MAX_ACCURACY_LEVEL = 'max-accuracy-level';
const ENABLED = 'enabled';

const APP_PERMISSIONS_TABLE = 'desktop';
const APP_PERMISSIONS_ID = 'geolocation';

const GeoclueAccuracyLevel = {
    NONE: 0,
    COUNTRY: 1,
    CITY: 4,
    NEIGHBORHOOD: 5,
    STREET: 6,
    EXACT: 8
};

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

var XdgAppIface = '<node> \
  <interface name="org.freedesktop.XdgApp.PermissionStore"> \
    <method name="Lookup"> \
      <arg name="table" type="s" direction="in"/> \
      <arg name="id" type="s" direction="in"/> \
      <arg name="permissions" type="a{sas}" direction="out"/> \
      <arg name="data" type="v" direction="out"/> \
    </method> \
    <method name="Set"> \
      <arg name="table" type="s" direction="in"/> \
      <arg name="create" type="b" direction="in"/> \
      <arg name="id" type="s" direction="in"/> \
      <arg name="app_permissions" type="a{sas}" direction="in"/> \
      <arg name="data" type="v" direction="in"/> \
    </method> \
  </interface> \
</node>';

const PermissionStore = Gio.DBusProxy.makeProxyWrapper(XdgAppIface);

const Indicator = new Lang.Class({
    Name: 'LocationIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._settings = new Gio.Settings({ schema_id: LOCATION_SCHEMA });
        this._settings.connect('changed::' + ENABLED,
                               Lang.bind(this, this._onMaxAccuracyLevelChanged));
        this._settings.connect('changed::' + MAX_ACCURACY_LEVEL,
                               Lang.bind(this, this._onMaxAccuracyLevelChanged));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'find-location-symbolic';

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._item.icon.icon_name = 'find-location-symbolic';

        this._agent = Gio.DBusExportedObject.wrapJSObject(AgentIface, this);
        this._agent.export(Gio.DBus.system, '/org/freedesktop/GeoClue2/Agent');

        this._item.label.text = _("Location Enabled");
        this._onOffAction = this._item.menu.addAction(_("Disable"), Lang.bind(this, this._onOnOffAction));
        this._item.menu.addSettingsAction(_("Privacy Settings"), 'gnome-privacy-panel.desktop');

        this.menu.addMenuItem(this._item);

        this._watchId = Gio.bus_watch_name(Gio.BusType.SYSTEM,
                                           'org.freedesktop.GeoClue2',
                                           0,
                                           Lang.bind(this, this._connectToGeoclue),
                                           Lang.bind(this, this._onGeoclueVanished));
        Main.sessionMode.connect('updated', Lang.bind(this, this._onSessionUpdated));
        this._onSessionUpdated();
        this._onMaxAccuracyLevelChanged();
        this._connectToGeoclue();
        this._connectToPermissionStore();
    },

    get MaxAccuracyLevel() {
        return this._getMaxAccuracyLevel();
    },

    AuthorizeAppAsync: function(params, invocation) {
        let [desktop_id, reqAccuracyLevel] = params;
        log("%s is requesting location".format(desktop_id));

        let callback = function(level, timesAllowed) {
            if (level >= GeoclueAccuracyLevel.NONE && timesAllowed > 2) {
                log("%s is in store".format(desktop_id));
                let accuracyLevel = clamp(reqAccuracyLevel, 0, level);
                this._completeAuthorizeApp(desktop_id,
                                           accuracyLevel,
                                           timesAllowed,
                                           invocation);
            } else {
                log("%s not in store".format(desktop_id));
                this._userAuthorizeApp(desktop_id,
                                       reqAccuracyLevel,
                                       timesAllowed,
                                       invocation);
            }
        }
        this._fetchPermissionFromStore(desktop_id, Lang.bind(this, callback);
    },

    _userAuthorizeApp: function(desktopId, reqAccuracyLevel, timesAllowed, invocation) {
        var appSystem = Shell.AppSystem.get_default();
        var app = appSystem.lookup_app(desktopId + ".desktop");
        if (app == null) {
            this._completeAuthorizeApp(desktopId,
                                       GeoclueAccuracyLevel.NONE,
                                       timesAllowed,
                                       invocation);
            return;
        }

        var name = app.get_name();
        var icon = app.get_app_info().get_icon();
        var reason = app.get_string("X-Geoclue-Reason");
        var allowCallback = function() {
            this._completeAuthorizeApp(desktopId,
                                       reqAccuracyLevel,
                                       timesAllowed,
                                       invocation);
        };
        var denyCallback = function() {
            this._completeAuthorizeApp(desktopId,
                                       GeoclueAccuracyLevel.NONE,
                                       timesAllowed,
                                       invocation);
        };

        this._showAppAuthDialog(name,
                                reason,
                                icon,
                                Lang.bind(this, allowCallback),
                                Lang.bind(this, denyCallback));
    },

    _showAppAuthDialog: function(name, reason, icon, allowCallback, denyCallback) {
        if (this._dialog == null)
            this._dialog = new GeolocationDialog(name, reason, icon);
        else
            this._dialog.update(name, reason, icon);

        let closedId = this._dialog.connect('closed', function() {
            this._dialog.disconnect(closedId);
            if (this._dialog.allowed)
                allowCallback ();
            else
                denyCallback ();
        }.bind(this));

        this._dialog.open(global.get_current_time ());
    },

    _completeAuthorizeApp: function(desktopId,
                                    accuracyLevel,
                                    timesAllowed,
                                    invocation) {
        if (accuracyLevel == GeoclueAccuracyLevel.NONE) {
            invocation.return_value(GLib.Variant.new('(bu)',
                                                     [false, accuracyLevel]));
            this._saveToPermissionStore(desktopId, accuracyLevel, 0);
            return;
        }

        let allowedAccuracyLevel = clamp(accuracyLevel,
                                         0,
                                         this._getMaxAccuracyLevel());
        invocation.return_value(GLib.Variant.new('(bu)',
                                                 [true, allowedAccuracyLevel]));

        this._saveToPermissionStore(desktopId, allowedAccuracyLevel, timesAllowed + 1);
    },

    _fetchPermissionFromStore: function(desktopId, callback) {
        if (this._permStoreProxy == null) {
            callback (-1, 0);

            return;
        }

        this._permStoreProxy.LookupRemote(APP_PERMISSIONS_TABLE,
                                          APP_PERMISSIONS_ID,
                                          function(result, error) {
            if (error != null) {
                log(error.message);
                callback(-1, 0);

                return;
            }

            let [permissions, data] = result;
            let permission = permissions[desktopId];
            if (permission == null) {
                callback(-1, 0);

                return;
            }

            let levelStr = permission[0];
            let level = GeoclueAccuracyLevel[levelStr.toUpperCase()] ||
                        GeoclueAccuracyLevel.NONE;
            let timesAllowed = data.get_byte();

            callback(level, timesAllowed);
        });
    },

    _saveToPermissionStore: function(desktopId,
                                     allowedAccuracyLevel,
                                     timesAllowed) {
        if (timesAllowed > 2 || this._permStoreProxy == null)
            return;

        let levelStr = Object.keys(GeoclueAccuracyLevel)[allowedAccuracyLevel]; 
        let permission = { desktopId: [levelStr] };
        let permissions = GLib.Variant.new('a{sas}', [permission]));
        let data = GLib.Variant.new('y', timesAllowed);

        this._permStoreProxy.SetRemote(APP_PERMISSIONS_TABLE,
                                       true,
                                       APP_PERMISSIONS_ID,
                                       permissions,
                                       data,
                                       function (result, error) {
            log(error.message);
        });
    },

    _syncIndicator: function() {
        if (this._managerProxy == null) {
            this._indicator.visible = false;
            this._item.actor.visible = false;
            return;
        }

        this._indicator.visible = this._managerProxy.InUse;
        this._item.actor.visible = this._indicator.visible;
        this._updateMenuLabels();
    },

    _connectToGeoclue: function() {
        if (this._managerProxy != null || this._connecting)
            return false;

        this._connecting = true;
        new GeoclueManager(Gio.DBus.system,
                           'org.freedesktop.GeoClue2',
                           '/org/freedesktop/GeoClue2/Manager',
                           Lang.bind(this, this._onManagerProxyReady));
        return true;
    },

    _onManagerProxyReady: function(proxy, error) {
        if (error != null) {
            log(error.message);
            this._connecting = false;
            return;
        }

        this._managerProxy = proxy;
        this._propertiesChangedId = this._managerProxy.connect('g-properties-changed',
                                                        Lang.bind(this, this._onGeocluePropsChanged));

        this._syncIndicator();

        this._managerProxy.AddAgentRemote('gnome-shell', Lang.bind(this, this._onAgentRegistered));
    },

    _onAgentRegistered: function(result, error) {
        this._connecting = false;
        this._notifyMaxAccuracyLevel();

        if (error != null)
            log(error.message);
    },

    _onGeoclueVanished: function() {
        if (this._propertiesChangedId) {
            this._managerProxy.disconnect(this._propertiesChangedId);
            this._propertiesChangedId = 0;
        }
        this._managerProxy = null;

        this._syncIndicator();
    },

    _onOnOffAction: function() {
        let enabled = this._settings.get_boolean(ENABLED);
        this._settings.set_boolean(ENABLED, !enabled);
    },

    _onSessionUpdated: function() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    },

    _updateMenuLabels: function() {
        if (this._settings.get_boolean(ENABLED)) {
            this._item.label.text = this._indicator.visible ? _("Location In Use")
                                                            : _("Location Enabled");
            this._onOffAction.label.text = _("Disable");
        } else {
            this._item.label.text = _("Location Disabled");
            this._onOffAction.label.text = _("Enable");
        }
    },

    _onMaxAccuracyLevelChanged: function() {
        this._updateMenuLabels();

        // Gotta ensure geoclue is up and we are registered as agent to it
        // before we emit the notify for this property change.
        if (!this._connectToGeoclue())
            this._notifyMaxAccuracyLevel();
    },

    _getMaxAccuracyLevel: function() {
        if (this._settings.get_boolean(ENABLED)) {
            let level = this._settings.get_string(MAX_ACCURACY_LEVEL);

            return GeoclueAccuracyLevel[level.toUpperCase()] ||
                   GeoclueAccuracyLevel.NONE;
        } else {
            return GeoclueAccuracyLevel.NONE;
        }
    },

    _notifyMaxAccuracyLevel: function() {
        let variant = new GLib.Variant('u', this._getMaxAccuracyLevel());
        this._agent.emit_property_changed('MaxAccuracyLevel', variant);
    },

    _onGeocluePropsChanged: function(proxy, properties) {
        let unpacked = properties.deep_unpack();
        if ("InUse" in unpacked)
            this._syncIndicator();
    },

    _connectToPermissionStore: function() {
        this._permStoreProxy = null;
        new PermissionStore(Gio.DBus.session,
                           'org.freedesktop.XdgApp',
                           '/org/freedesktop/XdgApp/PermissionStore',
                           Lang.bind(this, this._onPermStoreProxyReady));
    },

    _onPermStoreProxyReady: function(proxy, error) {
        if (error != null) {
            log(error.message);
            return;
        }

        this._permStoreProxy = proxy;
    },
});

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

const GeolocationDialog = new Lang.Class({
    Name: 'GeolocationDialog',
    Extends: ModalDialog.ModalDialog,

    // FIXME: Would be nice to show the application icon too
    _init: function(name, reason, icon) {
        this.parent({ destroyOnClose: false });

        let text = _("'%s' is requesting access to location data.").format (name);
        this._label = new St.Label({ style_class: 'prompt-dialog-description',
                                     text: text });

        this.contentLayout.add(this._label, {});

        if (reason != null) {
            this._reasonLabel = new St.Label({ style_class: 'prompt-dialog-description',
                                               text: reason });
            this.contentLayout.add(this._reasonLabel, {});
        } else {
            this._reasonLabel = null;
        }

        this._allowButton = this.addButton({ label: _("Confirm"),
                                             action: this._onAllowClicked.bind(this),
                                             default: false },
                                           { expand: true, x_fill: false, x_align: St.Align.END });
        this._denyButton = this.addButton({ label: _("Cancel"),
                                            action: this._onDisallowClicked.bind(this),
                                            default: true },
                                          { expand: true, x_fill: false, x_align: St.Align.START });
    },

    update: function(name, reason, icon) {
        let text = _("'%s' is requesting access to location data.").format (name);
        this._label.text = text;

        if (this._reasonLabel != null) {
            this.contentLayout.remove(this._reasonLabel, {});
            this._reasonLabel = null;
        }

        if (reason != null) {
            this._reasonLabel = new St.Label({ style_class: 'prompt-dialog-description',
                                               text: reason });
            this.contentLayout.add(this._reasonLabel, {});
        }
    },

    _onAllowClicked: function() {
        this.allowed = true;
        this.close();
    },

    _onDisallowClicked: function() {
        this.allowed = false;
        this.close();
    }
});
