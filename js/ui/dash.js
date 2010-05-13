/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const DND = imports.ui.dnd;
const DocDisplay = imports.ui.docDisplay;
const PlaceDisplay = imports.ui.placeDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Search = imports.ui.search;

// 25 search results (per result type) should be enough for everyone
const MAX_RENDERED_SEARCH_RESULTS = 25;

const DOCS = 'docs';
const PLACES = 'places';

/*
 * Returns the index in an array of a given length that is obtained
 * if the provided index is incremented by an increment and the array
 * is wrapped in if necessary.
 *
 * index: prior index, expects 0 <= index < length
 * increment: the change in index, expects abs(increment) <= length
 * length: the length of the array
 */
function _getIndexWrapped(index, increment, length) {
   return (index + increment + length) % length;
}

function _createDisplay(displayType, flags) {
    if (displayType == DOCS)
        return new DocDisplay.DocDisplay(flags);
    else if (displayType == PLACES)
        return new PlaceDisplay.PlaceDisplay(flags);
    return null;
}

function Pane() {
    this._init();
}

Pane.prototype = {
    _init: function () {
        this._open = false;

        this.actor = new St.BoxLayout({ style_class: 'dash-pane',
                                         vertical: true,
                                         reactive: true });
        this.actor.connect('button-press-event', Lang.bind(this, function (a, e) {
            // Eat button press events so they don't go through and close the pane
            return true;
        }));

        let chromeTop = new St.BoxLayout();

        let dummy = new St.Bin();
        chromeTop.add(dummy, { expand: true });
        this.actor.add(chromeTop);

        this.content = new St.BoxLayout({ vertical: true });
        this.actor.add(this.content, { expand: true });

        // Hidden by default
        this.actor.hide();
    },

    open: function () {
        if (this._open)
            return;
        this._open = true;
        this.actor.show();
        this.emit('open-state-changed', this._open);
    },

    close: function () {
        if (!this._open)
            return;
        this._open = false;
        this.actor.hide();
        this.emit('open-state-changed', this._open);
    },

    destroyContent: function() {
        let children = this.content.get_children();
        for (let i = 0; i < children.length; i++) {
            children[i].destroy();
        }
    },

    toggle: function () {
        if (this._open)
            this.close();
        else
            this.open();
    }
};
Signals.addSignalMethods(Pane.prototype);

function ResultArea(displayType, flags) {
    this._init(displayType, flags);
}

ResultArea.prototype = {
    _init : function(displayType, flags) {
        this.actor = new St.BoxLayout({ vertical: true });
        this.resultsContainer = new St.BoxLayout({ style_class: 'dash-results-container' });
        this.actor.add(this.resultsContainer, { expand: true });

        this.display = _createDisplay(displayType, flags);
        this.resultsContainer.add(this.display.actor, { expand: true });
        this.display.load();
    }
};

// Utility function shared between ResultPane and the DocDisplay in the main dash.
// Connects to the detail signal of the display, and on-demand creates a new
// pane.
function createPaneForDetails(dash, display) {
    let detailPane = null;
    display.connect('show-details', Lang.bind(this, function(display, index) {
        if (detailPane == null) {
            detailPane = new Pane();
            detailPane.connect('open-state-changed', Lang.bind(this, function (pane, isOpen) {
                if (!isOpen) {
                    /* Ensure we don't keep around large preview textures */
                    detailPane.destroyContent();
                }
            }));
            dash._addPane(detailPane, St.Align.START);
        }

        if (index >= 0) {
            detailPane.destroyContent();
            let details = display.createDetailsForIndex(index);
            detailPane.content.add(details, { expand: true });
            detailPane.open();
        } else {
            detailPane.close();
        }
    }));
    return null;
}

function ResultPane(dash) {
    this._init(dash);
}

ResultPane.prototype = {
    __proto__: Pane.prototype,

    _init: function(dash) {
        Pane.prototype._init.call(this);
        this._dash = dash;
    },

    // Create a display of displayType and pack it into this pane's
    // content area.  Return the display.
    packResults: function(displayType) {
        let resultArea = new ResultArea(displayType);

        createPaneForDetails(this._dash, resultArea.display);

        this.content.add(resultArea.actor, { expand: true });
        this.connect('open-state-changed', Lang.bind(this, function(pane, isOpen) {
            resultArea.display.resetState();
        }));
        return resultArea.display;
    }
};

function SearchEntry() {
    this._init();
}

SearchEntry.prototype = {
    _init : function() {
        this.actor = new St.Entry({ name: 'searchEntry',
                                    hint_text: _("Find") });
        this.entry = this.actor.clutter_text;

        this.actor.clutter_text.connect('text-changed', Lang.bind(this,
            function() {
                if (this.isActive())
                    this.actor.set_secondary_icon_from_file(global.imagedir +
                                                            'close-black.svg');
                else
                    this.actor.set_secondary_icon_from_file(null);
            }));
        this.actor.connect('secondary-icon-clicked', Lang.bind(this,
            function() {
                this.reset();
            }));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this.pane = null;

        this._capturedEventId = 0;
    },

    show: function() {
        if (this._capturedEventId == 0)
            this._capturedEventId = global.stage.connect('captured-event',
                                 Lang.bind(this, this._onCapturedEvent));
        this.entry.set_cursor_visible(true);
        this.entry.set_selection(0, 0);
    },

    hide: function() {
        if (this.isActive())
            this.reset();
        if (this._capturedEventId > 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
    },

    setPane: function (pane) {
        this._pane = pane;
    },

    reset: function () {
        this.entry.text = '';
        global.stage.set_key_focus(null);
        this.entry.set_cursor_visible(true);
        this.entry.set_selection(0, 0);
    },

    getText: function () {
        return this.entry.get_text().replace(/^\s+/g, '').replace(/\s+$/g, '');
    },

    // some search term has been entered
    isActive: function() {
        return this.actor.get_text() != '';
    },

    // the entry does not show the hint
    _isActivated: function() {
        return this.entry.text == this.actor.get_text();
    },

    _onCapturedEvent: function(actor, event) {
        let source = event.get_source();
        let panelEvent = false;

        if (source) {
            let parent = source;
            do {
                if (parent == Main.panel.actor)
                    break;
            } while ((parent = parent.get_parent()) != null);
            panelEvent = (parent != null);
        }

        switch (event.type()) {
            case Clutter.EventType.BUTTON_PRESS:
                // the user clicked outside after activating the entry, but
                // with no search term entered - cancel the search
                if (source != this.entry && this.entry.text == '') {
                    this.reset();
                    // allow only panel events to continue
                    return !panelEvent;
                }
                return false;
            case Clutter.EventType.KEY_PRESS:
                // If neither the stage nor our entry have key focus, some
                // "special" actor grabbed the focus (run dialog, looking
                // glass); we don't want to interfere with that
                let focus = global.stage.get_key_focus();
                if (focus != global.stage && focus != this.entry)
                    return false;

                let sym = event.get_key_symbol();

                // If we have an active search, Escape cancels it - if we
                // haven't, the key is ignored
                if (sym == Clutter.Escape)
                    if (this._isActivated()) {
                        this.reset();
                        return true;
                    } else {
                        return false;
                    }

                 // Ignore non-printable keys
                 if (!Clutter.keysym_to_unicode(sym))
                     return false;

                // Search started - move the key focus to the entry and
                // "repeat" the event
                if (!this._isActivated()) {
                    global.stage.set_key_focus(this.entry);
                    this.entry.event(event, false);
                }

                return false;
            default:
                // Suppress all other events outside the panel while the entry
                // is activated and no search has been entered - any click
                // outside the entry will cancel the search
                return (this.entry.text == '' && !panelEvent);
        }
    },

    _onDestroy: function() {
        if (this._capturedEventId > 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
    }
};
Signals.addSignalMethods(SearchEntry.prototype);

function SearchResult(provider, metaInfo, terms) {
    this._init(provider, metaInfo, terms);
}

SearchResult.prototype = {
    _init: function(provider, metaInfo, terms) {
        this.provider = provider;
        this.metaInfo = metaInfo;
        this.actor = new St.Clickable({ style_class: 'dash-search-result',
                                        reactive: true,
                                        x_align: St.Align.START,
                                        x_fill: true,
                                        y_fill: true });
        this.actor._delegate = this;

        let content = provider.createResultActor(metaInfo, terms);
        if (content == null) {
            content = new St.BoxLayout({ style_class: 'dash-search-result-content' });
            let title = new St.Label({ text: this.metaInfo['name'] });
            let icon = this.metaInfo['icon'];
            content.add(icon, { y_fill: false });
            content.add(title, { expand: true, y_fill: false });
        }
        this._content = content;
        this.actor.set_child(content);

        this.actor.connect('clicked', Lang.bind(this, this._onResultClicked));

        let draggable = DND.makeDraggable(this.actor);
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

function OverflowSearchResults(provider) {
    this._init(provider);
}

OverflowSearchResults.prototype = {
    __proto__: Search.SearchResultDisplay.prototype,

    _init: function(provider) {
        Search.SearchResultDisplay.prototype._init.call(this, provider);
        this.actor = new St.OverflowBox({ style_class: 'dash-search-section-list-results' });
    },

    getVisibleResultCount: function() {
        return this.actor.get_n_visible();
    },

    renderResults: function(results, terms) {
        for (let i = 0; i < results.length && i < MAX_RENDERED_SEARCH_RESULTS; i++) {
            let result = results[i];
            let meta = this.provider.getResultMeta(result);
            let display = new SearchResult(this.provider, meta, terms);
            this.actor.add_actor(display.actor);
        }
    },

    selectIndex: function(index) {
        let nVisible = this.actor.get_n_visible();
        let children = this.actor.get_children();
        if (this.selectionIndex >= 0) {
            let prevActor = children[this.selectionIndex];
            prevActor._delegate.setSelected(false);
        }
        this.selectionIndex = -1;
        if (index >= nVisible)
            return false;
        else if (index < 0)
            return false;
        let targetActor = children[index];
        targetActor._delegate.setSelected(true);
        this.selectionIndex = index;
        return true;
    },

    activateSelected: function() {
        let children = this.actor.get_children();
        let targetActor = children[this.selectionIndex];
        targetActor._delegate.activate();
    }
};

function SearchResults(searchSystem) {
    this._init(searchSystem);
}

SearchResults.prototype = {
    _init: function(searchSystem) {
        this._searchSystem = searchSystem;

        this.actor = new St.BoxLayout({ name: 'dashSearchResults',
                                        vertical: true });
        this._statusText = new St.Label({ style_class: 'dash-search-statustext' });
        this.actor.add(this._statusText);
        this._selectedProvider = -1;
        this._providers = this._searchSystem.getProviders();
        this._providerMeta = [];
        for (let i = 0; i < this._providers.length; i++) {
            let provider = this._providers[i];
            let providerBox = new St.BoxLayout({ style_class: 'dash-search-section',
                                                  vertical: true });
            let titleButton = new St.Button({ style_class: 'dash-search-section-header',
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

            let resultDisplayBin = new St.Bin({ style_class: 'dash-search-section-results',
                                                x_fill: true,
                                                y_fill: true });
            providerBox.add(resultDisplayBin, { expand: true });
            let resultDisplay = provider.createResultContainerActor();
            if (resultDisplay == null) {
                resultDisplay = new OverflowSearchResults(provider);
            }
            resultDisplayBin.set_child(resultDisplay.actor);

            this._providerMeta.push({ actor: providerBox,
                                      resultDisplay: resultDisplay,
                                      count: count });
            this.actor.add(providerBox);
        }
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

function MoreLink() {
    this._init();
}

MoreLink.prototype = {
    _init : function () {
        this.actor = new St.BoxLayout({ style_class: 'more-link',
                                        reactive: true });
        this.pane = null;

        this._expander = new St.Bin({ style_class: 'more-link-expander' });
        this.actor.add(this._expander, { expand: true, y_fill: false });
    },

    activate: function() {
        if (this.pane == null) {
            // Ensure the pane is created; the activated handler will call setPane
            this.emit('activated');
        }
        this._pane.toggle();
        return true;
    },

    setPane: function (pane) {
        this._pane = pane;
        this._pane.connect('open-state-changed', Lang.bind(this, function(pane, isOpen) {
            if (isOpen)
                this._expander.add_style_class_name('open');
            else
                this._expander.remove_style_class_name('open');
        }));
    }
};

Signals.addSignalMethods(MoreLink.prototype);

function BackLink() {
    this._init();
}

BackLink.prototype = {
    _init : function () {
        this.actor = new St.Button({ style_class: 'section-header-back',
                                      reactive: true });
        this.actor.set_child(new St.Bin({ style_class: 'section-header-back-image' }));
    }
};

function SectionHeader(title, suppressBrowse) {
    this._init(title, suppressBrowse);
}

SectionHeader.prototype = {
    _init : function (title, suppressBrowse) {
        this.actor = new St.Bin({ style_class: 'section-header',
                                  x_align: St.Align.START,
                                  x_fill: true,
                                  y_fill: true,
                                  reactive: !suppressBrowse });
        this._innerBox = new St.BoxLayout({ style_class: 'section-header-inner' });
        this.actor.set_child(this._innerBox);

        this.backLink = new BackLink();
        this._innerBox.add(this.backLink.actor);
        this.backLink.actor.hide();
        this.backLink.actor.connect('clicked', Lang.bind(this, function (actor) {
            this.emit('back-link-activated');
        }));

        let textBox = new St.BoxLayout({ style_class: 'section-text-content' });
        this.text = new St.Label({ style_class: 'section-title',
                                   text: title });
        textBox.add(this.text, { x_align: St.Align.START });

        this.countText = new St.Label({ style_class: 'section-count' });
        textBox.add(this.countText, { expand: true, x_fill: false, x_align: St.Align.END });
        this.countText.hide();

        this._innerBox.add(textBox, { expand: true });

        if (!suppressBrowse) {
            this.moreLink = new MoreLink();
            this._innerBox.add(this.moreLink.actor, { x_align: St.Align.END });
            this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        }
    },

    _onButtonPress: function() {
        this.moreLink.activate();
    },

    setTitle : function(title) {
        this.text.text = title;
    },

    setBackLinkVisible : function(visible) {
        if (visible)
            this.backLink.actor.show();
        else
            this.backLink.actor.hide();
    },

    setMoreLinkVisible : function(visible) {
        if (visible)
            this.moreLink.actor.show();
        else
            this.moreLink.actor.hide();
    },

    setCountText : function(countText) {
        if (countText == '') {
            this.countText.hide();
        } else {
            this.countText.show();
            this.countText.text = countText;
        }
    }
};

Signals.addSignalMethods(SectionHeader.prototype);

function SearchSectionHeader(title, onClick) {
    this._init(title, onClick);
}

SearchSectionHeader.prototype = {
    _init : function(title, onClick) {
        this.actor = new St.Button({ style_class: 'dash-search-section-header',
                                      x_fill: true,
                                      y_fill: true });
        let box = new St.BoxLayout();
        this.actor.set_child(box);
        let titleText = new St.Label({ style_class: 'dash-search-section-title',
                                        text: title });
        this.countText = new St.Label({ style_class: 'dash-search-section-count' });

        box.add(titleText);
        box.add(this.countText, { expand: true, x_fill: false, x_align: St.Align.END });

        this.actor.connect('clicked', onClick);
    }
};

function Section(titleString, suppressBrowse) {
    this._init(titleString, suppressBrowse);
}

Section.prototype = {
    _init: function(titleString, suppressBrowse) {
        this.actor = new St.BoxLayout({ style_class: 'dash-section',
                                         vertical: true });
        this.header = new SectionHeader(titleString, suppressBrowse);
        this.actor.add(this.header.actor);
        this.content = new St.BoxLayout({ style_class: 'dash-section-content',
                                           vertical: true });
        this.actor.add(this.content);
    }
};

function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        // dash and the popup panes need to be reactive so that the clicks in unoccupied places on them
        // are not passed to the transparent background underneath them. This background is used for the workspaces area when
        // the additional dash panes are being shown and it handles clicks by closing the additional panes, so that the user
        // can interact with the workspaces. However, this behavior is not desirable when the click is actually over a pane.
        //
        // We have to make the individual panes reactive instead of making the whole dash actor reactive because the width
        // of the Group actor ends up including the width of its hidden children, so we were getting a reactive object as
        // wide as the details pane that was blocking the clicks to the workspaces underneath it even when the details pane
        // was actually hidden.
        this.actor = new St.BoxLayout({ name: 'dash',
                                        vertical: true,
                                        reactive: true });

        // The searchArea just holds the entry
        this.searchArea = new St.BoxLayout({ name: 'dashSearchArea',
                                             vertical: true });
        this.sectionArea = new St.BoxLayout({ name: 'dashSections',
                                               vertical: true });

        this.actor.add(this.searchArea);
        this.actor.add(this.sectionArea);

        // The currently active popup display
        this._activePane = null;

        /***** Search *****/

        this._searchActive = false;
        this._searchPending = false;
        this._searchEntry = new SearchEntry();
        this.searchArea.add(this._searchEntry.actor, { y_fill: false, expand: true });

        this._searchSystem = new Search.SearchSystem();
        this._searchSystem.registerProvider(new AppDisplay.AppSearchProvider());
        this._searchSystem.registerProvider(new AppDisplay.PrefsSearchProvider());
        this._searchSystem.registerProvider(new PlaceDisplay.PlaceSearchProvider());
        this._searchSystem.registerProvider(new DocDisplay.DocSearchProvider());

        this.searchResults = new SearchResults(this._searchSystem);
        this.actor.add(this.searchResults.actor);
        this.searchResults.actor.hide();

        this._keyPressId = 0;
        this._searchTimeoutId = 0;
        this._searchEntry.entry.connect('text-changed', Lang.bind(this, function (se, prop) {
            let searchPreviouslyActive = this._searchActive;
            this._searchActive = this._searchEntry.isActive();
            this._searchPending = this._searchActive && !searchPreviouslyActive;
            if (this._searchPending) {
                this.searchResults.startingSearch();
            }
            if (this._searchActive) {
                this.searchResults.actor.show();
                this.sectionArea.hide();
            } else {
                this.searchResults.actor.hide();
                this.sectionArea.show();
            }
            if (!this._searchActive) {
                if (this._searchTimeoutId > 0) {
                    Mainloop.source_remove(this._searchTimeoutId);
                    this._searchTimeoutId = 0;
                }
                return;
            }
            if (this._searchTimeoutId > 0)
                return;
            this._searchTimeoutId = Mainloop.timeout_add(150, Lang.bind(this, this._doSearch));
        }));
        this._searchEntry.entry.connect('activate', Lang.bind(this, function (se) {
            if (this._searchTimeoutId > 0) {
                Mainloop.source_remove(this._searchTimeoutId);
                this._doSearch();
            }
            this.searchResults.activateSelected();
            return true;
        }));

        /***** Applications *****/

        this._appsSection = new Section(_("APPLICATIONS"));
        let appWell = new AppDisplay.AppWell();
        this._appsSection.content.add(appWell.actor, { expand: true });

        this._allApps = null;
        this._appsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._allApps == null) {
                this._allApps = new AppDisplay.AllAppDisplay();
                this._addPane(this._allApps, St.Align.START);
                link.setPane(this._allApps);
           }
        }));

        this.sectionArea.add(this._appsSection.actor);

        /***** Places *****/

        /* Translators: This is in the sense of locations for documents,
           network locations, etc. */
        this._placesSection = new Section(_("PLACES & DEVICES"), true);
        let placesDisplay = new PlaceDisplay.DashPlaceDisplay();
        this._placesSection.content.add(placesDisplay.actor, { expand: true });
        this.sectionArea.add(this._placesSection.actor);

        /***** Documents *****/

        this._docsSection = new Section(_("RECENT ITEMS"));

        this._docDisplay = new DocDisplay.DashDocDisplay();
        this._docsSection.content.add(this._docDisplay.actor, { expand: true });

        this._moreDocsPane = null;
        this._docsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreDocsPane == null) {
                this._moreDocsPane = new ResultPane(this);
                this._moreDocsPane.packResults(DOCS);
                this._addPane(this._moreDocsPane, St.Align.END);
                link.setPane(this._moreDocsPane);
           }
        }));

        this._docDisplay.connect('changed', Lang.bind(this, function () {
            this._docsSection.header.setMoreLinkVisible(
                this._docDisplay.actor.get_children().length > 0);
        }));
        this._docDisplay.emit('changed');

        this.sectionArea.add(this._docsSection.actor, { expand: true });
    },

    _onKeyPress: function(stage, event) {
        // If neither the stage nor the search entry have key focus, some
        // "special" actor grabbed the focus (run dialog, looking glass);
        // we don't want to interfere with that
        let focus = stage.get_key_focus();
        if (focus != stage && focus != this._searchEntry.entry)
            return false;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.Escape) {
            // If we're in one of the "more" modes or showing the
            // details pane, close them
            if (this._activePane != null)
                this._activePane.close();
            // Otherwise, just close the Overview entirely
            else
                Main.overview.hide();
            return true;
        } else if (symbol == Clutter.Up) {
            if (!this._searchActive)
                return true;
            this.searchResults.selectUp(false);

            return true;
        } else if (symbol == Clutter.Down) {
            if (!this._searchActive)
                return true;

            this.searchResults.selectDown(false);
            return true;
        }
        return false;
    },

    _doSearch: function () {
        this._searchTimeoutId = 0;
        let text = this._searchEntry.getText();
        this.searchResults.updateSearch(text);

        return false;
    },

    show: function() {
        this._searchEntry.show();
        if (this._keyPressId == 0)
            this._keyPressId = global.stage.connect('key-press-event',
                                                    Lang.bind(this, this._onKeyPress));
    },

    hide: function() {
        this._firstSelectAfterOverlayShow = true;
        this._searchEntry.hide();
        if (this._activePane != null)
            this._activePane.close();
        if (this._keyPressId > 0) {
            global.stage.disconnect(this._keyPressId);
            this._keyPressId = 0;
        }
    },

    closePanes: function () {
        if (this._activePane != null)
            this._activePane.close();
    },

    _addPane: function(pane, align) {
        pane.connect('open-state-changed', Lang.bind(this, function (pane, isOpen) {
            if (isOpen) {
                if (pane != this._activePane && this._activePane != null) {
                    this._activePane.close();
                }
                this._activePane = pane;
            } else if (pane == this._activePane) {
                this._activePane = null;
            }
        }));
        Main.overview.addPane(pane, align);
    }
};
Signals.addSignalMethods(Dash.prototype);
