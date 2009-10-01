/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Workspaces = imports.ui.workspaces;

const GLOW_COLOR = new Clutter.Color();
GLOW_COLOR.from_pixel(0x4f6ba4ff);
const GLOW_PADDING_HORIZONTAL = 3;
const GLOW_PADDING_VERTICAL = 3;

const APPICON_ICON_SIZE = 48;

const APPICON_PADDING = 1;
const APPICON_BORDER_WIDTH = 1;
const APPICON_CORNER_RADIUS = 4;

const APPICON_MENU_POPUP_TIMEOUT_MS = 600;

const APPICON_DEFAULT_BORDER_COLOR = new Clutter.Color();
APPICON_DEFAULT_BORDER_COLOR.from_pixel(0x787878ff);
const APPICON_MENU_BACKGROUND_COLOR = new Clutter.Color();
APPICON_MENU_BACKGROUND_COLOR.from_pixel(0x292929ff);
const APPICON_MENU_FONT = 'Sans 14px';
const APPICON_MENU_COLOR = new Clutter.Color();
APPICON_MENU_COLOR.from_pixel(0xffffffff);
const APPICON_MENU_SELECTED_COLOR = new Clutter.Color();
APPICON_MENU_SELECTED_COLOR.from_pixel(0x005b97ff);
const APPICON_MENU_SEPARATOR_COLOR = new Clutter.Color();
APPICON_MENU_SEPARATOR_COLOR.from_pixel(0x787878ff);
const APPICON_MENU_BORDER_WIDTH = 1;
const APPICON_MENU_ARROW_SIZE = 12;
const APPICON_MENU_CORNER_RADIUS = 4;
const APPICON_MENU_PADDING = 4;

const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

const MenuType = { NONE: 0, ON_RIGHT: 1, BELOW: 2 };

function AppIcon(appInfo, menuType) {
    this._init(appInfo, menuType || MenuType.NONE);
}

AppIcon.prototype = {
    _init : function(appInfo, menuType, showGlow) {
        this.appInfo = appInfo;
        this._menuType = menuType;

        this.actor = new Shell.ButtonBox({ orientation: Big.BoxOrientation.VERTICAL,
                                           border: APPICON_BORDER_WIDTH,
                                           corner_radius: APPICON_CORNER_RADIUS,
                                           padding: APPICON_PADDING,
                                           reactive: true });
        this.actor._delegate = this;
        this.highlight_border_color = APPICON_DEFAULT_BORDER_COLOR;

        if (menuType != MenuType.NONE) {
            this.windows = Shell.AppMonitor.get_default().get_windows_for_app(appInfo.get_id());
            for (let i = 0; i < this.windows.length; i++) {
                this.windows[i].connect('notify::user-time', Lang.bind(this, this._resortWindows));
            }
            this._resortWindows();

            this.actor.connect('button-press-event', Lang.bind(this, this._updateMenuOnButtonPress));
            this.actor.connect('notify::hover', Lang.bind(this, this._updateMenuOnHoverChanged));
            this.actor.connect('activate', Lang.bind(this, this._updateMenuOnActivate));

            this._menuTimeoutId = 0;
            this._menu = null;
        } else
            this.windows = [];

        let iconBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    x_align: Big.BoxAlignment.CENTER,
                                    y_align: Big.BoxAlignment.CENTER,
                                    width: APPICON_ICON_SIZE,
                                    height: APPICON_ICON_SIZE });
        this.icon = appInfo.create_icon_texture(APPICON_ICON_SIZE);
        iconBox.append(this.icon, Big.BoxPackFlags.NONE);

        this.actor.append(iconBox, Big.BoxPackFlags.EXPAND);

        let nameBox = new Shell.GenericContainer();
        nameBox.connect('get-preferred-width', Lang.bind(this, this._nameBoxGetPreferredWidth));
        nameBox.connect('get-preferred-height', Lang.bind(this, this._nameBoxGetPreferredHeight));
        nameBox.connect('allocate', Lang.bind(this, this._nameBoxAllocate));
        this._nameBox = nameBox;

        this._name = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 12px",
                                        line_alignment: Pango.Alignment.CENTER,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: appInfo.get_name() });
        nameBox.add_actor(this._name);
        if (showGlow) {
            this._glowBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
            let glowPath = GLib.filename_to_uri(global.imagedir + 'app-well-glow.png', '');
            for (let i = 0; i < this.windows.length && i < 3; i++) {
                let glow = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                          glowPath, -1, -1);
                glow.keep_aspect_ratio = false;
                this._glowBox.append(glow, Big.BoxPackFlags.EXPAND);
            }
            this._nameBox.add_actor(this._glowBox);
            this._glowBox.lower(this._name);
        }
        else
            this._glowBox = null;

        this.actor.append(nameBox, Big.BoxPackFlags.NONE);
    },

    _nameBoxGetPreferredWidth: function (nameBox, forHeight, alloc) {
        let [min, natural] = this._name.get_preferred_width(forHeight);
        alloc.min_size = min + GLOW_PADDING_HORIZONTAL * 2;
        alloc.natural_size = natural + GLOW_PADDING_HORIZONTAL * 2;
    },

    _nameBoxGetPreferredHeight: function (nameBox, forWidth, alloc) {
        let [min, natural] = this._name.get_preferred_height(forWidth);
        alloc.min_size = min + GLOW_PADDING_VERTICAL * 2;
        alloc.natural_size = natural + GLOW_PADDING_VERTICAL * 2;
    },

    _nameBoxAllocate: function (nameBox, box, flags) {
        let childBox = new Clutter.ActorBox();
        let [minWidth, naturalWidth] = this._name.get_preferred_width(-1);
        let [minHeight, naturalHeight] = this._name.get_preferred_height(-1);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        let targetWidth = availWidth;
        let xPadding = 0;
        if (naturalWidth < availWidth) {
            xPadding = Math.floor((availWidth - naturalWidth) / 2);
        }
        childBox.x1 = xPadding;
        childBox.x2 = availWidth - xPadding;
        childBox.y1 = GLOW_PADDING_VERTICAL;
        childBox.y2 = availHeight - GLOW_PADDING_VERTICAL;
        this._name.allocate(childBox, flags);

        // Now the glow
        if (this._glowBox != null) {
            let glowPaddingHoriz = Math.max(0, xPadding - GLOW_PADDING_HORIZONTAL);
            glowPaddingHoriz = Math.max(GLOW_PADDING_HORIZONTAL, glowPaddingHoriz);
            childBox.x1 = glowPaddingHoriz;
            childBox.x2 = availWidth - glowPaddingHoriz;
            childBox.y1 = 0;
            childBox.y2 = availHeight;
            this._glowBox.allocate(childBox, flags);
        }
    },

    _resortWindows: function() {
        this.windows.sort(function (a, b) {
            let activeWorkspace = global.screen.get_active_workspace();
            let wsA = a.get_workspace() == activeWorkspace;
            let wsB = b.get_workspace() == activeWorkspace;

            if (wsA && !wsB)
                return -1;
            else if (wsB && !wsA)
                return 1;

            let visA = a.showing_on_its_workspace();
            let visB = b.showing_on_its_workspace();

            if (visA && !visB)
                return -1;
            else if (visB && !visA)
                return 1;
            else
                return b.get_user_time() - a.get_user_time();
        });
    },

    // AppIcon itself is not a draggable, but if you want to make
    // a subclass of it draggable, you can use this method to create
    // a drag actor
    createDragActor: function() {
        return this.appInfo.create_icon_texture(APPICON_ICON_SIZE);
    },

    setHighlight: function(highlight) {
        if (highlight) {
            this.actor.border_color = this.highlight_border_color;
        } else {
            this.actor.border_color = TRANSPARENT_COLOR;
        }
    },

    _updateMenuOnActivate: function(actor, event) {
        if (this._menuTimeoutId != 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
        this.emit('activate');
        return false;
    },

    _updateMenuOnHoverChanged: function() {
        if (!this.actor.hover && this._menuTimeoutId != 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
        return false;
    },

    _updateMenuOnButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            if (this._menuTimeoutId != 0)
                Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = Mainloop.timeout_add(APPICON_MENU_POPUP_TIMEOUT_MS,
                                                       Lang.bind(this, function () { this.popupMenu(button); }));
        } else if (button == 3) {
            this.popupMenu(button);
        }
        return false;
    },

    popupMenu: function(activatingButton) {
        if (this._menuTimeoutId != 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }

        this.actor.fake_release();

        if (!this._menu) {
            this._menu = new AppIconMenu(this, this._menuType);
            this._menu.connect('highlight-window', Lang.bind(this, function (menu, window) {
                this.highlightWindow(window);
            }));
            this._menu.connect('activate-window', Lang.bind(this, function (menu, window) {
                this.activateWindow(window);
            }));
            this._menu.connect('popup', Lang.bind(this, function (menu, isPoppedUp) {
                if (isPoppedUp)
                    this.menuPoppedUp();
                else
                    this.menuPoppedDown();
            }));
        }

        this._menu.popup(activatingButton);

        return false;
    },

    // Default implementations; AppDisplay.RunningWellItem overrides these
    highlightWindow: function(window) {
        this.emit('highlight-window', window);
    },

    activateWindow: function(window) {
        this.emit('activate-window', window);
    },

    menuPoppedUp: function() {
        this.emit('menu-popped-up', this._menu);
    },

    menuPoppedDown: function() {
        this.emit('menu-popped-down', this._menu);
    }
};

Signals.addSignalMethods(AppIcon.prototype);

function AppIconMenu(source, type) {
    this._init(source, type);
}

AppIconMenu.prototype = {
    _init: function(source, type) {
        this._source = source;
        this._type = type;

        this.actor = new Shell.GenericContainer({ reactive: true });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this._windowContainer = new Shell.Menu({ orientation: Big.BoxOrientation.VERTICAL,
                                                 border_color: source.highlight_border_color,
                                                 border: APPICON_MENU_BORDER_WIDTH,
                                                 background_color: APPICON_MENU_BACKGROUND_COLOR,
                                                 padding: 4,
                                                 corner_radius: APPICON_MENU_CORNER_RADIUS,
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
            Shell.draw_box_pointer(texture,
                                   this._type == MenuType.ON_RIGHT ? Clutter.Gravity.WEST : Clutter.Gravity.NORTH,
                                   source.highlight_border_color,
                                   APPICON_MENU_BACKGROUND_COLOR);
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
        if (this._type == MenuType.ON_RIGHT) {
            min += APPICON_MENU_ARROW_SIZE;
            natural += APPICON_MENU_ARROW_SIZE;
        }
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [min, natural] = this._windowContainer.get_preferred_height(forWidth);
        if (this._type == MenuType.BELOW) {
            min += APPICON_MENU_ARROW_SIZE;
            natural += APPICON_MENU_ARROW_SIZE;
        }
        alloc.min_size = min;
        alloc.natural_size = natural;
    },

    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();

        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        if (this._type == MenuType.ON_RIGHT) {
            childBox.x1 = 0;
            childBox.x2 = APPICON_MENU_ARROW_SIZE;
            childBox.y1 = Math.floor((height / 2) - (APPICON_MENU_ARROW_SIZE / 2));
            childBox.y2 = childBox.y1 + APPICON_MENU_ARROW_SIZE;
            this._arrow.allocate(childBox, flags);

            childBox.x1 = APPICON_MENU_ARROW_SIZE - APPICON_MENU_BORDER_WIDTH;
            childBox.x2 = width;
            childBox.y1 = 0;
            childBox.y2 = height;
            this._windowContainer.allocate(childBox, flags);
        } else /* MenuType.BELOW */ {
            childBox.x1 = Math.floor((width / 2) - (APPICON_MENU_ARROW_SIZE / 2));
            childBox.x2 = childBox.x1 + APPICON_MENU_ARROW_SIZE;
            childBox.y1 = 0;
            childBox.y2 = APPICON_MENU_ARROW_SIZE;
            this._arrow.allocate(childBox, flags);

            childBox.x1 = 0;
            childBox.x2 = width;
            childBox.y1 = APPICON_MENU_ARROW_SIZE - APPICON_MENU_BORDER_WIDTH;
            childBox.y2 = height;
            this._windowContainer.allocate(childBox, flags);
        }
    },

    _redisplay: function() {
        this._windowContainer.remove_all();

        let windows = this._source.windows;

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
        let separatorShown = windows[0].get_workspace() != activeWorkspace;

        for (let i = 0; i < windows.length; i++) {
            if (!separatorShown && windows[i].get_workspace() != activeWorkspace) {
                this._appendSeparator();
                separatorShown = true;
            }

            let icon = null;
            if (iconsDiffer)
                icon = Shell.TextureCache.get_default().bind_pixbuf_property(windows[i], "mini-icon");

            let box = this._appendMenuItem(icon, windows[i].title);
            box._window = windows[i];
        }

        if (windows.length > 0)
            this._appendSeparator();

        this._newWindowMenuItem = windows.length > 0 ? this._appendMenuItem(null, _("New Window")) : null;

        let favorites = Shell.AppSystem.get_default().get_favorites();
        let id = this._source.appInfo.get_id();
        this._isFavorite = false;
        for (let i = 0; i < favorites.length; i++) {
            if (id == favorites[i]) {
                this._isFavorite = true;
                break;
            }
        }
        if (windows.length > 0)
            this._appendSeparator();
        this._toggleFavoriteMenuItem = this._appendMenuItem(null, this._isFavorite ? _("Remove from favorites")
                                                                    : _("Add to favorites"));

        this._highlightedItem = null;
    },

    _appendSeparator: function () {
        let box = new Big.Box({ padding_top: 2, padding_bottom: 2 });
        box.append(new Clutter.Rectangle({ height: 1,
                                           color: APPICON_MENU_SEPARATOR_COLOR }),
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
                                       font_name: APPICON_MENU_FONT,
                                       ellipsize: Pango.EllipsizeMode.END,
                                       color: APPICON_MENU_COLOR });
        vCenter.append(label, Big.BoxPackFlags.NONE);
        box.append(vCenter, Big.BoxPackFlags.NONE);
        this._windowContainer.append(box, Big.BoxPackFlags.NONE);
        return box;
    },

    popup: function(activatingButton) {
        let [stageX, stageY] = this._source.actor.get_transformed_position();
        let [stageWidth, stageHeight] = this._source.actor.get_transformed_size();

        this._redisplay();

        this._windowContainer.popup(activatingButton, Main.currentTime());

        this.emit('popup', true);

        let x, y;
        if (this._type == MenuType.ON_RIGHT) {
            x = Math.floor(stageX + stageWidth);
            y = Math.floor(stageY + (stageHeight / 2) - (this.actor.height / 2));
        } else {
            x = Math.floor(stageX + (stageWidth / 2) - (this.actor.width / 2));
            y = Math.floor(stageY + stageHeight);
        }

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
        if (actor._delegate instanceof Workspaces.WindowClone)
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
            this._highlightedItem.background_color = TRANSPARENT_COLOR;
            this.emit('highlight-window', null);
        }
        this._highlightedItem = item;
        if (this._highlightedItem) {
            this._highlightedItem.background_color = APPICON_MENU_SELECTED_COLOR;
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
            this._source.appInfo.launch();
            this.emit('activate-window', null);
        } else if (child == this._toggleFavoriteMenuItem) {
            let appSys = Shell.AppSystem.get_default();
            if (this._isFavorite)
                appSys.remove_favorite(this._source.appInfo.get_id());
            else
                appSys.add_favorite(this._source.appInfo.get_id());
        }
        this.popdown();
    },

    _onWindowSelectionCancelled: function () {
        this.emit('highlight-window', null);
        this.popdown();
    }
};

Signals.addSignalMethods(AppIconMenu.prototype);
