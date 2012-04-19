// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

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

    _init: function(name, layout, engine) {
        this.parent();

        this.label = new St.Label({ text: name });
        this.indicator = new St.Label({ text: layout });
        this.addActor(this.label);
        this.addActor(this.indicator);

        this.name = name;
        this.layout = layout;
        this.engine = engine;
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

        this._inputSourcesChanged();

        if (global.session_type == Shell.SessionType.USER) {
            this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
            this.menu.addAction(_("Show Keyboard Layout"), Lang.bind(this, function() {
                Main.overview.hide();
                Util.spawn(['gkbd-keyboard-display', '-l', String(this._selectedLayout.layout)]);
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

    _currentISChanged: function() {
        let source = this._settings.get_value(KEY_CURRENT_IS);
        let name = source.get_child_value(0).get_string()[0];
        let layout = source.get_child_value(1).get_string()[0];
        let engine = source.get_child_value(2).get_string()[0];

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
            if (item.name == name) {
                item.setShowDot(true);
                this._selectedLayout = item;
                break;
            }
        }

        for (let i = 0; i < this._labelActors.length; ++i) {
            let actor = this._labelActors[i];
            if (actor.text == layout) {
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
            let layout = sources.get_child_value(i).get_child_value(1).get_string()[0];
            let engine = sources.get_child_value(i).get_child_value(2).get_string()[0];

            let item = new LayoutMenuItem(name, layout, engine);
            this._layoutItems.push(item);
            this.menu.addMenuItem(item, i);
            item.connect('activate', Lang.bind(this, function() {
                if (this._selectedLayout == null || item.name != this._selectedLayout.name) {
                    let name = GLib.Variant.new_string(item.name);
                    let layout = GLib.Variant.new_string(item.layout);
                    let engine = GLib.Variant.new_string(item.engine);
                    let tuple = GLib.Variant.new_tuple([name, layout, engine], 3);
                    this._settings.set_value(KEY_CURRENT_IS, tuple);
                }
            }));

            let shortLabel = new St.Label({ text: layout });
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
            if (item.name == this._selectedLayout.name) {
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
            if (item.name == this._selectedLayout.name) {
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
