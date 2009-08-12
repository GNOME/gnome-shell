/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;

const AppDisplay = imports.ui.appDisplay;
const DocDisplay = imports.ui.docDisplay;
const Places = imports.ui.places;
const GenericDisplay = imports.ui.genericDisplay;
const Button = imports.ui.button;
const Main = imports.ui.main;

const DEFAULT_PADDING = 4;
const DASH_SECTION_PADDING = 6;
const DASH_SECTION_SPACING = 40;
const DASH_CORNER_RADIUS = 5;

const BACKGROUND_COLOR = new Clutter.Color();
BACKGROUND_COLOR.from_pixel(0x000000c0);

const DASH_PADDING_SIDE = 14;

const SEARCH_BORDER_BOTTOM_COLOR = new Clutter.Color();
SEARCH_BORDER_BOTTOM_COLOR.from_pixel(0x191919ff);

const SECTION_BORDER_COLOR = new Clutter.Color();
SECTION_BORDER_COLOR.from_pixel(0x262626ff);
const SECTION_BORDER = 1;
const SECTION_INNER_BORDER_COLOR = new Clutter.Color();
SECTION_INNER_BORDER_COLOR.from_pixel(0x000000ff);
const SECTION_BACKGROUND_TOP_COLOR = new Clutter.Color();
SECTION_BACKGROUND_TOP_COLOR.from_pixel(0x161616ff);
const SECTION_BACKGROUND_BOTTOM_COLOR = new Clutter.Color();
SECTION_BACKGROUND_BOTTOM_COLOR.from_pixel(0x000000ff);
const SECTION_INNER_SPACING = 8;

const BROWSE_ACTIVATED_BG = new Clutter.Color();
BROWSE_ACTIVATED_BG.from_pixel(0x303030f0);

const TEXT_COLOR = new Clutter.Color();
TEXT_COLOR.from_pixel(0x5f5f5fff);
const BRIGHT_TEXT_COLOR = new Clutter.Color();
BRIGHT_TEXT_COLOR.from_pixel(0xffffffff);

const PANE_BORDER_COLOR = new Clutter.Color();
PANE_BORDER_COLOR.from_pixel(0x101d3cfa);
const PANE_BORDER_WIDTH = 2;

const PANE_BACKGROUND_COLOR = new Clutter.Color();
PANE_BACKGROUND_COLOR.from_pixel(0x000000f4);


function Pane() {
    this._init();
}

Pane.prototype = {
    _init: function () {
        this._open = false;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   background_color: PANE_BACKGROUND_COLOR,
                                   border: PANE_BORDER_WIDTH,
                                   border_color: PANE_BORDER_COLOR,
                                   padding: DEFAULT_PADDING,
                                   reactive: true });
        this.actor.connect('button-press-event', Lang.bind(this, function (a, e) {
            // Eat button press events so they don't go through and close the pane
            return true;
        }));

        let chromeTop = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                      spacing: 6 });

        let global = Shell.Global.get();
        let closeIconUri = "file://" + global.imagedir + "close.svg";
        let closeIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                       closeIconUri,
                                                                       16,
                                                                       16);
        closeIcon.reactive = true;
        closeIcon.connect('button-press-event', Lang.bind(this, function (b, e) {
            this.close();
            return true;
        }));
        chromeTop.append(closeIcon, Big.BoxPackFlags.END);
        this.actor.append(chromeTop, Big.BoxPackFlags.NONE);

        this.content = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                     spacing: DEFAULT_PADDING });
        this.actor.append(this.content, Big.BoxPackFlags.EXPAND);

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

function ResultArea(displayClass, enableNavigation) {
    this._init(displayClass, enableNavigation);
}

ResultArea.prototype = {
    _init : function(displayClass, enableNavigation) {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.resultsContainer = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                              spacing: DEFAULT_PADDING
                                            });
        this.actor.append(this.resultsContainer, Big.BoxPackFlags.EXPAND);
        this.navContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.resultsContainer.append(this.navContainer, Big.BoxPackFlags.NONE);

        this.display = new displayClass();

        this.navArea = this.display.getNavigationArea();
        if (enableNavigation && this.navArea)
            this.navContainer.append(this.navArea, Big.BoxPackFlags.EXPAND);

        this.resultsContainer.append(this.display.actor, Big.BoxPackFlags.EXPAND);

        this.controlBox = new Big.Box({ x_align: Big.BoxAlignment.CENTER });
        this.controlBox.append(this.display.displayControl, Big.BoxPackFlags.NONE);
        this.actor.append(this.controlBox, Big.BoxPackFlags.EXPAND);

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
            detailPane.content.append(details, Big.BoxPackFlags.EXPAND);
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

    // Create an instance of displayClass and pack it into this pane's
    // content area.  Return the displayClass instance.
    packResults: function(displayClass, enableNavigation) {
        let resultArea = new ResultArea(displayClass, enableNavigation);

        createPaneForDetails(this._dash, resultArea.display);

        this.content.append(resultArea.actor, Big.BoxPackFlags.EXPAND);
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
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   y_align: Big.BoxAlignment.CENTER,
                                   border_bottom: SECTION_BORDER,
                                   border_color: SEARCH_BORDER_BOTTOM_COLOR });
        this.pane = null;

        this._defaultText = "Find apps or documents";

        let textProperties = { font_name: "Sans 12" };
        let entryProperties = { editable: true,
                                activatable: true,
                                single_line_mode: true,
                                color: BRIGHT_TEXT_COLOR,
                                cursor_color: BRIGHT_TEXT_COLOR,
                                text: '' };
        Lang.copyProperties(textProperties, entryProperties);
        // We need to initialize the text for the entry to have the cursor displayed
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this.entry = new Clutter.Text(entryProperties);
        this.entry.connect('notify::text', Lang.bind(this, function () {
            this._resetTextState();
        }));
        this.actor.append(this.entry, Big.BoxPackFlags.EXPAND);

        // Mark as editable just to get a cursor
        let defaultTextProperties = { ellipsize: Pango.EllipsizeMode.END,
                                      text: "Find apps or documents",
                                      editable: true,
                                      color: TEXT_COLOR,
                                      cursor_visible: false,
                                      single_line_mode: true };
        Lang.copyProperties(textProperties, defaultTextProperties);
        this._defaultText = new Clutter.Text(defaultTextProperties);
        this.actor.add_actor(this._defaultText);
        this.entry.connect('notify::allocation', Lang.bind(this, function () {
            this._repositionDefaultText();
        }));

        this._iconBox = new Big.Box({ x_align: Big.BoxAlignment.CENTER,
                                      y_align: Big.BoxAlignment.CENTER });
        this.actor.append(this._iconBox, Big.BoxPackFlags.END);

        let global = Shell.Global.get();
        let magnifierUri = "file://" + global.imagedir + "magnifier.svg";
        this._magnifierIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                             magnifierUri, 29, 18);
        let closeUri = "file://" + global.imagedir + "close.svg";
        this._closeIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                         closeUri, 18, 18);
        this._closeIcon.reactive = true;
        this._closeIcon.connect('button-press-event', Lang.bind(this, function () {
            this.entry.text = '';
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

    _resetTextState: function () {
        let text = this.getText();
        this._iconBox.remove_all();
        if (text != '') {
            this._defaultText.hide();
            this._iconBox.append(this._closeIcon, Big.BoxPackFlags.NONE);
        } else {
            this._defaultText.show();
            this._iconBox.append(this._magnifierIcon, Big.BoxPackFlags.NONE);
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
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   padding_right: DEFAULT_PADDING,
                                   padding_left: DEFAULT_PADDING,
                                   reactive: true,
                                   x_align: Big.BoxAlignment.CENTER,
                                   y_align: Big.BoxAlignment.CENTER,
                                   border_left: SECTION_BORDER,
                                   border_color: SECTION_BORDER_COLOR });
        this.pane = null;

        let text = new Clutter.Text({ font_name: "Sans 12px",
                                      color: BRIGHT_TEXT_COLOR,
                                      text: "Browse" });
        this.actor.append(text, Big.BoxPackFlags.NONE);

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

function SectionHeader(title, suppressBrowse) {
    this._init(title, suppressBrowse);
}

SectionHeader.prototype = {
    _init : function (title, suppressBrowse) {
        this.actor = new Big.Box({ border: SECTION_BORDER,
                                   border_color: SECTION_BORDER_COLOR });
        this._innerBox = new Big.Box({ border: SECTION_BORDER,
                                       border_color: SECTION_INNER_BORDER_COLOR,
                                       padding_left: DEFAULT_PADDING,
                                       orientation: Big.BoxOrientation.HORIZONTAL });
        this.actor.append(this._innerBox, Big.BoxPackFlags.EXPAND);
        let backgroundGradient = Shell.create_vertical_gradient(SECTION_BACKGROUND_TOP_COLOR,
                                                                SECTION_BACKGROUND_BOTTOM_COLOR);
        this._innerBox.add_actor(backgroundGradient);
        this._innerBox.connect('notify::allocation', Lang.bind(this, function (actor) {
            let [width, height] = actor.get_size();
            backgroundGradient.set_size(width, height);
        }));
        let textBox = new Big.Box({ padding_top: DEFAULT_PADDING,
                                    padding_bottom: DEFAULT_PADDING });
        let text = new Clutter.Text({ color: TEXT_COLOR,
                                      font_name: "Sans Bold 12px",
                                      text: title });
        textBox.append(text, Big.BoxPackFlags.NONE);
        this._innerBox.append(textBox, Big.BoxPackFlags.EXPAND);
        if (!suppressBrowse) {
            this.moreLink = new MoreLink();
            this._innerBox.append(this.moreLink.actor, Big.BoxPackFlags.END);
        }
    }
}

function Section(titleString, suppressBrowse) {
    this._init(titleString, suppressBrowse);
}

Section.prototype = {
    _init: function(titleString, suppressBrowse) {
        this.actor = new Big.Box({ spacing: SECTION_INNER_SPACING });
        this.header = new SectionHeader(titleString, suppressBrowse);
        this.actor.append(this.header.actor, Big.BoxPackFlags.NONE);
        this.content = new Big.Box();
        this.actor.append(this.content, Big.BoxPackFlags.EXPAND);
    }
}

function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        let global = Shell.Global.get();

        // dash and the popup panes need to be reactive so that the clicks in unoccupied places on them
        // are not passed to the transparent background underneath them. This background is used for the workspaces area when
        // the additional dash panes are being shown and it handles clicks by closing the additional panes, so that the user
        // can interact with the workspaces. However, this behavior is not desirable when the click is actually over a pane.
        //
        // We have to make the individual panes reactive instead of making the whole dash actor reactive because the width
        // of the Group actor ends up including the width of its hidden children, so we were getting a reactive object as
        // wide as the details pane that was blocking the clicks to the workspaces underneath it even when the details pane
        // was actually hidden.
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   background_color: BACKGROUND_COLOR,
                                   corner_radius: DASH_CORNER_RADIUS,
                                   padding_left: DASH_PADDING_SIDE,
                                   padding_right: DASH_PADDING_SIDE,
                                   reactive: true });

        // Size for this one explicitly set from overlay.js
        this.searchArea = new Big.Box({ y_align: Big.BoxAlignment.CENTER });

        this.sectionArea = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                         spacing: DASH_SECTION_SPACING });

        this.actor.append(this.searchArea, Big.BoxPackFlags.NONE);
        this.actor.append(this.sectionArea, Big.BoxPackFlags.NONE);

        // The currently active popup display
        this._activePane = null;

        /***** Search *****/

        this._searchPane = null;
        this._searchActive = false;
        this._searchEntry = new SearchEntry();
        this.searchArea.append(this._searchEntry.actor, Big.BoxPackFlags.EXPAND);

        this._searchAreaApps = null;
        this._searchAreaDocs = null;

        this._searchQueued = false;
        this._searchEntry.entry.connect('text-changed', Lang.bind(this, function (se, prop) {
            let text = this._searchEntry.getText();
            this._searchActive = text != '';
            if (this._searchQueued)
                return;
            if (this._searchPane == null) {
                this._searchPane = new ResultPane(this);
                this._searchPane.content.append(new Clutter.Text({ color: TEXT_COLOR,
                                                                   font_name: 'Sans Bold 10px',
                                                                   text: "APPLICATIONS" }),
                                                 Big.BoxPackFlags.NONE);
                this._searchAreaApps = this._searchPane.packResults(AppDisplay.AppDisplay, false);
                this._searchPane.content.append(new Clutter.Text({ color: TEXT_COLOR,
                                                                   font_name: 'Sans Bold 10px',
                                                                   text: "RECENT DOCUMENTS" }),
                                                 Big.BoxPackFlags.NONE);
                this._searchAreaDocs = this._searchPane.packResults(DocDisplay.DocDisplay, false);
                this._addPane(this._searchPane);
                this._searchEntry.setPane(this._searchPane);
            }
            this._searchQueued = true;
            Mainloop.timeout_add(250, Lang.bind(this, function() {
                let text = this._searchEntry.getText();
                // Strip leading and trailing whitespace
                text = text.replace(/^\s+/g, "").replace(/\s+$/g, "");
                this._searchQueued = false;
                this._searchAreaApps.setSearch(text);
                this._searchAreaDocs.setSearch(text);
                if (text == '')
                    this._searchPane.close();
                else
                    this._searchPane.open();
                return false;
            }));
        }));
        this._searchEntry.entry.connect('activate', Lang.bind(this, function (se) {
            // only one of the displays will have an item selected, so it's ok to
            // call activateSelected() on all of them
            this._searchAreaApps.activateSelected();
            this._searchAreaDocs.activateSelected();
            return true;
        }));
        this._searchEntry.entry.connect('key-press-event', Lang.bind(this, function (se, e) {
            let text = this._searchEntry.getText();
            let symbol = Shell.get_event_key_symbol(e);
            if (symbol == Clutter.Escape) {
                // Escape will keep clearing things back to the desktop. First, if
                // we have active text, we remove it.
                if (text != '')
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
                if (this._searchAreaDocs.hasSelected())
                  this._searchAreaDocs.selectUp();
                else if (this._searchAreaApps.hasItems())
                  this._searchAreaApps.selectUp();
                else
                  this._searchAreaDocs.selectUp();
                return true;
            } else if (symbol == Clutter.Down) {
                if (!this._searchActive)
                    return true;
                if (this._searchAreaDocs.hasSelected())
                  this._searchAreaDocs.selectDown();
                else if (this._searchAreaApps.hasItems())
                  this._searchAreaApps.selectDown();
                else
                  this._searchAreaDocs.selectDown();
                return true;
            }
            return false;
        }));

        /***** Applications *****/

        let appsSection = new Section("APPLICATIONS");
        let appWell = new AppDisplay.AppWell();
        appsSection.content.append(appWell.actor, Big.BoxPackFlags.EXPAND);

        this._moreAppsPane = null;
        appsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreAppsPane == null) {
                this._moreAppsPane = new ResultPane(this);
                this._moreAppsPane.packResults(AppDisplay.AppDisplay, true);
                this._addPane(this._moreAppsPane);
                link.setPane(this._moreAppsPane);
           }
        }));

        this.sectionArea.append(appsSection.actor, Big.BoxPackFlags.NONE);

        /***** Places *****/

        let placesSection = new Section("PLACES", true);
        let placesDisplay = new Places.Places();
        placesSection.content.append(placesDisplay.actor, Big.BoxPackFlags.EXPAND);
        this.sectionArea.append(placesSection.actor, Big.BoxPackFlags.NONE);

        /***** Documents *****/

        let docsSection = new Section("RECENT DOCUMENTS");

        let docDisplay = new DocDisplay.DocDisplay();
        docDisplay.load();
        docsSection.content.append(docDisplay.actor, Big.BoxPackFlags.EXPAND);
        createPaneForDetails(this, docDisplay);

        this._moreDocsPane = null;
        docsSection.header.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreDocsPane == null) {
                this._moreDocsPane = new ResultPane(this);
                this._moreDocsPane.packResults(DocDisplay.DocDisplay, true);
                this._addPane(this._moreDocsPane);
                link.setPane(this._moreDocsPane);
           }
        }));

        this.sectionArea.append(docsSection.actor, Big.BoxPackFlags.EXPAND);
    },

    show: function() {
        let global = Shell.Global.get();
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
