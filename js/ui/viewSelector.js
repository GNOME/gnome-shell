// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ViewSelector, ViewsClone */

const { EosMetrics, Clutter, Gio,
        GLib, GObject, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppDisplay = imports.ui.appDisplay;
const DiscoveryFeedButton = imports.ui.discoveryFeedButton;
const LayoutManager = imports.ui.layout;
const Main = imports.ui.main;
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
    APPS: 2,
};

const ViewsDisplayPage = {
    APP_GRID: 1,
    SEARCH: 2,
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
        actor.connect('captured-event::touchpad', this._handleEvent.bind(this));
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_PINCH)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 3)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END)
            this.emit('activated', event.get_gesture_pinch_scale());

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

            if (motion == true)
                [x, y] = this.get_motion_coords(i);
            else
                [x, y] = this.get_press_coords(i);

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
    Properties: {
        'expansion': GObject.ParamSpec.float(
            'expansion',
            'expansion',
            'expansion',
            GObject.ParamFlags.READWRITE,
            0, 1, 0),
    },
}, class ViewsDisplayLayout extends Clutter.BoxLayout {
    _init(entry, discoveryFeedButton, appDisplay, searchResultsActor) {
        super._init();

        this._entry = entry;
        this._discoveryFeedButton = discoveryFeedButton;
        this._appDisplay = appDisplay;
        this._searchResultsActor = searchResultsActor;

        this._entry.connect('style-changed', this._onStyleChanged.bind(this));
        this._appDisplay.connect('style-changed', this._onStyleChanged.bind(this));

        this._heightAboveEntry = 0;
        this.expansion = 0;
    }

    _onStyleChanged() {
        this.layout_changed();
    }

    _centeredHeightAbove(height, availHeight) {
        return Math.max(0, Math.floor((availHeight - height) / 2));
    }

    _computeAppDisplayPlacement(viewHeight, entryHeight, availHeight) {
        // If we have the space for it, we add some padding to the top of the
        // all view when calculating its centered position. This is to offset
        // the icon labels at the bottom of the icon grid, so the icons
        // themselves appears centered.
        let themeNode = this._appDisplay.get_theme_node();
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

        // AppDisplay height
        // Step 1: pre pre-allocate the grid with the maximum available size
        this._appDisplay.adaptToSize(availWidth, availHeight - entryHeight);

        // Use the maximum preferred size for now
        let appDisplayHeight = this._appDisplay.get_preferred_height(availWidth)[1];

        let heightAboveGrid = this._computeAppDisplayPlacement(appDisplayHeight, entryHeight, availHeight);
        this._heightAboveEntry = this._centeredHeightAbove(entryHeight, heightAboveGrid);

        let entryBox = allocation.copy();
        entryBox.y1 = this._heightAboveEntry + entryTopMargin;
        entryBox.y2 = entryBox.y1 + entryHeight;

        let discoveryFeedButtonBox = allocation.copy();
        if (this._discoveryFeedButton) {
            discoveryFeedButtonBox =
                DiscoveryFeedButton.determineAllocationWithinBox(
                    this._discoveryFeedButton, allocation, availWidth);
        }

        let appDisplayBox = allocation.copy();
        // The grid container box should have the dimensions of this container but start
        // after the search entry and according to the calculated xplacement policies
        appDisplayBox.y1 = heightAboveGrid;

        let searchResultsBox = allocation.copy();

        // The views clone does not have a searchResultsActor
        if (this._searchResultsActor) {
            let searchResultsHeight = availHeight - entryHeight;
            searchResultsBox.x1 = allocation.x1;
            searchResultsBox.x2 = allocation.x2;
            searchResultsBox.y1 = entryBox.y2;
            searchResultsBox.y2 = searchResultsBox.y1 + searchResultsHeight;
        }

        // Step 2: pre-allocate to a smaller, but realistic, size
        this._appDisplay.adaptToSize(availWidth, appDisplayBox.get_height());

        return [entryBox, discoveryFeedButtonBox, appDisplayBox, searchResultsBox];
    }

    vfunc_allocate(actor, box, flags) {
        let [entryBox, discoveryFeedButtonBox, appDisplayBox, searchResultsBox] =
            this._computeChildrenAllocation(box);

        this._entry.allocate(entryBox, flags);

        if (this._discoveryFeedButton)
            this._discoveryFeedButton.allocate(discoveryFeedButtonBox, flags);

        // Step 3: actually allocate the grid
        this._appDisplay.allocate(appDisplayBox, flags);

        if (this._searchResultsActor)
            this._searchResultsActor.allocate(searchResultsBox, flags);
    }

    set expansion(v) {
        if (v === this._expansion || !this._searchResultsActor)
            return;

        this._appDisplay.visible = v !== 1;
        this._searchResultsActor.visible = v !== 0;

        this._appDisplay.opacity = (1 - v) * 255;
        this._searchResultsActor.opacity = v * 255;

        if (this._discoveryFeedButton) {
            this._discoveryFeedButton.changeVisbilityState(v !== 1);
            this._discoveryFeedButton.opacity = (1 - v) * 255;
        }

        let entryTranslation = -this._heightAboveEntry * v;
        this._entry.translation_y = entryTranslation;

        this._searchResultsActor.translation_y = entryTranslation;

        this._expansion = v;
        this.notify('expansion');
    }

    get expansion() {
        return this._expansion;
    }
});

var ViewsDisplayConstraint = GObject.registerClass(
class ViewsDisplayConstraint extends LayoutManager.MonitorConstraint {
    vfunc_update_allocation(actor, actorBox) {
        let originalBox = actorBox.copy();
        super.vfunc_update_allocation(actor, actorBox);

        actorBox.init_rect(
            originalBox.get_x(), originalBox.get_y(),
            actorBox.get_width(), originalBox.get_height());
    }
});

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

        const appDisplayClone = new AppDisplay.AppDisplay();
        appDisplayClone._eventBlocker.visible = true;

        let discoveryFeedButton = DiscoveryFeedButton.maybeCreateInactiveButton();
        let layoutManager = new ViewsDisplayLayout(entry, discoveryFeedButton, appDisplayClone, null);
        super._init({
            layout_manager: layoutManager,
            x_expand: true,
            y_expand: true,
            reactive: false,
            visible: false,
            opacity: 0,
        });

        Shell.util_set_hidden_from_pick(this, true);

        // Ensure the cloned grid is scrolled to the same page as the original one
        let originalAdjustment = appDisplay.scrollView.vscroll.adjustment;
        let cloneAdjustment = appDisplayClone.scrollView.vscroll.adjustment;
        originalAdjustment.bind_property('value', cloneAdjustment, 'value', GObject.BindingFlags.SYNC_CREATE);

        if (discoveryFeedButton)
            this.add_child(discoveryFeedButton);
        this.add_child(entry);
        this.add_child(appDisplayClone);

        this._saturation = new Clutter.DesaturateEffect({
            name: 'saturation',
            factor: AppDisplay.EOS_INACTIVE_GRID_SATURATION,
        });
        this.add_effect(this._saturation);

        let workareaConstraint = new LayoutManager.MonitorConstraint({
            primary: true,
            work_area: true,
        });
        this.add_constraint(workareaConstraint);

        Main.overview.connect('showing', () => {
            this.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
            this._saturation.factor = AppDisplay.EOS_INACTIVE_GRID_SATURATION;
        });
        Main.overview.connect('hidden', () => {
            this.opacity = AppDisplay.EOS_INACTIVE_GRID_OPACITY;
            this._saturation.factor = AppDisplay.EOS_INACTIVE_GRID_SATURATION;

            // When we're hidden and coming from the apps page, tween out the
            // clone saturation and opacity in the background as an override
            if (!this._forOverview &&
                this._viewSelector.getActivePage() === ViewPage.APPS) {
                this.show();
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
                    });

                GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                    appDisplayClone.gridActor.queue_relayout();
                    return GLib.SOURCE_REMOVE;
                });
            }
        });
    }

    set saturation(factor) {
        this._saturation.factor = factor;
    }

    get saturation() {
        return this._saturation.factor;
    }
});
var ViewsDisplay = GObject.registerClass(
class ViewsDisplay extends St.Widget {
    _init() {
        this._enterSearchTimeoutId = 0;
        this._localSearchMetricTimeoutId = 0;
        this._activePage = ViewsDisplayPage.APP_GRID;

        this._appDisplay = new AppDisplay.AppDisplay();

        this._searchResults = new Search.SearchResultsView();
        this._searchResults.connect('search-progress-updated', this._updateSpinner.bind(this));
        this._searchResults.connect('search-close-clicked', this._resetSearch.bind(this));

        // Since the entry isn't inside the results container we install this
        // dummy widget as the last results container child so that we can
        // include the entry in the keynav tab path
        this._focusTrap = new FocusTrap({ can_focus: true });
        this._focusTrap.connect('key-focus-in', () => {
            this._entry.grab_key_focus();
        });
        this._searchResults.add_actor(this._focusTrap);

        global.focus_manager.add_group(this._searchResults);

        this._entry = new ShellEntry.OverviewEntry();
        this._entry.connect('search-activated', this._onSearchActivated.bind(this));
        this._entry.connect('search-active-changed', this._onSearchActiveChanged.bind(this));
        this._entry.connect('search-navigate-focus', this._onSearchNavigateFocus.bind(this));
        this._entry.connect('search-terms-changed', this._onSearchTermsChanged.bind(this));

        this._entry.clutter_text.connect('key-focus-in', () => {
            this._searchResults.highlightDefault(true);
        });
        this._entry.clutter_text.connect('key-focus-out', () => {
            this._searchResults.highlightDefault(false);
        });

        // Clicking on any empty area should exit search and get back to the desktop.
        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', this._resetSearch.bind(this));
        Main.overview.addAction(clickAction, false);
        this._searchResults.bind_property('mapped', clickAction,
            'enabled', GObject.BindingFlags.SYNC_CREATE);

        let discoveryFeedButton = DiscoveryFeedButton.maybeCreateButton();

        super._init({
            layout_manager: new ViewsDisplayLayout(
                this._entry,
                discoveryFeedButton,
                this._appDisplay,
                this._searchResults),
            x_expand: true,
            y_expand: true,
        });

        this.add_child(this._entry);
        if (discoveryFeedButton)
            this.add_child(discoveryFeedButton);
        this.add_actor(this._appDisplay);
        this.add_child(this._searchResults);
    }

    showPage(page, doAnimation) {
        if (this._activePage === page)
            return;

        this._activePage = page;

        let tweenTarget = page === ViewsDisplayPage.SEARCH ? 1 : 0;
        if (doAnimation) {
            this._searchResults.isAnimating = true;
            this.ease_property('@layout.expansion', tweenTarget, {
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._searchResults.isAnimating = false;
                },
            });
        } else {
            this.layout_manager.expansion = tweenTarget;
        }
    }

    _recordDesktopSearchMetric(query, searchProvider) {
        let eventRecorder = EosMetrics.EventRecorder.get_default();
        let auxiliaryPayload = new GLib.Variant('(us)', [searchProvider, query]);
        eventRecorder.record_event(EVENT_DESKTOP_SEARCH, auxiliaryPayload);
    }

    _updateSpinner() {
        // Make sure we never set the spinner on when there's nothing to search,
        // regardless of the reported current state, as it can be out of date.
        let searchTerms = this.entry.text.trim();
        let spinning = (searchTerms.length > 0) && this._searchResults.searchInProgress;
        this._entry.setSpinning(spinning);
    }

    _enterSearch() {
        if (this._enterSearchTimeoutId > 0)
            return;

        // We give a very short time for search results to populate before
        // triggering the animation, unless an animation is already in progress
        if (this._searchResults.isAnimating) {
            this.showPage(ViewsDisplayPage.SEARCH, true);
            return;
        }

        this._enterSearchTimeoutId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            SEARCH_ACTIVATION_TIMEOUT, () => {
                this._enterSearchTimeoutId = 0;
                this.showPage(ViewsDisplayPage.SEARCH, true);

                return GLib.SOURCE_REMOVE;
            });
    }

    _leaveSearch() {
        if (this._enterSearchTimeoutId > 0) {
            GLib.source_remove(this._enterSearchTimeoutId);
            this._enterSearchTimeoutId = 0;
        }
        this.showPage(ViewsDisplayPage.APP_GRID, true);
    }

    _onSearchActivated() {
        this._searchResults.activateDefault();
        this._resetSearch();
    }

    _onSearchActiveChanged() {
        if (this._entry.active)
            this._enterSearch();
        else
            this._leaveSearch();
    }

    _onSearchNavigateFocus(entry, direction) {
        this._searchResults.navigateFocus(direction);
    }

    _onSearchTermsChanged() {
        let terms = this._entry.getSearchTerms();
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
                if (query !== '') {
                    this._recordDesktopSearchMetric(query,
                        DesktopSearchProvider.MY_COMPUTER);
                }
                this._localSearchMetricTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });
    }

    _resetSearch() {
        this._entry.resetSearch();
    }

    get entry() {
        return this._entry;
    }

    get appDisplay() {
        return this._appDisplay;
    }

    get activeViewsPage() {
        return this._activePage;
    }
});

var ViewSelector = GObject.registerClass({
    Signals: {
        'page-changed': {},
        'page-empty': {},
    },
}, class ViewSelector extends Shell.Stack {
    _init(workspaceAdjustment) {
        super._init({
            name: 'viewSelector',
            x_expand: true,
        });

        this._activePage = null;

        this._workspacesDisplay =
            new WorkspacesView.WorkspacesDisplay(workspaceAdjustment);
        this._workspacesDisplay.connect('empty-space-clicked',
            this._onEmptySpaceClicked.bind(this));
        this._workspacesPage = this._addPage(this._workspacesDisplay,
                                             _("Windows"), 'focus-windows-symbolic');

        this._viewsDisplay = new ViewsDisplay();
        this._appsPage = this._addPage(this._viewsDisplay,
                                       _("Applications"), 'view-app-grid-symbolic');
        this._appsPage.add_constraint(new ViewsDisplayConstraint({
            primary: true,
            work_area: true,
        }));

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

        let overviewViewsClone = new ViewsClone(this, this._viewsDisplay, true);
        Main.overview.setViewsClone(overviewViewsClone);
        this._appsPage.connect('notify::opacity', () => {
            overviewViewsClone.visible = this._appsPage.opacity != 255;
        });
    }

    _onEmptySpaceClicked() {
        this.setActivePage(ViewPage.APPS);
    }

    showApps() {
        Main.overview.show();
    }

    _clearSearch() {
        this._entry.resetSearch();
        this._viewsDisplay.showPage(ViewsDisplayPage.APP_GRID, false);
    }

    show(viewPage) {
        this._clearSearch();

        // We're always starting up to the APPS page, so avoid making the workspacesDisplay
        // (used for the Windows picker) visible to prevent situations where that actor
        // would intercept clicks meant for the desktop's icons grid.
        if (!Main.layoutManager.startingUp)
            this._workspacesDisplay.show(true);

        this._showPage(this._pageFromViewPage(viewPage));
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
        if (this._activePage === this._appsPage)
            this._entry.grab_key_focus();
    }

    _addPage(actor, name, a11yIcon, params) {
        params = Params.parse(params, { a11yFocus: null });

        let page = new St.Bin({
            child: actor,
            opacity: 0,
        });

        if (params.a11yFocus) {
            Main.ctrlAltTabManager.addGroup(params.a11yFocus, name, a11yIcon);
        } else {
            Main.ctrlAltTabManager.addGroup(actor, name, a11yIcon, {
                proxy: this,
                focusCallback: () => this._a11yFocusPage(page),
            });
        }
        this.add_actor(page);
        return page;
    }

    _fadePageIn() {
        this._activePage.ease({
            opacity: 255,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _fadePageOut(page) {
        let oldPage = page;
        page.ease({
            opacity: 0,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => this._animateIn(oldPage),
        });
    }

    _animateIn(oldPage) {
        if (oldPage)
            oldPage.opacity = 0;

        this.emit('page-empty');

        if (this._activePage == this._appsPage && oldPage == this._workspacesPage) {
            // Restore opacity, in case we animated via _fadePageOut
            this._activePage.opacity = 255;
            this.appDisplay.animate(IconGrid.AnimationDirection.IN);
        } else {
            this._fadePageIn();
        }
    }

    _animateOut(page) {
        let oldPage = page;
        if (page == this._appsPage &&
            this._activePage == this._workspacesPage &&
            !Main.overview.animationInProgress) {
            this.appDisplay.animate(IconGrid.AnimationDirection.OUT, () => {
                this._animateIn(oldPage);
            });
        } else {
            this._fadePageOut(page);
        }
    }

    _pageChanged() {
        if (this._activePage !== this._appsPage)
            this._clearSearch();

        this._pageChanged();
    }

    _showPage(page) {
        if (!Main.overview.visible)
            return;

        if (page == this._activePage)
            return;

        let oldPage = this._activePage;
        this._activePage = page;
        this.emit('page-changed');

        if (oldPage)
            this._animateOut(oldPage);
        else
            this._animateIn();
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

        if (symbol == Clutter.KEY_Escape && this._activePage == this._workspacesPage) {
            Main.overview.toggleWindows();
            return Clutter.EVENT_STOP;
        }

        if (this._entry.handleStageEvent(event))
            return Clutter.EVENT_STOP;

        if (this._entry.active)
            return Clutter.EVENT_PROPAGATE;

        if (!global.stage.key_focus) {
            if (symbol === Clutter.KEY_Tab || symbol === Clutter.KEY_Down) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_ISO_Left_Tab) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_BACKWARD, false);
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _pageFromViewPage(viewPage) {
        let page;

        if (viewPage === ViewPage.WINDOWS)
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
        this._showPage(this._pageFromViewPage(viewPage));
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

    get searchEntry() {
        return this._viewsDisplay.entry;
    }
});
