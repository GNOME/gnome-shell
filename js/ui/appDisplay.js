/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;
const Mainloop = imports.mainloop;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppIcon = imports.ui.appIcon;
const DND = imports.ui.dnd;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Workspaces = imports.ui.workspaces;

const ENTERED_MENU_COLOR = new Clutter.Color();
ENTERED_MENU_COLOR.from_pixel(0x00ff0022);

const WELL_DEFAULT_COLUMNS = 4;
const WELL_ITEM_MIN_HSPACING = 4;
const WELL_ITEM_VSPACING = 4;

const MENU_ARROW_SIZE = 12;
const MENU_SPACING = 7;

const MAX_ITEMS = 30;

/* This class represents a single display item containing information about an application.
 *
 * appInfo - AppInfo object containing information about the application
 */
function AppDisplayItem(appInfo) {
    this._init(appInfo);
}

AppDisplayItem.prototype = {
    __proto__:  GenericDisplay.GenericDisplayItem.prototype,

    _init : function(appInfo) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this);
        this._appInfo = appInfo;

        this._setItemInfo(appInfo.get_name(), appInfo.get_description());
    },

    getId: function() {
        return this._appInfo.get_id();
    },

    //// Public method overrides ////

    // Opens an application represented by this display item.
    launch : function() {
        let windows = Shell.AppMonitor.get_default().get_windows_for_app(this._appInfo.get_id());
        if (windows.length > 0) {
            let mostRecentWindow = windows[0];
            Main.overview.activateWindow(mostRecentWindow, Main.currentTime());
        } else {
            this._appInfo.launch();
        }
    },

    //// Protected method overrides ////

    // Returns an icon for the item.
    _createIcon : function() {
        return this._appInfo.create_icon_texture(GenericDisplay.ITEM_DISPLAY_ICON_SIZE);
    },

    // Returns a preview icon for the item.
    _createPreviewIcon : function() {
        return this._appInfo.create_icon_texture(GenericDisplay.PREVIEW_ICON_SIZE);
    },

    shellWorkspaceLaunch: function() {
        this.launch();
    }
};

const MENU_UNSELECTED = 0;
const MENU_SELECTED = 1;
const MENU_ENTERED = 2;

function MenuItem(name, id) {
    this._init(name, id);
}

/**
 * MenuItem:
 * Shows the list of menus in the sidebar.
 */
MenuItem.prototype = {
    _init: function(name, id) {
        this.id = id;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: 4,
                                   corner_radius: 4,
                                   padding_right: 4,
                                   padding_left: 4,
                                   reactive: true });
        this.actor.connect('button-press-event', Lang.bind(this, function (a, e) {
            this.setState(MENU_SELECTED);
        }));

        this._text = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        text: name });

        // We use individual boxes for the label and the arrow to ensure that they
        // are aligned vertically. Just setting y_align: Big.BoxAlignment.CENTER
        // on this.actor does not seem to achieve that.  
        let labelBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER,
                                     padding: 4 });

        labelBox.append(this._text, Big.BoxPackFlags.NONE);
       
        this.actor.append(labelBox, Big.BoxPackFlags.EXPAND);

        let arrowBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER });

        this._arrow = new Shell.Arrow({ surface_width: MENU_ARROW_SIZE,
                                        surface_height: MENU_ARROW_SIZE,
                                        direction: Gtk.ArrowType.RIGHT,
                                        opacity: 0 });
        arrowBox.append(this._arrow, Big.BoxPackFlags.NONE);
        this.actor.append(arrowBox, Big.BoxPackFlags.NONE);
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
 * showPrefs - a boolean indicating if this AppDisplay should contain preference
 *             applets, rather than applications
 */
function AppDisplay(showPrefs) {
    this._init(showPrefs);
}

AppDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(showPrefs) {
        GenericDisplay.GenericDisplay.prototype._init.call(this);

        this._showPrefs = showPrefs;

        this._menus = [];
        this._menuDisplays = [];
        // map<search term, map<appId, true>>
        // We use a map of appIds instead of an array to ensure that we don't have duplicates and for easier lookup.
        this._menuSearchAppMatches = {};

        this._appMonitor = Shell.AppMonitor.get_default();
        this._appSystem = Shell.AppSystem.get_default();
        this._appsStale = true;
        this._appSystem.connect('installed-changed', Lang.bind(this, function(appSys) {
            this._appsStale = true;
            this._redisplay(GenericDisplay.RedisplayFlags.NONE);
        }));
        this._appSystem.connect('favorites-changed', Lang.bind(this, function(appSys) {
            this._appsStale = true;
            this._redisplay(GenericDisplay.RedisplayFlags.NONE);
        }));

        this._focusInMenus = true;
        this._activeMenuIndex = -1;
        this._activeMenu = null;
        this._activeMenuApps = null;
        this._menuDisplay = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          spacing: MENU_SPACING
                                       });

        this.connect('expanded', Lang.bind(this, function (self) {
            this._filterReset();
        }));
        this._filterReset();
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
    getNavigationArea: function() {
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

    setSearch: function(text) {
        let lowertext = text.toLowerCase();
        if (lowertext == this._search)
            return;

        // We prepare menu matches up-front, so that we don't
        // need to go over all menu items for each application
        // and then get all applications for a matching menu
        // to see if a particular application passed to
        // _isInfoMatching() is a match.
        let terms = lowertext.split(/\s+/);
        this._menuSearchAppMatches = {};
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            this._menuSearchAppMatches[term] = {};
            for (let j = 0; j < this._menus.length; j++) {
                let menuItem = this._menus[j];
                // Match only on the beginning of the words in category names,
                // because otherwise it introduces unnecessary noise in the results.
                if (menuItem.name.toLowerCase().indexOf(term) == 0 ||
                    menuItem.name.toLowerCase().indexOf(" " + term) > 0) {
                    let menuApps = this._appSystem.get_applications_for_menu(menuItem.id);
                    for (let k = 0; k < menuApps.length; k++) {
                        let menuApp = menuApps[k];
                        this._menuSearchAppMatches[term][menuApp.get_id()] = true;
                    }
                }
            }
        }

        GenericDisplay.GenericDisplay.prototype.setSearch.call(this, text);
    },

    // Protected overrides

    _filterActive: function() {
        // We always have a filter now since a menu must be selected
        return true;
    },

    _filterReset: function() {
        GenericDisplay.GenericDisplay.prototype._filterReset.call(this);
        this._selectMenuIndex(0);
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

    _getMostUsed: function() {
        let context = "";
        return this._appMonitor.get_most_used_apps(context, 30).map(Lang.bind(this, function (id) {
            return this._appSystem.lookup_cached_app(id);
        })).filter(function (e) { return e != null });
    },

    _addMenuItem: function(name, id, index) {
        let display = new MenuItem(name, id);
        this._menuDisplays.push(display);
        display.connect('state-changed', Lang.bind(this, function (display) {
            let activated = display.getState() != MENU_UNSELECTED;
            if (!activated && display == this._activeMenu) {
                this._activeMenuIndex = -1;
                this._activeMenu = null;
            } else if (activated) {
                if (display != this._activeMenu && this._activeMenu != null)
                    this._activeMenu.setState(MENU_UNSELECTED);
                this._activeMenuIndex = index;
                this._activeMenu = display;
                if (id == null) {
                    this._activeMenuApps = this._getMostUsed();
                } else {
                    this._activeMenuApps = this._appSystem.get_applications_for_menu(id);
                }
            }
            this._redisplay(GenericDisplay.RedisplayFlags.FULL);
        }));
        this._menuDisplay.append(display.actor, 0);
    },

    _redisplayMenus: function() {
        this._menuDisplay.remove_all();
        this._addMenuItem(_("Frequent"), null, 'gtk-select-all');
        // Adding an empty box here results in double spacing between
        // "Frequent" and the other items.
        let separator_actor = new Big.Box();
        this._menuDisplay.append(separator_actor, 0);
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            this._addMenuItem(menu.name, menu.id, i+1);
        }
    },

    _addApp: function(appInfo) {
        let appId = appInfo.get_id();
        this._allItems[appId] = appInfo;
    },

    //// Protected method overrides ////

    // Gets information about all applications by calling Gio.app_info_get_all().
    _refreshCache : function() {
        if (!this._appsStale)
            return true;
        this._allItems = {};

        if (this._showPrefs) {
            // Get the desktop file ids for settings/preferences.
            // These are used for search results, but not in the app menus.
            let settings = this._appSystem.get_all_settings();
            for (let i = 0; i < settings.length; i++) {
                let app = settings[i];
                this._addApp(app);
            }
        } else {
            // Loop over the toplevel menu items, load the set of desktop file ids
            // associated with each one, skipping empty menus
            let allMenus = this._appSystem.get_menus();
            this._menus = [];
            for (let i = 0; i < allMenus.length; i++) {
                let menu = allMenus[i];
                let menuApps = this._appSystem.get_applications_for_menu(menu.id);
                let hasVisibleApps = menuApps.some(function (app) { return !app.get_is_nodisplay(); });
                if (!hasVisibleApps) {
                    continue;
                }
                this._menus.push(menu);
                for (let j = 0; j < menuApps.length; j++) {
                    let app = menuApps[j];
                    this._addApp(app);
                }
            }
            this._redisplayMenus();
        }

        this._appsStale = false;
        return false;
    },

    // Stub this out; the app display always has a category selected
    _setDefaultList : function() {
        this._matchedItems = {};
        this._matchedItemKeys = [];
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
    // the name, description, execution command, and category for the application.
    // Item info is expected to be Shell.AppInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching : function(itemInfo, search) {
        // Don't show nodisplay items here
        if (itemInfo.get_is_nodisplay())
            return false;
        // Search takes precedence; not typically useful to search within a
        // menu
        if (this._activeMenu == null || search != "")
            return this._isInfoMatchingSearch(itemInfo, search);
        else
            return this._isInfoMatchingMenu(itemInfo);
    },

    _isInfoMatchingMenu: function(itemInfo) {
        let id = itemInfo.get_id();
        for (let i = 0; i < this._activeMenuApps.length; i++) {
            let activeApp = this._activeMenuApps[i];
            if (activeApp.get_id() == id)
                return true;
        }
        return false;
    },

    _isInfoMatchingSearch: function(itemInfo, search) {
        if (search == null || search == '')
            return true;

        let fold = function(s) {
            if (!s)
                return s;
            return GLib.utf8_casefold(GLib.utf8_normalize(s, -1,
                                                          GLib.NormalizeMode.ALL), -1);
        };
        let name = fold(itemInfo.get_name());
        if (name.indexOf(search) >= 0)
            return true;

        let description = fold(itemInfo.get_description());
        if (description) {
            if (description.indexOf(search) >= 0)
                return true;
        }

        let exec = fold(itemInfo.get_executable());
        if (exec == null) {
            log("Missing an executable for " + itemInfo.name);
        } else {
            if (exec.indexOf(search) >= 0)
                return true;
        }

        if (this._menuSearchAppMatches[search]) {
            if (this._menuSearchAppMatches[search].hasOwnProperty(itemInfo.get_id()))
                return true;
        } else {
            log("Missing an entry for search term " + search + " in this._menuSearchAppMatches");
        }

        return false;
    },

    // Creates an AppDisplayItem based on itemInfo, which is expected be an Shell.AppInfo object.
    _createDisplayItem: function(itemInfo) {
        return new AppDisplayItem(itemInfo);
    }
};

Signals.addSignalMethods(AppDisplay.prototype);

function BaseWellItem(app, isFavorite, hasMenu) {
    this._init(app, isFavorite, hasMenu);
}

BaseWellItem.prototype = {
    __proto__: AppIcon.AppIcon.prototype,

    _init: function(app, isFavorite) {
        AppIcon.AppIcon.prototype._init.call(this, { app: app,
                                                     menuType: AppIcon.MenuType.ON_RIGHT,
                                                     glow: true });

        this.isFavorite = isFavorite;

        this._draggable = DND.makeDraggable(this.actor, true);

        // Do these as anonymous functions to avoid conflict with handlers in subclasses
        this.actor.connect('button-press-event', Lang.bind(this, function(actor, event) {
            let [stageX, stageY] = event.get_coords();
            this._dragStartX = stageX;
            this._dragStartY = stageY;
            return false;
        }));
        this.actor.connect('notify::hover', Lang.bind(this, function () {
            let hover = this.actor.hover;
            if (!hover) {
                if (this.actor.pressed && this._dragStartX != null) {
                    this.actor.fake_release();
                    this._draggable.startDrag(this._dragStartX, this._dragStartY,
                                              Main.currentTime());
                } else {
                    this._dragStartX = null;
                    this._dragStartY = null;
                }
            }
        }));
    },

    shellWorkspaceLaunch : function() {
        // Here we just always launch the application again, even if we know
        // it was already running.  For most applications this
        // should have the effect of creating a new window, whether that's
        // a second process (in the case of Calculator) or IPC to existing
        // instance (Firefox).  There are a few less-sensical cases such
        // as say Pidgin, but ideally what we do there is have the app
        // express to us that it doesn't do relaunch=new-window in the
        // .desktop file.
        this.app.get_info().launch();
    },

    getDragActor: function() {
        return this.createDragActor();
    },

    // Returns the original icon that is being used as a source for the cloned texture
    // that represents the item as it is being dragged.
    getDragActorSource: function() {
        return this.actor;
    }
}

function RunningWellItem(app, isFavorite) {
    this._init(app, isFavorite);
}

RunningWellItem.prototype = {
    __proto__: BaseWellItem.prototype,

    _init: function(app, isFavorite) {
        BaseWellItem.prototype._init.call(this, app, isFavorite);

        this._dragStartX = 0;
        this._dragStartY = 0;

        this.actor.connect('activate', Lang.bind(this, this._onActivate));
    },

    _onActivate: function (actor, event) {
        let modifiers = Shell.get_event_state(event);

        if (modifiers & Clutter.ModifierType.CONTROL_MASK) {
            this.app.get_info().launch();
        } else {
            this.activateMostRecentWindow();
        }
    },

    activateMostRecentWindow: function () {
        let mostRecentWindow = this.app.get_windows()[0];
        Main.overview.activateWindow(mostRecentWindow, Main.currentTime());
    },

    highlightWindow: function(metaWindow) {
        Main.overview.getWorkspacesForWindow(metaWindow).setHighlightWindow(metaWindow);
    },

    activateWindow: function(metaWindow) {
        if (metaWindow) {
            this._didActivateWindow = true;
            Main.overview.activateWindow(metaWindow, Main.currentTime());
        } else
            Main.overview.hide();
    },

    menuPoppedUp: function() {
        Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(this.app.get_id());
    },

    menuPoppedDown: function() {
        if (this._didActivateWindow)
            return;

        Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(null);
    }
};

function InactiveWellItem(app, isFavorite) {
    this._init(app, isFavorite);
}

InactiveWellItem.prototype = {
    __proto__: BaseWellItem.prototype,

    _init : function(app, isFavorite) {
        BaseWellItem.prototype._init.call(this, app, isFavorite);

        this.actor.connect('notify::pressed', Lang.bind(this, this._onPressedChanged));
        this.actor.connect('activate', Lang.bind(this, this._onActivate));
    },

    _onPressedChanged: function() {
        this.setHighlight(this.actor.pressed);
    },

    _onActivate: function() {
        this.app.get_info().launch();
        Main.overview.hide();
        return true;
    },

    menuPoppedUp: function() {
    },

    menuPoppedDown: function() {
    }
};

function WellGrid() {
    this._init();
}

WellGrid.prototype = {
    _init: function() {
        this.actor = new Shell.GenericContainer();

        this._separator = new Big.Box({ height: 1 });
        this.actor.add_actor(this._separator);
        this._separatorIndex = 0;
        this._cachedSeparatorY = 0;

        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
    },

    _getPreferredWidth: function (grid, forHeight, alloc) {
        let [itemMin, itemNatural] = this._getItemPreferredWidth();
        let children = this._getItemChildren();
        let nColumns;
        if (children.length < WELL_DEFAULT_COLUMNS)
            nColumns = children.length;
        else
            nColumns = WELL_DEFAULT_COLUMNS;
        alloc.min_size = itemMin;
        alloc.natural_size = itemNatural * nColumns;
    },

    _getPreferredHeight: function (grid, forWidth, alloc) {
        let [rows, columns, itemWidth, itemHeight] = this._computeLayout(forWidth);
        let totalVerticalSpacing = Math.max(rows - 1, 0) * WELL_ITEM_VSPACING;

        let [separatorMin, separatorNatural] = this._separator.get_preferred_height(forWidth);
        alloc.min_size = alloc.natural_size = rows * itemHeight + totalVerticalSpacing + separatorNatural;
    },

    _allocate: function (grid, box, flags) {
        let children = this._getItemChildren();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [rows, columns, itemWidth, itemHeight] = this._computeLayout(availWidth);

        let [separatorMin, separatorNatural] = this._separator.get_preferred_height(-1);

        let x = box.x1;
        let y = box.y1;
        let columnIndex = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinWidth, childNaturalWidth] = children[i].get_preferred_width(-1);

            /* Center the item in its allocation horizontally */
            let width = Math.min(itemWidth, childNaturalWidth);
            let horizSpacing = (itemWidth - width) / 2;

            let childBox = new Clutter.ActorBox();
            childBox.x1 = Math.floor(x + horizSpacing);
            childBox.y1 = y;
            childBox.x2 = childBox.x1 + width;
            childBox.y2 = childBox.y1 + itemHeight;
            children[i].allocate(childBox, flags);

            columnIndex++;
            if (columnIndex == columns) {
                columnIndex = 0;
            }

            if (columnIndex == 0) {
                y += itemHeight + WELL_ITEM_VSPACING;
                x = box.x1;
            } else {
                x += itemWidth;
            }
        }
    },

    removeAll: function () {
        let itemChildren = this._getItemChildren();
        for (let i = 0; i < itemChildren.length; i++) {
            itemChildren[i].destroy();
        }
    },

    _getItemChildren: function () {
        let children = this.actor.get_children();
        children.shift();
        return children;
    },

    _computeLayout: function (forWidth) {
        let [itemMinWidth, itemNaturalWidth] = this._getItemPreferredWidth();
        let columnsNatural;
        let i;
        let children = this._getItemChildren();
        if (children.length == 0)
            return [0, WELL_DEFAULT_COLUMNS, 0, 0];
        let nColumns = 0;
        let usedWidth = 0;
        // Big.Box will allocate us at 0x0 if we are not visible; this is probably a
        // Big.Box bug but it can't be fixed because if children are skipped in allocate()
        // Clutter gets confused (see http://bugzilla.openedhand.com/show_bug.cgi?id=1831)
        if (forWidth <= 0) {
            nColumns = WELL_DEFAULT_COLUMNS;
        } else {
            while (nColumns < WELL_DEFAULT_COLUMNS &&
                   nColumns < children.length &&
                   usedWidth + itemMinWidth <= forWidth) {
               // By including WELL_ITEM_MIN_HSPACING in usedWidth, we are ensuring
               // that the number of columns we end up with will allow the spacing
               // between the columns to be at least that value.
               usedWidth += itemMinWidth + WELL_ITEM_MIN_HSPACING;
               nColumns++;
            }
        }

        if (nColumns == 0) {
           log("WellGrid: couldn't fit a column in width " + forWidth);
           /* FIXME - fall back to smaller icon size */
        }

        let minWidth = itemMinWidth * nColumns;

        let lastColumnIndex = nColumns - 1;
        let rows = Math.ceil(children.length / nColumns);

        let itemWidth;
        if (forWidth <= 0) {
            itemWidth = itemNaturalWidth;
        } else {
            itemWidth = Math.floor(forWidth / nColumns);
        }

        let itemNaturalHeight = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMin, childNatural] = children[i].get_preferred_height(itemWidth);
            if (childNatural > itemNaturalHeight)
                itemNaturalHeight = childNatural;
        }

        return [rows, nColumns, itemWidth, itemNaturalHeight];
    },

    _getItemPreferredWidth: function () {
        let children = this._getItemChildren();
        let minWidth = 0;
        let naturalWidth = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMin, childNatural] = children[i].get_preferred_width(-1);
            if (childMin > minWidth)
                minWidth = childMin;
            if (childNatural > naturalWidth)
                naturalWidth = childNatural;
        }
        return [minWidth, naturalWidth];
    }
}

function AppWell() {
    this._init();
}

AppWell.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   x_align: Big.BoxAlignment.CENTER });
        this.actor._delegate = this;

        this._pendingRedisplay = false;
        this.actor.connect('notify::mapped', Lang.bind(this, this._onMappedNotify));

        this._grid = new WellGrid();
        this.actor.append(this._grid.actor, Big.BoxPackFlags.EXPAND);

        this._appSystem = Shell.AppSystem.get_default();
        this._appMonitor = Shell.AppMonitor.get_default();

        this._appSystem.connect('installed-changed', Lang.bind(this, function(appSys) {
            this._redisplay();
        }));
        this._appSystem.connect('favorites-changed', Lang.bind(this, function(appSys) {
            this._redisplay();
        }));
        this._appMonitor.connect('window-added', Lang.bind(this, function(monitor) {
            this._redisplay();
        }));
        this._appMonitor.connect('window-removed', Lang.bind(this, function(monitor) {
            this._redisplay();
        }));

        this._redisplay();
    },

    _appIdListToHash: function(apps) {
        let ids = {};
        for (let i = 0; i < apps.length; i++)
            ids[apps[i].get_id()] = apps[i];
        return ids;
    },

    _onMappedNotify: function() {
        let mapped = this.actor.mapped;
        if (mapped && this._pendingRedisplay)
            this._redisplay();
    },

    _redisplay: function () {
        let mapped = this.actor.mapped;
        if (!mapped) {
            this._pendingRedisplay = true;
            return;
        }
        this._pendingRedisplay = false;

        this._grid.removeAll();

        let favorites = this._appMonitor.get_favorites();
        let favoriteIds = this._appIdListToHash(favorites);

        /* hardcode here pending some design about how exactly desktop contexts behave */
        let contextId = "";

        let running = this._appMonitor.get_running_apps(contextId);
        let runningIds = this._appIdListToHash(running)

        for (let i = 0; i < favorites.length; i++) {
            let app = favorites[i];
            let display;
            if (app.get_windows().length > 0) {
                display = new RunningWellItem(app, true);
            } else {
                display = new InactiveWellItem(app, true);
            }
            this._grid.actor.add_actor(display.actor);
        }

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            let display = new RunningWellItem(app, false);
            this._grid.actor.add_actor(display.actor);
        }
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let appSystem = Shell.AppSystem.get_default();

        let app = null;
        if (source instanceof AppDisplayItem) {
            app = appSystem.lookup_cached_app(source.getId());
        } else if (source instanceof Workspaces.WindowClone) {
            let appMonitor = Shell.AppMonitor.get_default();
            app = appMonitor.get_window_app(source.metaWindow);
        }

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient()) {
            return false;
        }

        let id = app.get_id();

        let favoriteIds = this._appSystem.get_favorites();
        let favoriteIdsObject = this._arrayValues(favoriteIds);

        let srcIsFavorite = (id in favoriteIdsObject);

        if (srcIsFavorite) {
            return false;
        } else {
            Mainloop.idle_add(function () {
                appSystem.add_favorite(id);
                return false;
            });
        }

        return true;
    }
};

Signals.addSignalMethods(AppWell.prototype);
