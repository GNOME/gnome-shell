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
const GenericDisplay = imports.ui.genericDisplay;

const NAUTILUS_PREFS_DIR = '/apps/nautilus/preferences';
const DESKTOP_IS_HOME_KEY = NAUTILUS_PREFS_DIR + '/desktop_is_home_dir';

const PLACES_VSPACING = 8;
const PLACES_ICON_SIZE = 16;

/**
 * Represents a place object, which is most normally a bookmark entry,
 * a mount/volume, or a special place like the Home Folder, Computer, and Network.
 *
 * @name: String title
 * @iconFactory: A JavaScript callback which will create an icon texture given a size parameter
 * @launch: A JavaScript callback to launch the entry
 */
function PlaceInfo(name, iconFactory, launch) {
    this._init(name, iconFactory, launch);
}

PlaceInfo.prototype = {
    _init: function(name, iconFactory, launch) {
        this.name = name;
        this.iconFactory = iconFactory;
        this.launch = launch;
        this.id = null;
    }
}

function PlacesManager() {
    this._init();
}

PlacesManager.prototype = {
    _init: function() {
        let gconf = Shell.GConf.get_default();
        gconf.watch_directory(NAUTILUS_PREFS_DIR);

        this._mounts = [];
        this._bookmarks = [];
        this._isDesktopHome = false;

        let homeFile = Gio.file_new_for_path (GLib.get_home_dir());
        let homeUri = homeFile.get_uri();
        let homeLabel = Shell.util_get_label_for_uri (homeUri);
        let homeIcon = Shell.util_get_icon_for_uri (homeUri);
        this._home = new PlaceInfo(homeLabel,
            function(size) {
                return Shell.TextureCache.get_default().load_gicon(homeIcon, size);
            },
            function() {
                Gio.app_info_launch_default_for_uri(homeUri, Main.createAppLaunchContext());
            });

        let desktopPath = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DESKTOP);
        let desktopFile = Gio.file_new_for_path (desktopPath);
        let desktopUri = desktopFile.get_uri();
        let desktopLabel = Shell.util_get_label_for_uri (desktopUri);
        let desktopIcon = Shell.util_get_icon_for_uri (desktopUri);
        this._desktopMenu = new PlaceInfo(desktopLabel,
            function(size) {
                return Shell.TextureCache.get_default().load_gicon(desktopIcon, size);
            },
            function() {
                Gio.app_info_launch_default_for_uri(desktopUri, Main.createAppLaunchContext());
            });

        this._connect = new PlaceInfo(_("Connect to..."),
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
            this._network = new PlaceInfo(networkApp.get_name(),
                function(size) {
                    return networkApp.create_icon_texture(size);
                },
                function () {
                    networkApp.launch();
                });
        }

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

            let item = new PlaceInfo(label,
                function(size) {
                    return Shell.TextureCache.get_default().load_gicon(icon, size);
                },
                function() {
                    Gio.app_info_launch_default_for_uri(bookmark, Main.createAppLaunchContext());
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
        let mountLabel = mount.get_name();
        let mountIcon = mount.get_icon();
        let root = mount.get_root();
        let mountUri = root.get_uri();
        let devItem = new PlaceInfo(mountLabel,
               function(size) {
                        return Shell.TextureCache.get_default().load_gicon(mountIcon, size);
               },
               function() {
                        Gio.app_info_launch_default_for_uri(mountUri, Main.createAppLaunchContext());
               });
        this._mounts.push(devItem);
    },

    getAllPlaces: function () {
        return this.getDefaultPlaces().concat(this.getBookmarks(), this.getMounts());
    },

    getDefaultPlaces: function () {
        let places = [this._home];

        if (this._isDesktopHome)
            places.push(this._desktopMenu);

        if (this._network)
            places.push(this._network);

        places.push(this._connect);
        return places;
    },

    getBookmarks: function () {
        return this._bookmarks;
    },

    getMounts: function () {
        return this._mounts;
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
                                   reactive: true,
                                   spacing: 4 });
        this.actor.connect('button-release-event', Lang.bind(this, function (b, e) {
            this._info.launch();
            Main.overview.hide();
        }));
        let text = new St.Label({ style_class: 'places-item',
                                  text: info.name });
        let iconBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER });
        iconBox.append(this._icon, Big.BoxPackFlags.NONE);
        this.actor.append(iconBox, Big.BoxPackFlags.NONE);
        this.actor.append(text, Big.BoxPackFlags.EXPAND);

        this.actor._delegate = this;
        let draggable = DND.makeDraggable(this.actor);
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
        this._actionsBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                         spacing: PLACES_VSPACING });

        this._devBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      spacing: PLACES_VSPACING,
                                      padding_top: 6 });

        this._dirsBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      spacing: PLACES_VSPACING });

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
        this._actionsBox.get_children().forEach(function (child) {
            child.destroy();
        });

        let places = Main.placesManager.getDefaultPlaces();
        for (let i = 0; i < places.length; i++)
            this._actionsBox.append(new DashPlaceDisplayItem(places[i]).actor, Big.BoxPackFlags.NONE);
    },

    _updateMounts: function() {
        this._devBox.get_children().forEach(function (child) {
            child.destroy();
        });

        let places = Main.placesManager.getMounts();
        for (let i = 0; i < places.length; i++)
            this._devBox.append(new DashPlaceDisplayItem(places[i]).actor, Big.BoxPackFlags.NONE);
    },

    _updateBookmarks: function() {
        this._dirsBox.get_children().forEach(function (child) {
            child.destroy();
        });

        let places = Main.placesManager.getBookmarks();
        for (let i = 0; i < places.length; i ++)
            this._dirsBox.append(new DashPlaceDisplayItem(places[i]).actor, Big.BoxPackFlags.NONE);
    }
};

Signals.addSignalMethods(DashPlaceDisplay.prototype);


function PlaceDisplayItem(placeInfo) {
    this._init(placeInfo);
}

PlaceDisplayItem.prototype = {
    __proto__: GenericDisplay.GenericDisplayItem.prototype,

    _init : function(placeInfo) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this);
        this._info = placeInfo;

        this._setItemInfo(placeInfo.name, '');
    },

    //// Public method overrides ////

    // Opens an application represented by this display item.
    launch : function() {
        this._info.launch();
    },

    shellWorkspaceLaunch: function() {
        this._info.launch();
    },

    //// Protected method overrides ////

    // Returns an icon for the item.
    _createIcon: function() {
        return this._info.iconFactory(GenericDisplay.ITEM_DISPLAY_ICON_SIZE);
    },

    // Returns a preview icon for the item.
    _createPreviewIcon: function() {
        return this._info.iconFactory(GenericDisplay.PREVIEW_ICON_SIZE);
    }

};

function PlaceDisplay(flags) {
    this._init(flags);
}

PlaceDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init: function(flags) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, flags);
        this._stale = true;
        Main.placesManager.connect('places-updated', Lang.bind(this, function (e) {
            this._stale = true;
        }));
    },

    //// Protected method overrides ////
    _refreshCache: function () {
        if (!this._stale)
            return true;
        this._allItems = {};
        let array = Main.placesManager.getAllPlaces();
        for (let i = 0; i < array.length; i ++) {
            // We are using an array id as placeInfo id because placeInfo doesn't have any
            // other information piece that can be used as a unique id. There are different
            // types of placeInfo, such as devices and directories that would result in differently
            // structured ids. Also the home directory can show up in both the default places and in
            // bookmarks which means its URI can't be used as a unique id. (This does mean it can
            // appear twice in search results, though that doesn't happen at the moment because we
            // name it "Home Folder" in default places and it's named with the user's system name
            // if it appears as a bookmark.)
            let placeInfo = array[i];
            placeInfo.id = i;
            this._allItems[i] = placeInfo;
        }
        this._stale = false;
        return false;
    },

    // Sets the list of the displayed items.
    _setDefaultList: function() {
        this._matchedItems = {};
        this._matchedItemKeys = [];
        for (id in this._allItems) {
            this._matchedItems[id] = 1;
            this._matchedItemKeys.push(id);
        }
        this._matchedItemKeys.sort(Lang.bind(this, this._compareItems));
    },

    // Checks if the item info can be a match for the search string by checking
    // the name of the place. Item info is expected to be PlaceInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching: function(itemInfo, search) {
        if (search == null || search == '')
            return true;

        let name = itemInfo.name.toLowerCase();
        if (name.indexOf(search) >= 0)
            return true;

        return false;
    },

    // Compares items associated with the item ids based on the alphabetical order
    // of the item names.
    // Returns an integer value indicating the result of the comparison.
    _compareItems: function(itemIdA, itemIdB) {
        let placeA = this._allItems[itemIdA];
        let placeB = this._allItems[itemIdB];
        return placeA.name.localeCompare(placeB.name);
    },

    // Creates a PlaceDisplayItem based on itemInfo, which is expected to be a PlaceInfo object.
    _createDisplayItem: function(itemInfo) {
        return new PlaceDisplayItem(itemInfo);
    }
};
