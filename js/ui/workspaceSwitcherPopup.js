import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import St from 'gi://St';

import * as Layout from './layout.js';
import * as Main from './main.js';

const ANIMATION_TIME = 100;
const DISPLAY_TIMEOUT = 600;

export const MonitorWorkspaceSwitcherPopup = GObject.registerClass(
class MonitorWorkspaceSwitcherPopup extends Clutter.Actor {
    constructor(constraint) {
        super({
            offscreen_redirect: Clutter.OffscreenRedirect.ALWAYS,
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.END,
        });
        this.add_constraint(constraint);

        this._list = new St.BoxLayout({
            style_class: 'workspace-switcher',
        });
        this.add_child(this._list);
    }

    redisplay(activeWorkspaceIndex) {
        const workspaceManager = global.workspace_manager;

        this._list.destroy_all_children();

        for (let i = 0; i < workspaceManager.n_workspaces; i++) {
            const indicator = new St.Bin({
                style_class: 'ws-switcher-indicator',
            });

            if (i === activeWorkspaceIndex)
                indicator.add_style_pseudo_class('active');

            this._list.add_child(indicator);
        }
    }
});

export const WorkspaceSwitcherPopup = GObject.registerClass(
class WorkspaceSwitcherPopup extends Clutter.Actor {
    constructor() {
        super();

        this._timeoutId = 0;

        Main.uiGroup.add_child(this);

        this.hide();

        if (Meta.prefs_get_workspaces_only_on_primary()) {
            const constraint = new Layout.MonitorConstraint({primary: true});
            this.add_child(new MonitorWorkspaceSwitcherPopup(constraint));
        } else {
            const monitors = Main.layoutManager.monitors;
            monitors.forEach((_, index) => {
                const constraint = new Layout.MonitorConstraint({index});
                this.add_child(new MonitorWorkspaceSwitcherPopup(constraint));
            });
        }

        const workspaceManager = global.workspace_manager;
        workspaceManager.connectObject(
            'workspace-added', this._redisplayAllPopups.bind(this),
            'workspace-removed', this._redisplayAllPopups.bind(this), this);
        this.connect('destroy', this._onDestroy.bind(this));
    }

    _redisplayAllPopups() {
        for (const popup of this)
            popup.redisplay(this._activeWorkspaceIndex);
    }

    display(activeWorkspaceIndex) {
        this._activeWorkspaceIndex = activeWorkspaceIndex;

        if (this._timeoutId !== 0)
            GLib.source_remove(this._timeoutId);
        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, DISPLAY_TIMEOUT, this._onTimeout.bind(this));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._onTimeout');

        this._redisplayAllPopups();

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
