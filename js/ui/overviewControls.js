// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ControlsManager */

const { Clutter, GObject, St } = imports.gi;

const Dash = imports.ui.dash;
const Overview = imports.ui.overview;
const ViewSelector = imports.ui.viewSelector;

var SIDE_CONTROLS_ANIMATION_TIME = Overview.ANIMATION_TIME;

var ControlsState = {
    HIDDEN: 0,
    WINDOW_PICKER: 1,
    APP_GRID: 2,
};

var ControlsManagerLayout = GObject.registerClass(
class ControlsManagerLayout extends Clutter.BoxLayout {
    _init(searchEntry, viewSelector, dash, stateAdjustment) {
        super._init({ orientation: Clutter.Orientation.VERTICAL });

        this._stateAdjustment = stateAdjustment;
        this._searchEntry = searchEntry;
        this._viewSelector = viewSelector;
        this._dash = dash;

        stateAdjustment.connect('notify::value', () => this.layout_changed());
    }

    vfunc_set_container(container) {
        this._container = container;
        this.hookup_style(container);
    }

    vfunc_allocate(container, box) {
        const childBox = new Clutter.ActorBox();

        const { spacing } = this;

        const [width, height] = box.get_size();
        let availableHeight = height;

        // Search entry
        const [searchHeight] = this._searchEntry.get_preferred_height(width);
        childBox.set_origin(0, 0);
        childBox.set_size(width, searchHeight);
        this._searchEntry.allocate(childBox);

        availableHeight -= searchHeight + spacing;

        // Dash
        const [, dashHeight] = this._dash.get_preferred_height(width);
        childBox.set_origin(0, height - dashHeight);
        childBox.set_size(width, dashHeight);
        this._dash.allocate(childBox);

        availableHeight -= dashHeight + spacing;

        // ViewSelector
        const initialBox = new Clutter.ActorBox();
        initialBox.set_origin(0, 0);
        initialBox.set_size(width, height);

        childBox.set_origin(0, searchHeight + spacing);
        childBox.set_size(width, availableHeight);

        const page = this._viewSelector.getActivePage();
        const progress = page === ViewSelector.ViewPage.SEARCH
            ? 1 : Math.min(this._stateAdjustment.value, 1);
        const viewSelectorBox = initialBox.interpolate(childBox, progress);
        this._viewSelector.allocate(viewSelectorBox);
    }
});

var ControlsManager = GObject.registerClass(
class ControlsManager extends St.Widget {
    _init() {
        super._init({
            style_class: 'controls-manager',
            x_expand: true,
            y_expand: true,
            clip_to_allocation: true,
        });

        this._ignoreShowAppsButtonToggle = false;

        this._searchEntry = new St.Entry({
            style_class: 'search-entry',
            /* Translators: this is the text displayed
               in the search entry when no search is
               active; it should not exceed ~30
               characters. */
            hint_text: _('Type to search'),
            track_hover: true,
            can_focus: true,
        });
        this._searchEntry.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);
        const searchEntryBin = new St.Bin({
            child: this._searchEntry,
            x_align: Clutter.ActorAlign.CENTER,
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

        this._stateAdjustment = new St.Adjustment({
            actor: this,
            value: ControlsState.WINDOW_PICKER,
            lower: ControlsState.HIDDEN,
            upper: ControlsState.APP_GRID,
        });

        this._nWorkspacesNotifyId =
            workspaceManager.connect('notify::n-workspaces',
                this._updateAdjustment.bind(this));

        this.viewSelector = new ViewSelector.ViewSelector(this._searchEntry,
            this._workspaceAdjustment,
            this.dash.showAppsButton,
            this._stateAdjustment);

        this.add_child(searchEntryBin);
        this.add_child(this.dash);
        this.add_child(this.viewSelector);

        this.layout_manager = new ControlsManagerLayout(searchEntryBin,
            this.viewSelector, this.dash, this._stateAdjustment);

        this.dash.showAppsButton.connect('notify::checked',
            this._onShowAppsButtonToggled.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onShowAppsButtonToggled() {
        if (this._ignoreShowAppsButtonToggle)
            return;

        const checked = this.dash.showAppsButton.checked;

        const value = checked
            ? ControlsState.APP_GRID : ControlsState.WINDOW_PICKER;
        this._stateAdjustment.remove_transition('value');
        this._stateAdjustment.ease(value, {
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
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

    animateToOverview(state, callback) {
        this._ignoreShowAppsButtonToggle = true;

        this.viewSelector.prepareToEnterOverview();

        this._stateAdjustment.value = ControlsState.HIDDEN;
        this._stateAdjustment.ease(state, {
            duration: Overview.ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => {
                if (callback)
                    callback();
            },
        });

        this.dash.showAppsButton.checked =
            state === ControlsState.APP_GRID;

        this._ignoreShowAppsButtonToggle = false;
    }

    animateFromOverview(callback) {
        this._ignoreShowAppsButtonToggle = true;

        this.viewSelector.prepareToLeaveOverview();

        this._stateAdjustment.ease(ControlsState.HIDDEN, {
            duration: Overview.ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => {
                this.dash.showAppsButton.checked = false;
                this._ignoreShowAppsButtonToggle = false;

                if (callback)
                    callback();
            },
        });
    }

    get searchEntry() {
        return this._searchEntry;
    }
});
