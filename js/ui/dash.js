/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;

const AppDisplay = imports.ui.appDisplay;
const DocDisplay = imports.ui.docDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Button = imports.ui.button;
const Main = imports.ui.main;

const DEFAULT_PADDING = 4;
const DASH_SECTION_PADDING = 6;
const DASH_SECTION_SPACING = 12;
const DASH_CORNER_RADIUS = 5;
const DASH_SEARCH_BG_COLOR = new Clutter.Color();
DASH_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const DASH_SECTION_COLOR = new Clutter.Color();
DASH_SECTION_COLOR.from_pixel(0x846c3dff);
const DASH_TEXT_COLOR = new Clutter.Color();
DASH_TEXT_COLOR.from_pixel(0xffffffff);

const PANE_BORDER_COLOR = new Clutter.Color();
PANE_BORDER_COLOR.from_pixel(0x213b5dfa);
const PANE_BORDER_WIDTH = 2;

const PANE_BACKGROUND_COLOR = new Clutter.Color();
PANE_BACKGROUND_COLOR.from_pixel(0x0d131ff4);


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
function createPaneForDetails(dash, display, detailsWidth) {
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
            let details = display.createDetailsForIndex(index, detailsWidth, -1);
            detailPane.content.append(details, Big.BoxPackFlags.EXPAND);
            detailPane.open();
        } else {
            detailPane.close();
        }
    }));
    return null;
}

function ResultPane(dash, detailsWidth) {
    this._init(dash, detailsWidth);
}

ResultPane.prototype = {
    __proto__: Pane.prototype,

    _init: function(dash, detailsWidth) {
        Pane.prototype._init.call(this);
        this._dash = dash;
        this._detailsWidth = detailsWidth;
    },

    // Create an instance of displayClass and pack it into this pane's
    // content area.  Return the displayClass instance.
    packResults: function(displayClass, enableNavigation) {
        let resultArea = new ResultArea(displayClass, enableNavigation);

        createPaneForDetails(this._dash, resultArea.display, this._detailsWidth);

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
                                   background_color: DASH_SEARCH_BG_COLOR,
                                   corner_radius: 4,
                                   spacing: DEFAULT_PADDING,
                                   padding: DEFAULT_PADDING
                                 });

        let icon = new Gio.ThemedIcon({ name: 'gtk-find' });
        let searchIconTexture = Shell.TextureCache.get_default().load_gicon(icon, 16);
        this.actor.append(searchIconTexture, Big.BoxPackFlags.NONE);

        this.pane = null;

        // We need to initialize the text for the entry to have the cursor displayed
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this.entry = new Clutter.Text({ font_name: "Sans 14px",
                                        editable: true,
                                        activatable: true,
                                        singleLineMode: true,
                                        text: ""
                                      });
        this.actor.append(this.entry, Big.BoxPackFlags.EXPAND);
    },

    setPane: function (pane) {
        this._pane = pane;
    }
};
Signals.addSignalMethods(SearchEntry.prototype);

function MoreLink() {
    this._init();
}

MoreLink.prototype = {
    _init : function () {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   padding_left: DEFAULT_PADDING,
                                   padding_right: DEFAULT_PADDING });
        let global = Shell.Global.get();
        let inactiveUri = "file://" + global.imagedir + "view-more.svg";
        let activeUri = "file://" + global.imagedir + "view-more-activated.svg";
        this._inactiveIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                            inactiveUri, 29, 18);
        this._activeIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                          activeUri, 29, 18);
        this._iconBox = new Big.Box({ reactive: true });
        this._iconBox.append(this._inactiveIcon, Big.BoxPackFlags.NONE);
        this.actor.append(this._iconBox, Big.BoxPackFlags.END);

        this.pane = null;

        this._iconBox.connect('button-press-event', Lang.bind(this, function (b, e) {
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
            this._iconBox.remove_all();
            this._iconBox.append(isOpen ? this._activeIcon : this._inactiveIcon,
                                 Big.BoxPackFlags.NONE);
        }));
    }
}

Signals.addSignalMethods(MoreLink.prototype);

function SectionHeader(title) {
    this._init(title);
}

SectionHeader.prototype = {
    _init : function (title) {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        let text = new Clutter.Text({ color: DASH_SECTION_COLOR,
                                      font_name: "Sans Bold 10px",
                                      text: title });
        this.moreLink = new MoreLink();
        this.actor.append(text, Big.BoxPackFlags.EXPAND);
        this.actor.append(this.moreLink.actor, Big.BoxPackFlags.END);
    }
}

function Dash(displayGridColumnWidth) {
    this._init(displayGridColumnWidth);
}

Dash.prototype = {
    _init : function(displayGridColumnWidth) {
        this._width = displayGridColumnWidth;

        this._detailsWidth = displayGridColumnWidth * 2;

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
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   width: this._width,
                                   padding: DEFAULT_PADDING,
                                   reactive: true });

        this.dashContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                           spacing: DASH_SECTION_SPACING });
        this.actor.append(this.dashContainer, Big.BoxPackFlags.EXPAND);

        // The currently active popup display
        this._activePane = null;

        /***** Search *****/

        this._searchPane = null;
        this._searchActive = false;
        this._searchEntry = new SearchEntry();
        this.dashContainer.append(this._searchEntry.actor, Big.BoxPackFlags.NONE);

        this._searchAreaApps = null;
        this._searchAreaDocs = null;

        this._searchQueued = false;
        this._searchEntry.entry.connect('text-changed', Lang.bind(this, function (se, prop) {
            this._searchActive = this._searchEntry.text != '';
            if (this._searchQueued)
                return;
            if (this._searchPane == null) {
                this._searchPane = new ResultPane(this, this._detailsWidth);
                this._searchPane.content.append(new Clutter.Text({ color: DASH_SECTION_COLOR,
                                                                   font_name: 'Sans Bold 10px',
                                                                   text: "APPLICATIONS" }),
                                                 Big.BoxPackFlags.NONE);
                this._searchAreaApps = this._searchPane.packResults(AppDisplay.AppDisplay, false);
                this._searchPane.content.append(new Clutter.Text({ color: DASH_SECTION_COLOR,
                                                                   font_name: 'Sans Bold 10px',
                                                                   text: "RECENT DOCUMENTS" }),
                                                 Big.BoxPackFlags.NONE);
                this._searchAreaDocs = this._searchPane.packResults(DocDisplay.DocDisplay, false);
                this._addPane(this._searchPane);
                this._searchEntry.setPane(this._searchPane);
            }
            this._searchQueued = true;
            Mainloop.timeout_add(250, Lang.bind(this, function() {
                // Strip leading and trailing whitespace
                let text = this._searchEntry.entry.text.replace(/^\s+/g, "").replace(/\s+$/g, "");
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
            let symbol = Shell.get_event_key_symbol(e);
            if (symbol == Clutter.Escape) {
                // Escape will keep clearing things back to the desktop. First, if
                // we have active text, we remove it.
                if (this._searchEntry.entry.text != '')
                    this._searchEntry.entry.text = '';
                // Next, if we're in one of the "more" modes or showing the details pane, close them
                else if (this._activePane != null)
                    this._activePane.close();
                // Finally, just close the overlay entirely
                else
                    Main.overlay.hide();
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

        let appsHeader = new SectionHeader("APPLICATIONS");
        this._appsSection = new Big.Box({ spacing: DEFAULT_PADDING });
        this._appsSection.append(appsHeader.actor, Big.BoxPackFlags.NONE);

        this._appsContent = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        this._appsSection.append(this._appsContent, Big.BoxPackFlags.EXPAND);
        this._appWell = new AppDisplay.AppWell(this._width);
        this._appsContent.append(this._appWell.actor, Big.BoxPackFlags.EXPAND);

        this._moreAppsPane = null;
        appsHeader.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreAppsPane == null) {
                this._moreAppsPane = new ResultPane(this, this._detailsWidth);
                this._moreAppsPane.packResults(AppDisplay.AppDisplay, true);
                this._addPane(this._moreAppsPane);
                link.setPane(this._moreAppsPane);
           }
        }));

        this.dashContainer.append(this._appsSection, Big.BoxPackFlags.NONE);

        /***** Documents *****/

        this._docsSection = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          spacing: DEFAULT_PADDING });
        this._moreDocsPane = null;

        let docsHeader = new SectionHeader("RECENT DOCUMENTS");
        this._docsSection.append(docsHeader.actor, Big.BoxPackFlags.NONE);

        this._docDisplay = new DocDisplay.DocDisplay();
        this._docDisplay.load();
        this._docsSection.append(this._docDisplay.actor, Big.BoxPackFlags.EXPAND);

        createPaneForDetails(this, this._docDisplay, this._detailsWidth);

        docsHeader.moreLink.connect('activated', Lang.bind(this, function (link) {
            if (this._moreDocsPane == null) {
                this._moreDocsPane = new ResultPane(this, this._detailsWidth);
                this._moreDocsPane.packResults(DocDisplay.DocDisplay, true);
                this._addPane(this._moreDocsPane);
                link.setPane(this._moreDocsPane);
           }
        }));

        this.dashContainer.append(this._docsSection, Big.BoxPackFlags.EXPAND);
    },

    show: function() {
        let global = Shell.Global.get();
        global.stage.set_key_focus(this._searchEntry.entry);
    },

    hide: function() {
        this._firstSelectAfterOverlayShow = true;
        if (this._searchEntry.entry.text != '')
            this._searchEntry.entry.text = '';
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
        Main.overlay.addPane(pane);
    }
};
Signals.addSignalMethods(Dash.prototype);
