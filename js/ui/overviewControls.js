// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ControlsManager */

const { Clutter, Gio, GObject, Meta, Shell, St } = imports.gi;

const AppDisplay = imports.ui.appDisplay;
const Dash = imports.ui.dash;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const SearchController = imports.ui.searchController;
const WindowManager = imports.ui.windowManager;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;
const WorkspacesView = imports.ui.workspacesView;

var SIDE_CONTROLS_ANIMATION_TIME = Overview.ANIMATION_TIME;

const DASH_HEIGHT_PERCENTAGE = 0.15;

var ControlsState = {
    HIDDEN: 0,
    WINDOW_PICKER: 1,
    APP_GRID: 2,
};

var DashFader = GObject.registerClass(
class DashFader extends St.Bin {
    _init(dash) {
        super._init({
            child: dash,
            x_expand: true,
            y_expand: true,
        });

        this._dash = dash;

        Main.overview.connect('window-drag-begin', this._onWindowDragBegin.bind(this));
        Main.overview.connect('window-drag-cancelled', this._onWindowDragEnd.bind(this));
        Main.overview.connect('window-drag-end', this._onWindowDragEnd.bind(this));
    }

    _onWindowDragBegin() {
        this.ease({
            opacity: 128,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _onWindowDragEnd() {
        this.ease({
            opacity: 255,
            duration: SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
        });
    }

    setMaxHeight(maxHeight) {
        this._dash.setMaxHeight(maxHeight);
    }
});

var ControlsManagerLayout = GObject.registerClass(
class ControlsManagerLayout extends Clutter.BinLayout {
    _init(searchEntry, appDisplay, workspacesDisplay, workspacesThumbnails,
        searchController, dash, adjustment) {
        super._init();

        this._appDisplay = appDisplay;
        this._workspacesDisplay = workspacesDisplay;
        this._workspacesThumbnails = workspacesThumbnails;
        this._adjustment = adjustment;
        this._searchEntry = searchEntry;
        this._searchController = searchController;
        this._dash = dash;

        adjustment.connect('notify::value', () => this.layout_changed());
    }

    _getWorkspacesBoxForState(state, params) {
        const workspaceBox = params.box.copy();
        const [width, height] = workspaceBox.get_size();

        switch (state) {
        case ControlsState.HIDDEN:
            break;
        case ControlsState.WINDOW_PICKER:
            workspaceBox.set_origin(0,
                params.searchHeight + params.spacing +
                (params.thumbnailsHeight > 0 ? params.thumbnailsHeight + params.spacing : 0));
            workspaceBox.set_size(width,
                height -
                params.dashHeight - params.spacing -
                params.searchHeight - params.spacing -
                (params.thumbnailsHeight > 0 ? params.thumbnailsHeight + params.spacing : 0));
            break;
        case ControlsState.APP_GRID:
            workspaceBox.set_origin(0, params.searchHeight + params.spacing);
            workspaceBox.set_size(width, Math.round(Math.max(height * 0.15)));
            break;
        }

        return workspaceBox;
    }

    _getAppDisplayBoxForState(state, params, appGridBox) {
        const { box, searchHeight, dashHeight, spacing } = params;
        const [width, height] = box.get_size();
        const appDisplayBox = new Clutter.ActorBox();

        switch (state) {
        case ControlsState.HIDDEN:
        case ControlsState.WINDOW_PICKER:
            appDisplayBox.set_origin(0, box.y2);
            break;
        case ControlsState.APP_GRID:
            appDisplayBox.set_origin(0,
                searchHeight + spacing + appGridBox.get_height());
            break;
        }

        appDisplayBox.set_size(width,
            height -
            searchHeight - spacing -
            appGridBox.get_height() - spacing -
            dashHeight);

        return appDisplayBox;
    }

    vfunc_set_container(container) {
        this._container = container;
    }

    vfunc_allocate(container, box) {
        const childBox = new Clutter.ActorBox();

        const spacing = container.get_theme_node().get_length('spacing');

        const [width, height] = box.get_size();
        let availableHeight = height;

        // Search entry
        const [searchHeight] = this._searchEntry.get_preferred_height(width);
        childBox.set_origin(0, 0);
        childBox.set_size(width, searchHeight);
        this._searchEntry.allocate(childBox);

        availableHeight -= searchHeight + spacing;

        // Dash
        const maxDashHeight = Math.round(box.get_height() * DASH_HEIGHT_PERCENTAGE);
        this._dash.setMaxHeight(maxDashHeight);

        let [, dashHeight] = this._dash.get_preferred_height(width);
        dashHeight = Math.min(dashHeight, maxDashHeight);
        childBox.set_origin(0, height - dashHeight);
        childBox.set_size(width, dashHeight);
        this._dash.allocate(childBox);

        availableHeight -= dashHeight + spacing;

        // Workspace Thumbnails
        let thumbnailsHeight = 0;
        if (this._workspacesThumbnails.visible) {
            [thumbnailsHeight] =
                this._workspacesThumbnails.get_preferred_height(width);
            thumbnailsHeight = Math.min(thumbnailsHeight, height * 0.06);
            childBox.set_origin(0, searchHeight + spacing);
            childBox.set_size(width, thumbnailsHeight);
            this._workspacesThumbnails.allocate_align_fill(childBox, 0.5, 0.5, false, true);
        }

        // Workspaces
        const params = { box, searchHeight, dashHeight, thumbnailsHeight, spacing };
        const workspaceBoxes = [
            this._getWorkspacesBoxForState(ControlsState.HIDDEN, params),
            this._getWorkspacesBoxForState(ControlsState.WINDOW_PICKER, params),
            this._getWorkspacesBoxForState(ControlsState.APP_GRID, params),
        ];
        const [state, initialState, finalState, progress] =
            this._adjustment.getState();
        if (initialState === finalState) {
            const workspacesBox = workspaceBoxes[state];
            this._workspacesDisplay.allocate(workspacesBox);
        } else {
            const initialBox = workspaceBoxes[initialState];
            const finalBox = workspaceBoxes[finalState];

            this._workspacesDisplay.allocate(initialBox.interpolate(finalBox, progress));
        }

        // AppDisplay
        const appGridBox = workspaceBoxes[ControlsState.APP_GRID];
        const appDisplayBoxes = [
            this._getAppDisplayBoxForState(ControlsState.HIDDEN, params, appGridBox),
            this._getAppDisplayBoxForState(ControlsState.WINDOW_PICKER, params, appGridBox),
            this._getAppDisplayBoxForState(ControlsState.APP_GRID, params, appGridBox),
        ];

        if (initialState === finalState) {
            const appDisplayBox = appDisplayBoxes[state];
            this._appDisplay.allocate(appDisplayBox);
        } else {
            const initialBox = appDisplayBoxes[initialState];
            const finalBox = appDisplayBoxes[finalState];

            this._appDisplay.allocate(initialBox.interpolate(finalBox, progress));
        }

        // Search
        childBox.set_origin(0, searchHeight + spacing);
        childBox.set_size(width, availableHeight);
        this._searchController.allocate(childBox);
    }
});

var OverviewAdjustment = GObject.registerClass(
class OverviewAdjustment extends St.Adjustment {
    _init(actor) {
        super._init({
            actor,
            value: ControlsState.WINDOW_PICKER,
            lower: ControlsState.HIDDEN,
            upper: ControlsState.APP_GRID,
        });
    }

    getState() {
        const state = this.value;

        const transition = this.get_transition('value');
        let initialState = transition
            ? transition.get_interval().peek_initial_value()
            : state;
        let finalState = transition
            ? transition.get_interval().peek_final_value()
            : state;

        if (initialState > finalState) {
            initialState = Math.ceil(initialState);
            finalState = Math.floor(finalState);
        } else {
            initialState = Math.floor(initialState);
            finalState = Math.ceil(finalState);
        }

        const length = Math.abs(finalState - initialState);
        const progress = length > 0
            ? Math.abs((state - initialState) / length)
            : 1;

        return [state, initialState, finalState, progress];
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

        this._animating = false;

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

        this._adjustment = new OverviewAdjustment(this);
        this._adjustment.connect('notify::value', this._update.bind(this));

        this._nWorkspacesNotifyId =
            workspaceManager.connect('notify::n-workspaces',
                this._updateAdjustment.bind(this));

        this._searchController = new SearchController.SearchController(
            this._searchEntry,
            this.dash.showAppsButton);
        this._searchController.connect('notify::searching', this._onSearchChanged.bind(this));

        this._thumbnailsBox =
            new WorkspaceThumbnail.ThumbnailsBox(this._workspaceAdjustment);
        this._workspacesDisplay = new WorkspacesView.WorkspacesDisplay(
            this._workspaceAdjustment,
            this._adjustment);
        this._appDisplay = new AppDisplay.AppDisplay();

        this.add_child(searchEntryBin);
        this.add_child(this._appDisplay);
        this.add_child(this._dashFader);
        this.add_child(this._searchController);
        this.add_child(this._thumbnailsBox);
        this.add_child(this._workspacesDisplay);

        this.layout_manager = new ControlsManagerLayout(searchEntryBin,
            this._appDisplay,
            this._workspacesDisplay,
            this._thumbnailsBox,
            this._searchController,
            this._dashFader,
            this._adjustment);

        this.dash.showAppsButton.connect('notify::checked',
            this._onShowAppsButtonToggled.bind(this));

        Main.wm.addKeybinding(
            'toggle-application-view',
            new Gio.Settings({ schema_id: WindowManager.SHELL_KEYBINDINGS_SCHEMA }),
            Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
            Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
            this._toggleAppsPage.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));

        this._update();
    }

    _getSnapForState(state) {
        switch (state) {
        case ControlsState.HIDDEN:
        case ControlsState.WINDOW_PICKER:
            return Clutter.Orientation.VERTICAL;
        case ControlsState.APP_GRID:
            return Clutter.Orientation.HORIZONTAL;
        default:
            return Clutter.Orientation.VERTICAL;
        }
    }

    _getThumbnailsBoxParams() {
        const [state, initialState, finalState, progress] = this._adjustment.getState();

        const opacityForState = s => {
            switch (s) {
            case ControlsState.HIDDEN:
            case ControlsState.WINDOW_PICKER:
                return [255, 1, 0];
            case ControlsState.APP_GRID:
                return [0, 0.66, this._thumbnailsBox.height / 2];
            default:
                return [255, 1, 0];
            }
        };

        if (initialState === finalState)
            return opacityForState(state);

        const [initialOpacity, initialScale, initialTranslationY] =
            opacityForState(initialState);

        const [finalOpacity, finalScale, finalTranslationY] =
            opacityForState(finalState);

        return [
            Math.interpolate(initialOpacity, finalOpacity, progress),
            Math.interpolate(initialScale, finalScale, progress),
            Math.interpolate(initialTranslationY, finalTranslationY, progress),
        ];
    }

    _updateThumbnailsBox(animate = false) {
        const { searching } = this._searchController;
        const [opacity, scale, translationY] = this._getThumbnailsBoxParams();

        const thumbnailsBoxVisible = !searching && opacity !== 0;
        if (thumbnailsBoxVisible) {
            this._thumbnailsBox.opacity = 0;
            this._thumbnailsBox.visible = thumbnailsBoxVisible;
        }

        const params = {
            opacity: searching ? 0 : opacity,
            duration: animate ? SIDE_CONTROLS_ANIMATION_TIME : 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => (this._thumbnailsBox.visible = thumbnailsBoxVisible),
        };

        if (!searching) {
            params.scale_x = scale;
            params.scale_y = scale;
            params.translation_y = translationY;
        }

        this._thumbnailsBox.ease(params);
    }

    _update() {
        const [, initialState, finalState, progress] = this._adjustment.getState();

        const snapAxis = Math.interpolate(
            this._getSnapForState(initialState),
            this._getSnapForState(finalState),
            progress);

        const { snapAdjustment } = this._workspacesDisplay;
        snapAdjustment.value = snapAxis;

        this._updateThumbnailsBox();
    }

    _onSearchChanged() {
        const { searching } = this._searchController;

        if (!searching) {
            this._appDisplay.show();
            this._workspacesDisplay.reactive = true;
            this._workspacesDisplay.setPrimaryWorkspaceVisible(true);
        } else {
            this._searchController.show();
        }

        this._updateThumbnailsBox(true);

        this._appDisplay.ease({
            opacity: searching ? 0 : 255,
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => (this._appDisplay.visible = !searching),
        });
        this._workspacesDisplay.ease({
            opacity: searching ? 0 : 255,
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._workspacesDisplay.reactive = !searching;
                this._workspacesDisplay.setPrimaryWorkspaceVisible(!searching);
            },
        });
        this._searchController.ease({
            opacity: searching ? 255 : 0,
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => (this._searchController.visible = searching),
        });
    }

    _onShowAppsButtonToggled() {
        if (this._animating)
            return;

        const checked = this.dash.showAppsButton.checked;

        const value = checked
            ? ControlsState.APP_GRID : ControlsState.WINDOW_PICKER;
        this._adjustment.remove_transition('value');
        this._adjustment.ease(value, {
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _toggleAppsPage() {
        if (Main.overview.visible) {
            const checked = this.dash.showAppsButton.checked;
            this.dash.showAppsButton.checked = !checked;
        } else {
            Main.overview.showApps();
        }
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

    vfunc_unmap() {
        this._workspacesDisplay.hide();
        super.vfunc_unmap();
    }

    animateToOverview(state, onComplete) {
        this._animating = true;

        this._searchController.prepareToEnterOverview();
        this._workspacesDisplay.prepareToEnterOverview();
        if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
            Main.overview.fadeOutDesktop();

        this._adjustment.value = ControlsState.HIDDEN;
        this._adjustment.ease(state, {
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete,
        });

        this.dash.showAppsButton.checked =
            state === ControlsState.APP_GRID;

        this._animating = false;
    }

    animateFromOverview(onComplete) {
        this._animating = true;

        this._searchController.reset();
        this._workspacesDisplay.prepareToLeaveOverview();
        if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
            Main.overview.fadeInDesktop();

        this._adjustment.ease(ControlsState.HIDDEN, {
            duration: SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => {
                this.dash.showAppsButton.checked = false;
                this._animating = false;
            },
            onComplete,
        });
    }

    get searchEntry() {
        return this._searchEntry;
    }

    gestureBegin(tracker) {
        const baseDistance = global.screen_height;
        const progress = this._adjustment.value;
        const points = [
            ControlsState.HIDDEN,
            ControlsState.WINDOW_PICKER,
            ControlsState.APP_GRID,
        ];

        const transition = this._adjustment.get_transition('value');
        const cancelProgress = transition
            ? transition.get_interval().peek_final_value()
            : Math.round(progress);

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
        this._searchController.prepareToEnterOverview();
        this._workspacesDisplay.prepareToEnterOverview();
        if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
            Main.overview.fadeInDesktop();
    }

    gestureProgress(progress) {
        this._adjustment.value = progress;
    }

    gestureEnd(target, duration, onComplete) {
        this._animating = true;

        if (target === ControlsState.HIDDEN) {
            this._searchController.reset();
            this._workspacesDisplay.prepareToLeaveOverview();
            if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
                Main.overview.fadeInDesktop();
        }

        this._adjustment.ease(target, {
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete,
        });

        this.dash.showAppsButton.checked =
            target === ControlsState.APP_GRID;
        this._animating = false;
    }

    get appDisplay() {
        return this._appDisplay;
    }
});
