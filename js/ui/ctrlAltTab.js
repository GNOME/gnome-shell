// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const SwitcherPopup = imports.ui.switcherPopup;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const POPUP_APPICON_SIZE = 96;
const POPUP_FADE_TIME = 0.1; // seconds

const SortGroup = {
    TOP:    0,
    MIDDLE: 1,
    BOTTOM: 2
};

const CtrlAltTabManager = new Lang.Class({
    Name: 'CtrlAltTabManager',

    _init: function() {
        this._items = [];
        this.addGroup(global.window_group, _("Windows"),
                      'emblem-documents-symbolic', { sortGroup: SortGroup.TOP,
                                                     focusCallback: Lang.bind(this, this._focusWindows) });
    },

    addGroup: function(root, name, icon, params) {
        let item = Params.parse(params, { sortGroup: SortGroup.MIDDLE,
                                          proxy: root,
                                          focusCallback: null });

        item.root = root;
        item.name = name;
        item.iconName = icon;

        this._items.push(item);
        root.connect('destroy', Lang.bind(this, function() { this.removeGroup(root); }));
        if (root instanceof St.Widget)
            global.focus_manager.add_group(root);
    },

    removeGroup: function(root) {
        if (root instanceof St.Widget)
            global.focus_manager.remove_group(root);
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].root == root) {
                this._items.splice(i, 1);
                return;
            }
        }
    },

    focusGroup: function(item, timestamp) {
        if (item.focusCallback)
            item.focusCallback(timestamp);
        else
            item.root.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
    },

    // Sort the items into a consistent order; panel first, tray last,
    // and everything else in between, sorted by X coordinate, so that
    // they will have the same left-to-right ordering in the
    // Ctrl-Alt-Tab dialog as they do onscreen.
    _sortItems: function(a, b) {
        if (a.sortGroup != b.sortGroup)
            return a.sortGroup - b.sortGroup;

        let ax, bx, y;
        [ax, y] = a.proxy.get_transformed_position();
        [bx, y] = b.proxy.get_transformed_position();

        return ax - bx;
    },

    popup: function(backward, binding, mask) {
        // Start with the set of focus groups that are currently mapped
        let items = this._items.filter(function (item) { return item.proxy.mapped; });

        // And add the windows metacity would show in its Ctrl-Alt-Tab list
        if (Main.sessionMode.hasWindows && !Main.overview.visible) {
            let screen = global.screen;
            let display = screen.get_display();
            let windows = display.get_tab_list(Meta.TabList.DOCKS, screen, screen.get_active_workspace ());
            let windowTracker = Shell.WindowTracker.get_default();
            let textureCache = St.TextureCache.get_default();
            for (let i = 0; i < windows.length; i++) {
                let icon = null;
                let iconName = null;
                if (windows[i].get_window_type () == Meta.WindowType.DESKTOP) {
                    iconName = 'video-display-symbolic';
                } else {
                    let app = windowTracker.get_window_app(windows[i]);
                    if (app)
                        icon = app.create_icon_texture(POPUP_APPICON_SIZE);
                    else
                        icon = textureCache.bind_pixbuf_property(windows[i], 'icon');
                }

                items.push({ name: windows[i].title,
                             proxy: windows[i].get_compositor_private(),
                             focusCallback: Lang.bind(windows[i],
                                 function(timestamp) {
                                     Main.activateWindow(this, timestamp);
                                 }),
                             iconActor: icon,
                             iconName: iconName,
                             sortGroup: SortGroup.MIDDLE });
            }
        }

        if (!items.length)
            return;

        items.sort(Lang.bind(this, this._sortItems));

        if (!this._popup) {
            this._popup = new CtrlAltTabPopup(items);
            this._popup.show(backward, binding, mask);

            this._popup.actor.connect('destroy',
                                      Lang.bind(this, function() {
                                          this._popup = null;
                                      }));
        }
    },

    _focusWindows: function(timestamp) {
        global.screen.focus_default_window(timestamp);
    }
});

const CtrlAltTabPopup = new Lang.Class({
    Name: 'CtrlAltTabPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _createSwitcher: function() {
        this._switcherList = new CtrlAltTabSwitcher(this._items);
        return true;
    },

    _initialSelection: function(backward, binding) {
        if (binding == 'switch-panels') {
            if (backward)
                this._selectedIndex = this._items.length - 1;
        } else if (binding == 'switch-panels-backward') {
            if (!backward)
                this._selectedIndex = this._items.length - 1;
        }
        this._select(this._selectedIndex);
    },

    _keyPressHandler: function(keysym, backwards, action) {
        if (action == Meta.KeyBindingAction.SWITCH_PANELS)
            this._select(backwards ? this._previous() : this._next());
        else if (action == Meta.KeyBindingAction.SWITCH_PANELS_BACKWARD)
            this._select(backwards ? this._next() : this._previous());
        else if (keysym == Clutter.Left)
            this._select(this._previous());
        else if (keysym == Clutter.Right)
            this._select(this._next());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    },

    _finish : function(time) {
        this.parent(time);
        Main.ctrlAltTabManager.focusGroup(this._items[this._selectedIndex], time);
    },
});

const CtrlAltTabSwitcher = new Lang.Class({
    Name: 'CtrlAltTabSwitcher',
    Extends: SwitcherPopup.SwitcherList,

    _init : function(items) {
        this.parent(true);

        for (let i = 0; i < items.length; i++)
            this._addIcon(items[i]);
    },

    _addIcon : function(item) {
        let box = new St.BoxLayout({ style_class: 'alt-tab-app',
                                     vertical: true });

        let icon = item.iconActor;
        if (!icon) {
            icon = new St.Icon({ icon_name: item.iconName,
                                 icon_size: POPUP_APPICON_SIZE });
        }
        box.add(icon, { x_fill: false, y_fill: false } );

        let text = new St.Label({ text: item.name });
        box.add(text, { x_fill: false });

        this.addItem(box, text);
    }
});
