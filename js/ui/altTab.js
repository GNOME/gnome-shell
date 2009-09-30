/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;

const AppIcon = imports.ui.appIcon;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

const POPUP_BG_COLOR = new Clutter.Color();
POPUP_BG_COLOR.from_pixel(0x00000080);
const POPUP_APPICON_BORDER_COLOR = new Clutter.Color();
POPUP_APPICON_BORDER_COLOR.from_pixel(0xffffffff);

const POPUP_APPS_BOX_SPACING = 8;
const POPUP_ICON_SIZE = 48;

const POPUP_POINTER_SELECTION_THRESHOLD = 3;

function AltTabPopup() {
    this._init();
}

AltTabPopup.prototype = {
    _init : function() {
        this.actor = new Big.Box({ background_color : POPUP_BG_COLOR,
                                   corner_radius: POPUP_APPS_BOX_SPACING,
                                   padding: POPUP_APPS_BOX_SPACING,
                                   spacing: POPUP_APPS_BOX_SPACING,
                                   orientation: Big.BoxOrientation.VERTICAL,
                                   reactive: true });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        // Here we use a GenericContainer instead of a BigBox in order to be
        // able to handle allocation. In particular, we want all the alt+tab
        // popup items to be the same size.
        this._appsBox = new Shell.GenericContainer();
        this._appsBox.spacing = POPUP_APPS_BOX_SPACING;

        this._appsBox.connect('get-preferred-width', Lang.bind(this, this._appsBoxGetPreferredWidth));
        this._appsBox.connect('get-preferred-height', Lang.bind(this, this._appsBoxGetPreferredHeight));
        this._appsBox.connect('allocate', Lang.bind(this, this._appsBoxAllocate));

        let gcenterbox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                       x_align: Big.BoxAlignment.CENTER });
        gcenterbox.append(this._appsBox, Big.BoxPackFlags.NONE);
        this.actor.append(gcenterbox, Big.BoxPackFlags.NONE);

        this._icons = [];
        this._currentWindows = [];
        this._haveModal = false;
        this._selected = 0;
        this._highlightedWindow = null;
        this._toplevels = global.window_group.get_children();

        global.stage.add_actor(this.actor);
    },

    _addIcon : function(appIcon) {
        appIcon.connect('activate', Lang.bind(this, this._appClicked));
        appIcon.connect('activate-window', Lang.bind(this, this._windowClicked));
        appIcon.connect('highlight-window', Lang.bind(this, this._windowHovered));
        appIcon.connect('menu-popped-up', Lang.bind(this, this._menuPoppedUp));
        appIcon.connect('menu-popped-down', Lang.bind(this, this._menuPoppedDown));

        appIcon.actor.connect('enter-event', Lang.bind(this, this._iconEntered));

        // FIXME?
        appIcon.actor.border = 2;
        appIcon.highlight_border_color = POPUP_APPICON_BORDER_COLOR;

        this._icons.push(appIcon);
        this._currentWindows.push(appIcon.windows[0]);

        this._appsBox.add_actor(appIcon.actor);
    },

    show : function(initialSelection) {
        let appMonitor = Shell.AppMonitor.get_default();
        let apps = appMonitor.get_running_apps ("");

        if (!apps.length)
            return false;

        if (!Main.pushModal(this.actor))
            return false;
        this._haveModal = true;

        this._keyPressEventId = global.stage.connect('key-press-event', Lang.bind(this, this._keyPressEvent));
        this._keyReleaseEventId = global.stage.connect('key-release-event', Lang.bind(this, this._keyReleaseEvent));

        this._motionEventId = this.actor.connect('motion-event', Lang.bind(this, this._mouseMoved));
        this._mouseActive = false;
        this._mouseMovement = 0;

        // Contruct the AppIcons, sort by time, add to the popup
        let icons = [];
        for (let i = 0; i < apps.length; i++)
            icons.push(new AppIcon.AppIcon(apps[i], AppIcon.MenuType.BELOW, false));
        icons.sort(Lang.bind(this, this._sortAppIcon));
        for (let i = 0; i < icons.length; i++)
            this._addIcon(icons[i]);

        // Need to specify explicit width and height because the
        // window_group may not actually cover the whole screen
        this._lightbox = new Lightbox.Lightbox(global.window_group,
                                               global.screen_width,
                                               global.screen_height);

        this.actor.show_all();

        let primary = global.get_primary_monitor();
        this.actor.x = primary.x + Math.floor((primary.width - this.actor.width) / 2);
        this.actor.y = primary.y + Math.floor((primary.height - this.actor.height) / 2);

        this._updateSelection(initialSelection);

        // There's a race condition; if the user released Alt before
        // we got the grab, then we won't be notified. (See
        // https://bugzilla.gnome.org/show_bug.cgi?id=596695 for
        // details.) So we check now. (Have to do this after calling
        // _updateSelection.)
        let [screen, x, y, mods] = Gdk.Display.get_default().get_pointer();
        if (!(mods & Gdk.ModifierType.MOD1_MASK)) {
            this._finish();
            return false;
        }

        return true;
    },

    _hasWindowsOnWorkspace: function(appIcon, workspace) {
        for (let i = 0; i < appIcon.windows.length; i++) {
            if (appIcon.windows[i].get_workspace() == workspace)
                return true;
        }
        return false;
    },

    _hasVisibleWindows : function(appIcon) {
        for (let i = 0; i < appIcon.windows.length; i++) {
            if (appIcon.windows[i].showing_on_its_workspace())
                return true;
        }
        return false;
    },

    _sortAppIcon : function(appIcon1, appIcon2) {
        // We intend to sort the application list so applications appear
        // with this order:
        //   1. Apps with visible windows on the active workspace;
        //   2. Apps with minimized windows on the active workspace;
        //   3. Apps with visible windows on any other workspace;
        //   4. Other apps.

        let workspace = global.screen.get_active_workspace();
        let ws1 = this._hasWindowsOnWorkspace(appIcon1, workspace);
        let ws2 = this._hasWindowsOnWorkspace(appIcon2, workspace);

        if (ws1 && !ws2)
            return -1;
        else if (ws2 && !ws1)
            return 1;

        let vis1 = this._hasVisibleWindows(appIcon1);
        let vis2 = this._hasVisibleWindows(appIcon2);

        if (vis1 && !vis2) {
            return -1;
        } else if (vis2 && !vis1) {
            return 1;
        } else {
            // The app's most-recently-used window is first
            // in its list
            return (appIcon2.windows[0].get_user_time() -
                    appIcon1.windows[0].get_user_time());
        }
    },

    _appsBoxGetPreferredWidth: function (actor, forHeight, alloc) {
        let children = this._appsBox.get_children();
        let maxChildMin = 0;
        let maxChildNat = 0;

        for (let i = 0; i < children.length; i++) {
            let [childMin, childNat] = children[i].get_preferred_width(forHeight);
            maxChildMin = Math.max(childMin, maxChildMin);
            maxChildNat = Math.max(childNat, maxChildNat);
        }

        let totalSpacing = this._appsBox.spacing * (children.length - 1);
        alloc.min_size = children.length * maxChildMin + totalSpacing;
        alloc.nat_size = children.length * maxChildNat + totalSpacing;
    },

    _appsBoxGetPreferredHeight: function (actor, forWidth, alloc) {
        let children = this._appsBox.get_children();
        let maxChildMin = 0;
        let maxChildNat = 0;

        for (let i = 0; i < children.length; i++) {
            let [childMin, childNat] = children[i].get_preferred_height(forWidth);
            maxChildMin = Math.max(childMin, maxChildMin);
            maxChildNat = Math.max(childNat, maxChildNat);
        }

        alloc.min_size = maxChildMin;
        alloc.nat_size = maxChildNat;
    },

    _appsBoxAllocate: function (actor, box, flags) {
        let children = this._appsBox.get_children();
        let totalSpacing = this._appsBox.spacing * (children.length - 1);
        let childHeight = box.y2 - box.y1;
        let childWidth = Math.max(0, box.x2 - box.x1 - totalSpacing) / children.length;

        let x = box.x1;
        for (let i = 0; i < children.length; i++) {
            let [childMin, childNat] = children[i].get_preferred_height(childWidth);
            let vSpacing = (childHeight - childNat) / 2;
            let childBox = new Clutter.ActorBox();
            childBox.x1 = x;
            childBox.y1 = vSpacing;
            childBox.x2 = x + childWidth;
            childBox.y2 = childHeight - vSpacing;
            children[i].allocate(childBox, flags);
            x += this._appsBox.spacing + childWidth;
        }
    },

    _keyPressEvent : function(actor, event) {
        let keysym = event.get_key_symbol();
        let backwards = (event.get_state() & Clutter.ModifierType.SHIFT_MASK);

        if (keysym == Clutter.Tab)
            this._updateSelection(backwards ? -1 : 1);
        else if (keysym == Clutter.Left)
            this._updateSelection(-1);
        else if (keysym == Clutter.Right)
            this._updateSelection(1);
        else if (keysym == Clutter.grave)
            this._updateWindowSelection(backwards ? -1 : 1);
        else if (keysym == Clutter.Up)
            this._updateWindowSelection(-1);
        else if (keysym == Clutter.Down)
            this._updateWindowSelection(1);
        else if (keysym == Clutter.Escape)
            this.destroy();

        return true;
    },

    _keyReleaseEvent : function(actor, event) {
        let keysym = event.get_key_symbol();

        if (keysym == Clutter.Alt_L || keysym == Clutter.Alt_R)
            this._finish();

        return true;
    },

    _appClicked : function(icon) {
        Main.activateWindow(icon.windows[0]);
        this.destroy();
    },

    _windowClicked : function(icon, window) {
        if (window)
            Main.activateWindow(window);
        this.destroy();
    },

    _windowHovered : function(icon, window) {
        if (window)
            this._highlightWindow(window);
    },

    _mouseMoved : function(actor, event) {
        if (++this._mouseMovement < POPUP_POINTER_SELECTION_THRESHOLD)
            return;

        this.actor.disconnect(this._motionEventId);
        this._mouseActive = true;

        actor = event.get_source();
        while (actor) {
            if (actor._delegate instanceof AppIcon.AppIcon) {
                this._iconEntered(actor, event);
                return;
            }
            actor = actor.get_parent();
        }
    },

    _iconEntered : function(actor, event) {
        let index = this._icons.indexOf(actor._delegate);
        if (this._mouseActive)
            this._updateSelection(index - this._selected);
    },

    _finish : function() {
        if (this._highlightedWindow)
            Main.activateWindow(this._highlightedWindow);
        this.destroy();
    },

    destroy : function() {
        this.actor.destroy();
    },

    _onDestroy : function() {
        if (this._haveModal)
            Main.popModal(this.actor);

        if (this._lightbox)
            this._lightbox.destroy();

        if (this._keyPressEventId)
            global.stage.disconnect(this._keyPressEventId);
        if (this._keyReleaseEventId)
            global.stage.disconnect(this._keyReleaseEventId);
    },

    _updateSelection : function(delta) {
        this._icons[this._selected].setHighlight(false);
        if (delta != 0 && this._selectedMenu)
                this._selectedMenu.popdown();

        this._selected = (this._selected + this._icons.length + delta) % this._icons.length;
        this._icons[this._selected].setHighlight(true);

        this._highlightWindow(this._currentWindows[this._selected]);
    },

    _menuPoppedUp : function(icon, menu) {
        this._selectedMenu = menu;
    },

    _menuPoppedDown : function(icon, menu) {
        this._selectedMenu = null;
    },

    _updateWindowSelection : function(delta) {
        let icon = this._icons[this._selected];

        if (!this._selectedMenu)
            icon.popupMenu();
        if (!this._selectedMenu)
            return;

        let next = 0;
        for (let i = 0; i < icon.windows.length; i++) {
            if (icon.windows[i] == this._highlightedWindow) {
                next = (i + icon.windows.length + delta) % icon.windows.length;
                break;
            }
        }
        this._selectedMenu.selectWindow(icon.windows[next]);
    },

    _highlightWindow : function(metaWin) {
        this._highlightedWindow = metaWin;
        this._currentWindows[this._selected] = metaWin;
        this._lightbox.highlight(this._highlightedWindow.get_compositor_private());
    }
};
