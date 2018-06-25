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

var APP_ICON_HOVER_TIMEOUT = 200; // milliseconds

var THUMBNAIL_DEFAULT_SIZE = 256;
var THUMBNAIL_POPUP_TIME = 500; // milliseconds
var THUMBNAIL_FADE_TIME = 0.1; // seconds

var WINDOW_PREVIEW_SIZE = 128;
var APP_ICON_SIZE = 96;
var APP_ICON_SIZE_SMALL = 48;

const baseIconSizes = [96, 64, 48, 32, 22];

var AppIconMode = {
    THUMBNAIL_ONLY: 1,
    APP_ICON_ONLY: 2,
    BOTH: 3,
};

function _createWindowClone(window, size) {
    let [width, height] = window.get_size();
    let scale = Math.min(1.0, size / width, size / height);
    return new Clutter.Clone({ source: window,
                               width: width * scale,
                               height: height * scale,
                               x_align: Clutter.ActorAlign.CENTER,
                               y_align: Clutter.ActorAlign.CENTER,
                               // usual hack for the usual bug in ClutterBinLayout...
                               x_expand: true,
                               y_expand: true });
};

function getWindows(workspace) {
    // We ignore skip-taskbar windows in switchers, but if they are attached
    // to their parent, their position in the MRU list may be more appropriate
    // than the parent; so start with the complete list ...
    let windows = global.display.get_tab_list(Meta.TabList.NORMAL_ALL,
                                              workspace);
    // ... map windows to their parent where appropriate ...
    return windows.map(w => {
        return w.is_attached_dialog() ? w.get_transient_for() : w;
    // ... and filter out skip-taskbar windows and duplicates
    }).filter((w, i, a) => !w.skip_taskbar && a.indexOf(w) == i);
}

var AppSwitcherPopup = new Lang.Class({
    Name: 'AppSwitcherPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init() {
        this.parent();

        this._thumbnails = null;
        this._thumbnailTimeoutId = 0;
        this._currentWindow = -1;

        this.thumbnailsVisible = false;

        let apps = Shell.AppSystem.get_default().get_running ();

        if (apps.length == 0)
            return;

        this._switcherList = new AppSwitcher(apps, this);
        this._items = this._switcherList.icons;
    },

    _allocate(actor, box, flags) {
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

    _initialSelection(backward, binding) {
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

    _nextWindow() {
        // We actually want the second window if we're in the unset state
        if (this._currentWindow == -1)
            this._currentWindow = 0;
        return SwitcherPopup.mod(this._currentWindow + 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    },
    _previousWindow() {
        // Also assume second window here
        if (this._currentWindow == -1)
            this._currentWindow = 1;
        return SwitcherPopup.mod(this._currentWindow - 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    },

    _closeAppWindow(appIndex, windowIndex) {
        let appIcon = this._items[appIndex];
        if (!appIcon)
            return;

        let window = appIcon.cachedWindows[windowIndex];
        if (!window)
            return;

        window.delete(global.get_current_time());
    },

    _quitApplication(appIndex) {
        let appIcon = this._items[appIndex];
        if (!appIcon)
            return;

        appIcon.app.request_quit();
    },

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.SWITCH_GROUP) {
            if (!this._thumbnailsFocused)
                this._select(this._selectedIndex, 0);
            else
                this._select(this._selectedIndex, this._nextWindow());
        } else if (action == Meta.KeyBindingAction.SWITCH_GROUP_BACKWARD) {
            this._select(this._selectedIndex, this._previousWindow());
        } else if (action == Meta.KeyBindingAction.SWITCH_APPLICATIONS) {
            this._select(this._next());
        } else if (action == Meta.KeyBindingAction.SWITCH_APPLICATIONS_BACKWARD) {
            this._select(this._previous());
        } else if (keysym == Clutter.q) {
            this._quitApplication(this._selectedIndex);
        } else if (this._thumbnailsFocused) {
            if (keysym == Clutter.Left)
                this._select(this._selectedIndex, this._previousWindow());
            else if (keysym == Clutter.Right)
                this._select(this._selectedIndex, this._nextWindow());
            else if (keysym == Clutter.Up)
                this._select(this._selectedIndex, null, true);
            else if (keysym == Clutter.w || keysym == Clutter.F4)
                this._closeAppWindow(this._selectedIndex, this._currentWindow);
            else
                return Clutter.EVENT_PROPAGATE;
        } else {
            if (keysym == Clutter.Left)
                this._select(this._previous());
            else if (keysym == Clutter.Right)
                this._select(this._next());
            else if (keysym == Clutter.Down)
                this._select(this._selectedIndex, 0);
            else
                return Clutter.EVENT_PROPAGATE;
        }

        return Clutter.EVENT_STOP;
    },

    _scrollHandler(direction) {
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

    _itemActivatedHandler(n) {
        // If the user clicks on the selected app, activate the
        // selected window; otherwise (eg, they click on an app while
        // !mouseActive) activate the clicked-on app.
        if (n == this._selectedIndex && this._currentWindow >= 0)
            this._select(n, this._currentWindow);
        else
            this._select(n);
    },

    _itemEnteredHandler(n) {
        this._select(n);
    },

    _windowActivated(thumbnailList, n) {
        let appIcon = this._items[this._selectedIndex];
        Main.activateWindow(appIcon.cachedWindows[n]);
        this.destroy();
    },

    _windowEntered(thumbnailList, n) {
        if (!this.mouseActive)
            return;

        this._select(this._selectedIndex, n);
    },

    _windowRemoved(thumbnailList, n) {
        let appIcon = this._items[this._selectedIndex];
        if (!appIcon)
            return;

        if (appIcon.cachedWindows.length > 0) {
            let newIndex = Math.min(n, appIcon.cachedWindows.length - 1);
            this._select(this._selectedIndex, newIndex);
        }
    },

    _finish(timestamp) {
        let appIcon = this._items[this._selectedIndex];
        if (this._currentWindow < 0)
            appIcon.app.activate_window(appIcon.cachedWindows[0], timestamp);
        else if (appIcon.cachedWindows[this._currentWindow])
            Main.activateWindow(appIcon.cachedWindows[this._currentWindow], timestamp);

        this.parent();
    },

    _onDestroy() {
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
    _select(app, window, forceAppFocus) {
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
                this._timeoutPopupThumbnails.bind(this));
            GLib.Source.set_name_by_id(this._thumbnailTimeoutId, '[gnome-shell] this._timeoutPopupThumbnails');
        }
    },

    _timeoutPopupThumbnails() {
        if (!this._thumbnails)
            this._createThumbnails();
        this._thumbnailTimeoutId = 0;
        this._thumbnailsFocused = false;
        return GLib.SOURCE_REMOVE;
    },

    _destroyThumbnails() {
        let thumbnailsActor = this._thumbnails.actor;
        Tweener.addTween(thumbnailsActor,
                         { opacity: 0,
                           time: THUMBNAIL_FADE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                               thumbnailsActor.destroy();
                               this.thumbnailsVisible = false;
                           }
                         });
        this._thumbnails = null;
        if  (this._switcherList._items[this._selectedIndex])
            this._switcherList._items[this._selectedIndex].remove_accessible_state (Atk.StateType.EXPANDED);
    },

    _createThumbnails() {
        this._thumbnails = new ThumbnailList (this._items[this._selectedIndex].cachedWindows);
        this._thumbnails.connect('item-activated', this._windowActivated.bind(this));
        this._thumbnails.connect('item-entered', this._windowEntered.bind(this));
        this._thumbnails.connect('item-removed', this._windowRemoved.bind(this));
        this._thumbnails.actor.connect('destroy', () => {
            this._thumbnails = null;
            this._thumbnailsFocused = false;
        });

        this.actor.add_actor(this._thumbnails.actor);

        // Need to force an allocation so we can figure out whether we
        // need to scroll when selecting
        this._thumbnails.actor.get_allocation_box();

        this._thumbnails.actor.opacity = 0;
        Tweener.addTween(this._thumbnails.actor,
                         { opacity: 255,
                           time: THUMBNAIL_FADE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => { this.thumbnailsVisible = true; }
                         });

        this._switcherList._items[this._selectedIndex].add_accessible_state (Atk.StateType.EXPANDED);
    }
});

var CyclerHighlight = new Lang.Class({
    Name: 'CyclerHighlight',

    _init() {
        this._window = null;

        this.actor = new St.Widget({ layout_manager: new Clutter.BinLayout() });

        this._clone = new Clutter.Clone();
        this.actor.add_actor(this._clone);

        this._highlight = new St.Widget({ style_class: 'cycler-highlight' });
        this.actor.add_actor(this._highlight);

        let coordinate = Clutter.BindCoordinate.ALL;
        let constraint = new Clutter.BindConstraint({ coordinate: coordinate });
        this._clone.bind_property('source', constraint, 'source', 0);

        this.actor.add_constraint(constraint);

        this.actor.connect('notify::allocation',
                           this._onAllocationChanged.bind(this));
        this.actor.connect('destroy', this._onDestroy.bind(this));
    },

    set window(w) {
        if (this._window == w)
            return;

        this._window = w;

        if (this._clone.source)
            this._clone.source.sync_visibility();

        let windowActor = this._window ? this._window.get_compositor_private()
                                       : null;

        if (windowActor)
            windowActor.hide();

        this._clone.source = windowActor;
    },

    _onAllocationChanged() {
        if (!this._window) {
            this._highlight.set_size(0, 0);
            this._highlight.hide();
        } else {
            let [x, y] = this.actor.allocation.get_origin();
            let rect = this._window.get_frame_rect();
            this._highlight.set_size(rect.width, rect.height);
            this._highlight.set_position(rect.x - x, rect.y - y);
            this._highlight.show();
        }
    },

    _onDestroy() {
        this.window = null;
    }
});

var CyclerPopup = new Lang.Class({
    Name: 'CyclerPopup',
    Extends: SwitcherPopup.SwitcherPopup,
    Abstract: true,

    _init() {
        this.parent();

        this._items = this._getWindows();

        if (this._items.length == 0)
            return;

        this._highlight = new CyclerHighlight();
        global.window_group.add_actor(this._highlight.actor);

        // We don't show an actual popup, so just provide what SwitcherPopup
        // expects instead of inheriting from SwitcherList
        this._switcherList = { actor: new St.Widget(),
                               highlight: this._highlightItem.bind(this),
                               connect() {} };
    },

    _highlightItem(index, justOutline) {
        this._highlight.window = this._items[index];
        global.window_group.set_child_above_sibling(this._highlight.actor, null);
    },

    _finish() {
        let window = this._items[this._selectedIndex];
        let ws = window.get_workspace();
        let workspaceManager = global.workspace_manager;
        let activeWs = workspaceManager.get_active_workspace();

        if (window.minimized) {
            Main.wm.skipNextEffect(window.get_compositor_private());
            window.unminimize();
        }

        if (activeWs == ws) {
            Main.activateWindow(window);
        } else {
            // If the selected window is on a different workspace, we don't
            // want it to disappear, then slide in with the workspace; instead,
            // always activate it on the active workspace ...
            activeWs.activate_with_focus(window, global.get_current_time());

            // ... then slide it over to the original workspace if necessary
            Main.wm.actionMoveWindow(window, ws);
        }

        this.parent();
    },

    _onDestroy() {
        this._highlight.actor.destroy();

        this.parent();
    }
});


var GroupCyclerPopup = new Lang.Class({
    Name: 'GroupCyclerPopup',
    Extends: CyclerPopup,

    _getWindows() {
        let app = Shell.WindowTracker.get_default().focus_app;
        return app ? app.get_windows() : [];
    },

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.CYCLE_GROUP)
            this._select(this._next());
        else if (action == Meta.KeyBindingAction.CYCLE_GROUP_BACKWARD)
            this._select(this._previous());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    }
});

var WindowSwitcherPopup = new Lang.Class({
    Name: 'WindowSwitcherPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init() {
        this.parent();
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell.window-switcher' });

        let windows = this._getWindowList();

        if (windows.length == 0)
            return;

        let mode = this._settings.get_enum('app-icon-mode');
        this._switcherList = new WindowList(windows, mode);
        this._items = this._switcherList.icons;
    },

    _getWindowList() {
        let workspace = null;

        if (this._settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        return getWindows(workspace);
    },

    _closeWindow(windowIndex) {
        let windowIcon = this._items[windowIndex];
        if (!windowIcon)
            return;

        windowIcon.window.delete(global.get_current_time());
    },

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.SWITCH_WINDOWS) {
            this._select(this._next());
        } else if (action == Meta.KeyBindingAction.SWITCH_WINDOWS_BACKWARD) {
            this._select(this._previous());
        } else {
            if (keysym == Clutter.Left)
                this._select(this._previous());
            else if (keysym == Clutter.Right)
                this._select(this._next());
            else if (keysym == Clutter.w || keysym == Clutter.F4)
                this._closeWindow(this._selectedIndex);
            else
                return Clutter.EVENT_PROPAGATE;
        }

        return Clutter.EVENT_STOP;
    },

    _finish() {
        Main.activateWindow(this._items[this._selectedIndex].window);

        this.parent();
    }
});

var WindowCyclerPopup = new Lang.Class({
    Name: 'WindowCyclerPopup',
    Extends: CyclerPopup,

    _init() {
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell.window-switcher' });
        this.parent();
    },

    _getWindows() {
        let workspace = null;

        if (this._settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        return getWindows(workspace);
    },

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.CYCLE_WINDOWS)
            this._select(this._next());
        else if (action == Meta.KeyBindingAction.CYCLE_WINDOWS_BACKWARD)
            this._select(this._previous());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    }
});

var AppIcon = new Lang.Class({
    Name: 'AppIcon',

    _init(app) {
        this.app = app;
        this.actor = new St.BoxLayout({ style_class: 'alt-tab-app',
                                         vertical: true });
        this.icon = null;
        this._iconBin = new St.Bin({ x_fill: true, y_fill: true });

        this.actor.add(this._iconBin, { x_fill: false, y_fill: false } );
        this.label = new St.Label({ text: this.app.get_name() });
        this.actor.add(this.label, { x_fill: false });
    },

    set_size(size) {
        this.icon = this.app.create_icon_texture(size);
        this._iconBin.child = this.icon;
    }
});

var AppSwitcher = new Lang.Class({
    Name: 'AppSwitcher',
    Extends: SwitcherPopup.SwitcherList,

    _init(apps, altTabPopup) {
        this.parent(true);

        this.icons = [];
        this._arrows = [];

        let windowTracker = Shell.WindowTracker.get_default();
        let settings = new Gio.Settings({ schema_id: 'org.gnome.shell.app-switcher' });

        let workspace = null;
        if (settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        let allWindows = global.display.get_tab_list(Meta.TabList.NORMAL, workspace);

        // Construct the AppIcons, add to the popup
        for (let i = 0; i < apps.length; i++) {
            let appIcon = new AppIcon(apps[i]);
            // Cache the window list now; we don't handle dynamic changes here,
            // and we don't want to be continually retrieving it
            appIcon.cachedWindows = allWindows.filter(
                w => windowTracker.get_window_app (w) == appIcon.app
            );
            if (appIcon.cachedWindows.length > 0)
                this._addIcon(appIcon);
        }

        this._curApp = -1;
        this._iconSize = 0;
        this._altTabPopup = altTabPopup;
        this._mouseTimeOutId = 0;

        this.actor.connect('destroy', this._onDestroy.bind(this));
    },

    _onDestroy() {
        if (this._mouseTimeOutId != 0)
            Mainloop.source_remove(this._mouseTimeOutId);

        this.icons.forEach(icon => {
            icon.app.disconnect(icon._stateChangedId);
        });
    },

    _setIconSize() {
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
        let iconSizes = baseIconSizes.map(s => s * scaleFactor);

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

    _getPreferredHeight(actor, forWidth, alloc) {
        this._setIconSize();
        this.parent(actor, forWidth, alloc);
    },

    _allocate(actor, box, flags) {
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
    _onItemEnter(index) {
        if (this._mouseTimeOutId != 0)
            Mainloop.source_remove(this._mouseTimeOutId);
        if (this._altTabPopup.thumbnailsVisible) {
            this._mouseTimeOutId = Mainloop.timeout_add(APP_ICON_HOVER_TIMEOUT,
                                                        () => {
                                                            this._enterItem(index);
                                                            this._mouseTimeOutId = 0;
                                                            return GLib.SOURCE_REMOVE;
                                                        });
            GLib.Source.set_name_by_id(this._mouseTimeOutId, '[gnome-shell] this._enterItem');
        } else
           this._itemEntered(index);
    },

    _enterItem(index) {
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
    highlight(n, justOutline) {
        if (this.icons[this._curApp]) {
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

    _addIcon(appIcon) {
        this.icons.push(appIcon);
        let item = this.addItem(appIcon.actor, appIcon.label);

        appIcon._stateChangedId = appIcon.app.connect('notify::state', app => {
            if (app.state != Shell.AppState.RUNNING)
                this._removeIcon(app);
        });

        let n = this._arrows.length;
        let arrow = new St.DrawingArea({ style_class: 'switcher-arrow' });
        arrow.connect('repaint', () => { SwitcherPopup.drawArrow(arrow, St.Side.BOTTOM); });
        this._list.add_actor(arrow);
        this._arrows.push(arrow);

        if (appIcon.cachedWindows.length == 1)
            arrow.hide();
        else
            item.add_accessible_state (Atk.StateType.EXPANDABLE);
    },

    _removeIcon(app) {
        let index = this.icons.findIndex(icon => {
            return icon.app == app;
        });
        if (index === -1)
            return;

        this.icons.splice(index, 1);
        this.removeItem(index);
    },
});

var ThumbnailList = new Lang.Class({
    Name: 'ThumbnailList',
    Extends: SwitcherPopup.SwitcherList,

    _init(windows) {
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

        this.actor.connect('destroy', this._onDestroy.bind(this));
    },

    addClones(availHeight) {
        if (!this._thumbnailBins.length)
            return;
        let totalPadding = this._items[0].get_theme_node().get_horizontal_padding() + this._items[0].get_theme_node().get_vertical_padding();
        totalPadding += this.actor.get_theme_node().get_horizontal_padding() + this.actor.get_theme_node().get_vertical_padding();
        let [labelMinHeight, labelNaturalHeight] = this._labels[0].get_preferred_height(-1);
        let spacing = this._items[0].child.get_theme_node().get_length('spacing');
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let thumbnailSize = THUMBNAIL_DEFAULT_SIZE * scaleFactor;

        availHeight = Math.min(availHeight - labelNaturalHeight - totalPadding - spacing, thumbnailSize);
        let binHeight = availHeight + this._items[0].get_theme_node().get_vertical_padding() + this.actor.get_theme_node().get_vertical_padding() - spacing;
        binHeight = Math.min(thumbnailSize, binHeight);

        for (let i = 0; i < this._thumbnailBins.length; i++) {
            let mutterWindow = this._windows[i].get_compositor_private();
            if (!mutterWindow)
                continue;

            let clone = _createWindowClone(mutterWindow, thumbnailSize);
            this._thumbnailBins[i].set_height(binHeight);
            this._thumbnailBins[i].add_actor(clone);

            clone._destroyId = mutterWindow.connect('destroy', source => {
                this._removeThumbnail(source, clone);
            });
            this._clones.push(clone);
        }

        // Make sure we only do this once
        this._thumbnailBins = new Array();
    },

    _removeThumbnail(source, clone) {
        let index = this._clones.indexOf(clone);
        if (index === -1)
            return;

        this._clones.splice(index, 1);
        this._windows.splice(index, 1);
        this._labels.splice(index, 1);
        this.removeItem(index);

        if (this._clones.length > 0)
            this.highlight(SwitcherPopup.mod(index, this._clones.length));
        else
            this.actor.destroy();
    },

    _onDestroy() {
        this._clones.forEach(clone => {
            if (clone.source)
                clone.source.disconnect(clone._destroyId);
        });
    },

});

var WindowIcon = new Lang.Class({
    Name: 'WindowIcon',

    _init(window, mode) {
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

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;

        switch (mode) {
            case AppIconMode.THUMBNAIL_ONLY:
                size = WINDOW_PREVIEW_SIZE;
                this._icon.add_actor(_createWindowClone(mutterWindow, size * scaleFactor));
                break;

            case AppIconMode.BOTH:
                size = WINDOW_PREVIEW_SIZE;
                this._icon.add_actor(_createWindowClone(mutterWindow, size * scaleFactor));

                if (this.app)
                    this._icon.add_actor(this._createAppIcon(this.app,
                                                             APP_ICON_SIZE_SMALL));
                break;

            case AppIconMode.APP_ICON_ONLY:
                size = APP_ICON_SIZE;
                this._icon.add_actor(this._createAppIcon(this.app, size));
        }

        this._icon.set_size(size * scaleFactor, size * scaleFactor);
    },

    _createAppIcon(app, size) {
        let appIcon = app ? app.create_icon_texture(size)
                          : new St.Icon({ icon_name: 'icon-missing',
                                          icon_size: size });
        appIcon.x_expand = appIcon.y_expand = true;
        appIcon.x_align = appIcon.y_align = Clutter.ActorAlign.END;

        return appIcon;
    }
});

var WindowList = new Lang.Class({
    Name: 'WindowList',
    Extends: SwitcherPopup.SwitcherList,

    _init(windows, mode) {
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

            icon._unmanagedSignalId = icon.window.connect('unmanaged', (window) => {
                this._removeWindow(window)
            });
        }

        this.actor.connect('destroy', () => { this._onDestroy(); });
    },

    _onDestroy() {
        this.icons.forEach(icon => {
            icon.window.disconnect(icon._unmanagedSignalId);
        });
    },

    _getPreferredHeight(actor, forWidth, alloc) {
        this.parent(actor, forWidth, alloc);

        let spacing = this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
        let [labelMin, labelNat] = this._label.get_preferred_height(-1);
        alloc.min_size += labelMin + spacing;
        alloc.natural_size += labelNat + spacing;
    },

    _allocateTop(actor, box, flags) {
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

    highlight(index, justOutline) {
        this.parent(index, justOutline);

        this._label.set_text(index == -1 ? '' : this.icons[index].label.text);
    },

    _removeWindow(window) {
        let index = this.icons.findIndex(icon => {
            return icon.window == window;
        });
        if (index === -1)
            return;

        this.icons.splice(index, 1);
        this.removeItem(index);
    }
});
