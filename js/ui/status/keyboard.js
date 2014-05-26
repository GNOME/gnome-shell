// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Gettext = imports.gettext;

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
const SwitcherPopup = imports.ui.switcherPopup;
const Util = imports.misc.util;

const DESKTOP_INPUT_SOURCES_SCHEMA = 'org.gnome.desktop.input-sources';
const KEY_CURRENT_INPUT_SOURCE = 'current';
const KEY_INPUT_SOURCES = 'sources';

const INPUT_SOURCE_TYPE_XKB = 'xkb';
const INPUT_SOURCE_TYPE_IBUS = 'ibus';

// This is the longest we'll keep the keyboard frozen until an input
// source is active.
const MAX_INPUT_SOURCE_ACTIVATION_TIME = 4000; // ms

const BUS_NAME = 'org.gnome.SettingsDaemon.Keyboard';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Keyboard';

const KeyboardManagerInterface = '<node> \
<interface name="org.gnome.SettingsDaemon.Keyboard"> \
<method name="SetInputSource"> \
    <arg type="u" direction="in" /> \
</method> \
</interface> \
</node>';

const KeyboardManagerProxy = Gio.DBusProxy.makeProxyWrapper(KeyboardManagerInterface);

function releaseKeyboard() {
    if (Main.modalCount > 0)
        global.display.unfreeze_keyboard(global.get_current_time());
    else
        global.display.ungrab_keyboard(global.get_current_time());
}

function holdKeyboard() {
    global.freeze_keyboard(global.get_current_time());
}

const IBusManager = new Lang.Class({
    Name: 'IBusManager',

    _init: function(readyCallback) {
        if (!IBus)
            return;

        IBus.init();

        this._readyCallback = readyCallback;
        this._candidatePopup = new IBusCandidatePopup.CandidatePopup();

        this._panelService = null;
        this._engines = {};
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;

        this._ibus = IBus.Bus.new_async();
        this._ibus.connect('connected', Lang.bind(this, this._onConnected));
        this._ibus.connect('disconnected', Lang.bind(this, this._clear));
        // Need to set this to get 'global-engine-changed' emitions
        this._ibus.set_watch_ibus_signal(true);
        this._ibus.connect('global-engine-changed', Lang.bind(this, this._engineChanged));
    },

    _clear: function() {
        if (this._panelService)
            this._panelService.destroy();

        this._panelService = null;
        this._candidatePopup.setPanelService(null);
        this._engines = {};
        this._ready = false;
        this._registerPropertiesId = 0;
        this._currentEngineName = null;

        if (this._readyCallback)
            this._readyCallback(false);
    },

    _onConnected: function() {
        this._ibus.list_engines_async(-1, null, Lang.bind(this, this._initEngines));
        this._ibus.request_name_async(IBus.SERVICE_PANEL,
                                      IBus.BusNameFlag.REPLACE_EXISTING,
                                      -1, null,
                                      Lang.bind(this, this._initPanelService));
    },

    _initEngines: function(ibus, result) {
        let enginesList = this._ibus.list_engines_async_finish(result);
        if (enginesList) {
            for (let i = 0; i < enginesList.length; ++i) {
                let name = enginesList[i].get_name();
                this._engines[name] = enginesList[i];
            }
            this._updateReadiness();
        } else {
            this._clear();
        }
    },

    _initPanelService: function(ibus, result) {
        let success = this._ibus.request_name_async_finish(result);
        if (success) {
            this._panelService = new IBus.PanelService({ connection: this._ibus.get_connection(),
                                                         object_path: IBus.PATH_PANEL });
            this._candidatePopup.setPanelService(this._panelService);
            this._panelService.connect('update-property', Lang.bind(this, this._updateProperty));
            // If an engine is already active we need to get its properties
            this._ibus.get_global_engine_async(-1, null, Lang.bind(this, function(i, result) {
                let engine;
                try {
                    engine = this._ibus.get_global_engine_async_finish(result);
                    if (!engine)
                        return;
                } catch(e) {
                    return;
                }
                this._engineChanged(this._ibus, engine.get_name());
            }));
            this._updateReadiness();
        } else {
            this._clear();
        }
    },

    _updateReadiness: function() {
        this._ready = (Object.keys(this._engines).length > 0 &&
                       this._panelService != null);

        if (this._readyCallback)
            this._readyCallback(this._ready);
    },

    _engineChanged: function(bus, engineName) {
        if (!this._ready)
            return;

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
        this.actor.add(this.label, { expand: true });
        this.actor.add(this.indicator);
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

const InputSourcePopup = new Lang.Class({
    Name: 'InputSourcePopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init: function(items, action, actionBackward) {
        this.parent(items);

        this._action = action;
        this._actionBackward = actionBackward;
    },

    _createSwitcher: function() {
        this._switcherList = new InputSourceSwitcher(this._items);
        return true;
    },

    _initialSelection: function(backward, binding) {
        if (binding == 'switch-input-source') {
            if (backward)
                this._selectedIndex = this._items.length - 1;
        } else if (binding == 'switch-input-source-backward') {
            if (!backward)
                this._selectedIndex = this._items.length - 1;
        }
        this._select(this._selectedIndex);
    },

    _keyPressHandler: function(keysym, backwards, action) {
        if (action == this._action)
            this._select(backwards ? this._previous() : this._next());
        else if (action == this._actionBackward)
            this._select(backwards ? this._next() : this._previous());
        else if (keysym == Clutter.Left)
            this._select(this._previous());
        else if (keysym == Clutter.Right)
            this._select(this._next());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    },

    _finish : function() {
        this.parent();

        this._items[this._selectedIndex].activate();
    },
});

const InputSourceSwitcher = new Lang.Class({
    Name: 'InputSourceSwitcher',
    Extends: SwitcherPopup.SwitcherList,

    _init: function(items) {
        this.parent(true);

        for (let i = 0; i < items.length; i++)
            this._addIcon(items[i]);
    },

    _addIcon: function(item) {
        let box = new St.BoxLayout({ vertical: true });

        let bin = new St.Bin({ style_class: 'input-source-switcher-symbol' });
        let symbol = new St.Label({ text: item.shortName });
        bin.set_child(symbol);
        box.add(bin, { x_fill: false, y_fill: false } );

        let text = new St.Label({ text: item.displayName });
        box.add(text, { x_fill: false });

        this.addItem(box, text);
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

        this._hbox = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        this._hbox.add_child(this._container);
        this._hbox.add_child(PopupMenu.arrowIcon(St.Side.BOTTOM));

        this.actor.add_child(this._hbox);
        this.actor.add_style_class_name('panel-status-button');

        // All valid input sources currently in the gsettings
        // KEY_INPUT_SOURCES list indexed by their index there
        this._inputSources = {};
        // All valid input sources currently in the gsettings
        // KEY_INPUT_SOURCES list of type INPUT_SOURCE_TYPE_IBUS
        // indexed by the IBus ID
        this._ibusSources = {};

        this._currentSource = null;

        // All valid input sources currently in the gsettings
        // KEY_INPUT_SOURCES list ordered by most recently used
        this._mruSources = [];
        this._keybindingAction =
            Main.wm.addKeybinding('switch-input-source',
                                  new Gio.Settings({ schema: "org.gnome.desktop.wm.keybindings" }),
                                  Meta.KeyBindingFlags.REVERSES,
                                  Shell.KeyBindingMode.ALL,
                                  Lang.bind(this, this._switchInputSource));
        this._keybindingActionBackward =
            Main.wm.addKeybinding('switch-input-source-backward',
                                  new Gio.Settings({ schema: "org.gnome.desktop.wm.keybindings" }),
                                  Meta.KeyBindingFlags.REVERSES |
                                  Meta.KeyBindingFlags.REVERSED,
                                  Shell.KeyBindingMode.ALL,
                                  Lang.bind(this, this._switchInputSource));
        this._settings = new Gio.Settings({ schema: DESKTOP_INPUT_SOURCES_SCHEMA });
        this._settings.connect('changed::' + KEY_CURRENT_INPUT_SOURCE, Lang.bind(this, this._currentInputSourceChanged));
        this._settings.connect('changed::' + KEY_INPUT_SOURCES, Lang.bind(this, this._inputSourcesChanged));

        this._xkbInfo = new GnomeDesktop.XkbInfo();

        this._propSeparator = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(this._propSeparator);
        this._propSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._propSection);
        this._propSection.actor.hide();

        this._ibusReady = false;
        this._ibusManager = new IBusManager(Lang.bind(this, this._ibusReadyCallback));
        this._ibusManager.connect('properties-registered', Lang.bind(this, this._ibusPropertiesRegistered));
        this._ibusManager.connect('property-updated', Lang.bind(this, this._ibusPropertyUpdated));
        this._inputSourcesChanged();

        this._keyboardManager = new KeyboardManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                                         function(proxy, error) {
                                                             if (error)
                                                                 log(error.message);
                                                         });
        this._keyboardManager.g_default_timeout = MAX_INPUT_SOURCE_ACTIVATION_TIME;

        global.display.connect('modifiers-accelerator-activated', Lang.bind(this, this._modifiersSwitcher));

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._showLayoutItem = this.menu.addAction(_("Show Keyboard Layout"), Lang.bind(this, this._showLayout));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();

        this._sourcesPerWindow = false;
        this._focusWindowNotifyId = 0;
        this._overviewShowingId = 0;
        this._overviewHiddenId = 0;
        this._settings.connect('changed::per-window', Lang.bind(this, this._sourcesPerWindowChanged));
        this._sourcesPerWindowChanged();
    },

    _sessionUpdated: function() {
        // re-using "allowSettings" for the keyboard layout is a bit shady,
        // but at least for now it is used as "allow popping up windows
        // from shell menus"; we can always add a separate sessionMode
        // option if need arises.
        this._showLayoutItem.actor.visible = Main.sessionMode.allowSettings;
    },

    _ibusReadyCallback: function(ready) {
        if (this._ibusReady == ready)
            return;

        this._ibusReady = ready;
        this._mruSources = [];
        this._inputSourcesChanged();
    },

    _modifiersSwitcher: function() {
        let sourceIndexes = Object.keys(this._inputSources);
        if (sourceIndexes.length == 0) {
            releaseKeyboard();
            return true;
        }

        let is = this._currentSource;
        if (!is)
            is = this._inputSources[sourceIndexes[0]];

        let nextIndex = is.index + 1;
        if (nextIndex > sourceIndexes[sourceIndexes.length - 1])
            nextIndex = 0;

        while (!(is = this._inputSources[nextIndex]))
            nextIndex += 1;

        is.activate();
        return true;
    },

    _switchInputSource: function(display, screen, window, binding) {
        if (this._mruSources.length < 2)
            return;

        // HACK: Fall back on simple input source switching since we
        // can't show a popup switcher while a GrabHelper grab is in
        // effect without considerable work to consolidate the usage
        // of pushModal/popModal and grabHelper. See
        // https://bugzilla.gnome.org/show_bug.cgi?id=695143 .
        if (Main.keybindingMode == Shell.KeyBindingMode.MESSAGE_TRAY ||
            Main.keybindingMode == Shell.KeyBindingMode.TOPBAR_POPUP) {
            this._modifiersSwitcher();
            return;
        }

        let popup = new InputSourcePopup(this._mruSources, this._keybindingAction, this._keybindingActionBackward);
        let modifiers = binding.get_modifiers();
        let backwards = modifiers & Meta.VirtualModifier.SHIFT_MASK;
        if (!popup.show(backwards, binding.get_name(), binding.get_mask()))
            popup.destroy();
    },

    _currentInputSourceChanged: function() {
        let nVisibleSources = Object.keys(this._inputSources).length;
        let newSourceIndex = this._settings.get_uint(KEY_CURRENT_INPUT_SOURCE);
        let newSource = this._inputSources[newSourceIndex];

        let oldSource;
        [oldSource, this._currentSource] = [this._currentSource, newSource];

        if (oldSource) {
            oldSource.menuItem.setOrnament(PopupMenu.Ornament.NONE);
            oldSource.indicatorLabel.hide();
        }

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

        newSource.menuItem.setOrnament(PopupMenu.Ornament.DOT);
        newSource.indicatorLabel.show();

        this._buildPropSection(newSource.properties);

        for (let i = 1; i < this._mruSources.length; ++i)
            if (this._mruSources[i] == newSource) {
                let currentSource = this._mruSources.splice(i, 1);
                this._mruSources = currentSource.concat(this._mruSources);
                break;
            }

        this._changePerWindowSource();
    },

    _inputSourcesChanged: function() {
        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        let nSources = sources.n_children();

        for (let i in this._inputSources)
            this._inputSources[i].destroy();

        this._inputSources = {};
        this._ibusSources = {};
        this._currentSource = null;

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
                    let longName = engineDesc.get_longname();
                    let textdomain = engineDesc.get_textdomain();
                    if (textdomain != '')
                        longName = Gettext.dgettext(textdomain, longName);
                    exists = true;
                    displayName = '%s (%s)'.format(language, longName);
                    shortName = this._makeEngineShortName(engineDesc);
                }
            }

            if (!exists)
                continue;

            let is = new InputSource(type, id, displayName, shortName, i);

            is.connect('activate', Lang.bind(this, function() {
                holdKeyboard();
                this._keyboardManager.SetInputSourceRemote(is.index, releaseKeyboard);
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

            is.indicatorLabel.hide();
            this._container.add_actor(is.indicatorLabel);
        }

        let sourcesList = [];
        for (let i in this._inputSources)
            sourcesList.push(this._inputSources[i]);

        let mruSources = [];
        for (let i = 0; i < this._mruSources.length; i++) {
            for (let j = 0; j < sourcesList.length; j++)
                if (this._mruSources[i].type == sourcesList[j].type &&
                    this._mruSources[i].id == sourcesList[j].id) {
                    mruSources = mruSources.concat(sourcesList.splice(j, 1));
                    break;
                }
        }
        this._mruSources = mruSources.concat(sourcesList);

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
                item.setOrnament(prop.get_state() == IBus.PropState.CHECKED ?
                                 PopupMenu.Ornament.DOT : PopupMenu.Ornament.NONE);
                item.connect('activate', Lang.bind(this, function() {
                    if (item.prop.get_state() == IBus.PropState.CHECKED)
                        return;

                    let group = item.radioGroup;
                    for (let i = 0; i < group.length; ++i) {
                        if (group[i] == item) {
                            item.setOrnament(PopupMenu.Ornament.DOT);
                            item.prop.set_state(IBus.PropState.CHECKED);
                            this._ibusManager.activateProperty(item.prop.get_key(),
                                                               IBus.PropState.CHECKED);
                        } else {
                            group[i].setOrnament(PopupMenu.Ornament.NONE);
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
                                                       item.prop.get_state());
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

    _getNewInputSource: function(current) {
        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            if (is.type == current.type &&
                is.id == current.id)
                return is;
        }
        return this._currentSource;
    },

    _getCurrentWindow: function() {
        if (Main.overview.visible)
            return Main.overview;
        else
            return global.display.focus_window;
    },

    _setPerWindowInputSource: function() {
        let window = this._getCurrentWindow();
        if (!window)
            return;

        if (!window._inputSources) {
            window._inputSources = this._inputSources;
            window._currentSource = this._currentSource;
        } else if (window._inputSources == this._inputSources) {
            window._currentSource.activate();
        } else {
            window._inputSources = this._inputSources;
            window._currentSource = this._getNewInputSource(window._currentSource);
            window._currentSource.activate();
        }
    },

    _sourcesPerWindowChanged: function() {
        this._sourcesPerWindow = this._settings.get_boolean('per-window');

        if (this._sourcesPerWindow && this._focusWindowNotifyId == 0) {
            this._focusWindowNotifyId = global.display.connect('notify::focus-window',
                                                               Lang.bind(this, this._setPerWindowInputSource));
            this._overviewShowingId = Main.overview.connect('showing',
                                                            Lang.bind(this, this._setPerWindowInputSource));
            this._overviewHiddenId = Main.overview.connect('hidden',
                                                           Lang.bind(this, this._setPerWindowInputSource));
        } else if (!this._sourcesPerWindow && this._focusWindowNotifyId != 0) {
            global.display.disconnect(this._focusWindowNotifyId);
            this._focusWindowNotifyId = 0;
            Main.overview.disconnect(this._overviewShowingId);
            this._overviewShowingId = 0;
            Main.overview.disconnect(this._overviewHiddenId);
            this._overviewHiddenId = 0;

            let windows = global.get_window_actors().map(function(w) {
                return w.meta_window;
            });
            for (let i = 0; i < windows.length; ++i) {
                delete windows[i]._inputSources;
                delete windows[i]._currentSource;
            }
            delete Main.overview._inputSources;
            delete Main.overview._currentSource;
        }
    },

    _changePerWindowSource: function() {
        if (!this._sourcesPerWindow)
            return;

        let window = this._getCurrentWindow();
        if (!window)
            return;

        window._inputSources = this._inputSources;
        window._currentSource = this._currentSource;
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
            is.indicatorLabel.allocate_align_fill(box, 0.5, 0.5, false, false, flags);
        }
    }
});
