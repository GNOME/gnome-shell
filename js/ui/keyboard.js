// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

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

const IBusManager = imports.misc.ibusManager;
const BoxPointer = imports.ui.boxpointer;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

var KEYBOARD_REST_TIME = Layout.KEYBOARD_ANIMATION_TIME * 2 * 1000;
var KEY_LONG_PRESS_TIME = 250;

const A11Y_APPLICATIONS_SCHEMA = 'org.gnome.desktop.a11y.applications';
const SHOW_KEYBOARD = 'screen-keyboard-enabled';

/* KeyContainer puts keys in a grid where a 1:1 key takes this size */
const KEY_SIZE = 2;

const defaultKeysPre = [
    [ [], [], [{ width: 1.5, level: 1, extraClassName: 'shift-key-lowercase' }], [{ label: '?123', width: 1.5, level: 2 }] ],
    [ [], [], [{ width: 1.5, level: 0, extraClassName: 'shift-key-uppercase' }], [{ label: '?123', width: 1.5, level: 2 }] ],
    [ [], [], [{ label: '=/<', width: 1.5, level: 3 }], [{ label: 'ABC', width: 1.5, level: 0 }] ],
    [ [], [], [{ label: '?123', width: 1.5, level: 2 }], [{ label: 'ABC', width: 1.5, level: 0 }] ],
];

const defaultKeysPost = [
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ width: 3, level: 1, right: true, extraClassName: 'shift-key-lowercase' }],
      [{ width: 1.5, action: 'languageMenu', extraClassName: 'layout-key' }, { width: 1.5, action: 'hide', extraClassName: 'hide-key' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ width: 3, level: 0, right: true, extraClassName: 'shift-key-uppercase' }],
      [{ width: 1.5, action: 'languageMenu', extraClassName: 'layout-key' }, { width: 1.5, action: 'hide', extraClassName: 'hide-key' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '=/<', width: 3, level: 3, right: true }],
      [{ width: 1.5, action: 'languageMenu', extraClassName: 'layout-key' }, { width: 1.5, action: 'hide', extraClassName: 'hide-key' }] ],
    [ [{ label: '⌫', width: 1.5, keyval: Clutter.KEY_BackSpace }],
      [{ width: 2, keyval: Clutter.KEY_Return, extraClassName: 'enter-key' }],
      [{ label: '?123', width: 3, level: 2, right: true }],
      [{ width: 1.5, action: 'languageMenu', extraClassName: 'layout-key' }, { width: 1.5, action: 'hide', extraClassName: 'hide-key' }] ],
];

var KeyContainer = new Lang.Class({
    Name: 'KeyContainer',
    Extends: St.Widget,

    _init() {
        let gridLayout = new Clutter.GridLayout({ orientation: Clutter.Orientation.HORIZONTAL,
                                                  column_homogeneous: true,
                                                  row_homogeneous: true });
        this.parent({ layout_manager: gridLayout });
        this._gridLayout = gridLayout;
        this._currentRow = 0;
        this._currentCol = 0;
        this._maxCols = 0;

        this._currentRow = null;
        this._rows = [];
    },

    appendRow(length) {
        this._currentRow++;
        this._currentCol = 0;

        let row = new Object();
        row.keys = [];
        row.width = 0;
        this._rows.push(row);
    },

    appendKey(key, width = 1, height = 1) {
        let keyInfo = {
            key,
            left: this._currentCol,
            top: this._currentRow,
            width,
            height
        };

        let row = this._rows[this._rows.length - 1];
        row.keys.push(keyInfo);
        row.width += width;

        this._currentCol += width;
        this._maxCols = Math.max(this._currentCol, this._maxCols);
    },

    vfunc_allocate(box, flags) {
        if (box.get_width() > 0 && box.get_height() > 0 && this._maxCols > 0) {
            let keyboardRatio = this._maxCols / this._rows.length;
            let sizeRatio = box.get_width() / box.get_height();

            if (sizeRatio >= keyboardRatio) {
                /* Restrict horizontally */
                let width = box.get_height() * keyboardRatio;
                let diff = box.get_width() - width;

                box.x1 += Math.floor(diff / 2);
                box.x2 -= Math.ceil(diff / 2);
            } else {
                /* Restrict vertically */
                let height = box.get_width() / keyboardRatio;
                let diff = box.get_height() - height;

                box.y1 += Math.floor(diff / 2);
                box.y2 -= Math.floor(diff / 2);
            }
        }

        this.parent (box, flags);
    },

    layoutButtons() {
        let nCol = 0, nRow = 0;

        for (let i = 0; i < this._rows.length; i++) {
            let row = this._rows[i];

            /* When starting a new row, see if we need some padding */
            if (nCol == 0) {
                let diff = this._maxCols - row.width;
                if (diff >= 1)
                    nCol = diff * KEY_SIZE / 2;
                else
                    nCol = diff * KEY_SIZE;
            }

            for (let j = 0; j < row.keys.length; j++) {
                let keyInfo = row.keys[j];
                let width = keyInfo.width * KEY_SIZE;
                let height = keyInfo.height * KEY_SIZE;

                this._gridLayout.attach(keyInfo.key, nCol, nRow, width, height);
                nCol += width;
            }

            nRow += KEY_SIZE;
            nCol = 0;
        }
    }
});

var Suggestions = new Lang.Class({
    Name: 'Suggestions',

    _init() {
        this.actor = new St.BoxLayout({ style_class: 'word-suggestions',
                                        vertical: false });
        this.actor.show();
    },

    add(word, callback) {
        let button = new St.Button({ label: word });
        button.connect('clicked', callback);
        this.actor.add(button);
    },

    clear() {
        this.actor.remove_all_children();
    },
});
Signals.addSignalMethods(Suggestions.prototype);

var LanguageSelectionPopup = new Lang.Class({
    Name: 'LanguageSelectionPopup',
    Extends: PopupMenu.PopupMenu,

    _init(actor) {
        this.parent(actor, 0.5, St.Side.BOTTOM);

        let inputSourceManager = InputSourceManager.getInputSourceManager();
        let inputSources = inputSourceManager.inputSources;

        for (let i in inputSources) {
            let is = inputSources[i];

            this.addAction(is.displayName, () => {
                inputSourceManager.activateInputSource(is, true);
            });
        }

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.addAction(_("Region & Language Settings"), this._launchSettings.bind(this));
        this._capturedEventId = 0;

        this._unmapId = actor.connect('notify::mapped', () => {
            if (!actor.is_mapped())
                this.close(true);
        });
    },

    _launchSettings() {
        Util.spawn(['gnome-control-center', 'region']);
        this.close(true);
    },

    _onCapturedEvent(actor, event) {
        if (event.get_source() == this.actor ||
            this.actor.contains(event.get_source()))
            return Clutter.EVENT_PROPAGATE;

        if (event.type() == Clutter.EventType.BUTTON_RELEASE || event.type() == Clutter.EventType.TOUCH_END)
            this.close(true);

        return Clutter.EVENT_STOP;
    },

    open(animate) {
        this.parent(animate);
        this._capturedEventId = global.stage.connect('captured-event',
                                                     this._onCapturedEvent.bind(this));
    },

    close(animate) {
        this.parent(animate);
        if (this._capturedEventId != 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
    },

    destroy() {
        if (this._capturedEventId != 0)
            global.stage.disconnect(this._capturedEventId);
        if (this._unmapId != 0)
            this.sourceActor.disconnect(this._unmapId);
        this.parent();
    },
});

var Key = new Lang.Class({
    Name: 'Key',

    _init(key, extendedKeys) {
        this.key = key || "";
        this.keyButton = this._makeKey(this.key);

        /* Add the key in a container, so keys can be padded without losing
         * logical proportions between those.
         */
        this.actor = new St.BoxLayout ({ style_class: 'key-container' });
        this.actor.add(this.keyButton, { expand: true, x_fill: true });
        this.actor.connect('destroy', this._onDestroy.bind(this));

        this._extended_keys = extendedKeys;
        this._extended_keyboard = null;
        this._pressTimeoutId = 0;
        this._capturedPress = false;

        this._capturedEventId = 0;
        this._unmapId = 0;
        this._longPress = false;
    },

    _onDestroy() {
        if (this._boxPointer) {
            this._boxPointer.actor.destroy();
            this._boxPointer = null;
        }
    },

    _ensureExtendedKeysPopup() {
        if (this._extended_keys.length == 0)
            return;

        this._boxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM,
                                                     { x_fill: true,
                                                       y_fill: true,
                                                       x_align: St.Align.START });
        this._boxPointer.actor.hide();
        Main.layoutManager.addChrome(this._boxPointer.actor);
        this._boxPointer.setPosition(this.keyButton, 0.5);

        // Adds style to existing keyboard style to avoid repetition
        this._boxPointer.actor.add_style_class_name('keyboard-subkeys');
        this._getExtendedKeys();
        this.keyButton._extended_keys = this._extended_keyboard;
    },

    _getKeyval(key) {
        let unicode = String.charCodeAt(key, 0);
        return Gdk.unicode_to_keyval(unicode);
    },

    _press(key) {
        if (key != this.key || this._extended_keys.length == 0) {
            this.emit('pressed', this._getKeyval(key), key);
        }

        if (key == this.key) {
            this._pressTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                    KEY_LONG_PRESS_TIME,
                                                    () => {
                                                        this._longPress = true;
                                                        this._pressTimeoutId = 0;

                                                        this.emit('long-press');

                                                        if (this._extended_keys.length > 0) {
                                                            this._touchPressed = false;
                                                            this.keyButton.set_hover(false);
                                                            this.keyButton.fake_release();
                                                            this._ensureExtendedKeysPopup();
                                                            this._showSubkeys();
                                                        }

                                                        return GLib.SOURCE_REMOVE;
                                                    });
        }
    },

    _release(key) {
        if (this._pressTimeoutId != 0) {
            GLib.source_remove(this._pressTimeoutId);
            this._pressTimeoutId = 0;
        }

        if (!this._longPress && key == this.key && this._extended_keys.length > 0)
            this.emit('pressed', this._getKeyval(key), key);

        this.emit('released', this._getKeyval(key), key);
        this._hideSubkeys();
        this._longPress = false;
    },

    _onCapturedEvent(actor, event) {
        let type = event.type();
        let press = (type == Clutter.EventType.BUTTON_PRESS || type == Clutter.EventType.TOUCH_BEGIN);
        let release = (type == Clutter.EventType.BUTTON_RELEASE || type == Clutter.EventType.TOUCH_END);

        if (event.get_source() == this._boxPointer.bin ||
            this._boxPointer.bin.contains(event.get_source()))
            return Clutter.EVENT_PROPAGATE;

        if (press)
            this._capturedPress = true;
        else if (release && this._capturedPress)
            this._hideSubkeys();

        return Clutter.EVENT_STOP;
    },

    _showSubkeys() {
        this._boxPointer.show(BoxPointer.PopupAnimation.FULL);
        this._capturedEventId = global.stage.connect('captured-event',
                                                     this._onCapturedEvent.bind(this));
        this._unmapId = this.keyButton.connect('notify::mapped', () => {
            if (!this.keyButton.is_mapped())
                this._hideSubkeys();
        });
    },

    _hideSubkeys() {
        if (this._boxPointer)
            this._boxPointer.hide(BoxPointer.PopupAnimation.FULL);
        if (this._capturedEventId) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
        if (this._unmapId) {
            this.keyButton.disconnect(this._unmapId);
            this._unmapId = 0;
        }
        this._capturedPress = false;
    },

    _makeKey(key) {
        let label = GLib.markup_escape_text(key, -1);
        let button = new St.Button ({ label: label,
                                      style_class: 'keyboard-key' });

        button.keyWidth = 1;
        button.connect('button-press-event', () => {
            this._press(key);
            return Clutter.EVENT_PROPAGATE;
        });
        button.connect('button-release-event', () => {
            this._release(key);
            return Clutter.EVENT_PROPAGATE;
        });
        button.connect('touch-event', (actor, event) => {
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
        });

        return button;
    },

    _getExtendedKeys() {
        this._extended_keyboard = new St.BoxLayout({ style_class: 'key-container',
                                                     vertical: false });
        for (let i = 0; i < this._extended_keys.length; ++i) {
            let extendedKey = this._extended_keys[i];
            let key = this._makeKey(extendedKey);

            key.extended_key = extendedKey;
            this._extended_keyboard.add(key);

            key.width = this.keyButton.width;
            key.height = this.keyButton.height;
        }
        this._boxPointer.bin.add_actor(this._extended_keyboard);
    },

    get subkeys() {
        return this._boxPointer;
    },

    setWidth(width) {
        this.keyButton.keyWidth = width;
    },

    setLatched(latched) {
        if (latched)
            this.keyButton.add_style_pseudo_class('latched');
        else
            this.keyButton.remove_style_pseudo_class('latched');
    },
});
Signals.addSignalMethods(Key.prototype);

var KeyboardModel = new Lang.Class({
    Name: 'KeyboardModel',

    _init(groupName) {
        try {
            this._model = this._loadModel(groupName);
        } catch (e) {
            this._model = this._loadModel('us');
        }
    },

    _loadModel(groupName) {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/osk-layouts/%s.json'.format(groupName));
        let [success, contents] = file.load_contents(null);

        return JSON.parse(contents);
    },

    getLevels() {
        return this._model.levels;
    },

    getKeysForLevel(levelName) {
        return this._model.levels.find(level => level == levelName);
    }
});

var FocusTracker = new Lang.Class({
    Name: 'FocusTracker',

    _init() {
        this._currentWindow = null;
        this._currentWindowPositionId = 0;

        global.screen.get_display().connect('notify::focus-window', () => {
            this._setCurrentWindow(global.screen.get_display().focus_window);
            this.emit('window-changed', this._currentWindow);
        });

        /* Valid for wayland clients */
        Main.inputMethod.connect('cursor-location-changed', (o, rect) => {
            let newRect = { x: rect.get_x(), y: rect.get_y(), width: rect.get_width(), height: rect.get_height() };
            this._setCurrentRect(newRect);
        });

        this._ibusManager = IBusManager.getIBusManager();
        this._ibusManager.connect('set-cursor-location', (manager, rect) => {
            /* Valid for X11 clients only */
            if (Main.inputMethod.currentFocus)
                return;

            this._setCurrentRect(rect);
        });
    },

    get currentWindow() {
        return this._currentWindow;
    },

    _setCurrentWindow(window) {
        if (this._currentWindow)
            this._currentWindow.disconnect(this._currentWindowPositionId);

        this._currentWindow = window;
        if (window) {
            this._currentWindowPositionId = this._currentWindow.connect('position-changed', () => {
                if (global.display.get_grab_op() == Meta.GrabOp.NONE)
                    this.emit('position-changed');
                else
                    this.emit('reset');
            });
        }
    },

    _setCurrentRect(rect) {
        if (this._currentWindow) {
            let frameRect = this._currentWindow.get_frame_rect();
            rect.x -= frameRect.x;
            rect.y -= frameRect.y;
        }

        this._rect = rect;
        this.emit('position-changed');
    },

    getCurrentRect() {
        let rect = { x: this._rect.x, y: this._rect.y,
                     width: this._rect.width, height: this._rect.height };

        if (this._currentWindow) {
            let frameRect = this._currentWindow.get_frame_rect();
            rect.x += frameRect.x;
            rect.y += frameRect.y;
        }

        return rect;
    }
});
Signals.addSignalMethods(FocusTracker.prototype);

var Keyboard = new Lang.Class({
    Name: 'Keyboard',

    _init() {
        this.actor = null;
        this._focusInExtendedKeys = false;

        this._languagePopup = null;
        this._currentFocusWindow = null;
        this._animFocusedWindow = null;
        this._delayedAnimFocusWindow = null;

        this._enableKeyboard = false; // a11y settings value
        this._enabled = false; // enabled state (by setting or device type)
        this._latched = false; // current level is latched

        this._a11yApplicationsSettings = new Gio.Settings({ schema_id: A11Y_APPLICATIONS_SCHEMA });
        this._a11yApplicationsSettings.connect('changed', this._syncEnabled.bind(this));
        this._lastDeviceId = null;
        this._suggestions = null;

        this._focusTracker = new FocusTracker();
        this._focusTracker.connect('position-changed', this._onFocusPositionChanged.bind(this));
        this._focusTracker.connect('reset', () => {
            this._delayedAnimFocusWindow = null;
            this._animFocusedWindow = null;
            this._oskFocusWindow = null;
        });

        Meta.get_backend().connect('last-device-changed', 
            (backend, deviceId) => {
                let manager = Clutter.DeviceManager.get_default();
                let device = manager.get_device(deviceId);

                if (device.get_device_name().indexOf('XTEST') < 0) {
                    this._lastDeviceId = deviceId;
                    this._syncEnabled();
                }
            });
        this._syncEnabled();

        this._showIdleId = 0;

        this._keyboardVisible = false;
        Main.layoutManager.connect('keyboard-visible-changed', (o, visible) => {
            this._keyboardVisible = visible;
        });
        this._keyboardRequested = false;
        this._keyboardRestingId = 0;

        Main.layoutManager.connect('monitors-changed', this._relayout.bind(this));
    },

    get visible() {
        return this._keyboardVisible;
    },

    _onFocusPositionChanged(focusTracker) {
        let rect = focusTracker.getCurrentRect();
        this.setCursorLocation(focusTracker.currentWindow, rect.x, rect.y, rect.width, rect.height);
    },

    _lastDeviceIsTouchscreen() {
        if (!this._lastDeviceId)
            return false;

        let manager = Clutter.DeviceManager.get_default();
        let device = manager.get_device(this._lastDeviceId);

        if (!device)
            return false;

        return device.get_device_type() == Clutter.InputDeviceType.TOUCHSCREEN_DEVICE;
    },

    _syncEnabled() {
        let wasEnabled = this._enabled;
        this._enableKeyboard = this._a11yApplicationsSettings.get_boolean(SHOW_KEYBOARD);
        this._enabled = this._enableKeyboard || this._lastDeviceIsTouchscreen();
        if (!this._enabled && !this._keyboardController)
            return;

        if (this._enabled && !this._keyboardController)
            this._setupKeyboard();
        else if (!this._enabled)
            this.setCursorLocation(null);

        if (!this._enabled && wasEnabled)
            Main.layoutManager.hideKeyboard(true);
    },

    _destroyKeyboard() {
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

        if (this._languagePopup) {
            this._languagePopup.destroy();
            this._languagePopup = null;
        }
    },

    _setupKeyboard() {
        this.actor = new St.BoxLayout({ name: 'keyboard', vertical: true, reactive: true });
        Main.layoutManager.keyboardBox.add_actor(this.actor);
        Main.layoutManager.trackChrome(this.actor);

        this._keyboardController = new KeyboardController();

        this._groups = {};
        this._current_page = null;

        this._suggestions = new Suggestions();
        this._suggestions.connect('suggestion-clicked', (suggestions, str) => {
            this._keyboardController.commitString(str);
        });
        this.actor.add(this._suggestions.actor,
                       { x_align: St.Align.MIDDLE,
                         x_fill: false });

        this._ensureKeysForGroup(this._keyboardController.getCurrentGroup());
        this._setActiveLayer(0);

        // Keyboard models are defined in LTR, we must override
        // the locale setting in order to avoid flipping the
        // keyboard on RTL locales.
        this.actor.text_direction = Clutter.TextDirection.LTR;

        this._keyboardNotifyId = this._keyboardController.connect('active-group', this._onGroupChanged.bind(this));
        this._keyboardGroupsChangedId = this._keyboardController.connect('groups-changed', this._onKeyboardGroupsChanged.bind(this));
        this._keyboardStateId = this._keyboardController.connect('panel-state', this._onKeyboardStateChanged.bind(this));
        this._focusNotifyId = global.stage.connect('notify::key-focus', this._onKeyFocusChanged.bind(this));

        this._relayout();
    },

    _onKeyFocusChanged() {
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
          this._showIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
              this.show(Main.layoutManager.focusIndex);
              return GLib.SOURCE_REMOVE;
          });
          GLib.Source.set_name_by_id(this._showIdleId, '[gnome-shell] this.show');
        }
    },

    _createLayersForGroup(groupName) {
        let keyboardModel = new KeyboardModel(groupName);
        let layers = {};
        let levels = keyboardModel.getLevels();
        for (let i = 0; i < levels.length; i++) {
            let currentLevel = levels[i];
            /* There are keyboard maps which consist of 3 levels (no uppercase,
             * basically). We however make things consistent by skipping that
             * second level.
             */
            let level = (i >= 1 && levels.length == 3) ? i + 1 : i;

            let layout = new KeyContainer();
            layout.shiftKeys = [];

            this._loadRows(currentLevel, level, levels.length, layout);
            layers[level] = layout;
            this.actor.add(layout, { expand: true });
            layout.layoutButtons();

            layout.hide();
        }
        return layers;
    },

    _ensureKeysForGroup(group) {
        if (!this._groups[group])
            this._groups[group] = this._createLayersForGroup(group);
    },

    _addRowKeys(keys, layout) {
        for (let i = 0; i < keys.length; ++i) {
            let key = keys[i];
            let button = new Key(key.shift(), key);

            /* Space key gets special width, dependent on the number of surrounding keys */
            if (button.key == ' ')
                button.setWidth(keys.length <= 3 ? 5 : 3);

            button.connect('pressed', (actor, keyval, str) => {
                if (!Main.inputMethod.currentFocus ||
                    !this._keyboardController.commitString(str, true)) {
                    if (keyval != 0) {
                        this._keyboardController.keyvalPress(keyval);
                        button._keyvalPress = true;
                    }
                }
            });
            button.connect('released', (actor, keyval, str) => {
                if (keyval != 0) {
                    if (button._keyvalPress)
                        this._keyboardController.keyvalRelease(keyval);
                    button._keyvalPress = false;
                }

                if (!this._latched)
                    this._setActiveLayer(0);
            });

            layout.appendKey(button.actor, button.keyButton.keyWidth);
        }
    },

    _popupLanguageMenu(keyActor) {
        if (this._languagePopup)
            this._languagePopup.destroy();

        this._languagePopup = new LanguageSelectionPopup(keyActor);
        Main.layoutManager.addChrome(this._languagePopup.actor);
        this._languagePopup.open(true);
    },

    _loadDefaultKeys(keys, layout, numLevels, numKeys) {
        let extraButton;
        for (let i = 0; i < keys.length; i++) {
            let key = keys[i];
            let keyval = key.keyval;
            let switchToLevel = key.level;
            let action = key.action;

            extraButton = new Key(key.label, []);

            extraButton.keyButton.add_style_class_name('default-key');
            if (key.extraClassName != null)
                extraButton.keyButton.add_style_class_name(key.extraClassName);
            if (key.width != null)
                extraButton.setWidth(key.width);

            let actor = extraButton.keyButton;

            extraButton.connect('pressed', () => {
                if (switchToLevel != null) {
                    this._setActiveLayer(switchToLevel);
                    // Shift only gets latched on long press
                    this._latched = (switchToLevel != 1);
                } else if (keyval != null) {
                    this._keyboardController.keyvalPress(keyval);
                }
            });
            extraButton.connect('released', () => {
                if (keyval != null)
                    this._keyboardController.keyvalRelease(keyval);
                else if (action == 'hide')
                    this.hide();
                else if (action == 'languageMenu')
                    this._popupLanguageMenu(actor);
            });

            if (switchToLevel == 0) {
                layout.shiftKeys.push(extraButton);
            } else if (switchToLevel == 1) {
                extraButton.connect('long-press', () => {
                    this._latched = true;
                    this._setCurrentLevelLatched(this._current_page, this._latched);
                });
            }

            /* Fixup default keys based on the number of levels/keys */
            if (switchToLevel == 1 && numLevels == 3) {
                // Hide shift key if the keymap has no uppercase level
                if (key.right) {
                    /* Only hide the key actor, so the container still takes space */
                    extraButton.keyButton.hide();
                } else {
                    extraButton.actor.hide();
                }
                extraButton.setWidth(1.5);
            } else if (key.right && numKeys > 8) {
                extraButton.setWidth(2);
            } else if (keyval == Clutter.KEY_Return && numKeys > 9) {
                extraButton.setWidth(1.5);
            }

            layout.appendKey(extraButton.actor, extraButton.keyButton.keyWidth);
        }
    },

    _setCurrentLevelLatched(layout, latched) {
        for (let i = 0; layout.shiftKeys[i]; i++) {
            let key = layout.shiftKeys[i];
            key.setLatched(latched);
        }
    },

    _getDefaultKeysForRow(row, numRows, level) {
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

    _mergeRowKeys(layout, pre, row, post, numLevels) {
        if (pre != null)
            this._loadDefaultKeys(pre, layout, numLevels, row.length);

        this._addRowKeys(row, layout);

        if (post != null)
            this._loadDefaultKeys(post, layout, numLevels, row.length);
    },

    _loadRows(model, level, numLevels, layout) {
        let rows = model.rows;
        for (let i = 0; i < rows.length; ++i) {
            layout.appendRow();
            let [pre, post] = this._getDefaultKeysForRow(i, rows.length, level);
            this._mergeRowKeys (layout, pre, rows[i], post, numLevels);
        }
    },

    _getGridSlots() {
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

    _relayout() {
        let monitor = Main.layoutManager.keyboardMonitor;

        if (this.actor == null || monitor == null)
            return;

        let maxHeight = monitor.height / 3;
        this.actor.width = monitor.width;
        this.actor.height = maxHeight;
    },

    _onGroupChanged() {
        this._ensureKeysForGroup(this._keyboardController.getCurrentGroup());
        this._setActiveLayer(0);
    },

    _onKeyboardGroupsChanged(keyboard) {
        this._groups = [];
        this._onGroupChanged();
    },

    _onKeyboardStateChanged(controller, state) {
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

    _setActiveLayer(activeLevel) {
        let activeGroupName = this._keyboardController.getCurrentGroup();
        let layers = this._groups[activeGroupName];

        if (this._current_page != null) {
            this._setCurrentLevelLatched(this._current_page, false);
            this._current_page.hide();
        }

        this._current_page = layers[activeLevel];
        this._current_page.show();
    },

    shouldTakeEvent(event) {
        let actor = event.get_source();
        return Main.layoutManager.keyboardBox.contains(actor) ||
               !!actor._extended_keys || !!actor.extended_key;
    },

    _clearKeyboardRestTimer() {
        if (!this._keyboardRestingId)
            return;
        GLib.source_remove(this._keyboardRestingId);
        this._keyboardRestingId = 0;
    },

    show(monitor) {
        if (!this._enabled)
            return;

        this._clearShowIdle();
        this._keyboardRequested = true;

        if (this._keyboardVisible) {
            if (monitor != Main.layoutManager.keyboardIndex) {
                Main.layoutManager.keyboardIndex = monitor;
                this._relayout();
            }
            return;
        }

        this._clearKeyboardRestTimer();
        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                   KEYBOARD_REST_TIME,
                                                   () => {
                                                       this._clearKeyboardRestTimer();
                                                       this._show(monitor);
                                                       return GLib.SOURCE_REMOVE;
                                                   });
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    },

    _show(monitor) {
        if (!this._keyboardRequested)
            return;

        Main.layoutManager.keyboardIndex = monitor;
        this._relayout();
        Main.layoutManager.showKeyboard();

        if (this._delayedAnimFocusWindow) {
            this._setAnimationWindow(this._delayedAnimFocusWindow);
            this._delayedAnimFocusWindow = null;
        }
    },

    hide() {
        if (!this._enabled)
            return;

        this._clearShowIdle();
        this._keyboardRequested = false;

        if (!this._keyboardVisible)
            return;

        this._clearKeyboardRestTimer();
        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                   KEYBOARD_REST_TIME,
                                                   () => {
                                                       this._clearKeyboardRestTimer();
                                                       this._hide();
                                                       return GLib.SOURCE_REMOVE;
                                                   });
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    },

    _hide() {
        if (this._keyboardRequested)
            return;

        Main.layoutManager.hideKeyboard();
        this.setCursorLocation(null);
    },

    _hideSubkeys() {
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

    resetSuggestions() {
        if (this._suggestions)
            this._suggestions.clear();
    },

    addSuggestion(text, callback) {
        if (!this._suggestions)
            return;
        this._suggestions.add(text, callback);
        this._suggestions.actor.show();
    },

    _clearShowIdle() {
        if (!this._showIdleId)
            return;
        GLib.source_remove(this._showIdleId);
        this._showIdleId = 0;
    },

    _windowSlideAnimationComplete(window, delta) {
        // Synchronize window and actor positions again.
        let windowActor = window.get_compositor_private();
        let frameRect = window.get_frame_rect();
        frameRect.y += delta;
        window.move_frame(true, frameRect.x, frameRect.y);
    },

    _animateWindow(window, show) {
        let windowActor = window.get_compositor_private();
        let deltaY = Main.layoutManager.keyboardBox.height;
        if (!windowActor)
            return;

        if (show) {
            Tweener.addTween(windowActor,
                             { y: windowActor.y - deltaY,
                               time: Layout.KEYBOARD_ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: this._windowSlideAnimationComplete,
                               onCompleteParams: [window, -deltaY] });
        } else {
            Tweener.addTween(windowActor,
                             { y: windowActor.y + deltaY,
                               time: Layout.KEYBOARD_ANIMATION_TIME,
                               transition: 'easeInQuad',
                               onComplete: this._windowSlideAnimationComplete,
                               onCompleteParams: [window, deltaY] });
        }
    },

    _setAnimationWindow(window) {
        if (this._animFocusedWindow == window)
            return;

        if (this._animFocusedWindow)
            this._animateWindow(this._animFocusedWindow, false);
        if (window)
            this._animateWindow(window, true);

        this._animFocusedWindow = window;
    },

    setCursorLocation(window, x, y , w, h) {
        let monitor = Main.layoutManager.keyboardMonitor;

        if (window && monitor) {
            let keyboardHeight = Main.layoutManager.keyboardBox.height;
            let focusObscured = false;

            if (y + h >= monitor.y + monitor.height - keyboardHeight) {
                if (this._keyboardVisible)
                    this._setAnimationWindow(window);
                else
                    this._delayedAnimFocusWindow = window;
            } else if (y < keyboardHeight) {
                this._delayedAnimFocusWindow = null;
                this._setAnimationWindow(null);
            }
        } else {
            this._setAnimationWindow(null);
        }

        this._oskFocusWindow = window;
    },
});

var KeyboardController = new Lang.Class({
    Name: 'KeyboardController',

    _init() {
        this.parent();
        let deviceManager = Clutter.DeviceManager.get_default();
        this._virtualDevice = deviceManager.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

        this._inputSourceManager = InputSourceManager.getInputSourceManager();
        this._sourceChangedId = this._inputSourceManager.connect('current-source-changed',
                                                                 this._onSourceChanged.bind(this));
        this._sourcesModifiedId = this._inputSourceManager.connect ('sources-changed',
                                                                    this._onSourcesModified.bind(this));
        this._currentSource = this._inputSourceManager.currentSource;

        Main.inputMethod.connect('notify::content-purpose',
                                 this._onContentPurposeHintsChanged.bind(this));
        Main.inputMethod.connect('notify::content-hints',
                                 this._onContentPurposeHintsChanged.bind(this));
        Main.inputMethod.connect('input-panel-state', (o, state) => {
            this.emit('panel-state', state);
        });
    },

    _onSourcesModified() {
        this.emit('groups-changed');
    },

    _onSourceChanged(inputSourceManager, oldSource) {
        let source = inputSourceManager.currentSource;
        this._currentSource = source;
        this.emit('active-group', source.id);
    },

    _onContentPurposeHintsChanged(method) {
        let hints = method.content_hints;
        let purpose = method.content_purpose;

        // XXX: hook numeric/emoji/etc special keyboards
    },

    getGroups() {
        let inputSources = this._inputSourceManager.inputSources;
        let groups = []

        for (let i in inputSources) {
            let is = inputSources[i];
            groups[is.index] = is.xkbId;
        }

        return groups;
    },

    getCurrentGroup() {
        return this._currentSource.xkbId;
    },

    commitString(string, fromKey) {
        if (string == null)
            return false;
        /* Let ibus methods fall through keyval emission */
        if (fromKey && this._currentSource.type == InputSourceManager.INPUT_SOURCE_TYPE_IBUS)
            return false;

        Main.inputMethod.commit(string);
        return true;
    },

    keyvalPress(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.PRESSED);
    },

    keyvalRelease(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, Clutter.KeyState.RELEASED);
    },
});
Signals.addSignalMethods(KeyboardController.prototype);
