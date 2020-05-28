// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ControlsManager */

const { Clutter, GObject, St } = imports.gi;

const Dash = imports.ui.dash;
const Main = imports.ui.main;
const ViewSelector = imports.ui.viewSelector;
const Overview = imports.ui.overview;

var SIDE_CONTROLS_ANIMATION_TIME = Overview.ANIMATION_TIME;

var FaderControl = GObject.registerClass(
class FaderControl extends St.Widget {
    _init(params) {
        super._init(params);

        this._inDrag = false;

        Main.overview.connect('item-drag-begin', this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-cancelled', this._onDragEnd.bind(this));

        Main.overview.connect('window-drag-begin', this._onWindowDragBegin.bind(this));
        Main.overview.connect('window-drag-cancelled', this._onWindowDragEnd.bind(this));
        Main.overview.connect('window-drag-end', this._onWindowDragEnd.bind(this));
    }

    _onWindowDragBegin() {
        this._onDragBegin();
    }

    _onWindowDragEnd() {
        this._onDragEnd();
    }

    _onDragBegin() {
        this._inDrag = true;
    }

    _onDragEnd() {
        this._inDrag = false;
    }

    fadeIn() {
        this.ease({
            opacity: 255,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
        });
    }

    fadeHalf() {
        this.ease({
            opacity: 128,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }
});

var DashFader = GObject.registerClass(
class DashFader extends FaderControl {
    _init(dash) {
        super._init({
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.END,
        });

        this._dash = dash;
        this.add_child(this._dash);
    }

    _onWindowDragBegin() {
        this.fadeHalf();
    }

    _onWindowDragEnd() {
        this.fadeIn();
    }
});

var ControlsManager = GObject.registerClass(
class ControlsManager extends St.Widget {
    _init(searchEntry) {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            clip_to_allocation: true,
        });

        this.dash = new Dash.Dash();
        this._dashFader = new DashFader(this.dash);

        let workspaceManager = global.workspace_manager;
        let activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        this._workspaceAdjustment = new St.Adjustment({
            actor: this,
            value: activeWorkspaceIndex,
            lower: 0,
            page_increment: 1,
            page_size: 1,
            step_increment: 0,
            upper: workspaceManager.n_workspaces,
        });

        this._nWorkspacesNotifyId =
            workspaceManager.connect('notify::n-workspaces',
                this._updateAdjustment.bind(this));

        this.viewSelector = new ViewSelector.ViewSelector(searchEntry,
            this._workspaceAdjustment, this.dash.showAppsButton);

        this._group = new St.BoxLayout({
            name: 'overview-group',
            vertical: true,
            x_expand: true,
            y_expand: true,
        });
        this.add_actor(this._group);

        const box = new St.BoxLayout({
            x_expand: true,
            y_expand: true,
        });
        box.add_child(this.viewSelector);

        this._group.add_child(box);
        this._group.add_actor(this._dashFader);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        global.workspace_manager.disconnect(this._nWorkspacesNotifyId);
    }

    _updateAdjustment() {
        let workspaceManager = global.workspace_manager;
        let newNumWorkspaces = workspaceManager.n_workspaces;
        let activeIndex = workspaceManager.get_active_workspace_index();

        this._workspaceAdjustment.upper = newNumWorkspaces;

        // A workspace might have been inserted or removed before the active
        // one, causing the adjustment to go out of sync, so update the value
        this._workspaceAdjustment.remove_transition('value');
        this._workspaceAdjustment.value = activeIndex;
    }
});
