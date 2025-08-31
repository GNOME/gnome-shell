import GdkPixbuf from 'gi://GdkPixbuf';
import Gio from 'gi://Gio';
import GioUnix from 'gi://GioUnix';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as FileUtils from '../misc/fileUtils.js';

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

const SearchProviderProxyInfo = Gio.DBusInterfaceInfo.new_for_xml(SearchProviderIface);
const SearchProvider2ProxyInfo = Gio.DBusInterfaceInfo.new_for_xml(SearchProvider2Iface);

/**
 * loadRemoteSearchProviders:
 *
 * @param {Gio.Settings} searchSettings - search settings
 * @returns {RemoteSearchProvider[]} - the list of remote providers
 */
export function loadRemoteSearchProviders(searchSettings) {
    let objectPaths = {};
    let loadedProviders = [];

    function loadRemoteSearchProvider(file) {
        let keyfile = new GLib.KeyFile();
        let path = file.get_path();

        try {
            keyfile.load_from_file(path, 0);
        } catch {
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
                appInfo = GioUnix.DesktopAppInfo.new(desktopId);
                if (!appInfo.should_show())
                    return;
            } catch {
                log(`Ignoring search provider ${path}: missing DesktopId`);
                return;
            }

            let autoStart = true;
            try {
                autoStart = keyfile.get_boolean(group, 'AutoStart');
            } catch {
                // ignore error
            }

            let version = '1';
            try {
                version = keyfile.get_string(group, 'Version');
            } catch {
                // ignore error
            }

            if (version >= 2)
                remoteProvider = new RemoteSearchProvider2(appInfo, busName, objectPath, autoStart);
            else
                remoteProvider = new RemoteSearchProvider(appInfo, busName, objectPath, autoStart);

            remoteProvider.defaultEnabled = true;
            try {
                remoteProvider.defaultEnabled = !keyfile.get_boolean(group, 'DefaultDisabled');
            } catch {
                // ignore error
            }

            objectPaths[objectPath] = remoteProvider;
            loadedProviders.push(remoteProvider);
        } catch (e) {
            log(`Failed to add search provider ${path}: ${e}`);
        }
    }

    if (searchSettings.get_boolean('disable-external'))
        return [];

    for (const {file} of FileUtils.collectFromDatadirs('search-providers', false))
        loadRemoteSearchProvider(file);

    let sortOrder = searchSettings.get_strv('sort-order');

    const disabled = searchSettings.get_strv('disabled');
    const enabled = searchSettings.get_strv('enabled');

    loadedProviders = loadedProviders.filter(provider => {
        let appId = provider.appInfo.get_id();

        if (provider.defaultEnabled)
            return !disabled.includes(appId);
        else
            return enabled.includes(appId);
    });

    loadedProviders.sort((providerA, providerB) => {
        let idxA, idxB;
        let appIdA, appIdB;

        appIdA = providerA.appInfo.get_id();
        appIdB = providerB.appInfo.get_id();

        idxA = sortOrder.indexOf(appIdA);
        idxB = sortOrder.indexOf(appIdB);

        // if no provider is found in the order, use alphabetical order
        if ((idxA === -1) && (idxB === -1)) {
            let nameA = providerA.appInfo.get_name();
            let nameB = providerB.appInfo.get_name();

            return GLib.utf8_collate(nameA, nameB);
        }

        // if providerA isn't found, it's sorted after providerB
        if (idxA === -1)
            return 1;

        // if providerB isn't found, it's sorted after providerA
        if (idxB === -1)
            return -1;

        // finally, if both providers are found, return their order in the list
        return idxA - idxB;
    });

    return loadedProviders;
}

class RemoteSearchProvider {
    constructor(appInfo, dbusName, dbusPath, autoStart, proxyInfo) {
        if (!proxyInfo)
            proxyInfo = SearchProviderProxyInfo;

        let gFlags = Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES;
        if (autoStart)
            gFlags |= Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION;
        else
            gFlags |= Gio.DBusProxyFlags.DO_NOT_AUTO_START;

        this.proxy = new Gio.DBusProxy({
            g_bus_type: Gio.BusType.SESSION,
            g_name: dbusName,
            g_object_path: dbusPath,
            g_interface_info: proxyInfo,
            g_interface_name: proxyInfo.name,
            gFlags,
        });
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
            const [
                width, height, rowStride, hasAlpha,
                bitsPerSample, nChannels_, data,
            ] = meta['icon-data'];
            gicon = Shell.util_create_pixbuf_from_data(data,
                GdkPixbuf.Colorspace.RGB,
                hasAlpha,
                bitsPerSample,
                width,
                height,
                rowStride);
        }

        if (gicon)
            icon = new St.Icon({gicon, icon_size: size});
        return icon;
    }

    filterResults(results, maxNumber) {
        if (results.length <= maxNumber)
            return results;

        let regularResults = results.filter(r => !r.startsWith('special:'));
        let specialResults = results.filter(r => r.startsWith('special:'));

        return regularResults.slice(0, maxNumber).concat(specialResults.slice(0, maxNumber));
    }

    async getInitialResultSet(terms, cancellable) {
        try {
            const [results] = await this.proxy.GetInitialResultSetAsync(terms, cancellable);
            return results;
        } catch (error) {
            if (!error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                log(`Received error from D-Bus search provider ${this.id}: ${error}`);
            return [];
        }
    }

    async getSubsearchResultSet(previousResults, newTerms, cancellable) {
        try {
            const [results] = await this.proxy.GetSubsearchResultSetAsync(previousResults, newTerms, cancellable);
            return results;
        } catch (error) {
            if (!error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                log(`Received error from D-Bus search provider ${this.id}: ${error}`);
            return [];
        }
    }

    async getResultMetas(ids, cancellable) {
        let metas;
        try {
            [metas] = await this.proxy.GetResultMetasAsync(ids, cancellable);
        } catch (error) {
            if (!error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                log(`Received error from D-Bus search provider ${this.id} during GetResultMetas: ${error}`);
            return [];
        }

        let resultMetas = [];
        for (let i = 0; i < metas.length; i++) {
            for (let prop in metas[i]) {
                // we can use the serialized icon variant directly
                if (prop !== 'icon')
                    metas[i][prop] = metas[i][prop].deepUnpack();
            }

            resultMetas.push({
                id: metas[i]['id'],
                name: metas[i]['name'],
                description: metas[i]['description'],
                createIcon: size => this.createIcon(size, metas[i]),
                clipboardText: metas[i]['clipboardText'],
            });
        }
        return resultMetas;
    }

    activateResult(id) {
        this.proxy.ActivateResultAsync(id).catch(logError);
    }

    launchSearch(_terms) {
        // the provider is not compatible with the new version of the interface, launch
        // the app itself but warn so we can catch the error in logs
        log(`Search provider ${this.appInfo.get_id()} does not implement LaunchSearch`);
        this.appInfo.launch([], global.create_app_launch_context(0, -1));
    }
}

class RemoteSearchProvider2 extends RemoteSearchProvider {
    constructor(appInfo, dbusName, dbusPath, autoStart) {
        super(appInfo, dbusName, dbusPath, autoStart, SearchProvider2ProxyInfo);

        this.canLaunchSearch = true;
    }

    activateResult(id, terms) {
        this.proxy.ActivateResultAsync(
            id, terms, global.get_current_time()).catch(logError);
    }

    launchSearch(terms) {
        this.proxy.LaunchSearchAsync(
            terms, global.get_current_time()).catch(logError);
    }
}
