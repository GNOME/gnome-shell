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
            this._ibus.connect('global-engine-changed', Lang.bind(this, this._resetProperties));
            this._panelService.connect('update-property', Lang.bind(this, this._updateProperty));
            this._resetProperties();
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

    _resetProperties: function() {
        this.emit('properties-registered', null);

        if (this._registerPropertiesId != 0)
            return;

        this._registerPropertiesId =
            this._panelService.connect('register-properties', Lang.bind(this, function(p, props) {
                if (!props.get(0))
                    return;

                this._panelService.disconnect(this._registerPropertiesId);
                this._registerPropertiesId = 0;

                this.emit('properties-registered', props);
            }));
    },

    _updateProperty: function(panel, prop) {
        this.emit('property-updated', prop);
    },

    activateProperty: function(key, state) {
        this._panelService.property_activate(key, state);
    },

    hasProperties: function(id) {
        if (id == 'anthy')
            return true;

        return false;
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

const InputSourceIndicator = new Lang.Class({
    Name: 'InputSourceIndicator',
    Extends: PanelMenu.Button,

    _propertiesWhitelist: [
        'InputMode',
        'TypingMode',
        'DictMode'
    ],

    _init: function() {
        this.parent(0.0, _("Keyboard"));

        this._container = new Shell.GenericContainer();
        this._container.connect('get-preferred-width', Lang.bind(this, this._containerGetPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._containerGetPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._containerAllocate));
        this.actor.add_actor(this._container);
        this.actor.add_style_class_name('panel-status-button');

        this._labelActors = {};
        this._layoutItems = {};

        this._settings = new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_SCHEMA });
        this._settings.connect('changed::' + KEY_CURRENT_INPUT_SOURCE, Lang.bind(this, this._currentInputSourceChanged));
        this._settings.connect('changed::' + KEY_INPUT_SOURCES, Lang.bind(this, this._inputSourcesChanged));

        this._currentSourceIndex = this._settings.get_uint(KEY_CURRENT_INPUT_SOURCE);
        this._xkbInfo = new GnomeDesktop.XkbInfo();

        this._propSeparator = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(this._propSeparator);
        this._propSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._propSection);
        this._propSection.actor.hide();

        this._properties = null;

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
        let nVisibleSources = Object.keys(this._layoutItems).length;
        let newCurrentSourceIndex = this._settings.get_uint(KEY_CURRENT_INPUT_SOURCE);
        let newLayoutItem = this._layoutItems[newCurrentSourceIndex];
        let hasProperties;

        if (newLayoutItem)
            hasProperties = this._ibusManager.hasProperties(newLayoutItem.ibusEngineId);
        else
            hasProperties = false;

        if (!newLayoutItem || (nVisibleSources < 2 && !hasProperties)) {
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

        if (this._layoutItems[this._currentSourceIndex]) {
            this._layoutItems[this._currentSourceIndex].setShowDot(false);
            this._container.set_skip_paint(this._labelActors[this._currentSourceIndex], true);
        }

        newLayoutItem.setShowDot(true);

        let newLabelActor = this._labelActors[newCurrentSourceIndex];
        this._container.set_skip_paint(newLabelActor, false);

        if (hasProperties)
            newLabelActor.set_text(newLayoutItem.indicator.get_text());

        this._currentSourceIndex = newCurrentSourceIndex;
    },

    _inputSourcesChanged: function() {
        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        let nSources = sources.n_children();

        for (let i in this._layoutItems)
            this._layoutItems[i].destroy();

        for (let i in this._labelActors)
            this._labelActors[i].destroy();

        this._layoutItems = {};
        this._labelActors = {};

        let infos = [];
        let infosByShortName = {};

        for (let i = 0; i < nSources; i++) {
            let info = { exists: false };
            let [type, id] = sources.get_child_value(i).deep_unpack();

            if (type == INPUT_SOURCE_TYPE_XKB) {
                [info.exists, info.displayName, info.shortName, , ] =
                    this._xkbInfo.get_layout_info(id);
            } else if (type == INPUT_SOURCE_TYPE_IBUS) {
                let engineDesc = this._ibusManager.getEngineDesc(id);
                if (engineDesc) {
                    let language = IBus.get_language_name(engineDesc.get_language());

                    info.exists = true;
                    info.displayName = language + ' (' + engineDesc.get_longname() + ')';
                    info.shortName = this._makeEngineShortName(engineDesc);
                    info.ibusEngineId = id;
                }
            }

            if (!info.exists)
                continue;

            info.sourceIndex = i;

            if (!(info.shortName in infosByShortName))
                infosByShortName[info.shortName] = [];
            infosByShortName[info.shortName].push(info);
            infos.push(info);
        }

        for (let i = 0; i < infos.length; i++) {
            let info = infos[i];
            if (infosByShortName[info.shortName].length > 1) {
                let sub = infosByShortName[info.shortName].indexOf(info) + 1;
                info.shortName += String.fromCharCode(0x2080 + sub);
            }

            let item = new LayoutMenuItem(info.displayName, info.shortName);
            item.ibusEngineId = info.ibusEngineId;
            this._layoutItems[info.sourceIndex] = item;
            this.menu.addMenuItem(item, i);
            item.connect('activate', Lang.bind(this, function() {
                this._settings.set_value(KEY_CURRENT_INPUT_SOURCE,
                                         GLib.Variant.new_uint32(info.sourceIndex));
            }));

            let shortLabel = new St.Label({ text: info.shortName });
            this._labelActors[info.sourceIndex] = shortLabel;
            this._container.add_actor(shortLabel);
            this._container.set_skip_paint(shortLabel, true);
        }

        this._currentInputSourceChanged();
    },

    _showLayout: function() {
        Main.overview.hide();

        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        let current = this._settings.get_uint(KEY_CURRENT_INPUT_SOURCE);
        let [type, id] = sources.get_child_value(current).deep_unpack();
        let xkbLayout = '';
        let xkbVariant = '';

        if (type == INPUT_SOURCE_TYPE_XKB) {
            [, , , xkbLayout, xkbVariant] = this._xkbInfo.get_layout_info(id);
        } else if (type == INPUT_SOURCE_TYPE_IBUS) {
            let engineDesc = this._ibusManager.getEngineDesc(id);
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

    _propertyWhitelisted: function(prop) {
        for (let i = 0; i < this._propertiesWhitelist.length; ++i) {
            let key = prop.get_key();
            if (key.substr(0, this._propertiesWhitelist[i].length) == this._propertiesWhitelist[i])
                return true;
        }
        return false;
    },

    _ibusPropertiesRegistered: function(im, props) {
        this._properties = props;
        this._buildPropSection();
    },

    _ibusPropertyUpdated: function(im, prop) {
        if (!this._propertyWhitelisted(prop))
            return;

        if (this._updateSubProperty(this._properties, prop))
            this._buildPropSection();
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

    _updateIndicatorLabel: function(text) {
        let layoutItem = this._layoutItems[this._currentSourceIndex];
        let hasProperties;

        if (layoutItem)
            hasProperties = this._ibusManager.hasProperties(layoutItem.ibusEngineId);
        else
            hasProperties = false;

        if (hasProperties)
            this._labelActors[this._currentSourceIndex].set_text(text);
    },

    _buildPropSection: function() {
        this._propSeparator.actor.hide();
        this._propSection.actor.hide();
        this._propSection.removeAll();

        this._buildPropSubMenu(this._propSection, this._properties);

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

            if (!this._propertyWhitelisted(prop) ||
                !prop.get_visible())
                continue;

            if (prop.get_key() == 'InputMode') {
                let text;
                if (prop.get_symbol)
                    text = prop.get_symbol().get_text();
                else
                    text = prop.get_label().get_text();

                if (text && text.length > 0 && text.length < 3)
                    this._updateIndicatorLabel(text);
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

        for (let i in this._labelActors) {
            let [min_width, natural_width] = this._labelActors[i].get_preferred_width(for_height);
            max_min_width = Math.max(max_min_width, min_width);
            max_natural_width = Math.max(max_natural_width, natural_width);
        }

        alloc.min_size = max_min_width;
        alloc.natural_size = max_natural_width;
    },

    _containerGetPreferredHeight: function(container, for_width, alloc) {
        let max_min_height = 0, max_natural_height = 0;

        for (let i in this._labelActors) {
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

        for (let i in this._labelActors)
            this._labelActors[i].allocate_align_fill(box, 0.5, 0, false, false, flags);
    }
});
