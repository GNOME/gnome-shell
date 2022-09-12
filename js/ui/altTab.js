// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AppSwitcherPopup, GroupCyclerPopup, WindowSwitcherPopup,
            WindowCyclerPopup */

const { Atk, Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;

const Main = imports.ui.main;
const SwitcherPopup = imports.ui.switcherPopup;

var APP_ICON_HOVER_TIMEOUT = 200; // milliseconds

var THUMBNAIL_DEFAULT_SIZE = 256;
var THUMBNAIL_POPUP_TIME = 500; // milliseconds
var THUMBNAIL_FADE_TIME = 100; // milliseconds

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
    return new Clutter.Clone({
        source: window,
        width: width * scale,
        height: height * scale,
        x_align: Clutter.ActorAlign.CENTER,
        y_align: Clutter.ActorAlign.CENTER,
        // usual hack for the usual bug in ClutterBinLayout...
        x_expand: true,
        y_expand: true,
    });
}

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

var AppSwitcherPopup = GObject.registerClass(
class AppSwitcherPopup extends SwitcherPopup.SwitcherPopup {
    _init() {
        super._init();

        this._thumbnails = null;
        this._thumbnailTimeoutId = 0;
        this._currentWindow = -1;

        this.thumbnailsVisible = false;

        let apps = Shell.AppSystem.get_default().get_running();

        this._switcherList = new AppSwitcher(apps, this);
        this._items = this._switcherList.icons;
    }

    vfunc_allocate(box) {
        super.vfunc_allocate(box);

        // Allocate the thumbnails
        // We try to avoid overflowing the screen so we base the resulting size on
        // those calculations
        if (this._thumbnails) {
            let childBox = this._switcherList.get_allocation_box();
            let primary = Main.layoutManager.primaryMonitor;

            let leftPadding = this.get_theme_node().get_padding(St.Side.LEFT);
            let rightPadding = this.get_theme_node().get_padding(St.Side.RIGHT);
            let bottomPadding = this.get_theme_node().get_padding(St.Side.BOTTOM);
            let hPadding = leftPadding + rightPadding;

            let icon = this._items[this._selectedIndex];
            let [posX] = icon.get_transformed_position();
            let thumbnailCenter = posX + icon.width / 2;
            let [, childNaturalWidth] = this._thumbnails.get_preferred_width(-1);
            childBox.x1 = Math.max(primary.x + leftPadding, Math.floor(thumbnailCenter - childNaturalWidth / 2));
            if (childBox.x1 + childNaturalWidth > primary.x + primary.width - hPadding) {
                let offset = childBox.x1 + childNaturalWidth - primary.width + hPadding;
                childBox.x1 = Math.max(primary.x + leftPadding, childBox.x1 - offset - hPadding);
            }

            let spacing = this.get_theme_node().get_length('spacing');

            childBox.x2 = childBox.x1 +  childNaturalWidth;
            if (childBox.x2 > primary.x + primary.width - rightPadding)
                childBox.x2 = primary.x + primary.width - rightPadding;
            childBox.y1 = this._switcherList.allocation.y2 + spacing;
            this._thumbnails.addClones(primary.y + primary.height - bottomPadding - childBox.y1);
            let [, childNaturalHeight] = this._thumbnails.get_preferred_height(-1);
            childBox.y2 = childBox.y1 + childNaturalHeight;
            this._thumbnails.allocate(childBox);
        }
    }

    _initialSelection(backward, binding) {
        if (binding == 'switch-group') {
            if (backward)
                this._select(0, this._items[0].cachedWindows.length - 1);
            else if (this._items[0].cachedWindows.length > 1)
                this._select(0, 1);
            else
                this._select(0, 0);
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
    }

    _nextWindow() {
        // We actually want the second window if we're in the unset state
        if (this._currentWindow == -1)
            this._currentWindow = 0;
        return SwitcherPopup.mod(this._currentWindow + 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    }

    _previousWindow() {
        // Also assume second window here
        if (this._currentWindow == -1)
            this._currentWindow = 1;
        return SwitcherPopup.mod(this._currentWindow - 1,
                                 this._items[this._selectedIndex].cachedWindows.length);
    }

    _closeAppWindow(appIndex, windowIndex) {
        let appIcon = this._items[appIndex];
        if (!appIcon)
            return;

        let window = appIcon.cachedWindows[windowIndex];
        if (!window)
            return;

        window.delete(global.get_current_time());
    }

    _quitApplication(appIndex) {
        let appIcon = this._items[appIndex];
        if (!appIcon)
            return;

        appIcon.app.request_quit();
    }

    _keyPressHandler(keysym, action) {
        const rtl = Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
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
        } else if (keysym == Clutter.KEY_q || keysym === Clutter.KEY_Q) {
            this._quitApplication(this._selectedIndex);
        } else if (this._thumbnailsFocused) {
            if (keysym === Clutter.KEY_Left)
                this._select(this._selectedIndex, rtl ? this._nextWindow() : this._previousWindow());
            else if (keysym === Clutter.KEY_Right)
                this._select(this._selectedIndex, rtl ? this._previousWindow() : this._nextWindow());
            else if (keysym === Clutter.KEY_Up)
                this._select(this._selectedIndex, null, true);
            else if (keysym === Clutter.KEY_w || keysym === Clutter.KEY_W || keysym === Clutter.KEY_F4)
                this._closeAppWindow(this._selectedIndex, this._currentWindow);
            else
                return Clutter.EVENT_PROPAGATE;
        } else if (keysym == Clutter.KEY_Left) {
            this._select(rtl ? this._next() : this._previous());
        } else if (keysym == Clutter.KEY_Right) {
            this._select(rtl ? this._previous() : this._next());
        } else if (keysym == Clutter.KEY_Down) {
            this._select(this._selectedIndex, 0);
        } else {
            return Clutter.EVENT_PROPAGATE;
        }

        return Clutter.EVENT_STOP;
    }

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
    }

    _itemActivatedHandler(n) {
        // If the user clicks on the selected app, activate the
        // selected window; otherwise (eg, they click on an app while
        // !mouseActive) activate the clicked-on app.
        if (n == this._selectedIndex && this._currentWindow >= 0)
            this._select(n, this._currentWindow);
        else
            this._select(n);
    }

    _windowActivated(thumbnailSwitcher, n) {
        let appIcon = this._items[this._selectedIndex];
        Main.activateWindow(appIcon.cachedWindows[n]);
        this.fadeAndDestroy();
    }

    _windowEntered(thumbnailSwitcher, n) {
        if (!this.mouseActive)
            return;

        this._select(this._selectedIndex, n);
    }

    _windowRemoved(thumbnailSwitcher, n) {
        let appIcon = this._items[this._selectedIndex];
        if (!appIcon)
            return;

        if (appIcon.cachedWindows.length > 0) {
            let newIndex = Math.min(n, appIcon.cachedWindows.length - 1);
            this._select(this._selectedIndex, newIndex);
        }
    }

    _finish(timestamp) {
        let appIcon = this._items[this._selectedIndex];
        if (this._currentWindow < 0)
            appIcon.app.activate_window(appIcon.cachedWindows[0], timestamp);
        else if (appIcon.cachedWindows[this._currentWindow])
            Main.activateWindow(appIcon.cachedWindows[this._currentWindow], timestamp);

        super._finish(timestamp);
    }

    _onDestroy() {
        if (this._thumbnailTimeoutId != 0)
            GLib.source_remove(this._thumbnailTimeoutId);

        super._onDestroy();
    }

    /**
     * _select:
     * @param {number} app: index of the app to select
     * @param {number=} window: index of which of @app's windows to select
     * @param {bool} forceAppFocus: optional flag, see below
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
            GLib.source_remove(this._thumbnailTimeoutId);
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
            this._thumbnailTimeoutId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                THUMBNAIL_POPUP_TIME,
                this._timeoutPopupThumbnails.bind(this));
            GLib.Source.set_name_by_id(this._thumbnailTimeoutId, '[gnome-shell] this._timeoutPopupThumbnails');
        }
    }

    _timeoutPopupThumbnails() {
        if (!this._thumbnails)
            this._createThumbnails();
        this._thumbnailTimeoutId = 0;
        this._thumbnailsFocused = false;
        return GLib.SOURCE_REMOVE;
    }

    _destroyThumbnails() {
        let thumbnailsActor = this._thumbnails;
        this._thumbnails.ease({
            opacity: 0,
            duration: THUMBNAIL_FADE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                thumbnailsActor.destroy();
                this.thumbnailsVisible = false;
            },
        });
        this._thumbnails = null;
        this._switcherList.removeAccessibleState(this._selectedIndex, Atk.StateType.EXPANDED);
    }

    _createThumbnails() {
        this._thumbnails = new ThumbnailSwitcher(this._items[this._selectedIndex].cachedWindows);
        this._thumbnails.connect('item-activated', this._windowActivated.bind(this));
        this._thumbnails.connect('item-entered', this._windowEntered.bind(this));
        this._thumbnails.connect('item-removed', this._windowRemoved.bind(this));
        this._thumbnails.connect('destroy', () => {
            this._thumbnails = null;
            this._thumbnailsFocused = false;
        });

        this.add_actor(this._thumbnails);

        // Need to force an allocation so we can figure out whether we
        // need to scroll when selecting
        this._thumbnails.get_allocation_box();

        this._thumbnails.opacity = 0;
        this._thumbnails.ease({
            opacity: 255,
            duration: THUMBNAIL_FADE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this.thumbnailsVisible = true;
            },
        });

        this._switcherList.addAccessibleState(this._selectedIndex, Atk.StateType.EXPANDED);
    }
});

var CyclerHighlight = GObject.registerClass(
class CyclerHighlight extends St.Widget {
    _init() {
        super._init({ layout_manager: new Clutter.BinLayout() });
        this._window = null;

        this._clone = new Clutter.Clone();
        this.add_actor(this._clone);

        this._highlight = new St.Widget({ style_class: 'cycler-highlight' });
        this.add_actor(this._highlight);

        let coordinate = Clutter.BindCoordinate.ALL;
        let constraint = new Clutter.BindConstraint({ coordinate });
        this._clone.bind_property('source', constraint, 'source', 0);

        this.add_constraint(constraint);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    set window(w) {
        if (this._window == w)
            return;

        this._window?.disconnectObject(this);

        this._window = w;

        if (this._clone.source)
            this._clone.source.sync_visibility();

        const windowActor = this._window?.get_compositor_private() ?? null;

        if (windowActor)
            windowActor.hide();

        this._clone.source = windowActor;

        if (this._window) {
            this._onSizeChanged();
            this._window.connectObject('size-changed',
                this._onSizeChanged.bind(this), this);
        } else {
            this._highlight.set_size(0, 0);
            this._highlight.hide();
        }
    }

    _onSizeChanged() {
        const bufferRect = this._window.get_buffer_rect();
        const rect = this._window.get_frame_rect();
        this._highlight.set_size(rect.width, rect.height);
        this._highlight.set_position(
            rect.x - bufferRect.x,
            rect.y - bufferRect.y);
        this._highlight.show();
    }

    _onDestroy() {
        this.window = null;
    }
});

// We don't show an actual popup, so just provide what SwitcherPopup
// expects instead of inheriting from SwitcherList
var CyclerList = GObject.registerClass({
    Signals: {
        'item-activated': { param_types: [GObject.TYPE_INT] },
        'item-entered': { param_types: [GObject.TYPE_INT] },
        'item-removed': { param_types: [GObject.TYPE_INT] },
        'item-highlighted': { param_types: [GObject.TYPE_INT] },
    },
}, class CyclerList extends St.Widget {
    highlight(index, _justOutline) {
        this.emit('item-highlighted', index);
    }
});

var CyclerPopup = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
}, class CyclerPopup extends SwitcherPopup.SwitcherPopup {
    _init() {
        super._init();

        this._items = this._getWindows();

        this._highlight = new CyclerHighlight();
        global.window_group.add_actor(this._highlight);

        this._switcherList = new CyclerList();
        this._switcherList.connect('item-highlighted', (list, index) => {
            this._highlightItem(index);
        });
    }

    _highlightItem(index, _justOutline) {
        this._highlight.window = this._items[index];
        global.window_group.set_child_above_sibling(this._highlight, null);
    }

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

        super._finish();
    }

    _onDestroy() {
        this._highlight.destroy();

        super._onDestroy();
    }
});


var GroupCyclerPopup = GObject.registerClass(
class GroupCyclerPopup extends CyclerPopup {
    _init() {
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell.app-switcher' });
        super._init();
    }

    _getWindows() {
        let app = Shell.WindowTracker.get_default().focus_app;
        let appWindows = app?.get_windows() ?? [];

        if (this._settings.get_boolean('current-workspace-only')) {
            const workspaceManager = global.workspace_manager;
            const workspace = workspaceManager.get_active_workspace();
            appWindows = appWindows.filter(
                window => window.located_on_workspace(workspace));
        }

        return appWindows;
    }

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

var WindowSwitcherPopup = GObject.registerClass(
class WindowSwitcherPopup extends SwitcherPopup.SwitcherPopup {
    _init() {
        super._init();
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell.window-switcher' });

        let windows = this._getWindowList();

        let mode = this._settings.get_enum('app-icon-mode');
        this._switcherList = new WindowSwitcher(windows, mode);
        this._items = this._switcherList.icons;
    }

    _getWindowList() {
        let workspace = null;

        if (this._settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        return getWindows(workspace);
    }

    _closeWindow(windowIndex) {
        let windowIcon = this._items[windowIndex];
        if (!windowIcon)
            return;

        windowIcon.window.delete(global.get_current_time());
    }

    _keyPressHandler(keysym, action) {
        const rtl = Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
        if (action == Meta.KeyBindingAction.SWITCH_WINDOWS)
            this._select(this._next());
        else if (action == Meta.KeyBindingAction.SWITCH_WINDOWS_BACKWARD)
            this._select(this._previous());
        else if (keysym == Clutter.KEY_Left)
            this._select(rtl ? this._next() : this._previous());
        else if (keysym == Clutter.KEY_Right)
            this._select(rtl ? this._previous() : this._next());
        else if (keysym === Clutter.KEY_w || keysym === Clutter.KEY_W || keysym === Clutter.KEY_F4)
            this._closeWindow(this._selectedIndex);
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    }

    _finish() {
        Main.activateWindow(this._items[this._selectedIndex].window);

        super._finish();
    }
});

var WindowCyclerPopup = GObject.registerClass(
class WindowCyclerPopup extends CyclerPopup {
    _init() {
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell.window-switcher' });
        super._init();
    }

    _getWindows() {
        let workspace = null;

        if (this._settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        return getWindows(workspace);
    }

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

var AppIcon = GObject.registerClass(
class AppIcon extends St.BoxLayout {
    _init(app) {
        super._init({
            style_class: 'alt-tab-app',
            vertical: true,
        });

        this.app = app;
        this.icon = null;
        this._iconBin = new St.Bin();

        this.add_child(this._iconBin);
        this.label = new St.Label({
            text: this.app.get_name(),
            x_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this.label);
    }

    // eslint-disable-next-line camelcase
    set_size(size) {
        this.icon = this.app.create_icon_texture(size);
        this._iconBin.child = this.icon;
    }
});

var AppSwitcher = GObject.registerClass(
class AppSwitcher extends SwitcherPopup.SwitcherList {
    _init(apps, altTabPopup) {
        super._init(true);

        this.icons = [];
        this._arrows = [];

        let windowTracker = Shell.WindowTracker.get_default();
        let settings = new Gio.Settings({ schema_id: 'org.gnome.shell.app-switcher' });

        let workspace = null;
        if (settings.get_boolean('current-workspace-only')) {
            let workspaceManager = global.workspace_manager;

            workspace = workspaceManager.get_active_workspace();
        }

        let allWindows = getWindows(workspace);

        // Construct the AppIcons, add to the popup
        for (let i = 0; i < apps.length; i++) {
            let appIcon = new AppIcon(apps[i]);
            // Cache the window list now; we don't handle dynamic changes here,
            // and we don't want to be continually retrieving it
            appIcon.cachedWindows = allWindows.filter(
                w => windowTracker.get_window_app(w) === appIcon.app);
            if (appIcon.cachedWindows.length > 0)
                this._addIcon(appIcon);
        }

        this._altTabPopup = altTabPopup;
        this._delayedHighlighted = -1;
        this._mouseTimeOutId = 0;

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._mouseTimeOutId != 0)
            GLib.source_remove(this._mouseTimeOutId);

        this.icons.forEach(
            icon => icon.app.disconnectObject(this));
    }

    _setIconSize() {
        let j = 0;
        while (this._items.length > 1 && this._items[j].style_class != 'item-box')
            j++;

        let themeNode = this._items[j].get_theme_node();
        this._list.ensure_style();

        let iconPadding = themeNode.get_horizontal_padding();
        let iconBorder = themeNode.get_border_width(St.Side.LEFT) + themeNode.get_border_width(St.Side.RIGHT);
        let [, labelNaturalHeight] = this.icons[j].label.get_preferred_height(-1);
        let iconSpacing = labelNaturalHeight + iconPadding + iconBorder;
        let totalSpacing = this._list.spacing * (this._items.length - 1);

        // We just assume the whole screen here due to weirdness happening with the passed width
        let primary = Main.layoutManager.primaryMonitor;
        let parentPadding = this.get_parent().get_theme_node().get_horizontal_padding();
        let availWidth = primary.width - parentPadding - this.get_theme_node().get_horizontal_padding();

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let iconSizes = baseIconSizes.map(s => s * scaleFactor);
        let iconSize = baseIconSizes[0];

        if (this._items.length > 1) {
            for (let i =  0; i < baseIconSizes.length; i++) {
                iconSize = baseIconSizes[i];
                let height = iconSizes[i] + iconSpacing;
                let w = height * this._items.length + totalSpacing;
                if (w <= availWidth)
                    break;
            }
        }

        this._iconSize = iconSize;

        for (let i = 0; i < this.icons.length; i++) {
            if (this.icons[i].icon != null)
                break;
            this.icons[i].set_size(iconSize);
        }
    }

    vfunc_get_preferred_height(forWidth) {
        if (!this._iconSize)
            this._setIconSize();

        return super.vfunc_get_preferred_height(forWidth);
    }

    vfunc_allocate(box) {
        // Allocate the main list items
        super.vfunc_allocate(box);

        let contentBox = this.get_theme_node().get_content_box(box);

        let arrowHeight = Math.floor(this.get_theme_node().get_padding(St.Side.BOTTOM) / 3);
        let arrowWidth = arrowHeight * 2;

        // Now allocate each arrow underneath its item
        let childBox = new Clutter.ActorBox();
        for (let i = 0; i < this._items.length; i++) {
            let itemBox = this._items[i].allocation;
            childBox.x1 = contentBox.x1 + Math.floor(itemBox.x1 + (itemBox.x2 - itemBox.x1 - arrowWidth) / 2);
            childBox.x2 = childBox.x1 + arrowWidth;
            childBox.y1 = contentBox.y1 + itemBox.y2 + arrowHeight;
            childBox.y2 = childBox.y1 + arrowHeight;
            this._arrows[i].allocate(childBox);
        }
    }

    // We override SwitcherList's _onItemMotion method to delay
    // activation when the thumbnail list is open
    _onItemMotion(item) {
        if (item === this._items[this._highlighted] ||
            item === this._items[this._delayedHighlighted])
            return Clutter.EVENT_PROPAGATE;

        const index = this._items.indexOf(item);

        if (this._mouseTimeOutId !== 0) {
            GLib.source_remove(this._mouseTimeOutId);
            this._delayedHighlighted = -1;
            this._mouseTimeOutId = 0;
        }

        if (this._altTabPopup.thumbnailsVisible) {
            this._delayedHighlighted = index;
            this._mouseTimeOutId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                APP_ICON_HOVER_TIMEOUT,
                () => {
                    this._enterItem(index);
                    this._delayedHighlighted = -1;
                    this._mouseTimeOutId = 0;
                    return GLib.SOURCE_REMOVE;
                });
            GLib.Source.set_name_by_id(this._mouseTimeOutId, '[gnome-shell] this._enterItem');
        } else {
            this._itemEntered(index);
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _enterItem(index) {
        let [x, y] = global.get_pointer();
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.ALL, x, y);
        if (this._items[index].contains(pickedActor))
            this._itemEntered(index);
    }

    // We override SwitcherList's highlight() method to also deal with
    // the AppSwitcher->ThumbnailSwitcher arrows. Apps with only 1 window
    // will hide their arrows by default, but show them when their
    // thumbnails are visible (ie, when the app icon is supposed to be
    // in justOutline mode). Apps with multiple windows will normally
    // show a dim arrow, but show a bright arrow when they are
    // highlighted.
    highlight(n, justOutline) {
        if (this.icons[this._highlighted]) {
            if (this.icons[this._highlighted].cachedWindows.length === 1)
                this._arrows[this._highlighted].hide();
            else
                this._arrows[this._highlighted].remove_style_pseudo_class('highlighted');
        }

        super.highlight(n, justOutline);

        if (this._highlighted !== -1) {
            if (justOutline && this.icons[this._highlighted].cachedWindows.length === 1)
                this._arrows[this._highlighted].show();
            else
                this._arrows[this._highlighted].add_style_pseudo_class('highlighted');
        }
    }

    _addIcon(appIcon) {
        this.icons.push(appIcon);
        let item = this.addItem(appIcon, appIcon.label);

        appIcon.app.connectObject('notify::state', app => {
            if (app.state != Shell.AppState.RUNNING)
                this._removeIcon(app);
        }, this);

        let arrow = new St.DrawingArea({ style_class: 'switcher-arrow' });
        arrow.connect('repaint', () => SwitcherPopup.drawArrow(arrow, St.Side.BOTTOM));
        this.add_actor(arrow);
        this._arrows.push(arrow);

        if (appIcon.cachedWindows.length == 1)
            arrow.hide();
        else
            item.add_accessible_state(Atk.StateType.EXPANDABLE);
    }

    _removeIcon(app) {
        let index = this.icons.findIndex(icon => {
            return icon.app == app;
        });
        if (index === -1)
            return;

        this._arrows[index].destroy();
        this._arrows.splice(index, 1);

        this.icons.splice(index, 1);
        this.removeItem(index);
    }
});

var ThumbnailSwitcher = GObject.registerClass(
class ThumbnailSwitcher extends SwitcherPopup.SwitcherList {
    _init(windows) {
        super._init(false);

        this._labels = [];
        this._thumbnailBins = [];
        this._clones = [];
        this._windows = windows;

        for (let i = 0; i < windows.length; i++) {
            const box = new St.BoxLayout({
                style_class: 'thumbnail-box',
                vertical: true,
            });

            let bin = new St.Bin({ style_class: 'thumbnail' });

            box.add_actor(bin);
            this._thumbnailBins.push(bin);

            const title = windows[i].get_title();
            const name = new St.Label({
                text: title,
                // St.Label doesn't support text-align
                x_align: Clutter.ActorAlign.CENTER,
            });
            this._labels.push(name);
            box.add_actor(name);

            this.addItem(box, name);
        }

        this.connect('destroy', this._onDestroy.bind(this));
    }

    addClones(availHeight) {
        if (!this._thumbnailBins.length)
            return;
        let totalPadding = this._items[0].get_theme_node().get_horizontal_padding() + this._items[0].get_theme_node().get_vertical_padding();
        totalPadding += this.get_theme_node().get_horizontal_padding() + this.get_theme_node().get_vertical_padding();
        let [, labelNaturalHeight] = this._labels[0].get_preferred_height(-1);
        let spacing = this._items[0].child.get_theme_node().get_length('spacing');
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let thumbnailSize = THUMBNAIL_DEFAULT_SIZE * scaleFactor;

        availHeight = Math.min(availHeight - labelNaturalHeight - totalPadding - spacing, thumbnailSize);
        let binHeight = availHeight + this._items[0].get_theme_node().get_vertical_padding() + this.get_theme_node().get_vertical_padding() - spacing;
        binHeight = Math.min(thumbnailSize, binHeight);

        for (let i = 0; i < this._thumbnailBins.length; i++) {
            let mutterWindow = this._windows[i].get_compositor_private();
            if (!mutterWindow)
                continue;

            let clone = _createWindowClone(mutterWindow, thumbnailSize);
            this._thumbnailBins[i].set_height(binHeight);
            this._thumbnailBins[i].add_actor(clone);

            mutterWindow.connectObject('destroy',
                source => this._removeThumbnail(source, clone), this);
            this._clones.push(clone);
        }

        // Make sure we only do this once
        this._thumbnailBins = [];
    }

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
            this.destroy();
    }

    _onDestroy() {
        this._clones.forEach(
            clone => clone?.source.disconnectObject(this));
    }
});

var WindowIcon = GObject.registerClass(
class WindowIcon extends St.BoxLayout {
    _init(window, mode) {
        super._init({
            style_class: 'alt-tab-app',
            vertical: true,
        });

        this.window = window;

        this._icon = new St.Widget({ layout_manager: new Clutter.BinLayout() });

        this.add_child(this._icon);
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

            if (this.app) {
                this._icon.add_actor(this._createAppIcon(this.app,
                                                         APP_ICON_SIZE_SMALL));
            }
            break;

        case AppIconMode.APP_ICON_ONLY:
            size = APP_ICON_SIZE;
            this._icon.add_actor(this._createAppIcon(this.app, size));
        }

        this._icon.set_size(size * scaleFactor, size * scaleFactor);
    }

    _createAppIcon(app, size) {
        let appIcon = app
            ? app.create_icon_texture(size)
            : new St.Icon({ icon_name: 'icon-missing', icon_size: size });
        appIcon.x_expand = appIcon.y_expand = true;
        appIcon.x_align = appIcon.y_align = Clutter.ActorAlign.END;

        return appIcon;
    }
});

var WindowSwitcher = GObject.registerClass(
class WindowSwitcher extends SwitcherPopup.SwitcherList {
    _init(windows, mode) {
        super._init(true);

        this._label = new St.Label({
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_actor(this._label);

        this.windows = windows;
        this.icons = [];

        for (let i = 0; i < windows.length; i++) {
            let win = windows[i];
            let icon = new WindowIcon(win, mode);

            this.addItem(icon, icon.label);
            this.icons.push(icon);

            icon.window.connectObject('unmanaged',
                window => this._removeWindow(window), this);
        }

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        this.icons.forEach(
            icon => icon.window.disconnectObject(this));
    }

    vfunc_get_preferred_height(forWidth) {
        let [minHeight, natHeight] = super.vfunc_get_preferred_height(forWidth);

        let spacing = this.get_theme_node().get_padding(St.Side.BOTTOM);
        let [labelMin, labelNat] = this._label.get_preferred_height(-1);

        minHeight += labelMin + spacing;
        natHeight += labelNat + spacing;

        return [minHeight, natHeight];
    }

    vfunc_allocate(box) {
        let themeNode = this.get_theme_node();
        let contentBox = themeNode.get_content_box(box);
        const labelHeight = this._label.height;
        const totalLabelHeight =
            labelHeight + themeNode.get_padding(St.Side.BOTTOM);

        box.y2 -= totalLabelHeight;
        super.vfunc_allocate(box);

        // Hooking up the parent vfunc will call this.set_allocation() with
        // the height without the label height, so call it again with the
        // correct size here.
        box.y2 += totalLabelHeight;
        this.set_allocation(box);

        const childBox = new Clutter.ActorBox();
        childBox.x1 = contentBox.x1;
        childBox.x2 = contentBox.x2;
        childBox.y2 = contentBox.y2;
        childBox.y1 = childBox.y2 - labelHeight;
        this._label.allocate(childBox);
    }

    highlight(index, justOutline) {
        super.highlight(index, justOutline);

        this._label.set_text(index == -1 ? '' : this.icons[index].label.text);
    }

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
