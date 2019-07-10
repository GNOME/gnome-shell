// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported AppIconBar */

const { Clutter, Gdk, Gio, GLib, GObject,
    Gtk, Meta, Shell, St } = imports.gi;

const AppActivation = imports.ui.appActivation;
const AppFavorites = imports.ui.appFavorites;
const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ParentalControlsManager = imports.misc.parentalControlsManager;
const PopupMenu = imports.ui.popupMenu;

const MAX_OPACITY = 255;

const ICON_SIZE = 24;

const ICON_SCROLL_ANIMATION_TIME = 300;
const ICON_SCROLL_ANIMATION_TYPE = Clutter.AnimationMode.LINEAR;

const ICON_BOUNCE_MAX_SCALE = 0.4;
const ICON_BOUNCE_ANIMATION_TIME = 400;
const ICON_BOUNCE_ANIMATION_TYPE_1 = Clutter.AnimationMode.EASE_OUT_SINE;
const ICON_BOUNCE_ANIMATION_TYPE_2 = Clutter.AnimationMode.EASE_OUT_BOUNCE;

const PANEL_WINDOW_MENU_THUMBNAIL_SIZE = 128;

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';

function _compareByStableSequence(winA, winB) {
    let seqA = winA.get_stable_sequence();
    let seqB = winB.get_stable_sequence();

    return seqA - seqB;
}

const WindowMenuItem = GObject.registerClass(
class WindowMenuItem extends PopupMenu.PopupBaseMenuItem {
    _init(window, params) {
        super._init(params);

        this.window = window;

        this.add_style_class_name('panel-window-menu-item');

        let windowActor = this._findWindowActor();
        let monitor = Main.layoutManager.primaryMonitor;

        // constraint the max size of the clone to the aspect ratio
        // of the primary display, where the panel lives
        let ratio = monitor.width / monitor.height;
        let maxW = ratio > 1
            ? PANEL_WINDOW_MENU_THUMBNAIL_SIZE : PANEL_WINDOW_MENU_THUMBNAIL_SIZE * ratio;
        let maxH = ratio > 1
            ? PANEL_WINDOW_MENU_THUMBNAIL_SIZE / ratio : PANEL_WINDOW_MENU_THUMBNAIL_SIZE;

        let clone = new Clutter.Actor({
            content: windowActor.get_texture(),
            request_mode: Clutter.RequestMode.CONTENT_SIZE,
        });
        let cloneW = clone.width;
        let cloneH = clone.height;

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let scale = Math.min(maxW / cloneW, maxH / cloneH) * scaleFactor;

        clone.set_size(Math.round(cloneW * scale), Math.round(cloneH * scale));

        this.cloneBin = new St.Bin({
            child: clone,
            style_class: 'panel-window-menu-item-clone',
        });
        this.add(this.cloneBin, { align: St.Align.MIDDLE });

        this.label = new St.Label({
            text: window.title,
            style_class: 'panel-window-menu-item-label',
            y_align: Clutter.ActorAlign.CENTER,
            y_expand: true,
        });

        this.add_child(this.label);
        this.label_actor = this.label;
    }

    _findWindowActor() {
        let actors = global.get_window_actors();
        let windowActors = actors.filter(actor => {
            return actor.meta_window === this.window;
        });

        return windowActors[0];
    }
});

const ScrollMenuItem = GObject.registerClass(
class ScrollMenuItem extends PopupMenu.PopupSubMenuMenuItem {
    _init() {
        super._init('');

        // remove all the stock style classes
        this.remove_style_class_name('popup-submenu-menu-item');
        this.remove_style_class_name('popup-menu-item');

        // remove all the stock actors
        this.remove_all_children();
        this.menu.destroy();

        this.label = null;
        this._triangle = null;

        this.menu = new PopupMenu.PopupSubMenu(this, new St.Label({ text: '' }));
        this.menu.actor.remove_style_class_name('popup-sub-menu');
    }

    _onKeyPressEvent() {
        // no special handling
        return false;
    }

    activate() {
        // override to do nothing
    }

    _onButtonReleaseEvent() {
        // override to do nothing
    }
});

const APP_ICON_MENU_ARROW_XALIGN = 0.5;

const AppIconMenu = class extends PopupMenu.PopupMenu {
    constructor(app, parentActor) {
        super(parentActor, APP_ICON_MENU_ARROW_XALIGN, St.Side.BOTTOM);

        this.actor.add_style_class_name('app-icon-menu');

        this._submenuItem = new ScrollMenuItem();
        this.addMenuItem(this._submenuItem);
        this._submenuItem.menu.connect('activate', this._onActivate.bind(this));

        // We want to popdown the menu when clicked on the source icon itself
        this.shouldSwitchToOnHover = false;

        this._app = app;
    }

    _redisplay() {
        this._submenuItem.menu.removeAll();

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();

        let windows = this._app.get_windows();
        let workspaceWindows = [];
        let otherWindows = [];

        windows.forEach(w => {
            if (w.is_skip_taskbar() || Shell.WindowTracker.is_speedwagon_window(w))
                return;

            if (w.located_on_workspace(activeWorkspace))
                workspaceWindows.push(w);
            else
                otherWindows.push(w);
        });

        workspaceWindows.sort(_compareByStableSequence.bind(this));
        otherWindows.sort(_compareByStableSequence.bind(this));

        let hasWorkspaceWindows = workspaceWindows.length > 0;
        let hasOtherWindows = otherWindows.length > 0;

        // Display windows from other workspaces first, if present, since our panel
        // is at the bottom, and it's much more convenient to just move up the pointer
        // to switch windows in the current workspace
        if (hasOtherWindows)
            this._appendOtherWorkspacesLabel();

        otherWindows.forEach(w => {
            this._appendMenuItem(w, hasOtherWindows);
        });

        if (hasOtherWindows && hasWorkspaceWindows)
            this._appendCurrentWorkspaceSeparator();

        workspaceWindows.forEach(w => {
            this._appendMenuItem(w, hasOtherWindows);
        });
    }

    _appendOtherWorkspacesLabel() {
        let label = new PopupMenu.PopupMenuItem(_('Other workspaces'));
        label.label.add_style_class_name('panel-window-menu-workspace-label');
        this._submenuItem.menu.addMenuItem(label);
    }

    _appendCurrentWorkspaceSeparator() {
        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this._submenuItem.menu.addMenuItem(separator);

        let label = new PopupMenu.PopupMenuItem(_('Current workspace'));
        label.label.add_style_class_name('panel-window-menu-workspace-label');
        this._submenuItem.menu.addMenuItem(label);
    }

    _appendMenuItem(window, hasOtherWindows) {
        let item = new WindowMenuItem(window);
        this._submenuItem.menu.addMenuItem(item);

        if (hasOtherWindows)
            item.cloneBin.add_style_pseudo_class('indented');
    }

    toggle(animation) {
        if (this.isOpen) {
            this.close(animation);
        } else {
            this._redisplay();
            this.open(animation);
            this._submenuItem.menu.open(BoxPointer.PopupAnimation.NONE);
        }
    }

    _onActivate(actor, item) {
        Main.activateWindow(item.window);
        this.close();
    }
};

/** AppIconButton:
 *
 * This class handles the application icon
 */
const AppIconButton = GObject.registerClass({
    Signals: {
        'app-icon-pressed': {},
        'app-icon-pinned': {},
        'app-icon-unpinned': {},
    },
}, class AppIconButton extends St.Button {
    _init(app, iconSize, menuManager, allowsPinning) {
        this._app = app;

        this._iconSize = iconSize;
        let icon = this._createIcon();

        super._init({
            style_class: 'app-icon-button',
            child: icon,
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
        });

        this._isBouncing = false;
        this._menuManager = menuManager;

        this._label = new St.Label({
            text: this._app.get_name(),
            style_class: 'app-icon-hover-label',
        });
        this._label.connect('style-changed', this._updateStyle.bind(this));

        // Handle the menu-on-press case for multiple windows
        this.connect('button-press-event', this._handleButtonPressEvent.bind(this));
        this.connect('clicked', this._handleClickEvent.bind(this));

        Main.layoutManager.connect('startup-complete', this._updateIconGeometry.bind(this));
        this.connect('notify::allocation', this._updateIconGeometry.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));
        this.connect('enter-event', this._showHoverState.bind(this));
        this.connect('leave-event', this._hideHoverState.bind(this));

        this._rightClickMenuManager = new PopupMenu.PopupMenuManager(this);

        this._rightClickMenu = new PopupMenu.PopupMenu(this, 0.0, St.Side.BOTTOM, 0);
        this._rightClickMenu.blockSourceEvents = true;

        if (allowsPinning) {
            this._pinMenuItem = this._rightClickMenu.addAction(_('Pin to Taskbar'), () => {
                this.emit('app-icon-pinned');
            });

            this._unpinMenuItem = this._rightClickMenu.addAction(_('Unpin from Taskbar'), () => {
                // Unpin from taskbar in idle, so that we can avoid destroying
                // the menu actor before it's closed
                GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                    this._menu.destroy();

                    this.emit('app-icon-unpinned');

                    return GLib.SOURCE_REMOVE;
                });
            });

            if (AppFavorites.getAppFavorites().isFavorite(this._app.get_id()))
                this._pinMenuItem.actor.visible = false;
            else
                this._unpinMenuItem.actor.visible = false;

            this._rightClickMenu.connect('menu-closed', () => {
                let isPinned = AppFavorites.getAppFavorites().isFavorite(this._app.get_id());
                this._pinMenuItem.actor.visible = !isPinned;
                this._unpinMenuItem.actor.visible = isPinned;
            });
        }

        this._quitMenuItem = this._rightClickMenu.addAction(_('Quit %s').format(this._app.get_name()), () => {
            this._app.request_quit();
        });
        this._rightClickMenuManager.addMenu(this._rightClickMenu);
        this._rightClickMenu.actor.hide();
        Main.uiGroup.add_actor(this._rightClickMenu.actor);

        this._menu = new AppIconMenu(this._app, this);
        this._menuManager.addMenu(this._menu);
        this._menu.actor.hide();
        Main.uiGroup.add_actor(this._menu.actor);

        this._menu.connect('open-state-changed', () => {
            // Setting the max-height won't do any good if the minimum height of the
            // menu is higher then the screen; it's useful if part of the menu is
            // scrollable so the minimum height is smaller than the natural height
            let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
            this._menu.actor.style = 'max-height: %dpx;'.format(Math.round(workArea.height));
        });

        this._appStateUpdatedId = this._app.connect('notify::state', this._syncQuitMenuItemVisible.bind(this));
        this._syncQuitMenuItemVisible();
    }

    _syncQuitMenuItemVisible() {
        let visible = this._app.get_state() === Shell.AppState.RUNNING;
        this._quitMenuItem.actor.visible = visible;
    }

    _createIcon() {
        return this._app.create_icon_texture(this._iconSize);
    }

    _hasOtherMenuOpen() {
        let activeIconMenu = this._menuManager.activeMenu;
        return activeIconMenu &&
            activeIconMenu !== this._menu &&
            activeIconMenu.isOpen;
    }

    _closeOtherMenus(animation) {
        // close any other open menu
        if (this._hasOtherMenuOpen())
            this._menuManager.activeMenu.toggle(animation);
    }

    _getInterestingWindows() {
        let windows = this._app.get_windows();
        let hasSpeedwagon = false;
        windows = windows.filter(metaWindow => {
            hasSpeedwagon = hasSpeedwagon || Shell.WindowTracker.is_speedwagon_window(metaWindow);
            return !metaWindow.is_skip_taskbar();
        });
        return [windows, hasSpeedwagon];
    }

    _getNumRealWindows(windows, hasSpeedwagon) {
        return windows.length - (hasSpeedwagon ? 1 : 0);
    }

    _handleButtonPressEvent(actor, event) {
        let button = event.get_button();
        let clickCount = event.get_click_count();

        if (button === Gdk.BUTTON_PRIMARY &&
            clickCount === 1) {
            this._hideHoverState();
            this.emit('app-icon-pressed');

            let [windows, hasSpeedwagon] = this._getInterestingWindows();
            let numRealWindows = this._getNumRealWindows(windows, hasSpeedwagon);

            if (numRealWindows > 1) {
                let hasOtherMenu = this._hasOtherMenuOpen();
                let animation = BoxPointer.PopupAnimation.FULL;
                if (hasOtherMenu)
                    animation = BoxPointer.PopupAnimation.NONE;

                this._closeOtherMenus(animation);
                this._animateBounce();

                this.fake_release();
                this._menu.toggle(animation);
                this._menuManager.ignoreRelease();

                // This will block the clicked signal from being emitted
                return true;
            }
        }

        this.sync_hover();
        return false;
    }

    _handleClickEvent() {
        let event = Clutter.get_current_event();
        let button = event.get_button();

        if (button === Gdk.BUTTON_SECONDARY) {
            this._hideHoverState();

            this._closeOtherMenus(BoxPointer.PopupAnimation.FULL);
            if (this._menu.isOpen)
                this._menu.toggle(BoxPointer.PopupAnimation.FULL);

            this._rightClickMenu.open();
            return;
        }

        let hasOtherMenu = this._hasOtherMenuOpen();
        this._closeOtherMenus(BoxPointer.PopupAnimation.FULL);
        this._animateBounce();

        let [windows, hasSpeedwagon] = this._getInterestingWindows();
        let numRealWindows = this._getNumRealWindows(windows, hasSpeedwagon);

        // The multiple windows case is handled in button-press-event
        if (windows.length === 0) {
            let activationContext = new AppActivation.AppActivationContext(this._app);
            activationContext.activate();
        } else if (numRealWindows === 1 && !hasSpeedwagon) {
            let win = windows[0];
            if (win.has_focus() && !Main.overview.visible && !hasOtherMenu) {
                // The overview is not visible, and this is the
                // currently focused application; minimize it
                win.minimize();
            } else {
                // Activate window normally
                Main.activateWindow(win);
            }
        }
    }

    activateFirstWindow() {
        this._animateBounce();
        this._closeOtherMenus(BoxPointer.PopupAnimation.FULL);
        let windows = this._getInterestingWindows()[0];
        if (windows.length > 0) {
            Main.activateWindow(windows[0]);
        } else {
            let activationContext = new AppActivation.AppActivationContext(this._app);
            activationContext.activate();
        }
    }

    _hideHoverState() {
        this.fake_release();
        if (this._label.get_parent() !== null)
            Main.uiGroup.remove_actor(this._label);
    }

    _showHoverState() {
        // Show label only if it's not already visible
        this.fake_release();
        if (this._label.get_parent())
            return;

        Main.uiGroup.add_actor(this._label);
        Main.uiGroup.set_child_above_sibling(this._label, null);

        // Calculate location of the label only if we're not tweening as the
        // values will be inaccurate
        if (!this._isBouncing) {
            let iconMidpoint = this.get_transformed_position()[0] + this.width / 2;
            this._label.translation_x = Math.floor(iconMidpoint - this._label.width / 2);
            this._label.translation_y = Math.floor(this.get_transformed_position()[1] - this._labelOffsetY);

            // Clip left edge to be the left edge of the screen
            this._label.translation_x = Math.max(this._label.translation_x, 0);
        }
    }

    _animateBounce() {
        if (this._isBouncing)
            return;

        this._isBouncing = true;

        this.ease({
            scale_y: 1 - ICON_BOUNCE_MAX_SCALE,
            scale_x: 1 + ICON_BOUNCE_MAX_SCALE,
            translation_y: this.height * ICON_BOUNCE_MAX_SCALE,
            translation_x: -this.width * ICON_BOUNCE_MAX_SCALE / 2,
            duration: ICON_BOUNCE_ANIMATION_TIME * 0.25,
            mode: ICON_BOUNCE_ANIMATION_TYPE_1,
            onComplete: () => {
                this.ease({
                    scale_y: 1,
                    scale_x: 1,
                    translation_y: 0,
                    translation_x: 0,
                    duration: ICON_BOUNCE_ANIMATION_TIME * 0.75,
                    mode: ICON_BOUNCE_ANIMATION_TYPE_2,
                    onComplete: () => {
                        this._isBouncing = false;
                    },
                });
            },
        });
    }

    setIconSize(iconSize) {
        let icon = this._app.create_icon_texture(iconSize);
        this._iconSize = iconSize;

        this.set_child(icon);
    }

    _onDestroy() {
        this._label.destroy();
        this._resetIconGeometry();

        if (this._appStateUpdatedId > 0) {
            this._app.disconnect(this._appStateUpdatedId);
            this._appStateUpdatedId = 0;
        }
    }

    _setIconRectForAllWindows(rectangle) {
        let windows = this._app.get_windows();
        windows.forEach(win => win.set_icon_geometry(rectangle));
    }

    _resetIconGeometry() {
        this._setIconRectForAllWindows(null);
    }

    _updateIconGeometry() {
        if (!this.mapped)
            return;

        let rect = new Meta.Rectangle();
        [rect.x, rect.y] = this.get_transformed_position();
        [rect.width, rect.height] = this.get_transformed_size();

        this._setIconRectForAllWindows(rect);
    }

    _updateStyle() {
        this._labelOffsetY = this._label.get_theme_node().get_length('-label-offset-y');
    }

    isPinned() {
        return AppFavorites.getAppFavorites().isFavorite(this._app.get_id());
    }
});

/** AppIconBarNavButton:
 *
 * This class handles the nav buttons on the app bar
 */
const AppIconBarNavButton = GObject.registerClass(
class AppIconBarNavButton extends St.Button {
    _init(iconName) {
        this._icon = new St.Icon({
            style_class: 'app-bar-nav-icon',
            iconName,
        });

        super._init({
            style_class: 'app-bar-nav-button',
            child: this._icon,
            can_focus: true,
            reactive: true,
            track_hover: true,
            button_mask: St.ButtonMask.ONE,
        });
    }
});


const ScrolledIconList = GObject.registerClass({
    Signals: {
        'icons-scrolled': {},
        'app-icon-pressed': {},
    },
}, class ScrolledIconList extends St.ScrollView {
    _init(menuManager) {
        super._init({
            hscrollbar_policy: Gtk.PolicyType.NEVER,
            style_class: 'scrolled-icon-list hfade',
            vscrollbar_policy: Gtk.PolicyType.NEVER,
            x_fill: true,
            y_fill: true,
        });

        this._menuManager = menuManager;

        // Due to the interactions with StScrollView,
        // StBoxLayout clips its painting to the content box, effectively
        // clipping out the side paddings we want to set on the actual icons
        // container. We need to go through some hoops and set the padding
        // on an intermediate spacer child instead
        let scrollChild = new St.BoxLayout();
        this.add_actor(scrollChild);

        this._spacerBin = new St.Widget({
            style_class: 'scrolled-icon-spacer',
            layout_manager: new Clutter.BinLayout(),
        });
        scrollChild.add_actor(this._spacerBin);

        this._container = new St.BoxLayout({
            style_class: 'scrolled-icon-container',
            x_expand: true,
            y_expand: true,
        });
        this._spacerBin.add_actor(this._container);

        this._iconSize = ICON_SIZE;
        this._iconSpacing = 0;

        this._iconOffset = 0;
        this._appsPerPage = -1;

        this._container.connect('style-changed', this._updateStyleConstants.bind(this));

        let appSys = Shell.AppSystem.get_default();

        this._parentalControlsManager = ParentalControlsManager.getDefault();

        this._taskbarApps = new Map();

        // Update for any apps running before the system started
        // (after a crash or a restart)
        let currentlyRunning = appSys.get_running();
        let appsByPid = [];
        for (let i = 0; i < currentlyRunning.length; i++) {
            let app = currentlyRunning[i];
            // Most apps have a single PID; ignore all but the first
            let pid = app.get_pids()[0];
            appsByPid.push({
                pid,
                app,
            });
        }

        let favorites = AppFavorites.getAppFavorites().getFavorites();
        for (let favorite of favorites)
            this._addButtonAnimated(favorite);

        // Sort numerically by PID
        // This preserves the original app order, until the maximum PID
        // value is reached and older PID values are recycled
        let sortedPids = appsByPid.sort((a, b) => a.pid - b.pid);
        for (let sortedPid of sortedPids)
            this._addButtonAnimated(sortedPid.app);

        appSys.connect('app-state-changed', this._onAppStateChanged.bind(this));

        this._parentalControlsManager.connect('app-filter-changed', () => {
            for (let [app, appButton] of this._taskbarApps) {
                let shouldShow = this._parentalControlsManager.shouldShowApp(app.get_app_info());
                let stopped = app.state === Shell.AppState.STOPPED;

                appButton.visible = !stopped || shouldShow;
            }
        });
    }

    setActiveApp(app) {
        this._taskbarApps.forEach((appButton, taskbarApp) => {
            if (app === taskbarApp)
                appButton.add_style_pseudo_class('highlighted');
            else
                appButton.remove_style_pseudo_class('highlighted');

            appButton.queue_redraw();
        });
    }

    getNumAppButtons() {
        return this._taskbarApps.size;
    }

    getNumVisibleAppButtons() {
        let buttons = [...this._taskbarApps.values()];
        return buttons.reduce((counter, appButton) => {
            return appButton.visible ? counter : counter + 1;
        }, 0);
    }

    activateNthApp(index) {
        let buttons = [...this._taskbarApps.values()];
        let appButton = buttons[index];
        if (appButton)
            appButton.activateFirstWindow();
    }

    getMinContentWidth(forHeight) {
        // We always want to show one icon, plus we want to keep the padding
        // added by the spacer actor
        let [minSpacerWidth] = this._spacerBin.get_preferred_width(forHeight);
        let [minContainerWidth] = this._container.get_preferred_width(forHeight);
        return this._iconSize + (minSpacerWidth - minContainerWidth);
    }

    _updatePage() {
        // Clip the values of the iconOffset
        let lastIconOffset = this.getNumVisibleAppButtons() - 1;
        let movableIconsPerPage = this._appsPerPage - 1;
        let iconOffset = Math.max(0, this._iconOffset);
        iconOffset = Math.min(lastIconOffset - movableIconsPerPage, iconOffset);

        if (this._iconOffset === iconOffset)
            return;

        this._iconOffset = iconOffset;

        let relativeAnimationTime = ICON_SCROLL_ANIMATION_TIME;

        let iconFullWidth = this._iconSize + this._iconSpacing;
        let pageSize = this._appsPerPage * iconFullWidth;
        let hadjustment = this.hscroll.adjustment;

        let currentOffset = this.hscroll.adjustment.get_value();
        let targetOffset = Math.min(this._iconOffset * iconFullWidth, hadjustment.upper);

        let distanceToTravel = Math.abs(targetOffset - currentOffset);
        if (distanceToTravel < pageSize)
            relativeAnimationTime = relativeAnimationTime * distanceToTravel / pageSize;

        hadjustment.ease(targetOffset, {
            duration: relativeAnimationTime,
            mode: ICON_SCROLL_ANIMATION_TYPE,
        });
        this.emit('icons-scrolled');
    }

    pageBack() {
        this._iconOffset -= this._appsPerPage - 1;
        this._updatePage();
    }

    pageForward() {
        this._iconOffset += this._appsPerPage - 1;
        this._updatePage();
    }

    isBackAllowed() {
        return this._iconOffset > 0;
    }

    isForwardAllowed() {
        return this._iconOffset < this.getNumVisibleAppButtons() - this._appsPerPage;
    }

    calculateNaturalSize(forWidth) {
        let [numOfPages, appsPerPage] = this._calculateNumberOfPages(forWidth);

        if (this._appsPerPage !== appsPerPage ||
            this._numberOfPages !== numOfPages) {
            this._appsPerPage = appsPerPage;
            this._numberOfPages = numOfPages;

            this._updatePage();
        }

        let iconFullSize = this._iconSize + this._iconSpacing;
        return this._appsPerPage * iconFullSize - this._iconSpacing;
    }

    _updateStyleConstants() {
        let node = this._container.get_theme_node();

        this._iconSize = node.get_length('-icon-size');

        // The theme will give us an already-scaled size, but both ScrolledIconList and
        // the instances of AppIconButton expect the unscaled versions, since the underlying
        // machinery will scale things later on as needed. Thus, we need to unscale it.
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._iconSize /= scaleFactor;

        this._taskbarApps.forEach(appButton => {
            appButton.setIconSize(this._iconSize);
        });

        this._iconSpacing = node.get_length('spacing');
    }

    _ensureIsVisible(app) {
        let apps = [...this._taskbarApps.keys()];
        let itemIndex = apps.indexOf(app);
        if (itemIndex !== -1)
            this._iconOffset = itemIndex;

        this._updatePage();
    }

    _isAppInteresting(app) {
        if (AppFavorites.getAppFavorites().isFavorite(app.get_id()))
            return true;

        if (app.state === Shell.AppState.STARTING)
            return true;

        if (app.state === Shell.AppState.RUNNING) {
            let windows = app.get_windows();
            return windows.some(metaWindow => !metaWindow.is_skip_taskbar());
        }

        return false;
    }

    _getIconButtonForActor(actor) {
        for (let appIconButton of this._taskbarApps.values()) {
            if (appIconButton !== null && appIconButton === actor)
                return appIconButton;
        }
        return null;
    }

    _countPinnedAppsAheadOf(button) {
        let count = 0;
        let actors = this._container.get_children();
        for (let i = 0; i < actors.length; i++) {
            let otherButton = this._getIconButtonForActor(actors[i]);
            if (otherButton === button)
                return count;
            if (otherButton && otherButton.isPinned())
                count++;
        }
        return -1;
    }

    _addButtonAnimated(app) {
        if (this._taskbarApps.has(app) || !this._isAppInteresting(app))
            return;

        let favorites = AppFavorites.getAppFavorites();
        let newChild = new AppIconButton(app, this._iconSize, this._menuManager, true);
        let newActor = newChild;

        newChild.connect('app-icon-pressed', () => {
            this.emit('app-icon-pressed');
        });
        newChild.connect('app-icon-pinned', () => {
            favorites.addFavoriteAtPos(app.get_id(), this._countPinnedAppsAheadOf(newChild));
        });
        newChild.connect('app-icon-unpinned', () => {
            favorites.removeFavorite(app.get_id());
            if (app.state === Shell.AppState.STOPPED) {
                newActor.destroy();
                this._taskbarApps.delete(app);
                this._updatePage();
            }
        });
        this._taskbarApps.set(app, newChild);

        this._container.add_actor(newActor);

        if (app.state == Shell.AppState.STOPPED &&
            !this._parentalControlsManager.shouldShowApp(app.get_app_info())) {
            newActor.hide();
        }
    }

    _addButton(app) {
        this._addButtonAnimated(app);
    }

    _onAppStateChanged(appSys, app) {
        let state = app.state;
        switch (state) {
        case Shell.AppState.STARTING:
            if (!this._parentalControlsManager.shouldShowApp(app.get_app_info()))
                break;
            this._addButton(app);
            this._ensureIsVisible(app);
            break;

        case Shell.AppState.RUNNING:
            this._addButton(app);
            this._ensureIsVisible(app);
            break;

        case Shell.AppState.STOPPED: {
            const appButton = this._taskbarApps.get(app);
            if (!appButton)
                break;

            if (AppFavorites.getAppFavorites().isFavorite(app.get_id())) {
                if (!this._parentalControlsManager.shouldShowApp(app.get_app_info()))
                    appButton.hide();
                break;
            }

            this._container.remove_actor(appButton);
            this._taskbarApps.delete(app);

            break;
        }
        }

        this._updatePage();
    }

    _calculateNumberOfPages(forWidth) {
        let minimumIconWidth = this._iconSize + this._iconSpacing;

        // We need to add one icon space to net width here so that the division
        // takes into account the fact that the last icon does not use iconSpacing
        let iconsPerPage = Math.floor((forWidth + this._iconSpacing) / minimumIconWidth);
        iconsPerPage = Math.max(1, iconsPerPage);

        let pages = Math.ceil(this.getNumVisibleAppButtons() / iconsPerPage);
        return [pages, iconsPerPage];
    }
});

var AppIconBarContainer = GObject.registerClass(
class AppIconBarContainer extends St.Widget {
    _init(backButton, forwardButton, scrolledIconList) {
        super._init({ name: 'appIconBarContainer' });

        this._spacing = 0;

        this._backButton = backButton;
        this.add_child(backButton);

        this._forwardButton = forwardButton;
        this.add_child(forwardButton);

        this._scrolledIconList = scrolledIconList;
        this.add_child(scrolledIconList);
    }

    _updateNavButtonState() {
        let backButtonOpacity = MAX_OPACITY;
        if (!this._scrolledIconList.isBackAllowed())
            backButtonOpacity = 0;

        let forwardButtonOpacity = MAX_OPACITY;
        if (!this._scrolledIconList.isForwardAllowed())
            forwardButtonOpacity = 0;

        this._backButton.opacity = backButtonOpacity;
        this._forwardButton.opacity = forwardButtonOpacity;
    }

    vfunc_get_preferred_width(forHeight) {
        let [minBackWidth, natBackWidth] = this._backButton.get_preferred_width(forHeight);
        let [minForwardWidth, natForwardWidth] = this._forwardButton.get_preferred_width(forHeight);

        // The scrolled icon list actor is a scrolled view with
        // hscrollbar-policy=NONE, so it will take the same width requisition as
        // its child. While we can use the natural one to measure the content,
        // we need a special method to measure the minimum width
        let minContentWidth = this._scrolledIconList.getMinContentWidth(forHeight);
        let [, natContentWidth] = this._scrolledIconList.get_preferred_width(forHeight);

        let minSize = minBackWidth + minForwardWidth + 2 * this._spacing + minContentWidth;
        let naturalSize = natBackWidth + natForwardWidth + 2 * this._spacing + natContentWidth;

        return [minSize, naturalSize];
    }

    vfunc_get_preferred_height(forWidth) {
        let [minListHeight, natListHeight] = this._scrolledIconList.get_preferred_height(forWidth);
        let [minBackHeight, natBackHeight] = this._backButton.get_preferred_height(forWidth);
        let [minForwardHeight, natForwardHeight] = this._forwardButton.get_preferred_height(forWidth);

        let minButtonHeight = Math.max(minBackHeight, minForwardHeight);
        let natButtonHeight = Math.max(natBackHeight, natForwardHeight);

        let minSize = Math.max(minButtonHeight, minListHeight);
        let naturalSize = Math.max(natButtonHeight, natListHeight);

        return [minSize, naturalSize];
    }

    vfunc_style_changed() {
        this._spacing = this.get_theme_node().get_length('spacing');
    }

    vfunc_allocate(box, flags) {
        let allocWidth = box.x2 - box.x1;
        let allocHeight = box.y2 - box.y1;

        let minBackWidth = this._backButton.get_preferred_width(allocHeight)[0];
        let minForwardWidth = this._forwardButton.get_preferred_width(allocHeight)[0];
        let maxIconSpace = Math.max(allocWidth - minBackWidth - minForwardWidth - 2 * this._spacing, 0);

        let childBox = new Clutter.ActorBox();
        childBox.y1 = 0;
        childBox.y2 = allocHeight;

        if (this.get_text_direction() === Clutter.TextDirection.RTL) {
            childBox.x1 = allocWidth;
            childBox.x2 = allocWidth;

            if (this._scrolledIconList.isBackAllowed()) {
                childBox.x1 = childBox.x2 - minBackWidth;
                this._backButton.allocate(childBox, flags);

                childBox.x1 -= this._spacing;
            }

            childBox.x2 = childBox.x1;
            childBox.x1 = childBox.x2 - this._scrolledIconList.calculateNaturalSize(maxIconSpace) - 2 * this._spacing;
            this._scrolledIconList.allocate(childBox, flags);

            childBox.x2 = childBox.x1;
            childBox.x1 = childBox.x2 - minForwardWidth;
            this._forwardButton.allocate(childBox, flags);
        } else {
            childBox.x1 = 0;
            childBox.x2 = 0;

            if (this._scrolledIconList.isBackAllowed()) {
                childBox.x2 = childBox.x1 + minBackWidth;
                this._backButton.allocate(childBox, flags);

                childBox.x2 += this._spacing;
            }

            childBox.x1 = childBox.x2;
            childBox.x2 = childBox.x1 + this._scrolledIconList.calculateNaturalSize(maxIconSpace) + 2 * this._spacing;
            this._scrolledIconList.allocate(childBox, flags);

            childBox.x1 = childBox.x2;
            childBox.x2 = childBox.x1 + minForwardWidth;
            this._forwardButton.allocate(childBox, flags);
        }

        this._updateNavButtonState();
    }
});

/** AppIconBar:
 *
 * This class handles positioning all the application icons and listening
 * for app state change signals
 */
var AppIconBar = GObject.registerClass(
class AppIconBar extends PanelMenu.Button {
    _init(panel) {
        super._init(0.0, null, true);
        this.add_style_class_name('app-icon-bar');

        this._panel = panel;

        this._menuManager = new PopupMenu.PopupMenuManager(this);

        this._backButton = new AppIconBarNavButton('go-previous-symbolic');
        this._backButton.connect('clicked', this._previousPageSelected.bind(this));

        this._scrolledIconList = new ScrolledIconList(this._menuManager);

        this._forwardButton = new AppIconBarNavButton('go-next-symbolic');
        this._forwardButton.connect('clicked', this._nextPageSelected.bind(this));

        let bin = new St.Bin({ name: 'appIconBar' });
        this.add_actor(bin);

        this._container =
            new AppIconBarContainer(this._backButton, this._forwardButton, this._scrolledIconList);
        bin.set_child(this._container);

        this._scrolledIconList.connect('icons-scrolled', () => {
            this._container.queue_relayout();
        });
        this._scrolledIconList.connect('app-icon-pressed', this._onAppIconPressed.bind(this));

        this._windowTracker = Shell.WindowTracker.get_default();
        this._windowTracker.connect('notify::focus-app', this._updateActiveApp.bind(this));
        Main.overview.connect('showing', this._updateActiveApp.bind(this));
        Main.overview.connect('hidden', this._updateActiveApp.bind(this));


        let keybindingSettings = new Gio.Settings({ schema: SHELL_KEYBINDINGS_SCHEMA });
        for (let index = 0; index < 8; index++) {
            let fullName = 'activate-icon-%d'.format(index + 1);
            Main.wm.addKeybinding(
                fullName,
                keybindingSettings,
                Meta.KeyBindingFlags.NONE,
                Shell.ActionMode.NORMAL |
                Shell.ActionMode.OVERVIEW,
                this._activateNthApp.bind(this, index));
        }
        Main.wm.addKeybinding(
            'activate-last-icon',
            keybindingSettings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.NORMAL |
            Shell.ActionMode.OVERVIEW,
            this._activateLastApp.bind(this));

        this._updateActiveApp();
    }

    _onAppIconPressed() {
        this._closeActivePanelMenu();
    }

    _closeActivePanelMenu() {
        let activeMenu = this._panel.menuManager.activeMenu;
        if (activeMenu)
            activeMenu.close(BoxPointer.PopupAnimation.FADE);
    }

    _activateNthApp(index) {
        this._scrolledIconList.activateNthApp(index);
    }

    _activateLastApp() {
        // Activate the index of the last button in the scrolled list
        this._activateNthApp(this._scrolledIconList.getNumAppButtons() - 1);
    }

    _updateActiveApp() {
        if (Main.overview.visible) {
            this._setActiveApp(null);
            return;
        }

        let focusApp = this._windowTracker.focus_app;
        this._setActiveApp(focusApp);
    }

    _setActiveApp(app) {
        this._scrolledIconList.setActiveApp(app);
    }

    _previousPageSelected() {
        this._scrolledIconList.pageBack();
    }

    _nextPageSelected() {
        this._scrolledIconList.pageForward();
    }
});
