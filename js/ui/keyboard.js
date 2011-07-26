/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Caribou = imports.gi.Caribou;
const Clutter = imports.gi.Clutter;
const DBus = imports.dbus;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PopupMenu = imports.ui.popupMenu;
const Tweener = imports.ui.tweener;

const KEYBOARD_SCHEMA = 'org.gnome.shell.keyboard';
const SHOW_KEYBOARD = 'show-keyboard';
const KEYBOARD_TYPE = 'keyboard-type';
const ENABLE_DRAGGABLE = 'enable-drag';
const ENABLE_FLOAT = 'enable-float';

// Key constants taken from Antler
const PRETTY_KEYS = {
    'BackSpace': '\u232b',
    'space': ' ',
    'Return': '\u23ce',
    'Caribou_Prefs': '\u2328',
    'Caribou_ShiftUp': '\u2b06',
    'Caribou_ShiftDown': '\u2b07',
    'Caribou_Emoticons': '\u263a',
    'Caribou_Symbols': '123',
    'Caribou_Symbols_More': '{#*',
    'Caribou_Alpha': 'Abc',
    'Tab': 'Tab',
    'Escape': 'Esc',
    'Control_L': 'Ctrl',
    'Alt_L': 'Alt'
};

const CaribouKeyboardIface = {
    name: 'org.gnome.Caribou.Keyboard',
    methods:    [ { name: 'Show',
                    inSignature: '',
                    outSignature: ''
                  },
                  { name: 'Hide',
                    inSignature: '',
                    outSignature: ''
                  },
                  { name: 'SetCursorLocation',
                    inSignature: 'iiii',
                    outSignature: ''
                  },
                  { name: 'SetEntryLocation',
                    inSignature: 'iiii',
                    outSignature: ''
                  } ],
    properties: [ { name: 'Name',
                    signature: 's',
                    access: 'read' } ]
};

function Key() {
    this._init.apply(this, arguments);
}

Key.prototype = {
    _init : function(key, key_width, key_height) {
        this._key = key;

        this._width = key_width;
        this._height = key_height;

        this.actor = this._getKey();

        this._extended_keys = this._key.get_extended_keys();
        this._extended_keyboard = {};

        if (this._key.name == "Control_L" || this._key.name == "Alt_L")
            this._key.latch = true;

        this._key.connect('key-pressed', Lang.bind(this, function ()
                                                   { this.actor.checked = true }));
        this._key.connect('key-released', Lang.bind(this, function ()
                                                    { this.actor.checked = false; }));

        if (this._extended_keys.length > 0) {
            this._grabbed = false;
            this._eventCaptureId = 0;
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
            Main.chrome.addActor(this._boxPointer.actor, { visibleInFullscreen: true,
                                                           affectsStruts: false });
        }
    },

    _getKey: function () {
        let label = this._key.name;

        if (label.length > 1) {
            let pretty = PRETTY_KEYS[label];
            if (pretty)
                label = pretty;
            else
                label = this._getUnichar(this._key);
        }

        label = GLib.markup_escape_text(label, -1);
        let button = new St.Button ({ label: label, style_class: 'keyboard-key' });

        button.width = this._width;
        button.key_width = this._key.width;
        button.height = this._height;
        button.draggable = false;
        button.connect('button-press-event', Lang.bind(this, function () { this._key.press(); }));
        button.connect('button-release-event', Lang.bind(this, function () { this._key.release(); }));

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
            let key = new St.Button({ label: label, style_class: 'keyboard-key' });
            key.extended_key = extended_key;
            key.width = this._width;
            key.height = this._height;
            key.draggable = false;
            key.connect('button-press-event', Lang.bind(this, function () { extended_key.press(); }));
            key.connect('button-release-event', Lang.bind(this, function () { extended_key.release(); }));
            this._extended_keyboard.add(key);
        }
        this._boxPointer.bin.add_actor(this._extended_keyboard);
    },

    _onEventCapture: function (actor, event) {
        let source = event.get_source();
        if (event.type() == Clutter.EventType.BUTTON_PRESS ||
            (event.type() == Clutter.EventType.BUTTON_RELEASE && source.draggable)) {
            if (this._extended_keyboard.contains(source)) {
                if (source.draggable) {
                    source.extended_key.press();
                    source.extended_key.release();
                }
                this._ungrab();
                return false;
            }
            this._boxPointer.actor.hide();
            this._ungrab();
            return true;
        }
        return false;
    },

    _ungrab: function () {
        global.stage.disconnect(this._eventCaptureId);
        this._eventCaptureId = 0;
        this._grabbed = false;
        Main.popModal(this.actor);
    },

    _onShowSubkeysChanged: function () {
        if (this._key.show_subkeys) {
            this.actor.fake_release();
            this._boxPointer.actor.raise_top();
            this._boxPointer.setPosition(this.actor, 5, 0.5);
            this._boxPointer.show(true);
            this.actor.set_hover(false);
            if (!this._grabbed) {
                 Main.pushModal(this.actor);
                 this._eventCaptureId = global.stage.connect('captured-event', Lang.bind(this, this._onEventCapture));
                 this._grabbed = true;
            }
            this._key.release();
        } else {
            if (this._grabbed)
                this._ungrab();
            this._boxPointer.hide(true);
        }
    }
};

function Keyboard() {
    this._init.apply(this, arguments);
}

Keyboard.prototype = {
    _init: function () {
        DBus.session.exportObject('/org/gnome/Caribou/Keyboard', this);
        DBus.session.acquire_name('org.gnome.Caribou.Keyboard', 0, null, null);

        this.actor = new St.BoxLayout({ name: 'keyboard', vertical: true, reactive: true });

        this._keyboardSettings = new Gio.Settings({ schema: KEYBOARD_SCHEMA });
        this._keyboardSettings.connect('changed', Lang.bind(this, this._display));

        this._setupKeyboard();

        Main.layoutManager.connect('monitors-changed', Lang.bind(this, this._redraw));

        Main.layoutManager.bottomBox.add_actor(this.actor);
    },

    init: function () {
        this._display();
    },

    _setupKeyboard: function() {
        if (this._keyboardNotifyId)
            this._keyboard.disconnect(this._keyboardNotifyId);
        let children = this.actor.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].destroy();

        this._keyboard = new Caribou.KeyboardModel({ keyboard_type: this._keyboardSettings.get_string(KEYBOARD_TYPE) });
        this._groups = {};
        this._current_page = null;

        // Initialize keyboard key measurements
        this._numOfHorizKeys = 0;
        this._numOfVertKeys = 0;

        this._floatId = 0;

        this._addKeys();

        this._keyboardNotifyId = this._keyboard.connect('notify::active-group', Lang.bind(this, this._onGroupChanged));
    },

    _display: function () {
        if (this._keyboard.keyboard_type != this._keyboardSettings.get_string(KEYBOARD_TYPE))
            this._setupKeyboard();

        this._showKeyboard = this._keyboardSettings.get_boolean(SHOW_KEYBOARD);
        this._draggable = this._keyboardSettings.get_boolean(ENABLE_DRAGGABLE);
        this._floating = this._keyboardSettings.get_boolean(ENABLE_FLOAT);
        if (this._floating) {
             this._floatId = this.actor.connect('button-press-event', Lang.bind(this, this._startDragging));
             this._dragging = false;
        }
        else
            this.actor.disconnect(this._floatId);
        if (this._showKeyboard)
            this.show();
        else {
            this.hide();
            this.destroySource();
        }
    },

    _startDragging: function (actor, event) {
        if (this._dragging) // don't allow two drags at the same time
            return;
        this._dragging = true;
        this._preDragStageMode = global.stage_input_mode;

        Clutter.grab_pointer(this.actor);
        global.set_stage_input_mode(Shell.StageInputMode.FULLSCREEN);

        this._releaseId = this.actor.connect('button-release-event', Lang.bind(this, this._endDragging));
        this._motionId = this.actor.connect('motion-event', Lang.bind(this, this._motionEvent));
        [this._dragStartX, this._dragStartY] = event.get_coords();
        [this._currentX, this._currentY] = this.actor.get_position();
    },

    _endDragging: function () {
        if (this._dragging) {
            this.actor.disconnect(this._releaseId);
            this.actor.disconnect(this._motionId);

            Clutter.ungrab_pointer();
            global.set_stage_input_mode(this._preDragStageMode);
            global.unset_cursor();
            this._dragging = false;
        }
        return true;
    },

    _motionEvent: function(actor, event) {
        let absX, absY;
        [absX, absY] = event.get_coords();
        global.set_cursor(Shell.Cursor.DND_IN_DRAG);
        this._moveHandle(absX, absY);
        return true;
    },

    _moveHandle: function (stageX, stageY) {
        let x, y;
        x = stageX - this._dragStartX + this._currentX;
        y = stageY - this._dragStartY + this._currentY;
        this.actor.set_position(x,y);

    },

    _addKeys: function () {
        let groups = this._keyboard.get_groups();
        for (let i = 0; i < groups.length; ++i) {
             let gname = groups[i];
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
             this._groups[gname] = layers;
        }

        this._setActiveLayer();
    },

    _getTrayIcon: function () {
        let trayButton = new St.Button ({ label: "tray", style_class: 'keyboard-key' });
        trayButton.key_width = 1;
        trayButton.connect('button-press-event', Lang.bind(this, function () {
            Main.layoutManager.updateForTray();
        }));

        Main.overview.connect('showing', Lang.bind(this, function () {
            trayButton.reactive = false;
            trayButton.add_style_pseudo_class('grayed');
        }));
        Main.overview.connect('hiding', Lang.bind(this, function () {
            trayButton.reactive = true;
            trayButton.remove_style_pseudo_class('grayed');
        }));

        return trayButton;
    },

    _addRows : function (keys, layout) {
        let keyboard_row = new St.BoxLayout();
        for (let i = 0; i < keys.length; ++i) {
            let children = keys[i].get_children();
            let right_box = new St.BoxLayout({ style_class: 'keyboard-row' });
            let left_box = new St.BoxLayout({ style_class: 'keyboard-row' });
            for (let j = 0; j < children.length; ++j) {
                if (this._numOfHorizKeys == 0)
                    this._numOfHorizKeys = children.length;
                let key = children[j];
                let button = new Key(key, 0, 0);

                if (key.align == 'right')
                    right_box.add(button.actor);
                else
                    left_box.add(button.actor);
                if (key.name == "Caribou_Prefs") {
                    key.connect('key-released', Lang.bind(this, this._onPrefsClick));

                    // Add new key for hiding message tray
                    right_box.add(this._getTrayIcon());
                }
            }
            keyboard_row.add(left_box, { expand: true, x_fill: false, x_align: St.Align.START });
            keyboard_row.add(right_box, { expand: true, x_fill: false, x_align: St.Align.END });
        }
        layout.add(keyboard_row);
    },

    _manageTray: function () {
        this.createSource();
    },

    _onPrefsClick: function () {
        this.hide();
        this._manageTray();
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
        let monitor = Main.layoutManager.bottomMonitor;
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
                    child.draggable = this._draggable;
                    if (child._extended_keys) {
                        let extended_keys = child._extended_keys.get_children();
                        for (let n = 0; n < extended_keys.length; ++n) {
                            let extended_key = extended_keys[n];
                            extended_key.width = keySize;
                            extended_key.height = keySize;
                            extended_key.draggable = this._draggable;
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

    createSource: function () {
        if (this._source == null) {
            this._source = new KeyboardSource(this);
            Main.messageTray.add(this._source);
        }
    },

    destroySource: function () {
        if (this._source) {
            this._source.destroy();
            this._source = null;
        }
    },

    show: function () {
        this._redraw();
        Main.layoutManager.showKeyboard();
    },

    hide: function () {
        Main.layoutManager.hideKeyboard();
    },

    // Window placement method
    _updatePosition: function (x, y) {
        let primary = Main.layoutManager.primaryMonitor;
        x -= this.actor.width / 2;
        // Determines bottom/top centered
        if (y <= primary.height / 2)
            y += this.actor.height / 2;
        else
            y -= 3 * this.actor.height / 2;

        // Accounting for monitor boundaries
        if (x < primary.x)
            x = primary.x;
        if (x + this.actor.width > primary.width)
            x = primary.width - this.actor.width;

        this.actor.set_position(x, y);
    },

    _moveTemporarily: function () {
        this._currentWindow = global.screen.get_display().focus_window;
        let rect = this._currentWindow.get_outer_rect();
        this._currentWindow.x = rect.x;
        this._currentWindow.y = rect.y;

        let newX = this._currentWindow.x;
        let newY = 3 * this.actor.height / 2;
        this._currentWindow.move_frame(true, newX, newY);
    },

    _setLocation: function (x, y) {
        if (this._floating)
            this._updatePosition(x, y);
        else {
            if (y >= 2 * this.actor.height)
                this._moveTemporarily();
        }
    },

    // D-Bus methods
    Show: function() {
        this.destroySource();
        this.show();
    },

    Hide: function() {
        if (this._currentWindow) {
            this._currentWindow.move_frame(true, this._currentWindow.x, this._currentWindow.y);
            this._currentWindow = null;
        }
        this.hide();
        this._manageTray();
    },

    SetCursorLocation: function(x, y, w, h) {
        this._setLocation(x, y);
    },

    SetEntryLocation: function(x, y, w, h) {
        this._setLocation(x, y);
    },

    get Name() {
        return 'gnome-shell';
    }
};
DBus.conformExport(Keyboard.prototype, CaribouKeyboardIface);

function KeyboardSource() {
    this._init.apply(this, arguments);
}

KeyboardSource.prototype = {
    __proto__: MessageTray.Source.prototype,

    _init: function(keyboard) {
        this._keyboard = keyboard;
        MessageTray.Source.prototype._init.call(this, _("Keyboard"));

        this._setSummaryIcon(this.createNotificationIcon());
    },

    createNotificationIcon: function() {
        return new St.Icon({ icon_name: 'input-keyboard',
                             icon_type: St.IconType.SYMBOLIC,
                             icon_size: this.ICON_SIZE });
    },

     handleSummaryClick: function() {
        let event = Clutter.get_current_event();
        if (event.type() != Clutter.EventType.BUTTON_RELEASE)
            return false;

        if (event.get_button() != 1)
            return false;

        this.open();
        return true;
    },

    open: function() {
        this._keyboard.show();
        this._keyboard.destroySource();
    }
};
