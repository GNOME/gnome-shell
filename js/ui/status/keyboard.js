// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

try {
    var IBus = imports.gi.IBus;
    if (!('new_async' in IBus.Bus))
        throw "IBus version is too old";
    const IBusCandidatePopup = imports.ui.ibusCandidatePopup;
} catch (e) {
    var IBus = null;
    log(e);
}

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const Util = imports.misc.util;

const DESKTOP_INPUT_SOURCES_SCHEMA = 'org.gnome.desktop.input-sources';
const KEY_CURRENT_INPUT_SOURCE = 'current';
const KEY_INPUT_SOURCES = 'sources';

const INPUT_SOURCE_TYPE_XKB = 'xkb';
const INPUT_SOURCE_TYPE_IBUS = 'ibus';

const IBusManager = new Lang.Class({
    Name: 'IBusManager',

    _init: function(readyCallback) {
        if (!IBus)
            return;

        IBus.init();

        this._readyCallback = readyCallback;
        this._candidatePopup = new IBusCandidatePopup.CandidatePopup();

        this._ibus = null;
        this._panelService = null;
        this._engines = {};
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;

        this._nameWatcherId = Gio.DBus.session.watch_name(IBus.SERVICE_IBUS,
                                                          Gio.BusNameWatcherFlags.NONE,
                                                          Lang.bind(this, this._onNameAppeared),
                                                          Lang.bind(this, this._clear));
    },

    _clear: function() {
        if (this._panelService)
            this._panelService.destroy();
        if (this._ibus)
            this._ibus.destroy();

        this._ibus = null;
        this._panelService = null;
        this._candidatePopup.setPanelService(null);
        this._engines = {};
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;
    },

    _onNameAppeared: function() {
        this._ibus = IBus.Bus.new_async();
        this._ibus.connect('connected', Lang.bind(this, this._onConnected));
    },

    _onConnected: function() {
        this._ibus.list_engines_async(-1, null, Lang.bind(this, this._initEngines));
        this._ibus.request_name_async(IBus.SERVICE_PANEL,
                                      IBus.BusNameFlag.REPLACE_EXISTING,
                                      -1, null,
                                      Lang.bind(this, this._initPanelService));
        this._ibus.connect('disconnected', Lang.bind(this, this._clear));
    },

    _initEngines: function(ibus, result) {
        let enginesList = this._ibus.list_engines_async_finish(result);
        if (enginesList) {
            for (let i = 0; i < enginesList.length; ++i) {
                let name = enginesList[i].get_name();
                this._engines[name] = enginesList[i];
            }
        } else {
            this._clear();
            return;
        }

        this._updateReadiness();
    },

    _initPanelService: function(ibus, result) {
        let success = this._ibus.request_name_async_finish(result);
        if (success) {
            this._panelService = new IBus.PanelService({ connection: this._ibus.get_connection(),
                                                         object_path: IBus.PATH_PANEL });
            this._candidatePopup.setPanelService(this._panelService);
            // Need to set this to get 'global-engine-changed' emitions
            this._ibus.set_watch_ibus_signal(true);
            this._ibus.connect('global-engine-changed', Lang.bind(this, this._engineChanged));
            this._panelService.connect('update-property', Lang.bind(this, this._updateProperty));
            // If an engine is already active we need to get its properties
            this._ibus.get_global_engine_async(-1, null, Lang.bind(this, function(i, result) {
                let engine = this._ibus.get_global_engine_async_finish(result);
                if (!engine)
                    return;
                this._engineChanged(this._ibus, engine.get_name());
            }));
        } else {
            this._clear();
            return;
        }

        this._updateReadiness();
    },

    _updateReadiness: function() {
        this._ready = (Object.keys(this._engines).length > 0 &&
                       this._panelService != null);

        if (this._ready && this._readyCallback)
            this._readyCallback();
    },

    _engineChanged: function(bus, engineName) {
        this._currentEngineName = engineName;

        if (this._registerPropertiesId != 0)
            return;

        this._registerPropertiesId =
            this._panelService.connect('register-properties', Lang.bind(this, function(p, props) {
                if (!props.get(0))
                    return;

                this._panelService.disconnect(this._registerPropertiesId);
                this._registerPropertiesId = 0;

                this.emit('properties-registered', this._currentEngineName, props);
            }));
    },

    _updateProperty: function(panel, prop) {
        this.emit('property-updated', this._currentEngineName, prop);
    },

    activateProperty: function(key, state) {
        this._panelService.property_activate(key, state);
    },

    getEngineDesc: function(id) {
        if (!IBus || !this._ready)
            return null;

        return this._engines[id];
    }
});
Signals.addSignalMethods(IBusManager.prototype);

const LayoutMenuItem = new Lang.Class({
    Name: 'LayoutMenuItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function(displayName, shortName) {
        this.parent();

        this.label = new St.Label({ text: displayName });
        this.indicator = new St.Label({ text: shortName });
        this.addActor(this.label);
        this.addActor(this.indicator);
        this.actor.label_actor = this.label;
    }
});

const InputSource = new Lang.Class({
    Name: 'InputSource',

    _init: function(type, id, displayName, shortName, index) {
        this.type = type;
        this.id = id;
        this.displayName = displayName;
        this._shortName = shortName;
        this.index = index;

        this._menuItem = new LayoutMenuItem(this.displayName, this._shortName);
        this._menuItem.connect('activate', Lang.bind(this, this.activate));
        this._indicatorLabel = new St.Label({ text: this._shortName });

        this.properties = null;
    },

    destroy: function() {
        this._menuItem.destroy();
        this._indicatorLabel.destroy();
    },

    get shortName() {
        return this._shortName;
    },

    set shortName(v) {
        this._shortName = v;
        this._menuItem.indicator.set_text(v);
        this._indicatorLabel.set_text(v);
    },

    get menuItem() {
        return this._menuItem;
    },

    get indicatorLabel() {
        return this._indicatorLabel;
    },

    activate: function() {
        this.emit('activate');
    },
});
Signals.addSignalMethods(InputSource.prototype);

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

        // All valid input sources currently in the gsettings
        // KEY_INPUT_SOURCES list indexed by their index there
        this._inputSources = {};
        // All valid input sources currently in the gsettings
        // KEY_INPUT_SOURCES list of type INPUT_SOURCE_TYPE_IBUS
        // indexed by the IBus ID
        this._ibusSources = {};

        this._currentSource = null;

        this._settings = new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_SCHEMA });
        this._settings.connect('changed::' + KEY_CURRENT_INPUT_SOURCE, Lang.bind(this, this._currentInputSourceChanged));
        this._settings.connect('changed::' + KEY_INPUT_SOURCES, Lang.bind(this, this._inputSourcesChanged));

        this._xkbInfo = new GnomeDesktop.XkbInfo();

        this._propSeparator = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(this._propSeparator);
        this._propSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._propSection);
        this._propSection.actor.hide();

        this._ibusManager = new IBusManager(Lang.bind(this, this._inputSourcesChanged));
        this._ibusManager.connect('properties-registered', Lang.bind(this, this._ibusPropertiesRegistered));
        this._ibusManager.connect('property-updated', Lang.bind(this, this._ibusPropertyUpdated));
        this._inputSourcesChanged();

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._showLayoutItem = this.menu.addAction(_("Show Keyboard Layout"), Lang.bind(this, this._showLayout));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();

        this.menu.addSettingsAction(_("Region and Language Settings"), 'gnome-region-panel.desktop');
    },

    _sessionUpdated: function() {
        // re-using "allowSettings" for the keyboard layout is a bit shady,
        // but at least for now it is used as "allow popping up windows
        // from shell menus"; we can always add a separate sessionMode
        // option if need arises.
        this._showLayoutItem.actor.visible = Main.sessionMode.allowSettings;
    },

    _currentInputSourceChanged: function() {
        let nVisibleSources = Object.keys(this._inputSources).length;
        let newSourceIndex = this._settings.get_uint(KEY_CURRENT_INPUT_SOURCE);
        let newSource = this._inputSources[newSourceIndex];

        if (!newSource || (nVisibleSources < 2 && !newSource.properties)) {
            // This source index might be invalid if we weren't able
            // to build a menu item for it, so we hide ourselves since
            // we can't fix it here. *shrug*

            // We also hide if we have only one visible source unless
            // it's an IBus source with properties.
            this.menu.close();
            this.actor.hide();
            return;
        }

        this.actor.show();

        let oldSource;
        [oldSource, this._currentSource] = [this._currentSource, newSource];

        if (oldSource) {
            oldSource.menuItem.setShowDot(false);
            this._container.set_skip_paint(oldSource.indicatorLabel, true);
        }

        newSource.menuItem.setShowDot(true);
        this._container.set_skip_paint(newSource.indicatorLabel, false);

        this._buildPropSection(newSource.properties);
    },

    _inputSourcesChanged: function() {
        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        let nSources = sources.n_children();

        for (let i in this._inputSources)
            this._inputSources[i].destroy();

        this._inputSources = {};
        this._ibusSources = {};

        let inputSourcesByShortName = {};

        for (let i = 0; i < nSources; i++) {
            let displayName;
            let shortName;
            let [type, id] = sources.get_child_value(i).deep_unpack();
            let exists = false;

            if (type == INPUT_SOURCE_TYPE_XKB) {
                [exists, displayName, shortName, , ] =
                    this._xkbInfo.get_layout_info(id);
            } else if (type == INPUT_SOURCE_TYPE_IBUS) {
                let engineDesc = this._ibusManager.getEngineDesc(id);
                if (engineDesc) {
                    let language = IBus.get_language_name(engineDesc.get_language());
                    exists = true;
                    displayName = language + ' (' + engineDesc.get_longname() + ')';
                    shortName = this._makeEngineShortName(engineDesc);
                }
            }

            if (!exists)
                continue;

            let is = new InputSource(type, id, displayName, shortName, i);

            is.connect('activate', Lang.bind(this, function() {
                this._settings.set_value(KEY_CURRENT_INPUT_SOURCE,
                                         GLib.Variant.new_uint32(is.index));
            }));

            if (!(is.shortName in inputSourcesByShortName))
                inputSourcesByShortName[is.shortName] = [];
            inputSourcesByShortName[is.shortName].push(is);

            this._inputSources[is.index] = is;

            if (is.type == INPUT_SOURCE_TYPE_IBUS)
                this._ibusSources[is.id] = is;
        }

        let menuIndex = 0;
        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            if (inputSourcesByShortName[is.shortName].length > 1) {
                let sub = inputSourcesByShortName[is.shortName].indexOf(is) + 1;
                is.shortName += String.fromCharCode(0x2080 + sub);
            }

            this.menu.addMenuItem(is.menuItem, menuIndex++);

            this._container.add_actor(is.indicatorLabel);
            this._container.set_skip_paint(is.indicatorLabel, true);
        }

        this._currentInputSourceChanged();
    },

    _showLayout: function() {
        Main.overview.hide();

        let source = this._currentSource;
        let xkbLayout = '';
        let xkbVariant = '';

        if (source.type == INPUT_SOURCE_TYPE_XKB) {
            [, , , xkbLayout, xkbVariant] = this._xkbInfo.get_layout_info(source.id);
        } else if (source.type == INPUT_SOURCE_TYPE_IBUS) {
            let engineDesc = this._ibusManager.getEngineDesc(source.id);
            if (engineDesc) {
                xkbLayout = engineDesc.get_layout();
                xkbVariant = '';
            }
        }

        if (!xkbLayout || xkbLayout.length == 0)
            return;

        let description = xkbLayout;
        if (xkbVariant.length > 0)
            description = description + '\t' + xkbVariant;

        Util.spawn(['gkbd-keyboard-display', '-l', description]);
    },

    _makeEngineShortName: function(engineDesc) {
        let symbol = engineDesc.get_symbol();
        if (symbol && symbol[0])
            return symbol;

        let langCode = engineDesc.get_language().split('_', 1)[0];
        if (langCode.length == 2 || langCode.length == 3)
            return langCode.toLowerCase();

        return String.fromCharCode(0x2328); // keyboard glyph
    },

    _ibusPropertiesRegistered: function(im, engineName, props) {
        let source = this._ibusSources[engineName];
        if (!source)
            return;

        source.properties = props;

        if (source == this._currentSource)
            this._currentInputSourceChanged();
    },

    _ibusPropertyUpdated: function(im, engineName, prop) {
        let source = this._ibusSources[engineName];
        if (!source)
            return;

        if (this._updateSubProperty(source.properties, prop) &&
            source == this._currentSource)
            this._currentInputSourceChanged();
    },

    _updateSubProperty: function(props, prop) {
        if (!props)
            return false;

        let p;
        for (let i = 0; (p = props.get(i)) != null; ++i) {
            if (p.get_key() == prop.get_key() && p.get_prop_type() == prop.get_prop_type()) {
                p.update(prop);
                return true;
            } else if (p.get_prop_type() == IBus.PropType.MENU) {
                if (this._updateSubProperty(p.get_sub_props(), prop))
                    return true;
            }
        }
        return false;
    },

    _buildPropSection: function(properties) {
        this._propSeparator.actor.hide();
        this._propSection.actor.hide();
        this._propSection.removeAll();

        this._buildPropSubMenu(this._propSection, properties);

        if (!this._propSection.isEmpty()) {
            this._propSection.actor.show();
            this._propSeparator.actor.show();
        }
    },

    _buildPropSubMenu: function(menu, props) {
        if (!props)
            return;

        let radioGroup = [];
        let p;
        for (let i = 0; (p = props.get(i)) != null; ++i) {
            let prop = p;

            if (!prop.get_visible())
                continue;

            if (prop.get_key() == 'InputMode') {
                let text;
                if (prop.get_symbol)
                    text = prop.get_symbol().get_text();
                else
                    text = prop.get_label().get_text();

                if (text && text.length > 0 && text.length < 3)
                    this._currentSource.indicatorLabel.set_text(text);
            }

            let item;
            switch (prop.get_prop_type()) {
            case IBus.PropType.MENU:
                item = new PopupMenu.PopupSubMenuMenuItem(prop.get_label().get_text());
                this._buildPropSubMenu(item.menu, prop.get_sub_props());
                break;

            case IBus.PropType.RADIO:
                item = new PopupMenu.PopupMenuItem(prop.get_label().get_text());
                item.prop = prop;
                radioGroup.push(item);
                item.radioGroup = radioGroup;
                item.setShowDot(prop.get_state() == IBus.PropState.CHECKED);
                item.connect('activate', Lang.bind(this, function() {
                    if (item.prop.get_state() == IBus.PropState.CHECKED)
                        return;

                    let group = item.radioGroup;
                    for (let i = 0; i < group.length; ++i) {
                        if (group[i] == item) {
                            item.setShowDot(true);
                            item.prop.set_state(IBus.PropState.CHECKED);
                            this._ibusManager.activateProperty(item.prop.get_key(),
                                                               IBus.PropState.CHECKED);
                        } else {
                            group[i].setShowDot(false);
                            group[i].prop.set_state(IBus.PropState.UNCHECKED);
                            this._ibusManager.activateProperty(group[i].prop.get_key(),
                                                               IBus.PropState.UNCHECKED);
                        }
                    }
                }));
                break;

            case IBus.PropType.TOGGLE:
                item = new PopupMenu.PopupSwitchMenuItem(prop.get_label().get_text(), prop.get_state() == IBus.PropState.CHECKED);
                item.prop = prop;
                item.connect('toggled', Lang.bind(this, function() {
                    if (item.state) {
                        item.prop.set_state(IBus.PropState.CHECKED);
                        this._ibusManager.activateProperty(item.prop.get_key(),
                                                           IBus.PropState.CHECKED);
                    } else {
                        item.prop.set_state(IBus.PropState.UNCHECKED);
                        this._ibusManager.activateProperty(item.prop.get_key(),
                                                           IBus.PropState.UNCHECKED);
                    }
                }));
                break;

            case IBus.PropType.NORMAL:
                item = new PopupMenu.PopupMenuItem(prop.get_label().get_text());
                item.prop = prop;
                item.connect('activate', Lang.bind(this, function() {
                    this._ibusManager.activateProperty(item.prop.get_key(),
                                                       IBus.PropState.CHECKED);
                }));
                break;

            case IBus.PropType.SEPARATOR:
                item = new PopupMenu.PopupSeparatorMenuItem();
                break;

            default:
                log ('IBus property %s has invalid type %d'.format(prop.get_key(), type));
                continue;
            }

            item.setSensitive(prop.get_sensitive());
            menu.addMenuItem(item);
        }
    },

    _containerGetPreferredWidth: function(container, for_height, alloc) {
        // Here, and in _containerGetPreferredHeight, we need to query
        // for the height of all children, but we ignore the results
        // for those we don't actually display.
        let max_min_width = 0, max_natural_width = 0;

        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            let [min_width, natural_width] = is.indicatorLabel.get_preferred_width(for_height);
            max_min_width = Math.max(max_min_width, min_width);
            max_natural_width = Math.max(max_natural_width, natural_width);
        }

        alloc.min_size = max_min_width;
        alloc.natural_size = max_natural_width;
    },

    _containerGetPreferredHeight: function(container, for_width, alloc) {
        let max_min_height = 0, max_natural_height = 0;

        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            let [min_height, natural_height] = is.indicatorLabel.get_preferred_height(for_width);
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

        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            is.indicatorLabel.allocate_align_fill(box, 0.5, 0, false, false, flags);
        }
    }
});
