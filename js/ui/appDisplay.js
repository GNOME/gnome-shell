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
const APP_PADDING = 18;

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
        this._appMonitor.connect('changed', Lang.bind(this, function(monitor) {
            this._redisplay(false);
        }));

        // Load the apps now so it doesn't slow down the first
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
            return this._appSystem.lookup_app(id + '.desktop');
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
        this._addMenuItem('Frequent', null, 'gtk-select-all');
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

        this._menus = this._appSystem.get_menus();
        // Loop over the toplevel menu items, load the set of desktop file ids
        // associated with each one
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            let menuApps = this._appSystem.get_applications_for_menu(menu.id);
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
        this.actor.connect('enter-event', Lang.bind(this,
            function(o, event) {
                this.actor.border = 1;
                this.actor.padding = 0;
                return false;
            }));
        this.actor.connect('leave-event', Lang.bind(this,
            function(o, event) {
                this.actor.border = 0;
                this.actor.padding = 1;
                return false;
            }));
        this.actor._delegate = this;
        this.actor.connect('button-release-event', Lang.bind(this, function (b, e) {
            this._handleActivate();
        }));

        let draggable = DND.makeDraggable(this.actor);

        let iconBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    x_align: Big.BoxAlignment.CENTER });
        this._icon = appInfo.create_icon_texture(APP_ICON_SIZE);
        iconBox.append(this._icon, Big.BoxPackFlags.NONE);

        this.actor.append(iconBox, Big.BoxPackFlags.NONE);

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
                Shell.draw_glow(tex,
                                GLOW_COLOR.red / 255,
                                GLOW_COLOR.green / 255,
                                GLOW_COLOR.blue / 255,
                                GLOW_COLOR.alpha / 255);
            }));
            this._name.connect('notify::allocation', Lang.bind(this, function (n, alloc) {
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
        if (this._windows.length == 0)
            this.launch();
        else {
            /* Pick the first window and activate it;
             * In the future, we want to have a menu dropdown here. */
            let first = this._windows[0];
            Main.overlay.activateWindow (first, Clutter.get_current_event_time());
        }
        this.emit('activated');
    },

    // Opens an application represented by this display item.
    launch : function() {
        this.appInfo.launch();
    },

    // Draggable interface - FIXME deduplicate with GenericDisplay
    getDragActor: function(stageX, stageY) {
        this.dragActor = this.appInfo.create_icon_texture(APP_ICON_SIZE);

        // If the user dragged from the icon itself, then position
        // the dragActor over the original icon. Otherwise center it
        // around the pointer
        let [iconX, iconY] = this._icon.get_transformed_position();
        let [iconWidth, iconHeight] = this._icon.get_transformed_size();
        if (stageX > iconX && stageX <= iconX + iconWidth &&
            stageY > iconY && stageY <= iconY + iconHeight)
            this.dragActor.set_position(iconX, iconY);
        else
            this.dragActor.set_position(stageX - this.dragActor.width / 2, stageY - this.dragActor.height / 2);
        return this.dragActor;
    },

    // Returns the original icon that is being used as a source for the cloned texture
    // that represents the item as it is being dragged.
    getDragActorSource: function() {
        return this._icon;
    },

    getWordWidth: function() {
        return this._wordWidth;
    },

    setWidth: function(width) {
        this._nameBox.width = width + GLOW_PADDING * 2;
    }
};

Signals.addSignalMethods(WellDisplayItem.prototype);

function WellArea(width, isFavorite) {
    this._init(width, isFavorite);
}

WellArea.prototype = {
    _init : function(width, isFavorite) {
        this.isFavorite = isFavorite;

        this._grid = new Tidy.Grid({ width: width, row_gap: 4 });
        this._grid._delegate = this;

        this.actor = this._grid;
    },

    redisplay: function (infos) {
        let children;

        children = this._grid.get_children();
        children.forEach(Lang.bind(this, function (v) {
            v.destroy();
        }));

        this._maxWordWidth = 0;
        let displays = []
        for (let i = 0; i < infos.length; i++) {
            let app = infos[i];
            let display = new WellDisplayItem(app, this.isFavorite);
            displays.push(display);
            let width = display.getWordWidth();
            if (width > this._maxWordWidth)
                this._maxWordWidth = width;
            display.connect('activated', Lang.bind(this, function (display) {
                this.emit('activated', display);
            }));
            this.actor.add_actor(display.actor);
        }
        this._displays = displays;
    },

    getWordWidth: function() {
        return this._maxWordWidth;
    },

    setItemWidth: function(width) {
        for (let i = 0; i < this._displays.length; i++) {
            let display = this._displays[i];
            display.setWidth(width);
        }
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let global = Shell.Global.get();

        let id = null;
        if (source instanceof WellDisplayItem) {
            id = source.appInfo.get_id();
        } else if (source instanceof AppDisplayItem) {
            id = source.getId();
        } else if (source instanceof Workspaces.WindowClone) {
            let appMonitor = Shell.AppMonitor.get_default();
            let app = appMonitor.get_window_app(source.metaWindow);
            id = app.get_id();
            if (id === null)
                return false;
        } else {
            return false;
        }

        let appSystem = Shell.AppSystem.get_default();

        if (source.isFavorite && (!this.isFavorite)) {
            Mainloop.idle_add(function () {
                appSystem.remove_favorite(id);
            });
        } else if ((!source.isFavorite) && this.isFavorite) {
            Mainloop.idle_add(function () {
                appSystem.add_favorite(id);
            });
        } else {
            return false;
        }

        return true;
    }
}

Signals.addSignalMethods(WellArea.prototype);

function AppWell(width) {
    this._init(width);
}

AppWell.prototype = {
    _init : function(width) {
        this._menus = [];
        this._menuDisplays = [];

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   spacing: 4,
                                   width: width });

        this._appSystem = Shell.AppSystem.get_default();
        this._appMonitor = Shell.AppMonitor.get_default();

        this._appSystem.connect('installed-changed', Lang.bind(this, function(appSys) {
            this._redisplay();
        }));
        this._appSystem.connect('favorites-changed', Lang.bind(this, function(appSys) {
            this._redisplay();
        }));
        this._appMonitor.connect('changed', Lang.bind(this, function(monitor) {
            this._redisplay();
        }));

        this._favoritesArea = new WellArea(width, true);
        this._favoritesArea.connect('activated', Lang.bind(this, function (a, display) {
            this.emit('activated');
        }));
        this.actor.append(this._favoritesArea.actor, Big.BoxPackFlags.NONE);

        this._runningBox = new Big.Box({ border_color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                         border_top: 1,
                                         corner_radius: 3,
                                         padding_top: GenericDisplay.PREVIEW_BOX_PADDING });
        this._runningArea = new WellArea(width, false);
        this._runningArea.connect('activated', Lang.bind(this, function (a, display) {
            this.emit('activated');
        }));
        this._runningBox.append(this._runningArea.actor, Big.BoxPackFlags.EXPAND);
        this.actor.append(this._runningBox, Big.BoxPackFlags.NONE);

        this._redisplay();
    },

    _lookupApps: function(appIds) {
        let result = [];
        for (let i = 0; i < appIds.length; i++) {
            let id = appIds[i];
            let app = this._appSystem.lookup_app(id);
            if (!app)
                continue;
            result.push(app);
        }
        return result;
    },

    _redisplay: function() {
        /* hardcode here pending some design about how exactly activities behave */
        let contextId = "";

        let arrayToObject = function(a) {
            let o = {};
            for (let i = 0; i < a.length; i++)
                o[a[i]] = 1;
            return o;
        };
        let favoriteIds = this._appSystem.get_favorites();
        let favoriteIdsObject = arrayToObject(favoriteIds);

        let runningIds = this._appMonitor.get_running_app_ids(contextId).filter(function (e) {
            return !(e in favoriteIdsObject);
        });
        let favorites = this._lookupApps(favoriteIds);
        let running = this._lookupApps(runningIds);
        this._favoritesArea.redisplay(favorites);
        this._runningArea.redisplay(running);
        let maxWidth = this._favoritesArea.getWordWidth();
        if (this._runningArea.getWordWidth() > maxWidth)
            maxWidth = this._runningArea.getWordWidth();
        this._favoritesArea.setItemWidth(maxWidth);
        this._runningArea.setItemWidth(maxWidth);
        // If it's empty, we hide it so the top border doesn't show up
        if (running.length == 0)
          this._runningBox.hide();
        else
          this._runningBox.show();
    }
};

Signals.addSignalMethods(AppWell.prototype);
