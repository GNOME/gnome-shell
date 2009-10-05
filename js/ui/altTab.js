/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const AppIcon = imports.ui.appIcon;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

const POPUP_APPICON_BORDER_COLOR = new Clutter.Color();
POPUP_APPICON_BORDER_COLOR.from_pixel(0xffffffff);
const POPUP_APPICON_SEPARATOR_COLOR = new Clutter.Color();
POPUP_APPICON_SEPARATOR_COLOR.from_pixel(0x80808066);

const POPUP_APPS_BOX_SPACING = 8;

const POPUP_POINTER_SELECTION_THRESHOLD = 3;

function AltTabPopup() {
    this._init();
}

AltTabPopup.prototype = {
    _init : function() {
        this.actor = new St.Bin({ name: 'appSwitcher',
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
        this.actor.add_actor(gcenterbox);

        this._icons = [];
        this._separator = null;
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

    _addSeparator: function () {
        let box = new Big.Box({ padding_top: 2, padding_bottom: 2 });
        box.append(new Clutter.Rectangle({ width: 1,
                                           color: POPUP_APPICON_SEPARATOR_COLOR }),
                   Big.BoxPackFlags.EXPAND);
        this._separator = box;
        this._appsBox.add_actor(box);
    },

    show : function(backward) {
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
        let activeWorkspace = global.screen.get_active_workspace();
        let workspaceIcons = [];
        let otherIcons = [];
        for (let i = 0; i < apps.length; i++) {
            let appIcon = new AppIcon.AppIcon(apps[i], AppIcon.MenuType.BELOW, false);
            if (this._hasWindowsOnWorkspace(appIcon, activeWorkspace))
              workspaceIcons.push(appIcon);
            else
              otherIcons.push(appIcon);
        }

        workspaceIcons.sort(Lang.bind(this, this._sortAppIcon));
        otherIcons.sort(Lang.bind(this, this._sortAppIcon));

        for (let i = 0; i < workspaceIcons.length; i++)
            this._addIcon(workspaceIcons[i]);
        if (workspaceIcons.length > 0 && otherIcons.length > 0)
            this._addSeparator();
        for (let i = 0; i < otherIcons.length; i++)
            this._addIcon(otherIcons[i]);

        // Need to specify explicit width and height because the
        // window_group may not actually cover the whole screen
        this._lightbox = new Lightbox.Lightbox(global.window_group,
                                               global.screen_width,
                                               global.screen_height);

        this.actor.show_all();

        let primary = global.get_primary_monitor();
        this.actor.x = primary.x + Math.floor((primary.width - this.actor.width) / 2);
        this.actor.y = primary.y + Math.floor((primary.height - this.actor.height) / 2);

        if (!backward && this._icons[this._selected].windows.length > 1) {
            let candidateWindow = this._icons[this._selected].windows[1];
            if (candidateWindow.get_workspace() == activeWorkspace) {
                this._currentWindows[this._selected] = candidateWindow;
                this._updateSelection(0);
            }
            else {
                this._updateSelection(1);
            }
        }
        else {
            this._updateSelection(backward ? -1 : 1);
        }

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
            if (children[i] != this._separator) {
                let [childMin, childNat] = children[i].get_preferred_width(forHeight);
                maxChildMin = Math.max(childMin, maxChildMin);
                maxChildNat = Math.max(childNat, maxChildNat);
            }
        }

        let separatorWidth = 0;
        if (this._separator)
            separatorWidth = this._separator.get_preferred_width(forHeight)[0];

        let totalSpacing = this._appsBox.spacing * (children.length - 1);
        alloc.min_size = this._icons.length * maxChildMin + separatorWidth + totalSpacing;
        alloc.nat_size = this._icons.length * maxChildNat + separatorWidth + totalSpacing;
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

        let separatorWidth = 0;
        if (this._separator)
            separatorWidth = this._separator.get_preferred_width(childHeight)[0];

        let childWidth = Math.max(0, box.x2 - box.x1 - totalSpacing) / this._icons.length;

        let x = box.x1;
        for (let i = 0; i < children.length; i++) {
            if (children[i] != this._separator) {
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
            else {
                // We want the separator to be more compact than the rest.
                let childBox = new Clutter.ActorBox();
                childBox.x1 = x;
                childBox.y1 = 0;
                childBox.x2 = x + separatorWidth;
                childBox.y2 = childHeight;
                children[i].allocate(childBox, flags);
                x += this._appsBox.spacing + separatorWidth;
            }
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
