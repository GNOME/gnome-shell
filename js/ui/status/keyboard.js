// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

try {
    var IBus = imports.gi.IBus;
    const CandidatePanel = imports.ui.status.candidatePanel;
} catch (e) {
    var IBus = null;
}

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const Util = imports.misc.util;

const DESKTOP_INPUT_SOURCES_KEYBINDINGS_SCHEMA = 'org.gnome.desktop.input-sources.keybindings';
const DESKTOP_INPUT_SOURCES_SCHEMA = 'org.gnome.desktop.input-sources';
const KEY_CURRENT_IS    = 'current';
const KEY_INPUT_SOURCES = 'sources';

const LayoutMenuItem = new Lang.Class({
    Name: 'LayoutMenuItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function(name, shortName, xkbLayout, xkbVariant, ibusEngine) {
        this.parent();

        this.label = new St.Label({ text: name });
        this.indicator = new St.Label({ text: shortName });
        this.addActor(this.label);
        this.addActor(this.indicator);

        this.sourceName = name;
        this.shortName = shortName;
        this.xkbLayout = xkbLayout;
        this.xkbVariant = xkbVariant;
        this.ibusEngine = ibusEngine;
    }
});

const InputSourceIndicator = new Lang.Class({
    Name: 'InputSourceIndicator',
    Extends: PanelMenu.Button,

    _init: function() {
        this.parent(0.0, _("Keyboard"));

        this._container = new Shell.GenericContainer();
        this._container.connect('get-preferred-width', Lang.bind(this, this._containerGetPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._containerGetPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._containerAllocate));
        this.actor.add_actor(this._container);
        this.actor.add_style_class_name('panel-status-button');

        this._labelActors = [ ];
        this._layoutItems = [ ];

        this._settings = new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_SCHEMA });
        this._settings.connect('changed::' + KEY_CURRENT_IS, Lang.bind(this, this._currentISChanged));
        this._settings.connect('changed::' + KEY_INPUT_SOURCES, Lang.bind(this, this._inputSourcesChanged));

        if (IBus)
            this._ibusInit();

        this._inputSourcesChanged();

        if (global.session_type == Shell.SessionType.USER) {
            this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
            this.menu.addAction(_("Show Keyboard Layout"), Lang.bind(this, function() {
                Main.overview.hide();
                let description = this._selectedLayout.xkbLayout;
                if (this._selectedLayout.xkbVariant.length > 0)
                    description = description + '\t' + this._selectedLayout.xkbVariant;
                Util.spawn(['gkbd-keyboard-display', '-l', description]);
            }));
        }
        this.menu.addSettingsAction(_("Region and Language Settings"), 'gnome-region-panel.desktop');

        global.display.add_keybinding('switch-next',
                                      new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_KEYBINDINGS_SCHEMA }),
                                      Meta.KeyBindingFlags.NONE,
                                      Lang.bind(this, this._switchNext));
        global.display.add_keybinding('switch-previous',
                                      new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_KEYBINDINGS_SCHEMA }),
                                      Meta.KeyBindingFlags.NONE,
                                      Lang.bind(this, this._switchPrevious));
    },

    _ibusInit: function() {
        IBus.init();
        this._ibus = new IBus.Bus();
        if (!this._ibus.is_connected()) {
            log('ibus-daemon is not running');
            return;
        }

        this._ibus.request_name(IBus.SERVICE_PANEL,
                                IBus.BusNameFlag.ALLOW_REPLACEMENT |
                                IBus.BusNameFlag.REPLACE_EXISTING);
        this._panel = new IBus.PanelService({ connection: this._ibus.get_connection(),
                                              object_path: IBus.PATH_PANEL });
        this._ibusInitPanelService();

        this._candidatePanel = new CandidatePanel.CandidatePanel();
        this._ibusInitCandidatePanel();
    },

    _ibusInitCandidatePanel: function() {
        this._candidatePanel.connect('cursor-up',
                                     Lang.bind(this, function(widget) {
                                         this.cursorUp();
                                     }));
        this._candidatePanel.connect('cursor-down',
                                     Lang.bind(this, function(widget) {
                                         this.cursorDown();
                                     }));
        this._candidatePanel.connect('page-up',
                                     Lang.bind(this, function(widget) {
                                         this.pageUp();
                                     }));
        this._candidatePanel.connect('page-down',
                                     Lang.bind(this, function(widget) {
                                         this.pageDown();
                                     }));
        this._candidatePanel.connect('candidate-clicked',
                                     Lang.bind(this, function(widget, index, button, state) {
                                         this.candidateClicked(index, button, state);
                                     }));
    },

    _ibusInitPanelService: function() {
        this._panel.connect('set-cursor-location',
                            Lang.bind(this, this.setCursorLocation));
        this._panel.connect('update-preedit-text',
                            Lang.bind(this, this.updatePreeditText));
        this._panel.connect('show-preedit-text',
                            Lang.bind(this, this.showPreeditText));
        this._panel.connect('hide-preedit-text',
                            Lang.bind(this, this.hidePreeditText));
        this._panel.connect('update-auxiliary-text',
                            Lang.bind(this, this.updateAuxiliaryText));
        this._panel.connect('show-auxiliary-text',
                            Lang.bind(this, this.showAuxiliaryText));
        this._panel.connect('hide-auxiliary-text',
                            Lang.bind(this, this.hideAuxiliaryText));
        this._panel.connect('update-lookup-table',
                            Lang.bind(this, this.updateLookupTable));
        this._panel.connect('show-lookup-table',
                            Lang.bind(this, this.showLookupTable));
        this._panel.connect('hide-lookup-table',
                            Lang.bind(this, this.hideLookupTable));
        this._panel.connect('page-up-lookup-table',
                            Lang.bind(this, this.pageUpLookupTable));
        this._panel.connect('page-down-lookup-table',
                            Lang.bind(this, this.pageDownLookupTable));
        this._panel.connect('cursor-up-lookup-table',
                            Lang.bind(this, this.cursorUpLookupTable));
        this._panel.connect('cursor-down-lookup-table',
                            Lang.bind(this, this.cursorDownLookupTable));
        this._panel.connect('focus-in', Lang.bind(this, this.focusIn));
        this._panel.connect('focus-out', Lang.bind(this, this.focusOut));
    },

    setCursorLocation: function(panel, x, y, w, h) {
        this._candidatePanel.setCursorLocation(x, y, w, h);
    },

    updatePreeditText: function(panel, text, cursorPos, visible) {
        this._candidatePanel.updatePreeditText(text, cursorPos, visible);
    },

    showPreeditText: function(panel) {
        this._candidatePanel.showPreeditText();
    },

    hidePreeditText: function(panel) {
        this._candidatePanel.hidePreeditText();
    },

    updateAuxiliaryText: function(panel, text, visible) {
        this._candidatePanel.updateAuxiliaryText(text, visible);
    },

    showAuxiliaryText: function(panel) {
        this._candidatePanel.showAuxiliaryText();
    },

    hideAuxiliaryText: function(panel) {
        this._candidatePanel.hideAuxiliaryText();
    },

    updateLookupTable: function(panel, lookupTable, visible) {
        this._candidatePanel.updateLookupTable(lookupTable, visible);
    },

    showLookupTable: function(panel) {
        this._candidatePanel.showLookupTable();
    },

    hideLookupTable: function(panel) {
        this._candidatePanel.hideLookupTable();
    },

    pageUpLookupTable: function(panel) {
        this._candidatePanel.pageUpLookupTable();
    },

    pageDownLookupTable: function(panel) {
        this._candidatePanel.pageDownLookupTable();
    },

    cursorUpLookupTable: function(panel) {
        this._candidatePanel.cursorUpLookupTable();
    },

    cursorDownLookupTable: function(panel) {
        this._candidatePanel.cursorDownLookupTable();
    },

    focusIn: function(panel, path) {
    },

    focusOut: function(panel, path) {
        this._candidatePanel.reset();
    },

    cursorUp: function() {
        this._panel.cursor_up();
    },

    cursorDown: function() {
        this._panel.cursor_down();
    },

    pageUp: function() {
        this._panel.page_up();
    },

    pageDown: function() {
        this._panel.page_down();
    },

    candidateClicked: function(index, button, state) {
        this._panel.candidate_clicked(index, button, state);
    },

    _currentISChanged: function() {
        let source = this._settings.get_value(KEY_CURRENT_IS);
        let name = source.get_child_value(0).get_string()[0];

        if (this._selectedLayout) {
            this._selectedLayout.setShowDot(false);
            this._selectedLayout = null;
        }

        if (this._selectedLabel) {
            this._container.set_skip_paint(this._selectedLabel, true);
            this._selectedLabel = null;
        }

        for (let i = 0; i < this._layoutItems.length; ++i) {
            let item = this._layoutItems[i];
            if (item.sourceName == name) {
                item.setShowDot(true);
                this._selectedLayout = item;
                break;
            }
        }

        for (let i = 0; i < this._labelActors.length; ++i) {
            let actor = this._labelActors[i];
            if (actor.sourceName == name) {
                this._selectedLabel = actor;
                this._container.set_skip_paint(actor, false);
                break;
            }
        }

        if (!this._selectedLayout || !this._selectedLabel)
            this._layoutItems[0].activate();
    },

    _inputSourcesChanged: function() {
        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        if (sources.n_children() > 1) {
            this.actor.show();
        } else {
            this.menu.close();
            this.actor.hide();
        }

        for (let i = 0; i < this._layoutItems.length; i++)
            this._layoutItems[i].destroy();

        for (let i = 0; i < this._labelActors.length; i++)
            this._labelActors[i].destroy();

        this._selectedLayout = null;
        this._layoutItems = [ ];
        this._selectedLabel = null;
        this._labelActors = [ ];

        for (let i = 0; i < sources.n_children(); ++i) {
            let name = sources.get_child_value(i).get_child_value(0).get_string()[0];
            let shortName = sources.get_child_value(i).get_child_value(1).get_string()[0];
            let xkbLayout = sources.get_child_value(i).get_child_value(2).get_string()[0];
            let xkbVariant = sources.get_child_value(i).get_child_value(3).get_string()[0];
            let ibusEngine = sources.get_child_value(i).get_child_value(4).get_string()[0];

            let item = new LayoutMenuItem(name, shortName, xkbLayout, xkbVariant, ibusEngine);
            this._layoutItems.push(item);
            this.menu.addMenuItem(item, i);
            item.connect('activate', Lang.bind(this, function() {
                if (this._selectedLayout == null || item.sourceName != this._selectedLayout.sourceName) {
                    let name = GLib.Variant.new_string(item.sourceName);
                    let shortName = GLib.Variant.new_string(item.shortName);
                    let xkbLayout = GLib.Variant.new_string(item.xkbLayout);
                    let xkbVariant = GLib.Variant.new_string(item.xkbVariant);
                    let ibusEngine = GLib.Variant.new_string(item.ibusEngine);
                    let tuple = GLib.Variant.new_tuple([name, shortName, xkbLayout, xkbVariant, ibusEngine], 5);
                    this._settings.set_value(KEY_CURRENT_IS, tuple);
                }
            }));

            let shortLabel = new St.Label({ text: shortName });
            shortLabel.sourceName = name;
            this._labelActors.push(shortLabel);
            this._container.add_actor(shortLabel);
            this._container.set_skip_paint(shortLabel, true);
        }

        this._currentISChanged();
    },

    _switchNext: function() {
        if (!this._selectedLayout || !this._selectedLabel) {
            this._layoutItems[0].activate();
            return;
          }
        for (let i = 0; i < this._layoutItems.length; ++i) {
            let item = this._layoutItems[i];
            if (item.sourceName == this._selectedLayout.sourceName) {
                this._layoutItems[(++i == this._layoutItems.length) ? 0 : i].activate();
                break;
            }
        }
    },

    _switchPrevious: function() {
        if (!this._selectedLayout || !this._selectedLabel) {
            this._layoutItems[0].activate();
            return;
        }
        for (let i = 0; i < this._layoutItems.length; ++i) {
            let item = this._layoutItems[i];
            if (item.sourceName == this._selectedLayout.sourceName) {
                this._layoutItems[(--i == -1) ? (this._layoutItems.length - 1) : i].activate();
                break;
            }
        }
    },

    _containerGetPreferredWidth: function(container, for_height, alloc) {
        // Here, and in _containerGetPreferredHeight, we need to query
        // for the height of all children, but we ignore the results
        // for those we don't actually display.
        let max_min_width = 0, max_natural_width = 0;

        for (let i = 0; i < this._labelActors.length; i++) {
            let [min_width, natural_width] = this._labelActors[i].get_preferred_width(for_height);
            max_min_width = Math.max(max_min_width, min_width);
            max_natural_width = Math.max(max_natural_width, natural_width);
        }

        alloc.min_size = max_min_width;
        alloc.natural_size = max_natural_width;
    },

    _containerGetPreferredHeight: function(container, for_width, alloc) {
        let max_min_height = 0, max_natural_height = 0;

        for (let i = 0; i < this._labelActors.length; i++) {
            let [min_height, natural_height] = this._labelActors[i].get_preferred_height(for_width);
            max_min_height = Math.max(max_min_height, min_height);
            max_natural_height = Math.max(max_natural_height, natural_height);
        }

        alloc.min_size = max_min_height;
        alloc.natural_size = max_natural_height;
    },

    _containerAllocate: function(container, box, flags) {
        // translate box to (0, 0)
        box.x2 -= box.x1;
        box.x1 = 0;
        box.y2 -= box.y1;
        box.y1 = 0;

        for (let i = 0; i < this._labelActors.length; i++)
            this._labelActors[i].allocate_align_fill(box, 0.5, 0, false, false, flags);
    }
});
