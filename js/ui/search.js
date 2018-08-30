// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const AppDisplay = imports.ui.appDisplay;
const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const RemoteSearch = imports.ui.remoteSearch;
const Util = imports.misc.util;

const SEARCH_PROVIDERS_SCHEMA = 'org.gnome.desktop.search-providers';

var MAX_LIST_SEARCH_RESULTS_ROWS = 5;
var MAX_GRID_SEARCH_RESULTS_ROWS = 1;

var MaxWidthBin = new Lang.Class({
    Name: 'MaxWidthBin',
    Extends: St.Bin,

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

        this.parent(adjustedBox, flags);
    }
});

var SearchResult = new Lang.Class({
    Name: 'SearchResult',

    _init(provider, metaInfo, resultsView) {
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
    },

    activate() {
        this.emit('activate', this.metaInfo.id);
    }
});
Signals.addSignalMethods(SearchResult.prototype);

var ListSearchResult = new Lang.Class({
    Name: 'ListSearchResult',
    Extends: SearchResult,

    ICON_SIZE: 24,

    _init(provider, metaInfo, resultsView) {
        this.parent(provider, metaInfo, resultsView);

        this.actor.style_class = 'list-search-result';
        this.actor.x_fill = true;

        let content = new St.BoxLayout({ style_class: 'list-search-result-content',
                                         vertical: false });
        this.actor.set_child(content);

        this._termsChangedId = 0;

        let titleBox = new St.BoxLayout({ style_class: 'list-search-result-title' });

        content.add(titleBox, { x_fill: true,
                                y_fill: false,
                                x_align: St.Align.START,
                                y_align: St.Align.MIDDLE });

        // An icon for, or thumbnail of, content
        let icon = this.metaInfo['createIcon'](this.ICON_SIZE);
        if (icon) {
            titleBox.add(icon);
        }

        let title = new St.Label({ text: this.metaInfo['name'] });
        titleBox.add(title, { x_fill: false,
                              y_fill: false,
                              x_align: St.Align.START,
                              y_align: St.Align.MIDDLE });

        this.actor.label_actor = title;

        if (this.metaInfo['description']) {
            this._descriptionLabel = new St.Label({ style_class: 'list-search-result-description' });
            content.add(this._descriptionLabel, { x_fill: false,
                                                  y_fill: false,
                                                  x_align: St.Align.START,
                                                  y_align: St.Align.MIDDLE });

            this._termsChangedId =
                this._resultsView.connect('terms-changed',
                                          this._highlightTerms.bind(this));

            this._highlightTerms();
        }

        this.actor.connect('destroy', this._onDestroy.bind(this));
    },

    _highlightTerms() {
        let markup = this._resultsView.highlightTerms(this.metaInfo['description'].split('\n')[0]);
        this._descriptionLabel.clutter_text.set_markup(markup);
    },

    _onDestroy() {
        if (this._termsChangedId)
            this._resultsView.disconnect(this._termsChangedId);
        this._termsChangedId = 0;
    }
});

var GridSearchResult = new Lang.Class({
    Name: 'GridSearchResult',
    Extends: SearchResult,

    _init(provider, metaInfo, resultsView) {
        this.parent(provider, metaInfo, resultsView);

        this.actor.style_class = 'grid-search-result';

        this.icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                          { createIcon: this.metaInfo['createIcon'] });
        let content = new St.Bin({ child: this.icon.actor });
        this.actor.set_child(content);
        this.actor.label_actor = this.icon.label;
    }
});

var SearchResultsBase = new Lang.Class({
    Name: 'SearchResultsBase',

    _init(provider, resultsView) {
        this.provider = provider;
        this._resultsView = resultsView;

        this._terms = [];

        this.actor = new St.BoxLayout({ style_class: 'search-section',
                                        vertical: true });

        this._resultDisplayBin = new St.Bin({ x_fill: true,
                                              y_fill: true });
        this.actor.add(this._resultDisplayBin, { expand: true });

        let separator = new St.Widget({ style_class: 'search-section-separator' });
        this.actor.add(separator);

        this._resultDisplays = {};

        this._clipboard = St.Clipboard.get_default();

        this._cancellable = new Gio.Cancellable();
    },

    destroy() {
        this.actor.destroy();
        this._terms = [];
    },

    _createResultDisplay(meta) {
        if (this.provider.createResultObject)
            return this.provider.createResultObject(meta, this._resultsView);

        return null;
    },

    clear() {
        this._cancellable.cancel();
        for (let resultId in this._resultDisplays)
            this._resultDisplays[resultId].actor.destroy();
        this._resultDisplays = {};
        this._clearResultDisplay();
        this.actor.hide();
    },

    _keyFocusIn(actor) {
        this.emit('key-focus-in', actor);
    },

    _activateResult(result, id) {
        this.provider.activateResult(id, this._terms);
        if (result.metaInfo.clipboardText)
            this._clipboard.set_text(St.ClipboardType.CLIPBOARD, result.metaInfo.clipboardText);
        Main.overview.toggle();
    },

    _setMoreCount(count) {
    },

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
                    log('Wrong number of result metas returned by search provider ' + this.provider.id +
                        ': expected ' + metasNeeded.length + ' but got ' + metas.length);
                    callback(false);
                    return;
                }
                if (metas.some(meta => !meta.name || !meta.id)) {
                    log('Invalid result meta returned from search provider ' + this.provider.id);
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
    },

    updateSearch(providerResults, terms, callback) {
        this._terms = terms;
        if (providerResults.length == 0) {
            this._clearResultDisplay();
            this.actor.hide();
            callback();
        } else {
            let maxResults = this._getMaxDisplayedResults();
            let results = this.provider.filterResults(providerResults, maxResults);
            let moreCount = Math.max(providerResults.length - results.length, 0);

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
                this._setMoreCount(this.provider.canLaunchSearch ? moreCount : 0);
                this.actor.show();
                callback();
            });
        }
    }
});

var ListSearchResults = new Lang.Class({
    Name: 'ListSearchResults',
    Extends: SearchResultsBase,

    _init(provider, resultsView) {
        this.parent(provider, resultsView);

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
        this._container.add(this._content, { expand: true });

        this._resultDisplayBin.set_child(this._container);
    },

    _setMoreCount(count) {
        this.providerInfo.setMoreCount(count);
    },

    _getMaxDisplayedResults() {
        return MAX_LIST_SEARCH_RESULTS_ROWS;
    },

    _clearResultDisplay() {
        this._content.remove_all_children();
    },

    _createResultDisplay(meta) {
        return this.parent(meta, this._resultsView) ||
               new ListSearchResult(this.provider, meta, this._resultsView);
    },

    _addItem(display) {
        this._content.add_actor(display.actor);
    },

    getFirstResult() {
        if (this._content.get_n_children() > 0)
            return this._content.get_child_at_index(0)._delegate;
        else
            return null;
    }
});
Signals.addSignalMethods(ListSearchResults.prototype);

var GridSearchResults = new Lang.Class({
    Name: 'GridSearchResults',
    Extends: SearchResultsBase,

    _init(provider, resultsView) {
        this.parent(provider, resultsView);
        // We need to use the parent container to know how much results we can show.
        // None of the actors in this class can be used for that, since the main actor
        // goes hidden when no results are displayed, and then it lost its allocation.
        // Then on the next use of _getMaxDisplayedResults allocation is 0, en therefore
        // it doesn't show any result although we have some.
        this._parentContainer = resultsView.actor;

        this._grid = new IconGrid.IconGrid({ rowLimit: MAX_GRID_SEARCH_RESULTS_ROWS,
                                             xAlign: St.Align.START });
        this._bin = new St.Bin({ x_align: St.Align.MIDDLE });
        this._bin.set_child(this._grid.actor);

        this._resultDisplayBin.set_child(this._bin);
    },

    _getMaxDisplayedResults() {
        let parentThemeNode = this._parentContainer.get_theme_node();
        let availableWidth = parentThemeNode.adjust_for_width(this._parentContainer.width);
        return this._grid.columnsForWidth(availableWidth) * this._grid.getRowLimit();
    },

    _clearResultDisplay() {
        this._grid.removeAll();
    },

    _createResultDisplay(meta) {
        return this.parent(meta, this._resultsView) ||
               new GridSearchResult(this.provider, meta, this._resultsView);
    },

    _addItem(display) {
        this._grid.addItem(display);
    },

    getFirstResult() {
        if (this._grid.visibleItemsCount() > 0)
            return this._grid.getItemAtIndex(0)._delegate;
        else
            return null;
    }
});
Signals.addSignalMethods(GridSearchResults.prototype);

var SearchResults = new Lang.Class({
    Name: 'SearchResults',

    _init() {
        this.actor = new St.BoxLayout({ name: 'searchResults',
                                        vertical: true });

        this._content = new St.BoxLayout({ name: 'searchResultsContent',
                                           vertical: true });
        this._contentBin = new MaxWidthBin({ name: 'searchResultsBin',
                                             x_fill: true,
                                             y_fill: true,
                                             child: this._content });

        let scrollChild = new St.BoxLayout();
        scrollChild.add(this._contentBin, { expand: true });

        this._scrollView = new St.ScrollView({ x_fill: true,
                                               y_fill: false,
                                               overlay_scrollbars: true,
                                               style_class: 'search-display vfade' });
        this._scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        this._scrollView.add_actor(scrollChild);
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
        this._reloadRemoteProviders();
    },

    _reloadRemoteProviders() {
        let remoteProviders = this._providers.filter(p => p.isRemoteProvider);
        remoteProviders.forEach(provider => {
            this._unregisterProvider(provider);
        });

        RemoteSearch.loadRemoteSearchProviders(this._searchSettings, providers => {
            providers.forEach(this._registerProvider.bind(this));
        });
    },

    _registerProvider(provider) {
        this._providers.push(provider);
        this._ensureProviderDisplay(provider);
    },

    _unregisterProvider(provider) {
        let index = this._providers.indexOf(provider);
        this._providers.splice(index, 1);

        if (provider.display)
            provider.display.destroy();
    },

    _gotResults(results, provider) {
        this._results[provider.id] = results;
        this._updateResults(provider, results);
    },

    _clearSearchTimeout() {
        if (this._searchTimeoutId > 0) {
            GLib.source_remove(this._searchTimeoutId);
            this._searchTimeoutId = 0;
        }
    },

    _reset() {
        this._terms = [];
        this._results = {};
        this._clearDisplay();
        this._clearSearchTimeout();
        this._defaultResult = null;
        this._startingSearch = false;

        this._updateSearchProgress();
    },

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
    },

    _onSearchTimeout() {
        this._searchTimeoutId = 0;
        this._doSearch();
        return GLib.SOURCE_REMOVE;
    },

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
    },

    _onPan(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollView.vscroll.adjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    _keyFocusIn(provider, actor) {
        Util.ensureActorVisibleInScrollView(this._scrollView, actor);
    },

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
    },

    _clearDisplay() {
        this._providers.forEach(provider => {
            provider.display.clear();
        });
    },

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
    },

    get searchInProgress() {
        if (this._startingSearch)
            return true;

        return this._providers.some(p => p.searchInProgress);
    },

    _updateSearchProgress() {
        let haveResults = this._providers.some(provider => {
            let display = provider.display;
            return (display.getFirstResult() != null);
        });

        this._scrollView.visible = haveResults;
        this._statusBin.visible = !haveResults;

        if (!haveResults) {
            if (this.searchInProgress) {
                this._statusText.set_text(_("Searching…"));
            } else {
                this._statusText.set_text(_("No results."));
            }
        }
    },

    _updateResults(provider, results) {
        let terms = this._terms;
        let display = provider.display;

        display.updateSearch(results, terms, () => {
            provider.searchInProgress = false;

            this._maybeSetInitialSelection();
            this._updateSearchProgress();
        });
    },

    activateDefault() {
        // If we have a search queued up, force the search now.
        if (this._searchTimeoutId > 0)
            this._doSearch();

        if (this._defaultResult)
            this._defaultResult.activate();
    },

    highlightDefault(highlight) {
        this._highlightDefault = highlight;
        this._setSelected(this._defaultResult, highlight);
    },

    popupMenuDefault() {
        // If we have a search queued up, force the search now.
        if (this._searchTimeoutId > 0)
            this._doSearch();

        if (this._defaultResult)
            this._defaultResult.actor.popup_menu();
    },

    navigateFocus(direction) {
        let rtl = this.actor.get_text_direction() == Clutter.TextDirection.RTL;
        if (direction == Gtk.DirectionType.TAB_BACKWARD ||
            direction == (rtl ? Gtk.DirectionType.RIGHT
                              : Gtk.DirectionType.LEFT) ||
            direction == Gtk.DirectionType.UP) {
            this.actor.navigate_focus(null, direction, false);
            return;
        }

        let from = this._defaultResult ? this._defaultResult.actor : null;
        this.actor.navigate_focus(from, direction, false);
    },

    _setSelected(result, selected) {
        if (!result)
            return;

        if (selected) {
            result.actor.add_style_pseudo_class('selected');
            Util.ensureActorVisibleInScrollView(this._scrollView, result.actor);
        } else {
            result.actor.remove_style_pseudo_class('selected');
        }
    },

    highlightTerms(description) {
        if (!description)
            return '';

        if (!this._highlightRegex)
            return description;

        return description.replace(this._highlightRegex, '<b>$1</b>');
    }
});
Signals.addSignalMethods(SearchResults.prototype);

var ProviderInfo = new Lang.Class({
    Name: 'ProviderInfo',
    Extends: St.Button,

    PROVIDER_ICON_SIZE: 32,

    _init(provider) {
        this.provider = provider;
        this.parent({ style_class: 'search-provider-icon',
                      reactive: true,
                      can_focus: true,
                      accessible_name: provider.appInfo.get_name(),
                      track_hover: true });

        this._content = new St.BoxLayout({ vertical: false,
                                           style_class: 'list-search-provider-content' });
        this.set_child(this._content);

        let icon = new St.Icon({ icon_size: this.PROVIDER_ICON_SIZE,
                                 gicon: provider.appInfo.get_icon() });

        let detailsBox = new St.BoxLayout({ style_class: 'list-search-provider-details',
                                            vertical: true,
                                            x_expand: true });

        let nameLabel = new St.Label({ text: provider.appInfo.get_name(),
                                       x_align: Clutter.ActorAlign.START });

        this._moreLabel = new St.Label({ x_align: Clutter.ActorAlign.START });

        detailsBox.add_actor(nameLabel);
        detailsBox.add_actor(this._moreLabel);


        this._content.add_actor(icon);
        this._content.add_actor(detailsBox);
    },

    animateLaunch() {
        let appSys = Shell.AppSystem.get_default();
        let app = appSys.lookup_app(this.provider.appInfo.get_id());
        if (app.state == Shell.AppState.STOPPED)
            IconGrid.zoomOutActor(this._content);
    },

    setMoreCount(count) {
        this._moreLabel.text = ngettext("%d more", "%d more", count).format(count);
        this._moreLabel.visible = count > 0;
    }
});
