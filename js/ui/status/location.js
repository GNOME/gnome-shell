// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;

const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const ModalDialog = imports.ui.modalDialog;
const PermissionStore = imports.misc.permissionStore;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const LOCATION_SCHEMA = 'org.gnome.system.location';
const MAX_ACCURACY_LEVEL = 'max-accuracy-level';
const ENABLED = 'enabled';

const APP_PERMISSIONS_TABLE = 'gnome';
const APP_PERMISSIONS_ID = 'geolocation';

var GeoclueAccuracyLevel = {
    NONE: 0,
    COUNTRY: 1,
    CITY: 4,
    NEIGHBORHOOD: 5,
    STREET: 6,
    EXACT: 8
};

function accuracyLevelToString(accuracyLevel) {
    for (let key in GeoclueAccuracyLevel) {
        if (GeoclueAccuracyLevel[key] == accuracyLevel)
            return key;
    }

    return 'NONE';
}

var GeoclueIface = `<node>
  <interface name="org.freedesktop.GeoClue2.Manager">
    <property name="InUse" type="b" access="read"/>
    <property name="AvailableAccuracyLevel" type="u" access="read"/>
    <method name="AddAgent">
      <arg name="id" type="s" direction="in"/>
    </method>
  </interface>
</node>`;

const GeoclueManager = Gio.DBusProxy.makeProxyWrapper(GeoclueIface);

var AgentIface = `<node>
  <interface name="org.freedesktop.GeoClue2.Agent">
    <property name="MaxAccuracyLevel" type="u" access="read"/>
    <method name="AuthorizeApp">
      <arg name="desktop_id" type="s" direction="in"/>
      <arg name="req_accuracy_level" type="u" direction="in"/>
      <arg name="authorized" type="b" direction="out"/>
      <arg name="allowed_accuracy_level" type="u" direction="out"/>
    </method>
  </interface>
</node>`;

var Indicator = new Lang.Class({
    Name: 'LocationIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._settings = new Gio.Settings({ schema_id: LOCATION_SCHEMA });
        this._settings.connect('changed::' + ENABLED,
                               this._onMaxAccuracyLevelChanged.bind(this));
        this._settings.connect('changed::' + MAX_ACCURACY_LEVEL,
                               this._onMaxAccuracyLevelChanged.bind(this));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'find-location-symbolic';

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._item.icon.icon_name = 'find-location-symbolic';

        this._agent = Gio.DBusExportedObject.wrapJSObject(AgentIface, this);
        this._agent.export(Gio.DBus.system, '/org/freedesktop/GeoClue2/Agent');

        this._item.label.text = _("Location Enabled");
        this._onOffAction = this._item.menu.addAction(_("Disable"), this._onOnOffAction.bind(this));
        this._item.menu.addSettingsAction(_("Privacy Settings"), 'gnome-privacy-panel.desktop');

        this.menu.addMenuItem(this._item);

        this._watchId = Gio.bus_watch_name(Gio.BusType.SYSTEM,
                                           'org.freedesktop.GeoClue2',
                                           0,
                                           this._connectToGeoclue.bind(this),
                                           this._onGeoclueVanished.bind(this));
        Main.sessionMode.connect('updated', this._onSessionUpdated.bind(this));
        this._onSessionUpdated();
        this._onMaxAccuracyLevelChanged();
        this._connectToGeoclue();
        this._connectToPermissionStore();
    },

    get MaxAccuracyLevel() {
        return this._getMaxAccuracyLevel();
    },

    AuthorizeAppAsync(params, invocation) {
        let [desktopId, reqAccuracyLevel] = params;

        let authorizer = new AppAuthorizer(desktopId,
                                           reqAccuracyLevel,
                                           this._permStoreProxy,
                                           this._getMaxAccuracyLevel());

        authorizer.authorize(accuracyLevel => {
            let ret = (accuracyLevel != GeoclueAccuracyLevel.NONE);
            invocation.return_value(GLib.Variant.new('(bu)',
                                                     [ret, accuracyLevel]));
        });
    },

    _syncIndicator() {
        if (this._managerProxy == null) {
            this._indicator.visible = false;
            this._item.actor.visible = false;
            return;
        }

        this._indicator.visible = this._managerProxy.InUse;
        this._item.actor.visible = this._indicator.visible;
        this._updateMenuLabels();
    },

    _connectToGeoclue() {
        if (this._managerProxy != null || this._connecting)
            return false;

        this._connecting = true;
        new GeoclueManager(Gio.DBus.system,
                           'org.freedesktop.GeoClue2',
                           '/org/freedesktop/GeoClue2/Manager',
                           this._onManagerProxyReady.bind(this));
        return true;
    },

    _onManagerProxyReady(proxy, error) {
        if (error != null) {
            log(error.message);
            this._connecting = false;
            return;
        }

        this._managerProxy = proxy;
        this._propertiesChangedId = this._managerProxy.connect('g-properties-changed',
                                                        this._onGeocluePropsChanged.bind(this));

        this._syncIndicator();

        this._managerProxy.AddAgentRemote('gnome-shell', this._onAgentRegistered.bind(this));
    },

    _onAgentRegistered(result, error) {
        this._connecting = false;
        this._notifyMaxAccuracyLevel();

        if (error != null)
            log(error.message);
    },

    _onGeoclueVanished() {
        if (this._propertiesChangedId) {
            this._managerProxy.disconnect(this._propertiesChangedId);
            this._propertiesChangedId = 0;
        }
        this._managerProxy = null;

        this._syncIndicator();
    },

    _onOnOffAction() {
        let enabled = this._settings.get_boolean(ENABLED);
        this._settings.set_boolean(ENABLED, !enabled);
    },

    _onSessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    },

    _updateMenuLabels() {
        if (this._settings.get_boolean(ENABLED)) {
            this._item.label.text = this._indicator.visible ? _("Location In Use")
                                                            : _("Location Enabled");
            this._onOffAction.label.text = _("Disable");
        } else {
            this._item.label.text = _("Location Disabled");
            this._onOffAction.label.text = _("Enable");
        }
    },

    _onMaxAccuracyLevelChanged() {
        this._updateMenuLabels();

        // Gotta ensure geoclue is up and we are registered as agent to it
        // before we emit the notify for this property change.
        if (!this._connectToGeoclue())
            this._notifyMaxAccuracyLevel();
    },

    _getMaxAccuracyLevel() {
        if (this._settings.get_boolean(ENABLED)) {
            let level = this._settings.get_string(MAX_ACCURACY_LEVEL);

            return GeoclueAccuracyLevel[level.toUpperCase()] ||
                   GeoclueAccuracyLevel.NONE;
        } else {
            return GeoclueAccuracyLevel.NONE;
        }
    },

    _notifyMaxAccuracyLevel() {
        let variant = new GLib.Variant('u', this._getMaxAccuracyLevel());
        this._agent.emit_property_changed('MaxAccuracyLevel', variant);
    },

    _onGeocluePropsChanged(proxy, properties) {
        let unpacked = properties.deep_unpack();
        if ("InUse" in unpacked)
            this._syncIndicator();
    },

    _connectToPermissionStore() {
        this._permStoreProxy = null;
        new PermissionStore.PermissionStore(this._onPermStoreProxyReady.bind(this));
    },

    _onPermStoreProxyReady(proxy, error) {
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

var AppAuthorizer = new Lang.Class({
    Name: 'LocationAppAuthorizer',

    _init(desktopId,
                    reqAccuracyLevel,
                    permStoreProxy,
                    maxAccuracyLevel) {
        this.desktopId = desktopId;
        this.reqAccuracyLevel = reqAccuracyLevel;
        this._permStoreProxy = permStoreProxy;
        this._maxAccuracyLevel = maxAccuracyLevel;
        this._permissions = {};

        this._accuracyLevel = GeoclueAccuracyLevel.NONE;
    },

    authorize(onAuthDone) {
        this._onAuthDone = onAuthDone;

        let appSystem = Shell.AppSystem.get_default();
        this._app = appSystem.lookup_app(this.desktopId + ".desktop");
        if (this._app == null || this._permStoreProxy == null) {
            this._completeAuth();

            return;
        }

        this._permStoreProxy.LookupRemote(APP_PERMISSIONS_TABLE,
                                          APP_PERMISSIONS_ID,
                                          this._onPermLookupDone.bind(this));
    },

    _onPermLookupDone(result, error) {
        if (error != null) {
            if (error.domain == Gio.DBusError) {
                // Likely no xdg-app installed, just authorize the app
                this._accuracyLevel = this.reqAccuracyLevel;
                this._permStoreProxy = null;
                this._completeAuth();
            } else {
                // Currently xdg-app throws an error if we lookup for
                // unknown ID (which would be the case first time this code
                // runs) so we continue with user authorization as normal
                // and ID is added to the store if user says "yes".
                log(error.message);
                this._permissions = {};
                this._userAuthorizeApp();
            }

            return;
        }

        [this._permissions] = result;
        let permission = this._permissions[this.desktopId];

        if (permission == null) {
            this._userAuthorizeApp();
        } else {
            let [levelStr] = permission || ['NONE'];
            this._accuracyLevel = GeoclueAccuracyLevel[levelStr] ||
                                  GeoclueAccuracyLevel.NONE;
            this._completeAuth();
        }
    },

    _userAuthorizeApp() {
        let name = this._app.get_name();
        let appInfo = this._app.get_app_info();
        let reason = appInfo.get_locale_string("X-Geoclue-Reason");

        this._showAppAuthDialog(name, reason);
    },

    _showAppAuthDialog(name, reason) {
        this._dialog = new GeolocationDialog(name,
                                             reason,
                                             this.reqAccuracyLevel);

        let responseId = this._dialog.connect('response', (dialog, level) => {
            this._dialog.disconnect(responseId);
            this._accuracyLevel = level;
            this._completeAuth();
        });

        this._dialog.open();
    },

    _completeAuth() {
        if (this._accuracyLevel != GeoclueAccuracyLevel.NONE) {
            this._accuracyLevel = clamp(this._accuracyLevel,
                                        0,
                                        this._maxAccuracyLevel);
        }
        this._saveToPermissionStore();

        this._onAuthDone(this._accuracyLevel);
    },

    _saveToPermissionStore() {
        if (this._permStoreProxy == null)
            return;

        let levelStr = accuracyLevelToString(this._accuracyLevel);
        let dateStr = Math.round(Date.now() / 1000).toString();
        this._permissions[this.desktopId] = [levelStr, dateStr];

        let data = GLib.Variant.new('av', {});

        this._permStoreProxy.SetRemote(APP_PERMISSIONS_TABLE,
                                       true,
                                       APP_PERMISSIONS_ID,
                                       this._permissions,
                                       data,
                                       (result, error) => {
            if (error != null)
                log(error.message);
        });
    },
});

var GeolocationDialog = new Lang.Class({
    Name: 'GeolocationDialog',
    Extends: ModalDialog.ModalDialog,

    _init(name, subtitle, reqAccuracyLevel) {
        this.parent({ styleClass: 'geolocation-dialog' });
        this.reqAccuracyLevel = reqAccuracyLevel;

        let icon = new Gio.ThemedIcon({ name: 'find-location-symbolic' });

        /* Translators: %s is an application name */
        let title = _("Give %s access to your location?").format(name);
        let body = _("Location access can be changed at any time from the privacy settings.");

        let contentParams = { icon, title, subtitle, body };
        let content = new Dialog.MessageDialogContent(contentParams);
        this.contentLayout.add_actor(content);

        let button = this.addButton({ label: _("Deny Access"),
                                      action: this._onDenyClicked.bind(this),
                                      key: Clutter.KEY_Escape });
        this.addButton({ label: _("Grant Access"),
                         action: this._onGrantClicked.bind(this) });

        this.setInitialKeyFocus(button);
    },

    _onGrantClicked() {
        this.emit('response', this.reqAccuracyLevel);
        this.close();
    },

    _onDenyClicked() {
        this.emit('response', GeoclueAccuracyLevel.NONE);
        this.close();
    }
});
Signals.addSignalMethods(GeolocationDialog.prototype);
