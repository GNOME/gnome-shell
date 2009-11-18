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
const Button = imports.ui.button;
const Main = imports.ui.main;

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

        this._backgroundGradient = null;
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));
        this.actor.connect('notify::allocation', Lang.bind(this, function (actor) {
            if (!this._backgroundGradient)
                return;
            this._onStyleChanged();
        }));

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

    _onStyleChanged: function () {
        if (this._backgroundGradient) {
            this._backgroundGradient.destroy();
        }
        // Manually implement the gradient
        let themeNode = this.actor.get_theme_node();
        let gradientTopColor = new Clutter.Color();
        if (!themeNode.get_color("-shell-gradient-top", false, gradientTopColor))
            return;
        let gradientBottomColor = new Clutter.Color();
        if (!themeNode.get_color("-shell-gradient-bottom", false, gradientBottomColor))
            return;
        this._backgroundGradient = Shell.create_vertical_gradient(gradientTopColor,
                                                                   gradientBottomColor);
        let box = this.actor.allocation;
        let contentBox = new Clutter.ActorBox();
        themeNode.get_content_box(box, contentBox);
        let width = contentBox.x2 - contentBox.x1;
        let height = contentBox.y2 - contentBox.y1;
        this._backgroundGradient.set_size(width, height);
        // This will set a fixed position, which puts us outside of the normal box layout
        this._backgroundGradient.set_position(0, 0);

        this._innerBox.add_actor(this._backgroundGradient);
        this._backgroundGradient.lower_bottom();
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

        // Size for this one explicitly set from overlay.js
        this.searchArea = new Big.Box({ y_align: Big.BoxAlignment.CENTER });

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
        this.searchArea.append(this._searchEntry.actor, Big.BoxPackFlags.EXPAND);

        this._searchTimeoutId = 0;
        this._searchEntry.entry.connect('text-changed', Lang.bind(this, function (se, prop) {
            let text = this._searchEntry.getText();
            text = text.replace(/^\s+/g, "").replace(/\s+$/g, "")
            let searchPreviouslyActive = this._searchActive;
            this._searchActive = text != '';
            this._searchPending = this._searchActive && !searchPreviouslyActive;
            this._updateDashActors();
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
            // Only one of the displays will have an item selected, so it's ok to
            // call activateSelected() on all of them.
            for (var i = 0; i < this._searchSections.length; i++) {
                let section = this._searchSections[i];
                section.resultArea.display.activateSelected();
            }
            return true;
        }));
        this._searchEntry.entry.connect('key-press-event', Lang.bind(this, function (se, e) {
            let text = this._searchEntry.getText();
            let symbol = e.get_key_symbol();
            if (symbol == Clutter.Escape) {
                // Escape will keep clearing things back to the desktop.
                // If we are showing a particular section of search, go back to all sections.
                if (this._searchResultsSingleShownSection != null)
                    this._showAllSearchSections();
                // If we have an active search, we remove it.
                else if (this._searchActive)
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
                // selectUp and selectDown wrap around in their respective displays
                // too, but there doesn't seem to be any flickering if we first select
                // something in one display, but then unset the selection, and move
                // it to the other display, so it's ok to do that.
                for (var i = 0; i < this._searchSections.length; i++) {
                    let section = this._searchSections[i];
                    if (section.resultArea.display.hasSelected() && !section.resultArea.display.selectUp()) {
                        if (this._searchResultsSingleShownSection != section.type) {
                            // We need to move the selection to the next section above this section that has items,
                            // wrapping around at the bottom, if necessary.
                            let newSectionIndex = this._findAnotherSectionWithItems(i, -1);
                            if (newSectionIndex >= 0) {
                                this._searchSections[newSectionIndex].resultArea.display.selectLastItem();
                                section.resultArea.display.unsetSelected();
                            }
                        }
                        break;
                    }
                }
                return true;
            } else if (symbol == Clutter.Down) {
                if (!this._searchActive)
                    return true;
                for (var i = 0; i < this._searchSections.length; i++) {
                    let section = this._searchSections[i];
                    if (section.resultArea.display.hasSelected() && !section.resultArea.display.selectDown()) {
                        if (this._searchResultsSingleShownSection != section.type) {
                            // We need to move the selection to the next section below this section that has items,
                            // wrapping around at the top, if necessary.
                            let newSectionIndex = this._findAnotherSectionWithItems(i, 1);
                            if (newSectionIndex >= 0) {
                                this._searchSections[newSectionIndex].resultArea.display.selectFirstItem();
                                section.resultArea.display.unsetSelected();
                            }
                        }
                        break;
                    }
                }
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

        /***** Search Results *****/

        this._searchResultsSection = new Section(_("SEARCH RESULTS"), true);

        this._searchResultsSingleShownSection = null;

        this._searchResultsSection.header.connect('back-link-activated', Lang.bind(this, function () {
            this._showAllSearchSections();
        }));

        this._searchSections = [
            { type: APPS,
              title: _("APPLICATIONS"),
              header: null,
              resultArea: null
            },
            { type: PREFS,
              title: _("PREFERENCES"),
              header: null,
              resultArea: null
            },
            { type: DOCS,
              title: _("RECENT DOCUMENTS"),
              header: null,
              resultArea: null
            },
            { type: PLACES,
              title: _("PLACES"),
              header: null,
              resultArea: null
            }
        ];

        for (var i = 0; i < this._searchSections.length; i++) {
            let section = this._searchSections[i];
            section.header = new SearchSectionHeader(section.title,
                                                     Lang.bind(this,
                                                               function () {
                                                                   this._showSingleSearchSection(section.type);
                                                               }));
            this._searchResultsSection.content.add(section.header.actor);
            section.resultArea = new ResultArea(section.type, GenericDisplay.GenericDisplayFlags.DISABLE_VSCROLLING);
            this._searchResultsSection.content.add(section.resultArea.actor, { expand: true });
            createPaneForDetails(this, section.resultArea.display);
        }

        this.sectionArea.add(this._searchResultsSection.actor, { expand: true });
        this._searchResultsSection.actor.hide();
    },

    _doSearch: function () {
        this._searchTimeoutId = 0;
        let text = this._searchEntry.getText();
        text = text.replace(/^\s+/g, "").replace(/\s+$/g, "");

        let selectionSet = false;

        for (var i = 0; i < this._searchSections.length; i++) {
            let section = this._searchSections[i];
            section.resultArea.display.setSearch(text);
            let itemCount = section.resultArea.display.getMatchedItemsCount();
            let itemCountText = itemCount + "";
            section.header.countText.text = itemCountText;

            if (this._searchResultsSingleShownSection == section.type) {
                this._searchResultsSection.header.setCountText(itemCountText);
                if (itemCount == 0) {
                    section.resultArea.actor.hide();
                } else {
                    section.resultArea.actor.show();
                }
            } else if (this._searchResultsSingleShownSection == null) {
                // Don't show the section if it has no results
                if (itemCount == 0) {
                    section.header.actor.hide();
                    section.resultArea.actor.hide();
                } else {
                    section.header.actor.show();
                    section.resultArea.actor.show();
                }
            }

            // Refresh the selection when a new search is applied.
            section.resultArea.display.unsetSelected();
            if (!selectionSet && section.resultArea.display.hasItems() &&
                (this._searchResultsSingleShownSection == null || this._searchResultsSingleShownSection == section.type)) {
                section.resultArea.display.selectFirstItem();
                selectionSet = true;
            }
        }

        // Here work around a bug that I never quite tracked down
        // the root cause of; it appeared that the search results
        // section was getting a 0 height allocation.
        this._searchResultsSection.content.queue_relayout();

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
    },

    _updateDashActors: function() {
        if (this._searchPending) {
            this._searchResultsSection.actor.show();
            // We initially hide all sections when we start a search. When the search timeout
            // first runs, the sections that have matching results are shown. As the search
            // is refined, only the sections that have matching results will be shown.
            for (let i = 0; i < this._searchSections.length; i++) {
                let section = this._searchSections[i];
                section.header.actor.hide();
                section.resultArea.actor.hide();
            }
            this._appsSection.actor.hide();
            this._placesSection.actor.hide();
            this._docsSection.actor.hide();
        } else if (!this._searchActive) {
            this._showAllSearchSections();
            this._searchResultsSection.actor.hide();
            this._appsSection.actor.show();
            this._placesSection.actor.show();
            this._docsSection.actor.show();
        }
    },

    _showSingleSearchSection: function(type) {
        // We currently don't allow going from showing one section to showing another section.
        if (this._searchResultsSingleShownSection != null) {
            throw new Error("We were already showing a single search section: '" + this._searchResultsSingleShownSection
                            + "' when _showSingleSearchSection() was called for '" + type + "'");
        }
        for (var i = 0; i < this._searchSections.length; i++) {
            let section = this._searchSections[i];
            if (section.type == type) {
                // This will be the only section shown.
                section.resultArea.display.selectFirstItem();
                let itemCount = section.resultArea.display.getMatchedItemsCount();
                let itemCountText = itemCount + "";
                section.header.actor.hide();
                this._searchResultsSection.header.setTitle(section.title);
                this._searchResultsSection.header.setBackLinkVisible(true);
                this._searchResultsSection.header.setCountText(itemCountText);
            } else {
                // We need to hide this section.
                section.header.actor.hide();
                section.resultArea.actor.hide();
                section.resultArea.display.unsetSelected();
            }
        }
        this._searchResultsSingleShownSection = type;
    },

    _showAllSearchSections: function() {
        if (this._searchResultsSingleShownSection != null) {
            let selectionSet = false;
            for (var i = 0; i < this._searchSections.length; i++) {
                let section = this._searchSections[i];
                if (section.type == this._searchResultsSingleShownSection) {
                    // This will no longer be the only section shown.
                    let itemCount = section.resultArea.display.getMatchedItemsCount();
                    if (itemCount != 0) {
                        section.header.actor.show();
                        section.resultArea.display.selectFirstItem();
                        selectionSet = true;
                    }
                    this._searchResultsSection.header.setTitle(_("SEARCH RESULTS"));
                    this._searchResultsSection.header.setBackLinkVisible(false);
                    this._searchResultsSection.header.setCountText("");
                } else {
                    // We need to restore this section.
                    let itemCount = section.resultArea.display.getMatchedItemsCount();
                    if (itemCount != 0) {
                        section.header.actor.show();
                        section.resultArea.actor.show();
                        // This ensures that some other section will have the selection if the
                        // single section that was being displayed did not have any items.
                        if (!selectionSet) {
                            section.resultArea.display.selectFirstItem();
                            selectionSet = true;
                        }
                    }
                }
            }
            this._searchResultsSingleShownSection = null;
        }
    },

    _findAnotherSectionWithItems: function(index, increment) {
        let pos = _getIndexWrapped(index, increment, this._searchSections.length);
        while (pos != index) {
            if (this._searchSections[pos].resultArea.display.hasItems())
                return pos;
            pos = _getIndexWrapped(pos, increment, this._searchSections.length);
        }
        return -1;
    }
};
Signals.addSignalMethods(Dash.prototype);
