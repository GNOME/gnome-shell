// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Main = imports.ui.main;
const SwitcherPopup = imports.ui.switcherPopup;
const Tweener = imports.ui.tweener;

const APP_ICON_HOVER_TIMEOUT = 200; // milliseconds

const THUMBNAIL_DEFAULT_SIZE = 256;
const THUMBNAIL_POPUP_TIME = 500; // milliseconds
const THUMBNAIL_FADE_TIME = 0.1; // seconds

const WINDOW_PREVIEW_SIZE = 128;
const APP_ICON_SIZE = 96;
const APP_ICON_SIZE_SMALL = 48;

const baseIconSizes = [96, 64, 48, 32, 22];

const AppIconMode = {
    THUMBNAIL_ONLY: 1,
    APP_ICON_ONLY: 2,
    BOTH: 3,
};

function _createWindowClone(window, size) {
    let windowTexture = window.get_texture();
    let [width, height] = windowTexture.get_size();
    let scale = Math.min(1.0, size / width, size / height);
    return new Clutter.Clone({ source: windowTexture,
                               width: width * scale,
                               height: height * scale,
                               x_align: Clutter.ActorAlign.CENTER,
                               y_align: Clutter.ActorAlign.CENTER,
                               // usual hack for the usual bug in ClutterBinLayout...
                               x_expand: true,
                               y_expand: true });
};

const AppSwitcherPopup = new Lang.Class({
    Name: 'AppSwitcherPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init : function() {
        this.parent();

        this._thumbnails = null;
        this._thumbnailTimeoutId = 0;
        this._currentWindow = -1;

        this.thumbnailsVisible = false;
    },

    _allocate: function (actor, box, flags) {
        this.parent(actor, box, flags);

        // Allocate the thumbnails
        // We try to avoid overflowing the screen so we base the resulting size on
        // those calculations
        if (this._thumbnails) {
            let childBox = this._switcherList.actor.get_allocation_box();
            let primary = Main.layoutManager.primaryMonitor;

            let leftPadding = this.actor.get_theme_node().get_padding(St.Side.LEFT);
            let rightPadding = this.actor.get_theme_node().get_padding(St.Side.RIGHT);
            let bottomPadding = this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
            let vPadding = this.actor.get_theme_node().get_vertical_padding();
            let hPadding = leftPadding + rightPadding;

            let icon = this._items[this._selectedIndex].actor;
            let [posX, posY] = icon.get_transformed_position();
            let thumbnailCenter = posX + icon.width / 2;
            let [childMinWidth, childNaturalWidth] = this._thumbnails.actor.get_preferred_width(-1);
            childBox.x1 = Math.max(primary.x + leftPadding, Math.floor(thumbnailCenter - childNaturalWidth / 2));
            if (childBox.x1 + childNaturalWidth > primary.x + primary.width - hPadding) {
                let offset = childBox.x1 + childNaturalWidth - primary.width + hPadding;
                childBox.x1 = Math.max(primary.x + leftPadding, childBox.x1 - offset - hPadding);
            }

            let spacing = this.actor.get_theme_node().get_length('spacing');

            childBox.x2 = childBox.x1 +  childNaturalWidth;
            if (childBox.x2 > primary.x + primary.width - rightPadding)
                childBox.x2 = primary.x + primary.width - rightPadding;
            childBox.y1 = this._switcherList.actor.allocation.y2 + spacing;
            this._thumbnails.addClones(primary.y + primary.height - bottomPadding - childBox.y1);
            let [childMinHeight, childNaturalHeight] = this._thumbnails.actor.get_preferred_height(-1);
            childBox.y2 = childBox.y1 + childNaturalHeight;
            this._thumbnails.actor.allocate(childBox, flags);
        }
    },

    _createSwitcher: function() {
        let apps = Shell.AppSystem.get_default().get_running ();

        if (apps.length == 0)
            return false;

        this._switcherList = new AppSwitcher(apps, this);
        this._items = this._switcherList.icons;
        if (this._items.length == 0)
            return false;

        return true;
    },

    _initialSelection: function(backward, binding) {
        if (binding == 'switch-group') {
            if (backward) {
                this._select(0, this._items[0].cachedWindows.length - 1);
            } else {
                if (this._items[0].cachedWindows.length > 1)
                    this._select(0, 1);
                else
                    this._select(0, 0);
            }
        } else if (binding == 'switch-group-backward') {
            this._select(0, this._items[0].cachedWindows.length - 1);
        } else if (binding == 'switch-applications-backward') {
            this._select(this._items.length - 1);
        } else if (this._items.length == 1) {
            this._select(0);
        } else if (backward) {
            this._select(this._items.length - 1);
        } else {
            this._select(1);
        }
    },

    _nextWindow : function() {
        // We actually want the second window if we're in the unset state
        if (this._currentWindow == -1)
            this._currentWindow = 0;
        return SwitcherPopup.mod(this._currentWindow + 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    },
    _previousWindow : function() {
        // Also assume second window here
        if (this._currentWindow == -1)
            this._currentWindow = 1;
        return SwitcherPopup.mod(this._currentWindow - 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    },

    _keyPressHandler: function(keysym, backwards, action) {
        if (action == Meta.KeyBindingAction.SWITCH_GROUP) {
            this._select(this._selectedIndex, backwards ? this._previousWindow() : this._nextWindow());
        } else if (action == Meta.KeyBindingAction.SWITCH_GROUP_BACKWARD) {
            this._select(this._selectedIndex, this._previousWindow());
        } else if (action == Meta.KeyBindingAction.SWITCH_APPLICATIONS) {
            this._select(backwards ? this._previous() : this._next());
        } else if (action == Meta.KeyBindingAction.SWITCH_APPLICATIONS_BACKWARD) {
            this._select(this._previous());
        } else if (this._thumbnailsFocused) {
            if (keysym == Clutter.Left)
                this._select(this._selectedIndex, this._previousWindow());
            else if (keysym == Clutter.Right)
                this._select(this._selectedIndex, this._nextWindow());
            else if (keysym == Clutter.Up)
                this._select(this._selectedIndex, null, true);
        } else {
            if (keysym == Clutter.Left)
                this._select(this._previous());
            else if (keysym == Clutter.Right)
                this._select(this._next());
            else if (keysym == Clutter.Down)
                this._select(this._selectedIndex, 0);
        }
    },

    _scrollHandler: function(direction) {
        if (direction == Clutter.ScrollDirection.UP) {
            if (this._thumbnailsFocused) {
                if (this._currentWindow == 0 || this._currentWindow == -1)
                    this._select(this._previous());
                else
                    this._select(this._selectedIndex, this._previousWindow());
            } else {
                let nwindows = this._items[this._selectedIndex].cachedWindows.length;
                if (nwindows > 1)
                    this._select(this._selectedIndex, nwindows - 1);
                else
                    this._select(this._previous());
            }
        } else if (direction == Clutter.ScrollDirection.DOWN) {
            if (this._thumbnailsFocused) {
                if (this._currentWindow == this._items[this._selectedIndex].cachedWindows.length - 1)
                    this._select(this._next());
                else
                    this._select(this._selectedIndex, this._nextWindow());
            } else {
                let nwindows = this._items[this._selectedIndex].cachedWindows.length;
                if (nwindows > 1)
                    this._select(this._selectedIndex, 0);
                else
                    this._select(this._next());
            }
        }
    },

    _itemActivatedHandler: function(n) {
        // If the user clicks on the selected app, activate the
        // selected window; otherwise (eg, they click on an app while
        // !mouseActive) activate the clicked-on app.
        if (n == this._selectedIndex && this._currentWindow >= 0)
            this._select(n, this._currentWindow);
        else
            this._select(n);
    },

    _itemEnteredHandler: function(n) {
        this._select(n);
    },

    _windowActivated : function(thumbnailList, n) {
        let appIcon = this._items[this._selectedIndex];
        Main.activateWindow(appIcon.cachedWindows[n]);
        this.destroy();
    },

    _windowEntered : function(thumbnailList, n) {
        if (!this.mouseActive)
            return;

        this._select(this._selectedIndex, n);
    },

    _finish : function(timestamp) {
        let appIcon = this._items[this._selectedIndex];
        if (this._currentWindow < 0)
            appIcon.app.activate_window(appIcon.cachedWindows[0], timestamp);
        else
            Main.activateWindow(appIcon.cachedWindows[this._currentWindow], timestamp);

        this.parent();
    },

    _onDestroy : function() {
        this.parent();

        if (this._thumbnails)
            this._destroyThumbnails();
        if (this._thumbnailTimeoutId != 0)
            Mainloop.source_remove(this._thumbnailTimeoutId);
    },

    /**
     * _select:
     * @app: index of the app to select
     * @window: (optional) index of which of @app's windows to select
     * @forceAppFocus: optional flag, see below
     *
     * Selects the indicated @app, and optional @window, and sets
     * this._thumbnailsFocused appropriately to indicate whether the
     * arrow keys should act on the app list or the thumbnail list.
     *
     * If @app is specified and @window is unspecified or %null, then
     * the app is highlighted (ie, given a light background), and the
     * current thumbnail list, if any, is destroyed. If @app has
     * multiple windows, and @forceAppFocus is not %true, then a
     * timeout is started to open a thumbnail list.
     *
     * If @app and @window are specified (and @forceAppFocus is not),
     * then @app will be outlined, a thumbnail list will be created
     * and focused (if it hasn't been already), and the @window'th
     * window in it will be highlighted.
     *
     * If @app and @window are specified and @forceAppFocus is %true,
     * then @app will be highlighted, and @window outlined, and the
     * app list will have the keyboard focus.
     */
    _select : function(app, window, forceAppFocus) {
        if (app != this._selectedIndex || window == null) {
            if (this._thumbnails)
                this._destroyThumbnails();
        }

        if (this._thumbnailTimeoutId != 0) {
            Mainloop.source_remove(this._thumbnailTimeoutId);
            this._thumbnailTimeoutId = 0;
        }

        this._thumbnailsFocused = (window != null) && !forceAppFocus;

        this._selectedIndex = app;
        this._currentWindow = window ? window : -1;
        this._switcherList.highlight(app, this._thumbnailsFocused);

        if (window != null) {
            if (!this._thumbnails)
                this._createThumbnails();
            this._currentWindow = window;
            this._thumbnails.highlight(window, forceAppFocus);
        } else if (this._items[this._selectedIndex].cachedWindows.length > 1 &&
                   !forceAppFocus) {
            this._thumbnailTimeoutId = Mainloop.timeout_add (
                THUMBNAIL_POPUP_TIME,
                Lang.bind(this, this._timeoutPopupThumbnails));
        }
    },

    _timeoutPopupThumbnails: function() {
        if (!this._thumbnails)
            this._createThumbnails();
        this._thumbnailTimeoutId = 0;
        this._thumbnailsFocused = false;
        return GLib.SOURCE_REMOVE;
    },

    _destroyThumbnails : function() {
        let thumbnailsActor = this._thumbnails.actor;
        Tweener.addTween(thumbnailsActor,
                         { opacity: 0,
                           time: THUMBNAIL_FADE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this, function() {
                                                            thumbnailsActor.destroy();
                                                            this.thumbnailsVisible = false;
                                                        })
                         });
        this._thumbnails = null;
        this._switcherList._items[this._selectedIndex].remove_accessible_state (Atk.StateType.EXPANDED);
    },

    _createThumbnails : function() {
        this._thumbnails = new ThumbnailList (this._items[this._selectedIndex].cachedWindows);
        this._thumbnails.connect('item-activated', Lang.bind(this, this._windowActivated));
        this._thumbnails.connect('item-entered', Lang.bind(this, this._windowEntered));

        this.actor.add_actor(this._thumbnails.actor);

        // Need to force an allocation so we can figure out whether we
        // need to scroll when selecting
        this._thumbnails.actor.get_allocation_box();

        this._thumbnails.actor.opacity = 0;
        Tweener.addTween(this._thumbnails.actor,
                         { opacity: 255,
                           time: THUMBNAIL_FADE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this, function () { this.thumbnailsVisible = true; })
                         });

        this._switcherList._items[this._selectedIndex].add_accessible_state (Atk.StateType.EXPANDED);
    }
});

const WindowSwitcherPopup = new Lang.Class({
    Name: 'WindowSwitcherPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init: function(items) {
        this.parent(items);
        this._settings = new Gio.Settings({ schema: 'org.gnome.shell.window-switcher' });
    },

    _getWindowList: function() {
        let workspace = this._settings.get_boolean('current-workspace-only') ? global.screen.get_active_workspace() : null;
        return global.display.get_tab_list(Meta.TabList.NORMAL, global.screen, workspace);
    },

    _createSwitcher: function() {
        let windows = this._getWindowList();

        if (windows.length == 0)
            return false;

        let mode = this._settings.get_enum('app-icon-mode');
        this._switcherList = new WindowList(windows, mode);
        this._items = this._switcherList.icons;

        if (this._items.length == 0)
            return false;

        return true;
    },

    _initialSelection: function(backward, binding) {
        if (binding == 'switch-windows-backward' || backward)
            this._select(this._items.length - 1);
        else if (this._items.length == 1)
            this._select(0);
        else
            this._select(1);
    },

    _keyPressHandler: function(keysym, backwards, action) {
        if (action == Meta.KeyBindingAction.SWITCH_WINDOWS) {
            this._select(backwards ? this._previous() : this._next());
        } else if (action == Meta.KeyBindingAction.SWITCH_WINDOWS_BACKWARD) {
            this._select(this._previous());
        } else {
            if (keysym == Clutter.Left)
                this._select(this._previous());
            else if (keysym == Clutter.Right)
                this._select(this._next());
        }
    },

    _finish: function() {
        Main.activateWindow(this._items[this._selectedIndex].window);

        this.parent();
    }
});

const AppIcon = new Lang.Class({
    Name: 'AppIcon',

    _init: function(app) {
        this.app = app;
        this.actor = new St.BoxLayout({ style_class: 'alt-tab-app',
                                         vertical: true });
        this.icon = null;
        this._iconBin = new St.Bin({ x_fill: true, y_fill: true });

        this.actor.add(this._iconBin, { x_fill: false, y_fill: false } );
        this.label = new St.Label({ text: this.app.get_name() });
        this.actor.add(this.label, { x_fill: false });
    },

    set_size: function(size) {
        this.icon = this.app.create_icon_texture(size);
        this._iconBin.child = this.icon;
    }
});

const AppSwitcher = new Lang.Class({
    Name: 'AppSwitcher',
    Extends: SwitcherPopup.SwitcherList,

    _init : function(apps, altTabPopup) {
        this.parent(true);

        this.icons = [];
        this._arrows = [];

        let windowTracker = Shell.WindowTracker.get_default();
        let settings = new Gio.Settings({ schema: 'org.gnome.shell.app-switcher' });
        let workspace = settings.get_boolean('current-workspace-only') ? global.screen.get_active_workspace()
                                                                       : null;
        let allWindows = global.display.get_tab_list(Meta.TabList.NORMAL,
                                                     global.screen, workspace);

        // Construct the AppIcons, add to the popup
        for (let i = 0; i < apps.length; i++) {
            let appIcon = new AppIcon(apps[i]);
            // Cache the window list now; we don't handle dynamic changes here,
            // and we don't want to be continually retrieving it
            appIcon.cachedWindows = allWindows.filter(function(w) {
                return windowTracker.get_window_app (w) == appIcon.app;
            });
            if (appIcon.cachedWindows.length > 0)
                this._addIcon(appIcon);
            else if (workspace == null)
                throw new Error('%s appears to be running, but doesn\'t have any windows'.format(appIcon.app.get_name()));
        }

        this._curApp = -1;
        this._iconSize = 0;
        this._altTabPopup = altTabPopup;
        this._mouseTimeOutId = 0;

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
    },

    _onDestroy: function() {
        if (this._mouseTimeOutId != 0)
            Mainloop.source_remove(this._mouseTimeOutId);
    },

    _setIconSize: function() {
        let j = 0;
        while(this._items.length > 1 && this._items[j].style_class != 'item-box') {
                j++;
        }
        let themeNode = this._items[j].get_theme_node();

        let iconPadding = themeNode.get_horizontal_padding();
        let iconBorder = themeNode.get_border_width(St.Side.LEFT) + themeNode.get_border_width(St.Side.RIGHT);
        let [iconMinHeight, iconNaturalHeight] = this.icons[j].label.get_preferred_height(-1);
        let iconSpacing = iconNaturalHeight + iconPadding + iconBorder;
        let totalSpacing = this._list.spacing * (this._items.length - 1);

        // We just assume the whole screen here due to weirdness happing with the passed width
        let primary = Main.layoutManager.primaryMonitor;
        let parentPadding = this.actor.get_parent().get_theme_node().get_horizontal_padding();
        let availWidth = primary.width - parentPadding - this.actor.get_theme_node().get_horizontal_padding();

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let iconSizes = baseIconSizes.map(function(s) {
            return s * scaleFactor;
        });

        if (this._items.length == 1) {
            this._iconSize = baseIconSizes[0];
        } else {
            for(let i =  0; i < baseIconSizes.length; i++) {
                this._iconSize = baseIconSizes[i];
                let height = iconSizes[i] + iconSpacing;
                let w = height * this._items.length + totalSpacing;
                if (w <= availWidth)
                    break;
            }
        }

        for(let i = 0; i < this.icons.length; i++) {
            if (this.icons[i].icon != null)
                break;
            this.icons[i].set_size(this._iconSize);
        }
    },

    _getPreferredHeight: function (actor, forWidth, alloc) {
        this._setIconSize();
        this.parent(actor, forWidth, alloc);
    },

    _allocate: function (actor, box, flags) {
        // Allocate the main list items
        this.parent(actor, box, flags);

        let arrowHeight = Math.floor(this.actor.get_theme_node().get_padding(St.Side.BOTTOM) / 3);
        let arrowWidth = arrowHeight * 2;

        // Now allocate each arrow underneath its item
        let childBox = new Clutter.ActorBox();
        for (let i = 0; i < this._items.length; i++) {
            let itemBox = this._items[i].allocation;
            childBox.x1 = Math.floor(itemBox.x1 + (itemBox.x2 - itemBox.x1 - arrowWidth) / 2);
            childBox.x2 = childBox.x1 + arrowWidth;
            childBox.y1 = itemBox.y2 + arrowHeight;
            childBox.y2 = childBox.y1 + arrowHeight;
            this._arrows[i].allocate(childBox, flags);
        }
    },

    // We override SwitcherList's _onItemEnter method to delay
    // activation when the thumbnail list is open
    _onItemEnter: function (index) {
        if (this._mouseTimeOutId != 0)
            Mainloop.source_remove(this._mouseTimeOutId);
        if (this._altTabPopup.thumbnailsVisible) {
            this._mouseTimeOutId = Mainloop.timeout_add(APP_ICON_HOVER_TIMEOUT,
                                                        Lang.bind(this, function () {
                                                                            this._enterItem(index);
                                                                            this._mouseTimeOutId = 0;
                                                                            return GLib.SOURCE_REMOVE;
                                                        }));
        } else
           this._itemEntered(index);
    },

    _enterItem: function(index) {
        let [x, y, mask] = global.get_pointer();
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.ALL, x, y);
        if (this._items[index].contains(pickedActor))
            this._itemEntered(index);
    },

    // We override SwitcherList's highlight() method to also deal with
    // the AppSwitcher->ThumbnailList arrows. Apps with only 1 window
    // will hide their arrows by default, but show them when their
    // thumbnails are visible (ie, when the app icon is supposed to be
    // in justOutline mode). Apps with multiple windows will normally
    // show a dim arrow, but show a bright arrow when they are
    // highlighted.
    highlight : function(n, justOutline) {
        if (this._curApp != -1) {
            if (this.icons[this._curApp].cachedWindows.length == 1)
                this._arrows[this._curApp].hide();
            else
                this._arrows[this._curApp].remove_style_pseudo_class('highlighted');
        }

        this.parent(n, justOutline);
        this._curApp = n;

        if (this._curApp != -1) {
            if (justOutline && this.icons[this._curApp].cachedWindows.length == 1)
                this._arrows[this._curApp].show();
            else
                this._arrows[this._curApp].add_style_pseudo_class('highlighted');
        }
    },

    _addIcon : function(appIcon) {
        this.icons.push(appIcon);
        let item = this.addItem(appIcon.actor, appIcon.label);

        let n = this._arrows.length;
        let arrow = new St.DrawingArea({ style_class: 'switcher-arrow' });
        arrow.connect('repaint', function() { SwitcherPopup.drawArrow(arrow, St.Side.BOTTOM); });
        this._list.add_actor(arrow);
        this._arrows.push(arrow);

        if (appIcon.cachedWindows.length == 1)
            arrow.hide();
        else
            item.add_accessible_state (Atk.StateType.EXPANDABLE);
    }
});

const ThumbnailList = new Lang.Class({
    Name: 'ThumbnailList',
    Extends: SwitcherPopup.SwitcherList,

    _init : function(windows) {
        this.parent(false);

        this._labels = new Array();
        this._thumbnailBins = new Array();
        this._clones = new Array();
        this._windows = windows;

        for (let i = 0; i < windows.length; i++) {
            let box = new St.BoxLayout({ style_class: 'thumbnail-box',
                                         vertical: true });

            let bin = new St.Bin({ style_class: 'thumbnail' });

            box.add_actor(bin);
            this._thumbnailBins.push(bin);

            let title = windows[i].get_title();
            if (title) {
                let name = new St.Label({ text: title });
                // St.Label doesn't support text-align so use a Bin
                let bin = new St.Bin({ x_align: St.Align.MIDDLE });
                this._labels.push(bin);
                bin.add_actor(name);
                box.add_actor(bin);

                this.addItem(box, name);
            } else {
                this.addItem(box, null);
            }

        }
    },

    addClones : function (availHeight) {
        if (!this._thumbnailBins.length)
            return;
        let totalPadding = this._items[0].get_theme_node().get_horizontal_padding() + this._items[0].get_theme_node().get_vertical_padding();
        totalPadding += this.actor.get_theme_node().get_horizontal_padding() + this.actor.get_theme_node().get_vertical_padding();
        let [labelMinHeight, labelNaturalHeight] = this._labels[0].get_preferred_height(-1);
        let spacing = this._items[0].child.get_theme_node().get_length('spacing');

        availHeight = Math.min(availHeight - labelNaturalHeight - totalPadding - spacing, THUMBNAIL_DEFAULT_SIZE);
        let binHeight = availHeight + this._items[0].get_theme_node().get_vertical_padding() + this.actor.get_theme_node().get_vertical_padding() - spacing;
        binHeight = Math.min(THUMBNAIL_DEFAULT_SIZE, binHeight);

        for (let i = 0; i < this._thumbnailBins.length; i++) {
            let mutterWindow = this._windows[i].get_compositor_private();
            if (!mutterWindow)
                continue;

            let clone = _createWindowClone(mutterWindow, THUMBNAIL_DEFAULT_SIZE);
            this._thumbnailBins[i].set_height(binHeight);
            this._thumbnailBins[i].add_actor(clone);
            this._clones.push(clone);
        }

        // Make sure we only do this once
        this._thumbnailBins = new Array();
    }
});

const WindowIcon = new Lang.Class({
    Name: 'WindowIcon',

    _init: function(window, mode) {
        this.window = window;

        this.actor = new St.BoxLayout({ style_class: 'alt-tab-app',
                                        vertical: true });
        this._icon = new St.Widget({ layout_manager: new Clutter.BinLayout() });

        this.actor.add(this._icon, { x_fill: false, y_fill: false } );
        this.label = new St.Label({ text: window.get_title() });

        let tracker = Shell.WindowTracker.get_default();
        this.app = tracker.get_window_app(window);

        let mutterWindow = this.window.get_compositor_private();
        let size;

        this._icon.destroy_all_children();

        switch (mode) {
            case AppIconMode.THUMBNAIL_ONLY:
                size = WINDOW_PREVIEW_SIZE;
                this._icon.add_actor(_createWindowClone(mutterWindow, WINDOW_PREVIEW_SIZE));
                break;

            case AppIconMode.BOTH:
                size = WINDOW_PREVIEW_SIZE;
                this._icon.add_actor(_createWindowClone(mutterWindow, WINDOW_PREVIEW_SIZE));

                if (this.app)
                    this._icon.add_actor(this._createAppIcon(this.app,
                                                             APP_ICON_SIZE_SMALL));
                break;

            case AppIconMode.APP_ICON_ONLY:
                size = APP_ICON_SIZE;
                this._icon.add_actor(this._createAppIcon(this.app, size));
        }

        this._icon.set_size(size, size);
    },

    _createAppIcon: function(app, size) {
        let appIcon = app ? app.create_icon_texture(size)
                          : new St.Icon({ icon_name: 'icon-missing',
                                          icon_size: size });
        appIcon.x_expand = appIcon.y_expand = true;
        appIcon.x_align = appIcon.y_align = Clutter.ActorAlign.END;

        return appIcon;
    }
});

const WindowList = new Lang.Class({
    Name: 'WindowList',
    Extends: SwitcherPopup.SwitcherList,

    _init : function(windows, mode) {
        this.parent(true);

        this._label = new St.Label({ x_align: Clutter.ActorAlign.CENTER,
                                     y_align: Clutter.ActorAlign.CENTER });
        this.actor.add_actor(this._label);

        this.windows = windows;
        this.icons = [];

        for (let i = 0; i < windows.length; i++) {
            let win = windows[i];
            let icon = new WindowIcon(win, mode);

            this.addItem(icon.actor, icon.label);
            this.icons.push(icon);
        }
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        this.parent(actor, forWidth, alloc);

        let spacing = this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
        let [labelMin, labelNat] = this._label.get_preferred_height(-1);
        alloc.min_size += labelMin + spacing;
        alloc.natural_size += labelNat + spacing;
    },

    _allocateTop: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        childBox.x1 = box.x1;
        childBox.x2 = box.x2;
        childBox.y2 = box.y2;
        childBox.y1 = childBox.y2 - this._label.height;
        this._label.allocate(childBox, flags);

        let spacing = this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
        box.y2 -= this._label.height + spacing;
        this.parent(actor, box, flags);
    },

    highlight: function(index, justOutline) {
        this.parent(index, justOutline);

        this._label.set_text(index == -1 ? '' : this.icons[index].label.text);
    }
});
