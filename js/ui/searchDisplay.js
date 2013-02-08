// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Separator = imports.ui.separator;
const Search = imports.ui.search;
const Util = imports.misc.util;

const MAX_LIST_SEARCH_RESULTS_ROWS = 3;
const MAX_GRID_SEARCH_RESULTS_ROWS = 1;

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

    _init: function(provider, metaInfo, terms) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this.terms = terms;

        this.actor = new St.Button({ reactive: true,
                                     can_focus: true,
                                     track_hover: true,
                                     x_align: St.Align.START,
                                     y_fill: true });

        this.actor._delegate = this;
        this.actor.connect('clicked', Lang.bind(this, this.activate));
    },

    activate: function() {
        this.provider.activateResult(this.metaInfo.id, this.terms);
        Main.overview.toggle();
    },

    setSelected: function(selected) {
        if (selected)
            this.actor.add_style_pseudo_class('selected');
        else
            this.actor.remove_style_pseudo_class('selected');
    }
});

const ListSearchResult = new Lang.Class({
    Name: 'ListSearchResult',
    Extends: SearchResult,

    ICON_SIZE: 64,

    _init: function(provider, metaInfo, terms) {
        this.parent(provider, metaInfo, terms);

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

    _init: function(provider, metaInfo, terms) {
        this.parent(provider, metaInfo, terms);

        this.actor.style_class = 'grid-search-result';

        let content = provider.createResultActor(metaInfo, terms);
        let dragSource = null;

        if (content == null) {
            content = new St.Bin();
            let icon = new IconGrid.BaseIcon(this.metaInfo['name'],
                                             { createIcon: this.metaInfo['createIcon'] });
            content.set_child(icon.actor);
            content.label_actor = icon.label;
            dragSource = icon.icon;
        } else {
            if (content._delegate && content._delegate.getDragActorSource)
                dragSource = content._delegate.getDragActorSource();
        }

        this.actor.set_child(content);
        this.actor.label_actor = content.label_actor;

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

        this._notDisplayedResult = [];
        this._terms = [];
    },

    hasMoreResults: function() {
        return this._notDisplayedResult.length > 0;
    },

    _keyFocusIn: function(icon) {
        this.emit('key-focus-in', icon);
    },

    setResults: function(results, terms) {
        // copy the lists
        this._notDisplayedResult = results.slice(0);
        this._terms = terms.slice(0);
    }
});

const ListSearchResults = new Lang.Class({
    Name: 'ListSearchResults',
    Extends: SearchResultsBase,

    _init: function(provider) {
        this.parent(provider);

        this.actor = new St.BoxLayout({ style_class: 'search-section-content' });
        this.providerIcon = new ProviderIcon(provider);
        this.providerIcon.connect('clicked', Lang.bind(this,
            function() {
                provider.launchSearch(this._terms);
                Main.overview.toggle();
            }));

        this.actor.add(this.providerIcon, { x_fill: false,
                                            y_fill: false,
                                            x_align: St.Align.START,
                                            y_align: St.Align.START });

        this._content = new St.BoxLayout({ style_class: 'list-search-results',
                                           vertical: true });
        this.actor.add(this._content, { expand: true });
    },

    getResultsForDisplay: function() {
        let canDisplay = MAX_LIST_SEARCH_RESULTS_ROWS;
        let newResults = this._notDisplayedResult.splice(0, canDisplay);
        return newResults;
    },

    getVisibleResultCount: function() {
        return this._content.get_n_children();
    },

    renderResults: function(metas) {
        for (let i = 0; i < metas.length; i++) {
            let display = new ListSearchResult(this.provider, metas[i], this._terms);
            display.actor.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
            this._content.add_actor(display.actor);
        }
    },

    clear: function () {
        this._content.destroy_all_children();
    },

    getFirstResult: function() {
        if (this.getVisibleResultCount() > 0)
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
        this.actor = new St.Bin({ x_align: St.Align.MIDDLE });

        this.actor.set_child(this._grid.actor);
    },

    getResultsForDisplay: function() {
        let canDisplay = this._grid.childrenInRow(this.actor.width) * this._grid.getRowLimit();
        let newResults = this._notDisplayedResult.splice(0, canDisplay);
        return newResults;
    },

    getVisibleResultCount: function() {
        return this._grid.visibleItemsCount();
    },

    renderResults: function(metas) {
        for (let i = 0; i < metas.length; i++) {
            let display = new GridSearchResult(this.provider, metas[i], this._terms);
            display.actor.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));
            this._grid.addItem(display.actor);
        }
    },

    clear: function () {
        this._grid.removeAll();
    },

    getFirstResult: function() {
        if (this.getVisibleResultCount() > 0)
            return this._grid.getItemAtIndex(0)._delegate;
        else
            return null;
    }
});
Signals.addSignalMethods(GridSearchResults.prototype);

const SearchResults = new Lang.Class({
    Name: 'SearchResults',

    _init: function(searchSystem) {
        this._searchSystem = searchSystem;
        this._searchSystem.connect('search-updated', Lang.bind(this, this._updateResults));

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
        this._providers = this._searchSystem.getProviders();
        this._providerMeta = [];
        for (let i = 0; i < this._providers.length; i++) {
            this.createProviderMeta(this._providers[i]);
        }

        this._highlightDefault = false;
        this._defaultResult = null;
    },

    _onPan: function(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollView.vscroll.adjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    _keyFocusIn: function(provider, icon) {
        Util.ensureActorVisibleInScrollView(this._scrollView, icon);
    },

    createProviderMeta: function(provider) {
        let providerBox = new St.BoxLayout({ style_class: 'search-section',
                                             vertical: true });
        let providerIcon = null;
        let resultDisplay = null;

        if (provider.appInfo) {
            resultDisplay = new ListSearchResults(provider);
            providerIcon = resultDisplay.providerIcon;
        } else {
            resultDisplay = new GridSearchResults(provider);
        }

        resultDisplay.connect('key-focus-in', Lang.bind(this, this._keyFocusIn));

        let resultDisplayBin = new St.Bin({ child: resultDisplay.actor,
                                            x_fill: true,
                                            y_fill: true });
        providerBox.add(resultDisplayBin, { expand: true });

        let separator = new Separator.HorizontalSeparator({ style_class: 'search-section-separator' });
        providerBox.add(separator.actor);

        this._providerMeta.push({ provider: provider,
                                  actor: providerBox,
                                  icon: providerIcon,
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
        this._statusBin.hide();
        this._clearDisplay();
        this._defaultResult = null;
    },

    startingSearch: function() {
        this.reset();
        this._statusText.set_text(_("Searching…"));
        this._statusBin.show();
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
            this._statusText.set_text(_("No results."));
            this._statusBin.show();
        } else {
            this._statusBin.hide();
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

            if (meta.icon)
                meta.icon.moreIcon.visible =
                    meta.resultDisplay.hasMoreResults() &&
                    provider.canLaunchSearch;

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
