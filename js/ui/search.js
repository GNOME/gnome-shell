// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const AppDisplay = imports.ui.appDisplay;
const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const RemoteSearch = imports.ui.remoteSearch;
const Separator = imports.ui.separator;
const Search = imports.ui.search;
const Util = imports.misc.util;

const SEARCH_PROVIDERS_SCHEMA = 'org.gnome.desktop.search-providers';

const MAX_LIST_SEARCH_RESULTS_ROWS = 3;
const MAX_GRID_SEARCH_RESULTS_ROWS = 1;

const SearchSystem = new Lang.Class({
    Name: 'SearchSystem',

    _init: function() {
        this._providers = [];

        this._registerProvider(new AppDisplay.AppSearchProvider());

        this._searchSettings = new Gio.Settings({ schema: Search.SEARCH_PROVIDERS_SCHEMA });
        this._searchSettings.connect('changed::disabled', Lang.bind(this, this._reloadRemoteProviders));
        this._searchSettings.connect('changed::disable-external', Lang.bind(this, this._reloadRemoteProviders));
        this._searchSettings.connect('changed::sort-order', Lang.bind(this, this._reloadRemoteProviders));

        this._reloadRemoteProviders();
    },

    _shouldUseSearchProvider: function(provider) {
        // the disable-external GSetting only affects remote providers
        if (!provider.isRemoteProvider)
            return true;

        if (this._searchSettings.get_boolean('disable-external'))
            return false;

        let appId = provider.appInfo.get_id();
        let disable = this._searchSettings.get_strv('disabled');
        return disable.indexOf(appId) == -1;
    },

    addProvider: function(provider) {
        this._providers.push(provider);
        this.emit('providers-changed');
    },

    _reloadRemoteProviders: function() {
        let remoteProviders = this._providers.filter(function(provider) {
            return provider.isRemoteProvider;
        });
        remoteProviders.forEach(Lang.bind(this, function(provider) {
            this._unregisterProvider(provider);
        }));

        RemoteSearch.loadRemoteSearchProviders(Lang.bind(this, function(provider) {
            if (!this._shouldUseSearchProvider(provider))
                return;

            this._registerProvider(provider);
        }));
        this.emit('providers-changed');
    },

    _registerProvider: function (provider) {
        provider.searchSystem = this;
        this._providers.push(provider);
    },

    _unregisterProvider: function (provider) {
        let index = this._providers.indexOf(provider);
        this._providers.splice(index, 1);
        provider.searchSystem = null;
    },

    getProviders: function() {
        return this._providers;
    },

    getTerms: function() {
        return this._previousTerms;
    },

    reset: function() {
        this._previousTerms = [];
        this._previousResults = [];
    },

    setResults: function(provider, results) {
        let i = this._providers.indexOf(provider);
        if (i == -1)
            return;

        this._previousResults[i] = [provider, results];
        this.emit('search-updated', this._previousResults[i]);
    },

    setTerms: function(terms) {
        if (!terms)
            return;

        let searchString = terms.join(' ');
        let previousSearchString = this._previousTerms.join(' ');
        if (searchString == previousSearchString)
            return;

        let isSubSearch = false;
        if (this._previousTerms.length > 0)
            isSubSearch = searchString.indexOf(previousSearchString) == 0;

        let previousResultsArr = this._previousResults;

        let results = [];
        this._previousTerms = terms;
        this._previousResults = results;

        if (isSubSearch) {
            for (let i = 0; i < this._providers.length; i++) {
                let [provider, previousResults] = previousResultsArr[i];
                results.push([provider, []]);
                provider.getSubsearchResultSet(previousResults, terms);
            }
        } else {
            for (let i = 0; i < this._providers.length; i++) {
                let provider = this._providers[i];
                results.push([provider, []]);
                provider.getInitialResultSet(terms);
            }
        }
    }
});
Signals.addSignalMethods(SearchSystem.prototype);

const MaxWidthBin = new Lang.Class({
    Name: 'MaxWidthBin',
    Extends: St.Bin,

    vfunc_allocate: function(box, flags) {
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

const SearchResult = new Lang.Class({
    Name: 'SearchResult',

    _init: function(provider, metaInfo) {
        this.provider = provider;
        this.metaInfo = metaInfo;

        this.actor = new St.Button({ reactive: true,
                                     can_focus: true,
                                     track_hover: true,
                                     x_align: St.Align.START,
                                     y_fill: true });

        this.actor._delegate = this;
        this.actor.connect('clicked', Lang.bind(this, this._activate));
    },

    _activate: function() {
        this.emit('activate', this.metaInfo.id);
    },

    setSelected: function(selected) {
        if (selected)
            this.actor.add_style_pseudo_class('selected');
        else
            this.actor.remove_style_pseudo_class('selected');
    }
});
Signals.addSignalMethods(SearchResult.prototype);

const ListSearchResult = new Lang.Class({
    Name: 'ListSearchResult',
    Extends: SearchResult,

    ICON_SIZE: 64,

    _init: function(provider, metaInfo) {
        this.parent(provider, metaInfo);

        this.actor.style_class = 'list-search-result';
        this.actor.x_fill = true;

        let content = new St.BoxLayout({ style_class: 'list-search-result-content',
                                         vertical: false });
        this.actor.set_child(content);

        // An icon for, or thumbnail of, content
        let icon = this.metaInfo['createIcon'](this.ICON_SIZE);
        if (icon) {
            content.add(icon);
        }

        let details = new St.BoxLayout({ vertical: true });
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
            let description = new St.Label({ style_class: 'list-search-result-description' });
            description.clutter_text.set_markup(this.metaInfo['description']);
            details.add(description, { x_fill: false,
                                       y_fill: false,
                                       x_align: St.Align.START,
                                       y_align: St.Align.END });
        }
    }
});

const GridSearchResult = new Lang.Class({
    Name: 'GridSearchResult',
    Extends: SearchResult,

    _init: function(provider, metaInfo) {
        this.parent(provider, metaInfo);

        this.actor.style_class = 'grid-search-result';

        let content = provider.createResultObject(metaInfo);
        let dragSource = null;

        if (content == null) {
            let actor = new St.Bin();
            let icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                             { createIcon: this.metaInfo['createIcon'] });
            actor.set_child(icon.actor);
            actor.label_actor = icon.label;
            dragSource = icon.icon;
            content = { actor: actor, icon: icon };
        } else {
            if (content._delegate && content._delegate.getDragActorSource)
                dragSource = content._delegate.getDragActorSource();
        }

        this.actor.set_child(content.actor);
        this.actor.label_actor = content.actor.label_actor;
        this.icon = content.icon;

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

        if (!dragSource)
            // not exactly right, but alignment problems are hard to notice
            dragSource = content;
        this._dragActorSource = dragSource;
    },

    getDragActorSource: function() {
        return this._dragActorSource;
    },

    getDragActor: function() {
        return this.metaInfo['createIcon'](Main.overview.dashIconSize);
    },

    shellWorkspaceLaunch: function(params) {
        if (this.provider.dragActivateResult)
            this.provider.dragActivateResult(this.metaInfo.id, params);
        else
            this.provider.activateResult(this.metaInfo.id, this.terms);
    }
});

const SearchResultsBase = new Lang.Class({
    Name: 'SearchResultsBase',

    _init: function(provider) {
        this.provider = provider;

        this._terms = [];

        this.actor = new St.BoxLayout({ style_class: 'search-section',
                                        vertical: true });

        this._resultDisplayBin = new St.Bin({ x_fill: true,
                                              y_fill: true });
        this.actor.add(this._resultDisplayBin, { expand: true });

        let separator = new Separator.HorizontalSeparator({ style_class: 'search-section-separator' });
        this.actor.add(separator.actor);

        this._resultDisplays = {};
    },

    destroy: function() {
        this.actor.destroy();
        this._terms = [];
    },

    _clearResultDisplay: function() {
    },

    clear: function() {
        this._resultDisplays = {};
        this._clearResultDisplay();
        this.actor.hide();
    },

    _keyFocusIn: function(actor) {
        this.emit('key-focus-in', actor);
    },

    _activateResult: function(result, id) {
        return this.provider.activateResult(id, this._terms);
    },

    _setMoreIconVisible: function(visible) {
    },

    _ensureResultActors: function(results, callback) {
        let metasNeeded = results.filter(Lang.bind(this, function(resultId) {
            return this._resultDisplays[resultId] === undefined;
        }));

        if (metasNeeded.length === 0) {
            callback();
        } else {
            this.provider.getResultMetas(metasNeeded, Lang.bind(this, function(metas) {
                metasNeeded.forEach(Lang.bind(this, function(resultId, i) {
                    let meta = metas[i];
                    let display = this._createResultDisplay(meta);
                    display.connect('activate', Lang.bind(this, this._activateResult));
                    display.actor.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
                    this._resultDisplays[resultId] = display;
                }));
                callback();
            }));
        }
    },

    updateSearch: function(providerResults, terms, callback) {
        this._terms = terms;

        if (providerResults.length == 0) {
            this._clearResultDisplay();
            this.actor.hide();
            callback();
        } else {
            let maxResults = this._getMaxDisplayedResults();
            let results = this.provider.filterResults(providerResults, maxResults);
            let hasMoreResults = results.length < providerResults.length;

            this._ensureResultActors(results, Lang.bind(this, function() {
                this._clearResultDisplay();

                // To avoid CSS transitions causing flickering when
                // the first search result stays the same, we hide the
                // content while filling in the results.
                this.actor.hide();
                this._clearResultDisplay();
                results.forEach(Lang.bind(this, function(resultId) {
                    this._addItem(this._resultDisplays[resultId]);
                }));
                this._setMoreIconVisible(hasMoreResults && this.provider.canLaunchSearch);
                this.actor.show();
                callback();
            }));
        }
    }
});

const ListSearchResults = new Lang.Class({
    Name: 'ListSearchResults',
    Extends: SearchResultsBase,

    _init: function(provider) {
        this.parent(provider);

        this._container = new St.BoxLayout({ style_class: 'search-section-content' });
        this.providerIcon = new ProviderIcon(provider);
        this.providerIcon.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
        this.providerIcon.connect('clicked', Lang.bind(this,
            function() {
                provider.launchSearch(this._terms);
                Main.overview.toggle();
            }));

        this._container.add(this.providerIcon, { x_fill: false,
                                                 y_fill: false,
                                                 x_align: St.Align.START,
                                                 y_align: St.Align.START });

        this._content = new St.BoxLayout({ style_class: 'list-search-results',
                                           vertical: true });
        this._container.add(this._content, { expand: true });

        this._resultDisplayBin.set_child(this._container);
    },

    _setMoreIconVisible: function(visible) {
        this.providerIcon.moreIcon.visible = true;
    },

    _getMaxDisplayedResults: function() {
        return MAX_LIST_SEARCH_RESULTS_ROWS;
    },

    _clearResultDisplay: function () {
        this._content.remove_all_children();
    },

    _createResultDisplay: function(meta) {
        return new ListSearchResult(this.provider, meta);
    },

    _addItem: function(display) {
        this._content.add_actor(display.actor);
    },

    getFirstResult: function() {
        if (this._content.get_n_children() > 0)
            return this._content.get_child_at_index(0)._delegate;
        else
            return null;
    }
});
Signals.addSignalMethods(ListSearchResults.prototype);

const GridSearchResults = new Lang.Class({
    Name: 'GridSearchResults',
    Extends: SearchResultsBase,

    _init: function(provider) {
        this.parent(provider);

        this._grid = new IconGrid.IconGrid({ rowLimit: MAX_GRID_SEARCH_RESULTS_ROWS,
                                             xAlign: St.Align.START });
        this._bin = new St.Bin({ x_align: St.Align.MIDDLE });
        this._bin.set_child(this._grid.actor);

        this._resultDisplayBin.set_child(this._bin);
    },

    _getMaxDisplayedResults: function() {
        return this._grid.columnsForWidth(this._bin.width) * this._grid.getRowLimit();
    },

    _renderResults: function(metas) {
        for (let i = 0; i < metas.length; i++) {
            let display = new GridSearchResult(this.provider, metas[i]);
            display.connect('activate', Lang.bind(this, this._activateResult));
            display.actor.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
            this._grid.addItem(display);
        }
    },

    _clearResultDisplay: function () {
        this._grid.removeAll();
    },

    _createResultDisplay: function(meta) {
        return new GridSearchResult(this.provider, meta);
    },

    _addItem: function(display) {
        this._grid.addItem(display.actor);
    },

    getFirstResult: function() {
        if (this._grid.visibleItemsCount() > 0)
            return this._grid.getItemAtIndex(0)._delegate;
        else
            return null;
    }
});
Signals.addSignalMethods(GridSearchResults.prototype);

const SearchResults = new Lang.Class({
    Name: 'SearchResults',

    _init: function() {
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
        action.connect('pan', Lang.bind(this, this._onPan));
        this._scrollView.add_action(action);

        this.actor.add(this._scrollView, { x_fill: true,
                                           y_fill: true,
                                           expand: true,
                                           x_align: St.Align.START,
                                           y_align: St.Align.START });

        this._statusText = new St.Label({ style_class: 'search-statustext' });
        this._statusBin = new St.Bin({ x_align: St.Align.MIDDLE,
                                       y_align: St.Align.MIDDLE });
        this._content.add(this._statusBin, { expand: true });
        this._statusBin.add_actor(this._statusText);

        this._highlightDefault = false;
        this._defaultResult = null;

        this._searchSystem = new SearchSystem();
        this._searchSystem.connect('search-updated', Lang.bind(this, this._updateResults));
        this._searchSystem.connect('providers-changed', Lang.bind(this, this._updateProviderDisplays));
        this._updateProviderDisplays();
    },

    _onPan: function(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollView.vscroll.adjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    _keyFocusIn: function(provider, actor) {
        Util.ensureActorVisibleInScrollView(this._scrollView, actor);
    },

    _ensureProviderDisplay: function(provider) {
        if (provider.display)
            return;

        let providerDisplay;
        if (provider.appInfo)
            providerDisplay = new ListSearchResults(provider);
        else
            providerDisplay = new GridSearchResults(provider);

        providerDisplay.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
        this._content.add(providerDisplay.actor);
        provider.display = providerDisplay;
    },

    _updateProviderDisplays: function() {
        this._searchSystem.getProviders().forEach(Lang.bind(this, this._ensureProviderDisplay));
    },

    _clearDisplay: function() {
        this._searchSystem.getProviders().forEach(function(provider) {
            provider.display.clear();
        });
    },

    reset: function() {
        this._searchSystem.reset();
        this._statusBin.hide();
        this._clearDisplay();
        this._defaultResult = null;
    },

    startingSearch: function() {
        this.reset();
        this._statusText.set_text(_("Searching…"));
        this._statusBin.show();
    },

    setTerms: function(terms) {
        this._searchSystem.setTerms(terms);
    },

    _maybeSetInitialSelection: function() {
        let newDefaultResult = null;

        let providers = this._searchSystem.getProviders();
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
            if (this._defaultResult)
                this._defaultResult.setSelected(false);
            if (newDefaultResult)
                newDefaultResult.setSelected(this._highlightDefault);

            this._defaultResult = newDefaultResult;
        }
    },

    _updateStatusText: function () {
        let haveResults = this._searchSystem.getProviders().some(function(provider) {
            let display = provider.display;
            return (display.getFirstResult() != null);
        });

        if (!haveResults) {
            this._statusText.set_text(_("No results."));
            this._statusBin.show();
        } else {
            this._statusBin.hide();
        }
    },

    _updateResults: function(searchSystem, results) {
        let terms = searchSystem.getTerms();
        let [provider, providerResults] = results;
        let display = provider.display;

        display.updateSearch(providerResults, terms, Lang.bind(this, function() {
            this._maybeSetInitialSelection();
            this._updateStatusText();
        }));
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
    }
});

const ProviderIcon = new Lang.Class({
    Name: 'ProviderIcon',
    Extends: St.Button,

    PROVIDER_ICON_SIZE: 48,

    _init: function(provider) {
        this.provider = provider;
        this.parent({ style_class: 'search-provider-icon',
                      reactive: true,
                      can_focus: true,
                      accessible_name: provider.appInfo.get_name(),
                      track_hover: true });

        this._content = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this.set_child(this._content);

        let rtl = (this.get_text_direction() == Clutter.TextDirection.RTL);

        this.moreIcon = new St.Widget({ style_class: 'search-provider-icon-more',
                                        visible: false,
                                        x_align: rtl ? Clutter.ActorAlign.START : Clutter.ActorAlign.END,
                                        y_align: Clutter.ActorAlign.END,
                                        x_expand: true,
                                        y_expand: true });

        let icon = new St.Icon({ icon_size: this.PROVIDER_ICON_SIZE,
                                 gicon: provider.appInfo.get_icon() });
        this._content.add_actor(icon);
        this._content.add_actor(this.moreIcon);
    }
});
