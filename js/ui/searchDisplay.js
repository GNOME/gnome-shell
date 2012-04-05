// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const St = imports.gi.St;

const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Search = imports.ui.search;

const MAX_SEARCH_RESULTS_ROWS = 1;


const SearchResult = new Lang.Class({
    Name: 'SearchResult',

    _init: function(provider, metaInfo, terms) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this.actor = new St.Button({ style_class: 'search-result',
                                     reactive: true,
                                     x_align: St.Align.START,
                                     y_fill: true });
        this.actor._delegate = this;
        this._dragActorSource = null;

        let content = provider.createResultActor(metaInfo, terms);
        if (content == null) {
            content = new St.Bin({ style_class: 'search-result-content',
                                   reactive: true,
                                   can_focus: true,
                                   track_hover: true });
            let icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                             { createIcon: this.metaInfo['createIcon'] });
            content.set_child(icon.actor);
            this._dragActorSource = icon.icon;
            this.actor.label_actor = icon.label;
        } else {
            if (content._delegate && content._delegate.getDragActorSource)
                this._dragActorSource = content._delegate.getDragActorSource();
        }
        this._content = content;
        this.actor.set_child(content);

        this.actor.connect('clicked', Lang.bind(this, this._onResultClicked));

        let draggable = DND.makeDraggable(this.actor);
        draggable.connect('drag-begin',
                          Lang.bind(this, function() {
                              Main.overview.beginItemDrag(this);
                          }));
        draggable.connect('drag-cancelled',
                          Lang.bind(this, function() {
                              Main.overview.cancelledItemDrag(this);
                          }));
        draggable.connect('drag-end',
                          Lang.bind(this, function() {
                              Main.overview.endItemDrag(this);
                          }));
    },

    setSelected: function(selected) {
        if (selected)
            this._content.add_style_pseudo_class('selected');
        else
            this._content.remove_style_pseudo_class('selected');
    },

    activate: function() {
        this.provider.activateResult(this.metaInfo.id);
        Main.overview.toggle();
    },

    _onResultClicked: function(actor) {
        this.activate();
    },

    getDragActorSource: function() {
        if (this._dragActorSource)
            return this._dragActorSource;
        // not exactly right, but alignment problems are hard to notice
        return this._content;
    },

    getDragActor: function(stageX, stageY) {
        return this.metaInfo['createIcon'](Main.overview.dashIconSize);
    },

    shellWorkspaceLaunch: function(params) {
        if (this.provider.dragActivateResult)
            this.provider.dragActivateResult(this.metaInfo.id, params);
        else
            this.provider.activateResult(this.metaInfo.id, params);
    }
});


const GridSearchResults = new Lang.Class({
    Name: 'GridSearchResults',
    Extends: Search.SearchResultDisplay,

    _init: function(provider, grid) {
        this.parent(provider);

        this._grid = grid || new IconGrid.IconGrid({ rowLimit: MAX_SEARCH_RESULTS_ROWS,
                                                     xAlign: St.Align.START });
        this.actor = new St.Bin({ x_align: St.Align.START });

        this.actor.set_child(this._grid.actor);
        this._width = 0;
        this.actor.connect('notify::width', Lang.bind(this, function() {
            this._width = this.actor.width;
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
                let results = this.getResultsForDisplay();
                if (results.length == 0)
                    return;

                if (provider.async) {
                    provider.getResultMetasAsync(results,
                                                 Lang.bind(this, this.renderResults));
                } else {
                    let metas = provider.getResultMetas(results);
                    this.renderResults(metas);
                }
            }));
        }));
        this._notDisplayedResult = [];
        this._terms = [];
        this._pendingClear = false;
    },

    getResultsForDisplay: function() {
        let alreadyVisible = this._pendingClear ? 0 : this._grid.visibleItemsCount();
        let canDisplay = this._grid.childrenInRow(this._width) * MAX_SEARCH_RESULTS_ROWS
                         - alreadyVisible;

        let numResults = Math.min(this._notDisplayedResult.length, canDisplay);

        return this._notDisplayedResult.splice(0, numResults);
    },

    getVisibleResultCount: function() {
        return this._grid.visibleItemsCount();
    },

    setResults: function(results, terms) {
        // copy the lists
        this._notDisplayedResult = results.slice(0);
        this._terms = terms.slice(0);
        this._pendingClear = true;
    },

    renderResults: function(metas) {
        for (let i = 0; i < metas.length; i++) {
            let display = new SearchResult(this.provider, metas[i], this._terms);
            this._grid.addItem(display.actor);
        }
    },

    clear: function () {
        this._grid.removeAll();
        this._pendingClear = false;
    },

    getFirstResult: function() {
        if (this.getVisibleResultCount() > 0)
            return this._grid.getItemAtIndex(0)._delegate;
        else
            return null;
    }
});

const SearchResults = new Lang.Class({
    Name: 'SearchResults',

    _init: function(searchSystem, openSearchSystem) {
        this._searchSystem = searchSystem;
        this._searchSystem.connect('search-updated', Lang.bind(this, this._updateCurrentResults));
        this._searchSystem.connect('search-completed', Lang.bind(this, this._updateResults));
        this._openSearchSystem = openSearchSystem;

        this.actor = new St.BoxLayout({ name: 'searchResults',
                                        vertical: true });

        this._content = new St.BoxLayout({ name: 'searchResultsContent',
                                           vertical: true });

        let scrollView = new St.ScrollView({ x_fill: true,
                                             y_fill: false,
                                             style_class: 'vfade' });
        scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        scrollView.add_actor(this._content);

        this.actor.add(scrollView, { x_fill: true,
                                     y_fill: false,
                                     expand: true,
                                     x_align: St.Align.START,
                                     y_align: St.Align.START });
        this.actor.connect('notify::mapped', Lang.bind(this,
            function() {
                if (!this.actor.mapped)
                    return;

                let adjustment = scrollView.vscroll.adjustment;
                let direction = Overview.SwipeScrollDirection.VERTICAL;
                Main.overview.setScrollAdjustment(adjustment, direction);
            }));

        this._statusText = new St.Label({ style_class: 'search-statustext' });
        this._content.add(this._statusText);
        this._providers = this._searchSystem.getProviders();
        this._providerMeta = [];
        this._providerMetaResults = {};
        for (let i = 0; i < this._providers.length; i++) {
            this.createProviderMeta(this._providers[i]);
            this._providerMetaResults[this.providers[i].title] = [];
        }
        this._searchProvidersBox = new St.BoxLayout({ style_class: 'search-providers-box' });
        this.actor.add(this._searchProvidersBox);

        this._openSearchProviders = [];
        this._openSearchSystem.connect('changed', Lang.bind(this, this._updateOpenSearchProviderButtons));
        this._updateOpenSearchProviderButtons();

        this._highlightDefault = false;
        this._defaultResult = null;
    },

    _updateOpenSearchProviderButtons: function() {
        for (let i = 0; i < this._openSearchProviders.length; i++)
            this._openSearchProviders[i].actor.destroy();
        this._openSearchProviders = this._openSearchSystem.getProviders();
        for (let i = 0; i < this._openSearchProviders.length; i++)
            this._createOpenSearchProviderButton(this._openSearchProviders[i]);
    },

    _createOpenSearchProviderButton: function(provider) {
        let button = new St.Button({ style_class: 'dash-search-button',
                                     reactive: true,
                                     can_focus: true,
                                     x_fill: true,
                                     y_align: St.Align.MIDDLE });
        let bin = new St.Bin({ x_fill: false,
                               x_align:St.Align.MIDDLE });
        button.connect('clicked', Lang.bind(this, function() {
            this._openSearchSystem.activateResult(provider.id);
        }));
        let title = new St.Label({ text: provider.name,
                                   style_class: 'dash-search-button-label' });

        button.label_actor = title;
        bin.set_child(title);
        button.set_child(bin);
        provider.actor = button;

        button.setSelected = function(selected) {
            if (selected)
                button.add_style_pseudo_class('selected');
            else
                button.remove_style_pseudo_class('selected');
        };
        button.activate = Lang.bind(this, function() {
            this._openSearchSystem.activateResult(provider.id);
        });
        button.actor = button;

        this._searchProvidersBox.add(button);
    },

    createProviderMeta: function(provider) {
        let providerBox = new St.BoxLayout({ style_class: 'search-section',
                                             vertical: true });
        let title = new St.Label({ style_class: 'search-section-header',
                                   text: provider.title });
        providerBox.add(title);

        let resultDisplayBin = new St.Bin({ style_class: 'search-section-results',
                                            x_fill: true,
                                            y_fill: true });
        providerBox.add(resultDisplayBin, { expand: true });
        let resultDisplay = provider.createResultContainerActor();
        if (resultDisplay == null) {
            resultDisplay = new GridSearchResults(provider);
        }
        resultDisplayBin.set_child(resultDisplay.actor);

        this._providerMeta.push({ provider: provider,
                                  actor: providerBox,
                                  resultDisplay: resultDisplay,
                                  hasPendingResults: false });
        this._content.add(providerBox);
    },

    destroyProviderMeta: function(provider) {
        for (let i=0; i < this._providerMeta.length; i++) {
            let meta = this._providerMeta[i];
            if (meta.provider == provider) {
                meta.actor.destroy();
                this._providerMeta.splice(i, 1);
                break;
            }
        }
    },

    _clearDisplay: function() {
        this._visibleResultsCount = 0;
        for (let i = 0; i < this._providerMeta.length; i++) {
            let meta = this._providerMeta[i];
            meta.resultDisplay.clear();
            meta.actor.hide();
        }
    },

    _clearDisplayForProvider: function(provider) {
        let meta = this._metaForProvider(provider);
        meta.resultDisplay.clear();
        meta.actor.hide();
    },

    reset: function() {
        this._searchSystem.reset();
        this._statusText.hide();
        this._clearDisplay();
    },

    startingSearch: function() {
        this.reset();
        this._statusText.set_text(_("Searching..."));
        this._statusText.show();
    },

    doSearch: function (searchString) {
        this._searchSystem.updateSearch(searchString);
    },

    _metaForProvider: function(provider) {
        return this._providerMeta[this._providers.indexOf(provider)];
    },

    _maybeSetInitialSelection: function() {
        let newDefaultResult = null;

        for (let i = 0; i < this._providerMeta.length; i++) {
            let meta = this._providerMeta[i];
            if (meta.hasPendingResults)
                return;

            if (!meta.actor.visible)
                continue;

            let firstResult = meta.resultDisplay.getFirstResult();
            if (firstResult) {
                newDefaultResult = firstResult;
                break; // select this one!
            }
        }

        if (!newDefaultResult)
            newDefaultResult = this._searchProvidersBox.get_first_child();

        if (newDefaultResult != this._defaultResult) {
            if (this._defaultResult)
                this._defaultResult.setSelected(false);
            if (newDefaultResult)
                newDefaultResult.setSelected(this._highlightDefault);

            this._defaultResult = newDefaultResult;
        }
    },

    _updateCurrentResults: function(searchSystem, results) {
        let terms = searchSystem.getTerms();
        let [provider, providerResults] = results;
        let meta = this._metaForProvider(provider);
        meta.hasPendingResults = false;
        this._updateProviderResults(provider, providerResults, terms);
    },

    _updateProviderResults: function(provider, providerResults, terms) {
        let meta = this._metaForProvider(provider);
        if (providerResults.length == 0) {
            this._clearDisplayForProvider(provider);
            meta.resultDisplay.setResults([], []);
        } else {
            this._providerMetaResults[provider.title] = providerResults;
            meta.resultDisplay.setResults(providerResults, terms);
            let results = meta.resultDisplay.getResultsForDisplay();

            if (provider.async) {
                provider.getResultMetasAsync(results, Lang.bind(this,
                    function(metas) {
                        this._clearDisplayForProvider(provider);
                        meta.actor.show();
                        this._content.hide();
                        meta.resultDisplay.renderResults(metas);
                        this._maybeSetInitialSelection();
                        this._content.show();
                    }));
            } else {
                let metas = provider.getResultMetas(results);
                this._clearDisplayForProvider(provider);
                meta.actor.show();
                meta.resultDisplay.renderResults(metas);
            }
        }
        this._maybeSetInitialSelection();
    },

    _updateResults: function(searchSystem, results) {
        if (results.length == 0) {
            this._statusText.set_text(_("No matching results."));
            this._statusText.show();
        } else {
            this._statusText.hide();
        }

        let terms = searchSystem.getTerms();
        this._openSearchSystem.setSearchTerms(terms);

        // To avoid CSS transitions causing flickering when the first search
        // result stays the same, we hide the content while filling in the
        // results.
        this._content.hide();

        for (let i = 0; i < results.length; i++) {
            let [provider, providerResults] = results[i];
            let meta = this._metaForProvider(provider);
            meta.hasPendingResults = provider.async;
            if (!meta.hasPendingResults)
                this._updateProviderResults(provider, providerResults, terms);
        }

        this._content.show();

        return true;
    },

    activateDefault: function() {
        if (this._defaultResult)
            this._defaultResult.activate();
    },

    highlightDefault: function(highlight) {
        this._highlightDefault = highlight;
        if (this._defaultResult)
            this._defaultResult.setSelected(highlight);
    },

    navigateFocus: function(direction) {
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
        if (this._defaultResult) {
            // The default result appears focused, so navigate directly to the
            // next result.
            this.actor.navigate_focus(global.stage.key_focus, direction, false);
        }
    }
});
