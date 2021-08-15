// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceSwitcherPopup */

const { Clutter, GLib, GObject, St } = imports.gi;

const Layout = imports.ui.layout;
const Main = imports.ui.main;

var ANIMATION_TIME = 100;
var DISPLAY_TIMEOUT = 600;


var WorkspaceSwitcherPopup = GObject.registerClass(
class WorkspaceSwitcherPopup extends Clutter.Actor {
    _init() {
        super._init({
            offscreen_redirect: Clutter.OffscreenRedirect.ALWAYS,
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.END,
        });

        const constraint = new Layout.MonitorConstraint({ primary: true });
        this.add_constraint(constraint);

        Main.uiGroup.add_actor(this);

        this._timeoutId = 0;

        this._list = new St.BoxLayout({
            style_class: 'workspace-switcher',
        });
        this.add_child(this._list);

        this._redisplay();

        this.hide();

        let workspaceManager = global.workspace_manager;
        workspaceManager.connectObject(
            'workspace-added', this._redisplay.bind(this),
            'workspace-removed', this._redisplay.bind(this), this);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _redisplay() {
        let workspaceManager = global.workspace_manager;

        this._list.destroy_all_children();

        for (let i = 0; i < workspaceManager.n_workspaces; i++) {
            const indicator = new St.Bin({
                style_class: 'ws-switcher-indicator',
            });

            if (i === this._activeWorkspaceIndex)
                indicator.add_style_pseudo_class('active');

            this._list.add_actor(indicator);
        }
    }

    display(activeWorkspaceIndex) {
        this._activeWorkspaceIndex = activeWorkspaceIndex;

        this._redisplay();
        if (this._timeoutId != 0)
            GLib.source_remove(this._timeoutId);
        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, DISPLAY_TIMEOUT, this._onTimeout.bind(this));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._onTimeout');

        const duration = this.visible ? 0 : ANIMATION_TIME;
        this.show();
        this.opacity = 0;
        this.ease({
            opacity: 255,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _onTimeout() {
        GLib.source_remove(this._timeoutId);
        this._timeoutId = 0;
        this.ease({
            opacity: 0.0,
            duration: ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this.destroy(),
        });
        return GLib.SOURCE_REMOVE;
    }

    _onDestroy() {
        if (this._timeoutId)
            GLib.source_remove(this._timeoutId);
        this._timeoutId = 0;
    }
});
