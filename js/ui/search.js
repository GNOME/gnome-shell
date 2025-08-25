import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as AppDisplay from './appDisplay.js';
import * as IconGrid from './iconGrid.js';
import * as Main from './main.js';
import * as ParentalControlsManager from '../misc/parentalControlsManager.js';
import * as RemoteSearch from './remoteSearch.js';
import {ensureActorVisibleInScrollView} from '../misc/animationUtils.js';

import {Highlighter} from '../misc/util.js';
import {Spinner} from './animation.js';

const SEARCH_PROVIDERS_SCHEMA = 'org.gnome.desktop.search-providers';

const MAX_LIST_SEARCH_RESULTS_ROWS = 5;
const SEARCH_SPINNER_SIZE = 64;

const MaxWidthBox = GObject.registerClass(
class MaxWidthBox extends St.BoxLayout {
    vfunc_allocate(box) {
        let themeNode = this.get_theme_node();
        let maxWidth = themeNode.get_max_width();
        let availWidth = box.x2 - box.x1;
        let adjustedBox = box;

        if (availWidth > maxWidth) {
            let excessWidth = availWidth - maxWidth;
            adjustedBox.x1 += Math.floor(excessWidth / 2);
            adjustedBox.x2 -= Math.floor(excessWidth / 2);
        }

        super.vfunc_allocate(adjustedBox);
    }
});

export const SearchResult = GObject.registerClass(
class SearchResult extends St.Button {
    _init(provider, metaInfo, resultsView) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this._resultsView = resultsView;

        super._init({
            reactive: true,
            can_focus: true,
            track_hover: true,
        });
    }

    vfunc_clicked() {
        this.activate();
    }

    activate() {
        this.provider.activateResult(this.metaInfo.id, this._resultsView.terms);

        if (this.metaInfo.clipboardText) {
            St.Clipboard.get_default().set_text(
                St.ClipboardType.CLIPBOARD, this.metaInfo.clipboardText);
        }
        Main.overview.toggle();
    }
});

export const ListSearchResult = GObject.registerClass(
class ListSearchResult extends SearchResult {
    _init(provider, metaInfo, resultsView) {
        super._init(provider, metaInfo, resultsView);

        this.style_class = 'list-search-result';

        let content = new St.BoxLayout({
            style_class: 'list-search-result-content',
            orientation: Clutter.Orientation.HORIZONTAL,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
            y_expand: true,
        });
        this.set_child(content);

        let titleBox = new St.BoxLayout({
            style_class: 'list-search-result-title',
            y_align: Clutter.ActorAlign.CENTER,
        });

        content.add_child(titleBox);

        // An icon for, or thumbnail of, content
        let icon = this.metaInfo['createIcon'](this.ICON_SIZE);
        if (icon)
            titleBox.add_child(icon);

        let title = new St.Label({
            text: this.metaInfo['name'],
            y_align: Clutter.ActorAlign.CENTER,
        });
        titleBox.add_child(title);

        this.label_actor = title;

        if (this.metaInfo['description']) {
            this._descriptionLabel = new St.Label({
                style_class: 'list-search-result-description',
                y_align: Clutter.ActorAlign.CENTER,
            });
            content.add_child(this._descriptionLabel);

            this._resultsView.connectObject(
                'terms-changed', this._highlightTerms.bind(this), this);

            this._highlightTerms();
        }
    }

    get ICON_SIZE() {
        return 24;
    }

    _highlightTerms() {
        let markup = this._resultsView.highlightTerms(this.metaInfo['description'].split('\n')[0]);
        this._descriptionLabel.clutter_text.set_markup(markup);
    }
});

export const GridSearchResult = GObject.registerClass(
class GridSearchResult extends SearchResult {
    _init(provider, metaInfo, resultsView) {
        super._init(provider, metaInfo, resultsView);

        this.style_class = 'grid-search-result';

        this.icon = new IconGrid.BaseIcon(this.metaInfo['name'], {
            createIcon: this.metaInfo['createIcon'],
        });
        let content = new St.Bin({
            child: this.icon,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
            y_expand: true,
        });
        this.set_child(content);
        this.label_actor = this.icon.label;
    }
});

const SearchResultsBase = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
    Properties: {
        'focus-child': GObject.ParamSpec.object(
            'focus-child', null, null,
            GObject.ParamFlags.READABLE,
            Clutter.Actor.$gtype),
    },
}, class SearchResultsBase extends St.BoxLayout {
    _init(provider, resultsView) {
        super._init({
            style_class: 'search-section',
            orientation: Clutter.Orientation.VERTICAL,
        });

        this.provider = provider;
        this._resultsView = resultsView;

        this._terms = [];
        this._focusChild = null;

        this._resultDisplayBin = new St.Bin();
        this.add_child(this._resultDisplayBin);

        let separator = new St.Widget({style_class: 'search-section-separator'});
        this.add_child(separator);

        this._resultDisplays = {};

        this._cancellable = new Gio.Cancellable();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        this._terms = [];
    }

    _createResultDisplay(meta) {
        if (this.provider.createResultObject)
            return this.provider.createResultObject(meta);

        return null;
    }

    clear() {
        this._cancellable.cancel();
        for (let resultId in this._resultDisplays)
            this._resultDisplays[resultId].destroy();
        this._resultDisplays = {};
        this._clearResultDisplay();
        this.hide();
    }

    get focusChild() {
        return this._focusChild;
    }

    _keyFocusIn(actor) {
        if (this._focusChild === actor)
            return;
        this._focusChild = actor;
        this.notify('focus-child');
    }

    _setMoreCount(_count) {
    }

    async _ensureResultActors(results) {
        let metasNeeded = results.filter(
            resultId => this._resultDisplays[resultId] === undefined);

        if (metasNeeded.length === 0)
            return;

        this._cancellable.cancel();
        this._cancellable.reset();

        const metas = await this.provider.getResultMetas(metasNeeded, this._cancellable);

        if (this._cancellable.is_cancelled()) {
            if (metas.length > 0)
                throw new Error(`Search provider ${this.provider.id} returned results after the request was canceled`);
        }

        if (metas.length !== metasNeeded.length) {
            throw new Error(`Wrong number of result metas returned by search provider ${this.provider.id}: ` +
                `expected ${metasNeeded.length} but got ${metas.length}`);
        }

        if (metas.some(meta => !meta.name || !meta.id))
            throw new Error(`Invalid result meta returned from search provider ${this.provider.id}`);

        metasNeeded.forEach((resultId, i) => {
            let meta = metas[i];
            let display = this._createResultDisplay(meta);
            display.connect('key-focus-in', this._keyFocusIn.bind(this));
            this._resultDisplays[resultId] = display;
        });
    }

    async updateSearch(providerResults, terms, callback) {
        this._terms = terms;
        if (providerResults.length === 0) {
            this._clearResultDisplay();
            this.hide();
            callback();
        } else {
            let maxResults = this._getMaxDisplayedResults();
            let results = maxResults > -1
                ? this.provider.filterResults(providerResults, maxResults)
                : providerResults;
            let moreCount = Math.max(providerResults.length - results.length, 0);

            try {
                await this._ensureResultActors(results);

                // To avoid CSS transitions causing flickering when
                // the first search result stays the same, we hide the
                // content while filling in the results.
                this.hide();
                this._clearResultDisplay();
                results.forEach(
                    resultId => this._addItem(this._resultDisplays[resultId]));
                this._setMoreCount(this.provider.canLaunchSearch ? moreCount : 0);
                this.show();
                callback();
            } catch {
                this._clearResultDisplay();
                callback();
            }
        }
    }
});

export const ListSearchResults = GObject.registerClass(
class ListSearchResults extends SearchResultsBase {
    _init(provider, resultsView) {
        super._init(provider, resultsView);

        this._container = new St.BoxLayout({
            style_class: 'search-section-content',
            x_expand: true,
        });
        this.providerInfo = new ProviderInfo(provider);
        this.providerInfo.connect('key-focus-in', this._keyFocusIn.bind(this));
        this.providerInfo.connect('clicked', () => {
            this.providerInfo.animateLaunch();
            provider.launchSearch(this._terms);
            Main.overview.toggle();
        });

        this._container.add_child(this.providerInfo);

        this._content = new St.BoxLayout({
            style_class: 'list-search-results',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });
        this._container.add_child(this._content);

        this._resultDisplayBin.child = this._container;
    }

    _setMoreCount(count) {
        this.providerInfo.setMoreCount(count);
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
        this._content.add_child(display);
    }

    getFirstResult() {
        if (this._content.get_n_children() > 0)
            return this._content.get_child_at_index(0);
        else
            return null;
    }
});

const GridSearchResultsLayout = GObject.registerClass({
    Properties: {
        'spacing': GObject.ParamSpec.int('spacing', null, null,
            GObject.ParamFlags.READWRITE, 0, GLib.MAXINT32, 0),
    },
}, class GridSearchResultsLayout extends Clutter.LayoutManager {
    _init() {
        super._init();
        this._spacing = 0;
    }

    vfunc_set_container(container) {
        this._container = container;
    }

    vfunc_get_preferred_width(container, forHeight) {
        let minWidth = 0;
        let natWidth = 0;
        let first = true;

        for (let child of container) {
            if (!child.visible)
                continue;

            const [childMinWidth, childNatWidth] = child.get_preferred_width(forHeight);

            minWidth = Math.max(minWidth, childMinWidth);
            natWidth += childNatWidth;

            if (first)
                first = false;
            else
                natWidth += this._spacing;
        }

        return [minWidth, natWidth];
    }

    vfunc_get_preferred_height(container, forWidth) {
        let minHeight = 0;
        let natHeight = 0;

        for (let child of container) {
            if (!child.visible)
                continue;

            const [childMinHeight, childNatHeight] = child.get_preferred_height(forWidth);

            minHeight = Math.max(minHeight, childMinHeight);
            natHeight = Math.max(natHeight, childNatHeight);
        }

        return [minHeight, natHeight];
    }

    vfunc_allocate(container, box) {
        const width = box.get_width();
        const ltr = container.get_text_direction() !== Clutter.TextDirection.RTL;

        const childBox = new Clutter.ActorBox();
        let accumulatedWidth = 0;

        let first = true;
        for (let child of container) {
            if (!child.visible)
                continue;

            if (first)
                first = false;
            else
                accumulatedWidth += this._spacing;

            const [childWidth] = child.get_preferred_width(-1);
            const [childHeight] = child.get_preferred_height(-1);

            if (ltr)
                childBox.set_origin(accumulatedWidth, 0);
            else
                childBox.set_origin(width - accumulatedWidth - childWidth, 0);

            accumulatedWidth += childWidth;

            if (accumulatedWidth <= width)
                childBox.set_size(childWidth, childHeight);
            else
                childBox.set_size(0, 0);

            child.allocate(childBox);
            child.can_focus = childBox.get_area() > 0;
        }
    }

    columnsForWidth(width) {
        if (!this._container)
            return -1;

        const [minWidth] = this.get_preferred_width(this._container, -1);

        if (minWidth === 0)
            return -1;

        let nCols = 0;
        while (width > minWidth) {
            width -= minWidth;
            if (nCols > 0)
                width -= this._spacing;
            nCols++;
        }

        return nCols;
    }

    get spacing() {
        return this._spacing;
    }

    set spacing(v) {
        if (this._spacing === v)
            return;
        this._spacing = v;
        this.layout_changed();
    }
});

export const GridSearchResults = GObject.registerClass(
class GridSearchResults extends SearchResultsBase {
    _init(provider, resultsView) {
        super._init(provider, resultsView);

        this._grid = new St.Widget({style_class: 'grid-search-results'});
        this._grid.layout_manager = new GridSearchResultsLayout();

        this._grid.connect('style-changed', () => {
            const node = this._grid.get_theme_node();
            this._grid.layout_manager.spacing = node.get_length('spacing');
        });

        this._resultDisplayBin.child = new St.Bin({
            child: this._grid,
            x_align: Clutter.ActorAlign.CENTER,
        });

        this._maxResults = provider.maxResults ?? -1;
    }

    _onDestroy() {
        if (this._updateSearchLater) {
            const laters = global.compositor.get_laters();
            laters.remove(this._updateSearchLater);
            delete this._updateSearchLater;
        }

        super._onDestroy();
    }

    updateSearch(...args) {
        if (this._notifyAllocationId)
            this.disconnect(this._notifyAllocationId);
        if (this._updateSearchLater) {
            const laters = global.compositor.get_laters();
            laters.remove(this._updateSearchLater);
            delete this._updateSearchLater;
        }

        // Make sure the maximum number of results calculated by
        // _getMaxDisplayedResults() is updated after width changes.
        this._notifyAllocationId = this.connect('notify::allocation', () => {
            if (this._updateSearchLater)
                return;
            const laters = global.compositor.get_laters();
            this._updateSearchLater = laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                delete this._updateSearchLater;
                super.updateSearch(...args);
                return GLib.SOURCE_REMOVE;
            });
        });

        super.updateSearch(...args);
    }

    _getMaxDisplayedResults() {
        const width = this.allocation.get_width();
        if (width === 0)
            return this._maxResults;

        const nCols = this._grid.layout_manager.columnsForWidth(width);
        if (nCols < 0)
            return this._maxResults;

        if (this._maxResults < 0)
            return nCols;

        return Math.min(nCols, this._maxResults);
    }

    _clearResultDisplay() {
        this._grid.remove_all_children();
    }

    _createResultDisplay(meta) {
        return super._createResultDisplay(meta) ||
               new GridSearchResult(this.provider, meta, this._resultsView);
    }

    _addItem(display) {
        this._grid.add_child(display);
    }

    getFirstResult() {
        for (let child of this._grid) {
            if (child.visible)
                return child;
        }
        return null;
    }
});

export const SearchResultsView = GObject.registerClass({
    Signals: {'terms-changed': {}},
}, class SearchResultsView extends St.BoxLayout {
    _init() {
        super._init({
            name: 'searchResults',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            y_expand: true,
        });

        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._parentalControlsManager.connect('app-filter-changed', this._reloadRemoteProviders.bind(this));

        this._content = new MaxWidthBox({
            name: 'searchResultsContent',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });

        this._scrollView = new St.ScrollView({
            overlay_scrollbars: true,
            style_class: 'search-display vfade',
            x_expand: true,
            y_expand: true,
            child: this._content,
        });

        const panGesture = new Clutter.PanGesture();
        panGesture.connect('pan-update', this._onPanUpdate.bind(this));
        this._scrollView.add_action(panGesture);

        this.add_child(this._scrollView);

        this._statusContainer = new St.BoxLayout({
            style_class: 'search-statusbox',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._statusSpinner = new Spinner(SEARCH_SPINNER_SIZE);

        this._statusText = new St.Label({style_class: 'search-statustext'});

        this._statusContainer.add_child(this._statusSpinner);
        this._statusContainer.add_child(this._statusText);

        this.add_child(this._statusContainer);

        this._highlightDefault = false;
        this._defaultResult = null;
        this._startingSearch = false;

        this._terms = [];
        this._results = {};

        this._providers = [];

        this._highlighter = new Highlighter();

        this._searchSettings = new Gio.Settings({schema_id: SEARCH_PROVIDERS_SCHEMA});
        this._searchSettings.connect('changed::disabled', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::enabled', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::disable-external', this._reloadRemoteProviders.bind(this));
        this._searchSettings.connect('changed::sort-order', this._reloadRemoteProviders.bind(this));

        this._searchTimeoutId = 0;
        this._cancellable = new Gio.Cancellable();

        this._registerProvider(new AppDisplay.AppSearchProvider());

        let appSystem = Shell.AppSystem.get_default();
        appSystem.connect('installed-changed', this._reloadRemoteProviders.bind(this));
        this._reloadRemoteProviders();
    }

    get terms() {
        return this._terms;
    }

    _reloadRemoteProviders() {
        let remoteProviders = this._providers.filter(p => p.isRemoteProvider);
        remoteProviders.forEach(provider => {
            this._unregisterProvider(provider);
        });

        const providers = RemoteSearch.loadRemoteSearchProviders(this._searchSettings);
        providers.forEach(this._registerProvider.bind(this));
    }

    _registerProvider(provider) {
        provider.searchInProgress = false;

        // Filter out unwanted providers.
        if (provider.appInfo && !this._parentalControlsManager.shouldShowApp(provider.appInfo))
            return;

        this._providers.push(provider);
        this._ensureProviderDisplay(provider);
    }

    _unregisterProvider(provider) {
        let index = this._providers.indexOf(provider);
        this._providers.splice(index, 1);

        if (provider.display)
            provider.display.destroy();
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

    async _doProviderSearch(provider, previousResults) {
        provider.searchInProgress = true;

        let results;
        if (this._isSubSearch && previousResults) {
            results = await provider.getSubsearchResultSet(
                previousResults,
                this._terms,
                this._cancellable);
        } else {
            results = await provider.getInitialResultSet(
                this._terms,
                this._cancellable);
        }

        this._results[provider.id] = results;
        this._updateResults(provider, results);
    }

    _doSearch() {
        this._startingSearch = false;

        let previousResults = this._results;
        this._results = {};

        this._providers.forEach(provider => {
            let previousProviderResults = previousResults[provider.id];
            this._doProviderSearch(provider, previousProviderResults);
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
        if (searchString === previousSearchString)
            return;

        this._startingSearch = true;

        this._cancellable.cancel();
        this._cancellable.reset();

        if (terms.length === 0) {
            this._reset();
            return;
        }

        let isSubSearch = false;
        if (this._terms.length > 0)
            isSubSearch = searchString.indexOf(previousSearchString) === 0;

        this._terms = terms;
        this._isSubSearch = isSubSearch;
        this._updateSearchProgress();

        if (this._searchTimeoutId === 0)
            this._searchTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 150, this._onSearchTimeout.bind(this));

        this._highlighter = new Highlighter(this._terms);

        this.emit('terms-changed');
    }

    _onPanUpdate(gesture) {
        const adjustment = this._scrollView.vadjustment;
        const delta = gesture.get_delta();

        adjustment.value -= (delta.get_y() / this.height) * adjustment.page_size;
    }

    _focusChildChanged(provider) {
        ensureActorVisibleInScrollView(this._scrollView, provider.focusChild);
    }

    _ensureProviderDisplay(provider) {
        if (provider.display)
            return;

        let providerDisplay;
        if (provider.appInfo)
            providerDisplay = new ListSearchResults(provider, this);
        else
            providerDisplay = new GridSearchResults(provider, this);

        providerDisplay.connect('notify::focus-child', this._focusChildChanged.bind(this));
        providerDisplay.hide();
        this._content.add_child(providerDisplay);
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

            if (!display.visible)
                continue;

            let firstResult = display.getFirstResult();
            if (firstResult) {
                newDefaultResult = firstResult;
                break; // select this one!
            }
        }

        if (newDefaultResult !== this._defaultResult) {
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

    _updateSearchProgress() {
        let haveResults = this._providers.some(provider => {
            let display = provider.display;
            return display.getFirstResult() != null;
        });

        this._scrollView.visible = haveResults;
        this._statusContainer.visible = !haveResults;

        if (!haveResults) {
            if (this.searchInProgress) {
                this._statusSpinner.play();
                this._statusText.set_text(_('Searching'));
            } else {
                this._statusSpinner.stop();
                this._statusText.set_text(_('No results'));
            }
        }
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
            this._defaultResult.popup_menu();
    }

    navigateFocus(direction) {
        let rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        if (direction === St.DirectionType.TAB_BACKWARD ||
            direction === (rtl
                ? St.DirectionType.RIGHT
                : St.DirectionType.LEFT) ||
            direction === St.DirectionType.UP) {
            this.navigate_focus(null, direction, false);
            return;
        }

        const from = this._defaultResult ?? null;
        this.navigate_focus(from, direction, false);
    }

    _setSelected(result, selected) {
        if (!result)
            return;

        if (selected) {
            result.add_style_pseudo_class('selected');
            ensureActorVisibleInScrollView(this._scrollView, result);
        } else {
            result.remove_style_pseudo_class('selected');
        }
    }

    highlightTerms(description) {
        if (!description)
            return '';

        return this._highlighter.highlight(description);
    }
});

const ProviderInfo = GObject.registerClass(
class ProviderInfo extends St.Button {
    _init(provider) {
        this.provider = provider;
        super._init({
            style_class: 'search-provider-icon',
            reactive: true,
            can_focus: true,
            accessible_name: provider.appInfo.get_name(),
            track_hover: true,
            y_align: Clutter.ActorAlign.START,
        });

        this._content = new St.BoxLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            style_class: 'list-search-provider-content',
        });
        this.set_child(this._content);

        const icon = new St.Icon({
            icon_size: this.PROVIDER_ICON_SIZE,
            gicon: provider.appInfo.get_icon(),
        });

        const detailsBox = new St.BoxLayout({
            style_class: 'list-search-provider-details',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });

        const nameLabel = new St.Label({
            text: provider.appInfo.get_name(),
            x_align: Clutter.ActorAlign.START,
        });

        this._moreLabel = new St.Label({x_align: Clutter.ActorAlign.START});

        detailsBox.add_child(nameLabel);
        detailsBox.add_child(this._moreLabel);


        this._content.add_child(icon);
        this._content.add_child(detailsBox);
    }

    get PROVIDER_ICON_SIZE() {
        return 32;
    }

    animateLaunch() {
        let appSys = Shell.AppSystem.get_default();
        let app = appSys.lookup_app(this.provider.appInfo.get_id());
        if (app.state === Shell.AppState.STOPPED)
            IconGrid.zoomOutActor(this._content);
    }

    setMoreCount(count) {
        this._moreLabel.text = ngettext('%d more', '%d more', count).format(count);
        this._moreLabel.visible = count > 0;
    }
});
