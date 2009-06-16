/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;

const AppInfo = imports.misc.appInfo;
const GenericDisplay = imports.ui.genericDisplay;

const ENTERED_MENU_COLOR = new Clutter.Color();
ENTERED_MENU_COLOR.from_pixel(0x00ff0022);

const MENU_ICON_SIZE = 24;
const MENU_SPACING = 15;

// TODO - move this into GConf once we're not a plugin anymore
// but have taken over metacity
// This list is taken from GNOME Online popular applications
// http://online.gnome.org/applications
// but with nautilus removed (since it should already be running)
// and evince, totem, and gnome-file-roller removed (since they're
// usually started by opening documents, not by opening the app
// directly)
const DEFAULT_APPLICATIONS = [
    'mozilla-firefox.desktop',
    'gnome-terminal.desktop',
    'evolution.desktop',
    'gedit.desktop',
    'mozilla-thunderbird.desktop',
    'rhythmbox.desktop',
    'epiphany.desktop',
    'xchat.desktop',
    'openoffice.org-1.9-writer.desktop',
    'emacs.desktop',
    'gnome-system-monitor.desktop',
    'openoffice.org-1.9-calc.desktop',
    'eclipse.desktop',
    'openoffice.org-1.9-impress.desktop',
    'vncviewer.desktop'
];

const MAX_ITEMS = 30;

/* This class represents a single display item containing information about an application.
 *
 * appInfo - AppInfo object containing information about the application
 * availableWidth - total width available for the item
 */
function AppDisplayItem(appInfo, availableWidth) {
    this._init(appInfo, availableWidth);
}

AppDisplayItem.prototype = {
    __proto__:  GenericDisplay.GenericDisplayItem.prototype,

    _init : function(appInfo, availableWidth) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this, availableWidth); 
        this._appInfo = appInfo;

        this._setItemInfo(appInfo.name, appInfo.description,
                          appInfo.getIcon(GenericDisplay.ITEM_DISPLAY_ICON_SIZE));
    },

    //// Public method overrides ////

    // Opens an application represented by this display item.
    launch : function() {
        this._appInfo.launch();
    },

    //// Protected method overrides ////

    // Ensures the preview icon is created.
    _ensurePreviewIconCreated : function() {
        if (!this._showPreview || this._previewIcon)
            return; 

        let previewIconPath = this._appInfo.getIconPath(GenericDisplay.PREVIEW_ICON_SIZE);
        if (previewIconPath) {
            try {
                this._previewIcon = new Clutter.Texture({ width: GenericDisplay.PREVIEW_ICON_SIZE, height: GenericDisplay.PREVIEW_ICON_SIZE});               
                this._previewIcon.set_from_file(previewIconPath);
            } catch (e) {
                // we can get an error here if the file path doesn't exist on the system
                log('Error loading AppDisplayItem preview icon ' + e);
            }
        }
    }
};

const MENU_UNSELECTED = 0;
const MENU_SELECTED = 1;
const MENU_ENTERED = 2;

function MenuItem(name, id, iconName) {
    this._init(name, id, iconName);
}

/**
 * MenuItem:
 * Shows the list of menus in the sidebar.
 */
MenuItem.prototype = {
    _init: function(name, id, iconName) {
        this.id = id;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: 4,
                                   corner_radius: 4,
                                   padding_right: 4,
                                   reactive: true });
        this.actor.connect('button-press-event', Lang.bind(this, function (a, e) {
            this.setState(MENU_SELECTED);
        }));

        let iconTheme = Gtk.IconTheme.get_default();
        let pixbuf = null;
        this._icon = new Clutter.Texture({ width: MENU_ICON_SIZE,
                                           height: MENU_ICON_SIZE });
        // Wine manages not to have an icon
        try {
            pixbuf = iconTheme.load_icon(iconName, MENU_ICON_SIZE, 0 /* flags */);
        } catch (e) {
            pixbuf = iconTheme.load_icon('gtk-file', MENU_ICON_SIZE, 0);
        }
        if (pixbuf != null)
            Shell.clutter_texture_set_from_pixbuf(this._icon, pixbuf);
        this.actor.append(this._icon, 0);
        this._text = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: name });
        this.actor.append(this._text, Big.BoxPackFlags.EXPAND);

        let box = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                y_align: Big.BoxAlignment.CENTER
                              });

        this._arrow = new Shell.Arrow({ surface_width: MENU_ICON_SIZE/2,
                                        surface_height: MENU_ICON_SIZE/2,
                                        direction: Gtk.ArrowType.RIGHT,
                                        opacity: 0
                                      });
        box.append(this._arrow, 0);
        this.actor.append(box, 0);
    },

    getState: function() {
        return this._state;
    },

    setState: function (state) {
        if (state == this._state)
            return;
        this._state = state;
        if (this._state == MENU_UNSELECTED) {
            this.actor.background_color = null;
            this._arrow.set_opacity(0);
        } else if (this._state == MENU_ENTERED) {
            this.actor.background_color = ENTERED_MENU_COLOR;
            this._arrow.set_opacity(0xFF/2);
        } else {
            this.actor.background_color = GenericDisplay.ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR;
            this._arrow.set_opacity(0xFF);
        }
        this.emit('state-changed')
    }
}
Signals.addSignalMethods(MenuItem.prototype);

/* This class represents a display containing a collection of application items.
 * The applications are sorted based on their popularity by default, and based on
 * their name if some search filter is applied.
 *
 * width - width available for the display
 * height - height available for the display
 */
function AppDisplay(width, height, numberOfColumns, columnGap) {
    this._init(width, height, numberOfColumns, columnGap);
}

AppDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(width, height, numberOfColumns, columnGap) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, width, height, numberOfColumns, columnGap);

        this._menus = [];
        this._menuDisplays = [];

        // map<itemId, array of category names>
        this._appCategories = {};

        this._appMonitor = new Shell.AppMonitor();
        this._appSystem = new Shell.AppSystem();
        this._appsStale = true;
        this._appSystem.connect('changed', Lang.bind(this, function(appSys) {
            this._appsStale = true;
            // We still need to determine what events other than search can trigger
            // a change in the set of applications that are being shown while the
            // user in in the overlay mode, however let's redisplay just in case.
            this._redisplay(false);
            this._redisplayMenus();
        }));
        this._appMonitor.connect('changed', Lang.bind(this, function(monitor) {
            this._redisplay(false);
        }));

        // Load the GAppInfos now so it doesn't slow down the first
        // transition into the overlay
        this._refreshCache();

        this._focusInMenus = true;
        this._activeMenuIndex = -1;
        this._activeMenu = null;
        this._activeMenuApps = null;
        this._menuDisplay = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          spacing: MENU_SPACING
                                       });
        this._redisplayMenus();

        this.connect('expanded', Lang.bind(this, function (self) {
            this._filterReset();
        }));
    },

    moveRight: function() {
        if (this._expanded && this._focusInMenu) {
            this._focusInMenu = false;
            this._activeMenu.setState(MENU_ENTERED);
            this.selectFirstItem();
        }
    },

    moveLeft: function() {
        if (this._expanded && !this._focusInMenu) {
            this._activeMenu.setState(MENU_SELECTED);
            this.unsetSelected();
            this._focusInMenu = true;
        }
    },

    // Override genericDisplay.js
    getSideArea: function() {
        return this._menuDisplay;
    },

    selectUp: function() {
        if (!(this._expanded && this._focusInMenu))
            return GenericDisplay.GenericDisplay.prototype.selectUp.call(this);
        this._selectMenuIndex(this._activeMenuIndex - 1);
        return true;
    },

    selectDown: function() {
        if (!(this._expanded && this._focusInMenu))
            return GenericDisplay.GenericDisplay.prototype.selectDown.call(this);
        this._selectMenuIndex(this._activeMenuIndex+1);
        return true;
    },

    // Protected overrides

    _filterActive: function() {
        return !!this._search || this._activeMenuIndex >= 0;
    },

    _filterReset: function() {
        GenericDisplay.GenericDisplay.prototype._filterReset.call(this);
        if (this._activeMenu != null)
            this._activeMenu.setState(MENU_UNSELECTED);
        this._activeMenuIndex = -1;
        this._activeMenu = null;
        this._focusInMenu = true;
    },

    //// Private ////

    _emitStateChange: function() {
        this.emit('state-changed');
    },

    _selectMenuIndex: function(index) {
        if (index < 0 || index >= this._menus.length)
            return;
        this._menuDisplays[index].setState(MENU_SELECTED);
    },

    _redisplayMenus: function() {
        this._menuDisplay.remove_all();
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            let display = new MenuItem(menu.name, menu.id, menu.icon);
            this._menuDisplays.push(display);
            let menuIndex = i;
            display.connect('state-changed', Lang.bind(this, function (display) {
                let activated = display.getState() != MENU_UNSELECTED;
                if (!activated && display == this._activeMenu) {
                    this._activeMenuIndex = -1;
                    this._activeMenu = null;
                } else if (activated) {
                    if (display != this._activeMenu && this._activeMenu != null)
                        this._activeMenu.setState(MENU_UNSELECTED);
                    this._activeMenuIndex = menuIndex;
                    this._activeMenu = display;
                    this._activeMenuApps = this._appSystem.get_applications_for_menu(menu.id);
                }
                this._redisplay();
            }));
            this._menuDisplay.append(display.actor, 0);
        }
    },

    _addApp: function(appId) {
        let appInfo = AppInfo.getAppInfo(appId);
        if (appInfo != null) {
            this._allItems[appId] = appInfo;
            // [] is returned if we could not get the categories or the list of categories was empty
            let categories = Shell.get_categories_for_desktop_file(appId);
            this._appCategories[appId] = categories;
        } else {
            log("appInfo for " + appId + " was not found.");
        }
    },

    //// Protected method overrides //// 

    // Gets information about all applications by calling Gio.app_info_get_all().
    _refreshCache : function() {
        let me = this;
        if (!this._appsStale)
            return;
        this._allItems = {};
        this._appCategories = {};

        this._menus = this._appSystem.get_menus();

        // Loop over the toplevel menu items, load the set of desktop file ids
        // associated with each one
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            let menuApps = this._appSystem.get_applications_for_menu(menu.id);
            for (let j = 0; j < menuApps.length; j++) {
                let appId = menuApps[j];
                this._addApp(appId);
            }
        }

        // Now grab the desktop file ids for settings/preferences.
        // These show up in search, but not with the rest of apps.
        let settings = this._appSystem.get_all_settings();
        for (let i = 0; i < settings.length; i++) {
            let appId = settings[i];
            this._addApp(appId);
        }

        this._appsStale = false;
    },

    // Sets the list of the displayed items based on the most used apps.
    _setDefaultList : function() {
        let matchedInfos = AppInfo.getMostUsedApps(MAX_ITEMS);
        this._matchedItems = matchedInfos.map(function(info) { return info.appId; });
    },

    // Compares items associated with the item ids based on the alphabetical order
    // of the item names.
    // Returns an integer value indicating the result of the comparison.
    _compareItems : function(itemIdA, itemIdB) {
        let appA = this._allItems[itemIdA];
        let appB = this._allItems[itemIdB];
        return appA.get_name().localeCompare(appB.get_name());
    },

    // Checks if the item info can be a match for the search string by checking
    // the name, description, execution command, and categories for the application. 
    // Item info is expected to be GAppInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching : function(itemInfo, search) {
        // Search takes precedence; not typically useful to search within a
        // menu
        if (this._activeMenu == null || search != "")
            return this._isInfoMatchingSearch(itemInfo, search);
        else
            return this._isInfoMatchingMenu(itemInfo, search);
    },

    _isInfoMatchingMenu : function(itemInfo, search) {
        let id = itemInfo.get_id();
        for (let i = 0; i < this._activeMenuApps.length; i++) {
            let activeId = this._activeMenuApps[i];
            if (activeId == id)
                return true;
        }
        return false;
    },

    _isInfoMatchingSearch: function(itemInfo, search) {
        if (search == null || search == '')
            return true;

        let name = itemInfo.get_name().toLowerCase();
        if (name.indexOf(search) >= 0)
            return true;

        let description = itemInfo.get_description();
        if (description) {
            description = description.toLowerCase();
            if (description.indexOf(search) >= 0)
                return true;
        }

        if (itemInfo.get_executable() == null) {
            log("Missing an executable for " + itemInfo.get_name());
        } else {
            let exec = itemInfo.get_executable().toLowerCase();
            if (exec.indexOf(search) >= 0)
                return true;
        }

        // we expect this._appCategories.hasOwnProperty(itemInfo.get_id()) to always be true here
        let categories = this._appCategories[itemInfo.get_id()];
        for (let i = 0; i < categories.length; i++) {
            let category = categories[i].toLowerCase();
            if (category.indexOf(search) >= 0)
                return true;
        }
       
        return false;
    },

    // Creates an AppDisplayItem based on itemInfo, which is expected be an AppInfo object.
    _createDisplayItem: function(itemInfo) {
        return new AppDisplayItem(itemInfo, this._columnWidth);
    }
};

Signals.addSignalMethods(AppDisplay.prototype);
