// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported loadRemoteSearchProviders */

const { GdkPixbuf, Gio, GLib, Shell, St } = imports.gi;

const FileUtils = imports.misc.fileUtils;

const KEY_FILE_GROUP = 'Shell Search Provider';

const SearchProviderIface = `
<node>
<interface name="org.gnome.Shell.SearchProvider">
<method name="GetInitialResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetSubsearchResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetResultMetas">
    <arg type="as" direction="in" />
    <arg type="aa{sv}" direction="out" />
</method>
<method name="ActivateResult">
    <arg type="s" direction="in" />
</method>
</interface>
</node>`;

const SearchProvider2Iface = `
<node>
<interface name="org.gnome.Shell.SearchProvider2">
<method name="GetInitialResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetSubsearchResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetResultMetas">
    <arg type="as" direction="in" />
    <arg type="aa{sv}" direction="out" />
</method>
<method name="ActivateResult">
    <arg type="s" direction="in" />
    <arg type="as" direction="in" />
    <arg type="u" direction="in" />
</method>
<method name="LaunchSearch">
    <arg type="as" direction="in" />
    <arg type="u" direction="in" />
</method>
</interface>
</node>`;

var SearchProviderProxyInfo = Gio.DBusInterfaceInfo.new_for_xml(SearchProviderIface);
var SearchProvider2ProxyInfo = Gio.DBusInterfaceInfo.new_for_xml(SearchProvider2Iface);

function loadRemoteSearchProviders(searchSettings, callback) {
    let objectPaths = {};
    let loadedProviders = [];

    function loadRemoteSearchProvider(file) {
        let keyfile = new GLib.KeyFile();
        let path = file.get_path();

        try {
            keyfile.load_from_file(path, 0);
        } catch (e) {
            return;
        }

        if (!keyfile.has_group(KEY_FILE_GROUP))
            return;

        let remoteProvider;
        try {
            let group = KEY_FILE_GROUP;
            let busName = keyfile.get_string(group, 'BusName');
            let objectPath = keyfile.get_string(group, 'ObjectPath');

            if (objectPaths[objectPath])
                return;

            let appInfo = null;
            try {
                let desktopId = keyfile.get_string(group, 'DesktopId');
                appInfo = Gio.DesktopAppInfo.new(desktopId);
                if (!appInfo.should_show())
                    return;
            } catch (e) {
                log(`Ignoring search provider ${path}: missing DesktopId`);
                return;
            }

            let autoStart = true;
            try {
                autoStart = keyfile.get_boolean(group, 'AutoStart');
            } catch (e) {
                // ignore error
            }

            let version = '1';
            try {
                version = keyfile.get_string(group, 'Version');
            } catch (e) {
                // ignore error
            }

            if (version >= 2)
                remoteProvider = new RemoteSearchProvider2(appInfo, busName, objectPath, autoStart);
            else
                remoteProvider = new RemoteSearchProvider(appInfo, busName, objectPath, autoStart);

            remoteProvider.defaultEnabled = true;
            try {
                remoteProvider.defaultEnabled = !keyfile.get_boolean(group, 'DefaultDisabled');
            } catch (e) {
                // ignore error
            }

            objectPaths[objectPath] = remoteProvider;
            loadedProviders.push(remoteProvider);
        } catch (e) {
            log('Failed to add search provider %s: %s'.format(path, e.toString()));
        }
    }

    if (searchSettings.get_boolean('disable-external')) {
        callback([]);
        return;
    }

    FileUtils.collectFromDatadirs('search-providers', false, loadRemoteSearchProvider);

    let sortOrder = searchSettings.get_strv('sort-order');

    // Special case gnome-control-center to be always active and always first
    sortOrder.unshift('gnome-control-center.desktop');

    loadedProviders = loadedProviders.filter(provider => {
        let appId = provider.appInfo.get_id();

        if (provider.defaultEnabled) {
            let disabled = searchSettings.get_strv('disabled');
            return !disabled.includes(appId);
        } else {
            let enabled = searchSettings.get_strv('enabled');
            return enabled.includes(appId);
        }
    });

    loadedProviders.sort((providerA, providerB) => {
        let idxA, idxB;
        let appIdA, appIdB;

        appIdA = providerA.appInfo.get_id();
        appIdB = providerB.appInfo.get_id();

        idxA = sortOrder.indexOf(appIdA);
        idxB = sortOrder.indexOf(appIdB);

        // if no provider is found in the order, use alphabetical order
        if ((idxA == -1) && (idxB == -1)) {
            let nameA = providerA.appInfo.get_name();
            let nameB = providerB.appInfo.get_name();

            return GLib.utf8_collate(nameA, nameB);
        }

        // if providerA isn't found, it's sorted after providerB
        if (idxA == -1)
            return 1;

        // if providerB isn't found, it's sorted after providerA
        if (idxB == -1)
            return -1;

        // finally, if both providers are found, return their order in the list
        return idxA - idxB;
    });

    callback(loadedProviders);
}

var RemoteSearchProvider = class {
    constructor(appInfo, dbusName, dbusPath, autoStart, proxyInfo) {
        if (!proxyInfo)
            proxyInfo = SearchProviderProxyInfo;

        let gFlags = Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES;
        if (autoStart)
            gFlags |= Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION;
        else
            gFlags |= Gio.DBusProxyFlags.DO_NOT_AUTO_START;

        this.proxy = new Gio.DBusProxy({ g_bus_type: Gio.BusType.SESSION,
                                         g_name: dbusName,
                                         g_object_path: dbusPath,
                                         g_interface_info: proxyInfo,
                                         g_interface_name: proxyInfo.name,
                                         gFlags });
        this.proxy.init_async(GLib.PRIORITY_DEFAULT, null);

        this.appInfo = appInfo;
        this.id = appInfo.get_id();
        this.isRemoteProvider = true;
        this.canLaunchSearch = false;
    }

    createIcon(size, meta) {
        let gicon = null;
        let icon = null;

        if (meta['icon']) {
            gicon = Gio.icon_deserialize(meta['icon']);
        } else if (meta['gicon']) {
            gicon = Gio.icon_new_for_string(meta['gicon']);
        } else if (meta['icon-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels_, data] = meta['icon-data'];
            gicon = Shell.util_create_pixbuf_from_data(data, GdkPixbuf.Colorspace.RGB, hasAlpha,
                                                       bitsPerSample, width, height, rowStride);
        }

        if (gicon)
            icon = new St.Icon({ gicon, icon_size: size });
        return icon;
    }

    filterResults(results, maxNumber) {
        if (results.length <= maxNumber)
            return results;

        let regularResults = results.filter(r => !r.startsWith('special:'));
        let specialResults = results.filter(r => r.startsWith('special:'));

        return regularResults.slice(0, maxNumber).concat(specialResults.slice(0, maxNumber));
    }

    _getResultsFinished(results, error, callback) {
        if (error) {
            if (error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;

            log('Received error from D-Bus search provider %s: %s'.format(this.id, String(error)));
            callback([]);
            return;
        }

        callback(results[0]);
    }

    getInitialResultSet(terms, callback, cancellable) {
        this.proxy.GetInitialResultSetRemote(terms,
                                             (results, error) => {
                                                 this._getResultsFinished(results, error, callback);
                                             },
                                             cancellable);
    }

    getSubsearchResultSet(previousResults, newTerms, callback, cancellable) {
        this.proxy.GetSubsearchResultSetRemote(previousResults, newTerms,
                                               (results, error) => {
                                                   this._getResultsFinished(results, error, callback);
                                               },
                                               cancellable);
    }

    _getResultMetasFinished(results, error, callback) {
        if (error) {
            if (!error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                log('Received error from D-Bus search provider %s during GetResultMetas: %s'.format(this.id, String(error)));
            callback([]);
            return;
        }
        let metas = results[0];
        let resultMetas = [];
        for (let i = 0; i < metas.length; i++) {
            for (let prop in metas[i]) {
                // we can use the serialized icon variant directly
                if (prop != 'icon')
                    metas[i][prop] = metas[i][prop].deep_unpack();
            }

            resultMetas.push({ id: metas[i]['id'],
                               name: metas[i]['name'],
                               description: metas[i]['description'],
                               createIcon: size => {
                                   return this.createIcon(size, metas[i]);
                               },
                               clipboardText: metas[i]['clipboardText'] });
        }
        callback(resultMetas);
    }

    getResultMetas(ids, callback, cancellable) {
        this.proxy.GetResultMetasRemote(ids,
                                        (results, error) => {
                                            this._getResultMetasFinished(results, error, callback);
                                        },
                                        cancellable);
    }

    activateResult(id) {
        this.proxy.ActivateResultRemote(id);
    }

    launchSearch(_terms) {
        // the provider is not compatible with the new version of the interface, launch
        // the app itself but warn so we can catch the error in logs
        log(`Search provider ${this.appInfo.get_id()} does not implement LaunchSearch`);
        this.appInfo.launch([], global.create_app_launch_context(0, -1));
    }
};

var RemoteSearchProvider2 = class extends RemoteSearchProvider {
    constructor(appInfo, dbusName, dbusPath, autoStart) {
        super(appInfo, dbusName, dbusPath, autoStart, SearchProvider2ProxyInfo);

        this.canLaunchSearch = true;
    }

    activateResult(id, terms) {
        this.proxy.ActivateResultRemote(id, terms, global.get_current_time());
    }

    launchSearch(terms) {
        this.proxy.LaunchSearchRemote(terms, global.get_current_time());
    }
};
