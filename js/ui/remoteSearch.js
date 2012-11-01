// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const St = imports.gi.St;

const FileUtils = imports.misc.fileUtils;
const Search = imports.ui.search;

const KEY_FILE_GROUP = 'Shell Search Provider';

const SearchProviderIface = <interface name="org.gnome.Shell.SearchProvider">
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
</interface>;

var SearchProviderProxy = Gio.DBusProxy.makeProxyWrapper(SearchProviderIface);

function loadRemoteSearchProviders(addProviderCallback) {
    let loadState = { loadedProviders: [],
                      objectPaths: {},
                      numLoading: 0,
                      addProviderCallback: addProviderCallback };

    let dataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'search-providers']);
        let dir = Gio.file_new_for_path(path);

        dir.query_info_async('standard:type', Gio.FileQueryInfoFlags.NONE,
            GLib.PRIORITY_DEFAULT, null,
                function(object, res) {
                    let exists = false;
                    try {
                        object.query_info_finish(res);
                        exists = true;
                    } catch (e) {
                    }

                    if (!exists)
                        return;

                    loadState.numLoading++;
                    loadRemoteSearchProvidersFromDir(dir, loadState);
                });
    }
};

function loadRemoteSearchProvidersFromDir(dir, loadState) {
    let dirPath = dir.get_path();
    FileUtils.listDirAsync(dir, Lang.bind(this, function(files) {
        for (let i = 0; i < files.length; i++) {
            let keyfile = new GLib.KeyFile();
            let path = GLib.build_filenamev([dirPath, files[i].get_name()]);

            try {
                keyfile.load_from_file(path, 0);
            } catch(e) {
                continue;
            }

            if (!keyfile.has_group(KEY_FILE_GROUP))
                continue;

            let remoteProvider;
            try {
                let group = KEY_FILE_GROUP;
                let busName = keyfile.get_string(group, 'BusName');
                let objectPath = keyfile.get_string(group, 'ObjectPath');

                if (loadState.objectPaths[objectPath])
                    continue;

                let appInfo = null;
                try {
                    let desktopId = keyfile.get_string(group, 'DesktopId');
                    appInfo = Gio.DesktopAppInfo.new(desktopId);
                } catch (e) {
                    log('Ignoring search provider ' + path + ': missing DesktopId');
                    continue;
                }

                remoteProvider = new RemoteSearchProvider(appInfo,
                                                          busName,
                                                          objectPath);
                loadState.objectPaths[objectPath] = remoteProvider;
                loadState.loadedProviders.push(remoteProvider);
            } catch(e) {
                log('Failed to add search provider %s: %s'.format(path, e.toString()));
                continue;
            }
        }

        remoteProvidersDirLoaded(loadState);
    }));

};

function remoteProvidersDirLoaded(loadState) {
    loadState.numLoading--;
    if (loadState.numLoading > 0)
        return;

    let searchSettings = new Gio.Settings({ schema: Search.SEARCH_PROVIDERS_SCHEMA });
    let sortOrder = searchSettings.get_strv('sort-order');
    let numSorted = sortOrder.length;

    loadState.loadedProviders.sort(
        function(providerA, providerB) {
            let idxA, idxB;
            let appIdA, appIdB;

            appIdA = providerA.appInfo.get_id();
            appIdB = providerB.appInfo.get_id();

            idxA = sortOrder.indexOf(appIdA);
            idxB = sortOrder.indexOf(appIdB);

            // if no provider is found in the order, use alphabetical order
            if ((idxA == -1) && (idxB == -1))
                return GLib.utf8_collate(providerA.title, providerB.title);

            if (numSorted > 1) {
                // if providerA is the last, it goes after everything
                if ((idxA + 1) == numSorted)
                    return 1;
                // if providerB is the last, it goes after everything
                else if ((idxB + 1) == numSorted)
                    return -1;
            }

            // if providerA isn't found, it's sorted after providerB
            if (idxA == -1)
                return 1;

            // if providerB isn't found, it's sorted after providerA
            if (idxB == -1)
                return -1;

            // finally, if both providers are found, return their order in the list
            return (idxA - idxB);
        });

    loadState.loadedProviders.forEach(
        function(provider) {
            loadState.addProviderCallback(provider);
        });
}

const RemoteSearchProvider = new Lang.Class({
    Name: 'RemoteSearchProvider',
    Extends: Search.SearchProvider,

    _init: function(appInfo, dbusName, dbusPath) {
        this._proxy = new SearchProviderProxy(Gio.DBus.session,
            dbusName, dbusPath, Lang.bind(this, this._onProxyConstructed));

        this.parent(appInfo.get_name().toUpperCase(), appInfo, true);
        this._cancellable = new Gio.Cancellable();
    },

    _onProxyConstructed: function(proxy) {
        // Do nothing
    },

    createIcon: function(size, meta) {
        if (meta['gicon']) {
            return new St.Icon({ gicon: Gio.icon_new_for_string(meta['gicon']),
                                 icon_size: size });
        } else if (meta['icon-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = meta['icon-data'];
            let textureCache = St.TextureCache.get_default();
            return textureCache.load_from_raw(data, hasAlpha,
                                              width, height, rowStride, size);
        }

        // Ugh, but we want to fall back to something ...
        return new St.Icon({ icon_name: 'text-x-generic',
                             icon_size: size });
    },

    _getResultsFinished: function(results, error) {
        if (error)
            return;
        this.searchSystem.pushResults(this, results[0]);
    },

    getInitialResultSet: function(terms) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this._proxy.GetInitialResultSetRemote(terms,
                                                  Lang.bind(this, this._getResultsFinished),
                                                  this._cancellable);
        } catch(e) {
            log('Error calling GetInitialResultSet for provider %s: %s'.format( this.title, e.toString()));
            this.searchSystem.pushResults(this, []);
        }
    },

    getSubsearchResultSet: function(previousResults, newTerms) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this._proxy.GetSubsearchResultSetRemote(previousResults, newTerms,
                                                    Lang.bind(this, this._getResultsFinished),
                                                    this._cancellable);
        } catch(e) {
            log('Error calling GetSubsearchResultSet for provider %s: %s'.format(this.title, e.toString()));
            this.searchSystem.pushResults(this, []);
        }
    },

    _getResultMetasFinished: function(results, error, callback) {
        if (error) {
            callback([]);
            return;
        }
        let metas = results[0];
        let resultMetas = [];
        for (let i = 0; i < metas.length; i++) {
            for (let prop in metas[i])
                metas[i][prop] = metas[i][prop].deep_unpack();
            resultMetas.push({ id: metas[i]['id'],
                               name: metas[i]['name'],
                               createIcon: Lang.bind(this,
                                                     this.createIcon, metas[i]) });
        }
        callback(resultMetas);
    },

    getResultMetas: function(ids, callback) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this._proxy.GetResultMetasRemote(ids,
                                             Lang.bind(this, this._getResultMetasFinished, callback),
                                             this._cancellable);
        } catch(e) {
            log('Error calling GetResultMetas for provider %s: %s'.format(this.title, e.toString()));
            callback([]);
        }
    },

    activateResult: function(id) {
        this._proxy.ActivateResultRemote(id);
    }
});


