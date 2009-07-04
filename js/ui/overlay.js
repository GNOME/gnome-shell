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
const Link = imports.ui.link;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const Workspaces = imports.ui.workspaces;

const ROOT_OVERLAY_COLOR = new Clutter.Color();
ROOT_OVERLAY_COLOR.from_pixel(0x000000bb);

// The factor to scale the overlay wallpaper with. This should not be less
// than 3/2, because the rule of thirds is used for positioning (see below).
const BACKGROUND_SCALE = 2;

const LABEL_HEIGHT = 16;
const DASH_MIN_WIDTH = 250;
const DASH_OUTER_PADDING = 4;
const DASH_SECTION_PADDING = 6;
const DASH_SECTION_SPACING = 6;
const DASH_CORNER_RADIUS = 5;
// This is the height of section components other than the item display.
const DASH_SECTION_MISC_HEIGHT = (LABEL_HEIGHT + DASH_SECTION_SPACING) * 2 + DASH_SECTION_PADDING;
const DASH_SEARCH_BG_COLOR = new Clutter.Color();
DASH_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const DASH_TEXT_COLOR = new Clutter.Color();
DASH_TEXT_COLOR.from_pixel(0xffffffff);

// Time for initial animation going into overlay mode
const ANIMATION_TIME = 0.25;

// We divide the screen into a grid of rows and columns, which we use
// to help us position the overlay components, such as the side panel
// that lists applications and documents, the workspaces display, and 
// the button for adding additional workspaces.
// In the regular mode, the side panel takes up one column on the left,
// and the workspaces display takes up the remaining columns.
// In the expanded side panel display mode, the side panel takes up two
// columns, and the workspaces display slides all the way to the right,
// being visible only in the last quarter of the right-most column.
// In the future, this mode will have more components, such as a display 
// of documents which were recently opened with a given application, which 
// will take up the remaining sections of the display.

const WIDE_SCREEN_CUT_OFF_RATIO = 1.4;

const COLUMNS_REGULAR_SCREEN = 4;
const ROWS_REGULAR_SCREEN = 8;
const COLUMNS_WIDE_SCREEN = 5;
const ROWS_WIDE_SCREEN = 10;

// Padding around workspace grid / Spacing between Dash and Workspaces
const WORKSPACE_GRID_PADDING = 12;

const COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN = 3;
const ROWS_FOR_WORKSPACES_REGULAR_SCREEN = 6;

const COLUMNS_FOR_WORKSPACES_WIDE_SCREEN = 4;
const ROWS_FOR_WORKSPACES_WIDE_SCREEN = 8;

// A multi-state; PENDING is used during animations
const STATE_ACTIVE = true;
const STATE_PENDING_INACTIVE = false;
const STATE_INACTIVE = false;

// The dash has a slightly transparent blue background with a gradient.
const DASH_LEFT_COLOR = new Clutter.Color();
DASH_LEFT_COLOR.from_pixel(0x324c6fbb);
const DASH_MIDDLE_COLOR = new Clutter.Color();
DASH_MIDDLE_COLOR.from_pixel(0x324c6faa);
const DASH_RIGHT_COLOR = new Clutter.Color();
DASH_RIGHT_COLOR.from_pixel(0x324c6fcc);

const DASH_BORDER_COLOR = new Clutter.Color();
DASH_BORDER_COLOR.from_pixel(0x213b5dff);

const DASH_BORDER_WIDTH = 2;

// The results and details panes have a somewhat transparent blue background with a gradient.
const PANE_LEFT_COLOR = new Clutter.Color();
PANE_LEFT_COLOR.from_pixel(0x324c6ff4);
const PANE_MIDDLE_COLOR = new Clutter.Color();
PANE_MIDDLE_COLOR.from_pixel(0x324c6ffa);
const PANE_RIGHT_COLOR = new Clutter.Color();
PANE_RIGHT_COLOR.from_pixel(0x324c6ff4);

const SHADOW_COLOR = new Clutter.Color();
SHADOW_COLOR.from_pixel(0x00000033);
const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

const SHADOW_WIDTH = 6;

const NUMBER_OF_SECTIONS_IN_SEARCH = 2;

let wideScreen = false;
let displayGridColumnWidth = null;
let displayGridRowHeight = null;

function SearchEntry(width) {
    this._init(width);
}

SearchEntry.prototype = {
    _init : function() {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   y_align: Big.BoxAlignment.CENTER,
                                   background_color: DASH_SEARCH_BG_COLOR,
                                   corner_radius: 4,
                                   spacing: 4,
                                   padding_left: 4,
                                   padding_right: 4,
                                   height: 24
                                 });

        let icontheme = Gtk.IconTheme.get_default();
        let searchIconTexture = new Clutter.Texture({});
        let searchIconPath = icontheme.lookup_icon('gtk-find', 16, 0).get_filename();
        searchIconTexture.set_from_file(searchIconPath);
        this.actor.append(searchIconTexture, 0);

        // We need to initialize the text for the entry to have the cursor displayed
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this.entry = new Clutter.Text({ font_name: "Sans 14px",
                                        editable: true,
                                        activatable: true,
                                        singleLineMode: true,
                                        text: ""
                                      });
        this.entry.connect('text-changed', Lang.bind(this, function (e) {
            let text = this.entry.text;
        }));
        this.actor.append(this.entry, Big.BoxPackFlags.EXPAND);
    }
};

function ItemResults(resultsWidth, resultsHeight, displayClass, labelText) {
    this._init(resultsWidth, resultsHeight, displayClass, labelText);
}

ItemResults.prototype = {
    _init: function(resultsWidth, resultsHeight, displayClass, labelText) {
        this._resultsWidth = resultsWidth;
        this._resultsHeight = resultsHeight;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   height: resultsHeight,
                                   padding: DASH_SECTION_PADDING + DASH_BORDER_WIDTH,
                                   spacing: DASH_SECTION_SPACING });

        this._resultsText = new Clutter.Text({ color: DASH_TEXT_COLOR,
                                               font_name: "Sans Bold 14px",
                                               text: labelText });

        this.resultsContainer = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                         spacing: 4 });
        this.navContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.resultsContainer.append(this.navContainer, Big.BoxPackFlags.NONE);

        // LABEL_HEIGHT is the height of this._resultsText and GenericDisplay.LABEL_HEIGHT is the height
        // of the display controls.
        this._displayHeight = resultsHeight - LABEL_HEIGHT - GenericDisplay.LABEL_HEIGHT - DASH_SECTION_SPACING * 2;
        this.display = new displayClass(resultsWidth);

        this.navArea = this.display.getNavigationArea();
        if (this.navArea)
            this.navContainer.append(this.navArea, Big.BoxPackFlags.EXPAND);

        this.resultsContainer.append(this.display.actor, Big.BoxPackFlags.EXPAND);

        this.controlBox = new Big.Box({ x_align: Big.BoxAlignment.CENTER });
        this.controlBox.append(this.display.displayControl, Big.BoxPackFlags.NONE);

        this._unsetSearchMode();
    },

    _setSearchMode: function() {
        if (this.navArea)
            this.navArea.hide();
        this.actor.height = this._resultsHeight /  NUMBER_OF_SECTIONS_IN_SEARCH;
        let displayHeight = this._displayHeight - this._resultsHeight * (NUMBER_OF_SECTIONS_IN_SEARCH - 1) /  NUMBER_OF_SECTIONS_IN_SEARCH;
        this.actor.remove_all();
        this.actor.append(this._resultsText, Big.BoxPackFlags.NONE);
        this.actor.append(this.resultsContainer, Big.BoxPackFlags.EXPAND);
        this.actor.append(this.controlBox, Big.BoxPackFlags.END);
    },

    _unsetSearchMode: function() {
        if (this.navArea)
            this.navArea.show();
        this.actor.height = this._resultsHeight;
        this.actor.remove_all();
        this.actor.append(this._resultsText, Big.BoxPackFlags.NONE);
        this.actor.append(this.resultsContainer, Big.BoxPackFlags.EXPAND);
        this.actor.append(this.controlBox, Big.BoxPackFlags.END);
    }
}

function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        let me = this;

        this._moreAppsMode = false;
        this._moreDocsMode = false;

        this._width = displayGridColumnWidth;

        this._displayWidth = displayGridColumnWidth - DASH_SECTION_PADDING * 2;
        this._resultsWidth = displayGridColumnWidth;
        this._detailsWidth = displayGridColumnWidth * 2;

        let bottomHeight = DASH_SECTION_PADDING;

        let global = Shell.Global.get();

        let resultsHeight = global.screen_height - Panel.PANEL_HEIGHT - DASH_SECTION_PADDING - bottomHeight;
        let detailsHeight = global.screen_height - Panel.PANEL_HEIGHT - DASH_SECTION_PADDING - bottomHeight;

        this.actor = new Clutter.Group({});
        this.actor.height = global.screen_height;


        // dashPane, as well as results and details panes need to be reactive so that the clicks in unoccupied places on them
        // are not passed to the transparent background underneath them. This background is used for the workspaces area when
        // the additional dash panes are being shown and it handles clicks by closing the additional panes, so that the user
        // can interact with the workspaces. However, this behavior is not desirable when the click is actually over a pane.
        //
        // We have to make the individual panes reactive instead of making the whole dash actor reactive because the width
        // of the Group actor ends up including the width of its hidden children, so we were getting a reactive object as
        // wide as the details pane that was blocking the clicks to the workspaces underneath it even when the details pane
        // was actually hidden. 
        let dashPane = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                     x: 0,
                                     y: Panel.PANEL_HEIGHT + DASH_SECTION_PADDING,
                                     width: this._width + SHADOW_WIDTH,
                                     height: global.screen_height - Panel.PANEL_HEIGHT - DASH_SECTION_PADDING - bottomHeight,
                                     reactive: true});

        let dashBackground = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                           width: this._width,
                                           height: global.screen_height - Panel.PANEL_HEIGHT - DASH_SECTION_PADDING - bottomHeight,
                                           corner_radius: DASH_CORNER_RADIUS,
                                           border: DASH_BORDER_WIDTH,
                                           border_color: DASH_BORDER_COLOR });

        dashPane.append(dashBackground, Big.BoxPackFlags.EXPAND);
        
        let dashLeft = global.create_horizontal_gradient(DASH_LEFT_COLOR,
                                                         DASH_MIDDLE_COLOR);
        let dashRight = global.create_horizontal_gradient(DASH_MIDDLE_COLOR,
                                                          DASH_RIGHT_COLOR);
        let dashShadow = global.create_horizontal_gradient(SHADOW_COLOR,
                                                           TRANSPARENT_COLOR);
        dashShadow.set_width(SHADOW_WIDTH);
        
        dashBackground.append(dashLeft, Big.BoxPackFlags.EXPAND);
        dashBackground.append(dashRight, Big.BoxPackFlags.EXPAND);
        dashPane.append(dashShadow, Big.BoxPackFlags.NONE);

        this.actor.add_actor(dashPane);

        this.dashOuterContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                               x: 0,
                                               y: dashPane.y + DASH_BORDER_WIDTH,
                                               width: this._width,
                                               height: global.screen_height - Panel.PANEL_HEIGHT - DASH_SECTION_PADDING - bottomHeight,
                                               padding: DASH_OUTER_PADDING
                                             });
        this.actor.add_actor(this.dashOuterContainer);
        this.dashContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL });
        this.dashOuterContainer.append(this.dashContainer, Big.BoxPackFlags.EXPAND);

        this._searchEntry = new SearchEntry();
        this.dashContainer.append(this._searchEntry.actor, Big.BoxPackFlags.NONE);

        this._searchQueued = false;
        this._searchEntry.entry.connect('text-changed', function (se, prop) {
            if (me._searchQueued)
                return;
            me._searchQueued = true;
            Mainloop.timeout_add(250, function() {
                // Strip leading and trailing whitespace
                let text = me._searchEntry.entry.text.replace(/^\s+/g, "").replace(/\s+$/g, "");
                me._searchQueued = false;
                me._resultsAppsSection.display.setSearch(text);
                me._resultsDocsSection.display.setSearch(text);
                if (text == '')
                    me._unsetSearchMode();
                else 
                    me._setSearchMode();
                   
                return false;
            });
        });
        this._searchEntry.entry.connect('activate', function (se) {
            // only one of the displays will have an item selected, so it's ok to
            // call activateSelected() on all of them
            me._docDisplay.activateSelected();
            me._resultsAppsSection.display.activateSelected();
            me._resultsDocsSection.display.activateSelected();
            return true;
        });
        this._searchEntry.entry.connect('key-press-event', function (se, e) {
            let symbol = Shell.get_event_key_symbol(e);
            if (symbol == Clutter.Escape) {
                // Escape will keep clearing things back to the desktop. First, if
                // we have active text, we remove it.
                if (me._searchEntry.entry.text != '')
                    me._searchEntry.entry.text = '';
                // Next, if we're in one of the "more" modes or showing the details pane, close them
                else if (me._moreAppsMode || me._moreDocsMode || me._detailsShowing())
                    me.unsetMoreMode();
                // Finally, just close the overlay entirely
                else
                    me.emit('activated');
                return true;
            } else if (symbol == Clutter.Up) {
                // selectUp and selectDown wrap around in their respective displays
                // too, but there doesn't seem to be any flickering if we first select
                // something in one display, but then unset the selection, and move
                // it to the other display, so it's ok to do that.
                if (me._resultsDocsSection.display.hasSelected())
                  me._resultsDocsSection.display.selectUp();
                else if (me._resultsAppsSection.display.hasItems())
                  me._resultsAppsSection.display.selectUp();
                else
                  me._resultsDocsSection.display.selectUp();
            } else if (symbol == Clutter.Down) {
                if (me._resultsDocsSection.display.hasSelected())
                  me._resultsDocsSection.display.selectDown();
                else if (me._resultsAppsSection.display.hasItems())
                  me._resultsAppsSection.display.selectDown();
                else
                  me._resultsDocsSection.display.selectDown();
            }
            return false;
        });

        this._appsText = new Clutter.Text({ color: DASH_TEXT_COLOR,
                                            font_name: "Sans Bold 14px",
                                            text: "Applications",
                                            height: LABEL_HEIGHT});
        this._appsSection = new Big.Box({ padding_top: DASH_SECTION_PADDING,
                                          spacing: DASH_SECTION_SPACING});
        this._appsSection.append(this._appsText, Big.BoxPackFlags.NONE);

        this._itemDisplayHeight = global.screen_height - this._appsSection.y - DASH_SECTION_MISC_HEIGHT * 2 - bottomHeight;
        
        this._appsContent = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        this._appsSection.append(this._appsContent, Big.BoxPackFlags.EXPAND);
        this._appWell = new AppDisplay.AppWell(this._displayWidth);
        this._appsContent.append(this._appWell.actor, Big.BoxPackFlags.EXPAND);

        let moreAppsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreAppsLink = new Link.Link({ color: DASH_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreAppsBox.append(this._moreAppsLink.actor, Big.BoxPackFlags.EXPAND);
        this._appsSection.append(moreAppsBox, Big.BoxPackFlags.NONE);

        this.dashContainer.append(this._appsSection, Big.BoxPackFlags.NONE);

        this._appsSectionDefaultHeight = this._appsSection.height;

        this._docsSection = new Big.Box({ padding_top: DASH_SECTION_PADDING,
                                          padding_bottom: DASH_SECTION_PADDING,
                                          spacing: DASH_SECTION_SPACING});

        this._docsText = new Clutter.Text({ color: DASH_TEXT_COLOR,
                                            font_name: "Sans Bold 14px",
                                            text: "Recent Documents",
                                            height: LABEL_HEIGHT});
        this._docsSection.append(this._docsText, Big.BoxPackFlags.NONE);

        this._docDisplay = new DocDisplay.DocDisplay(this._displayWidth);
        this._docsSection.append(this._docDisplay.actor, Big.BoxPackFlags.EXPAND);

        let moreDocsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreDocsLink = new Link.Link({ color: DASH_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreDocsBox.append(this._moreDocsLink.actor, Big.BoxPackFlags.EXPAND);
        this._docsSection.append(moreDocsBox, Big.BoxPackFlags.NONE);

        this.dashContainer.append(this._docsSection, Big.BoxPackFlags.EXPAND);

        this._docsSectionDefaultHeight = this._docsSection.height;

        // The "More" or search results area
        this._resultsAppsSection = new ItemResults(this._resultsWidth, resultsHeight, AppDisplay.AppDisplay, "Applications");
        this._resultsDocsSection = new ItemResults(this._resultsWidth, resultsHeight, DocDisplay.DocDisplay, "Documents");

        this._resultsPane = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          x: this._width,
                                          y: Panel.PANEL_HEIGHT + DASH_SECTION_PADDING,
                                          height: resultsHeight,
                                          reactive: true });

        let resultsBackground = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                              height: resultsHeight,
                                              corner_radius: DASH_CORNER_RADIUS,
                                              border: DASH_BORDER_WIDTH,
                                              border_color: DASH_BORDER_COLOR });
        this._resultsPane.add_actor(resultsBackground);

        let resultsLeft = global.create_horizontal_gradient(PANE_LEFT_COLOR,
                                                            PANE_MIDDLE_COLOR);
        let resultsRight = global.create_horizontal_gradient(PANE_MIDDLE_COLOR,
                                                             PANE_RIGHT_COLOR);
        let resultsShadow = global.create_horizontal_gradient(SHADOW_COLOR,
                                                              TRANSPARENT_COLOR);
        resultsShadow.set_width(SHADOW_WIDTH);

        resultsBackground.append(resultsLeft, Big.BoxPackFlags.EXPAND);
        resultsBackground.append(resultsRight, Big.BoxPackFlags.EXPAND);
        this._resultsPane.add_actor(resultsShadow);
        this._resultsPane.connect('notify::allocation', Lang.bind(this, function (b, a) {
            let width = this._resultsPane.width;
            resultsBackground.width = width;
            resultsShadow.width = width;
        }));

        this.actor.add_actor(this._resultsPane);
        this._resultsPane.hide();

        this._detailsPane = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                          y: Panel.PANEL_HEIGHT + DASH_SECTION_PADDING,
                                          width: this._detailsWidth + SHADOW_WIDTH,
                                          height: detailsHeight,
                                          reactive: true });
        this._firstSelectAfterOverlayShow = true;

        let detailsBackground = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                              width: this._detailsWidth,
                                              height: detailsHeight,
                                              corner_radius: DASH_CORNER_RADIUS,
                                              border: DASH_BORDER_WIDTH,
                                              border_color: DASH_BORDER_COLOR });

        this._detailsPane.append(detailsBackground, Big.BoxPackFlags.EXPAND);

        let detailsLeft = global.create_horizontal_gradient(PANE_LEFT_COLOR,
                                                            PANE_MIDDLE_COLOR);
        let detailsRight = global.create_horizontal_gradient(PANE_MIDDLE_COLOR,
                                                             PANE_RIGHT_COLOR);
        let detailsShadow = global.create_horizontal_gradient(SHADOW_COLOR,
                                                              TRANSPARENT_COLOR);
        detailsShadow.set_width(SHADOW_WIDTH);
        
        detailsBackground.append(detailsLeft, Big.BoxPackFlags.EXPAND);
        detailsBackground.append(detailsRight, Big.BoxPackFlags.EXPAND);
        this._detailsPane.append(detailsShadow, Big.BoxPackFlags.NONE);

        this._detailsContent = new Big.Box({ padding: DASH_SECTION_PADDING + DASH_BORDER_WIDTH });
        this._detailsPane.add_actor(this._detailsContent);

        this.actor.add_actor(this._detailsPane);
        this._detailsPane.hide();

        let itemDetailsAvailableWidth = this._detailsWidth - DASH_SECTION_PADDING * 2 - DASH_BORDER_WIDTH * 2;
        let itemDetailsAvailableHeight = detailsHeight - DASH_SECTION_PADDING * 2 - DASH_BORDER_WIDTH * 2;

        this._docDisplay.setAvailableDimensionsForItemDetails(itemDetailsAvailableWidth, itemDetailsAvailableHeight);
        this._resultsAppsSection.display.setAvailableDimensionsForItemDetails(itemDetailsAvailableWidth, itemDetailsAvailableHeight);
        this._resultsDocsSection.display.setAvailableDimensionsForItemDetails(itemDetailsAvailableWidth, itemDetailsAvailableHeight);

        /* Proxy the activated signals */
        this._appWell.connect('activated', function(well) {
            me.emit('activated');
        });
        this._docDisplay.connect('activated', function(docDisplay) {
            me.emit('activated');
        });
        this._resultsAppsSection.display.connect('activated', function(resultsAppsDisplay) {
            me.emit('activated');
        });
        this._resultsDocsSection.display.connect('activated', function(resultsDocsDisplay) {
            me.emit('activated');
        });
        this._docDisplay.connect('selected', Lang.bind(this, function(docDisplay) {
            this._resultsDocsSection.display.unsetSelected();
            this._resultsAppsSection.display.unsetSelected();
            this._showDetails();
            this._detailsContent.remove_all();
            this._detailsContent.append(this._docDisplay.selectedItemDetails, Big.BoxPackFlags.NONE);
        }));
        this._resultsDocsSection.display.connect('selected', Lang.bind(this, function(resultsDocDisplay) {
            this._docDisplay.unsetSelected();
            this._resultsAppsSection.display.unsetSelected();
            this._showDetails();
            this._detailsContent.remove_all();
            this._detailsContent.append(this._resultsDocsSection.display.selectedItemDetails, Big.BoxPackFlags.NONE);
        }));
        this._resultsAppsSection.display.connect('selected', Lang.bind(this, function(resultsAppDisplay) {
            this._docDisplay.unsetSelected();
            this._resultsDocsSection.display.unsetSelected();
            this._showDetails();
            this._detailsContent.remove_all();
            this._detailsContent.append(this._resultsAppsSection.display.selectedItemDetails, Big.BoxPackFlags.NONE);
        }));

        this._moreAppsLink.connect('clicked',
            function(o, event) {
                if (me._moreAppsMode) {
                    me._unsetMoreAppsMode();
                }  else {
                    me._setMoreAppsMode();
                }
            });

        this._moreDocsLink.connect('clicked',
            function(o, event) {
                if (me._moreDocsMode) {
                    me._unsetMoreDocsMode();
                }  else {
                    me._setMoreDocsMode();
                }
            });
    },

    show: function() {
        let global = Shell.Global.get();

        this._appsContent.show();
        this._docDisplay.show();
        global.stage.set_key_focus(this._searchEntry.entry);
    },

    hide: function() {
        this._firstSelectAfterOverlayShow = true;
        this._appsContent.hide();
        this._docDisplay.hide(); 
        this._searchEntry.entry.text = '';
        this.unsetMoreMode();
    },

    unsetMoreMode: function() {
        this._unsetMoreAppsMode();
        this._unsetMoreDocsMode();
        if (this._detailsShowing()) { 
             this._detailsPane.hide();
             this.emit('panes-removed');
        }
        this._unsetSearchMode();
    },

    // Sets the 'More' mode for browsing applications.
    _setMoreAppsMode: function() {
        if (this._moreAppsMode)
            return;

        this._unsetMoreDocsMode();
        this._unsetSearchMode();
        this._moreAppsMode = true;

        this._resultsAppsSection.display.show();
        this._resultsPane.append(this._resultsAppsSection.actor, Big.BoxPackFlags.EXPAND);
        this._resultsPane.show();

        this._moreAppsLink.setText("Less...");
        this._repositionDetails();
        this.emit('panes-displayed');
    },

    // Unsets the 'More' mode for browsing applications.
    _unsetMoreAppsMode: function() {
        if (!this._moreAppsMode)
            return;

        this._moreAppsMode = false;

        this._resultsPane.remove_actor(this._resultsAppsSection.actor);
        this._resultsAppsSection.display.hide();
        this._resultsPane.hide(); 

        this._moreAppsLink.setText("More...");

        this._repositionDetails();
        if (!this._detailsShowing()) {
            this.emit('panes-removed');
        }
    },   
 
    // Sets the 'More' mode for browsing documents.
    _setMoreDocsMode: function() {
        if (this._moreDocsMode)
            return;

        this._unsetMoreAppsMode();
        this._unsetSearchMode();
        this._moreDocsMode = true;

        this._resultsDocsSection.display.show();
        this._resultsPane.append(this._resultsDocsSection.actor, Big.BoxPackFlags.EXPAND);
        this._resultsPane.show();

        this._moreDocsLink.setText("Less...");

        this._repositionDetails();

        this.emit('panes-displayed');
    },

    // Unsets the 'More' mode for browsing documents. 
    _unsetMoreDocsMode: function() {
        if (!this._moreDocsMode)
            return;

        this._moreDocsMode = false;

        this._resultsPane.hide();
        this._resultsPane.remove_actor(this._resultsDocsSection.actor);
        this._resultsDocsSection.display.hide();

        this._moreDocsLink.setText("More...");

        this._repositionDetails();

        if (!this._detailsShowing()) {
            this.emit('panes-removed');
        }
    },

    _setSearchMode: function() {
        this._repositionDetails();

        if (this._resultsShowing())
            return;

        this._resultsAppsSection._setSearchMode();
        this._resultsAppsSection.display.show();
        this._resultsPane.append(this._resultsAppsSection.actor, Big.BoxPackFlags.EXPAND);

        this._resultsDocsSection._setSearchMode();
        this._resultsDocsSection.display.show();
        this._resultsPane.append(this._resultsDocsSection.actor, Big.BoxPackFlags.EXPAND);

        this._resultsPane.show();

        this.emit('panes-displayed');
    },

    _unsetSearchMode: function() {
        this._repositionDetails();

        if (this._moreDocsMode || this._moreAppsMode || !this._resultsShowing())
            return;

        this._resultsPane.hide();

        this._resultsPane.remove_actor(this._resultsAppsSection.actor);
        this._resultsAppsSection.display.hide();
        this._resultsAppsSection._unsetSearchMode();

        this._resultsPane.remove_actor(this._resultsDocsSection.actor);
        this._resultsDocsSection.display.hide();
        this._resultsDocsSection._unsetSearchMode();
        this._resultsDocsSection.actor.set_y(0);

        this._repositionDetails();

        if (!this._detailsShowing()) {
            this.emit('panes-removed');
        }
    },

    _repositionDetails: function () {
        let x;
        if (this._resultsPane.visible)
            x = this._resultsPane.x + this._resultsPane.width;
        else
            x = this._width;
        x += DASH_SECTION_PADDING;
        this._detailsPane.x = x;
    },

    _showDetails: function () {
        this._detailsPane.show();
        this._repositionDetails();
        this.emit('panes-displayed');
    },

    _detailsShowing: function() {
        return this._detailsPane.visible;
    },

    _resultsShowing: function() {
        return this._resultsPane.visible;
    }      
};

Signals.addSignalMethods(Dash.prototype);

function Overlay() {
    this._init();
}

Overlay.prototype = {
    _init : function() {
        let me = this;

        let global = Shell.Global.get();

        wideScreen = (global.screen_width/global.screen_height > WIDE_SCREEN_CUT_OFF_RATIO);

        // We divide the screen into an imaginary grid which helps us determine the layout of
        // different visual components.  
        if (wideScreen) {
            displayGridColumnWidth = global.screen_width / COLUMNS_WIDE_SCREEN;
            displayGridRowHeight = global.screen_height / ROWS_WIDE_SCREEN;
        } else {
            displayGridColumnWidth = global.screen_width / COLUMNS_REGULAR_SCREEN;
            displayGridRowHeight = global.screen_height / ROWS_REGULAR_SCREEN;
        }

        this._group = new Clutter.Group();
        this._group._delegate = this;

        this.visible = false;
        this._hideInProgress = false;

        // A scaled root pixmap actor is used as a background. It is zoomed in
        // to the lower right intersection of the lines that divide the image
        // evenly in a 3x3 grid. This is based on the rule of thirds, a
        // compositional rule of thumb in visual arts. The choice for the
        // lower right point is based on a quick survey of GNOME wallpapers.
        let background = global.create_root_pixmap_actor();
        background.width = global.screen_width * BACKGROUND_SCALE;
        background.height = global.screen_height * BACKGROUND_SCALE;
        background.x = -global.screen_width * (4 * BACKGROUND_SCALE - 3) / 6;
        background.y = -global.screen_height * (4 * BACKGROUND_SCALE - 3) / 6;
        this._group.add_actor(background);

        // Transparent background is used to catch clicks outside of the dash panes when the panes
        // are being displayed and the workspaces area should not be reactive. Catching such a
        // click results in the panes being closed and the workspaces area becoming reactive again. 
        this._transparentBackground = new Clutter.Rectangle({ opacity: 0,
                                                              width: global.screen_width,
                                                              height: global.screen_height - Panel.PANEL_HEIGHT,
                                                              y: Panel.PANEL_HEIGHT,
                                                              reactive: true });
        this._group.add_actor(this._transparentBackground);

        // Draw a semitransparent rectangle over the background for readability.
        let backOver = new Clutter.Rectangle({ color: ROOT_OVERLAY_COLOR,
                                               width: global.screen_width,
                                               height: global.screen_height - Panel.PANEL_HEIGHT,
                                               y: Panel.PANEL_HEIGHT });
        this._group.add_actor(backOver);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash();
        this._group.add_actor(this._dash.actor);
        this._workspaces = null;
        this._buttonEventHandlerId = null;
        this._dash.connect('activated', function(dash) {
            // TODO - have some sort of animation/effect while
            // transitioning to the new app.  We definitely need
            // startup-notification integration at least.
            me.hide();
        });
        this._dash.connect('panes-displayed', function(dash) {
            if (me._buttonEventHandlerId == null) {
                me._transparentBackground.raise_top();
                me._dash.actor.raise_top();
                me._buttonEventHandlerId = me._transparentBackground.connect('button-release-event', function(background) {
                    me._dash.unsetMoreMode();
                    return true;
                }); 
            }    
        });
        this._dash.connect('panes-removed', function(dash) {
            if (me._buttonEventHandlerId != null) {
                me._transparentBackground.lower_bottom();  
                me._transparentBackground.disconnect(me._buttonEventHandlerId);  
                me._buttonEventHandlerId = null;
            }
        });
    },

    //// Draggable target interface ////

    // Unsets the expanded display mode if a GenericDisplayItem is being 
    // dragged over the overlay, i.e. as soon as it starts being dragged.
    // This closes the additional panes and allows the user to place
    // the item on any workspace.
    handleDragOver : function(source, actor, x, y, time) {
        if (source instanceof GenericDisplay.GenericDisplayItem) {
            this._dash.unsetMoreMode();
            return true;
        }
  
        return false;
    },

    //// Public methods ////

    show : function() {
        if (this.visible)
            return;
        if (!Main.startModal())
            return;

        this.visible = true;

        let global = Shell.Global.get();
        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height; 

        this._dash.show();

        let columnsUsed = wideScreen ? COLUMNS_FOR_WORKSPACES_WIDE_SCREEN : COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN;
        let rowsUsed = wideScreen ? ROWS_FOR_WORKSPACES_WIDE_SCREEN : ROWS_FOR_WORKSPACES_REGULAR_SCREEN;  
         
        let workspacesWidth = displayGridColumnWidth * columnsUsed - WORKSPACE_GRID_PADDING * 2;
        // We scale the vertical padding by (screenHeight / screenWidth) so that the workspace preserves its aspect ratio.
        let workspacesHeight = displayGridRowHeight * rowsUsed - WORKSPACE_GRID_PADDING * (screenHeight / screenWidth) * 2;

        let workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
        let workspacesY = displayGridRowHeight + WORKSPACE_GRID_PADDING * (screenHeight / screenWidth);

        // place the 'Add Workspace' button in the bottom row of the grid
        let addButtonSize = Math.floor(displayGridRowHeight * 3/5);
        let addButtonX = workspacesX + workspacesWidth - addButtonSize;
        let addButtonY = screenHeight - Math.floor(displayGridRowHeight * 4/5);

        this._workspaces = new Workspaces.Workspaces(workspacesWidth, workspacesHeight, workspacesX, workspacesY, 
                                                     addButtonSize, addButtonX, addButtonY);
        this._group.add_actor(this._workspaces.actor);

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the overlay is displayed greatly
        // increases performance of the overlay especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the overlay rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();

        // Try to make the menu not too visible behind the empty space between
        // the workspace previews by sliding in its clipping rectangle.
        // We want to finish drawing the Dash just before the top workspace fully
        // slides in on the top. Which means that we have more time to wait before
        // drawing the dash if the active workspace is displayed on the bottom of
        // the workspaces grid, and almost no time to wait if it is displayed in the top
        // row of the workspaces grid. The calculations used below try to roughly
        // capture the animation ratio for when workspaces are covering the top of the overlay
        // vs. when workspaces are already below the top of the overlay, and apply it
        // to clipping the dash. The clipping is removed in this._showDone().
        this._dash.actor.set_clip(0, 0,
                                      this._workspaces.getFullSizeX(),
                                      this._dash.actor.height);
        Tweener.addTween(this._dash.actor,
                         { clipWidthRight: this._dash._width + WORKSPACE_GRID_PADDING + this._workspaces.getWidthToTopActiveWorkspace(),
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._showDone,
                           onCompleteScope: this

                         });

        this.emit('showing');
    },

    hide : function() {
        if (!this.visible || this._hideInProgress)
            return;

        let global = Shell.Global.get();

        this._hideInProgress = true;
        // lower the Dash, so that workspaces display is on top and covers the Dash while it is sliding out
        this._dash.actor.lower(this._workspaces.actor);
        this._workspaces.hide();

        // Try to make the menu not too visible behind the empty space between
        // the workspace previews by sliding in its clipping rectangle.
        // The logic used is the same as described in this.show(). If the active workspace
        // is displayed in the top row, than almost full animation time is needed for it
        // to reach the top of the overlay and cover the Dash fully, while if the
        // active workspace is in the lower row, than the top left workspace reaches the
        // top of the overlay sooner as it is moving out of the way.
        // The clipping is removed in this._hideDone().
        this._dash.actor.set_clip(0, 0,
                                      this._dash.actor.width + WORKSPACE_GRID_PADDING + this._workspaces.getWidthToTopActiveWorkspace(),
                                      this._dash.actor.height);
        Tweener.addTween(this._dash.actor,
                         { clipWidthRight: this._workspaces.getFullSizeX() + this._workspaces.getWidthToTopActiveWorkspace() - global.screen_width,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });

        this.emit('hiding');
    },

    toggle: function() {
        if (this.visible)
            this.hide();
        else
            this.show();
    },

    //// Private methods ////

    // Raises the Dash to the top, so that we can tell if the pointer is above one of its items.
    // We need to do this once the workspaces are shown because the workspaces actor currently covers
    // the whole screen, regardless of where the workspaces are actually displayed.
    //
    // Once we rework the workspaces actor to only cover the area it actually needs, we can
    // remove this workaround. Also http://bugzilla.openedhand.com/show_bug.cgi?id=1513 requests being
    // able to pick only a reactive actor at a certain position, rather than any actor. Being able
    // to do that would allow us to not have to raise the Dash.
    _showDone: function() {
        if (this._hideInProgress)
            return;

        this._dash.actor.raise_top();
        this._dash.actor.remove_clip();

        this.emit('shown');
    },

    _hideDone: function() {
        let global = Shell.Global.get();

        global.window_group.show();

        this._workspaces.destroy();
        this._workspaces = null;

        this._dash.actor.remove_clip();
        this._dash.hide();
        this._group.hide();

        this.visible = false; 
        this._hideInProgress = false;

        Main.endModal();
        this.emit('hidden');
    }
};
Signals.addSignalMethods(Overlay.prototype);

Tweener.registerSpecialProperty("clipHeightBottom", _clipHeightBottomGet, _clipHeightBottomSet);

function _clipHeightBottomGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipHeight;
}

function _clipHeightBottomSet(actor, clipHeight) {
    actor.set_clip(0, 0, actor.width, clipHeight);
}

Tweener.registerSpecialProperty("clipHeightTop", _clipHeightTopGet, _clipHeightTopSet);

function _clipHeightTopGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipHeight;
}

function _clipHeightTopSet(actor, clipHeight) {
    actor.set_clip(0, actor.height - clipHeight, actor.width, clipHeight);
}

Tweener.registerSpecialProperty("clipWidthRight", _clipWidthRightGet, _clipWidthRightSet);

function _clipWidthRightGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipWidth;
}

function _clipWidthRightSet(actor, clipWidth) {
    actor.set_clip(0, 0, clipWidth, actor.height);
}
