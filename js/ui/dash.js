/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
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
const DocDisplay = imports.ui.docDisplay;
const PlaceDisplay = imports.ui.placeDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Search = imports.ui.search;

// 25 search results (per result type) should be enough for everyone
const MAX_RENDERED_SEARCH_RESULTS = 25;

const DEFAULT_PADDING = 4;
const DEFAULT_SPACING = 4;

const BACKGROUND_COLOR = new Clutter.Color();
BACKGROUND_COLOR.from_pixel(0x000000c0);

const PRELIGHT_COLOR = new Clutter.Color();
PRELIGHT_COLOR.from_pixel(0x4f6fadaa);

const TEXT_COLOR = new Clutter.Color();
TEXT_COLOR.from_pixel(0x5f5f5fff);
const BRIGHTER_TEXT_COLOR = new Clutter.Color();
BRIGHTER_TEXT_COLOR.from_pixel(0xbbbbbbff);
const BRIGHT_TEXT_COLOR = new Clutter.Color();
BRIGHT_TEXT_COLOR.from_pixel(0xffffffff);
const SEARCH_TEXT_COLOR = new Clutter.Color();
SEARCH_TEXT_COLOR.from_pixel(0x333333ff);

const SEARCH_CURSOR_COLOR = BRIGHT_TEXT_COLOR;
const HIGHLIGHTED_SEARCH_CURSOR_COLOR = SEARCH_TEXT_COLOR;

const SEARCH_BORDER_BOTTOM_COLOR = new Clutter.Color();
SEARCH_BORDER_BOTTOM_COLOR.from_pixel(0x191919ff);

const BROWSE_ACTIVATED_BG = new Clutter.Color();
BROWSE_ACTIVATED_BG.from_pixel(0x303030f0);

const APPS = "apps";
const PREFS = "prefs";
const DOCS = "docs";
const PLACES = "places";

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
    if (displayType == APPS)
        return new AppDisplay.AppDisplay(false, flags);
    else if (displayType == PREFS)
        return new AppDisplay.AppDisplay(true, flags);
    else if (displayType == DOCS)
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

        this.actor = new St.BoxLayout({ style_class: "dash-pane",
                                         vertical: true,
                                         reactive: true });
        this.actor.connect('button-press-event', Lang.bind(this, function (a, e) {
            // Eat button press events so they don't go through and close the pane
            return true;
        }));

        let chromeTop = new St.BoxLayout();

        let closeIcon = new St.Button({ style_class: "dash-pane-close" });
        closeIcon.connect('clicked', Lang.bind(this, function (b, e) {
            this.close();
        }));
        let dummy = new St.Bin();
        chromeTop.add(dummy, { expand: true });
        chromeTop.add(closeIcon, { x_align: St.Align.END });
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
}
Signals.addSignalMethods(Pane.prototype);

function ResultArea(displayType, flags) {
    this._init(displayType, flags);
}

ResultArea.prototype = {
    _init : function(displayType, flags) {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.resultsContainer = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                              spacing: DEFAULT_PADDING
                                            });
        this.actor.append(this.resultsContainer, Big.BoxPackFlags.EXPAND);

        this.display = _createDisplay(displayType, flags);
        this.resultsContainer.append(this.display.actor, Big.BoxPackFlags.EXPAND);
        this.display.load();
    }
}

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
            dash._addPane(detailPane);
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
}

function SearchEntry() {
    this._init();
}

SearchEntry.prototype = {
    _init : function() {
        this.actor = new St.BoxLayout({ name: "searchEntry",
                                        reactive: true });
        let box = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                y_align: Big.BoxAlignment.CENTER });
        this.actor.add(box, { expand: true });
        this.actor.connect('button-press-event', Lang.bind(this, function () {
            this._resetTextState(true);
            return false;
        }));

        this.pane = null;

        this._defaultText = _("Find...");

        let textProperties = { font_name: "Sans 16px" };
        let entryProperties = { editable: true,
                                activatable: true,
                                single_line_mode: true,
                                color: SEARCH_TEXT_COLOR,
                                cursor_color: SEARCH_CURSOR_COLOR };
        Lang.copyProperties(textProperties, entryProperties);
        this.entry = new Clutter.Text(entryProperties);

        this.entry.connect('notify::text', Lang.bind(this, function () {
            this._resetTextState(false);
        }));
        box.append(this.entry, Big.BoxPackFlags.EXPAND);

        // Mark as editable just to get a cursor
        let defaultTextProperties = { ellipsize: Pango.EllipsizeMode.END,
                                      text: this._defaultText,
                                      editable: true,
                                      color: TEXT_COLOR,
                                      cursor_visible: false,
                                      single_line_mode: true };
        Lang.copyProperties(textProperties, defaultTextProperties);
        this._defaultText = new Clutter.Text(defaultTextProperties);
        box.add_actor(this._defaultText);
        this.entry.connect('notify::allocation', Lang.bind(this, function () {
            this._repositionDefaultText();
        }));

        this._iconBox = new Big.Box({ x_align: Big.BoxAlignment.CENTER,
                                      y_align: Big.BoxAlignment.CENTER,
                                      padding_right: 4 });
        box.append(this._iconBox, Big.BoxPackFlags.END);

        let magnifierUri = "file://" + global.imagedir + "magnifier.svg";
        this._magnifierIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                             magnifierUri, 18, 18);
        let closeUri = "file://" + global.imagedir + "close-black.svg";
        this._closeIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                         closeUri, 18, 18);
        this._closeIcon.reactive = true;
        this._closeIcon.connect('button-press-event', Lang.bind(this, function () {
            // Resetting this.entry.text will trigger notify::text signal which will
            // result in this._resetTextState() being called, but we should not rely
            // on that not short-circuiting if the text was already empty, so we call
            // this._resetTextState() explicitly in that case.
            if (this.entry.text == '')
                this._resetTextState(false);
            else
                this.entry.text = '';

            // Return true to stop the signal emission, so that this.actor doesn't get
            // the button-press-event and re-highlight itself.
            return true;
        }));
        this._repositionDefaultText();
        this._resetTextState();
    },

    setPane: function (pane) {
        this._pane = pane;
    },

    reset: function () {
        this.entry.text = '';
    },

    getText: function () {
        return this.entry.text;
    },

    _resetTextState: function (searchEntryClicked) {
        let text = this.getText();
        this._iconBox.remove_all();
        // We highlight the search box if the user starts typing in it
        // or just clicks in it to indicate that the search is active.
        if (text != '' || searchEntryClicked) {
            if (!searchEntryClicked)
                this._defaultText.hide();
            this._iconBox.append(this._closeIcon, Big.BoxPackFlags.NONE);
            this.actor.set_style_pseudo_class('active');
            this.entry.cursor_color = HIGHLIGHTED_SEARCH_CURSOR_COLOR;
        } else {
            this._defaultText.show();
            this._iconBox.append(this._magnifierIcon, Big.BoxPackFlags.NONE);
            this.actor.set_style_pseudo_class(null);
            this.entry.cursor_color = SEARCH_CURSOR_COLOR;
        }
    },

    _repositionDefaultText: function () {
        // Offset a little to show the cursor
        this._defaultText.set_position(this.entry.x + 4, this.entry.y);
        this._defaultText.set_size(this.entry.width, this.entry.height);
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
    },

    setSelected: function(selected) {
        this._content.set_style_pseudo_class(selected ? 'selected' : null);
    },

    activate: function() {
        this.provider.activateResult(this.metaInfo.id);
        Main.overview.toggle();
    },

    _onResultClicked: function(actor, event) {
        this.activate();
    }
}

function OverflowSearchResults(provider) {
    this._init(provider);
}

OverflowSearchResults.prototype = {
    __proto__: Search.SearchResultDisplay.prototype,

    _init: function(provider) {
        Search.SearchResultDisplay.prototype._init.call(this, provider);
        this.actor = new St.OverflowBox({ style_class: 'dash-search-section-list-results' });
    },

    renderResults: function(results, terms) {
        for (let i = 0; i < results.length && i < MAX_RENDERED_SEARCH_RESULTS; i++) {
            let result = results[i];
            let meta = this.provider.getResultMeta(result);
            let display = new SearchResult(this.provider, meta, terms);
            this.actor.add_actor(display.actor);
        }
    },

    getVisibleCount: function() {
        return this.actor.get_n_visible();
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
    }
}

function SearchResults(searchSystem) {
    this._init(searchSystem);
}

SearchResults.prototype = {
    _init: function(searchSystem) {
        this._searchSystem = searchSystem;

        this.actor = new St.BoxLayout({ name: 'dashSearchResults',
                                        vertical: true });
        this._searchingNotice = new St.Label({ style_class: 'dash-search-starting',
                                               text: _("Searching...") });
        this.actor.add(this._searchingNotice);
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
        this._searchingNotice.hide();
        this._clearDisplay();
    },

    startingSearch: function() {
        this.reset();
        this._searchingNotice.show();
    },

    _metaForProvider: function(provider) {
        return this._providerMeta[this._providers.indexOf(provider)];
    },

    updateSearch: function (searchString) {
        let results = this._searchSystem.updateSearch(searchString);

        this._searchingNotice.hide();
        this._clearDisplay();

        let terms = this._searchSystem.getTerms();

        for (let i = 0; i < results.length; i++) {
            let [provider, providerResults] = results[i];
            let meta = this._metaForProvider(provider);
            meta.actor.show();
            meta.resultDisplay.renderResults(providerResults, terms);
            meta.count.set_text(""+providerResults.length);
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
            index = resultDisplay.getVisibleCount() - 1;
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
        let children = resultDisplay.actor.get_children();
        let targetActor = children[resultDisplay.getSelectionIndex()];
        targetActor._delegate.activate();
    }
}

function MoreLink() {
    this._init();
}

MoreLink.prototype = {
    _init : function () {
        this.actor = new St.BoxLayout({ style_class: "more-link",
                                        reactive: true });
        this.pane = null;

        let expander = new St.Bin({ style_class: "more-link-expander" });
        this.actor.add(expander, { expand: true, y_fill: false });

        this.actor.connect('button-press-event', Lang.bind(this, function (b, e) {
            if (this.pane == null) {
                // Ensure the pane is created; the activated handler will call setPane
                this.emit('activated');
            }
            this._pane.toggle();
            return true;
        }));
    },

    setPane: function (pane) {
        this._pane = pane;
        this._pane.connect('open-state-changed', Lang.bind(this, function(pane, isOpen) {
        }));
    }
}

Signals.addSignalMethods(MoreLink.prototype);

function BackLink() {
    this._init();
}

BackLink.prototype = {
    _init : function () {
        this.actor = new St.Button({ style_class: "section-header-back",
                                      reactive: true });
        this.actor.set_child(new St.Bin({ style_class: "section-header-back-image" }));
    }
}

function SectionHeader(title, suppressBrowse) {
    this._init(title, suppressBrowse);
}

SectionHeader.prototype = {
    _init : function (title, suppressBrowse) {
        this.actor = new St.Bin({ style_class: "section-header",
                                  x_align: St.Align.START,
                                  x_fill: true,
                                  y_fill: true });
        this._innerBox = new St.BoxLayout({ style_class: "section-header-inner" });
        this.actor.set_child(this._innerBox);

        this.backLink = new BackLink();
        this._innerBox.add(this.backLink.actor);
        this.backLink.actor.hide();
        this.backLink.actor.connect('clicked', Lang.bind(this, function (actor) {
            this.emit('back-link-activated');
        }));

        let textBox = new St.BoxLayout({ style_class: "section-text-content" });
        this.text = new St.Label({ style_class: "section-title",
                                   text: title });
        textBox.add(this.text, { x_align: St.Align.START });

        this.countText = new St.Label({ style_class: "section-count" });
        textBox.add(this.countText, { expand: true, x_fill: false, x_align: St.Align.END });
        this.countText.hide();

        this._innerBox.add(textBox, { expand: true });

        if (!suppressBrowse) {
            this.moreLink = new MoreLink();
            this._innerBox.add(this.moreLink.actor, { x_align: St.Align.END });
        }
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
        if (countText == "") {
            this.countText.hide();
        } else {
            this.countText.show();
            this.countText.text = countText;
        }
    }
}

Signals.addSignalMethods(SectionHeader.prototype);

function SearchSectionHeader(title, onClick) {
    this._init(title, onClick);
}

SearchSectionHeader.prototype = {
    _init : function(title, onClick) {
        this.actor = new St.Button({ style_class: "dash-search-section-header",
                                      x_fill: true,
                                      y_fill: true });
        let box = new St.BoxLayout();
        this.actor.set_child(box);
        let titleText = new St.Label({ style_class: "dash-search-section-title",
                                        text: title });
        this.countText = new St.Label({ style_class: "dash-search-section-count" });

        box.add(titleText);
        box.add(this.countText, { expand: true, x_fill: false, x_align: St.Align.END });

        this.actor.connect('clicked', onClick);
    }
}

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
}

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
        this.actor = new St.BoxLayout({ name: "dash",
                                        vertical: true,
                                        reactive: true });

        // The searchArea just holds the entry
        this.searchArea = new St.BoxLayout({ name: "dashSearchArea",
                                             vertical: true });
        this.sectionArea = new St.BoxLayout({ name: "dashSections",
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

        this._searchTimeoutId = 0;
        this._searchEntry.entry.connect('text-changed', Lang.bind(this, function (se, prop) {
            let text = this._searchEntry.getText();
            text = text.replace(/^\s+/g, "").replace(/\s+$/g, "");
            let searchPreviouslyActive = this._searchActive;
            this._searchActive = text != '';
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
        this._searchEntry.entry.connect('key-press-event', Lang.bind(this, function (se, e) {
            let symbol = e.get_key_symbol();
            if (symbol == Clutter.Escape) {
                // Escape will keep clearing things back to the desktop.
                // If we have an active search, we remove it.
                if (this._searchActive)
                    this._searchEntry.reset();
                // Next, if we're in one of the "more" modes or showing the details pane, close them
                else if (this._activePane != null)
                    this._activePane.close();
                // Finally, just close the Overview entirely
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
        }));

        /***** Applications *****/

        this._appsSection = new Section(_("APPLICATIONS"));
        let appWell = new AppDisplay.AppWell();
        this._appsSection.content.add(appWell.actor, { expand: true });

        this._moreAppsPane = null;
        this._appsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreAppsPane == null) {
                this._moreAppsPane = new ResultPane(this);
                this._moreAppsPane.packResults(APPS);
                this._addPane(this._moreAppsPane);
                link.setPane(this._moreAppsPane);
           }
        }));

        this.sectionArea.add(this._appsSection.actor);

        /***** Places *****/

        /* Translators: This is in the sense of locations for documents,
           network locations, etc. */
        this._placesSection = new Section(_("PLACES"), true);
        let placesDisplay = new PlaceDisplay.DashPlaceDisplay();
        this._placesSection.content.add(placesDisplay.actor, { expand: true });
        this.sectionArea.add(this._placesSection.actor);

        /***** Documents *****/

        this._docsSection = new Section(_("RECENT DOCUMENTS"));

        this._docDisplay = new DocDisplay.DashDocDisplay();
        this._docsSection.content.add(this._docDisplay.actor, { expand: true });

        this._moreDocsPane = null;
        this._docsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreDocsPane == null) {
                this._moreDocsPane = new ResultPane(this);
                this._moreDocsPane.packResults(DOCS);
                this._addPane(this._moreDocsPane);
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

    _doSearch: function () {
        this._searchTimeoutId = 0;
        let text = this._searchEntry.getText();
        this.searchResults.updateSearch(text);

        return false;
    },

    show: function() {
        global.stage.set_key_focus(this._searchEntry.entry);
    },

    hide: function() {
        this._firstSelectAfterOverlayShow = true;
        this._searchEntry.reset();
        if (this._activePane != null)
            this._activePane.close();
    },

    closePanes: function () {
        if (this._activePane != null)
            this._activePane.close();
    },

    _addPane: function(pane) {
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
        Main.overview.addPane(pane);
    }
};
Signals.addSignalMethods(Dash.prototype);
