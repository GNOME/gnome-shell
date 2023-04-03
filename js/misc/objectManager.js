// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ObjectManager */

const { Gio, GLib } = imports.gi;
const Params = imports.misc.params;
const Signals = imports.misc.signals;

// Specified in the D-Bus specification here:
// http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager
const ObjectManagerIface = `
<node>
<interface name="org.freedesktop.DBus.ObjectManager">
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
</node>`;

const ObjectManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(ObjectManagerIface);

var ObjectManager = class extends Signals.EventEmitter {
    constructor(params) {
        super();

        params = Params.parse(params, {
            connection: null,
            name: null,
            objectPath: null,
            knownInterfaces: null,
            cancellable: null,
            onLoaded: null,
        });

        this._connection = params.connection;
        this._serviceName = params.name;
        this._managerPath = params.objectPath;
        this._cancellable = params.cancellable;

        this._managerProxy = new Gio.DBusProxy({
            g_connection: this._connection,
            g_interface_name: ObjectManagerInfo.name,
            g_interface_info: ObjectManagerInfo,
            g_name: this._serviceName,
            g_object_path: this._managerPath,
            g_flags: Gio.DBusProxyFlags.DO_NOT_AUTO_START,
        });

        this._interfaceInfos = {};
        this._objects = {};
        this._interfaces = {};
        this._onLoaded = params.onLoaded;

        if (params.knownInterfaces)
            this._registerInterfaces(params.knownInterfaces);

        this._initManagerProxy();
    }

    _completeLoad() {
        if (this._onLoaded)
            this._onLoaded();
    }

    async _addInterface(objectPath, interfaceName) {
        let info = this._interfaceInfos[interfaceName];

        if (!info)
            return;

        const proxy = new Gio.DBusProxy({
            g_connection: this._connection,
            g_name: this._serviceName,
            g_object_path: objectPath,
            g_interface_name: interfaceName,
            g_interface_info: info,
            g_flags: Gio.DBusProxyFlags.DO_NOT_AUTO_START,
        });

        try {
            await proxy.init_async(GLib.PRIORITY_DEFAULT, this._cancellable);
        } catch (e) {
            logError(e, `could not initialize proxy for interface ${interfaceName}`);
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
    }

    _removeInterface(objectPath, interfaceName) {
        if (!this._objects[objectPath])
            return;

        let proxy = this._objects[objectPath][interfaceName];

        if (this._interfaces[interfaceName]) {
            let index = this._interfaces[interfaceName].indexOf(proxy);

            if (index >= 0)
                this._interfaces[interfaceName].splice(index, 1);

            if (this._interfaces[interfaceName].length === 0)
                delete this._interfaces[interfaceName];
        }

        this.emit('interface-removed', interfaceName, proxy);

        delete this._objects[objectPath][interfaceName];

        if (Object.keys(this._objects[objectPath]).length === 0) {
            delete this._objects[objectPath];
            this.emit('object-removed', objectPath);
        }
    }

    async _initManagerProxy() {
        try {
            await this._managerProxy.init_async(
                GLib.PRIORITY_DEFAULT, this._cancellable);
        } catch (e) {
            logError(e, `could not initialize object manager for object ${this._serviceName}`);

            this._completeLoad();
            return;
        }

        this._managerProxy.connectSignal('InterfacesAdded',
            (objectManager, sender, [objectPath, interfaces]) => {
                let interfaceNames = Object.keys(interfaces);
                for (let i = 0; i < interfaceNames.length; i++)
                    this._addInterface(objectPath, interfaceNames[i]);
            });
        this._managerProxy.connectSignal('InterfacesRemoved',
            (objectManager, sender, [objectPath, interfaceNames]) => {
                for (let i = 0; i < interfaceNames.length; i++)
                    this._removeInterface(objectPath, interfaceNames[i]);
            });

        if (Object.keys(this._interfaceInfos).length === 0) {
            this._completeLoad();
            return;
        }

        this._managerProxy.connect('notify::g-name-owner', () => {
            if (this._managerProxy.g_name_owner)
                this._onNameAppeared();
            else
                this._onNameVanished();
        });

        if (this._managerProxy.g_name_owner)
            this._onNameAppeared();
    }

    async _onNameAppeared() {
        try {
            const [objects] = await this._managerProxy.GetManagedObjectsAsync();

            if (!objects) {
                this._completeLoad();
                return;
            }

            const objectPaths = Object.keys(objects);
            await Promise.allSettled(objectPaths.flatMap(objectPath => {
                const object = objects[objectPath];
                const interfaceNames = Object.getOwnPropertyNames(object);
                return interfaceNames.map(
                    ifaceName => this._addInterface(objectPath, ifaceName));
            }));
        } catch (error) {
            logError(error, `could not get remote objects for service ${this._serviceName} path ${this._managerPath}`);
        } finally {
            this._completeLoad();
        }
    }

    _onNameVanished() {
        let objectPaths = Object.keys(this._objects);
        for (let i = 0; i < objectPaths.length; i++) {
            let objectPath = objectPaths[i];
            let object = this._objects[objectPath];

            let interfaceNames = Object.keys(object);
            for (let j = 0; j < interfaceNames.length; j++) {
                let interfaceName = interfaceNames[j];

                if (object[interfaceName])
                    this._removeInterface(objectPath, interfaceName);
            }
        }
    }

    _registerInterfaces(interfaces) {
        for (let i = 0; i < interfaces.length; i++) {
            let info = Gio.DBusInterfaceInfo.new_for_xml(interfaces[i]);
            this._interfaceInfos[info.name] = info;
        }
    }

    getProxy(objectPath, interfaceName) {
        let object = this._objects[objectPath];

        if (!object)
            return null;

        return object[interfaceName];
    }

    getProxiesForInterface(interfaceName) {
        let proxyList = this._interfaces[interfaceName];

        if (!proxyList)
            return [];

        return proxyList;
    }

    getAllProxies() {
        let proxies = [];

        let objectPaths = Object.keys(this._objects);
        for (let i = 0; i < objectPaths.length; i++) {
            let object = this._objects[objectPaths];

            let interfaceNames = Object.keys(object);
            for (let j = 0; j < interfaceNames.length; j++) {
                let interfaceName = interfaceNames[j];
                if (object[interfaceName])
                    proxies.push(object(interfaceName));
            }
        }

        return proxies;
    }
};
