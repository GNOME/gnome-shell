/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const AppFavorites = imports.ui.appFavorites;
const DND = imports.ui.dnd;
const DocDisplay = imports.ui.docDisplay;
const PlaceDisplay = imports.ui.placeDisplay;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Search = imports.ui.search;
const SearchDisplay = imports.ui.searchDisplay;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;

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

        // Hidden by default
        this.actor.hide();
    },

    open: function () {
        if (this._open)
            return;
        this._open = true;
        this.emit('open-state-changed', this._open);
        this.actor.opacity = 0;
        this.actor.show();
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: Overview.PANE_FADE_TIME,
                           transition: 'easeOutQuad'
                         });
    },

    close: function () {
        if (!this._open)
            return;
        this._open = false;
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: Overview.PANE_FADE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this, function() {
                               this.actor.hide();
                               this.emit('open-state-changed', this._open);
                           })
                         });
    },

    destroyContent: function() {
        let children = this.actor.get_children();
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

function ResultArea() {
    this._init();
}

ResultArea.prototype = {
    _init : function() {
        this.actor = new St.BoxLayout({ vertical: true });
        this.resultsContainer = new St.BoxLayout({ style_class: 'dash-results-container' });
        this.actor.add(this.resultsContainer, { expand: true });

        this.display = new DocDisplay.DocDisplay();
        this.resultsContainer.add(this.display.actor, { expand: true });
        this.display.load();
    }
};

function ResultPane(dash) {
    this._init(dash);
}

ResultPane.prototype = {
    __proto__: Pane.prototype,

    _init: function(dash) {
        Pane.prototype._init.call(this);

        let resultArea = new ResultArea();
        this.actor.add(resultArea.actor, { expand: true });
        this.connect('open-state-changed', Lang.bind(this, function(pane, isOpen) {
            resultArea.display.resetState();
        }));
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

        global.stage.connect('notify::key-focus', Lang.bind(this, this._updateCursorVisibility));

        this.pane = null;

        this._capturedEventId = 0;
    },

    _updateCursorVisibility: function() {
        let focus = global.stage.get_key_focus();
        if (focus == global.stage || focus == this.entry)
            this.entry.set_cursor_visible(true);
        else
            this.entry.set_cursor_visible(false);
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

    reset: function () {
        let [x, y, mask] = global.get_pointer();
        let actor = global.stage.get_actor_at_pos (Clutter.PickMode.REACTIVE,
                                                   x, y);
        // this.actor is never hovered directly, only its clutter_text and icon
        let hovered = this.actor == actor.get_parent();

        this.actor.set_hover(hovered);

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
        let panelEvent = source && Main.panel.actor.contains(source);

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
        if (!this.actor.visible)
            return true; // If the link isn't visible we don't want the header to react
                         // to clicks
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

        let textBox = new St.BoxLayout({ style_class: 'section-text-content' });
        this.text = new St.Label({ style_class: 'section-title',
                                   text: title });
        textBox.add(this.text, { x_align: St.Align.START });

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

    setMoreLinkVisible : function(visible) {
        if (visible)
            this.moreLink.actor.show();
        else
            this.moreLink.actor.hide();
    }
};

Signals.addSignalMethods(SectionHeader.prototype);


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

function OldDash() {
    this._init();
}

OldDash.prototype = {
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

        this.searchResults = new SearchDisplay.SearchResults(this._searchSystem);
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

    addSearchProvider: function(provider) {
        //Add a new search provider to the dash.

        this._searchSystem.registerProvider(provider);
        this.searchResults.createProviderMeta(provider);
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


function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];
        this._maxHeight = -1;

        this._favorites = [];

        this._box = new St.BoxLayout({ name: 'dash',
                                       vertical: true,
                                       clip_to_allocation: true });
        this._box._delegate = this;

        this.actor = new St.Bin({ y_align: St.Align.START, child: this._box });
        this.actor.connect('notify::height', Lang.bind(this,
            function() {
                if (this._maxHeight != this.actor.height)
                    this._queueRedisplay();
                this._maxHeight = this.actor.height;
            }));

        this._workId = Main.initializeDeferredWork(this._box, Lang.bind(this, this._redisplay));

        this._tracker = Shell.WindowTracker.get_default();
        this._appSystem = Shell.AppSystem.get_default();

        this._appSystem.connect('installed-changed', Lang.bind(this, this._queueRedisplay));
        AppFavorites.getAppFavorites().connect('changed', Lang.bind(this, this._queueRedisplay));
        this._tracker.connect('app-state-changed', Lang.bind(this, this._queueRedisplay));
    },

    _appIdListToHash: function(apps) {
        let ids = {};
        for (let i = 0; i < apps.length; i++)
            ids[apps[i].get_id()] = apps[i];
        return ids;
    },

    _queueRedisplay: function () {
        Main.queueDeferredWork(this._workId);
    },

    _redisplay: function () {
        this._box.hide();
        this._box.remove_all();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        /* hardcode here pending some design about how exactly desktop contexts behave */
        let contextId = '';

        let running = this._tracker.get_running_apps(contextId);

        for (let id in favorites) {
            let app = favorites[id];
            let display = new AppDisplay.AppWellIcon(app);
            this._box.add(display.actor);
        }

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            if (app.get_id() in favorites)
                continue;
            let display = new AppDisplay.AppWellIcon(app);
            this._box.add(display.actor);
        }

        let children = this._box.get_children();
        if (children.length == 0) {
            this._box.add_style_pseudo_class('empty');
        } else {
            this._box.remove_style_pseudo_class('empty');

            if (this._maxHeight > -1) {
                let iconSizes = [ 48, 32, 24, 22, 16 ];

                for (let i = 0; i < iconSizes.length; i++) {
                    let minHeight, natHeight;

                    this._iconSize = iconSizes[i];
                    for (let j = 0; j < children.length; j++)
                        children[j]._delegate.icon.setIconSize(this._iconSize);

                    [minHeight, natHeight] = this.actor.get_preferred_height(-1);

                    if (natHeight <= this._maxHeight)
                        break;
                }
            }
        }
        this._box.show();
    },

    handleDragOver : function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplay.AppWellIcon)
            app = this._appSystem.get_app(source.getId());
        else if (source instanceof Workspace.WindowClone)
            app = this._tracker.get_window_app(source.metaWindow);

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient())
            return DND.DragMotionResult.NO_DROP;

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite)
            return DND.DragMotionResult.NO_DROP;

        return DND.DragMotionResult.COPY_DROP;
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplay.AppWellIcon) {
            app = this._appSystem.get_app(source.getId());
        } else if (source instanceof Workspace.WindowClone) {
            app = this._tracker.get_window_app(source.metaWindow);
        }

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient()) {
            return false;
        }

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite) {
            return false;
        } else {
            Mainloop.idle_add(Lang.bind(this, function () {
                AppFavorites.getAppFavorites().addFavorite(id);
                return false;
            }));
        }

        return true;
    }
};

Signals.addSignalMethods(Dash.prototype);
