/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

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
const MENU_POPUP_TIMEOUT = 600;

function AllAppView() {
    this._init();
}

AllAppView.prototype = {
    _init: function(apps) {
        this.actor = new St.BoxLayout({ vertical: true });
        this._grid = new WellGrid(true);
        this._appSystem = Shell.AppSystem.get_default();
        this.actor.add(this._grid.actor, { y_align: St.Align.START, expand: true });
    },

    _removeAll: function() {
        this._grid.removeAll();
        this._apps = [];
    },

    _addApp: function(app) {
        let App = new AppWellIcon(this._appSystem.get_app(app.get_id()));
        App.connect('launching', Lang.bind(this, function() {
            this.emit('launching');
        }));
        App._draggable.connect('drag-begin', Lang.bind(this, function() {
            this.emit('drag-begin');
        }));

        this._grid.addItem(App.actor);

        this._apps.push(App);
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

Signals.addSignalMethods(AllAppView.prototype);

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

        let bin = new St.BoxLayout({ style_class: 'all-app-controls-panel',
                                     reactive: true });
        this.actor = new St.BoxLayout({ style_class: 'all-app', vertical: true });
        this.actor.hide();

        let view = new St.ScrollView({ x_fill: true,
                                       y_fill: false,
                                       style_class: 'all-app-scroll-view',
                                       vshadows: true });
        this._scrollView = view;
        this.actor.add(bin);
        this.actor.add(view, { expand: true, y_fill: false, y_align: St.Align.START });

        this._appView = new AllAppView();
        this._appView.connect('launching', Lang.bind(this, this.close));
        this._appView.connect('drag-begin', Lang.bind(this, this.close));
        this._scrollView.add_actor(this._appView.actor);

        this._scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);

        this._workId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));
    },

    _redisplay: function() {
        let apps = this._appSystem.get_flattened_apps().filter(function(app) {
            return !app.get_is_nodisplay();
        });

        this._appView.refresh(apps);
    },

    toggle: function() {
        this.emit('open-state-changed', !this.actor.visible);

        this.actor.visible = !this.actor.visible;
    },

    close: function() {
        if (!this.actor.visible)
            return;
        this.toggle();
    }
};

Signals.addSignalMethods(AllAppDisplay.prototype);

function AppSearchResultDisplay(provider) {
    this._init(provider);
}

AppSearchResultDisplay.prototype = {
    __proto__: Search.SearchResultDisplay.prototype,

    _init: function (provider) {
        Search.SearchResultDisplay.prototype._init.call(this, provider);
        this._spacing = 0;
        this.actor = new St.Bin({ name: 'dashAppSearchResults',
                                  x_align: St.Align.START });
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));
        let container = new Shell.GenericContainer();
        this._container = container;
        this.actor.set_child(container);
        container.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        container.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        container.connect('allocate', Lang.bind(this, this._allocate));
    },

    _getPreferredWidth: function (actor, forHeight, alloc) {
        let children = actor.get_children();

        for (let i = 0; i < children.length; i++) {
            let [minSize, natSize] = children[i].get_preferred_width(forHeight);
            alloc.natural_size += natSize;
        }
    },

    _getPreferredHeight: function (actor, forWidth, alloc) {
        let children = actor.get_children();

        for (let i = 0; i < children.length; i++) {
            let [minSize, natSize] = children[i].get_preferred_height(forWidth);
            if (minSize > alloc.min_size)
                alloc.min_size = minSize;
            if (natSize > alloc.natural_size)
                alloc.natural_size = natSize;
        }
    },

    _allocate: function (actor, box, flags) {
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let children = actor.get_children();

        let x = 0;
        let i;
        for (i = 0; i < children.length; i++) {
            let child = children[i];
            let childBox = new Clutter.ActorBox();

            let [minWidth, minHeight, natWidth, natHeight] = child.get_preferred_size();

            if (x + natWidth > availWidth) {
                actor.set_skip_paint(child, true);
                continue;
            }

            let yPadding = Math.max(0, availHeight - natHeight);

            childBox.x1 = x;
            childBox.x2 = childBox.x1 + natWidth;
            childBox.y1 = Math.floor(yPadding / 2);
            childBox.y2 = availHeight - childBox.y1;

            x = childBox.x2 + this._spacing;

            child.allocate(childBox, flags);
            actor.set_skip_paint(child, false);
        }
    },

    _onStyleChanged: function () {
        let themeNode = this.actor.get_theme_node();
        let [success, len] = themeNode.get_length('spacing', false);
        if (success)
            this._spacing = len;
        this._container.queue_relayout();
    },

    renderResults: function(results, terms) {
        let appSys = Shell.AppSystem.get_default();
        for (let i = 0; i < results.length && i < WELL_MAX_COLUMNS; i++) {
            let result = results[i];
            let app = appSys.get_app(result);
            let display = new AppWellIcon(app);
            this._container.add_actor(display.actor);
        }
    },

    clear: function () {
        this._container.get_children().forEach(function (actor) { actor.destroy(); });
        this.selectionIndex = -1;
    },

    getVisibleResultCount: function() {
        let nChildren = this._container.get_children().length;
        return nChildren - this._container.get_n_skip_paint();
    },

    selectIndex: function (index) {
        let nVisible = this.getVisibleResultCount();
        let children = this._container.get_children();
        if (this.selectionIndex >= 0) {
            let prevActor = children[this.selectionIndex];
            prevActor._delegate.setSelected(false);
        }
        this.selectionIndex = -1;
        if (index >= nVisible)
            return false;
        else if (index < 0)
            return false;
        let targetActor = children[index];
        targetActor._delegate.setSelected(true);
        this.selectionIndex = index;
        return true;
    },

    activateSelected: function() {
        if (this.selectionIndex < 0)
            return;
        let children = this._container.get_children();
        let targetActor = children[this.selectionIndex];
        this.provider.activateResult(targetActor._delegate.app.get_id());
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

    createResultContainerActor: function () {
        return new AppSearchResultDisplay(this);
    },

    createResultActor: function (resultMeta, terms) {
        return new AppIcon(resultMeta.id);
    },

    expandSearch: function(terms) {
        log("TODO expand search");
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
};

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

        this._draggable = DND.makeDraggable(this.actor);
        this._draggable.connect('drag-begin', Lang.bind(this,
            function () {
                this._removeMenuTimeout();
            }));

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('show', Lang.bind(this, this._onShow));
        this.actor.connect('hide', Lang.bind(this, this._onHideDestroy));
        this.actor.connect('destroy', Lang.bind(this, this._onHideDestroy));

        this._appWindowChangedId = 0;
        this._menuTimeoutId = 0;
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
        this._removeMenuTimeout();
    },

    _removeMenuTimeout: function() {
        if (this._menuTimeoutId > 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
    },

    _updateStyleClass: function() {
        let windows = this.app.get_windows();
        let running = windows.length > 0;
        this._running = running;
        let style = "app-well-app";
        if (this._running)
            style += " running";
        if (this._selected)
            style += " selected";
        this.actor.style_class = style;
    },

    _onButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            this._removeMenuTimeout();
            this._menuTimeoutId = Mainloop.timeout_add(MENU_POPUP_TIMEOUT,
                Lang.bind(this, function() {
                    this.popupMenu(button);
                }));
        }
    },

    _onClicked: function(actor, event) {
        this._removeMenuTimeout();

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

    getId: function() {
        return this.app.get_id();
    },

    popupMenu: function(activatingButton) {
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

    setSelected: function (isSelected) {
        this._selected = isSelected;
        this._updateStyleClass();
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
        return this.app.get_windows().length > 0;
    },

    _onActivate: function (event) {
        let running = this._getRunning();
        this.emit('launching');

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
};
Signals.addSignalMethods(AppWellIcon.prototype);

function AppIconMenu(source) {
    this._init(source);
}

AppIconMenu.prototype = {
    _init: function(source) {
        this._source = source;

        this.actor = new Shell.GenericContainer({ reactive: true });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this._windowContainer = new Shell.Menu({ style_class: 'app-well-menu',
                                                 vertical: true,
                                                 width: Main.overview._dash.actor.width });
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

        this._arrow = new St.DrawingArea({ style_class: 'app-well-menu-arrow' });
        this._arrow.connect('repaint', Lang.bind(this, function (area) {
            Shell.draw_box_pointer(area, Shell.PointerDirection.LEFT);
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
        let [menuMin, menuNatural] = this._windowContainer.get_preferred_width(forHeight);
        let [arrowMin, arrowNatural] = this._arrow.get_preferred_width(forHeight);
        alloc.min_size = menuMin + arrowMin;
        alloc.natural_size = menuNatural + arrowNatural;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [min, natural] = this._windowContainer.get_preferred_height(forWidth);
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        let themeNode = this._windowContainer.get_theme_node();

        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        let [arrowMinWidth, arrowWidth] = this._arrow.get_preferred_width(height);

        childBox.x1 = 0;
        childBox.x2 = arrowWidth;
        childBox.y1 = Math.floor((height / 2) - (arrowWidth / 2));
        childBox.y2 = childBox.y1 + arrowWidth;
        this._arrow.allocate(childBox, flags);

        // Ensure the arrow is above the border area
        let border = themeNode.get_border_width(St.Side.LEFT);
        childBox.x1 = arrowWidth - border;
        childBox.x2 = width;
        childBox.y1 = 0;
        childBox.y2 = height;
        this._windowContainer.allocate(childBox, flags);
    },

    _redisplay: function() {
        this._windowContainer.remove_all();

        let windows = this._source.app.get_windows();

        this._windowContainer.show();

        let iconsDiffer = false;
        let texCache = St.TextureCache.get_default();
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
        this._windowContainer.add_actor(bin);
    },

    _appendMenuItem: function(labelText) {
        let box = new St.BoxLayout({ style_class: 'app-well-menu-item',
                                      reactive: true });
        let label = new St.Label({ text: labelText });
        box.add(label);
        this._windowContainer.add_actor(box);
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
            this._highlightedItem.remove_style_pseudo_class('hover');
            this.emit('highlight-window', null);
        }
        this._highlightedItem = item;
        if (this._highlightedItem) {
            item.add_style_pseudo_class('hover');
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
    }
};
Signals.addSignalMethods(AppIconMenu.prototype);

function WellGrid() {
    this._init();
}

WellGrid.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: "dashAppWell", vertical: true });
        // Pulled from CSS, but hardcode some defaults here
        this._spacing = 0;
        this._item_size = 48;
        this._grid = new Shell.GenericContainer();
        this.actor.add(this._grid, { expand: true, y_align: St.Align.START });
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
};

function AppWell() {
    this._init();
}

AppWell.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];

        this._favorites = [];

        this._grid = new WellGrid();
        this.actor = this._grid.actor;
        this.actor._delegate = this;

        this._workId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));

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
        if (source instanceof AppWellIcon) {
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
