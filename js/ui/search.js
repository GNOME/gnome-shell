// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppDisplay = imports.ui.appDisplay;
const IconGrid = imports.ui.iconGrid;
const InternetSearch = imports.ui.internetSearch;
const Main = imports.ui.main;
const RemoteSearch = imports.ui.remoteSearch;
const Separator = imports.ui.separator;
const Util = imports.misc.util;

const SEARCH_PROVIDERS_SCHEMA = 'org.gnome.desktop.search-providers';

var MAX_LIST_SEARCH_RESULTS_ROWS = 5;
var MAX_GRID_SEARCH_RESULTS_ROWS = 1;
var MAX_GRID_SEARCH_RESULTS_COLS = 8;

var MaxWidthBox = GObject.registerClass(
class MaxWidthBox extends St.BoxLayout {
    vfunc_allocate(box, flags) {
        let themeNode = this.get_theme_node();
        let maxWidth = themeNode.get_max_width();
        let availWidth = box.x2 - box.x1;
        let adjustedBox = box;

        if (availWidth > maxWidth) {
            let excessWidth = availWidth - maxWidth;
            adjustedBox.x1 += Math.floor(excessWidth / 2);
            adjustedBox.x2 -= Math.floor(excessWidth / 2);
        }

        super.vfunc_allocate(adjustedBox, flags);
    }
});

var SearchResult = class {
    constructor(provider, metaInfo, resultsView) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this._resultsView = resultsView;

        this.actor = new St.Button({ reactive: true,
                                     can_focus: true,
                                     track_hover: true,
                                     x_align: St.Align.START,
                                     y_fill: true });

        this.actor._delegate = this;
        this.actor.connect('clicked', this.activate.bind(this));
    }

    activate() {
        this.emit('activate', this.metaInfo.id);
    }
};
Signals.addSignalMethods(SearchResult.prototype);

var ListDescriptionBox = GObject.registerClass(
class ListDescriptionBox extends St.BoxLayout {
    vfunc_get_preferred_height(forWidth) {
        // This container requests space for the title and description
        // regardless of visibility, but allocates normally to visible actors.
        // This allows us have a constant sized box, but still center the title
        // label when the description is not present.
        let min = 0, nat = 0;
        let children = this.get_children();
        for (let i = 0; i < children.length; i++) {
            let child = children[i];
            let [childMin, childNat] = child.get_preferred_height(forWidth);
            min += childMin;
            nat += childNat;
        }
        return [min, nat];
    }
});

var ListSearchResult = class extends SearchResult {

    constructor(provider, metaInfo, resultsView) {
        super(provider, metaInfo, resultsView);

        this.actor.style_class = 'list-search-result';
        this.actor.x_fill = true;

        let content = new St.BoxLayout({ style_class: 'list-search-result-content',
                                         vertical: false });
        this.actor.set_child(content);

        this._termsChangedId = 0;

        // An icon for, or thumbnail of, content
        let icon = this.metaInfo['createIcon'](this.ICON_SIZE);
        if (icon) {
            content.add(icon);
        }

        let details = new ListDescriptionBox({ vertical: true });
        content.add(details, { x_fill: true,
                               y_fill: false,
                               x_align: St.Align.START,
                               y_align: St.Align.MIDDLE });

        let title = new St.Label({ style_class: 'list-search-result-title',
                                   text: this.metaInfo['name'] })
        details.add(title, { x_fill: false,
                             y_fill: false,
                             x_align: St.Align.START,
                             y_align: St.Align.START });
        this.actor.label_actor = title;

        if (this.metaInfo['description']) {
            this._descriptionLabel = new St.Label({ style_class: 'list-search-result-description' });
            details.add(this._descriptionLabel, { x_fill: false,
                                                  y_fill: false,
                                                  x_align: St.Align.START,
                                                  y_align: St.Align.END });

            this._termsChangedId =
                this._resultsView.connect('terms-changed',
                                          this._highlightTerms.bind(this));

            this._highlightTerms();
        }

        let hoverIcon = new St.Icon({ style_class: 'list-search-result-arrow-icon',
                                      icon_name: 'go-next-symbolic' });
        content.add(hoverIcon, { x_fill: false,
                                 x_align: St.Align.END,
                                 expand: true });

        this.actor.connect('destroy', this._onDestroy.bind(this));
    }

    get ICON_SIZE() {
        return 24;
    }

    _highlightTerms() {
        let markup = this._resultsView.highlightTerms(this.metaInfo['description'].split('\n')[0]);
        this._descriptionLabel.clutter_text.set_markup(markup);
    }

    _onDestroy() {
        if (this._termsChangedId)
            this._resultsView.disconnect(this._termsChangedId);
        this._termsChangedId = 0;
    }
};

var GridSearchResult = class extends SearchResult {
    constructor(provider, metaInfo, resultsView) {
        super(provider, metaInfo, resultsView);

        this.actor.style_class = 'grid-search-result';

        this.icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                          { createIcon: this.metaInfo['createIcon'] });
        let content = new St.Bin({ child: this.icon });
        this.actor.set_child(content);
        this.actor.label_actor = this.icon.label;
    }
};

var SearchResultsBase = class {
    constructor(provider, resultsView) {
        this.provider = provider;
        this._resultsView = resultsView;

        this._terms = [];

        this.actor = new St.BoxLayout({ style_class: 'search-section',
                                        vertical: true });

        this._resultDisplayBin = new St.Bin({ x_fill: true,
                                              y_fill: true });
        this.actor.add(this._resultDisplayBin, { expand: true });

        let separator = new Separator.HorizontalSeparator({ style_class: 'search-section-separator' });
        this.actor.add(separator.actor);

        this._resultDisplays = {};

        this._clipboard = St.Clipboard.get_default();

        this._cancellable = new Gio.Cancellable();

        this.actor.connect('destroy', this._onDestroy.bind(this));
    }

    destroy() {
        this.actor.destroy();
    }

    _onDestroy() {
        this._terms = [];
    }

    _createResultDisplay(meta) {
        if (this.provider.createResultObject)
            return this.provider.createResultObject(meta, this._resultsView);

        return null;
    }

    clear() {
        this._cancellable.cancel();
        for (let resultId in this._resultDisplays)
            this._resultDisplays[resultId].actor.destroy();
        this._resultDisplays = {};
        this._clearResultDisplay();
        this.actor.hide();
    }

    _keyFocusIn(actor) {
        this.emit('key-focus-in', actor);
    }

    _activateResult(result, id) {
        this.provider.activateResult(id, this._terms);
        if (result.metaInfo.clipboardText)
            this._clipboard.set_text(St.ClipboardType.CLIPBOARD, result.metaInfo.clipboardText);
        Main.overview.hide();
    }

    _ensureResultActors(results, callback) {
        let metasNeeded = results.filter(
            resultId => this._resultDisplays[resultId] === undefined
        );

        if (metasNeeded.length === 0) {
            callback(true);
        } else {
            this._cancellable.cancel();
            this._cancellable.reset();

            this.provider.getResultMetas(metasNeeded, metas => {
                if (this._cancellable.is_cancelled()) {
                    if (metas.length > 0)
                        log(`Search provider ${this.provider.id} returned results after the request was canceled`);
                    callback(false);
                    return;
                }
                if (metas.length != metasNeeded.length) {
                    log(`Wrong number of result metas returned by search provider ${this.provider.id}: ` +
                        `expected ${metasNeeded.length} but got ${metas.length}`);
                    callback(false);
                    return;
                }
                if (metas.some(meta => !meta.name || !meta.id)) {
                    log(`Invalid result meta returned from search provider ${this.provider.id}`);
                    callback(false);
                    return;
                }

                metasNeeded.forEach((resultId, i) => {
                    let meta = metas[i];
                    let display = this._createResultDisplay(meta);
                    display.connect('activate', this._activateResult.bind(this));
                    display.actor.connect('key-focus-in', this._keyFocusIn.bind(this));
                    this._resultDisplays[resultId] = display;
                });
                callback(true);
            }, this._cancellable);
        }
    }

    updateSearch(providerResults, terms, callback) {
        this._terms = terms;
        if (providerResults.length == 0) {
            this._clearResultDisplay();
            this.actor.hide();
            callback();
        } else {
            let maxResults = this._getMaxDisplayedResults();
            let results = maxResults > -1
                ? this.provider.filterResults(providerResults, maxResults)
                : providerResults;

            this._ensureResultActors(results, successful => {
                if (!successful) {
                    this._clearResultDisplay();
                    callback();
                    return;
                }

                // To avoid CSS transitions causing flickering when
                // the first search result stays the same, we hide the
                // content while filling in the results.
                this.actor.hide();
                this._clearResultDisplay();
                results.forEach(resultId => {
                    this._addItem(this._resultDisplays[resultId]);
                });
                this.actor.show();
                callback();
            });
        }
    }
};

var ListSearchResults = class extends SearchResultsBase {
    constructor(provider, resultsView) {
        super(provider, resultsView);

        this._container = new St.BoxLayout({ style_class: 'search-section-content' });
        this.providerInfo = new ProviderInfo(provider);
        this.providerInfo.connect('key-focus-in', this._keyFocusIn.bind(this));
        this.providerInfo.connect('clicked', () => {
            this.providerInfo.animateLaunch();
            provider.launchSearch(this._terms);
            Main.overview.toggle();
        });

        this._container.add(this.providerInfo, { x_fill: false,
                                                 y_fill: false,
                                                 x_align: St.Align.START,
                                                 y_align: St.Align.START });

        this._content = new St.BoxLayout({ style_class: 'list-search-results',
                                           vertical: true });
        this._container.add(this._content, { expand: true,
                                             y_fill: false,
                                             y_align: St.Align.MIDDLE });

        this._resultDisplayBin.set_child(this._container);
    }

    _getMaxDisplayedResults() {
        return MAX_LIST_SEARCH_RESULTS_ROWS;
    }

    _clearResultDisplay() {
        this._content.remove_all_children();
    }

    _createResultDisplay(meta) {
        return super._createResultDisplay(meta) ||
               new ListSearchResult(this.provider, meta, this._resultsView);
    }

    _addItem(display) {
        if (this._content.get_n_children() > 0) {
            display.separator = new Separator.HorizontalSeparator({ style_class: 'search-section-separator' });
            this._content.add(display.separator.actor);
        }
        this._content.add_actor(display.actor);
    }

    getFirstResult() {
        if (this._content.get_n_children() > 0)
            return this._content.get_child_at_index(0)._delegate;
        else
            return null;
    }
};
Signals.addSignalMethods(ListSearchResults.prototype);

var GridSearchResults = class extends SearchResultsBase {
    constructor(provider, resultsView) {
        super(provider, resultsView);

        this._grid = new IconGrid.IconGrid({ rowLimit: MAX_GRID_SEARCH_RESULTS_ROWS,
                                             xAlign: St.Align.MIDDLE });

        this._bin = new St.Bin({ x_align: St.Align.MIDDLE });
        this._bin.set_child(this._grid);

        this._resultDisplayBin.set_child(this._bin);
    }

    _onDestroy() {
        if (this._updateSearchLater) {
            Meta.later_remove(this._updateSearchLater);
            delete this._updateSearchLater;
        }

        super._onDestroy();
    }

    updateSearch(...args) {
        if (this._notifyAllocationId)
            this.actor.disconnect(this._notifyAllocationId);
        if (this._updateSearchLater) {
            Meta.later_remove(this._updateSearchLater);
            delete this._updateSearchLater;
        }

        // Make sure the maximum number of results calculated by
        // _getMaxDisplayedResults() is updated after width changes.
        this._notifyAllocationId = this.actor.connect('notify::allocation', () => {
            if (this._updateSearchLater)
                return;
            this._updateSearchLater = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                delete this._updateSearchLater;
                super.updateSearch(...args);
                return GLib.SOURCE_REMOVE;
            });
        });

        super.updateSearch(...args);
    }

    _getMaxDisplayedResults() {
        return MAX_GRID_SEARCH_RESULTS_ROWS * MAX_GRID_SEARCH_RESULTS_COLS;
    }

    _clearResultDisplay() {
        this._grid.removeAll();
    }

    _createResultDisplay(meta) {
        return super._createResultDisplay(meta) ||
               new GridSearchResult(this.provider, meta, this._resultsView);
    }

    _addItem(display) {
        this._grid.addItem(display);
    }

    getFirstResult() {
        if (this._grid.visibleItemsCount() > 0)
            return this._grid.getItemAtIndex(0)._delegate;
        else
            return null;
    }
};
Signals.addSignalMethods(GridSearchResults.prototype);

var SearchResultsBin = GObject.registerClass(
class SearchResultsBin extends St.BoxLayout {
    vfunc_allocate(box, flags) {
        let themeNode = this.get_theme_node();
        let maxWidth = themeNode.get_max_width();
        let availWidth = box.x2 - box.x1;
        let adjustedBox = box;

        if (availWidth > maxWidth) {
            let excessWidth = availWidth - maxWidth;
            adjustedBox.x1 += Math.floor(excessWidth / 2);
            adjustedBox.x2 -= Math.floor(excessWidth / 2);
        }

        super.vfunc_allocate(adjustedBox, flags);
    }
});

var SearchResults = class {
    constructor() {
        this.actor = new SearchResultsBin({ name: 'searchResults',
                                            vertical: true });
        Util.blockClickEventsOnActor(this.actor);

        let closeIcon = new St.Icon({ icon_name: 'window-close-symbolic' });
        let closeButton = new St.Button({ name: 'searchResultsCloseButton',
                                          child: closeIcon,
                                          x_expand: true,
                                          y_expand: false });
        // We need to set the ClutterActor align, not St.Bin
        closeButton.set_x_align(Clutter.ActorAlign.END);
        closeButton.set_y_align(Clutter.ActorAlign.START);
        closeButton.connect('clicked', () => {
            this.emit('search-close-clicked');
        });

        let topBin = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        topBin.add_actor(closeButton);

        this.actor.add_child(topBin);

        this._content = new MaxWidthBox({ name: 'searchResultsContent',
                                          vertical: true });

        this._scrollView = new St.ScrollView({ x_fill: true,
                                               y_fill: false,
                                               overlay_scrollbars: true,
                                               style_class: 'search-display vfade' });
        this._scrollView.set_policy(St.PolicyType.NEVER, St.PolicyType.AUTOMATIC);
        this._scrollView.add_actor(this._content);

        let action = new Clutter.PanAction({ interpolate: true });
        action.connect('pan', this._onPan.bind(this));
        this._scrollView.add_action(action);

        this.actor.add(this._scrollView, { x_fill: true,
                                           y_fill: true,
                                           expand: true,
                                           x_align: St.Align.START,
                                           y_align: St.Align.START });

        this._statusText = new St.Label({ style_class: 'search-statustext' });
        this._statusBin = new St.Bin({ x_align: St.Align.MIDDLE,
                                       y_align: St.Align.MIDDLE });
        this.actor.add(this._statusBin, { expand: true });
        this._statusBin.add_actor(this._statusText);

        this._highlightDefault = false;
        this._defaultResult = null;
        this._startingSearch = false;

        this._terms = [];
        this._results = {};
        this._isAnimating = false;

        this._providers = [];

        this._highlightRegex = null;

        this._searchSettings = new Gio.Settings({ schema_id: SEARCH_PROVIDERS_SCHEMA });
        this._searchSettings.connect('changed::disabled', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::enabled', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::disable-external', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::sort-order', this._reloadRemoteProviders.bind(this));

        this._searchTimeoutId = 0;
        this._cancellable = new Gio.Cancellable();

        this._registerProvider(new AppDisplay.AppSearchProvider());

        let appSystem = Shell.AppSystem.get_default();
        appSystem.connect('installed-changed', this._reloadRemoteProviders.bind(this));

        this._internetProvider = InternetSearch.getInternetSearchProvider();
        if (this._internetProvider)
            this._registerProvider(this._internetProvider);

        this._reloadRemoteProviders();
    }

    _reloadRemoteProviders() {
        let remoteProviders = this._providers.filter(p => p.isRemoteProvider);
        remoteProviders.forEach(provider => {
            this._unregisterProvider(provider);
        });

        RemoteSearch.loadRemoteSearchProviders(this._searchSettings, providers => {
            providers.forEach(this._registerProvider.bind(this));
        });
    }

    _registerProvider(provider) {
        provider.searchInProgress = false;
        this._providers.push(provider);
        this._ensureProviderDisplay(provider);
    }

    _unregisterProvider(provider) {
        let index = this._providers.indexOf(provider);
        this._providers.splice(index, 1);

        if (provider.display)
            provider.display.destroy();
    }

    _gotResults(results, provider) {
        this._results[provider.id] = results;
        this._updateResults(provider, results);
    }

    _clearSearchTimeout() {
        if (this._searchTimeoutId > 0) {
            GLib.source_remove(this._searchTimeoutId);
            this._searchTimeoutId = 0;
        }
    }

    _reset() {
        this._terms = [];
        this._results = {};
        this._clearDisplay();
        this._clearSearchTimeout();
        this._defaultResult = null;
        this._startingSearch = false;

        this._updateSearchProgress();
    }

    _doSearch() {
        this._startingSearch = false;

        let previousResults = this._results;
        this._results = {};

        this._providers.forEach(provider => {
            provider.searchInProgress = true;

            let previousProviderResults = previousResults[provider.id];
            if (this._isSubSearch && previousProviderResults)
                provider.getSubsearchResultSet(previousProviderResults,
                                               this._terms,
                                               results => {
                                                   this._gotResults(results, provider);
                                               },
                                               this._cancellable);
            else
                provider.getInitialResultSet(this._terms,
                                             results => {
                                                 this._gotResults(results, provider);
                                             },
                                             this._cancellable);
        });

        this._updateSearchProgress();

        this._clearSearchTimeout();
    }

    _onSearchTimeout() {
        this._searchTimeoutId = 0;
        this._doSearch();
        return GLib.SOURCE_REMOVE;
    }

    setTerms(terms) {
        // Check for the case of making a duplicate previous search before
        // setting state of the current search or cancelling the search.
        // This will prevent incorrect state being as a result of a duplicate
        // search while the previous search is still active.
        let searchString = terms.join(' ');
        let previousSearchString = this._terms.join(' ');
        if (searchString == previousSearchString)
            return;

        this._startingSearch = true;

        this._cancellable.cancel();
        this._cancellable.reset();

        if (terms.length == 0) {
            this._reset();
            return;
        }

        let isSubSearch = false;
        if (this._terms.length > 0)
            isSubSearch = searchString.indexOf(previousSearchString) == 0;

        this._terms = terms;
        this._isSubSearch = isSubSearch;
        this._updateSearchProgress();

        if (this._searchTimeoutId == 0)
            this._searchTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 150, this._onSearchTimeout.bind(this));

        let escapedTerms = this._terms.map(term => Shell.util_regex_escape(term));
        this._highlightRegex = new RegExp(`(${escapedTerms.join('|')})`, 'gi');

        this.emit('terms-changed');
    }

    _onPan(action) {
        let [dist_, dx_, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollView.vscroll.adjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    }

    _keyFocusIn(provider, actor) {
        Util.ensureActorVisibleInScrollView(this._scrollView, actor);
    }

    _ensureProviderDisplay(provider) {
        if (provider.display)
            return;

        let providerDisplay;
        if (provider.appInfo)
            providerDisplay = new ListSearchResults(provider, this);
        else
            providerDisplay = new GridSearchResults(provider, this);

        providerDisplay.connect('key-focus-in', this._keyFocusIn.bind(this));
        providerDisplay.actor.hide();
        this._content.add(providerDisplay.actor);
        provider.display = providerDisplay;
    }

    _clearDisplay() {
        this._providers.forEach(provider => {
            provider.display.clear();
        });
    }

    _maybeSetInitialSelection() {
        let newDefaultResult = null;

        let providers = this._providers;
        for (let i = 0; i < providers.length; i++) {
            let provider = providers[i];
            let display = provider.display;

            if (!display.actor.visible)
                continue;

            let firstResult = display.getFirstResult();
            if (firstResult) {
                newDefaultResult = firstResult;
                break; // select this one!
            }
        }

        if (newDefaultResult != this._defaultResult) {
            this._setSelected(this._defaultResult, false);
            this._setSelected(newDefaultResult, this._highlightDefault);

            this._defaultResult = newDefaultResult;
        }
    }

    get searchInProgress() {
        if (this._startingSearch)
            return true;

        return this._providers.some(p => p.searchInProgress);
    }

    get isAnimating() {
        return this._isAnimating;
    }

    set isAnimating (v) {
        if (this._isAnimating == v)
            return;

        this._isAnimating = v;
        this._updateSearchProgress();
        if (!this._isAnimating) {
            this._providers.forEach((provider) => {
                let results = this._results[provider.id];
                if (results)
                    this._updateResults(provider, results);
            });
        }
    }

    _syncSeparatorVisibility() {
        let lastVisibleDisplay;
        for (let i = 0; i < this._providers.length; i++) {
            let provider = this._providers[i];
            let display = provider.display;

            if (!display.separator)
                continue;

            display.separator.actor.show();
            if (display.actor.visible)
                lastVisibleDisplay = display;
        }

        if (lastVisibleDisplay)
            lastVisibleDisplay.separator.actor.hide();
    }

    _updateSearchProgress() {
        let haveResults = this._providers.some(provider => {
            let display = provider.display;
            return (display.getFirstResult() != null);
        });
        let showStatus = !haveResults && !this.isAnimating;

        this._syncSeparatorVisibility();
        this._scrollView.visible = haveResults;
        this._statusBin.visible = showStatus;

        if (showStatus) {
            if (this.searchInProgress) {
                this._statusText.set_text(_("Searching…"));
            } else {
                this._statusText.set_text(_("No results."));
            }
        }

        this.emit('search-progress-updated');
    }

    _updateResults(provider, results) {
        let terms = this._terms;
        let display = provider.display;

        display.updateSearch(results, terms, () => {
            provider.searchInProgress = false;

            this._maybeSetInitialSelection();
            this._updateSearchProgress();
        });
    }

    activateDefault() {
        // If we are about to activate a result, we are done animating and need
        // to update the display immediately.
        this.isAnimating = false;

        // If we have a search queued up, force the search now.
        if (this._searchTimeoutId > 0)
            this._doSearch();

        if (this._defaultResult)
            this._defaultResult.activate();
    }

    highlightDefault(highlight) {
        this._highlightDefault = highlight;
        this._setSelected(this._defaultResult, highlight);
    }

    popupMenuDefault() {
        // If we have a search queued up, force the search now.
        if (this._searchTimeoutId > 0)
            this._doSearch();

        if (this._defaultResult)
            this._defaultResult.actor.popup_menu();
    }

    navigateFocus(direction) {
        let rtl = this.actor.get_text_direction() == Clutter.TextDirection.RTL;
        if (direction == St.DirectionType.TAB_BACKWARD ||
            direction == (rtl
                ? St.DirectionType.RIGHT
                : St.DirectionType.LEFT) ||
            direction == St.DirectionType.UP) {
            this.actor.navigate_focus(null, direction, false);
            return;
        }

        let from = this._defaultResult ? this._defaultResult.actor : null;
        this.actor.navigate_focus(from, direction, false);
    }

    _setSelected(result, selected) {
        if (!result)
            return;

        if (selected) {
            result.actor.add_style_pseudo_class('selected');
            Util.ensureActorVisibleInScrollView(this._scrollView, result.actor);
        } else {
            result.actor.remove_style_pseudo_class('selected');
        }
    }

    highlightTerms(description) {
        if (!description)
            return '';

        if (!this._highlightRegex)
            return description;

        return description.replace(this._highlightRegex, '<b>$1</b>');
    }
};
Signals.addSignalMethods(SearchResults.prototype);

var ProviderInfo = GObject.registerClass(
class ProviderInfo extends St.Button {
    _init(provider) {
        this.provider = provider;
        super._init({ style_class: 'search-provider-icon',
                      reactive: true,
                      can_focus: true,
                      accessible_name: provider.appInfo.get_name(),
                      track_hover: true });

        let icon = new St.Icon({ icon_size: this.PROVIDER_ICON_SIZE,
                                 gicon: provider.appInfo.get_icon() });

        this._content = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._content.add_actor(icon);

        let box = new St.BoxLayout({ vertical: true, x_expand: false });
        this.set_child(box);

        box.add_actor(this._content);

        let label = new St.Label({ text: provider.appInfo.get_name(),
                                   style_class: 'search-provider-icon-label' });
        box.add_actor(label);
    }

    get PROVIDER_ICON_SIZE() {
        return 64;
    }

    animateLaunch() {
        let appSys = Shell.AppSystem.get_default();
        let app = appSys.lookup_app(this.provider.appInfo.get_id());
        if (app.state == Shell.AppState.STOPPED)
            IconGrid.zoomOutActor(this._content);
    }
});
