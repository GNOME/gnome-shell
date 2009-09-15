/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;

const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const POPUP_BG_COLOR = new Clutter.Color();
POPUP_BG_COLOR.from_pixel(0x00000080);
const POPUP_INDICATOR_COLOR = new Clutter.Color();
POPUP_INDICATOR_COLOR.from_pixel(0xf0f0f0ff);
const POPUP_TRANSPARENT = new Clutter.Color();
POPUP_TRANSPARENT.from_pixel(0x00000000);

const POPUP_INDICATOR_WIDTH = 4;
const POPUP_GRID_SPACING = 8;
const POPUP_ICON_SIZE = 48;
const POPUP_NUM_COLUMNS = 5;

const POPUP_LABEL_MAX_WIDTH = POPUP_NUM_COLUMNS * (POPUP_ICON_SIZE + POPUP_GRID_SPACING);

const SWITCH_TIME = 0.1;

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

        // Selected-window label
        this._label = new Clutter.Text({ font_name: "Sans 16px",
                                         ellipsize: Pango.EllipsizeMode.END });

        let labelbox = new Big.Box({ background_color: POPUP_INDICATOR_COLOR,
                                     corner_radius: POPUP_GRID_SPACING / 2,
                                     padding: POPUP_GRID_SPACING / 2 });
        labelbox.append(this._label, Big.BoxPackFlags.NONE);
        let lcenterbox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                       x_align: Big.BoxAlignment.CENTER,
                                       width: POPUP_LABEL_MAX_WIDTH + POPUP_GRID_SPACING });
        lcenterbox.append(labelbox, Big.BoxPackFlags.NONE);
        this.actor.append(lcenterbox, Big.BoxPackFlags.NONE);

        // Indicator around selected icon
        this._indicator = new Big.Rectangle({ border_width: POPUP_INDICATOR_WIDTH,
                                              corner_radius: POPUP_INDICATOR_WIDTH / 2,
                                              border_color: POPUP_INDICATOR_COLOR,
                                              color: POPUP_TRANSPARENT });
        this.actor.append(this._indicator, Big.BoxPackFlags.FIXED);

        this._items = [];
        this._haveModal = false;

        global.stage.add_actor(this.actor);
    },

    _addWindow : function(win) {
        let item = { window: win,
                     metaWindow: win.get_meta_window() };

        let pixbuf = item.metaWindow.icon;
        item.icon = new Clutter.Texture({ width: POPUP_ICON_SIZE,
                                          height: POPUP_ICON_SIZE,
                                          keep_aspect_ratio: true });
        Shell.clutter_texture_set_from_pixbuf(item.icon, pixbuf);

        item.box = new Big.Box({ padding: POPUP_INDICATOR_WIDTH * 2 });
        item.box.append(item.icon, Big.BoxPackFlags.NONE);

        item.n = this._items.length;
        this._items.push(item);

        // Add it to the grid
        if (!this._gridRow || this._gridRow.get_children().length == POPUP_NUM_COLUMNS) {
            this._gridRow = new Big.Box({ spacing: POPUP_GRID_SPACING,
                                          orientation: Big.BoxOrientation.HORIZONTAL });
            this._grid.append(this._gridRow, Big.BoxPackFlags.NONE);
        }
        this._gridRow.append(item.box, Big.BoxPackFlags.NONE);
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

        // Fill in the windows
        let windows = [];
        for (let i = 0; i < apps.length; i++) {
            let appWindows = appMonitor.get_windows_for_app(apps[i].get_id());
            windows = windows.concat(appWindows);
        }

        windows.sort(function(w1, w2) { return w2.get_user_time() - w1.get_user_time(); });

        for (let i = 0; i < windows.length; i++)
            this._addWindow(windows[i].get_compositor_private());

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
        else if (keysym == Clutter.Escape)
            this.destroy();

        return true;
    },

    _keyReleaseEvent : function(actor, event) {
        let keysym = event.get_key_symbol();

        if (keysym == Clutter.Alt_L || keysym == Clutter.Alt_R) {
            if (this._selected) {
                Main.activateWindow(this._selected.metaWindow,
                                    event.get_time());
            }
            this.destroy();
        }

        return true;
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
        let n = ((this._selected ? this._selected.n : 0) + this._items.length + delta) % this._items.length;

        if (this._selected) {
            // Unselect previous

            if (this._allocationChangedId) {
                this._selected.box.disconnect(this._allocationChangedId);
                delete this._allocationChangedId;
            }
        }

        let item = this._items[n];
        let changed = this._selected && item != this._selected;
        this._selected = item;

        if (this._selected) {
            this._label.set_size(-1, -1);
            this._label.text = this._selected.metaWindow.title;
            if (this._label.width > POPUP_LABEL_MAX_WIDTH)
                this._label.width = POPUP_LABEL_MAX_WIDTH;

            // Figure out this._selected.box's coordinates in terms of
            // this.actor
            let bx = this._selected.box.x, by = this._selected.box.y;
            let actor = this._selected.box.get_parent();
            while (actor != this.actor) {
                bx += actor.x;
                by += actor.y;
                actor = actor.get_parent();
            }

            if (changed) {
                Tweener.addTween(this._indicator,
                                 { x: bx,
                                   y: by,
                                   width: this._selected.box.width,
                                   height: this._selected.box.height,
                                   time: SWITCH_TIME,
                                   transition: "easeOutQuad" });
            } else {
                Tweener.removeTweens(this.indicator);
                this._indicator.set_position(bx, by);
                this._indicator.set_size(this._selected.box.width,
                                         this._selected.box.height);
            }
            this._indicator.show();

            this._lightbox.highlight(this._selected.window);

            this._allocationChangedId =
                this._selected.box.connect('notify::allocation',
                                           Lang.bind(this, this._allocationChanged));
        } else {
            this._label.text = "";
            this._indicator.hide();
            this._lightbox.highlight(null);
        }
    },

    _allocationChanged : function() {
        this._updateSelection(0);
    }
};
