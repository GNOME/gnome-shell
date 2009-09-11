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
const WELL_ITEM_HSPACING = 0;
const WELL_ITEM_VSPACING = 4;

const WELL_MENU_POPUP_TIMEOUT_MS = 600;

const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

const WELL_MENU_BACKGROUND_COLOR = new Clutter.Color();
WELL_MENU_BACKGROUND_COLOR.from_pixel(0x292929ff);
const WELL_MENU_FONT = 'Sans 14px';
const WELL_MENU_COLOR = new Clutter.Color();
WELL_MENU_COLOR.from_pixel(0xffffffff);
const WELL_MENU_SELECTED_COLOR = new Clutter.Color();
WELL_MENU_SELECTED_COLOR.from_pixel(0x005b97ff);
const WELL_MENU_SEPARATOR_COLOR = new Clutter.Color();
WELL_MENU_SEPARATOR_COLOR.from_pixel(0x787878ff);
const WELL_MENU_BORDER_WIDTH = 1;
const WELL_MENU_ARROW_SIZE = 12;
const WELL_MENU_CORNER_RADIUS = 4;
const WELL_MENU_PADDING = 4;

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
        let windows = Shell.AppMonitor.get_default().get_windows_for_app(this._appInfo.get_id());
        if (windows.length > 0) {
            let mostRecentWindow = windows[0];
            Main.overview.activateWindow(mostRecentWindow, Clutter.get_current_event_time());
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
        if (!this._appsStale)
            return;
        this._allItems = {};
        this._appCategories = {};

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

function WellMenu(source) {
    this._init(source);
}

WellMenu.prototype = {
    _init: function(source) {
        this._source = source;

        // Whether or not we successfully picked a window
        this.didActivateWindow = false;

        this.actor = new Shell.GenericContainer({ reactive: true });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this._windowContainer = new Shell.Menu({ orientation: Big.BoxOrientation.VERTICAL,
                                                 border_color: AppIcon.APPICON_BORDER_COLOR,
                                                 border: WELL_MENU_BORDER_WIDTH,
                                                 background_color: WELL_MENU_BACKGROUND_COLOR,
                                                 padding: 4,
                                                 corner_radius: WELL_MENU_CORNER_RADIUS,
                                                 width: Main.overview._dash.actor.width * 0.75 });
        this._windowContainer.connect('unselected', Lang.bind(this, this._onItemUnselected));
        this._windowContainer.connect('selected', Lang.bind(this, this._onItemSelected));
        this._windowContainer.connect('cancelled', Lang.bind(this, this._onWindowSelectionCancelled));
        this._windowContainer.connect('activate', Lang.bind(this, this._onItemActivate));
        this.actor.add_actor(this._windowContainer);

        // Stay popped up on release over application icon
        this._windowContainer.set_persistent_source(this._source.actor);

        // Intercept events while the menu has the pointer grab to do window-related effects
        this._windowContainer.connect('enter-event', Lang.bind(this, this._onMenuEnter));
        this._windowContainer.connect('leave-event', Lang.bind(this, this._onMenuLeave));
        this._windowContainer.connect('button-release-event', Lang.bind(this, this._onMenuButtonRelease));

        this._arrow = new Shell.DrawingArea();
        this._arrow.connect('redraw', Lang.bind(this, function (area, texture) {
            Shell.draw_box_pointer(texture, AppIcon.APPICON_BORDER_COLOR, WELL_MENU_BACKGROUND_COLOR);
        }));
        this.actor.add_actor(this._arrow);

        // Chain our visibility and lifecycle to that of the source
        source.actor.connect('notify::mapped', Lang.bind(this, function () {
            if (!source.actor.mapped)
                this._windowContainer.popdown();
        }));
        source.actor.connect('destroy', Lang.bind(this, function () { this.actor.destroy(); }));

        global.stage.add_actor(this.actor);
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [min, natural] = this._windowContainer.get_preferred_width(forHeight);
        alloc.min_size = min + WELL_MENU_ARROW_SIZE;
        alloc.natural_size = natural + WELL_MENU_ARROW_SIZE;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [min, natural] = this._windowContainer.get_preferred_height(forWidth);
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();

        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        childBox.x1 = 0;
        childBox.x2 = WELL_MENU_ARROW_SIZE;
        childBox.y1 = Math.floor((height / 2) - (WELL_MENU_ARROW_SIZE / 2));
        childBox.y2 = childBox.y1 + WELL_MENU_ARROW_SIZE;
        this._arrow.allocate(childBox, flags);

        /* overlap by one pixel to hide the border */
        childBox.x1 = WELL_MENU_ARROW_SIZE - 1;
        childBox.x2 = width;
        childBox.y1 = 0;
        childBox.y2 = height;
        this._windowContainer.allocate(childBox, flags);
    },

    _redisplay: function() {
        this._windowContainer.remove_all();

        this.didActivateWindow = false;

        let windows = this._source.windows;

        this._windowContainer.show();

        let iconsDiffer = false;
        let texCache = Shell.TextureCache.get_default();
        let firstIcon = windows[0].mini_icon;
        for (let i = 1; i < windows.length; i++) {
            if (!texCache.pixbuf_equal(windows[i].mini_icon, firstIcon)) {
                iconsDiffer = true;
                break;
            }
        }

        let activeWorkspace = global.screen.get_active_workspace();

        let currentWorkspaceWindows = windows.filter(function (w) {
            return w.get_workspace() == activeWorkspace;
        });
        let otherWorkspaceWindows = windows.filter(function (w) {
            return w.get_workspace() != activeWorkspace;
        });

        this._appendWindows(currentWorkspaceWindows, iconsDiffer);
        if (currentWorkspaceWindows.length > 0 && otherWorkspaceWindows.length > 0) {
            this._appendSeparator();
        }
        this._appendWindows(otherWorkspaceWindows, iconsDiffer);

        if (!this._source.appInfo.is_transient()) {
            this._appendSeparator();

            this._newWindowMenuItem = this._appendMenuItem(null, _("New Window"));
        }
    },

    _appendSeparator: function () {
        let box = new Big.Box({ padding_top: 2, padding_bottom: 2 });
        box.append(new Clutter.Rectangle({ height: 1,
                                           color: WELL_MENU_SEPARATOR_COLOR }),
                   Big.BoxPackFlags.EXPAND);
        this._windowContainer.append_separator(box, Big.BoxPackFlags.NONE);
    },

    _appendMenuItem: function(iconTexture, labelText) {
        /* Use padding here rather than spacing in the box above so that
         * we have a larger reactive area.
         */
        let box = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                padding_top: 4,
                                padding_bottom: 4,
                                spacing: 4,
                                reactive: true });
        let vCenter;
        if (iconTexture != null) {
            vCenter = new Big.Box({ y_align: Big.BoxAlignment.CENTER });
            vCenter.append(iconTexture, Big.BoxPackFlags.NONE);
            box.append(vCenter, Big.BoxPackFlags.NONE);
        }
        vCenter = new Big.Box({ y_align: Big.BoxAlignment.CENTER });
        let label = new Clutter.Text({ text: labelText,
                                       font_name: WELL_MENU_FONT,
                                       ellipsize: Pango.EllipsizeMode.END,
                                       color: WELL_MENU_COLOR });
        vCenter.append(label, Big.BoxPackFlags.NONE);
        box.append(vCenter, Big.BoxPackFlags.NONE);
        this._windowContainer.append(box, Big.BoxPackFlags.NONE);
        return box;
    },

    _appendWindows: function (windows, iconsDiffer) {
        for (let i = 0; i < windows.length; i++) {
            let metaWindow = windows[i];

            let icon = null;
            if (iconsDiffer) {
                icon = Shell.TextureCache.get_default().bind_pixbuf_property(metaWindow, "mini-icon");
            }
            let box = this._appendMenuItem(icon, metaWindow.title);
            box._window = metaWindow;
        }
    },

    popup: function() {
        let [stageX, stageY] = this._source.actor.get_transformed_position();
        let [stageWidth, stageHeight] = this._source.actor.get_transformed_size();

        this._redisplay();

        this._windowContainer.popup(0, Clutter.get_current_event_time());

        this.emit('popup', true);

        let x = Math.floor(stageX + stageWidth);
        let y = Math.floor(stageY + (stageHeight / 2) - (this.actor.height / 2));
        this.actor.set_position(x, y);
        this.actor.show();
    },

    _findWindowCloneForActor: function (actor) {
        if (actor._delegate instanceof Workspaces.WindowClone)
            return actor._delegate;
        return null;
    },

    // This function is called while the menu has a pointer grab; what we want
    // to do is see if the mouse was released over a window clone actor
    _onMenuButtonRelease: function (actor, event) {
        let clone = this._findWindowCloneForActor(event.get_source());
        if (clone) {
            this.didActivateWindow = true;
            Main.overview.activateWindow(clone.metaWindow, event.get_time());
        }
    },

    _lookupMenuItemForWindow: function (metaWindow) {
        let children = this._windowContainer.get_children();
        for (let i = 0; i < children.length; i++) {
            let child = children[i];
            let menuMetaWindow = child._window;
            if (menuMetaWindow == metaWindow)
                return child;
        }
        return null;
    },

    // Called while menu has a pointer grab
    _onMenuEnter: function (actor, event) {
        let clone = this._findWindowCloneForActor(event.get_source());
        if (clone) {
            let menu = this._lookupMenuItemForWindow(clone.metaWindow);
            menu.background_color = WELL_MENU_SELECTED_COLOR;
            this.emit('highlight-window', clone.metaWindow);
        }
    },

    // Called while menu has a pointer grab
    _onMenuLeave: function (actor, event) {
        let clone = this._findWindowCloneForActor(event.get_source());
        if (clone) {
            let menu = this._lookupMenuItemForWindow(clone.metaWindow);
            menu.background_color = TRANSPARENT_COLOR;
            this.emit('highlight-window', null);
        }
    },

    _onItemUnselected: function (actor, child) {
        child.background_color = TRANSPARENT_COLOR;
        if (child._window) {
            this.emit('highlight-window', null);
        }
    },

    _onItemSelected: function (actor, child) {
        child.background_color = WELL_MENU_SELECTED_COLOR;
        if (child._window) {
            this.emit('highlight-window', child._window);
        }
    },

    _onItemActivate: function (actor, child) {
        if (child._window) {
            let metaWindow = child._window;
            this.didActivateWindow = true;
            Main.overview.activateWindow(metaWindow, Clutter.get_current_event_time());
        } else if (child == this._newWindowMenuItem) {
            this._source.appInfo.launch();
            Main.overview.hide();
        }
        this.emit('popup', false);
        this.actor.hide();
    },

    _onWindowSelectionCancelled: function () {
        this.emit('highlight-window', null);
        this.emit('popup', false);
        this.actor.hide();
    }
}

Signals.addSignalMethods(WellMenu.prototype);

function BaseWellItem(appInfo, isFavorite) {
    this._init(appInfo, isFavorite);
}

BaseWellItem.prototype = {
    __proto__: AppIcon.AppIcon.prototype,

    _init: function(appInfo, isFavorite) {
        AppIcon.AppIcon.prototype._init.call(this, appInfo);

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
                                              Clutter.get_current_event_time());
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
        this.appInfo.launch();
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

function RunningWellItem(appInfo, isFavorite) {
    this._init(appInfo, isFavorite);
}

RunningWellItem.prototype = {
    __proto__: BaseWellItem.prototype,

    _init: function(appInfo, isFavorite) {
        BaseWellItem.prototype._init.call(this, appInfo, isFavorite);

        this._menuTimeoutId = 0;
        this._menu = null;
        this._dragStartX = 0;
        this._dragStartY = 0;

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('notify::hover', Lang.bind(this, this._onHoverChanged));
        this.actor.connect('activate', Lang.bind(this, this._onActivate));
    },

    _onActivate: function (actor, event) {
        let modifiers = event.get_state();

        if (this._menuTimeoutId > 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }

        if (modifiers & Clutter.ModifierType.CONTROL_MASK) {
            this.appInfo.launch();
        } else {
            this.activateMostRecentWindow();
        }
    },

    activateMostRecentWindow: function () {
        // The _get_windows_for_app sorts them for us
        let mostRecentWindow = this.windows[0];
        Main.overview.activateWindow(mostRecentWindow, Clutter.get_current_event_time());
    },

    _onHoverChanged: function() {
        let hover = this.actor.hover;
        if (!hover && this._menuTimeoutId > 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
    },

    _onButtonPress: function(actor, event) {
        if (this._menuTimeoutId > 0)
            Mainloop.source_remove(this._menuTimeoutId);
        this._menuTimeoutId = Mainloop.timeout_add(WELL_MENU_POPUP_TIMEOUT_MS,
                                                   Lang.bind(this, this._popupMenu));
        return false;
    },

    _popupMenu: function() {
        this._menuTimeoutId = 0;

        this.actor.fake_release();

        if (this._menu == null) {
            this._menu = new WellMenu(this);
            this._menu.connect('highlight-window', Lang.bind(this, function (menu, metaWindow) {
                Main.overview.getWorkspacesForWindow(metaWindow).setHighlightWindow(metaWindow);
            }));
            this._menu.connect('popup', Lang.bind(this, function (menu, isPoppedUp) {
                let id;

                // If we successfully picked a window, don't reset the workspace
                // state, since picking a window already did that.
                if (!isPoppedUp && menu.didActivateWindow)
                    return;
                if (isPoppedUp)
                    id = this.appInfo.get_id();
                else
                    id = null;

                Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(id);
            }));
        }

        this._menu.popup();

        return false;
    }
}

function InactiveWellItem(appInfo, isFavorite) {
    this._init(appInfo, isFavorite);
}

InactiveWellItem.prototype = {
    __proto__: BaseWellItem.prototype,

    _init : function(appInfo, isFavorite) {
        BaseWellItem.prototype._init.call(this, appInfo, isFavorite);

        this.actor.connect('notify::pressed', Lang.bind(this, this._onPressedChanged));
        this.actor.connect('activate', Lang.bind(this, this._onActivate));
    },

    _onPressedChanged: function() {
        this.setHighlight(this.actor.pressed);
    },

    _onActivate: function() {
        if (this.windows.length == 0) {
            this.appInfo.launch();
            Main.overview.hide();
            return true;
        }
        return false;
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

    _addApps: function(apps, isFavorite) {
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            let windows = this._appMonitor.get_windows_for_app(app.get_id());
            let display;
            if (windows.length > 0)
                display = new RunningWellItem(app, isFavorite);
            else
                display = new InactiveWellItem(app, isFavorite);
            this._grid.actor.add_actor(display.actor);
        }
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let appSystem = Shell.AppSystem.get_default();

        let app = null;
        if (source instanceof BaseWellItem) {
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
