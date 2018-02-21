// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*

const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const RemoteMenu = imports.ui.remoteMenu;

var WindowMenu = new Lang.Class({
    Name: 'WindowMenu',
    Extends: PopupMenu.PopupMenu,

    _init(window, sourceActor) {
        this.parent(sourceActor, 0, St.Side.TOP);

        this.actor.add_style_class_name('window-menu');

        Main.layoutManager.uiGroup.add_actor(this.actor);
        this.actor.hide();

        this._buildMenu(window);
    },

    _buildMenu(window) {
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
            item.setOrnament(PopupMenu.Ornament.CHECK);
        if (window.get_maximized() == Meta.MaximizeFlags.BOTH ||
            type == Meta.WindowType.DOCK ||
            type == Meta.WindowType.DESKTOP ||
            type == Meta.WindowType.SPLASHSCREEN)
            item.setSensitive(false);

        if (Main.sessionMode.hasWorkspaces &&
            (!Meta.prefs_get_workspaces_only_on_primary() ||
             window.is_on_primary_monitor())) {
            let isSticky = window.is_on_all_workspaces();

            item = this.addAction(_("Always on Visible Workspace"), Lang.bind(this, function() {
                if (isSticky)
                    window.unstick();
                else
                    window.stick();
            }));
            if (isSticky)
                item.setOrnament(PopupMenu.Ornament.CHECK);
            if (window.is_always_on_all_workspaces())
                item.setSensitive(false);

            if (!isSticky) {
                let workspace = window.get_workspace();
                if (workspace != workspace.get_neighbor(Meta.MotionDirection.LEFT)) {
                     this.addAction(_("Move to Workspace Left"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.LEFT));
                    }));
                }
                if (workspace != workspace.get_neighbor(Meta.MotionDirection.RIGHT)) {
                     this.addAction(_("Move to Workspace Right"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.RIGHT));
                    }));
                }
                if (workspace != workspace.get_neighbor(Meta.MotionDirection.UP)) {
                    this.addAction(_("Move to Workspace Up"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.UP));
                    }));
                }
                if (workspace != workspace.get_neighbor(Meta.MotionDirection.DOWN)) {
                     this.addAction(_("Move to Workspace Down"), Lang.bind(this, function(event) {
                        window.change_workspace(workspace.get_neighbor(Meta.MotionDirection.DOWN));
                    }));
                }
            }
        }

        let screen = global.screen;
        let nMonitors = screen.get_n_monitors();
        if (nMonitors > 1) {
          this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

          let monitorIndex = window.get_monitor();

          let upMonitorIndex = screen.get_monitor_neighbor_index(monitorIndex, Meta.ScreenDirection.UP);
          if (upMonitorIndex != -1) {
            this.addAction(_("Move to Monitor Up"), Lang.bind(this, function(event) {
              window.move_to_monitor(upMonitorIndex);
            }));
          }
          let downMonitorIndex = screen.get_monitor_neighbor_index(monitorIndex, Meta.ScreenDirection.DOWN);
          if (downMonitorIndex != -1) {
            this.addAction(_("Move to Monitor Down"), Lang.bind(this, function(event) {
              window.move_to_monitor(downMonitorIndex);
            }));
          }
          let leftMonitorIndex = screen.get_monitor_neighbor_index(monitorIndex, Meta.ScreenDirection.LEFT);
          if (leftMonitorIndex != -1) {
            this.addAction(_("Move to Monitor Left"), Lang.bind(this, function(event) {
              window.move_to_monitor(leftMonitorIndex);
            }));
          }
          let rightMonitorIndex = screen.get_monitor_neighbor_index(monitorIndex, Meta.ScreenDirection.RIGHT);
          if (rightMonitorIndex != -1) {
            this.addAction(_("Move to Monitor Right"), Lang.bind(this, function(event) {
              window.move_to_monitor(rightMonitorIndex);
            }));
          }
        }

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        item = this.addAction(_("Close"), Lang.bind(this, function(event) {
            window.delete(event.get_time());
        }));
        if (!window.can_close())
            item.setSensitive(false);
    }
});

var AppMenu = new Lang.Class({
    Name: 'AppMenu',
    Extends: RemoteMenu.RemoteMenu,

    _init(window, sourceActor) {
        let app = Shell.WindowTracker.get_default().get_window_app(window);

        this.parent(sourceActor, app.menu, app.action_group);

        this.actor.add_style_class_name('fallback-app-menu');
        let variant = window.get_gtk_theme_variant();
        if (variant)
            this.actor.add_style_class_name(variant);

        Main.layoutManager.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }
});

var WindowMenuManager = new Lang.Class({
    Name: 'WindowMenuManager',

    _init() {
        this._manager = new PopupMenu.PopupMenuManager({ actor: Main.layoutManager.dummyCursor });

        this._sourceActor = new St.Widget({ reactive: true, visible: false });
        this._sourceActor.connect('button-press-event', Lang.bind(this,
            function() {
                this._manager.activeMenu.toggle();
            }));
        Main.uiGroup.add_actor(this._sourceActor);
    },

    showWindowMenuForWindow(window, type, rect) {
        let menuType = (type == Meta.WindowMenuType.WM) ? WindowMenu : AppMenu;
        let menu = new menuType(window, this._sourceActor);

        this._manager.addMenu(menu);

        menu.connect('activate', function() {
            window.check_alive(global.get_current_time());
        });
        let destroyId = window.connect('unmanaged',
            function() {
                menu.close();
            });

        this._sourceActor.set_size(Math.max(1, rect.width), Math.max(1, rect.height));
        this._sourceActor.set_position(rect.x, rect.y);
        this._sourceActor.show();

        menu.open(BoxPointer.PopupAnimation.NONE);
        menu.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        menu.connect('open-state-changed', Lang.bind(this, function(menu_, isOpen) {
            if (isOpen)
                return;

            this._sourceActor.hide();
            menu.destroy();
            window.disconnect(destroyId);
        }));
    }
});
