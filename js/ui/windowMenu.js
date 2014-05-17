// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*

const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const WindowMenu = new Lang.Class({
    Name: 'WindowMenu',
    Extends: PopupMenu.PopupMenu,

    _init: function(window) {
        this.parent(Main.layoutManager.dummyCursor, 0, St.Side.TOP);

        this.actor.add_style_class_name('window-menu');

        Main.layoutManager.uiGroup.add_actor(this.actor);
        this.actor.hide();

        this._buildMenu(window);
    },

    _buildMenu: function(window) {
        let type = window.get_window_type();

        let item;

        item = this.addAction(_("Minimize"), Lang.bind(this, function(event) {
            window.minimize();
        }));
        if (!window.can_minimize())
            item.setSensitive(false);

        if (window.get_maximized()) {
            item = this.addAction(_("Unmaximize"), Lang.bind(this, function() {
                window.unmaximize(Meta.MaximizeFlags.BOTH);
            }));
        } else {
            item = this.addAction(_("Maximize"), Lang.bind(this, function() {
                window.maximize(Meta.MaximizeFlags.BOTH);
            }));
        }
        if (!window.can_maximize())
            item.setSensitive(false);

        item = this.addAction(_("Move"), Lang.bind(this, function(event) {
            window.begin_grab_op(Meta.GrabOp.KEYBOARD_MOVING, true, event.get_time());
        }));
        if (!window.allows_move())
            item.setSensitive(false);

        item = this.addAction(_("Resize"), Lang.bind(this, function(event) {
            window.begin_grab_op(Meta.GrabOp.KEYBOARD_RESIZING_UNKNOWN, true, event.get_time());
        }));
        if (!window.allows_resize())
            item.setSensitive(false);

        if (!window.titlebar_is_onscreen() && type != Meta.WindowType.DOCK && type != Meta.WindowType.DESKTOP) {
            this.addAction(_("Move Titlebar Onscreen"), Lang.bind(this, function(event) {
                window.shove_titlebar_onscreen();
            }));
        }

        item = this.addAction(_("Always on Top"), Lang.bind(this, function() {
            if (window.is_above())
                window.unmake_above();
            else
                window.make_above();
        }));
        if (window.is_above())
            item.setOrnament(PopupMenu.Ornament.DOT);
        if (window.get_maximized() ||
            type == Meta.WindowType.DOCK ||
            type == Meta.WindowType.DESKTOP ||
            type == Meta.WindowType.SPLASHSCREEN)
            item.setSensitive(false);

        if (!Meta.prefs_get_workspaces_only_on_primary() || window.is_on_primary_monitor()) {
            let isSticky = window.is_on_all_workspaces();

            item = this.addAction(_("Always on Visible Workspace"), Lang.bind(this, function() {
                if (isSticky)
                    window.unstick();
                else
                    window.stick();
            }));
            if (isSticky)
                item.setOrnament(PopupMenu.Ornament.DOT);
            if (window.is_always_on_all_workspaces())
                item.setSensitive(false);

            let nWorkspaces = global.screen.n_workspaces;

            if (!isSticky) {
                let workspace = window.get_workspace();
                let idx = workspace.index();
                if (idx > 0) {
                    this.addAction(_("Move to Workspace Up"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.UP));
                    }));
                }
                if (idx < nWorkspaces) {
                     this.addAction(_("Move to Workspace Down"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.DOWN));
                    }));
                }
            }
        }

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        item = this.addAction(_("Close"), Lang.bind(this, function(event) {
            window.close(event.get_time());
        }));
        if (!window.can_close())
            item.setSensitive(false);
    }
});

const WindowMenuManager = new Lang.Class({
    Name: 'WindowMenuManager',

    _init: function() {
        this._manager = new PopupMenu.PopupMenuManager({ actor: Main.layoutManager.dummyCursor });
    },

    showForWindow: function(window) {
        let menu = new WindowMenu(window);
        this._manager.addMenu(menu);

        let [x, y] = global.get_pointer();
        Main.layoutManager.setDummyCursorGeometry(x, y, 0, 0);
        menu.open(BoxPointer.PopupAnimation.NONE);
        menu.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        menu.connect('open-state-changed', Lang.bind(this, function(menu_, isOpen) {
            if (!isOpen)
                menu.destroy();
        }));
    },
});
