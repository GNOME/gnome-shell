/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Search = imports.ui.search;

const NAUTILUS_PREFS_DIR = '/apps/nautilus/preferences';
const DESKTOP_IS_HOME_KEY = NAUTILUS_PREFS_DIR + '/desktop_is_home_dir';

const PLACES_ICON_SIZE = 16;

/**
 * Represents a place object, which is most normally a bookmark entry,
 * a mount/volume, or a special place like the Home Folder, Computer, and Network.
 *
 * @name: String title
 * @iconFactory: A JavaScript callback which will create an icon texture given a size parameter
 * @launch: A JavaScript callback to launch the entry
 */
function PlaceInfo(id, name, iconFactory, launch) {
    this._init(id, name, iconFactory, launch);
}

PlaceInfo.prototype = {
    _init: function(id, name, iconFactory, launch) {
        this.id = id;
        this.name = name;
        this._lowerName = name.toLowerCase();
        this.iconFactory = iconFactory;
        this.launch = launch;
    },

    matchTerms: function(terms) {
        let mtype = Search.MatchType.NONE;
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            let idx = this._lowerName.indexOf(term);
            if (idx == 0)
                return Search.MatchType.PREFIX;
            else if (idx > 0)
                mtype = Search.MatchType.SUBSTRING;
        }
        return mtype;
    },

    isRemovable: function() {
        return false;
    }
}

function PlaceDeviceInfo(mount) {
    this._init(mount);
}

PlaceDeviceInfo.prototype = {
    __proto__: PlaceInfo.prototype,

    _init: function(mount) {
        this._mount = mount;
        this.name = mount.get_name();
        this._lowerName = this.name.toLowerCase();
        this.id = "mount:" + mount.get_root().get_uri();
    },

    iconFactory: function(size) {
        let icon = this._mount.get_icon();
        return Shell.TextureCache.get_default().load_gicon(icon, size);
    },

    launch: function() {
        Gio.app_info_launch_default_for_uri(this._mount.get_root().get_uri(),
                                            global.create_app_launch_context());
    },

    isRemovable: function() {
        return this._mount.can_unmount();
    },

    remove: function() {
        if (!this.isRemovable())
            return;

        this._mount.unmount(0, null, Lang.bind(this, this._removeFinish), null);
    },

    _removeFinish: function(o, res, data) {
        this._mount.unmount_finish(res);
    }
}


function PlacesManager() {
    this._init();
}

PlacesManager.prototype = {
    _init: function() {
        let gconf = Shell.GConf.get_default();
        gconf.watch_directory(NAUTILUS_PREFS_DIR);

        this._defaultPlaces = [];
        this._mounts = [];
        this._bookmarks = [];
        this._isDesktopHome = false;

        let homeFile = Gio.file_new_for_path (GLib.get_home_dir());
        let homeUri = homeFile.get_uri();
        let homeLabel = Shell.util_get_label_for_uri (homeUri);
        let homeIcon = Shell.util_get_icon_for_uri (homeUri);
        this._home = new PlaceInfo('special:home', homeLabel,
            function(size) {
                return Shell.TextureCache.get_default().load_gicon(homeIcon, size);
            },
            function() {
                Gio.app_info_launch_default_for_uri(homeUri, global.create_app_launch_context());
            });

        let desktopPath = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DESKTOP);
        let desktopFile = Gio.file_new_for_path (desktopPath);
        let desktopUri = desktopFile.get_uri();
        let desktopLabel = Shell.util_get_label_for_uri (desktopUri);
        let desktopIcon = Shell.util_get_icon_for_uri (desktopUri);
        this._desktopMenu = new PlaceInfo('special:desktop', desktopLabel,
            function(size) {
                return Shell.TextureCache.get_default().load_gicon(desktopIcon, size);
            },
            function() {
                Gio.app_info_launch_default_for_uri(desktopUri, global.create_app_launch_context());
            });

        this._connect = new PlaceInfo('special:connect', _("Connect to..."),
            function (size) {
                return Shell.TextureCache.get_default().load_icon_name("applications-internet", size);
            },
            function () {
                new Shell.Process({ args: ['nautilus-connect-server'] }).run();
            });

        let networkApp = null;
        try {
            networkApp = Shell.AppSystem.get_default().load_from_desktop_file('gnome-network-scheme.desktop');
        } catch(e) {
            try {
                networkApp = Shell.AppSystem.get_default().load_from_desktop_file('network-scheme.desktop');
            } catch(e) {
                log("Cannot create \"Network\" item, .desktop file not found or corrupt.");
            }
        }

        if (networkApp != null) {
            this._network = new PlaceInfo('special:network', networkApp.get_name(),
                function(size) {
                    return networkApp.create_icon_texture(size);
                },
                function () {
                    networkApp.launch();
                });
        }

        this._defaultPlaces.push(this._home);

        if (!this._isDesktopHome)
            this._defaultPlaces.push(this._desktopMenu);

        if (this._network)
            this._defaultPlaces.push(this._network);

        this._defaultPlaces.push(this._connect);

        /*
        * Show devices, code more or less ported from nautilus-places-sidebar.c
        */
        this._volumeMonitor = Gio.VolumeMonitor.get();
        this._volumeMonitor.connect('volume-added', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('volume-removed',Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('volume-changed', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('mount-added', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('mount-removed', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('mount-changed', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('drive-connected', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('drive-disconnected', Lang.bind(this, this._updateDevices));
        this._volumeMonitor.connect('drive-changed', Lang.bind(this, this._updateDevices));
        this._updateDevices();

        this._bookmarksPath = GLib.build_filenamev([GLib.get_home_dir(), ".gtk-bookmarks"]);
        this._bookmarksFile = Gio.file_new_for_path(this._bookmarksPath);
        let monitor = this._bookmarksFile.monitor_file(Gio.FileMonitorFlags.NONE, null);
        this._bookmarkTimeoutId = 0;
        monitor.connect('changed', Lang.bind(this, function () {
            if (this._bookmarkTimeoutId > 0)
                return;
            /* Defensive event compression */
            this._bookmarkTimeoutId = Mainloop.timeout_add(100, Lang.bind(this, function () {
                this._bookmarkTimeoutId = 0;
                this._reloadBookmarks();
                return false;
            }));
        }));

        this._reloadBookmarks();
        this._updateDesktopMenuVisibility();

        gconf.connect('changed::' + DESKTOP_IS_HOME_KEY, Lang.bind(this, this._updateDesktopMenuVisibility));

    },

    _updateDevices: function() {
        this._mounts = [];

        /* first go through all connected drives */
        let drives = this._volumeMonitor.get_connected_drives();
        for (let i = 0; i < drives.length; i++) {
            let volumes = drives[i].get_volumes();
            for(let j = 0; j < volumes.length; j++) {
                let mount = volumes[j].get_mount();
                if(mount != null) {
                    this._addMount(mount);
                }
            }
        }

        /* add all volumes that is not associated with a drive */
        let volumes = this._volumeMonitor.get_volumes();
        for(let i = 0; i < volumes.length; i++) {
            if(volumes[i].get_drive() != null)
                continue;

            let mount = volumes[i].get_mount();
            if(mount != null) {
                this._addMount(mount);
            }
        }

        /* add mounts that have no volume (/etc/mtab mounts, ftp, sftp,...) */
        let mounts = this._volumeMonitor.get_mounts();
        for(let i = 0; i < mounts.length; i++) {
            if(mounts[i].is_shadowed())
                continue;

            if(mounts[i].get_volume())
                continue;

            this._addMount(mounts[i]);
        }

        /* We emit two signals, one for a generic 'all places' update
         * and the other for one specific to mounts. We do this because
         * clients like PlaceDisplay may only care about places in general
         * being updated while clients like DashPlaceDisplay care which
         * specific type of place got updated.
         */
        this.emit('mounts-updated');
        this.emit('places-updated');

    },

    _reloadBookmarks: function() {

        this._bookmarks = [];

        if (!GLib.file_test(this._bookmarksPath, GLib.FileTest.EXISTS))
            return;

        let [success, bookmarksContent, len] = GLib.file_get_contents(this._bookmarksPath);

        if (!success)
            return;

        let bookmarks = bookmarksContent.split('\n');

        let bookmarksToLabel = {};
        let bookmarksOrder = [];
        for (let i = 0; i < bookmarks.length; i++) {
            let bookmarkLine = bookmarks[i];
            let components = bookmarkLine.split(' ');
            let bookmark = components[0];
            if (bookmark in bookmarksToLabel)
                continue;
            let label = null;
            if (components.length > 1)
                label = components.slice(1).join(' ');
            bookmarksToLabel[bookmark] = label;
            bookmarksOrder.push(bookmark);
        }

        for (let i = 0; i < bookmarksOrder.length; i++) {
            let bookmark = bookmarksOrder[i];
            let label = bookmarksToLabel[bookmark];
            let file = Gio.file_new_for_uri(bookmark);
            if (!file.query_exists(null))
                continue;
            if (label == null)
                label = Shell.util_get_label_for_uri(bookmark);
            if (label == null)
                continue;
            let icon = Shell.util_get_icon_for_uri(bookmark);

            let item = new PlaceInfo('bookmark:' + bookmark, label,
                function(size) {
                    return Shell.TextureCache.get_default().load_gicon(icon, size);
                },
                function() {
                    Gio.app_info_launch_default_for_uri(bookmark, global.create_app_launch_context());
                });
            this._bookmarks.push(item);
        }

        /* See comment in _updateDevices for explanation why there are two signals. */
        this.emit('bookmarks-updated');
        this.emit('places-updated');
    },

    _updateDesktopMenuVisibility: function() {
        let gconf = Shell.GConf.get_default();
        this._isDesktopHome = gconf.get_boolean(DESKTOP_IS_HOME_KEY);

        /* See comment in _updateDevices for explanation why there are two signals. */
        this.emit('defaults-updated');
        this.emit('places-updated');
    },

    _addMount: function(mount) {
        let devItem = new PlaceDeviceInfo(mount);
        this._mounts.push(devItem);
    },

    getAllPlaces: function () {
        return this.getDefaultPlaces().concat(this.getBookmarks(), this.getMounts());
    },

    getDefaultPlaces: function () {
        return this._defaultPlaces;
    },

    getBookmarks: function () {
        return this._bookmarks;
    },

    getMounts: function () {
        return this._mounts;
    },

    _lookupById: function(sourceArray, id) {
        for (let i = 0; i < sourceArray.length; i++) {
            let place = sourceArray[i];
            if (place.id == id)
                return place;
        }
        return null;
    },

    lookupPlaceById: function(id) {
        let colonIdx = id.indexOf(':');
        let type = id.substring(0, colonIdx);
        let sourceArray = null;
        if (type == 'special')
            sourceArray = this._defaultPlaces;
        else if (type == 'mount')
            sourceArray = this._mounts;
        else if (type == 'bookmark')
            sourceArray = this._bookmarks;
        return this._lookupById(sourceArray, id);
    }
};

Signals.addSignalMethods(PlacesManager.prototype);

/**
 * An entry in the places menu.
 * @info The corresponding PlaceInfo to populate this entry.
 */
function DashPlaceDisplayItem(info) {
    this._init(info);
}

DashPlaceDisplayItem.prototype = {
    _init: function(info) {
        this.name = info.name;
        this._info = info;
        this._icon = info.iconFactory(PLACES_ICON_SIZE);
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: 4 });
        let text = new St.Button({ style_class: 'places-item',
                                   label: info.name,
                                   x_align: St.Align.START });
        text.connect('clicked', Lang.bind(this, this._onClicked));
        let iconBox = new St.Bin({ child: this._icon, reactive: true });
        iconBox.connect('button-release-event',
                        Lang.bind(this, this._onClicked));
        this.actor.append(iconBox, Big.BoxPackFlags.NONE);
        this.actor.append(text, Big.BoxPackFlags.EXPAND);

        if (info.isRemovable()) {
            let removeIcon = Shell.TextureCache.get_default().load_icon_name ('media-eject', PLACES_ICON_SIZE);
            let removeIconBox = new St.Button({ child: removeIcon,
                                                reactive: true });
            this.actor.append(removeIconBox, Big.BoxPackFlags.NONE);
            removeIconBox.connect('clicked',
                                  Lang.bind(this, function() {
                                                  this._info.remove();
                                            }));
        }

        this.actor._delegate = this;
        let draggable = DND.makeDraggable(this.actor);
    },

    _onClicked: function(b) {
        this._info.launch();
        Main.overview.hide();
    },

    getDragActorSource: function() {
        return this._icon;
    },

    getDragActor: function(stageX, stageY) {
        return this._info.iconFactory(PLACES_ICON_SIZE);
    },

    //// Drag and drop methods ////

    shellWorkspaceLaunch: function() {
        this._info.launch();
    }
};

function DashPlaceDisplay() {
    this._init();
}

DashPlaceDisplay.prototype = {
    _init: function() {

        // Places is divided semi-arbitrarily into left and right; a grid would
        // look better in that there would be an even number of items left+right,
        // but it seems like we want some sort of differentiation between actions
        // like "Connect to server..." and regular folders
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: 4 });
        this._leftBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.actor.append(this._leftBox, Big.BoxPackFlags.EXPAND);
        this._rightBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.actor.append(this._rightBox, Big.BoxPackFlags.EXPAND);

        // Subdivide left into actions and devices
        this._actionsBox = new St.BoxLayout({ style_class: 'places-actions',
                                               vertical: true });

        this._devBox = new St.BoxLayout({ style_class: 'places-actions',
                                           name: 'placesDevices',
                                           vertical: true });

        this._dirsBox = new St.BoxLayout({ style_class: 'places-actions',
                                            vertical: true });

        this._leftBox.append(this._actionsBox, Big.BoxPackFlags.NONE);
        this._leftBox.append(this._devBox, Big.BoxPackFlags.NONE);

        this._rightBox.append(this._dirsBox, Big.BoxPackFlags.NONE);

        Main.placesManager.connect('defaults-updated', Lang.bind(this, this._updateDefaults));
        Main.placesManager.connect('bookmarks-updated', Lang.bind(this, this._updateBookmarks));
        Main.placesManager.connect('mounts-updated', Lang.bind(this, this._updateMounts));

        this._updateDefaults();
        this._updateMounts();
        this._updateBookmarks();
    },

    _updateDefaults: function() {
        this._actionsBox.destroy_children();

        let places = Main.placesManager.getDefaultPlaces();
        for (let i = 0; i < places.length; i++)
            this._actionsBox.add(new DashPlaceDisplayItem(places[i]).actor);
    },

    _updateMounts: function() {
        this._devBox.destroy_children();

        let places = Main.placesManager.getMounts();
        for (let i = 0; i < places.length; i++)
            this._devBox.add(new DashPlaceDisplayItem(places[i]).actor);
    },

    _updateBookmarks: function() {
        this._dirsBox.destroy_children();

        let places = Main.placesManager.getBookmarks();
        for (let i = 0; i < places.length; i ++)
            this._dirsBox.add(new DashPlaceDisplayItem(places[i]).actor);
    }
};

Signals.addSignalMethods(DashPlaceDisplay.prototype);

function PlaceSearchProvider() {
    this._init();
}

PlaceSearchProvider.prototype = {
    __proto__: Search.SearchProvider.prototype,

    _init: function() {
        Search.SearchProvider.prototype._init.call(this, _("PLACES & DEVICES"));
    },

    getResultMeta: function(resultId) {
        let placeInfo = Main.placesManager.lookupPlaceById(resultId);
        if (!placeInfo)
            return null;
        return { 'id': resultId,
                 'name': placeInfo.name,
                 'icon': placeInfo.iconFactory(Search.RESULT_ICON_SIZE) };
    },

    activateResult: function(id) {
        let placeInfo = Main.placesManager.lookupPlaceById(id);
        placeInfo.launch();
    },

    _compareResultMeta: function (idA, idB) {
        let infoA = Main.placesManager.lookupPlaceById(idA);
        let infoB = Main.placesManager.lookupPlaceById(idB);
        return infoA.name.localeCompare(infoB.name);
    },

    _searchPlaces: function(places, terms) {
        let multipleResults = [];
        let prefixResults = [];
        let substringResults = [];

        terms = terms.map(String.toLowerCase);

        for (let i = 0; i < places.length; i++) {
            let place = places[i];
            let mtype = place.matchTerms(terms);
            if (mtype == Search.MatchType.MULTIPLE)
                multipleResults.push(place.id);
            else if (mtype == Search.MatchType.PREFIX)
                prefixResults.push(place.id);
            else if (mtype == Search.MatchType.SUBSTRING)
                substringResults.push(place.id);
        }
        multipleResults.sort(this._compareResultMeta);
        prefixResults.sort(this._compareResultMeta);
        substringResults.sort(this._compareResultMeta);
        return multipleResults.concat(prefixResults.concat(substringResults));
    },

    getInitialResultSet: function(terms) {
        let places = Main.placesManager.getAllPlaces();
        return this._searchPlaces(places, terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        let places = previousResults.map(function (id) { return Main.placesManager.lookupPlaceById(id); });
        return this._searchPlaces(places, terms);
    }
}
