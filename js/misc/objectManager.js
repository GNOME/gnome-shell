// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Params = imports.misc.params;
const Signals = imports.signals;

// Specified in the D-Bus specification here:
// http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager
const ObjectManagerIface = <interface name="org.freedesktop.DBus.ObjectManager">
  <method name="GetManagedObjects">
    <arg name="objects" type="a{oa{sa{sv}}}" direction="out"/>
  </method>
  <signal name="InterfacesAdded">
    <arg name="objectPath" type="o"/>
    <arg name="interfaces" type="a{sa{sv}}" />
  </signal>
  <signal name="InterfacesRemoved">
    <arg name="objectPath" type="o"/>
    <arg name="interfaces" type="as" />
  </signal>
</interface>

const ObjectManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(ObjectManagerIface);

const ObjectManager = new Lang.Class({
    Name: 'ObjectManager',
    _init: function(params) {
        params = Params.parse(params, { connection: null,
                                        name: null,
                                        objectPath: null,
                                        knownInterfaces: null,
                                        cancellable: null,
                                        onLoaded: null });

        this._connection = params.connection;
        this._serviceName = params.name;
        this._managerPath = params.objectPath;
        this._cancellable = params.cancellable;

        this._managerProxy = new Gio.DBusProxy({ g_connection: this._connection,
                                                 g_interface_name: ObjectManagerInfo.name,
                                                 g_interface_info: ObjectManagerInfo,
                                                 g_name: this._serviceName,
                                                 g_object_path: this._managerPath,
                                                 g_flags: Gio.DBusProxyFlags.NONE });

        this._interfaceInfos = {};
        this._objects = {};
        this._interfaces = {};
        this._pendingProxies = [];
        this._onLoaded = params.onLoaded;

        if (params.knownInterfaces)
            this._registerInterfaces(params.knownInterfaces);

        this._managerProxy.init_async(GLib.PRIORITY_DEFAULT,
                                      this._cancellable,
                                      Lang.bind(this, this._onManagerProxyLoaded));
    },

    _addInterface: function(objectPath, interfaceName, onFinished) {
        let info = this._interfaceInfos[interfaceName];

        if (!info)
            return;

        let proxy = new Gio.DBusProxy({ g_connection: this._connection,
                                        g_name: this._serviceName,
                                        g_object_path: objectPath,
                                        g_interface_name: interfaceName,
                                        g_interface_info: info,
                                        g_flags: Gio.DBusProxyFlags.NONE });

        this._pendingProxies.push(proxy);

        proxy.init_async(GLib.PRIORITY_DEFAULT,
                         this._cancellable,
                         Lang.bind(this, function(initable, result) {
                             let index = this._pendingProxies.indexOf(proxy);

                             if (index >= 0)
                                 this._pendingProxies.splice(index, 1);

                             let error = null;
                             try {
                                 initable.init_finish(result);
                             } catch(e) {
                                 logError(e, 'could not initialize proxy for interface ' + interfaceName);

                                 if (onFinished)
                                     onFinished();
                                 return;
                             }

                             let isNewObject;

                             if (!this._objects[objectPath]) {
                                 this._objects[objectPath] = {};
                                 isNewObject = true;
                             } else {
                                 isNewObject = false;
                             }

                             this._objects[objectPath][interfaceName] = proxy;

                             if (!this._interfaces[interfaceName])
                                 this._interfaces[interfaceName] = [];

                             this._interfaces[interfaceName].push(proxy);

                             if (isNewObject)
                                 this.emit('object-added', objectPath);

                             this.emit('interface-added', interfaceName, proxy);

                             if (onFinished)
                                 onFinished();
                         }));
    },

    _removeInterface: function(objectPath, interfaceName) {
        if (!this._objects[objectPath])
            return;

        let proxy = this._objects[objectPath][interfaceName];

        if (this._interfaces[interfaceName]) {
            let index = this._interfaces[interfaceName].indexOf(proxy);

            if (index >= 0)
                this._interfaces[interfaceName].splice(index, 1);

            if (this._interfaces[interfaceName].length == 0)
                delete this._interfaces[interfaceName];
        }

        this.emit('interface-removed', interfaceName, proxy);

        this._objects[objectPath][interfaceName] = null;

        if (Object.keys(this._objects[objectPath]).length == 0) {
            delete this._objects[objectPath];
            this.emit('object-removed', objectPath);
        }
    },

    _onManagerProxyLoaded: function(initable, result) {
        let error = null;
        try {
            initable.init_finish(result);
        } catch(e) {
            logError(e, 'could not initialize object manager for object ' + params.name);

            if (this._onLoaded)
                this._onLoaded();
            return;
        }

        this._managerProxy.connectSignal('InterfacesAdded',
                                         Lang.bind(this, function(objectManager, sender, [objectPath, interfaces]) {
                                             let interfaceNames = Object.keys(interfaces);
                                             for (let i = 0; i < interfaceNames.length; i++)
                                                 this._addInterface(objectPath, interfaceNames[i]);
                                         }));
        this._managerProxy.connectSignal('InterfacesRemoved',
                                         Lang.bind(this, function(objectManager, sender, [objectPath, interfaceNames]) {
                                             for (let i = 0; i < interfaceNames.length; i++)
                                                 this._removeInterface(objectPath, interfaceNames[i]);
                                         }));


        this._managerProxy.GetManagedObjectsRemote(Lang.bind(this, function(result, error) {
            if (!result) {
                if (error) {
                   logError(error, 'could not get remote objects for service ' + this._serviceName + ' path ' + this._managerPath);
                }

                if (this._onLoaded)
                    this._onLoaded();
                return;
            }

            let [objects] = result;

            if (Object.keys(this._interfaceInfos).length == 0) {
                if (this._onLoaded)
                    this._onLoaded();
                return;
            }

            let numLoadInhibitors = 0;

            // First inhibitor is to prevent onLoaded from getting
            // called until all interfaces have started being added.
            // Subsequent inhibitors are to prevent onLoaded from getting
            // called until all interfaces finish getting added.
            numLoadInhibitors++;
            let objectPaths = Object.keys(objects);
            for (let i = 0; i < objectPaths.length; i++) {
                let objectPath = objectPaths[i];
                let object = objects[objectPath];

                let interfaceNames = Object.keys(object);
                for (let j = 0; j < interfaceNames.length; j++) {
                    let interfaceName = interfaceNames[j];

                    numLoadInhibitors++;
                    this._addInterface(objectPath,
                                       interfaceName,
                                       Lang.bind(this, function() {
                                           numLoadInhibitors--;

                                           if (numLoadInhibitors == 0) {
                                               if (this._onLoaded)
                                                   this._onLoaded();
                                           }
                                       }));
                }
            }
            numLoadInhibitors--;

            if (numLoadInhibitors == 0) {
                if (this._onLoaded)
                    this._onLoaded();
            }
        }));
    },

    _registerInterfaces: function(interfaces) {
        for (let i = 0; i < interfaces.length; i++) {
            let info = Gio.DBusInterfaceInfo.new_for_xml(interfaces[i]);

            this._interfaceInfos[info.name] = info;
        }
    },

    getProxy: function(objectPath, interfaceName) {
        let object = this._objects[objectPath];

        if (!object)
            return null;

        return object[interfaceName];
    },

    getProxiesForInterface: function(interfaceName) {
        let proxyList = this._interfaces[interfaceName];

        if (!proxyList)
            return [];

        return proxyList;
    },

    getAllProxies: function() {
        let proxies = [];

        let objectPaths = Object.keys(this._objects);
        for (let i = 0; i < objectPaths.length; i++) {
            let object = this._objects[objectPaths];

            let interfaceNames = Object.keys(object);
            for (let j = 0; i < interfaceNames.length; i++) {
                let interfaceName = interfaceNames[i];
                if (object[interfaceName])
                    proxies.push(object(interfaceName));
            }
        }

        return proxies;
    }
});
Signals.addSignalMethods(ObjectManager.prototype);
