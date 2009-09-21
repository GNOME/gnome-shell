/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;

const AppIcon = imports.ui.appIcon;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

const POPUP_BG_COLOR = new Clutter.Color();
POPUP_BG_COLOR.from_pixel(0x000000ff);
const POPUP_TRANSPARENT = new Clutter.Color();
POPUP_TRANSPARENT.from_pixel(0x00000000);

const POPUP_GRID_SPACING = 8;
const POPUP_ICON_SIZE = 48;
const POPUP_NUM_COLUMNS = 5;

const POPUP_POINTER_SELECTION_THRESHOLD = 3;

function AltTabPopup() {
    this._init();
}

AltTabPopup.prototype = {
    _init : function() {
        this.actor = new Big.Box({ background_color : POPUP_BG_COLOR,
                                   corner_radius: POPUP_GRID_SPACING,
                                   padding: POPUP_GRID_SPACING,
                                   spacing: POPUP_GRID_SPACING,
                                   orientation: Big.BoxOrientation.VERTICAL,
                                   reactive: true });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        // Icon grid.  TODO: Investigate Nbtk.Grid once that lands.  Currently
        // just implemented using a chain of Big.Box.
        this._grid = new Big.Box({ spacing: POPUP_GRID_SPACING,
                                   orientation: Big.BoxOrientation.VERTICAL });
        let gcenterbox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                       x_align: Big.BoxAlignment.CENTER });
        gcenterbox.append(this._grid, Big.BoxPackFlags.NONE);
        this.actor.append(gcenterbox, Big.BoxPackFlags.NONE);

        this._icons = [];
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

        this._icons.push(appIcon);

        // Add it to the grid
        if (!this._gridRow || this._gridRow.get_children().length == POPUP_NUM_COLUMNS) {
            this._gridRow = new Big.Box({ spacing: POPUP_GRID_SPACING,
                                          orientation: Big.BoxOrientation.HORIZONTAL });
            this._grid.append(this._gridRow, Big.BoxPackFlags.NONE);
        }
        this._gridRow.append(appIcon.actor, Big.BoxPackFlags.NONE);
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
            icons.push(new AppIcon.AppIcon(apps[i], AppIcon.MenuType.BELOW));
        icons.sort(function(i1, i2) {
                       // The app's most-recently-used window is first
                       // in its list
                       return (i2.windows[0].get_user_time() -
                               i1.windows[0].get_user_time());
                   });
        for (let i = 0; i < icons.length; i++)
            this._addIcon(icons[i]);

        // Need to specify explicit width and height because the
        // window_group may not actually cover the whole screen
        this._lightbox = new Lightbox.Lightbox(global.window_group,
                                               global.screen_width,
                                               global.screen_height);

        this.actor.show_all();
        this.actor.x = Math.floor((global.screen_width - this.actor.width) / 2);
        this.actor.y = Math.floor((global.screen_height - this.actor.height) / 2);

        this._updateSelection(initialSelection);
        return true;
    },

    _keyPressEvent : function(actor, event) {
        let keysym = event.get_key_symbol();
        let backwards = (event.get_state() & Clutter.ModifierType.SHIFT_MASK);

        if (keysym == Clutter.Tab)
            this._updateSelection(backwards ? -1 : 1);
        else if (keysym == Clutter.grave)
            this._updateWindowSelection(backwards ? -1 : 1);
        else if (keysym == Clutter.Escape)
            this.destroy();

        return true;
    },

    _keyReleaseEvent : function(actor, event) {
        let keysym = event.get_key_symbol();

        if (keysym == Clutter.Alt_L || keysym == Clutter.Alt_R) {
            if (this._highlightedWindow)
                Main.activateWindow(this._highlightedWindow, event.get_time());
            this.destroy();
        }

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

        this._highlightWindow(this._icons[this._selected].windows[0]);
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
        this._lightbox.highlight(this._highlightedWindow.get_compositor_private());
    }
};
