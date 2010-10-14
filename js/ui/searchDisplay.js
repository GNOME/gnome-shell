/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;
const Gtk = imports.gi.Gtk;
const St = imports.gi.St;

const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Search = imports.ui.search;

const MAX_SEARCH_RESULTS_ROWS = 2;


function SearchResult(provider, metaInfo, terms) {
    this._init(provider, metaInfo, terms);
}

SearchResult.prototype = {
    _init: function(provider, metaInfo, terms) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this.actor = new St.Clickable({ style_class: 'search-result',
                                        reactive: true,
                                        x_align: St.Align.START,
                                        y_fill: true });
        this.actor._delegate = this;

        let content = provider.createResultActor(metaInfo, terms);
        if (content == null) {
            content = new St.Bin({ style_class: 'search-result-content',
                                   reactive: true,
                                   track_hover: true });
            let icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                             { createIcon: Lang.bind(this, function(size) {
                                                 return this.metaInfo['icon'];
                                             })});
            content.set_child(icon.actor);
        }
        this._content = content;
        this.actor.set_child(content);

        this.actor.connect('clicked', Lang.bind(this, this._onResultClicked));

        let draggable = DND.makeDraggable(this.actor);
        draggable.connect('drag-begin',
                          Lang.bind(this, function() {
                              Main.overview.beginItemDrag(this);
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

    _onResultClicked: function(actor, event) {
        this.activate();
    },

    getDragActorSource: function() {
        return this.metaInfo['icon'];
    },

    getDragActor: function(stageX, stageY) {
        return new Clutter.Clone({ source: this.metaInfo['icon'] });
    },

    shellWorkspaceLaunch: function() {
        if (this.provider.dragActivateResult)
            this.provider.dragActivateResult(this.metaInfo.id);
        else
            this.provider.activateResult(this.metaInfo.id);
    }
};


function GridSearchResults(provider) {
    this._init(provider);
}

GridSearchResults.prototype = {
    __proto__: Search.SearchResultDisplay.prototype,

    _init: function(provider) {
        Search.SearchResultDisplay.prototype._init.call(this, provider);
        this._grid = new IconGrid.IconGrid({ rowLimit: MAX_SEARCH_RESULTS_ROWS,
                                             xAlign: St.Align.START });
        this.actor = new St.Bin({ x_align: St.Align.START });
        this.actor.set_child(this._grid.actor);
        this.selectionIndex = -1;
    },

    getVisibleResultCount: function() {
        return this._grid.visibleItemsCount();
    },

    renderResults: function(results, terms) {
        for (let i = 0; i < results.length; i++) {
            let result = results[i];
            let meta = this.provider.getResultMeta(result);
            let display = new SearchResult(this.provider, meta, terms);
            this._grid.addItem(display.actor);
        }
    },

    clear: function () {
        this._grid.removeAll();
        this.selectionIndex = -1;
    },

    selectIndex: function (index) {
        let nVisible = this.getVisibleResultCount();
        if (this.selectionIndex >= 0) {
            let prevActor = this._grid.getItemAtIndex(this.selectionIndex);
            prevActor._delegate.setSelected(false);
        }
        this.selectionIndex = -1;
        if (index >= nVisible)
            return false;
        else if (index < 0)
            return false;
        let targetActor = this._grid.getItemAtIndex(index);
        targetActor._delegate.setSelected(true);
        this.selectionIndex = index;
        return true;
    },

    activateSelected: function() {
        if (this.selectionIndex < 0)
            return;
        let targetActor = this._grid.getItemAtIndex(this.selectionIndex);
        targetActor._delegate.activate();
    }
};


function SearchResults(searchSystem) {
    this._init(searchSystem);
}

SearchResults.prototype = {
    _init: function(searchSystem) {
        this._searchSystem = searchSystem;

        this.actor = new St.Bin({ name: 'searchResults',
                                  y_align: St.Align.START,
                                  x_align: St.Align.START,
                                  x_fill: true });
        this._content = new St.BoxLayout({ name: 'searchResultsContent',
                                           vertical: true });

        let scrollView = new St.ScrollView({ x_fill: true,
                                             y_fill: false,
                                             vshadows: true });
        scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        scrollView.add_actor(this._content);

        this.actor.set_child(scrollView);

        this._statusText = new St.Label({ style_class: 'search-statustext' });
        this._content.add(this._statusText);
        this._selectedProvider = -1;
        this._providers = this._searchSystem.getProviders();
        this._providerMeta = [];
        for (let i = 0; i < this._providers.length; i++)
            this.createProviderMeta(this._providers[i]);
    },

    createProviderMeta: function(provider) {
        let providerBox = new St.BoxLayout({ style_class: 'search-section',
                                             vertical: true });
        let titleButton = new St.Button({ style_class: 'search-section-header',
                                          reactive: true,
                                          x_fill: true,
                                          y_fill: true });
        titleButton.connect('clicked', Lang.bind(this, function () { this._onHeaderClicked(provider); }));
        providerBox.add(titleButton);
        let titleBox = new St.BoxLayout();
        titleButton.set_child(titleBox);
        let title = new St.Label({ text: provider.title });
        let count = new St.Label();
        titleBox.add(title, { expand: true });
        titleBox.add(count);

        let resultDisplayBin = new St.Bin({ style_class: 'search-section-results',
                                            x_fill: true,
                                            y_fill: true });
        providerBox.add(resultDisplayBin, { expand: true });
        let resultDisplay = provider.createResultContainerActor();
        if (resultDisplay == null) {
            resultDisplay = new GridSearchResults(provider);
        }
        resultDisplayBin.set_child(resultDisplay.actor);

        this._providerMeta.push({ actor: providerBox,
                                  resultDisplay: resultDisplay,
                                  count: count });
        this._content.add(providerBox);
    },

    _clearDisplay: function() {
        this._selectedProvider = -1;
        this._visibleResultsCount = 0;
        for (let i = 0; i < this._providerMeta.length; i++) {
            let meta = this._providerMeta[i];
            meta.resultDisplay.clear();
            meta.actor.hide();
        }
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

    _metaForProvider: function(provider) {
        return this._providerMeta[this._providers.indexOf(provider)];
    },

    updateSearch: function (searchString) {
        let results = this._searchSystem.updateSearch(searchString);

        this._clearDisplay();

        if (results.length == 0) {
            this._statusText.set_text(_("No matching results."));
            this._statusText.show();
            return true;
        } else {
            this._statusText.hide();
        }

        let terms = this._searchSystem.getTerms();

        for (let i = 0; i < results.length; i++) {
            let [provider, providerResults] = results[i];
            let meta = this._metaForProvider(provider);
            meta.actor.show();
            meta.resultDisplay.renderResults(providerResults, terms);
            meta.count.set_text('' + providerResults.length);
        }

        this.selectDown(false);

        return true;
    },

    _onHeaderClicked: function(provider) {
        provider.expandSearch(this._searchSystem.getTerms());
    },

    _modifyActorSelection: function(resultDisplay, up) {
        let success;
        let index = resultDisplay.getSelectionIndex();
        if (up && index == -1)
            index = resultDisplay.getVisibleResultCount() - 1;
        else if (up)
            index = index - 1;
        else
            index = index + 1;
        return resultDisplay.selectIndex(index);
    },

    selectUp: function(recursing) {
        for (let i = this._selectedProvider; i >= 0; i--) {
            let meta = this._providerMeta[i];
            if (!meta.actor.visible)
                continue;
            let success = this._modifyActorSelection(meta.resultDisplay, true);
            if (success) {
                this._selectedProvider = i;
                return;
            }
        }
        if (this._providerMeta.length > 0 && !recursing) {
            this._selectedProvider = this._providerMeta.length - 1;
            this.selectUp(true);
        }
    },

    selectDown: function(recursing) {
        let current = this._selectedProvider;
        if (current == -1)
            current = 0;
        for (let i = current; i < this._providerMeta.length; i++) {
            let meta = this._providerMeta[i];
            if (!meta.actor.visible)
                continue;
            let success = this._modifyActorSelection(meta.resultDisplay, false);
            if (success) {
                this._selectedProvider = i;
                return;
            }
        }
        if (this._providerMeta.length > 0 && !recursing) {
            this._selectedProvider = 0;
            this.selectDown(true);
        }
    },

    activateSelected: function() {
        let current = this._selectedProvider;
        if (current < 0)
            return;
        let meta = this._providerMeta[current];
        let resultDisplay = meta.resultDisplay;
        resultDisplay.activateSelected();
        Main.overview.hide();
    }
};
