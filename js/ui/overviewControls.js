// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ControlsManager */

const { Clutter, GObject, St } = imports.gi;

const Dash = imports.ui.dash;
const ViewSelector = imports.ui.viewSelector;

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

        this._group.add_child(this.viewSelector);
        this._group.add_actor(this.dash);

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
