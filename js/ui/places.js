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

const Main = imports.ui.main;
const GenericDisplay = imports.ui.genericDisplay;

const PLACES_VSPACING = 8;
const PLACES_ICON_SIZE = 16;

function PlaceDisplay(name, icon, onActivate) {
    this._init(name, icon, onActivate);
}

PlaceDisplay.prototype = {
    _init : function(name, iconTexture, onActivate) {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   reactive: true,
                                   spacing: 4 });
        this.actor.connect('button-press-event', Lang.bind(this, function (b, e) {
            onActivate(this);
        }));
        let text = new Clutter.Text({ font_name: "Sans 14px",
                                      ellipsize: Pango.EllipsizeMode.END,
                                      color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                      text: name });
        this.actor.append(iconTexture, Big.BoxPackFlags.NONE);
        this.actor.append(text, Big.BoxPackFlags.EXPAND);
    }
};
Signals.addSignalMethods(PlaceDisplay.prototype);

function Places() {
    this._init();
}

Places.prototype = {
    _init : function() {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: 4 });
        this._menuBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      spacing: PLACES_VSPACING });
        this.actor.append(this._menuBox, Big.BoxPackFlags.EXPAND);
        this._dirsBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      spacing: PLACES_VSPACING });
        this.actor.append(this._dirsBox, Big.BoxPackFlags.EXPAND);

        let homeFile = Gio.file_new_for_path (GLib.get_home_dir());
        let homeUri = homeFile.get_uri();
        let homeLabel = Shell.util_get_label_for_uri (homeUri);
        let homeIcon = Shell.util_get_icon_for_uri (homeUri);
        let homeTexture = Shell.TextureCache.get_default().load_gicon(homeIcon, PLACES_ICON_SIZE);
        let home = new PlaceDisplay(homeLabel, homeTexture, Lang.bind(this, function() {
            Main.overview.hide();
            Gio.app_info_launch_default_for_uri(homeUri, Main.createAppLaunchContext());
        }));

        this._menuBox.append(home.actor, Big.BoxPackFlags.NONE);

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
            let networkIcon = networkApp.create_icon_texture(PLACES_ICON_SIZE);
            let network = new PlaceDisplay(networkApp.get_name(), networkIcon, Lang.bind(this, function () {
                Main.overview.hide();
                networkApp.launch();
            }));
            this._menuBox.append(network.actor, Big.BoxPackFlags.NONE);
        }

        let connectIcon = Shell.TextureCache.get_default().load_icon_name("applications-internet", PLACES_ICON_SIZE);
        let connect = new PlaceDisplay('Connect to...', connectIcon, Lang.bind(this, function () {
            Main.overview.hide();
            new Shell.Process({ args: ['nautilus-connect-server'] }).run();
        }));
        this._menuBox.append(connect.actor, Big.BoxPackFlags.NONE);

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
    },

    _reloadBookmarks: function() {

        this._dirsBox.remove_all();

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
            let iconTexture = Shell.TextureCache.get_default().load_gicon(icon, PLACES_ICON_SIZE);
            let item = new PlaceDisplay(label, iconTexture, Lang.bind(this, function() {
                Main.overview.hide();
                Gio.app_info_launch_default_for_uri(bookmark, Main.createAppLaunchContext());
            }));
            this._dirsBox.append(item.actor, Big.BoxPackFlags.NONE);
        }
    }
};
Signals.addSignalMethods(Places.prototype);
