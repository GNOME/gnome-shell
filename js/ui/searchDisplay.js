// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

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
                                   track_hover: true,
                                   accessible_role: Atk.Role.PUSH_BUTTON });
            let icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                             { createIcon: this.metaInfo['createIcon'] });
            content.set_child(icon.actor);
            this._dragActorSource = icon.icon;
            content.label_actor = icon.label;
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

                provider.getResultMetas(results, Lang.bind(this, this.renderResults));
            }));
        }));
        this._notDisplayedResult = [];
        this._terms = [];
        this._pendingClear = false;
    },

    getResultsForDisplay: function() {
        let alreadyVisible = this._pendingClear ? 0 : this._grid.visibleItemsCount();
        let canDisplay = this._grid.childrenInRow(this._width) * this._grid.getRowLimit()
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

    _init: function(searchSystem) {
        this._searchSystem = searchSystem;
        this._searchSystem.connect('search-updated', Lang.bind(this, this._updateResults));

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
        for (let i = 0; i < this._providers.length; i++) {
            this.createProviderMeta(this._providers[i]);
        }
        this._searchProvidersBox = new St.BoxLayout({ style_class: 'search-providers-box' });
        this.actor.add(this._searchProvidersBox);

        this._highlightDefault = false;
        this._defaultResult = null;
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
        let resultDisplay = new GridSearchResults(provider);
        resultDisplayBin.set_child(resultDisplay.actor);

        this._providerMeta.push({ provider: provider,
                                  actor: providerBox,
                                  resultDisplay: resultDisplay });
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

    _updateStatusText: function () {
        let haveResults = false;

        for (let i = 0; i < this._providerMeta.length; ++i)
            if (this._providerMeta[i].resultDisplay.getFirstResult()) {
                haveResults = true;
                break;
            }

        if (!haveResults) {
            this._statusText.set_text(_("No matching results."));
            this._statusText.show();
        } else {
            this._statusText.hide();
        }
    },

    _updateResults: function(searchSystem, results) {
        let terms = searchSystem.getTerms();
        let [provider, providerResults] = results;
        let meta = this._metaForProvider(provider);

        if (providerResults.length == 0) {
            this._clearDisplayForProvider(provider);
            meta.resultDisplay.setResults([], []);
            this._maybeSetInitialSelection();
            this._updateStatusText();
        } else {
            meta.resultDisplay.setResults(providerResults, terms);
            let results = meta.resultDisplay.getResultsForDisplay();

            provider.getResultMetas(results, Lang.bind(this, function(metas) {
                this._clearDisplayForProvider(provider);
                meta.actor.show();

                // Hiding drops the key focus if we have it
                let focus = global.stage.get_key_focus();
                // To avoid CSS transitions causing flickering when
                // the first search result stays the same, we hide the
                // content while filling in the results.
                this._content.hide();

                meta.resultDisplay.renderResults(metas);
                this._maybeSetInitialSelection();
                this._updateStatusText();

                this._content.show();
                if (this._content.contains(focus))
                    global.stage.set_key_focus(focus);
            }));
        }
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
