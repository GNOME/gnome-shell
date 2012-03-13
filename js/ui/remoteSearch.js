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
    let dataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'search-providers']);
        let dir = Gio.file_new_for_path(path);
        if (!dir.query_exists(null))
            continue;
        loadRemoteSearchProvidersFromDir(dir, addProviderCallback);
    }
};

function loadRemoteSearchProvidersFromDir(dir, addProviderCallback) {
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
                let icon = keyfile.get_string(group, 'Icon');
                let busName = keyfile.get_string(group, 'BusName');
                let objectPath = keyfile.get_string(group, 'ObjectPath');
                title = keyfile.get_locale_string(group, 'Title', null);

                remoteProvider = new RemoteSearchProvider(title,
                                                          icon,
                                                          busName,
                                                          objectPath);
            } catch(e) {
                log('Failed to add search provider "%s": %s'.format(title, e.toString()));
                continue;
            }

            addProviderCallback(remoteProvider);
        }
    }));

};

const RemoteSearchProvider = new Lang.Class({
    Name: 'RemoteSearchProvider',
    Extends: Search.SearchProvider,

    _init: function(title, icon, dbusName, dbusPath) {
        this._proxy = new SearchProviderProxy(Gio.DBus.session,
                                              dbusName, dbusPath);

        this.parent(title.toUpperCase());
        this.async = true;
        this._cancellable = new Gio.Cancellable();
    },

    createIcon: function(size, meta) {
        if (meta['gicon']) {
            return new St.Icon({ gicon: Gio.icon_new_for_string(meta['gicon']),
                                 icon_size: size,
                                 icon_type: St.IconType.FULLCOLOR });
        } else if (meta['icon-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = meta['icon-data'];
            let textureCache = St.TextureCache.get_default();
            return textureCache.load_from_raw(data, hasAlpha,
                                              width, height, rowStride, size);
        }

        // Ugh, but we want to fall back to something ...
        return new St.Icon({ icon_name: 'text-x-generic',
                             icon_size: size,
                             icon_type: St.IconType.FULLCOLOR });
    },

    _getResultsFinished: function(results, error) {
        if (error)
            return;
        this.searchSystem.pushResults(this, results[0]);
    },

    getInitialResultSetAsync: function(terms) {
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

    getSubsearchResultSetAsync: function(previousResults, newTerms) {
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

    getResultMetasAsync: function(ids, callback) {
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


