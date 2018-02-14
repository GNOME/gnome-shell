// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ViewSelector */

const { EosMetrics, Clutter, Gio,
        GLib, GObject, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppDisplay = imports.ui.appDisplay;
const DiscoveryFeedButton = imports.ui.discoveryFeedButton;
const LayoutManager = imports.ui.layout;
const Main = imports.ui.main;
const Monitor = imports.ui.monitor;
const OverviewControls = imports.ui.overviewControls;
const Params = imports.misc.params;
const Search = imports.ui.search;
const ShellEntry = imports.ui.shellEntry;
const WorkspacesView = imports.ui.workspacesView;
const EdgeDragAction = imports.ui.edgeDragAction;
const IconGrid = imports.ui.iconGrid;

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';
var PINCH_GESTURE_THRESHOLD = 0.7;

const SEARCH_ACTIVATION_TIMEOUT = 50;

const SEARCH_METRIC_INACTIVITY_TIMEOUT_SECONDS = 3;

// Occurs when a user initiates a search from the desktop. The payload, with
// type `(us)`, consists of an enum value from the DesktopSearchProvider enum
// telling what kind of search was requested; followed by the search query.
const EVENT_DESKTOP_SEARCH = 'b02266bc-b010-44b2-ae0f-8f116ffa50eb';

// Represents the various search providers that can be used for searching from
// the desktop. Keep in sync with the corresponding enum in
// https://github.com/endlessm/eos-analytics/tree/master/src/main/java/com/endlessm/postprocessing/query/SearchQuery.java.
const DesktopSearchProvider = {
    MY_COMPUTER: 0,
};

var ViewPage = {
    WINDOWS: 1,
    APPS: 2
};

const ViewsDisplayPage = {
    APP_GRID: 1,
    SEARCH: 2
};

var FocusTrap = GObject.registerClass(
class FocusTrap extends St.Widget {
    vfunc_navigate_focus(from, direction) {
        if (direction == St.DirectionType.TAB_FORWARD ||
            direction == St.DirectionType.TAB_BACKWARD)
            return super.vfunc_navigate_focus(from, direction);
        return false;
    }
});

function getTermsForSearchString(searchString) {
    searchString = searchString.replace(/^\s+/g, '').replace(/\s+$/g, '');
    if (searchString == '')
        return [];

    let terms = searchString.split(/\s+/);
    return terms;
}

var TouchpadShowOverviewAction = class {
    constructor(actor) {
        actor.connect('captured-event', this._handleEvent.bind(this));
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_PINCH)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 3)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END)
            this.emit('activated', event.get_gesture_pinch_scale ());

        return Clutter.EVENT_STOP;
    }
};
Signals.addSignalMethods(TouchpadShowOverviewAction.prototype);

var ShowOverviewAction = GObject.registerClass({
    Signals: { 'activated': { param_types: [GObject.TYPE_DOUBLE] } },
}, class ShowOverviewAction extends Clutter.GestureAction {
    _init() {
        super._init();
        this.set_n_touch_points(3);

        global.display.connect('grab-op-begin', () => {
            this.cancel();
        });
    }

    vfunc_gesture_prepare(_actor) {
        return Main.actionMode == Shell.ActionMode.NORMAL &&
               this.get_n_current_points() == this.get_n_touch_points();
    }

    _getBoundingRect(motion) {
        let minX, minY, maxX, maxY;

        for (let i = 0; i < this.get_n_current_points(); i++) {
            let x, y;

            if (motion == true) {
                [x, y] = this.get_motion_coords(i);
            } else {
                [x, y] = this.get_press_coords(i);
            }

            if (i == 0) {
                minX = maxX = x;
                minY = maxY = y;
            } else {
                minX = Math.min(minX, x);
                minY = Math.min(minY, y);
                maxX = Math.max(maxX, x);
                maxY = Math.max(maxY, y);
            }
        }

        return new Meta.Rectangle({ x: minX,
                                    y: minY,
                                    width: maxX - minX,
                                    height: maxY - minY });
    }

    vfunc_gesture_begin(_actor) {
        this._initialRect = this._getBoundingRect(false);
        return true;
    }

    vfunc_gesture_end(_actor) {
        let rect = this._getBoundingRect(true);
        let oldArea = this._initialRect.width * this._initialRect.height;
        let newArea = rect.width * rect.height;
        let areaDiff = newArea / oldArea;

        this.emit('activated', areaDiff);
    }
});

var ViewsDisplayLayout = GObject.registerClass({
    Signals: {
        'grid-available-size-changed': {
            param_types: [GObject.TYPE_INT, GObject.TYPE_INT]
        },
    },
    Properties: {
        'expansion': GObject.ParamSpec.double(
            'expansion',
            'expansion',
            'expansion',
            GObject.ParamFlags.READWRITE,
            0, 1, 0)
    },
}, class ViewsDisplayLayout extends Clutter.BinLayout {
    _init(entry, discoveryFeedButton, gridContainerActor, searchResultsActor) {
        super._init();

        this._entry = entry;
        this._discoveryFeedButton = discoveryFeedButton;
        this._gridContainerActor = gridContainerActor;
        this._searchResultsActor = searchResultsActor;

        this._entry.connect('style-changed', this._onStyleChanged.bind(this));
        this._gridContainerActor.connect('style-changed', this._onStyleChanged.bind(this));

        this._heightAboveEntry = 0;
        this.expansion = 0;
        this._lowResolutionMode = false;
    }

    _onStyleChanged() {
        this.layout_changed();
    }

    _centeredHeightAbove(height, availHeight) {
        return Math.max(0, Math.floor((availHeight - height) / 2));
    }

    _computeGridContainerPlacement(viewHeight, entryHeight, availHeight) {
        // If we have the space for it, we add some padding to the top of the
        // all view when calculating its centered position. This is to offset
        // the icon labels at the bottom of the icon grid, so the icons
        // themselves appears centered.
        let themeNode = this._gridContainerActor.get_theme_node();
        let topPadding = themeNode.get_length('-natural-padding-top');
        let heightAbove = this._centeredHeightAbove(viewHeight + topPadding, availHeight);
        let leftover = Math.max(availHeight - viewHeight - heightAbove, 0);
        heightAbove += Math.min(topPadding, leftover);
        // Always leave enough room for the search entry at the top
        heightAbove = Math.max(entryHeight, heightAbove);
        return heightAbove;
    }

    _computeChildrenAllocation(allocation) {
        let availWidth = allocation.x2 - allocation.x1;
        let availHeight = allocation.y2 - allocation.y1;

        // Entry height
        let entryHeight = this._entry.get_preferred_height(availWidth)[1];
        let themeNode = this._entry.get_theme_node();
        let entryMinPadding = themeNode.get_length('-minimum-vpadding');
        let entryTopMargin = themeNode.get_length('margin-top');
        entryHeight += entryMinPadding * 2;

        // GridContainer height
        let gridContainerHeight = this._gridContainerActor.get_preferred_height(availWidth)[1];
        let heightAboveGrid = this._computeGridContainerPlacement(gridContainerHeight, entryHeight, availHeight);
        this._heightAboveEntry = this._centeredHeightAbove(entryHeight, heightAboveGrid);

        let entryBox = allocation.copy();
        entryBox.y1 = this._heightAboveEntry + entryTopMargin;
        entryBox.y2 = entryBox.y1 + entryHeight;

        let discoveryFeedButtonBox = allocation.copy();
        if (this._discoveryFeedButton)
            discoveryFeedButtonBox = DiscoveryFeedButton.determineAllocationWithinBox(this._discoveryFeedButton,
                                                                                      allocation,
                                                                                      availWidth);

        let gridContainerBox = allocation.copy();
        // The grid container box should have the dimensions of this container but start
        // after the search entry and according to the calculated xplacement policies
        gridContainerBox.y1 = this._computeGridContainerPlacement(gridContainerHeight, entryHeight, availHeight);

        let searchResultsBox = allocation.copy();

        // The views clone does not have a searchResultsActor
        if (this._searchResultsActor) {
            let searchResultsHeight = availHeight - entryHeight;
            searchResultsBox.x1 = allocation.x1;
            searchResultsBox.x2 = allocation.x2;
            searchResultsBox.y1 = entryBox.y2;
            searchResultsBox.y2 = searchResultsBox.y1 + searchResultsHeight;
        }

        return [entryBox, discoveryFeedButtonBox, gridContainerBox, searchResultsBox];
    }

    vfunc_allocate(actor, box, flags) {
        let [entryBox, discoveryFeedButtonBox, gridContainerBox, searchResultsBox] =
            this._computeChildrenAllocation(box);

        // We want to emit the signal BEFORE any allocation has happened since the
        // icon grid will need to precompute certain values before being able to
        // report a sensible preferred height for the specified width.

        this.emit('grid-available-size-changed',
                  gridContainerBox.x2 - gridContainerBox.x1,
                  gridContainerBox.y2 - gridContainerBox.y1);

        this._entry.allocate(entryBox, flags);
        if (this._discoveryFeedButton)
            this._discoveryFeedButton.allocate(discoveryFeedButtonBox, flags);
        this._gridContainerActor.allocate(gridContainerBox, flags);
        if (this._searchResultsActor)
            this._searchResultsActor.allocate(searchResultsBox, flags);
    }

    set expansion(v) {
        if (v == this._expansion || this._searchResultsActor == null)
            return;

        this._gridContainerActor.visible = v != 1;
        this._searchResultsActor.visible = v != 0;

        this._gridContainerActor.opacity = (1 - v) * 255;
        this._searchResultsActor.opacity = v * 255;

        if (this._discoveryFeedButton) {
            this._discoveryFeedButton.changeVisbilityState(v != 1);
            this._discoveryFeedButton.opacity = (1 - v) * 255;
        }

        let entryTranslation = - this._heightAboveEntry * v;
        this._entry.translation_y = entryTranslation;

        this._searchResultsActor.translation_y = entryTranslation;

        this._expansion = v;
        this.notify('expansion')
    }

    get expansion() {
        return this._expansion;
    }
});

var ViewsDisplayContainer = GObject.registerClass(
class ViewsDisplayContainer extends St.Widget {
    _init(entry, discoveryFeedButton, gridContainer, searchResults) {
        this._entry = entry;
        this._discoveryFeedButton = discoveryFeedButton;
        this._gridContainer = gridContainer;
        this._searchResults = searchResults;

        this._activePage = ViewsDisplayPage.APP_GRID;

        let layoutManager = new ViewsDisplayLayout(
            entry,
            discoveryFeedButton,
            gridContainer.actor,
            searchResults.actor);
        super._init({
            layout_manager: layoutManager,
            x_expand: true,
            y_expand: true,
        });

        layoutManager.connect('grid-available-size-changed', this._onGridAvailableSizeChanged.bind(this));

        this.add_child(this._entry);
        if (this._discoveryFeedButton)
            this.add_child(this._discoveryFeedButton);
        this.add_child(this._gridContainer.actor);
        this.add_child(this._searchResults.actor);
    }

    _onGridAvailableSizeChanged(actor, width, height) {
        let box = new Clutter.ActorBox();
        box.x1 = box.y1 = 0;
        box.x2 = width;
        box.y2 = height;
        box = this._gridContainer.actor.get_theme_node().get_content_box(box);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        this._gridContainer.adaptToSize(availWidth, availHeight);
    }

    showPage(page, doAnimation) {
        if (this._activePage === page)
            return;

        this._activePage = page;

        let tweenTarget = page == ViewsDisplayPage.SEARCH ? 1 : 0;
        if (doAnimation) {
            this._searchResults.isAnimating = true;
            this.ease_property('@layout.expansion', tweenTarget, {
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._searchResults.isAnimating = false,
            });
        } else {
            this.layout_manager.expansion = tweenTarget;
        }
    }

    getActivePage() {
        return this._activePage;
    }
});

var ViewsDisplay = class {
    constructor() {
        this._enterSearchTimeoutId = 0;
        this._localSearchMetricTimeoutId = 0;

        this._appDisplay = new AppDisplay.AppDisplay()

        this._searchResults = new Search.SearchResults();
        this._searchResults.connect('search-progress-updated', this._updateSpinner.bind(this));

        // Since the entry isn't inside the results container we install this
        // dummy widget as the last results container child so that we can
        // include the entry in the keynav tab path
        this._focusTrap = new FocusTrap({ can_focus: true });
        this._focusTrap.connect('key-focus-in', () => {
            this.entry.grab_key_focus();
        });
        this._searchResults.actor.add_actor(this._focusTrap);

        global.focus_manager.add_group(this._searchResults.actor);

        this.entry = new ShellEntry.OverviewEntry();
        this.entry.connect('search-activated', this._onSearchActivated.bind(this));
        this.entry.connect('search-active-changed', this._onSearchActiveChanged.bind(this));
        this.entry.connect('search-navigate-focus', this._onSearchNavigateFocus.bind(this));
        this.entry.connect('search-terms-changed', this._onSearchTermsChanged.bind(this));

        this.entry.clutter_text.connect('key-focus-in', () => {
            this._searchResults.highlightDefault(true);
        });
        this.entry.clutter_text.connect('key-focus-out', () => {
            this._searchResults.highlightDefault(false);
        });

        // Clicking on any empty area should exit search and get back to the desktop.
        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', this._resetSearch.bind(this));
        Main.overview.addAction(clickAction, false);
        this._searchResults.actor.bind_property('mapped', clickAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);

        this.actor = new ViewsDisplayContainer(this.entry,
                                               DiscoveryFeedButton.maybeCreateButton(),
                                               this._appDisplay,
                                               this._searchResults);
    }

    _recordDesktopSearchMetric(query, searchProvider) {
        let eventRecorder = EosMetrics.EventRecorder.get_default();
        let auxiliaryPayload = new GLib.Variant('(us)', [searchProvider, query]);
        eventRecorder.record_event(EVENT_DESKTOP_SEARCH, auxiliaryPayload);
    }

    _updateSpinner() {
        this.entry.setSpinning(this._searchResults.searchInProgress);
    }

    _enterSearch() {
        if (this._enterSearchTimeoutId > 0)
            return;

        // We give a very short time for search results to populate before
        // triggering the animation, unless an animation is already in progress
        if (this._searchResults.isAnimating) {
            this.actor.showPage(ViewsDisplayPage.SEARCH, true);
            return;
        }

        this._enterSearchTimeoutId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            SEARCH_ACTIVATION_TIMEOUT, () => {
                this._enterSearchTimeoutId = 0;
                this.actor.showPage(ViewsDisplayPage.SEARCH, true);

                return GLib.SOURCE_REMOVE;
            }
        );
    }

    _leaveSearch() {
        if (this._enterSearchTimeoutId > 0) {
            GLib.source_remove(this._enterSearchTimeoutId);
            this._enterSearchTimeoutId = 0;
        }
        this.actor.showPage(ViewsDisplayPage.APP_GRID, true);
    }

    _onSearchActivated() {
        this._searchResults.activateDefault();
        this._resetSearch();
    }

    _onSearchActiveChanged() {
        if (this.entry.active)
            this._enterSearch();
        else
            this._leaveSearch();
    }

    _onSearchNavigateFocus(entry, direction) {
        this._searchResults.navigateFocus(direction);
    }

    _onSearchTermsChanged() {
        let terms = this.entry.getSearchTerms();
        this._searchResults.setTerms(terms);

        // Since the search is live, only record a metric a few seconds after
        // the user has stopped typing. Don't record one if the user deleted
        // what they wrote and left it at that.
        if (this._localSearchMetricTimeoutId > 0)
            GLib.source_remove(this._localSearchMetricTimeoutId);

        this._localSearchMetricTimeoutId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            SEARCH_METRIC_INACTIVITY_TIMEOUT_SECONDS,
            () => {
                let query = terms.join(' ');
                if (query !== '')
                    this._recordDesktopSearchMetric(query,
                        DesktopSearchProvider.MY_COMPUTER);
                this._localSearchMetricTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            }
        );
    }

    _resetSearch() {
        this.entry.resetSearch();
    }

    get appDisplay() {
        return this._appDisplay;
    }

    get activeViewsPage() {
        return this.actor.getActivePage();
    }
};

var ViewsClone = GObject.registerClass(
class ViewsClone extends St.Widget {
    _init(viewSelector, viewsDisplay, forOverview) {
        this._viewSelector = viewSelector;
        this._viewsDisplay = viewsDisplay;
        this._forOverview = forOverview;

        let appDisplay = this._viewsDisplay.appDisplay;
        let entry = new ShellEntry.OverviewEntry();
        entry.reactive = false;
        entry.clutter_text.reactive = false;

        let iconGridClone = new Clutter.Clone({
            source: appDisplay.gridActor,
            x_expand: true,
            y_expand: true,
            reactive: false,
        });

        let appGridContainer =
            new AppDisplay.AllViewContainer(iconGridClone, {
                allowScrolling: false
        });
        appGridContainer.reactive = false;

        let discoveryFeedButton = DiscoveryFeedButton.maybeCreateInactiveButton();
        let layoutManager = new ViewsDisplayLayout(
            entry,
            discoveryFeedButton,
            appGridContainer,
            null
        );
        super._init({
            layout_manager: layoutManager,
            x_expand: true,
            y_expand: true,
            reactive: false,
            opacity: AppDisplay.EOS_ACTIVE_GRID_OPACITY,
        });

        // Ensure the cloned grid is scrolled to the same page as the original one
        let originalGridContainer = appDisplay.gridContainer;
        let originalAdjustment = originalGridContainer.scrollView.vscroll.adjustment;
        let cloneAdjustment = appGridContainer.scrollView.vscroll.adjustment;
        originalAdjustment.bind_property('value', cloneAdjustment, 'value', GObject.BindingFlags.SYNC_CREATE);

        if (discoveryFeedButton)
            this.add_child(discoveryFeedButton);
        this.add_child(entry);
        this.add_child(appGridContainer);

        this._saturation = new Clutter.DesaturateEffect({
            name: 'saturation',
            factor: AppDisplay.EOS_INACTIVE_GRID_SATURATION,
            enabled: false,
        });
        this.add_effect(this._saturation);

        let workareaConstraint = new Monitor.MonitorConstraint({ primary: true,
                                                                 work_area: true });
        this.add_constraint(workareaConstraint);

        Main.overview.connect('showing', () => {
            this.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
            this._saturation.factor = AppDisplay.EOS_INACTIVE_GRID_SATURATION;
            this._saturation.enabled = this._forOverview;
        });
        Main.overview.connect('hidden', () => {
            this.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
            this._saturation.factor = AppDisplay.EOS_INACTIVE_GRID_SATURATION;
            this._saturation.enabled = !this._forOverview;

            // When we're hidden and coming from the apps page, tween out the
            // clone saturation and opacity in the background as an override
            if (!this._forOverview &&
                this._viewSelector.getActivePage() == ViewPage.APPS) {
                this.opacity = AppDisplay.EOS_ACTIVE_GRID_OPACITY;
                this.saturation = AppDisplay.EOS_ACTIVE_GRID_SATURATION;
                this.ease({
                    opacity: AppDisplay.EOS_INACTIVE_GRID_OPACITY,
                    duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });

                this.ease_property(
                    '@effects.saturation.factor',
                    AppDisplay.EOS_INACTIVE_GRID_SATURATION, {
                        duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    }
                );
            }
        });

        let settings = Clutter.Settings.get_default();
        settings.connect('notify::font-dpi', () => {
            let overviewVisible = Main.layoutManager.overviewGroup.visible;
            let saturationEnabled = this._saturation.enabled;

            // Maybe because of the already known issue with FBO and ClutterClones,
            // simply redrawing the overview group without assuring it is visible
            // won't work. Clutter was supposed to do that, but it doesn't. The
            // FBO, in this case, is introduced through the saturation effect.
            this._saturation.enabled = false;
            Main.layoutManager.overviewGroup.visible = true;

            Main.layoutManager.overviewGroup.queue_redraw();

            // Restore the previous states
            Main.layoutManager.overviewGroup.visible = overviewVisible;
            this._saturation.enabled = saturationEnabled;
        });
    }

    set saturation(factor) {
        this._saturation.factor = factor;
    }

    get saturation() {
        return this._saturation.factor;
    }
});

var ViewsDisplayConstraint = GObject.registerClass(
class ViewsDisplayConstraint extends Monitor.MonitorConstraint {
    vfunc_update_allocation(actor, actorBox) {
        let originalBox = actorBox.copy();
        super.vfunc_update_allocation(actor, actorBox);

        actorBox.init_rect(originalBox.get_x(), originalBox.get_y(),
                           actorBox.get_width(), originalBox.get_height());
    }
});

var ViewSelector = class {
    constructor() {
        this.actor = new Shell.Stack({ name: 'viewSelector' });

        this._activePage = null;

        this._workspacesDisplay = new WorkspacesView.WorkspacesDisplay();
        this._workspacesDisplay.connect('empty-space-clicked', this._onEmptySpaceClicked.bind(this));
        this._workspacesPage = this._addPage(this._workspacesDisplay.actor,
                                             _("Windows"), 'focus-windows-symbolic');

        this._viewsDisplay = new ViewsDisplay();
        this._appsPage = this._addPage(this._viewsDisplay.actor,
                                       _("Applications"), 'view-grid-symbolic');
        this._appsPage.add_constraint(new ViewsDisplayConstraint({ primary: true,
                                                                   work_area: true }));

        this.appDisplay = this._viewsDisplay.appDisplay;
        this._entry = this._viewsDisplay.entry;

        this._stageKeyPressId = 0;

        this._addViewsPageClone();

        Main.overview.connect('showing', () => {
            this._stageKeyPressId = global.stage.connect('key-press-event',
                                                         this._onStageKeyPress.bind(this));
        });
        Main.overview.connect('hiding', () => {
            if (this._stageKeyPressId != 0) {
                global.stage.disconnect(this._stageKeyPressId);
                this._stageKeyPressId = 0;
            }
        });
        Main.overview.connect('shown', () => {
            // If we were animating from the desktop view to the
            // apps page the workspace page was visible, allowing
            // the windows to animate, but now we no longer want to
            // show it given that we are now on the apps page or
            // search page.
            if (this._activePage != this._workspacesPage) {
                this._workspacesPage.opacity = 0;
                this._workspacesPage.hide();
            }

            // Make sure to hide the overview immediately if we're starting up
            // coming from a previous session with apps running and visible.
            if (Main.layoutManager.startingUp && Main.workspaceMonitor.hasVisibleWindows)
                Main.overview.hide();
        });

        Main.wm.addKeybinding('toggle-application-view',
                              new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                              Shell.ActionMode.NORMAL |
                              Shell.ActionMode.OVERVIEW,
                              Main.overview.toggleApps.bind(this));

        Main.wm.addKeybinding('toggle-overview',
                              new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                              Shell.ActionMode.NORMAL |
                              Shell.ActionMode.OVERVIEW,
                              Main.overview.toggleWindows.bind(Main.overview));

        let side;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            side = St.Side.RIGHT;
        else
            side = St.Side.LEFT;
        let gesture = new EdgeDragAction.EdgeDragAction(side,
                                                        Shell.ActionMode.NORMAL);
        gesture.connect('activated', () => {
            if (Main.overview.visible)
                Main.overview.hide();
            else
                this.showApps();
        });
        global.stage.add_action(gesture);

        gesture = new ShowOverviewAction();
        gesture.connect('activated', this._pinchGestureActivated.bind(this));
        global.stage.add_action(gesture);

        gesture = new TouchpadShowOverviewAction(global.stage);
        gesture.connect('activated', this._pinchGestureActivated.bind(this));
    }

    _pinchGestureActivated(action, scale) {
        if (scale < PINCH_GESTURE_THRESHOLD)
            Main.overview.show();
    }

    _addViewsPageClone() {
        let layoutViewsClone = new ViewsClone(this, this._viewsDisplay, false);
        Main.layoutManager.setViewsClone(layoutViewsClone);

        this._overviewViewsClone = new ViewsClone(this, this._viewsDisplay, true);
        Main.overview.setViewsClone(this._overviewViewsClone);
        this._appsPage.bind_property('visible',
                                     this._overviewViewsClone, 'visible',
                                     GObject.BindingFlags.SYNC_CREATE |
                                     GObject.BindingFlags.INVERT_BOOLEAN);
    }

    _onEmptySpaceClicked() {
        this.setActivePage(ViewPage.APPS);
    }

    showApps() {
        Main.overview.show();
    }

    _clearSearch() {
        this._entry.resetSearch();
        this._viewsDisplay.actor.showPage(ViewsDisplayPage.APP_GRID, false);
    }

    show(viewPage) {
        this._clearSearch();

        // We're always starting up to the APPS page, so avoid making the workspacesDisplay
        // (used for the Windows picker) visible to prevent situations where that actor
        // would intercept clicks meant for the desktop's icons grid.
        if (!Main.layoutManager.startingUp)
            this._workspacesDisplay.show(viewPage == ViewPage.APPS);

        this._showPage(this._pageFromViewPage(viewPage), false);
    }

    animateFromOverview() {
        // Make sure workspace page is fully visible to allow
        // workspace.js do the animation of the windows
        this._workspacesPage.opacity = 255;

        this._workspacesDisplay.animateFromOverview(this._activePage != this._workspacesPage);
    }

    setWorkspacesFullGeometry(geom) {
        this._workspacesDisplay.setWorkspacesFullGeometry(geom);
    }

    hide() {
        this._workspacesDisplay.hide();
    }

    focusSearch() {
        if (this._activePage == this._appsPage)
            this._entry.grab_key_focus();
    }

    _addPage(actor, name, a11yIcon, params) {
        params = Params.parse(params, { a11yFocus: null });

        let page = new St.Bin({ child: actor,
                                x_align: St.Align.START,
                                y_align: St.Align.START,
                                x_fill: true,
                                y_fill: true });
        if (params.a11yFocus)
            Main.ctrlAltTabManager.addGroup(params.a11yFocus, name, a11yIcon);
        else
            Main.ctrlAltTabManager.addGroup(actor, name, a11yIcon, {
                proxy: this.actor,
                focusCallback: () => this._a11yFocusPage(page),
            });
        page.hide();
        this.actor.add_actor(page);
        return page;
    }

    _fadePageIn(oldPage, doFadeAnimation) {
        if (oldPage) {
            oldPage.opacity = 0;
            oldPage.hide();
        }

        this.emit('page-empty');
        this._activePage.show();

        if (doFadeAnimation) {
            this._activePage.ease({
                opacity: 255,
                duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            this._activePage.opacity = 255;
        }
    }

    _pageChanged() {
        if (this._activePage != this._appsPage)
            this._clearSearch();

        this._pageChanged();
    }

    _showPage(page, doFadeAnimation) {
        if (page == this._activePage)
            return;

        let oldPage = this._activePage;
        this._activePage = page;
        this.emit('page-changed');

        if (oldPage && doFadeAnimation) {
            // When fading to the apps page, tween the opacity of the
            // clone instead, and set the apps page to full solid immediately
            if (page == this._appsPage) {
                page.opacity = 255;
                this._overviewViewsClone.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
                this._overviewViewsClone.saturation = AppDisplay.EOS_INACTIVE_GRID_SATURATION;
                this._overviewViewsClone.ease({
                    opacity: AppDisplay.EOS_ACTIVE_GRID_OPACITY,
                    duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => {
                        this._overviewViewsClone.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
                        this._overviewViewsClone.saturation = AppDisplay.EOS_INACTIVE_GRID_SATURATION;
                    }
                });
                this._overviewViewsClone.ease_property(
                    '@effects.saturation.factor',
                    AppDisplay.EOS_ACTIVE_GRID_SATURATION, {
                        duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    }
                );
            }

            // When fading from the apps page, tween the opacity of the
            // clone instead. The code in this._fadePageIn() will hide
            // the actual page immediately
            if (oldPage == this._appsPage) {
                this._overviewViewsClone.opacity = AppDisplay.EOS_ACTIVE_GRID_OPACITY;
                this._overviewViewsClone.saturation = AppDisplay.EOS_ACTIVE_GRID_SATURATION;
                this._overviewViewsClone.ease({
                    opacity: AppDisplay.EOS_INACTIVE_GRID_OPACITY,
                    duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
                this._overviewViewsClone.ease_property(
                    '@effects.saturation.factor',
                    AppDisplay.EOS_INACTIVE_GRID_SATURATION, {
                        duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    }
                );
                this._fadePageIn(oldPage, doFadeAnimation);
            } else {

                oldPage.ease({
                    opacity: 0,
                    duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => this._fadePageIn(oldPage, doFadeAnimation),
                });
            }
        } else {
            this._fadePageIn(oldPage, doFadeAnimation);
        }
    }

    _a11yFocusPage(page) {
        page.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
    }

    _onStageKeyPress(actor, event) {
        // Ignore events while anything but the overview has
        // pushed a modal (system modals, looking glass, ...)
        if (Main.modalCount > 1)
            return Clutter.EVENT_PROPAGATE;

        let symbol = event.get_key_symbol();

        if (symbol == Clutter.Escape && this._activePage == this._workspacesPage) {
            Main.overview.toggleWindows();
            return Clutter.EVENT_STOP;
        }

        if (this._entry.handleStageEvent(event))
            return Clutter.EVENT_STOP;

        if (this._entry.active)
            return Clutter.EVENT_PROPAGATE;

        if (!global.stage.key_focus) {
            if (symbol == Clutter.Tab || symbol == Clutter.Down) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
                return Clutter.EVENT_STOP;
            } else if (symbol == Clutter.ISO_Left_Tab) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_BACKWARD, false);
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _pageFromViewPage(viewPage) {
        let page;

        if (viewPage == ViewPage.WINDOWS)
            page = this._workspacesPage;
        else
            page = this._appsPage;

        return page;
    }

    getActivePage() {
        if (this._activePage == this._workspacesPage)
            return ViewPage.WINDOWS;
        else
            return ViewPage.APPS;
    }

    setActivePage(viewPage) {
        this._showPage(this._pageFromViewPage(viewPage), true);
    }

    fadeIn() {
        this._activePage.ease({
            opacity: 255,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    fadeHalf() {
        this._activePage.ease({
            opacity: 128,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME / 2,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }
};
Signals.addSignalMethods(ViewSelector.prototype);
