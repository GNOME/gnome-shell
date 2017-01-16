// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const FocusCaretTracker = imports.ui.focusCaretTracker;
const Atspi = imports.gi.Atspi;
const Caribou = imports.gi.Caribou;
const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const InputSourceManager = imports.ui.status.keyboard;

const BoxPointer = imports.ui.boxpointer;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const KEYBOARD_REST_TIME = Layout.KEYBOARD_ANIMATION_TIME * 2 * 1000;

const KEYBOARD_SCHEMA = 'org.gnome.shell.keyboard';
const KEYBOARD_TYPE = 'keyboard-type';

const A11Y_APPLICATIONS_SCHEMA = 'org.gnome.desktop.a11y.applications';
const SHOW_KEYBOARD = 'screen-keyboard-enabled';

const Key = new Lang.Class({
    Name: 'Key',

    _init : function(key) {
        this._key = key;
        this.actor = this._makeKey(key, GLib.markup_escape_text(key.label, -1));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._extended_keys = this._key.get_extended_keys();
        this._extended_keyboard = null;

        if (this._key.name == 'Control_L' || this._key.name == 'Alt_L')
            this._key.latch = true;

        if (this._extended_keys.length > 0) {
            this._key.connect('notify::show-subkeys', Lang.bind(this, this._onShowSubkeysChanged));
            this._boxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM,
                                                         { x_fill: true,
                                                           y_fill: true,
                                                           x_align: St.Align.START });
            // Adds style to existing keyboard style to avoid repetition
            this._boxPointer.actor.add_style_class_name('keyboard-subkeys');
            this._getExtendedKeys();
            this.actor._extended_keys = this._extended_keyboard;
            this._boxPointer.actor.hide();
            Main.layoutManager.addChrome(this._boxPointer.actor);
        }
    },

    _onDestroy: function() {
        if (this._boxPointer) {
            this._boxPointer.actor.destroy();
            this._boxPointer = null;
        }
    },

    _makeKey: function (key, label) {
        let button = new St.Button ({ label: label,
                                      style_class: 'keyboard-key' });

        button.key_width = this._key.width;
        button.connect('button-press-event', Lang.bind(this,
            function () {
                key.press();
                return Clutter.EVENT_PROPAGATE;
            }));
        button.connect('button-release-event', Lang.bind(this,
            function () {
                key.release();
                return Clutter.EVENT_PROPAGATE;
            }));
        button.connect('touch-event', Lang.bind(this,
            function (actor, event) {
                let device = event.get_device();
                let sequence = event.get_event_sequence();

                // We only handle touch events here on wayland. On X11
                // we do get emulated pointer events, which already works
                // for single-touch cases. Besides, the X11 passive touch grab
                // set up by Mutter will make us see first the touch events
                // and later the pointer events, so it will look like two
                // unrelated series of events, we want to avoid double handling
                // in these cases.
                if (!Meta.is_wayland_compositor())
                    return Clutter.EVENT_PROPAGATE;

                if (!this._touchPressed &&
                    event.type() == Clutter.EventType.TOUCH_BEGIN) {
                    device.sequence_grab(sequence, actor);
                    this._touchPressed = true;
                    key.press();
                } else if (this._touchPressed &&
                           event.type() == Clutter.EventType.TOUCH_END &&
                           device.sequence_get_grabbed_actor(sequence) == actor) {
                    device.sequence_ungrab(sequence);
                    this._touchPressed = false;
                    key.release();
                }
                return Clutter.EVENT_PROPAGATE;
            }));

        return button;
    },

    _getUnichar: function(key) {
        let keyval = key.keyval;
        let unichar = Gdk.keyval_to_unicode(keyval);
        if (unichar) {
            return String.fromCharCode(unichar);
        } else {
            return key.name;
        }
    },

    _getExtendedKeys: function () {
        this._extended_keyboard = new St.BoxLayout({ style_class: 'keyboard-layout',
                                                     vertical: false });
        for (let i = 0; i < this._extended_keys.length; ++i) {
            let extended_key = this._extended_keys[i];
            let label = this._getUnichar(extended_key);
            let key = this._makeKey(extended_key, label);

            key.extended_key = extended_key;
            this._extended_keyboard.add(key);
        }
        this._boxPointer.bin.add_actor(this._extended_keyboard);
    },

    get subkeys() {
        return this._boxPointer;
    },

    _onShowSubkeysChanged: function () {
        if (this._key.show_subkeys) {
            this._boxPointer.actor.raise_top();
            this._boxPointer.setPosition(this.actor, 0.5);
            this.emit('show-subkeys');
            this.actor.fake_release();
            this.actor.set_hover(false);
        } else {
            this.emit('hide-subkeys');
        }
    }
});
Signals.addSignalMethods(Key.prototype);

const Keyboard = new Lang.Class({
    Name: 'Keyboard',

    _init: function () {
        this.actor = null;
        this._focusInTray = false;
        this._focusInExtendedKeys = false;

        this._focusCaretTracker = new FocusCaretTracker.FocusCaretTracker();
        this._focusCaretTracker.connect('focus-changed', Lang.bind(this, this._onFocusChanged));
        this._focusCaretTracker.connect('caret-moved', Lang.bind(this, this._onCaretMoved));
        this._currentAccessible = null;
        this._caretTrackingEnabled = false;
        this._updateCaretPositionId = 0;

        this._keyboardSettings = new Gio.Settings({ schema_id: KEYBOARD_SCHEMA });
        this._keyboardSettings.connect('changed', Lang.bind(this, this._sync));
        this._a11yApplicationsSettings = new Gio.Settings({ schema_id: A11Y_APPLICATIONS_SCHEMA });
        this._a11yApplicationsSettings.connect('changed', Lang.bind(this, this._sync));
        this._daemonProxy = null;
        this._lastDeviceId = null;

        Caribou.DisplayAdapter.set_default(new LocalAdapter());

        Meta.get_backend().connect('last-device-changed', Lang.bind(this,
            function (backend, deviceId) {
                let manager = Clutter.DeviceManager.get_default();
                let device = manager.get_device(deviceId);

                if (device.get_device_name().indexOf('XTEST') < 0) {
                    this._lastDeviceId = deviceId;
                    this._sync();
                }
            }));
        this._sync();

        this._showIdleId = 0;
        this._subkeysBoxPointer = null;
        this._capturedEventId = 0;
        this._capturedPress = false;

        this._keyboardVisible = false;
        Main.layoutManager.connect('keyboard-visible-changed', Lang.bind(this, function(o, visible) {
            this._keyboardVisible = visible;
        }));
        this._keyboardRequested = false;
        this._keyboardRestingId = 0;

        Main.layoutManager.connect('monitors-changed', Lang.bind(this, this._redraw));
        this._redraw();
    },

    _setCaretTrackerEnabled: function (enabled) {
        if (this._caretTrackingEnabled == enabled)
            return;

        this._caretTrackingEnabled = enabled;

        if (enabled) {
            this._focusCaretTracker.registerFocusListener();
            this._focusCaretTracker.registerCaretListener();
        } else {
            this._focusCaretTracker.deregisterFocusListener();
            this._focusCaretTracker.deregisterCaretListener();
        }
    },

    _updateCaretPosition: function (accessible) {
        if (this._updateCaretPositionId)
            GLib.source_remove(this._updateCaretPositionId);
        this._updateCaretPositionId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, Lang.bind(this, function() {
            this._updateCaretPositionId = 0;

            let currentWindow = global.screen.get_display().focus_window;
            if (!currentWindow)
                return GLib.SOURCE_REMOVE;

            let windowRect = currentWindow.get_frame_rect();
            let text = accessible.get_text_iface();
            let component = accessible.get_component_iface();

            try {
                let caretOffset = text.get_caret_offset();
                let caretRect = text.get_character_extents(caretOffset, Atspi.CoordType.WINDOW);
                let focusRect = component.get_extents(Atspi.CoordType.WINDOW);

                caretRect.x += windowRect.x;
                caretRect.y += windowRect.y;
                focusRect.x += windowRect.x;
                focusRect.y += windowRect.y;

                if (caretRect.width == 0 && caretRect.height == 0)
                    caretRect = focusRect;

                this.setEntryLocation(focusRect.x, focusRect.y, focusRect.width, focusRect.height);
                this.setCursorLocation(caretRect.x, caretRect.y, caretRect.width, caretRect.height);
            } catch (e) {
                log('Error updating caret position for OSK: ' + e.message);
            }

            return GLib.SOURCE_REMOVE;
        }));

        GLib.Source.set_name_by_id(this._updateCaretPositionId, '[gnome-shell] this._updateCaretPosition');
    },

    _focusIsTextEntry: function (accessible) {
        try {
            let role = accessible.get_role();
            let stateSet = accessible.get_state_set();
            return stateSet.contains(Atspi.StateType.EDITABLE) || role == Atspi.Role.TERMINAL;
        } catch (e) {
            log('Error determining accessible role: ' + e.message);
            return false;
        }
    },

    _onFocusChanged: function (caretTracker, event) {
        let accessible = event.source;
        if (!this._focusIsTextEntry(accessible))
            return;

        let focused = event.detail1 != 0;
        if (focused) {
            this._currentAccessible = accessible;
            this._updateCaretPosition(accessible);
            this.show(Main.layoutManager.focusIndex);
        } else if (this._currentAccessible == accessible) {
            this._currentAccessible = null;
            this.hide();
        }
    },

    _onCaretMoved: function (caretTracker, event) {
        let accessible = event.source;
        if (this._currentAccessible == accessible)
            this._updateCaretPosition(accessible);
    },

    _lastDeviceIsTouchscreen: function () {
        if (!this._lastDeviceId)
            return false;

        let manager = Clutter.DeviceManager.get_default();
        let device = manager.get_device(this._lastDeviceId);

        if (!device)
            return false;

        return device.get_device_type() == Clutter.InputDeviceType.TOUCHSCREEN_DEVICE;
    },

    _sync: function () {
        this._enableKeyboard = this._a11yApplicationsSettings.get_boolean(SHOW_KEYBOARD) ||
                               this._lastDeviceIsTouchscreen();
        if (!this._enableKeyboard && !this._keyboard)
            return;
        if (this._enableKeyboard && this._keyboard &&
            this._keyboard.keyboard_type == this._keyboardSettings.get_string(KEYBOARD_TYPE))
            return;

        this._setCaretTrackerEnabled(this._enableKeyboard);

        if (this._keyboard)
            this._destroyKeyboard();

        if (this._enableKeyboard)
            this._setupKeyboard();
        else
            Main.layoutManager.hideKeyboard(true);
    },

    _destroyKeyboard: function() {
        if (this._keyboardNotifyId)
            this._keyboard.disconnect(this._keyboardNotifyId);
        if (this._keyboardGroupAddedId)
            this._keyboard.disconnect(this._keyboardGroupAddedId);
        if (this._keyboardGroupRemovedId)
            this._keyboard.disconnect(this._keyboardGroupRemovedId);
        if (this._focusNotifyId)
            global.stage.disconnect(this._focusNotifyId);
        this._keyboard = null;
        this.actor.destroy();
        this.actor = null;

        this._destroySource();
        if (this._daemonProxy) {
            this._daemonProxy.QuitRemote(function (result, error) {
                if (error) {
                    log(error.message);
                    return;
                }
            });
            this._daemonProxy = null;
        }
    },

    _setupKeyboard: function() {
        this.actor = new St.BoxLayout({ name: 'keyboard', vertical: true, reactive: true });
        Main.layoutManager.keyboardBox.add_actor(this.actor);
        Main.layoutManager.trackChrome(this.actor);

        this._keyboard = new Caribou.KeyboardModel({ keyboard_type: this._keyboardSettings.get_string(KEYBOARD_TYPE) });
        this._groups = {};
        this._current_page = null;

        // Initialize keyboard key measurements
        this._numOfHorizKeys = 0;
        this._numOfVertKeys = 0;

        this._addKeys();

        // Keys should be layout according to the group, not the
        // locale; as Caribou already provides the expected layout,
        // this means enforcing LTR for all locales.
        this.actor.text_direction = Clutter.TextDirection.LTR;

        this._keyboardNotifyId = this._keyboard.connect('notify::active-group', Lang.bind(this, this._onGroupChanged));
        this._keyboardGroupAddedId = this._keyboard.connect('group-added', Lang.bind(this, this._onGroupAdded));
        this._keyboardGroupRemovedId = this._keyboard.connect('group-removed', Lang.bind(this, this._onGroupRemoved));
        this._focusNotifyId = global.stage.connect('notify::key-focus', Lang.bind(this, this._onKeyFocusChanged));

        this._createSource();
    },

    _onKeyFocusChanged: function () {
        let focus = global.stage.key_focus;

        // Showing an extended key popup and clicking a key from the extended keys
        // will grab focus, but ignore that
        let extendedKeysWereFocused = this._focusInExtendedKeys;
        this._focusInExtendedKeys = focus && (focus._extended_keys || focus.extended_key);
        if (this._focusInExtendedKeys || extendedKeysWereFocused)
            return;

        // Ignore focus changes caused by message tray showing/hiding
        let trayWasFocused = this._focusInTray;
        this._focusInTray = (focus && Main.messageTray.actor.contains(focus));
        if (this._focusInTray || trayWasFocused)
            return;

        let time = global.get_current_time();
        if (!(focus instanceof Clutter.Text)) {
            this.hide();
            return;
        }

        if (!this._showIdleId) {
          this._showIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE,
                                           Lang.bind(this, function() {
                                               this.show(Main.layoutManager.focusIndex);
                                               return GLib.SOURCE_REMOVE;
                                           }));
          GLib.Source.set_name_by_id(this._showIdleId, '[gnome-shell] this.show');
        }
    },

    _createLayersForGroup: function (gname) {
        let group = this._keyboard.get_group(gname);
        group.connect('notify::active-level', Lang.bind(this, this._onLevelChanged));
        let layers = {};
        let levels = group.get_levels();
        for (let j = 0; j < levels.length; ++j) {
            let lname = levels[j];
            let level = group.get_level(lname);
            let layout = new St.BoxLayout({ style_class: 'keyboard-layout',
                                                 vertical: true });
            this._loadRows(level, layout);
            layers[lname] = layout;
            this.actor.add(layout, { x_fill: false });

            layout.hide();
        }
        return layers;
    },

    _addKeys: function () {
        let groups = this._keyboard.get_groups();
        for (let i = 0; i < groups.length; ++i) {
             let gname = groups[i];
             this._groups[gname] = this._createLayersForGroup(gname);
        }

        this._setActiveLayer();
    },

    _onCapturedEvent: function(actor, event) {
        let type = event.type();
        let press = type == Clutter.EventType.BUTTON_PRESS;
        let release = type == Clutter.EventType.BUTTON_RELEASE;

        if (press)
            this._capturedPress = true;
        else if (release && this._capturedPress)
            this._hideSubkeys();

        return Clutter.EVENT_STOP;
    },

    _addRows : function (keys, layout) {
        let keyboard_row = new St.BoxLayout();
        for (let i = 0; i < keys.length; ++i) {
            let children = keys[i].get_children();
            let left_box = new St.BoxLayout({ style_class: 'keyboard-row' });
            let center_box = new St.BoxLayout({ style_class: 'keyboard-row' });
            let right_box = new St.BoxLayout({ style_class: 'keyboard-row' });
            for (let j = 0; j < children.length; ++j) {
                if (this._numOfHorizKeys == 0)
                    this._numOfHorizKeys = children.length;
                let key = children[j];
                let button = new Key(key);

                switch (key.align) {
                case 'right':
                    right_box.add(button.actor);
                    break;
                case 'center':
                    center_box.add(button.actor);
                    break;
                case 'left':
                default:
                    left_box.add(button.actor);
                    break;
                }
                if (key.name == 'Caribou_Prefs') {
                    key.connect('key-released', Lang.bind(this, this.hide));
                }

                button.connect('show-subkeys', Lang.bind(this, function() {
                    if (this._subkeysBoxPointer)
                        this._subkeysBoxPointer.hide(BoxPointer.PopupAnimation.FULL);
                    this._subkeysBoxPointer = button.subkeys;
                    this._subkeysBoxPointer.show(BoxPointer.PopupAnimation.FULL);
                    if (!this._capturedEventId)
                        this._capturedEventId = this.actor.connect('captured-event',
                                                                   Lang.bind(this, this._onCapturedEvent));
                }));
                button.connect('hide-subkeys', Lang.bind(this, function() {
                    this._hideSubkeys();
                }));
            }
            keyboard_row.add(left_box, { expand: true, x_fill: false, x_align: St.Align.START });
            keyboard_row.add(center_box, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });
            keyboard_row.add(right_box, { expand: true, x_fill: false, x_align: St.Align.END });
        }
        layout.add(keyboard_row);
    },

    _loadRows : function (level, layout) {
        let rows = level.get_rows();
        for (let i = 0; i < rows.length; ++i) {
            let row = rows[i];
            if (this._numOfVertKeys == 0)
                this._numOfVertKeys = rows.length;
            this._addRows(row.get_columns(), layout);
        }

    },

    _redraw: function () {
        if (!this._enableKeyboard)
            return;

        let monitor = Main.layoutManager.keyboardMonitor;
        let maxHeight = monitor.height / 3;
        this.actor.width = monitor.width;

        let layout = this._current_page;
        let verticalSpacing = layout.get_theme_node().get_length('spacing');
        let padding = layout.get_theme_node().get_length('padding');

        let box = layout.get_children()[0].get_children()[0];
        let horizontalSpacing = box.get_theme_node().get_length('spacing');
        let allHorizontalSpacing = (this._numOfHorizKeys - 1) * horizontalSpacing;
        let keyWidth = Math.floor((this.actor.width - allHorizontalSpacing - 2 * padding) / this._numOfHorizKeys);

        let allVerticalSpacing = (this._numOfVertKeys - 1) * verticalSpacing;
        let keyHeight = Math.floor((maxHeight - allVerticalSpacing - 2 * padding) / this._numOfVertKeys);

        let keySize = Math.min(keyWidth, keyHeight);
        this.actor.height = keySize * this._numOfVertKeys + allVerticalSpacing + 2 * padding;

        let rows = this._current_page.get_children();
        for (let i = 0; i < rows.length; ++i) {
            let keyboard_row = rows[i];
            let boxes = keyboard_row.get_children();
            for (let j = 0; j < boxes.length; ++j) {
                let keys = boxes[j].get_children();
                for (let k = 0; k < keys.length; ++k) {
                    let child = keys[k];
                    child.width = keySize * child.key_width;
                    child.height = keySize;
                    if (child._extended_keys) {
                        let extended_keys = child._extended_keys.get_children();
                        for (let n = 0; n < extended_keys.length; ++n) {
                            let extended_key = extended_keys[n];
                            extended_key.width = keySize;
                            extended_key.height = keySize;
                        }
                    }
                }
            }
        }
    },

    _onLevelChanged: function () {
        this._setActiveLayer();
        this._redraw();
    },

    _onGroupChanged: function () {
        this._setActiveLayer();
        this._redraw();
    },

    _onGroupAdded: function (keyboard, gname) {
        this._groups[gname] = this._createLayersForGroup(gname);
    },

    _onGroupRemoved: function (keyboard, gname) {
        delete this._groups[gname];
    },

    _setActiveLayer: function () {
        let active_group_name = this._keyboard.active_group;
        let active_group = this._keyboard.get_group(active_group_name);
        let active_level = active_group.active_level;
        let layers = this._groups[active_group_name];

        if (this._current_page != null) {
            this._current_page.hide();
        }

        this._current_page = layers[active_level];
        this._current_page.show();
    },

    _createSource: function () {
        if (this._source == null) {
            this._source = new KeyboardSource(this);
            Main.messageTray.add(this._source);
        }
    },

    _destroySource: function () {
        if (this._source) {
            this._source.destroy();
            this._source = null;
        }
    },

    shouldTakeEvent: function(event) {
        let actor = event.get_source();
        return Main.layoutManager.keyboardBox.contains(actor) ||
               actor._extended_keys || actor.extended_key;
    },

    _clearKeyboardRestTimer: function() {
        if (!this._keyboardRestingId)
            return;
        GLib.source_remove(this._keyboardRestingId);
        this._keyboardRestingId = 0;
    },

    show: function (monitor) {
        if (!this._enableKeyboard)
            return;

        this._clearShowIdle();
        this._keyboardRequested = true;

        if (this._keyboardVisible) {
            if (monitor != Main.layoutManager.keyboardIndex) {
                Main.layoutManager.keyboardIndex = monitor;
                this._redraw();
            }
            return;
        }

        this._clearKeyboardRestTimer();
        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                   KEYBOARD_REST_TIME,
                                                   Lang.bind(this, function() {
                                                       this._clearKeyboardRestTimer();
                                                       this._show(monitor);
                                                       return GLib.SOURCE_REMOVE;
                                                   }));
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    },

    _show: function(monitor) {
        if (!this._keyboardRequested)
            return;

        Main.layoutManager.keyboardIndex = monitor;
        this._redraw();
        Main.layoutManager.showKeyboard();
        this._destroySource();
    },

    hide: function () {
        if (!this._enableKeyboard)
            return;

        this._clearShowIdle();
        this._keyboardRequested = false;

        if (!this._keyboardVisible)
            return;

        this._clearKeyboardRestTimer();
        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                   KEYBOARD_REST_TIME,
                                                   Lang.bind(this, function() {
                                                       this._clearKeyboardRestTimer();
                                                       this._hide();
                                                       return GLib.SOURCE_REMOVE;
                                                   }));
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    },

    _hide: function() {
        if (this._keyboardRequested)
            return;

        this._hideSubkeys();
        Main.layoutManager.hideKeyboard();
        this._createSource();
    },

    _hideSubkeys: function() {
        if (this._subkeysBoxPointer) {
            this._subkeysBoxPointer.hide(BoxPointer.PopupAnimation.FULL);
            this._subkeysBoxPointer = null;
        }
        if (this._capturedEventId) {
            this.actor.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
        this._capturedPress = false;
    },

    _moveTemporarily: function () {
        let currentWindow = global.screen.get_display().focus_window;
        let rect = currentWindow.get_frame_rect();

        let newX = rect.x;
        let newY = 3 * this.actor.height / 2;
        currentWindow.move_frame(true, newX, newY);
    },

    _setLocation: function (x, y) {
        if (y >= 2 * this.actor.height)
            this._moveTemporarily();
    },

    _clearShowIdle: function() {
        if (!this._showIdleId)
            return;
        GLib.source_remove(this._showIdleId);
        this._showIdleId = 0;
    },

    setCursorLocation: function(x, y, w, h) {
        if (!this._enableKeyboard)
            return;

//        this._setLocation(x, y);
    },

    setEntryLocation: function(x, y, w, h) {
        if (!this._enableKeyboard)
            return;

//        this._setLocation(x, y);
    },
});

const KeyboardSource = new Lang.Class({
    Name: 'KeyboardSource',
    Extends: MessageTray.Source,

    _init: function(keyboard) {
        this._keyboard = keyboard;
        this.parent(_("Keyboard"), 'input-keyboard-symbolic');
        this.keepTrayOnSummaryClick = true;
    },

    handleSummaryClick: function(button) {
        this.open();
        return true;
    },

    open: function() {
        // Show the OSK below the message tray
        this._keyboard.show(Main.layoutManager.bottomIndex);
    }
});

const LocalAdapter = new Lang.Class({
    Name: 'LocalAdapter',
    Extends: Caribou.XAdapter,

    _init: function () {
        this.parent();
        let deviceManager = Clutter.DeviceManager.get_default();
        this._virtualDevice = deviceManager.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

        this._inputSourceManager = InputSourceManager.getInputSourceManager();
        this._sourceChangedId = this._inputSourceManager.connect('current-source-changed',
                                                                 Lang.bind(this, this._onSourceChanged));
        this._sourcesModifiedId = this._inputSourceManager.connect ('sources-changed',
                                                                    Lang.bind(this, this._onSourcesModified));
    },

    _onSourcesModified: function () {
        this.emit('config-changed');
    },

    _onSourceChanged: function (inputSourceManager, oldSource) {
        let source = inputSourceManager.currentSource;
        this.emit('group-changed', source.index, source.id, '');
    },

    vfunc_get_groups: function () {
        let inputSources = this._inputSourceManager.inputSources;
        let groups = []
        let variants = [];

        for (let i in inputSources) {
            let is = inputSources[i];
            groups[is.index] = is.id;
            variants[is.index] = '';
        }

        return [groups, groups.length, variants, variants.length];
    },

    vfunc_keyval_press: function(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.PRESSED);
    },

    vfunc_keyval_release: function(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.RELEASED);
    },
});
