// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const FocusCaretTracker = imports.ui.focusCaretTracker;
const Atspi = imports.gi.Atspi;
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

var KEYBOARD_REST_TIME = Layout.KEYBOARD_ANIMATION_TIME * 2 * 1000;
var KEY_LONG_PRESS_TIME = 250;

const A11Y_APPLICATIONS_SCHEMA = 'org.gnome.desktop.a11y.applications';
const SHOW_KEYBOARD = 'screen-keyboard-enabled';

const defaultKeysPre = [
    [ [], [], [{ label: '⇧', width: 1.5, level: 1 }], [{ label: '?123', width: 1.5, level: 2 }] ],
    [ [], [], [{ label: '⇪', width: 1.5, level: 0 }], [{ label: '?123', width: 1.5, level: 2 }] ],
    [ [], [], [{ label: '=/<', width: 1.5, level: 3 }], [{ label: 'ABC', width: 1.5, level: 0 }] ],
    [ [], [], [{ label: '?123', width: 1.5, level: 2 }], [{ label: 'ABC', width: 1.5, level: 0 }] ],
];

const defaultKeysPost = [
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ label: '⏎', width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '⇧', width: 3, level: 1, right: true }],
      [{ label: '🌐', width: 1.5 }, { label: '⌨', width: 1.5, action: 'hide' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ label: '⏎', width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '⇪', width: 3, level: 0, right: true }],
      [{ label: '🌐', width: 1.5 }, { label: '⌨', width: 1.5, action: 'hide' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ label: '⏎', width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '=/<', width: 3, level: 3, right: true }],
      [{ label: '🌐', width: 1.5 }, { label: '⌨', width: 1.5, action: 'hide' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ label: '⏎', width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '?123', width: 3, level: 2, right: true }],
      [{ label: '🌐', width: 1.5 }, { label: '⌨', width: 1.5, action: 'hide' }] ],
];

var Suggestions = new Lang.Class({
    Name: 'Suggestions',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'word-suggestions',
                                        vertical: false });
        this.actor.hide();
    },

    add: function(word, callback) {
        let button = new St.Button({ label: word });
        button.connect('clicked', callback);
        this.actor.add(button);
        this.actor.show();
    },

    clear: function() {
        this.actor.remove_all_children();
        this.actor.hide();
    },
});
Signals.addSignalMethods(Suggestions.prototype);

var Key = new Lang.Class({
    Name: 'Key',

    _init : function(key, extendedKeys) {
        this.key = key;
        this.actor = this._makeKey(this.key);

        /* Add the key in a container, so keys can be padded without losing
         * logical proportions between those.
         */
        this.container = new St.BoxLayout ({ style_class: 'key-container' });
        this.container.add(this.actor, { expand: true, x_fill: true });
        this.container.connect('destroy', Lang.bind(this, this._onDestroy));

        this._extended_keys = extendedKeys;
        this._extended_keyboard = null;
        this._pressTimeoutId = 0;
    },

    _onDestroy: function() {
        if (this._boxPointer) {
            this._boxPointer.actor.destroy();
            this._boxPointer = null;
        }
    },

    _ensureExtendedKeysPopup: function() {
        if (this._extended_keys.length == 0)
            return;

        this._boxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM,
                                                     { x_fill: true,
                                                       y_fill: true,
                                                       x_align: St.Align.START });
        this._boxPointer.actor.hide();
        Main.layoutManager.addChrome(this._boxPointer.actor);
        this._boxPointer.setPosition(this.actor, 0.5);

        // Adds style to existing keyboard style to avoid repetition
        this._boxPointer.actor.add_style_class_name('keyboard-subkeys');
        this._getExtendedKeys();
        this.actor._extended_keys = this._extended_keyboard;
    },

    _getKeyval: function(key) {
        let unicode = String.charCodeAt(key, 0);
        return Gdk.unicode_to_keyval(unicode);
    },

    _press: function(key) {
        if (key != this.key || this._extended_keys.length == 0) {
            this.emit('pressed', this._getKeyval(key), key);
        } else if (key == this.key) {
            this._pressTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                    KEY_LONG_PRESS_TIME,
                                                    Lang.bind(this, function() {
                                                        this.actor.set_hover(false);
                                                        this.actor.fake_release();
                                                        this._pressTimeoutId = 0;
                                                        this._touchPressed = false;
                                                        this._ensureExtendedKeysPopup();
                                                        this.actor.fake_release();
                                                        this.actor.set_hover(false);
                                                        this.emit('show-subkeys');
                                                        return GLib.SOURCE_REMOVE;
                                                    }));
        }
    },

    _release: function(key) {
        if (this._pressTimeoutId != 0) {
            GLib.source_remove(this._pressTimeoutId);
            this._pressTimeoutId = 0;
            this.emit('pressed', this._getKeyval(key), key);
        }

        this.emit('released', this._getKeyval(key), key);
    },

    _makeKey: function (key) {
        let label = GLib.markup_escape_text(key, -1);
        let button = new St.Button ({ label: label,
                                      style_class: 'keyboard-key' });

        button.keyWidth = 1;
        button.connect('button-press-event', Lang.bind(this,
            function () {
                this._press(key);
                return Clutter.EVENT_PROPAGATE;
            }));
        button.connect('button-release-event', Lang.bind(this,
            function () {
                this._release(key);
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
                    this._press(key);
                } else if (this._touchPressed &&
                           event.type() == Clutter.EventType.TOUCH_END &&
                           device.sequence_get_grabbed_actor(sequence) == actor) {
                    device.sequence_ungrab(sequence);
                    this._touchPressed = false;
                    this._release(key);
                }
                return Clutter.EVENT_PROPAGATE;
            }));

        return button;
    },

    _getExtendedKeys: function () {
        this._extended_keyboard = new St.BoxLayout({ style_class: 'key-container',
                                                     vertical: false });
        for (let i = 0; i < this._extended_keys.length; ++i) {
            let extendedKey = this._extended_keys[i];
            let key = this._makeKey(extendedKey);

            key.extended_key = extendedKey;
            this._extended_keyboard.add(key);

            key.width = this.actor.width;
            key.height = this.actor.height;
        }
        this._boxPointer.bin.add_actor(this._extended_keyboard);
    },

    get subkeys() {
        return this._boxPointer;
    },

    setWidth: function (width) {
        this.actor.keyWidth = width;
    },
});
Signals.addSignalMethods(Key.prototype);

var KeyboardModel = new Lang.Class({
    Name: 'KeyboardModel',

    _init: function (groupName) {
        try {
            this._model = this._loadModel(groupName);
        } catch (e) {
            this._model = this._loadModel('us');
        }
    },

    _loadModel: function(groupName) {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/osk-layouts/%s.json'.format(groupName));
        let [success, contents] = file.load_contents(null);

        return JSON.parse(contents);
    },

    getLevels: function() {
        return this._model.levels;
    },

    getKeysForLevel: function(levelName) {
        let levels = this._model.levels;
        for (let i = 0; i < levels.length; i++) {
            let level = levels[i];
            if (levels[i] == levelName)
                return levels[i];
        }
        return null;
    }
});

var Keyboard = new Lang.Class({
    Name: 'Keyboard',

    _init: function () {
        this.actor = null;
        this._focusInExtendedKeys = false;

        this._focusCaretTracker = new FocusCaretTracker.FocusCaretTracker();
        this._focusCaretTracker.connect('focus-changed', Lang.bind(this, this._onFocusChanged));
        this._focusCaretTracker.connect('caret-moved', Lang.bind(this, this._onCaretMoved));
        this._currentAccessible = null;
        this._caretTrackingEnabled = false;
        this._updateCaretPositionId = 0;

        this._enableKeyboard = false; // a11y settings value
        this._enabled = false; // enabled state (by setting or device type)

        this._a11yApplicationsSettings = new Gio.Settings({ schema_id: A11Y_APPLICATIONS_SCHEMA });
        this._a11yApplicationsSettings.connect('changed', Lang.bind(this, this._syncEnabled));
        this._lastDeviceId = null;
        this._suggestions = null;

        Meta.get_backend().connect('last-device-changed', Lang.bind(this,
            function (backend, deviceId) {
                let manager = Clutter.DeviceManager.get_default();
                let device = manager.get_device(deviceId);

                if (device.get_device_name().indexOf('XTEST') < 0) {
                    this._lastDeviceId = deviceId;
                    this._syncEnabled();
                }
            }));
        this._syncEnabled();

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

    get visible() {
        return this._keyboardVisible;
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

    _syncEnabled: function () {
        let wasEnabled = this._enabled;
        this._enableKeyboard = this._a11yApplicationsSettings.get_boolean(SHOW_KEYBOARD);
        this._enabled = this._enableKeyboard || this._lastDeviceIsTouchscreen();
        if (!this._enabled && !this._keyboardController)
            return;

        this._setCaretTrackerEnabled(this._enabled);

        if (this._enabled && !this._keyboardController)
            this._setupKeyboard();

        if (!this._enabled && wasEnabled) {
            this._hideSubkeys();
            Main.layoutManager.hideKeyboard(true);
        }
    },

    _destroyKeyboard: function() {
        if (this._keyboardNotifyId)
            this._keyboardController.disconnect(this._keyboardNotifyId);
        if (this._keyboardGroupsChangedId)
            this._keyboardController.disconnect(this._keyboardGroupsChangedId);
        if (this._keyboardStateId)
            this._keyboardController.disconnect(this._keyboardStateId);
        if (this._focusNotifyId)
            global.stage.disconnect(this._focusNotifyId);
        this._keyboard = null;
        this.actor.destroy();
        this.actor = null;
    },

    _setupKeyboard: function() {
        this.actor = new St.BoxLayout({ name: 'keyboard', vertical: true, reactive: true });
        Main.layoutManager.keyboardBox.add_actor(this.actor);
        Main.layoutManager.trackChrome(this.actor);

        this._keyboardController = new KeyboardController();

        this._groups = {};
        this._current_page = null;

        this._suggestions = new Suggestions();
        this._suggestions.connect('suggestion-clicked', Lang.bind(this, function(suggestions, str) {
            this._keyboardController.commitString(str);
        }));
        this.actor.add(this._suggestions.actor,
                       { x_align: St.Align.MIDDLE,
                         x_fill: false });

        this._addKeys();

        // Keyboard models are defined in LTR, we must override
        // the locale setting in order to avoid flipping the
        // keyboard on RTL locales.
        this.actor.text_direction = Clutter.TextDirection.LTR;

        this._keyboardNotifyId = this._keyboardController.connect('active-group', Lang.bind(this, this._onGroupChanged));
        this._keyboardGroupsChangedId = this._keyboardController.connect('groups-changed', Lang.bind(this, this._onKeyboardGroupsChanged));
        this._keyboardStateId = this._keyboardController.connect('panel-state', Lang.bind(this, this._onKeyboardStateChanged));
        this._focusNotifyId = global.stage.connect('notify::key-focus', Lang.bind(this, this._onKeyFocusChanged));
    },

    _onKeyFocusChanged: function () {
        let focus = global.stage.key_focus;

        // Showing an extended key popup and clicking a key from the extended keys
        // will grab focus, but ignore that
        let extendedKeysWereFocused = this._focusInExtendedKeys;
        this._focusInExtendedKeys = focus && (focus._extended_keys || focus.extended_key);
        if (this._focusInExtendedKeys || extendedKeysWereFocused)
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

    _createLayersForGroup: function (groupName) {
        let keyboardModel = new KeyboardModel(groupName);
        let layers = {};
        let levels = keyboardModel.getLevels();
        for (let i = 0; i < levels.length; i++) {
            let currentLevel = levels[i];
            let level = (i >= 1 && levels.length == 3) ? i + 1 : i;
            let layout = new St.BoxLayout({ style_class: 'keyboard-layout',
                                            vertical: true });
            this._loadRows(currentLevel, level, levels.length, layout);
            layers[level] = layout;
            this.actor.add(layout, { x_fill: false });

            layout.hide();
        }
        return layers;
    },

    _addKeys: function () {
        let groups = this._keyboardController.getGroups();
        for (let i = 0; i < groups.length; ++i) {
             let gname = groups[i];
             this._groups[gname] = this._createLayersForGroup(gname);
        }

        this._setActiveLayer(0);
    },

    _onCapturedEvent: function(actor, event) {
        let type = event.type();
        let press = (type == Clutter.EventType.BUTTON_PRESS || type == Clutter.EventType.TOUCH_BEGIN);
        let release = (type == Clutter.EventType.BUTTON_RELEASE || type == Clutter.EventType.TOUCH_END);

        if (press)
            this._capturedPress = true;
        else if (release && this._capturedPress)
            this._hideSubkeys();

        return Clutter.EVENT_STOP;
    },

    _addRowKeys : function (keys, keyboardRow) {
        for (let i = 0; i < keys.length; ++i) {
            let key = keys[i];
            let button = new Key(key.shift(), key);

            /* Space key gets special width, dependent on the number of surrounding keys */
            if (button.key == ' ')
                button.setWidth(keys.length <= 3 ? 5 : 3);

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
            button.connect('pressed', Lang.bind(this, function(actor, keyval, str) {
                this._hideSubkeys();
                if (!Main.inputMethod.currentFocus ||
                    !this._keyboardController.commitString(str, true)) {
                    if (keyval != 0) {
                        this._keyboardController.keyvalPress(keyval);
                        button._keyvalPress = true;
                    }
                }
            }));
            button.connect('released', Lang.bind(this, function(actor, keyval, str) {
                this._hideSubkeys();
                if (keyval != 0) {
                    if (button._keyvalPress)
                        this._keyboardController.keyvalRelease(keyval);
                    button._keyvalPress = false;
                }
            }));

            keyboardRow.add(button.container, { expand: true, x_fill: false, x_align: St.Align.END });
        }
    },

    _loadDefaultKeys: function(keys, rowActor, numLevels, numKeys) {
        let extraButton;
        for (let i = 0; i < keys.length; i++) {
            let key = keys[i];
            let keyval = key.keyval;
            let switchToLevel = key.level;
            let action = key.action;

            extraButton = new Key(key.label, []);
            rowActor.add(extraButton.container);

            extraButton.actor.add_style_class_name('default-key');
            if (key.extraClassName != null)
                extraButton.actor.add_style_class_name(key.extraClassName);
            if (key.width != null)
                extraButton.setWidth(key.width);

            extraButton.connect('released', Lang.bind(this, function() {
                if (switchToLevel != null)
                    this._onLevelChanged(switchToLevel);
                else if (keyval != null)
                    this._keyboardController.keyvalPress(keyval);
            }));
            extraButton.connect('released', Lang.bind(this, function() {
                if (keyval != null)
                    this._keyboardController.keyvalRelease(keyval);
                else if (action == 'hide')
                    this.hide();
            }));

            /* Fixup default keys based on the number of levels/keys */
            if (key.label == '⇧' && numLevels == 3) {
                if (key.right) {
                    /* Only hide the key actor, so the container still takes space */
                    extraButton.actor.hide();
                } else {
                    extraButton.container.hide();
                }
                extraButton.setWidth(1.5);
            } else if (key.right && numKeys > 8) {
                extraButton.setWidth(2);
            } else if (key.label == '⏎' && numKeys > 9) {
                extraButton.setWidth(1.5);
            }
        }
    },

    _getDefaultKeysForRow: function(row, numRows, level) {
        let pre, post;

        /* The first 2 rows in defaultKeysPre/Post belong together with
         * the first 2 rows on each keymap. On keymaps that have more than
         * 4 rows, the last 2 default key rows must be respectively
         * assigned to the 2 last keymap ones.
         */
        if (row < 2) {
            return [defaultKeysPre[level][row], defaultKeysPost[level][row]];
        } else if (row >= numRows - 2) {
            let defaultRow = row - (numRows - 2) + 2;
            return [defaultKeysPre[level][defaultRow], defaultKeysPost[level][defaultRow]];
        } else {
            return [null, null];
        }
    },

    _mergeRowKeys: function (keyboardRow, pre, row, post, numLevels) {
        if (pre != null)
            this._loadDefaultKeys(pre, keyboardRow, numLevels, row.length);

        this._addRowKeys(row, keyboardRow);

        if (post != null)
            this._loadDefaultKeys(post, keyboardRow, numLevels, row.length);
    },

    _loadRows : function (model, level, numLevels, layout) {
        let rows = model.rows;
        for (let i = 0; i < rows.length; ++i) {
            let row = rows[i];
            let keyboardRow = new St.BoxLayout({ style_class: 'keyboard-row',
                                                 x_expand: false,
                                                 x_align: Clutter.ActorAlign.END });
            layout.add(keyboardRow);

            let [pre, post] = this._getDefaultKeysForRow(i, rows.length, level);
            this._mergeRowKeys (keyboardRow, pre, row, post, numLevels);
        }
    },

    _getGridSlots: function() {
        let numOfHorizSlots = 0, numOfVertSlots;
        let rows = this._current_page.get_children();
        numOfVertSlots = rows.length;

        for (let i = 0; i < rows.length; ++i) {
            let keyboard_row = rows[i];
            let keys = keyboard_row.get_children();

            numOfHorizSlots = Math.max(numOfHorizSlots, keys.length);
        }

        return [numOfHorizSlots, numOfVertSlots];
    },

    _redraw: function () {
        if (!this._enabled || !this._current_page)
            return;

        let monitor = Main.layoutManager.keyboardMonitor;
        let maxHeight = monitor.height / 3;
        this.actor.width = monitor.width;

        let layout = this._current_page;
        let verticalSpacing = layout.get_theme_node().get_length('spacing');
        let padding = layout.get_theme_node().get_length('padding');

        let [numOfHorizSlots, numOfVertSlots] = this._getGridSlots ();

        let box = layout.get_children()[0].get_children()[0];
        let horizontalSpacing = box.get_theme_node().get_length('spacing');
        let allHorizontalSpacing = (numOfHorizSlots - 1) * horizontalSpacing;
        let keyWidth = Math.floor((this.actor.width - allHorizontalSpacing - 2 * padding) / numOfHorizSlots);

        let allVerticalSpacing = (numOfVertSlots - 1) * verticalSpacing;
        let keyHeight = Math.floor((maxHeight - allVerticalSpacing - 2 * padding) / numOfVertSlots);

        let keySize = Math.min(keyWidth, keyHeight);
        layout.height = keySize * numOfVertSlots + allVerticalSpacing + 2 * padding;

        let rows = this._current_page.get_children();
        for (let i = 0; i < rows.length; ++i) {
            let keyboard_row = rows[i];
            let keys = keyboard_row.get_children();
            for (let j = 0; j < keys.length; ++j) {
                let child = keys[j];
                let keyActor = child.get_children()[0];
                child.width = keySize * keyActor.keyWidth;
                child.height = keySize;
                if (keyActor._extended_keys) {
                    let extended_keys = keyActor._extended_keys.get_children();
                    for (let n = 0; n < extended_keys.length; ++n) {
                        let extended_key = extended_keys[n];
                        extended_key.width = keySize;
                        extended_key.height = keySize;
                    }
                }
            }
        }
    },

    _onLevelChanged: function (level) {
        this._setActiveLayer(level);
    },

    _onGroupChanged: function () {
        this._setActiveLayer(0);
    },

    _onKeyboardGroupsChanged: function(keyboard) {
        this._groups = [];
        this._addKeys();
    },

    _onKeyboardStateChanged: function(controller, state) {
        let enabled;
        if (state == Clutter.InputPanelState.OFF)
            enabled = false;
        else if (state == Clutter.InputPanelState.ON)
            enabled = true;
        else if (state == Clutter.InputPanelState.TOGGLE)
            enabled = (this._keyboardVisible == false);
        else
            return;

        if (enabled)
            this.show(Main.layoutManager.focusIndex);
        else
            this.hide();
    },

    _setActiveLayer: function (activeLevel) {
        let activeGroupName = this._keyboardController.getCurrentGroup();
        let layers = this._groups[activeGroupName];

        if (this._current_page != null) {
            this._current_page.hide();
        }

        this._current_page = layers[activeLevel];
        this._redraw();
        this._current_page.show();
    },

    shouldTakeEvent: function(event) {
        let actor = event.get_source();
        return Main.layoutManager.keyboardBox.contains(actor) ||
               !!actor._extended_keys || !!actor.extended_key;
    },

    _clearKeyboardRestTimer: function() {
        if (!this._keyboardRestingId)
            return;
        GLib.source_remove(this._keyboardRestingId);
        this._keyboardRestingId = 0;
    },

    show: function (monitor) {
        if (!this._enabled)
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
    },

    hide: function () {
        if (!this._enabled)
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

    resetSuggestions: function(suggestions) {
        if (!this._suggestions)
            return;
        this._suggestions.clear();
        this._suggestions.actor.hide();
    },

    addSuggestion: function(text, callback) {
        if (!this._suggestions)
            return;
        this._suggestions.add(text, callback);
        this._suggestions.actor.show();
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
        if (!this._enabled)
            return;

//        this._setLocation(x, y);
    },

    setEntryLocation: function(x, y, w, h) {
        if (!this._enabled)
            return;

//        this._setLocation(x, y);
    },
});

var KeyboardController = new Lang.Class({
    Name: 'KeyboardController',

    _init: function () {
        this.parent();
        let deviceManager = Clutter.DeviceManager.get_default();
        this._virtualDevice = deviceManager.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

        this._inputSourceManager = InputSourceManager.getInputSourceManager();
        this._sourceChangedId = this._inputSourceManager.connect('current-source-changed',
                                                                 Lang.bind(this, this._onSourceChanged));
        this._sourcesModifiedId = this._inputSourceManager.connect ('sources-changed',
                                                                    Lang.bind(this, this._onSourcesModified));
        this._currentSource = this._inputSourceManager.currentSource;

        Main.inputMethod.connect('notify::content-purpose', Lang.bind(this, this._onContentPurposeHintsChanged));
        Main.inputMethod.connect('notify::content-hints', Lang.bind(this, this._onContentPurposeHintsChanged));
        Main.inputMethod.connect('input-panel-state', Lang.bind(this, function(o, state) { this.emit('panel-state', state); }));
    },

    _onSourcesModified: function () {
        this.emit('groups-changed');
    },

    _onSourceChanged: function (inputSourceManager, oldSource) {
        let source = inputSourceManager.currentSource;
        this._currentSource = source;
        this.emit('active-group', source.id);
    },

    _onContentPurposeHintsChanged: function(method) {
        let hints = method.content_hints;
        let purpose = method.content_purpose;

        // XXX: hook numeric/emoji/etc special keyboards
    },

    getGroups: function () {
        let inputSources = this._inputSourceManager.inputSources;
        let groups = []

        for (let i in inputSources) {
            let is = inputSources[i];
            groups[is.index] = is.xkbId;
        }

        return groups;
    },

    getCurrentGroup: function () {
        return this._currentSource.xkbId;
    },

    commitString: function(string, fromKey) {
        if (string == null)
            return false;
        /* Let ibus methods fall through keyval emission */
        if (fromKey && this._currentSource.type == InputSourceManager.INPUT_SOURCE_TYPE_IBUS)
            return false;

        Main.inputMethod.commit(string);
        return true;
    },

    keyvalPress: function(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.PRESSED);
    },

    keyvalRelease: function(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.RELEASED);
    },
});
Signals.addSignalMethods(KeyboardController.prototype);
