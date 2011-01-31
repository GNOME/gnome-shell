/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
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
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const PopupMenu = imports.ui.popupMenu;
const Search = imports.ui.search;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const Params = imports.misc.params;

const MENU_POPUP_TIMEOUT = 600;

function AlphabeticalView() {
    this._init();
}

AlphabeticalView.prototype = {
    _init: function() {
        this._grid = new IconGrid.IconGrid({ xAlign: St.Align.START });
        this._appSystem = Shell.AppSystem.get_default();

        this._filterApp = null;

        let box = new St.BoxLayout({ vertical: true });
        box.add(this._grid.actor, { y_align: St.Align.START, expand: true });

        this.actor = new St.ScrollView({ x_fill: true,
                                         y_fill: false,
                                         y_align: St.Align.START,
                                         vfade: true });
        this.actor.add_actor(box);
        this.actor.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        this.actor.connect('notify::mapped', Lang.bind(this,
            function() {
                if (!this.actor.mapped)
                    return;

                let adjustment = this.actor.vscroll.adjustment;
                let direction = Overview.SwipeScrollDirection.VERTICAL;
                Main.overview.setScrollAdjustment(adjustment, direction);
            }));
    },

    _removeAll: function() {
        this._grid.removeAll();
        this._apps = [];
    },

    _addApp: function(appInfo) {
        let appIcon = new AppWellIcon(this._appSystem.get_app(appInfo.get_id()));

        this._grid.addItem(appIcon.actor);

        appIcon._appInfo = appInfo;
        if (this._filterApp && !this._filterApp(appInfo))
            appIcon.actor.hide();

        this._apps.push(appIcon);
    },

    setFilter: function(filter) {
        this._filterApp = filter;
        for (let i = 0; i < this._apps.length; i++)
            this._apps[i].actor.visible = filter(this._apps[i]._appInfo);
    },

    refresh: function(apps) {
        let ids = [];
        for (let i in apps)
            ids.push(i);
        ids.sort(function(a, b) {
            return apps[a].get_name().localeCompare(apps[b].get_name());
        });

        this._removeAll();

        for (let i = 0; i < ids.length; i++) {
            this._addApp(apps[ids[i]]);
        }
    }
};

function ViewByCategories() {
    this._init();
}

ViewByCategories.prototype = {
    _init: function() {
        this._appSystem = Shell.AppSystem.get_default();
        this.actor = new St.BoxLayout({ style_class: 'all-app' });
        this.actor._delegate = this;

        this._view = new AlphabeticalView();

        this._filters = new St.BoxLayout({ vertical: true });
        this.actor.add(this._view.actor, { expand: true, x_fill: true, y_fill: true });
        this.actor.add(this._filters, { expand: false, y_fill: false, y_align: St.Align.START });

        this._sections = [];
    },

    _selectCategory: function(num) {
        if (num != -1)
            this._allFilter.remove_style_pseudo_class('selected');
        else
            this._allFilter.add_style_pseudo_class('selected');

        this._view.setFilter(Lang.bind(this, function(app) {
            if (num == -1)
                return true;
            return this._sections[num].name == app.get_section();
        }));

        for (let i = 0; i < this._sections.length; i++) {
            if (i == num)
                this._sections[i].filterActor.add_style_pseudo_class('selected');
            else
                this._sections[i].filterActor.remove_style_pseudo_class('selected');
        }
    },

    _addFilter: function(name, num) {
        let button = new St.Button({ label: name,
                                     style_class: 'app-filter',
                                     x_align: St.Align.START });
        this._filters.add(button, { expand: true, x_fill: true, y_fill: false });
        button.connect('clicked', Lang.bind(this, function() {
            this._selectCategory(num);
        }));

        if (num != -1)
            this._sections[num] = { filterActor: button,
                                    name: name };
        else
            this._allFilter = button;
    },

    _removeAll: function() {
        this._sections = [];
        this._filters.destroy_children();
    },

    refresh: function(apps) {
        this._removeAll();

        let sections = this._appSystem.get_sections();
        this._apps = apps;
        this._view.refresh(apps);

        /* Translators: Filter to display all applications */
        this._addFilter(_("All"), -1);

        if (!sections)
            return;

        for (let i = 0; i < sections.length; i++)
            this._addFilter(sections[i], i);

        this._selectCategory(-1);
    }
};

/* This class represents a display containing a collection of application items.
 * The applications are sorted based on their name.
 */
function AllAppDisplay() {
    this._init();
}

AllAppDisplay.prototype = {
    _init: function() {
        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('installed-changed', Lang.bind(this, function() {
            Main.queueDeferredWork(this._workId);
        }));

        this._appView = new ViewByCategories();
        this.actor = new St.Bin({ child: this._appView.actor, x_fill: true, y_fill: true });

        this._workId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));
    },

    _redisplay: function() {
        let apps = this._appSystem.get_flattened_apps().filter(function(app) {
            return !app.get_is_nodisplay();
        });

        this._appView.refresh(apps);
    }
};

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
        app.activate();
    },

    dragActivateResult: function(id) {
        let app = this._appSys.get_app(id);
        app.open_new_window();
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

    createResultActor: function (resultMeta, terms) {
        let app = this._appSys.get_app(resultMeta['id']);
        let icon = new AppWellIcon(app);
        return icon.actor;
    },

    expandSearch: function(terms) {
        log('TODO expand search');
    }
};

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
};

function AppIcon(app, params) {
    this._init(app, params);
}

AppIcon.prototype = {
    __proto__:  IconGrid.BaseIcon.prototype,

    _init : function(app, params) {
        this.app = app;

        let label = this.app.get_name();

        IconGrid.BaseIcon.prototype._init.call(this,
                                               label,
                                               params);
    },

    createIcon: function(iconSize) {
        return this.app.create_icon_texture(iconSize);
    }
};

function AppWellIcon(app, iconParams) {
    this._init(app, iconParams);
}

AppWellIcon.prototype = {
    _init : function(app, iconParams) {
        this.app = app;
        this.actor = new St.Clickable({ style_class: 'app-well-app',
                                         reactive: true,
                                         x_fill: true,
                                         y_fill: true });
        this.actor._delegate = this;

        this.icon = new AppIcon(app, iconParams);
        this.actor.set_child(this.icon.actor);

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));

        this._menu = null;
        this._menuManager = new PopupMenu.PopupMenuManager(this);

        this._draggable = DND.makeDraggable(this.actor);
        this._draggable.connect('drag-begin', Lang.bind(this,
            function () {
                this._removeMenuTimeout();
                Main.overview.beginItemDrag(this);
            }));
        this._draggable.connect('drag-end', Lang.bind(this,
            function () {
               Main.overview.endItemDrag(this);
            }));

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._menuTimeoutId = 0;
        this._stateChangedId = this.app.connect('notify::state',
                                                Lang.bind(this,
                                                          this._onStateChanged));
        this._onStateChanged();
    },

    _onDestroy: function() {
        if (this._stateChangedId > 0)
            this.app.disconnect(this._stateChangedId);
        this._stateChangedId = 0;
        this._removeMenuTimeout();
    },

    _removeMenuTimeout: function() {
        if (this._menuTimeoutId > 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
    },

    _onStateChanged: function() {
        if (this.app.state != Shell.AppState.STOPPED)
            this.actor.add_style_class_name('running');
        else
            this.actor.remove_style_class_name('running');
    },

    _onButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            this._removeMenuTimeout();
            this._menuTimeoutId = Mainloop.timeout_add(MENU_POPUP_TIMEOUT,
                Lang.bind(this, function() {
                    this.popupMenu();
                }));
        }
    },

    _onClicked: function(actor, event) {
        this._removeMenuTimeout();

        let button = event.get_button();
        if (button == 1) {
            this._onActivate(event);
        } else if (button == 2) {
            let newWorkspace = Main.overview.workspaces.addWorkspace();
            if (newWorkspace != null) {
                newWorkspace.activate(global.get_current_time());
                this.emit('launching');
                this.app.open_new_window();
                Main.overview.hide();
            }
        } else if (button == 3) {
            this.popupMenu();
        }
        return false;
    },

    getId: function() {
        return this.app.get_id();
    },

    popupMenu: function() {
        this._removeMenuTimeout();
        this.actor.fake_release();

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

            this._menuManager.addMenu(this._menu);
        }

        this._menu.popup();

        return false;
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
        } else {
            Main.overview.hide();
        }
    },

    _onMenuPoppedUp: function() {
        if (this._getRunning()) {
            Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(this.app.get_id());
            this._setWindowSelection = true;
            this._didActivateWindow = false;
        }
    },

    _onMenuPoppedDown: function() {
        this.actor.sync_hover();

        if (this._didActivateWindow)
            return;
        if (!this._setWindowSelection)
            return;

        Main.overview.getWorkspacesForWindow(null).setApplicationWindowSelection(null);
        this._setWindowSelection = false;
    },

    _getRunning: function() {
        return this.app.state != Shell.AppState.STOPPED;
    },

    _onActivate: function (event) {
        this.emit('launching');
        let modifiers = Shell.get_event_state(event);

        if (modifiers & Clutter.ModifierType.CONTROL_MASK
            && this.app.state == Shell.AppState.RUNNING) {
            this.app.open_new_window();
        } else {
            this.app.activate();
        }
        Main.overview.hide();
    },

    // called by this._menuManager when it has the grab
    menuEventFilter: function(event) {
        return this._menu.menuEventFilter(event);
    },

    shellWorkspaceLaunch : function() {
        this.app.open_new_window();
    },

    getDragActor: function() {
        return this.app.create_icon_texture(this.icon.iconSize);
    },

    // Returns the original actor that should align with the actor
    // we show as the item is being dragged.
    getDragActorSource: function() {
        return this.icon.icon;
    }
};
Signals.addSignalMethods(AppWellIcon.prototype);

function AppIconMenu(source) {
    this._init(source);
}

AppIconMenu.prototype = {
    __proto__: PopupMenu.PopupMenu.prototype,

    _init: function(source) {
        let side = St.Side.LEFT;
        if (St.Widget.get_default_direction() == St.TextDirection.RTL)
            side = St.Side.RIGHT;

        PopupMenu.PopupMenu.prototype._init.call(this, source.actor, St.Align.MIDDLE, side, 0);

        this._source = source;

        this.connect('active-changed', Lang.bind(this, this._onActiveChanged));
        this.connect('activate', Lang.bind(this, this._onActivate));
        this.connect('open-state-changed', Lang.bind(this, this._onOpenStateChanged));

        this.actor.add_style_class_name('app-well-menu');

        // Chain our visibility and lifecycle to that of the source
        source.actor.connect('notify::mapped', Lang.bind(this, function () {
            if (!source.actor.mapped)
                this.close();
        }));
        source.actor.connect('destroy', Lang.bind(this, function () { this.actor.destroy(); }));

        Main.uiGroup.add_actor(this.actor);
    },

    _redisplay: function() {
        this.removeAll();

        let windows = this._source.app.get_windows();

        // Display the app windows menu items and the separator between windows
        // of the current desktop and other windows.
        let activeWorkspace = global.screen.get_active_workspace();
        let separatorShown = windows.length > 0 && windows[0].get_workspace() != activeWorkspace;

        for (let i = 0; i < windows.length; i++) {
            if (!separatorShown && windows[i].get_workspace() != activeWorkspace) {
                this._appendSeparator();
                separatorShown = true;
            }
            let item = this._appendMenuItem(windows[i].title);
            item._window = windows[i];
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
        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this.addMenuItem(separator);
    },

    _appendMenuItem: function(labelText) {
        // FIXME: app-well-menu-item style
        let item = new PopupMenu.PopupMenuItem(labelText);
        this.addMenuItem(item);
        return item;
    },

    popup: function(activatingButton) {
        this._redisplay();
        this.open();
    },

    _onOpenStateChanged: function (menu, open) {
        if (open) {
            this.emit('popup', true);
        } else {
            this._updateHighlight(null);
            this.emit('popup', false);
        }
    },

    // called by this._menuManager when it has the grab
    menuEventFilter: function(event) {
        let eventType = event.type();

        // Check if the user is interacting with a window representation
        // rather than interacting with the menu

        if (eventType == Clutter.EventType.BUTTON_RELEASE) {
            let metaWindow = this._findMetaWindowForActor(event.get_source());
            if (metaWindow)
                this.emit('activate-window', metaWindow);
        } else if (eventType == Clutter.EventType.ENTER) {
            let metaWindow = this._findMetaWindowForActor(event.get_source());
            if (metaWindow)
                this._selectMenuItemForWindow(metaWindow, true);
        } else if (eventType == Clutter.EventType.LEAVE) {
            let metaWindow = this._findMetaWindowForActor(event.get_source());
            if (metaWindow)
                this._selectMenuItemForWindow(metaWindow, false);
        }

        return false;
    },

    _findMetaWindowForActor: function (actor) {
        if (actor._delegate.metaWindow)
            return actor._delegate.metaWindow;
        else if (actor.get_meta_window)
            return actor.get_meta_window();
        return null;
    },

    _updateHighlight: function (item) {
        if (this._highlightedItem)
            this.emit('highlight-window', null);
        this._highlightedItem = item;
        if (this._highlightedItem) {
            let window = this._highlightedItem._window;
            if (window)
                this.emit('highlight-window', window);
        }
    },

    _selectMenuItemForWindow: function (metaWindow, selected) {
        let items = this.getMenuItems();
        for (let i = 0; i < items.length; i++) {
            let item = items[i];
            let menuMetaWindow = item._window;
            if (menuMetaWindow == metaWindow)
                item.setActive(selected);
        }
    },

    _onActiveChanged: function (menu, child) {
        this._updateHighlight(child);
    },

    _onActivate: function (actor, child) {
        if (child._window) {
            let metaWindow = child._window;
            this.emit('activate-window', metaWindow);
        } else if (child == this._newWindowMenuItem) {
            this._source.app.open_new_window();
            this.emit('activate-window', null);
        } else if (child == this._toggleFavoriteMenuItem) {
            let favs = AppFavorites.getAppFavorites();
            let isFavorite = favs.isFavorite(this._source.app.get_id());
            if (isFavorite)
                favs.removeFavorite(this._source.app.get_id());
            else
                favs.addFavorite(this._source.app.get_id());
        }
        this.close();
    }
};
Signals.addSignalMethods(AppIconMenu.prototype);
