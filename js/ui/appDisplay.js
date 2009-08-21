/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Tidy = imports.gi.Tidy;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;
const Mainloop = imports.mainloop;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DND = imports.ui.dnd;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Workspaces = imports.ui.workspaces;

const ENTERED_MENU_COLOR = new Clutter.Color();
ENTERED_MENU_COLOR.from_pixel(0x00ff0022);

const GLOW_COLOR = new Clutter.Color();
GLOW_COLOR.from_pixel(0x4f6ba4ff);
const GLOW_PADDING = 5;


const APP_ICON_SIZE = 48;
const WELL_DEFAULT_COLUMNS = 4;
const WELL_ITEM_HSPACING = 0;
const WELL_ITEM_VSPACING = 4;

const MENU_ICON_SIZE = 24;
const MENU_SPACING = 15;

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
        this._appInfo.launch();
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
        this.actor.append(this._icon, Big.BoxPackFlags.NONE);
        this._text = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        text: name });

        // We use individual boxes for the label and the arrow to ensure that they
        // are aligned vertically. Just setting y_align: Big.BoxAlignment.CENTER
        // on this.actor does not seem to achieve that.  
        let labelBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER });

        labelBox.append(this._text, Big.BoxPackFlags.NONE);
       
        this.actor.append(labelBox, Big.BoxPackFlags.EXPAND);

        let arrowBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER });

        this._arrow = new Shell.Arrow({ surface_width: MENU_ICON_SIZE / 2,
                                        surface_height: MENU_ICON_SIZE / 2,
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
 */
function AppDisplay() {
    this._init();
}

AppDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function() {
        GenericDisplay.GenericDisplay.prototype._init.call(this);

        this._menus = [];
        this._menuDisplays = [];

        // map<itemId, array of category names>
        this._appCategories = {};

        this._appMonitor = Shell.AppMonitor.get_default();
        this._appSystem = Shell.AppSystem.get_default();
        this._appsStale = true;
        this._appSystem.connect('installed-changed', Lang.bind(this, function(appSys) {
            this._appsStale = true;
            this._redisplay(false);
            this._redisplayMenus();
        }));
        this._appSystem.connect('favorites-changed', Lang.bind(this, function(appSys) {
            this._redisplay(false);
        }));
        this._appMonitor.connect('app-added', Lang.bind(this, function(monitor) {
            this._redisplay(false);
        }));
        this._appMonitor.connect('app-removed', Lang.bind(this, function(monitor) {
            this._redisplay(false);
        }));

        // Load the apps now so it doesn't slow down the first
        // transition into the Overview
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

    _addMenuItem: function(name, id, icon, index) {
        let display = new MenuItem(name, id, icon);
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
            this._redisplay(true);
        }));
        this._menuDisplay.append(display.actor, 0);
    },

    _redisplayMenus: function() {
        this._menuDisplay.remove_all();
        this._addMenuItem(_("Frequent"), null, 'gtk-select-all');
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            this._addMenuItem(menu.name, menu.id, menu.icon, i+1);
        }
    },

    _addApp: function(appInfo) {
        let appId = appInfo.get_id();
        this._allItems[appId] = appInfo;
    },

    //// Protected method overrides ////

    // Gets information about all applications by calling Gio.app_info_get_all().
    _refreshCache : function() {
        let me = this;
        if (!this._appsStale)
            return;
        this._allItems = {};
        this._appCategories = {};

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

        // Now grab the desktop file ids for settings/preferences.
        // These show up in search, but not with the rest of apps.
        let settings = this._appSystem.get_all_settings();
        for (let i = 0; i < settings.length; i++) {
            let app = settings[i];
            this._addApp(app);
        }

        this._appsStale = false;
    },

    // Stub this out; the app display always has a category selected
    _setDefaultList : function() {
        this._matchedItems = [];
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
            return this._isInfoMatchingMenu(itemInfo, search);
    },

    _isInfoMatchingMenu : function(itemInfo, search) {
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

        let categories = itemInfo.get_categories();
        for (let i = 0; i < categories.length; i++) {
            let category = fold(categories[i]);
            if (category.indexOf(search) >= 0)
                return true;
        }

        return false;
    },

    // Creates an AppDisplayItem based on itemInfo, which is expected be an Shell.AppInfo object.
    _createDisplayItem: function(itemInfo) {
        return new AppDisplayItem(itemInfo);
    }
};

Signals.addSignalMethods(AppDisplay.prototype);

function WellDisplayItem(appInfo, isFavorite) {
    this._init(appInfo, isFavorite);
}

WellDisplayItem.prototype = {
    _init : function(appInfo, isFavorite) {
        this.appInfo = appInfo;

        this.isFavorite = isFavorite;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   corner_radius: 2,
                                   border: 0,
                                   padding: 1,
                                   border_color: GenericDisplay.ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR,
                                   reactive: true });
        this.actor._delegate = this;
        this.actor.connect('button-release-event', Lang.bind(this, function (b, e) {
            this._handleActivate();
        }));

        let draggable = DND.makeDraggable(this.actor);

        let iconBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    x_align: Big.BoxAlignment.CENTER,
                                    y_align: Big.BoxAlignment.CENTER });
        this._icon = appInfo.create_icon_texture(APP_ICON_SIZE);
        iconBox.append(this._icon, Big.BoxPackFlags.NONE);

        this.actor.append(iconBox, Big.BoxPackFlags.EXPAND);

        this._windows = Shell.AppMonitor.get_default().get_windows_for_app(appInfo.get_id());

        let nameBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    x_align: Big.BoxAlignment.CENTER });
        this._nameBox = nameBox;

        this._name = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 12px",
                                        line_alignment: Pango.Alignment.CENTER,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: appInfo.get_name() });
        nameBox.append(this._name, Big.BoxPackFlags.NONE);
        if (this._windows.length > 0) {
            let glow = new Shell.DrawingArea({});
            glow.connect('redraw', Lang.bind(this, function (e, tex) {
                Shell.draw_app_highlight(tex,
                                         this._windows.length,
                                         GLOW_COLOR.red / 255,
                                         GLOW_COLOR.green / 255,
                                         GLOW_COLOR.blue / 255,
                                         GLOW_COLOR.alpha / 255);
            }));
            this._name.connect('notify::allocation', Lang.bind(this, function () {
                let x = this._name.x;
                let y = this._name.y;
                let width = this._name.width;
                let height = this._name.height;
                // If we're smaller than the allocated box width, pad out the glow a bit
                // to make it more visible
                if ((width + GLOW_PADDING * 2) < this._nameBox.width) {
                    width += GLOW_PADDING * 2;
                    x -= GLOW_PADDING;
                }
                glow.set_size(width, height);
                glow.set_position(x, y);
            }));
            nameBox.add_actor(glow);
            glow.lower(this._name);
        }
        this.actor.append(nameBox, Big.BoxPackFlags.NONE);
    },

    _handleActivate: function () {
        if (this._windows.length == 0) {
            this.appInfo.launch();
            Main.overview.hide();
        } else {
            /* Pick the first window and activate it;
             * In the future, we want to have a menu dropdown here. */
            let first = this._windows[0];
            Main.overview.activateWindow(first, Clutter.get_current_event_time());
        }
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
        this.appInfo.launch();
    },

    getDragActor: function(stageX, stageY) {
        return this.appInfo.create_icon_texture(APP_ICON_SIZE);
    },

    // Returns the original icon that is being used as a source for the cloned texture
    // that represents the item as it is being dragged.
    getDragActorSource: function() {
        return this._icon;
    },

    setWidth: function(width) {
        this._nameBox.width = width + GLOW_PADDING * 2;
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
        let spacing = Math.max(nColumns - 1, 0) * WELL_ITEM_HSPACING;
        alloc.min_size = itemMin * nColumns + spacing;
        alloc.natural_size = itemNatural * nColumns + spacing;
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

            let atSeparator = (i == this._separatorIndex - 1);

            columnIndex++;
            if (columnIndex == columns || atSeparator) {
                columnIndex = 0;
            }

            if (columnIndex == 0) {
                y += itemHeight + WELL_ITEM_VSPACING;
                x = box.x1;
            } else {
                x += itemWidth + WELL_ITEM_HSPACING;
            }

            if (atSeparator) {
                y += separatorNatural + WELL_ITEM_VSPACING;
            }
        }

        let separatorRowIndex = Math.ceil(this._separatorIndex / columns);

        /* Allocate the separator */
        let childBox = new Clutter.ActorBox();
        childBox.x1 = box.x1;
        childBox.y1 = (itemHeight + WELL_ITEM_VSPACING) * separatorRowIndex;
        this._cachedSeparatorY = childBox.y1;
        childBox.x2 = box.x2;
        childBox.y2 = childBox.y1+separatorNatural;
        this._separator.allocate(childBox, flags);
    },

    setSeparatorIndex: function (index) {
        this._separatorIndex = index;
        this.actor.queue_relayout();
    },

    removeAll: function () {
        let itemChildren = this._getItemChildren();
        for (let i = 0; i < itemChildren.length; i++) {
            itemChildren[i].destroy();
        }
        this._separatorIndex = 0;
    },

    isBeforeSeparator: function(x, y) {
        return y < this._cachedSeparatorY;
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
        let nColumns;
        if (children.length < WELL_DEFAULT_COLUMNS)
            nColumns = children.length;
         else
            nColumns = WELL_DEFAULT_COLUMNS;

        if (forWidth >= 0 && forWidth < minWidth) {
           log("WellGrid: trying to allocate for width " + forWidth + " but min is " + minWidth);
           /* FIXME - we should fall back to fewer than WELL_DEFAULT_COLUMNS here */
        }

        let horizSpacingTotal = Math.max(nColumns - 1, 0) * WELL_ITEM_HSPACING;
        let minWidth = itemMinWidth * nColumns + horizSpacingTotal;

        let lastColumnIndex = nColumns - 1;
        let separatorColumns = lastColumnIndex - ((lastColumnIndex + this._separatorIndex) % nColumns);
        let rows = Math.ceil((children.length + separatorColumns) / nColumns);

        let itemWidth;
        if (forWidth < 0) {
            itemWidth = itemNaturalWidth;
        } else {
            itemWidth = Math.max(forWidth - horizSpacingTotal, 0) / nColumns;
        }

        let itemNaturalHeight = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMin, childNatural] = children[i].get_preferred_height(itemWidth);
            if (childNatural > itemNaturalHeight)
                itemNaturalHeight = childNatural;
        }

        return [rows, WELL_DEFAULT_COLUMNS, itemWidth, itemNaturalHeight];
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

    _lookupApps: function(appIds) {
        let result = [];
        for (let i = 0; i < appIds.length; i++) {
            let id = appIds[i];
            let app = this._appSystem.lookup_cached_app(id);
            if (!app)
                continue;
            result.push(app);
        }
        return result;
    },

    _arrayValues: function(array) {
        return array.reduce(function (values, id, index) {
                            values[id] = index; return values; }, {});
    },

    _redisplay: function () {
        this._grid.removeAll();

        let favoriteIds = this._appSystem.get_favorites();
        let favoriteIdsHash = this._arrayValues(favoriteIds);

        /* hardcode here pending some design about how exactly desktop contexts behave */
        let contextId = "";

        let running = this._appMonitor.get_running_apps(contextId).filter(function (e) {
            return !(e.get_id() in favoriteIdsHash);
        });
        let favorites = this._lookupApps(favoriteIds);

        let displays = []
        this._addApps(favorites, true);
        this._grid.setSeparatorIndex(favorites.length);
        this._addApps(running, false);
        this._displays = displays;
    },

    _addApps: function(apps) {
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            let display = new WellDisplayItem(app, this.isFavorite);
            this._grid.actor.add_actor(display.actor);
        }
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let global = Shell.Global.get();

        let appSystem = Shell.AppSystem.get_default();

        let app = null;
        if (source instanceof WellDisplayItem) {
            app = source.appInfo;
        } else if (source instanceof AppDisplayItem) {
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

        let dropIsFavorite = this._grid.isBeforeSeparator(x - this._grid.actor.x,
                                                          y - this._grid.actor.y);
        let srcIsFavorite = (id in favoriteIdsObject);

        if (srcIsFavorite && (!dropIsFavorite)) {
            Mainloop.idle_add(function () {
                appSystem.remove_favorite(id);
                return false;
            });
        } else if ((!srcIsFavorite) && dropIsFavorite) {
            Mainloop.idle_add(function () {
                appSystem.add_favorite(id);
                return false;
            });
        } else {
            return false;
        }

        return true;
    }
};

Signals.addSignalMethods(AppWell.prototype);
