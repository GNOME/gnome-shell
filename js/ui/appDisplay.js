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
const St = imports.gi.St;
const Mainloop = imports.mainloop;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppFavorites = imports.ui.appFavorites;
const DND = imports.ui.dnd;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Search = imports.ui.search;
const Workspace = imports.ui.workspace;

const APPICON_SIZE = 48;
const WELL_MAX_COLUMNS = 8;

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
        let appSys = Shell.AppSystem.get_default();
        let app = appSys.get_app(this._appInfo.get_id());
        let windows = app.get_windows();
        if (windows.length > 0) {
            let mostRecentWindow = windows[0];
            Main.activateWindow(mostRecentWindow);
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

/* This class represents a display containing a collection of application items.
 * The applications are sorted based on their popularity by default, and based on
 * their name if some search filter is applied.
 *
 * showPrefs - a boolean indicating if this AppDisplay should contain preference
 *             applets, rather than applications
 */
function AppDisplay(showPrefs, flags) {
    this._init(showPrefs, flags);
}

AppDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(showPrefs, flags) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, flags);

        this._showPrefs = showPrefs;

        this._menus = [];
        this._menuDisplays = [];
        // map<search term, map<appId, true>>
        // We use a map of appIds instead of an array to ensure that we don't have duplicates and for easier lookup.
        this._menuSearchAppMatches = {};

        this._appSystem = Shell.AppSystem.get_default();
        this._appsStale = true;
        this._appSystem.connect('installed-changed', Lang.bind(this, function(appSys) {
            this._appsStale = true;
            this._redisplay(GenericDisplay.RedisplayFlags.NONE);
        }));
    },

    //// Private ////

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
            let apps = this._appSystem.get_flattened_apps();
            for (let i = 0; i < apps.length; i++) {
                let app = apps[i];
                this._addApp(app);
            }
        }

        this._appsStale = false;
        return false;
    },

    _setDefaultList : function() {
        this._matchedItems = this._allItems;
        this._matchedItemKeys = [];
        for (let itemId in this._matchedItems) {
            let app = this._allItems[itemId];
            if (app.get_is_nodisplay())
                continue;
            this._matchedItemKeys.push(itemId);
        }
        this._matchedItemKeys.sort(Lang.bind(this, this._compareItems));
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

        return false;
    },

    // Creates an AppDisplayItem based on itemInfo, which is expected be an Shell.AppInfo object.
    _createDisplayItem: function(itemInfo) {
        return new AppDisplayItem(itemInfo);
    }
};

Signals.addSignalMethods(AppDisplay.prototype);

function BaseAppSearchProvider() {
    this._init();
}

BaseAppSearchProvider.prototype = {
    __proto__: Search.SearchProvider.prototype,

    _init: function(name) {
        Search.SearchProvider.prototype._init.call(this, name);
        this._appSys = Shell.AppSystem.get_default();
    },

    getResultMeta: function(resultId) {
        let app = this._appSys.get_app(resultId);
        if (!app)
            return null;
        return { 'id': resultId,
                 'name': app.get_name(),
                 'icon': app.create_icon_texture(Search.RESULT_ICON_SIZE)};
    },

    activateResult: function(id) {
        let app = this._appSys.get_app(id);
        let windows = app.get_windows();

        if (windows.length > 0)
            Main.activateWindow(windows[0]);
        else
            app.launch();
    },

    dragActivateResult: function(id) {
        let app = this._appSys.get_app(id);
        app.launch();
    }
};

function AppSearchProvider() {
    this._init();
}

AppSearchProvider.prototype = {
    __proto__: BaseAppSearchProvider.prototype,

    _init: function() {
         BaseAppSearchProvider.prototype._init.call(this, _("APPLICATIONS"));
    },

    getInitialResultSet: function(terms) {
        return this._appSys.initial_search(false, terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._appSys.subsearch(false, previousResults, terms);
    },

    expandSearch: function(terms) {
        log("TODO expand search");
    }
}

function PrefsSearchProvider() {
    this._init();
}

PrefsSearchProvider.prototype = {
    __proto__: BaseAppSearchProvider.prototype,

    _init: function() {
        BaseAppSearchProvider.prototype._init.call(this, _("PREFERENCES"));
    },

    getInitialResultSet: function(terms) {
        return this._appSys.initial_search(true, terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._appSys.subsearch(true, previousResults, terms);
    },

    expandSearch: function(terms) {
        let controlCenter = this._appSys.load_from_desktop_file('gnomecc.desktop');
        controlCenter.launch();
        Main.overview.hide();
    }
}

function AppIcon(app) {
    this._init(app);
}

AppIcon.prototype = {
    _init : function(app) {
        this.app = app;

        this.actor = new St.Bin({ style_class: 'app-icon',
                                  x_fill: true,
                                  y_fill: true });
        this.actor._delegate = this;

        let box = new St.BoxLayout({ vertical: true });
        this.actor.set_child(box);

        this.icon = this.app.create_icon_texture(APPICON_SIZE);

        box.add(this.icon, { expand: true, x_fill: false, y_fill: false });

        this._name = new St.Label({ text: this.app.get_name() });
        this._name.clutter_text.line_alignment = Pango.Alignment.CENTER;
        box.add_actor(this._name);
    }
}

function AppWellIcon(app) {
    this._init(app);
}

AppWellIcon.prototype = {
    _init : function(app) {
        this.app = app;
        this._running = false;
        this.actor = new St.Clickable({ style_class: 'app-well-app',
                                         reactive: true,
                                         x_fill: true,
                                         y_fill: true });
        this.actor._delegate = this;

        this._icon = new AppIcon(app);
        this.actor.set_child(this._icon.actor);

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this._menu = null;

        this._draggable = DND.makeDraggable(this.actor, true);
        this._dragStartX = null;
        this._dragStartY = null;

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('notify::hover', Lang.bind(this, this._onHoverChange));
        this.actor.connect('show', Lang.bind(this, this._onShow));
        this.actor.connect('hide', Lang.bind(this, this._onHideDestroy));
        this.actor.connect('destroy', Lang.bind(this, this._onHideDestroy));

        this._appWindowChangedId = 0;
    },

    _onShow: function() {
        this._appWindowChangedId = this.app.connect('windows-changed',
                                                    Lang.bind(this,
                                                              this._updateStyleClass));
        this._updateStyleClass();
    },

    _onHideDestroy: function() {
        if (this._appWindowChangedId > 0)
            this.app.disconnect(this._appWindowChangedId);
    },

    _updateStyleClass: function() {
        let windows = this.app.get_windows();
        let running = windows.length > 0;
        if (running == this._running)
            return;
        this._running = running;
        this.actor.style_class = this._running ? "app-well-app running"
                                               : "app-well-app";
    },

    _onButtonPress: function(actor, event) {
        let [stageX, stageY] = event.get_coords();
        this._dragStartX = stageX;
        this._dragStartY = stageY;
    },

    _onHoverChange: function(actor) {
        let hover = this.actor.hover;
        if (!hover) {
            if (this.actor.pressed && this._dragStartX != null) {
                this.actor.fake_release();
                this._draggable.startDrag(this._dragStartX, this._dragStartY,
                                          global.get_current_time());
            } else {
                this._dragStartX = null;
                this._dragStartY = null;
            }
        }
    },

    _onClicked: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            this._onActivate(event);
        } else if (button == 3) {
            // Don't bind to the right click here; we want left click outside the
            // area to deactivate as well.
            this.popupMenu(0);
        }
        return false;
    },

    popupMenu: function(activatingButton) {
        if (!this._menu) {
            this._menu = new AppIconMenu(this);
            this._menu.connect('highlight-window', Lang.bind(this, function (menu, window) {
                this.highlightWindow(window);
            }));
            this._menu.connect('activate-window', Lang.bind(this, function (menu, window) {
                this.activateWindow(window);
            }));
            this._menu.connect('popup', Lang.bind(this, function (menu, isPoppedUp) {
                if (isPoppedUp) {
                    this._onMenuPoppedUp();
                } else {
                    this._onMenuPoppedDown();
                }
            }));
        }

        this._menu.popup(activatingButton);

        return false;
    },

    activateMostRecentWindow: function () {
        let mostRecentWindow = this.app.get_windows()[0];
        Main.activateWindow(mostRecentWindow);
    },

    highlightWindow: function(metaWindow) {
        if (this._didActivateWindow)
            return;
        if (!this._getRunning())
            return;
        Main.overview.getWorkspacesForWindow(metaWindow).setHighlightWindow(metaWindow);
    },

    activateWindow: function(metaWindow) {
        if (metaWindow) {
            this._didActivateWindow = true;
            Main.activateWindow(metaWindow);
        } else
            Main.overview.hide();
    },

    _onMenuPoppedUp: function() {
        if (this._getRunning()) {
            Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(this.app.get_id());
            this._setWindowSelection = true;
            this._didActivateWindow = false;
        }
    },

    _onMenuPoppedDown: function() {
        if (this._didActivateWindow)
            return;
        if (!this._setWindowSelection)
            return;

        Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(null);
        this._setWindowSelection = false;
    },

    _getRunning: function() {
        return this.app.get_windows().length > 0;
    },

    _onActivate: function (event) {
        let running = this._getRunning();

        if (!running) {
            this.app.launch();
            Main.overview.hide();
        } else {
            let modifiers = Shell.get_event_state(event);

            if (modifiers & Clutter.ModifierType.CONTROL_MASK) {
                this.app.launch();
                Main.overview.hide();
            } else {
                this.activateMostRecentWindow();
            }
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
        this.app.launch();
    },

    getDragActor: function() {
        return this.app.create_icon_texture(APPICON_SIZE);
    },

    // Returns the original actor that should align with the actor
    // we show as the item is being dragged.
    getDragActorSource: function() {
        return this._icon.icon;
    }
}
Signals.addSignalMethods(AppWellIcon.prototype);

function AppIconMenu(source) {
    this._init(source);
}

AppIconMenu.prototype = {
    _init: function(source) {
        this._source = source;

        this._arrowSize = 4; // CSS default
        this._spacing = 0; // CSS default

        this._dragStartX = 0;
        this._dragStartY = 0;

        this.actor = new Shell.GenericContainer({ reactive: true });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this._windowContainerBox = new St.Bin({ style_class: 'app-well-menu' });
        this._windowContainer = new Shell.Menu({ orientation: Big.BoxOrientation.VERTICAL,
                                                  width: Main.overview._dash.actor.width });
        this._windowContainerBox.set_child(this._windowContainer);
        this._windowContainer.connect('unselected', Lang.bind(this, this._onItemUnselected));
        this._windowContainer.connect('selected', Lang.bind(this, this._onItemSelected));
        this._windowContainer.connect('cancelled', Lang.bind(this, this._onWindowSelectionCancelled));
        this._windowContainer.connect('activate', Lang.bind(this, this._onItemActivate));
        this.actor.add_actor(this._windowContainerBox);

        // Stay popped up on release over application icon
        this._windowContainer.set_persistent_source(this._source.actor);

        // Intercept events while the menu has the pointer grab to do window-related effects
        this._windowContainer.connect('enter-event', Lang.bind(this, this._onMenuEnter));
        this._windowContainer.connect('leave-event', Lang.bind(this, this._onMenuLeave));
        this._windowContainer.connect('button-release-event', Lang.bind(this, this._onMenuButtonRelease));

        this._borderColor = new Clutter.Color();
        this._backgroundColor = new Clutter.Color();
        this._windowContainerBox.connect('style-changed', Lang.bind(this, this._onStyleChanged));

        this._arrow = new St.DrawingArea();
        this._arrow.connect('redraw', Lang.bind(this, function (area, texture) {
            Shell.draw_box_pointer(texture,
                                   Shell.PointerDirection.LEFT,
                                   this._borderColor,
                                   this._backgroundColor);
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
        let [min, natural] = this._windowContainerBox.get_preferred_width(forHeight);
        min += this._arrowSize;
        natural += this._arrowSize;
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [min, natural] = this._windowContainerBox.get_preferred_height(forWidth);
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        let themeNode = this._windowContainerBox.get_theme_node();

        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        childBox.x1 = 0;
        childBox.x2 = this._arrowSize;
        childBox.y1 = Math.floor((height / 2) - (this._arrowSize / 2));
        childBox.y2 = childBox.y1 + this._arrowSize;
        this._arrow.allocate(childBox, flags);

        // Ensure the arrow is above the border area
        let border = themeNode.get_border_width(St.Side.LEFT);
        childBox.x1 = this._arrowSize - border;
        childBox.x2 = width;
        childBox.y1 = 0;
        childBox.y2 = height;
        this._windowContainerBox.allocate(childBox, flags);
    },

    _redisplay: function() {
        this._windowContainer.remove_all();

        let windows = this._source.app.get_windows();

        this._windowContainer.show();

        let iconsDiffer = false;
        let texCache = Shell.TextureCache.get_default();
        if (windows.length > 0) {
            let firstIcon = windows[0].mini_icon;
            for (let i = 1; i < windows.length; i++) {
                if (!texCache.pixbuf_equal(windows[i].mini_icon, firstIcon)) {
                    iconsDiffer = true;
                    break;
                }
            }
        }

        // Display the app windows menu items and the separator between windows
        // of the current desktop and other windows.
        let activeWorkspace = global.screen.get_active_workspace();
        let separatorShown = windows.length > 0 && windows[0].get_workspace() != activeWorkspace;

        for (let i = 0; i < windows.length; i++) {
            if (!separatorShown && windows[i].get_workspace() != activeWorkspace) {
                this._appendSeparator();
                separatorShown = true;
            }
            let box = this._appendMenuItem(windows[i].title);
            box._window = windows[i];
        }

        if (windows.length > 0)
            this._appendSeparator();

        let isFavorite = AppFavorites.getAppFavorites().isFavorite(this._source.app.get_id());

        this._newWindowMenuItem = windows.length > 0 ? this._appendMenuItem(_("New Window")) : null;

        if (windows.length > 0)
            this._appendSeparator();
        this._toggleFavoriteMenuItem = this._appendMenuItem(isFavorite ? _("Remove from Favorites")
                                                                    : _("Add to Favorites"));

        this._highlightedItem = null;
    },

    _appendSeparator: function () {
        let bin = new St.Bin({ style_class: "app-well-menu-separator" });
        this._windowContainer.append_separator(bin, Big.BoxPackFlags.NONE);
    },

    _appendMenuItem: function(labelText) {
        let box = new St.BoxLayout({ style_class: 'app-well-menu-item',
                                      reactive: true });
        let label = new St.Label({ text: labelText });
        box.add(label);
        this._windowContainer.append(box, Big.BoxPackFlags.NONE);
        return box;
    },

    popup: function(activatingButton) {
        let [stageX, stageY] = this._source.actor.get_transformed_position();
        let [stageWidth, stageHeight] = this._source.actor.get_transformed_size();

        this._redisplay();

        this._windowContainer.popup(activatingButton, global.get_current_time());

        this.emit('popup', true);

        let x, y;
        x = Math.floor(stageX + stageWidth);
        y = Math.floor(stageY + (stageHeight / 2) - (this.actor.height / 2));

        this.actor.set_position(x, y);
        this.actor.show();
    },

    popdown: function() {
        this._windowContainer.popdown();
        this.emit('popup', false);
        this.actor.hide();
    },

    selectWindow: function(metaWindow) {
        this._selectMenuItemForWindow(metaWindow);
    },

    _findMetaWindowForActor: function (actor) {
        if (actor._delegate instanceof Workspace.WindowClone)
            return actor._delegate.metaWindow;
        else if (actor.get_meta_window)
            return actor.get_meta_window();
        return null;
    },

    // This function is called while the menu has a pointer grab; what we want
    // to do is see if the mouse was released over a window representation
    _onMenuButtonRelease: function (actor, event) {
        let metaWindow = this._findMetaWindowForActor(event.get_source());
        if (metaWindow) {
            this.emit('activate-window', metaWindow);
        }
    },

    _updateHighlight: function (item) {
        if (this._highlightedItem) {
            this._highlightedItem.set_style_pseudo_class(null);
            this.emit('highlight-window', null);
        }
        this._highlightedItem = item;
        if (this._highlightedItem) {
            item.set_style_pseudo_class('hover');
            let window = this._highlightedItem._window;
            if (window)
                this.emit('highlight-window', window);
        }
    },

    _selectMenuItemForWindow: function (metaWindow) {
        let children = this._windowContainer.get_children();
        for (let i = 0; i < children.length; i++) {
            let child = children[i];
            let menuMetaWindow = child._window;
            if (menuMetaWindow == metaWindow)
                this._updateHighlight(child);
        }
    },

    // Called while menu has a pointer grab
    _onMenuEnter: function (actor, event) {
        let metaWindow = this._findMetaWindowForActor(event.get_source());
        if (metaWindow) {
            this._selectMenuItemForWindow(metaWindow);
        }
    },

    // Called while menu has a pointer grab
    _onMenuLeave: function (actor, event) {
        let metaWindow = this._findMetaWindowForActor(event.get_source());
        if (metaWindow) {
            this._updateHighlight(null);
        }
    },

    _onItemUnselected: function (actor, child) {
        this._updateHighlight(null);
    },

    _onItemSelected: function (actor, child) {
        this._updateHighlight(child);
    },

    _onItemActivate: function (actor, child) {
        if (child._window) {
            let metaWindow = child._window;
            this.emit('activate-window', metaWindow);
        } else if (child == this._newWindowMenuItem) {
            this._source.app.launch();
            this.emit('activate-window', null);
        } else if (child == this._toggleFavoriteMenuItem) {
            let favs = AppFavorites.getAppFavorites();
            let isFavorite = favs.isFavorite(this._source.app.get_id());
            if (isFavorite)
                favs.removeFavorite(this._source.app.get_id());
            else
                favs.addFavorite(this._source.app.get_id());
        }
        this.popdown();
    },

    _onWindowSelectionCancelled: function () {
        this.emit('highlight-window', null);
        this.popdown();
    },

    _onStyleChanged: function() {
        let themeNode = this._windowContainerBox.get_theme_node();
        let [success, len] = themeNode.get_length('-shell-arrow-size', false);
        if (success) {
            this._arrowSize = len;
            this.actor.queue_relayout();
        }
        [success, len] = themeNode.get_length('-shell-menu-spacing', false)
        if (success) {
            this._windowContainer.spacing = len;
        }
        let color = new Clutter.Color();
        if (themeNode.get_background_color(color)) {
            this._backgroundColor = color;
            color = new Clutter.Color();
        }
        if (themeNode.get_border_color(St.Side.LEFT, color)) {
            this._borderColor = color;
        }
        this._arrow.emit_redraw();
    }
};
Signals.addSignalMethods(AppIconMenu.prototype);

function WellGrid() {
    this._init();
}

WellGrid.prototype = {
    _init: function() {
        this.actor = new St.Bin({ name: "dashAppWell" });
        // Pulled from CSS, but hardcode some defaults here
        this._spacing = 0;
        this._item_size = 48;
        this._grid = new Shell.GenericContainer();
        this.actor.set_child(this._grid);
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));

        this._grid.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._grid.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._grid.connect('allocate', Lang.bind(this, this._allocate));
    },

    _getPreferredWidth: function (grid, forHeight, alloc) {
        let children = this._grid.get_children();
        let nColumns = children.length;
        let totalSpacing = Math.max(0, nColumns - 1) * this._spacing;
        // Kind of a lie, but not really an issue right now.  If
        // we wanted to support some sort of hidden/overflow that would
        // need higher level design
        alloc.min_size = this._item_size;
        alloc.natural_size = nColumns * this._item_size + totalSpacing;
    },

    _getPreferredHeight: function (grid, forWidth, alloc) {
        let children = this._grid.get_children();
        let [nColumns, usedWidth] = this._computeLayout(forWidth);
        let nRows;
        if (nColumns > 0)
            nRows = Math.ceil(children.length / nColumns);
        else
            nRows = 0;
        let totalSpacing = Math.max(0, nRows - 1) * this._spacing;
        let height = nRows * this._item_size + totalSpacing;
        alloc.min_size = height;
        alloc.natural_size = height;
    },

    _allocate: function (grid, box, flags) {
        let children = this._grid.get_children();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [nColumns, usedWidth] = this._computeLayout(availWidth);

        let overallPaddingX = Math.floor((availWidth - usedWidth) / 2);

        let x = box.x1 + overallPaddingX;
        let y = box.y1;
        let columnIndex = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinWidth, childMinHeight, childNaturalWidth, childNaturalHeight]
                = children[i].get_preferred_size();

            /* Center the item in its allocation horizontally */
            let width = Math.min(this._item_size, childNaturalWidth);
            let childXSpacing = Math.max(0, width - childNaturalWidth) / 2;
            let height = Math.min(this._item_size, childNaturalHeight);
            let childYSpacing = Math.max(0, height - childNaturalHeight) / 2;

            let childBox = new Clutter.ActorBox();
            childBox.x1 = Math.floor(x + childXSpacing);
            childBox.y1 = Math.floor(y + childYSpacing);
            childBox.x2 = childBox.x1 + width;
            childBox.y2 = childBox.y1 + height;
            children[i].allocate(childBox, flags);

            columnIndex++;
            if (columnIndex == nColumns) {
                columnIndex = 0;
            }

            if (columnIndex == 0) {
                y += this._item_size + this._spacing;
                x = box.x1 + overallPaddingX;
            } else {
                x += this._item_size + this._spacing;
            }
        }
    },

    _computeLayout: function (forWidth) {
        let children = this._grid.get_children();
        let nColumns = 0;
        let usedWidth = 0;
        while (nColumns < WELL_MAX_COLUMNS &&
                nColumns < children.length &&
                (usedWidth + this._item_size <= forWidth)) {
            usedWidth += this._item_size + this._spacing;
            nColumns += 1;
        }

        if (nColumns > 0)
            usedWidth -= this._spacing;

        return [nColumns, usedWidth];
    },

    _onStyleChanged: function() {
        let themeNode = this.actor.get_theme_node();
        let [success, len] = themeNode.get_length('spacing', false);
        if (success)
            this._spacing = len;
        [success, len] = themeNode.get_length('-shell-grid-item-size', false);
        if (success)
            this._item_size = len;
        this._grid.queue_relayout();
    },

    removeAll: function () {
        this._grid.get_children().forEach(Lang.bind(this, function (child) {
            child.destroy();
        }));
    },

    addItem: function(actor) {
        this._grid.add_actor(actor);
    }
}

function AppWell() {
    this._init();
}

AppWell.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];

        this._favorites = [];

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   x_align: Big.BoxAlignment.CENTER });
        this.actor._delegate = this;

        this._workId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));

        this._grid = new WellGrid();
        this.actor.append(this._grid.actor, Big.BoxPackFlags.EXPAND);

        this._tracker = Shell.WindowTracker.get_default();
        this._appSystem = Shell.AppSystem.get_default();

        this._appSystem.connect('installed-changed', Lang.bind(this, this._queueRedisplay));
        AppFavorites.getAppFavorites().connect('changed', Lang.bind(this, this._queueRedisplay));
        this._tracker.connect('app-running-changed', Lang.bind(this, this._queueRedisplay));
    },

    _appIdListToHash: function(apps) {
        let ids = {};
        for (let i = 0; i < apps.length; i++)
            ids[apps[i].get_id()] = apps[i];
        return ids;
    },

    _queueRedisplay: function () {
        Main.queueDeferredWork(this._workId);
    },

    _redisplay: function () {
        this._grid.removeAll();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        /* hardcode here pending some design about how exactly desktop contexts behave */
        let contextId = "";

        let running = this._tracker.get_running_apps(contextId);
        let runningIds = this._appIdListToHash(running);

        let nFavorites = 0;
        for (let id in favorites) {
            let app = favorites[id];
            let display = new AppWellIcon(app);
            this._grid.addItem(display.actor);
            nFavorites++;
        }

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            if (app.get_id() in favorites)
                continue;
            let display = new AppWellIcon(app);
            this._grid.addItem(display.actor);
        }

        if (running.length == 0 && nFavorites == 0) {
            let text = new St.Label({ text: _("Drag here to add favorites")});
            this._grid.actor.set_child(text);
        }
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplayItem) {
            app = this._appSystem.get_app(source.getId());
        } else if (source instanceof Workspace.WindowClone) {
            app = this._tracker.get_window_app(source.metaWindow);
        }

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient()) {
            return false;
        }

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite) {
            return false;
        } else {
            Mainloop.idle_add(Lang.bind(this, function () {
                AppFavorites.getAppFavorites().addFavorite(id);
                return false;
            }));
        }

        return true;
    }
};

Signals.addSignalMethods(AppWell.prototype);
