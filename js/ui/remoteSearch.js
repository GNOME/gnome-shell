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

var SearchProviderProxy = new Gio.DBusProxyClass({
    Name: 'SearchProviderProxy',
    Interface: SearchProviderIface,

    _init: function(params) {
        params.g_bus_type = Gio.BusType.SESSION;
        this.parent(params);
    }
});

function loadRemoteSearchProviders(addProviderCallback) {
    let dataDirs = GLib.get_system_data_dirs();
    let loadedProviders = {};
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'search-providers']);
        let dir = Gio.file_new_for_path(path);
        if (!dir.query_exists(null))
            continue;
        loadRemoteSearchProvidersFromDir(dir, loadedProviders, addProviderCallback);
    }
};

function loadRemoteSearchProvidersFromDir(dir, loadedProviders, addProviderCallback) {
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

            let remoteProvider, title;
            try {
                let group = KEY_FILE_GROUP;
                let busName = keyfile.get_string(group, 'BusName');
                let objectPath = keyfile.get_string(group, 'ObjectPath');

                if (loadedProviders[objectPath])
                    continue;

                let appInfo = null;
                try {
                    let desktopId = keyfile.get_string(group, 'DesktopId');
                    appInfo = Gio.DesktopAppInfo.new(desktopId);
                } catch (e) {
                }

                let icon;
                if (appInfo) {
                    icon = appInfo.get_icon();
                    title = appInfo.get_name();
                } else {
                    let iconName = keyfile.get_string(group, 'Icon');
                    icon = new Gio.ThemedIcon({ name: iconName });
                    title = keyfile.get_locale_string(group, 'Title', null);
                }

                remoteProvider = new RemoteSearchProvider(title,
                                                          icon,
                                                          busName,
                                                          objectPath);
                remoteProvider.initAsync(null, function(obj, result) {
                    try {
                        remoteProvider.initFinish(result);
                    } catch(e) {
                        log('Failed to add search provider "%s": %s'.format(title, e.toString()));
                        return;
                    }

                    addProviderCallback(remoteProvider);
                });

                loadedProviders[objectPath] = remoteProvider;
            } catch(e) {
                log('Failed to add search provider "%s": %s'.format(title, e.toString()));
                continue;
            }
        }
    }));

};

const RemoteSearchProvider = new Lang.Class({
    Name: 'RemoteSearchProvider',
    Extends: Search.SearchProvider,

    _init: function(title, icon, dbusName, dbusPath) {
        this.parent(title.toUpperCase());

        this._proxy = new SearchProviderProxy({ g_name: dbusName,
                                                g_object_path: dbusPath });
        this._cancellable = new Gio.Cancellable();
    },

    initAsync: function(cancellable, asyncCallback) {
        // Can't pass "this" as source object, because RemoteSearchProvider
        // is not a GObject.Object (and in gjs you can't inherit from a JS
        // type that in turn inherits from GObject)

        let simpleResult = Gio.SimpleAsyncResult.new(null, asyncCallback, null);
        simpleResult.set_check_cancellable(cancellable);

        this._proxy.init_async(GLib.PRIORITY_DEFAULT, cancellable, Lang.bind(this, function(proxy, result) {
            try {
                proxy.init_finish(result);

                simpleResult.set_op_res_gboolean(true);
            } catch(e if e instanceof GLib.Error) {
                simpleResult.set_from_error(e);
            }

            simpleResult.complete();
        }));
    },

    initFinish: function(simpleResult) {
        if (!simpleResult.propagate_error())
            return simpleResult.get_op_res_gboolean();

        return false;
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

    _getResultsFinished: function(proxy, result) {
        try {
            // We rely on a small implementation detail of the
            // GDBus bindings here: all *Finish are equal

            let [results] = proxy.GetInitialResultSetFinish(result);
            this.searchSystem.pushResults(this, results);
        } catch(e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                log('Received error from search provider %s: %s'.format(this.title, String(e)));
        }
    },

    getInitialResultSet: function(terms) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this._proxy.GetInitialResultSetRemote(terms,
                                                  this._cancellable,
                                                  Lang.bind(this, this._getResultsFinished));
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
                                                    this._cancellable,
                                                    Lang.bind(this, this._getResultsFinished))
        } catch(e) {
            log('Error calling GetSubsearchResultSet for provider %s: %s'.format(this.title, e.toString()));
            this.searchSystem.pushResults(this, []);
        }
    },

    _getResultMetasFinished: function(proxy, result, callback) {
        try {
            let [metas] = results.GetResultMetasFinish(result);
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
        } catch(e) {
            callback([]);
        }
    },

    getResultMetas: function(ids, callback) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this._proxy.GetResultMetasRemote(ids,
                                             this._cancellable,
                                             Lang.bind(this, this._getResultMetasFinished, callback));
        } catch(e) {
            log('Error calling GetResultMetas for provider %s: %s'.format(this.title, e.toString()));
            callback([]);
        }
    },

    activateResult: function(id) {
        this._proxy.ActivateResultRemote(id, null, null);
    }
});


