// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Gettext = imports.gettext;

const IBus = imports.misc.ibusManager.IBus;
const IBusManager = imports.misc.ibusManager;
const KeyboardManager = imports.misc.keyboardManager;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const SwitcherPopup = imports.ui.switcherPopup;
const Util = imports.misc.util;

const DESKTOP_INPUT_SOURCES_SCHEMA = 'org.gnome.desktop.input-sources';
const KEY_INPUT_SOURCES = 'sources';
const KEY_KEYBOARD_OPTIONS = 'xkb-options';

const INPUT_SOURCE_TYPE_XKB = 'xkb';
const INPUT_SOURCE_TYPE_IBUS = 'ibus';

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

        this.properties = null;

        this.xkbId = this._getXkbId();
    },

    get shortName() {
        return this._shortName;
    },

    set shortName(v) {
        this._shortName = v;
        this.emit('changed');
    },

    activate: function() {
        this.emit('activate');
    },

    _getXkbId: function() {
        let engineDesc = IBusManager.getIBusManager().getEngineDesc(this.id);
        if (!engineDesc)
            return this.id;

        if (engineDesc.variant && engineDesc.variant.length > 0)
            return engineDesc.layout + '+' + engineDesc.variant;
        else
            return engineDesc.layout;
    }
});
Signals.addSignalMethods(InputSource.prototype);

const InputSourcePopup = new Lang.Class({
    Name: 'InputSourcePopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init: function(items, action, actionBackward) {
        this.parent(items);

        this._action = action;
        this._actionBackward = actionBackward;

        this._switcherList = new InputSourceSwitcher(this._items);
    },

    _keyPressHandler: function(keysym, action) {
        if (action == this._action)
            this._select(this._next());
        else if (action == this._actionBackward)
            this._select(this._previous());
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

const InputSourceManager = new Lang.Class({
    Name: 'InputSourceManager',

    _init: function() {
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
                                  new Gio.Settings({ schema_id: "org.gnome.desktop.wm.keybindings" }),
                                  Meta.KeyBindingFlags.NONE,
                                  Shell.KeyBindingMode.ALL,
                                  Lang.bind(this, this._switchInputSource));
        this._keybindingActionBackward =
            Main.wm.addKeybinding('switch-input-source-backward',
                                  new Gio.Settings({ schema_id: "org.gnome.desktop.wm.keybindings" }),
                                  Meta.KeyBindingFlags.IS_REVERSED,
                                  Shell.KeyBindingMode.ALL,
                                  Lang.bind(this, this._switchInputSource));
        this._settings = new Gio.Settings({ schema_id: DESKTOP_INPUT_SOURCES_SCHEMA });
        this._settings.connect('changed::' + KEY_INPUT_SOURCES, Lang.bind(this, this._inputSourcesChanged));
        this._settings.connect('changed::' + KEY_KEYBOARD_OPTIONS, Lang.bind(this, this._keyboardOptionsChanged));

        this._xkbInfo = KeyboardManager.getXkbInfo();
        this._keyboardManager = KeyboardManager.getKeyboardManager();

        this._ibusReady = false;
        this._ibusManager = IBusManager.getIBusManager();
        this._ibusManager.connect('ready', Lang.bind(this, this._ibusReadyCallback));
        this._ibusManager.connect('properties-registered', Lang.bind(this, this._ibusPropertiesRegistered));
        this._ibusManager.connect('property-updated', Lang.bind(this, this._ibusPropertyUpdated));

        global.display.connect('modifiers-accelerator-activated', Lang.bind(this, this._modifiersSwitcher));

        this._sourcesPerWindow = false;
        this._focusWindowNotifyId = 0;
        this._overviewShowingId = 0;
        this._overviewHiddenId = 0;
        this._settings.connect('changed::per-window', Lang.bind(this, this._sourcesPerWindowChanged));
        this._sourcesPerWindowChanged();
    },

    reload: function() {
        this._keyboardManager.setKeyboardOptions(this._settings.get_strv(KEY_KEYBOARD_OPTIONS));
        this._inputSourcesChanged();
    },

    _ibusReadyCallback: function(im, ready) {
        if (this._ibusReady == ready)
            return;

        this._ibusReady = ready;
        this._mruSources = [];
        this._inputSourcesChanged();
    },

    _modifiersSwitcher: function() {
        let sourceIndexes = Object.keys(this._inputSources);
        if (sourceIndexes.length == 0) {
            KeyboardManager.releaseKeyboard();
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
        if (!popup.show(binding.is_reversed(), binding.get_name(), binding.get_mask()))
            popup.destroy();
    },

    _keyboardOptionsChanged: function() {
        this._keyboardManager.setKeyboardOptions(this._settings.get_strv(KEY_KEYBOARD_OPTIONS));
        this._keyboardManager.reapply();
    },

    _currentInputSourceChanged: function(newSource) {
        let oldSource;
        [oldSource, this._currentSource] = [this._currentSource, newSource];

        this.emit('current-source-changed', oldSource);

        for (let i = 1; i < this._mruSources.length; ++i)
            if (this._mruSources[i] == newSource) {
                let currentSource = this._mruSources.splice(i, 1);
                this._mruSources = currentSource.concat(this._mruSources);
                break;
            }

        this._changePerWindowSource();
    },

    _activateInputSource: function(is) {
        KeyboardManager.holdKeyboard();
        this._keyboardManager.apply(is.xkbId);

        // All the "xkb:..." IBus engines simply "echo" back symbols,
        // despite their naming implying differently, so we always set
        // one in order for XIM applications to work given that we set
        // XMODIFIERS=@im=ibus in the first place so that they can
        // work without restarting when/if the user adds an IBus input
        // source.
        let engine;
        if (is.type == INPUT_SOURCE_TYPE_IBUS)
            engine = is.id;
        else
            engine = 'xkb:us::eng';

        this._ibusManager.setEngine(engine, KeyboardManager.releaseKeyboard);
        this._currentInputSourceChanged(is);
    },

    _inputSourcesChanged: function() {
        let sources = this._settings.get_value(KEY_INPUT_SOURCES);
        let nSources = sources.n_children();

        this._inputSources = {};
        this._ibusSources = {};

        let infosList = [];
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

            if (exists)
                infosList.push({ type: type, id: id, displayName: displayName, shortName: shortName });
        }

        if (infosList.length == 0) {
            let type = INPUT_SOURCE_TYPE_XKB;
            let id = KeyboardManager.DEFAULT_LAYOUT;
            let [ , displayName, shortName, , ] = this._xkbInfo.get_layout_info(id);
            infosList.push({ type: type, id: id, displayName: displayName, shortName: shortName });
        }

        let inputSourcesByShortName = {};
        for (let i = 0; i < infosList.length; i++) {
            let is = new InputSource(infosList[i].type,
                                     infosList[i].id,
                                     infosList[i].displayName,
                                     infosList[i].shortName,
                                     i);
            is.connect('activate', Lang.bind(this, this._activateInputSource));

            if (!(is.shortName in inputSourcesByShortName))
                inputSourcesByShortName[is.shortName] = [];
            inputSourcesByShortName[is.shortName].push(is);

            this._inputSources[is.index] = is;

            if (is.type == INPUT_SOURCE_TYPE_IBUS)
                this._ibusSources[is.id] = is;
        }

        for (let i in this._inputSources) {
            let is = this._inputSources[i];
            if (inputSourcesByShortName[is.shortName].length > 1) {
                let sub = inputSourcesByShortName[is.shortName].indexOf(is) + 1;
                is.shortName += String.fromCharCode(0x2080 + sub);
            }
        }

        this.emit('sources-changed');

        let sourcesList = [];
        for (let i in this._inputSources)
            sourcesList.push(this._inputSources[i]);

        this._keyboardManager.setUserLayouts(sourcesList.map(function(x) { return x.xkbId; }));

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

        if (this._mruSources.length > 0)
            this._mruSources[0].activate();
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
            this.emit('current-source-changed', null);
    },

    _ibusPropertyUpdated: function(im, engineName, prop) {
        let source = this._ibusSources[engineName];
        if (!source)
            return;

        if (this._updateSubProperty(source.properties, prop) &&
            source == this._currentSource)
            this.emit('current-source-changed', null);
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

    get currentSource() {
        return this._currentSource;
    },

    get inputSources() {
        return this._inputSources;
    },
});
Signals.addSignalMethods(InputSourceManager.prototype);

let _inputSourceManager = null;

function getInputSourceManager() {
    if (_inputSourceManager == null)
        _inputSourceManager = new InputSourceManager();
    return _inputSourceManager;
}

const InputSourceIndicator = new Lang.Class({
    Name: 'InputSourceIndicator',
    Extends: PanelMenu.Button,

    _init: function() {
        this.parent(0.0, _("Keyboard"));

        this._menuItems = {};
        this._indicatorLabels = {};

        this._container = new Shell.GenericContainer();
        this._container.connect('get-preferred-width', Lang.bind(this, this._containerGetPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._containerGetPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._containerAllocate));

        this._hbox = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        this._hbox.add_child(this._container);
        this._hbox.add_child(PopupMenu.arrowIcon(St.Side.BOTTOM));

        this.actor.add_child(this._hbox);
        this.actor.add_style_class_name('panel-status-button');

        this._propSeparator = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(this._propSeparator);
        this._propSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._propSection);
        this._propSection.actor.hide();

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._showLayoutItem = this.menu.addAction(_("Show Keyboard Layout"), Lang.bind(this, this._showLayout));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();

        this._inputSourceManager = getInputSourceManager();
        this._inputSourceManager.connect('sources-changed', Lang.bind(this, this._sourcesChanged));
        this._inputSourceManager.connect('current-source-changed', Lang.bind(this, this._currentSourceChanged));
        this._inputSourceManager.reload();
    },

    _sessionUpdated: function() {
        // re-using "allowSettings" for the keyboard layout is a bit shady,
        // but at least for now it is used as "allow popping up windows
        // from shell menus"; we can always add a separate sessionMode
        // option if need arises.
        this._showLayoutItem.actor.visible = Main.sessionMode.allowSettings;
    },

    _sourcesChanged: function() {
        for (let i in this._menuItems)
            this._menuItems[i].destroy();
        for (let i in this._indicatorLabels)
            this._indicatorLabels[i].destroy();

        let menuIndex = 0;
        for (let i in this._inputSourceManager.inputSources) {
            let is = this._inputSourceManager.inputSources[i];

            let menuItem = new LayoutMenuItem(is.displayName, is.shortName);
            menuItem.connect('activate', Lang.bind(is, is.activate));
            let indicatorLabel = new St.Label({ text: is.shortName,
                                                visible: false });

            this._menuItems[i] = menuItem;
            this._indicatorLabels[i] = indicatorLabel;
            is.connect('changed', function() {
                menuItem.indicator.set_text(is.shortName);
                indicatorLabel.set_text(is.shorName);
            });

            this.menu.addMenuItem(menuItem, menuIndex++);
            this._container.add_actor(indicatorLabel);
        }
    },

    _currentSourceChanged: function(manager, oldSource) {
        let nVisibleSources = Object.keys(this._inputSourceManager.inputSources).length;
        let newSource = this._inputSourceManager.currentSource;

        if (oldSource) {
            this._menuItems[oldSource.index].setOrnament(PopupMenu.Ornament.NONE);
            this._indicatorLabels[oldSource.index].hide();
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

        this._buildPropSection(newSource.properties);

        this._menuItems[newSource.index].setOrnament(PopupMenu.Ornament.DOT);
        this._indicatorLabels[newSource.index].show();
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

        let ibusManager = IBusManager.getIBusManager();
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

                let currentSource = this._inputSourceManager.currentSource;
                if (currentSource) {
                    let indicatorLabel = this._indicatorLabels[currentSource.index];
                    if (text && text.length > 0 && text.length < 3)
                        indicatorLabel.set_text(text);
                }
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
                            ibusManager.activateProperty(item.prop.get_key(),
                                                         IBus.PropState.CHECKED);
                        } else {
                            group[i].setOrnament(PopupMenu.Ornament.NONE);
                            group[i].prop.set_state(IBus.PropState.UNCHECKED);
                            ibusManager.activateProperty(group[i].prop.get_key(),
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
                        ibusManager.activateProperty(item.prop.get_key(),
                                                     IBus.PropState.CHECKED);
                    } else {
                        item.prop.set_state(IBus.PropState.UNCHECKED);
                        ibusManager.activateProperty(item.prop.get_key(),
                                                     IBus.PropState.UNCHECKED);
                    }
                }));
                break;

            case IBus.PropType.NORMAL:
                item = new PopupMenu.PopupMenuItem(prop.get_label().get_text());
                item.prop = prop;
                item.connect('activate', Lang.bind(this, function() {
                    ibusManager.activateProperty(item.prop.get_key(),
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

    _showLayout: function() {
        Main.overview.hide();

        let source = this._inputSourceManager.currentSource;
        let xkbLayout = '';
        let xkbVariant = '';

        if (source.type == INPUT_SOURCE_TYPE_XKB) {
            [, , , xkbLayout, xkbVariant] = KeyboardManager.getXkbInfo().get_layout_info(source.id);
        } else if (source.type == INPUT_SOURCE_TYPE_IBUS) {
            let engineDesc = IBusManager.getIBusManager().getEngineDesc(source.id);
            if (engineDesc) {
                xkbLayout = engineDesc.get_layout();
                xkbVariant = engineDesc.get_layout_variant();
            }
        }

        if (!xkbLayout || xkbLayout.length == 0)
            return;

        let description = xkbLayout;
        if (xkbVariant.length > 0)
            description = description + '\t' + xkbVariant;

        Util.spawn(['gkbd-keyboard-display', '-l', description]);
    },

    _containerGetPreferredWidth: function(container, for_height, alloc) {
        // Here, and in _containerGetPreferredHeight, we need to query
        // for the height of all children, but we ignore the results
        // for those we don't actually display.
        let max_min_width = 0, max_natural_width = 0;

        for (let i in this._inputSourceManager.inputSources) {
            let label = this._indicatorLabels[i];
            let [min_width, natural_width] = label.get_preferred_width(for_height);
            max_min_width = Math.max(max_min_width, min_width);
            max_natural_width = Math.max(max_natural_width, natural_width);
        }

        alloc.min_size = max_min_width;
        alloc.natural_size = max_natural_width;
    },

    _containerGetPreferredHeight: function(container, for_width, alloc) {
        let max_min_height = 0, max_natural_height = 0;

        for (let i in this._inputSourceManager.inputSources) {
            let label = this._indicatorLabels[i];
            let [min_height, natural_height] = label.get_preferred_height(for_width);
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

        for (let i in this._inputSourceManager.inputSources) {
            let label = this._indicatorLabels[i];
            label.allocate_align_fill(box, 0.5, 0.5, false, false, flags);
        }
    }
});
