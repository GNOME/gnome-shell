import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Graphene from 'gi://Graphene';
import IBus from 'gi://IBus';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Signals from '../misc/signals.js';

import * as InputSourceManager from './status/keyboard.js';
import * as IBusManager from '../misc/ibusManager.js';
import * as BoxPointer from './boxpointer.js';
import * as Main from './main.js';
import * as PageIndicators from './pageIndicators.js';
import * as PopupMenu from './popupMenu.js';
import * as SwipeTracker from './swipeTracker.js';

const KEYBOARD_ANIMATION_TIME = 150;
const KEYBOARD_REST_TIME = KEYBOARD_ANIMATION_TIME * 2;
const KEY_LONG_PRESS_TIME = 250;

const A11Y_APPLICATIONS_SCHEMA = 'org.gnome.desktop.a11y.applications';
const SHOW_KEYBOARD = 'screen-keyboard-enabled';
const EMOJI_PAGE_SEPARATION = 32;

/* KeyContainer puts keys in a grid where a 1:1 key takes this size */
const KEY_SIZE = 2;

const KEY_RELEASE_TIMEOUT = 50;
const BACKSPACE_WORD_DELETE_THRESHOLD = 50;

const AspectContainer = GObject.registerClass(
class AspectContainer extends St.Widget {
    _init(params) {
        super._init(params);
        this._ratio = 1;
    }

    setRatio(relWidth, relHeight) {
        this._ratio = relWidth / relHeight;
        this.queue_relayout();
    }

    vfunc_get_preferred_width(forHeight) {
        let [min, nat] = super.vfunc_get_preferred_width(forHeight);

        if (forHeight > 0)
            nat = forHeight * this._ratio;

        return [min, nat];
    }

    vfunc_get_preferred_height(forWidth) {
        let [min, nat] = super.vfunc_get_preferred_height(forWidth);

        if (forWidth > 0)
            nat = forWidth / this._ratio;

        return [min, nat];
    }

    vfunc_allocate(box) {
        if (box.get_width() > 0 && box.get_height() > 0) {
            let sizeRatio = box.get_width() / box.get_height();
            if (sizeRatio >= this._ratio) {
                /* Restrict horizontally */
                let width = box.get_height() * this._ratio;
                let diff = box.get_width() - width;

                box.x1 += Math.floor(diff / 2);
                box.x2 -= Math.ceil(diff / 2);
            }
        }

        super.vfunc_allocate(box);
    }
});

const KeyContainer = GObject.registerClass(
class KeyContainer extends St.Widget {
    _init() {
        const gridLayout = new Clutter.GridLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            column_homogeneous: true,
            row_homogeneous: true,
        });
        super._init({
            layout_manager: gridLayout,
            x_expand: true,
            y_expand: true,
        });
        this._gridLayout = gridLayout;
        this._nRows = 0;
        this._currentCol = 0;
        this._maxCols = 0;
    }

    appendRow() {
        this._nRows++;
        this._currentCol = 0;
    }

    appendKey(key, width = 1, height = 1, leftOffset = 0) {
        const left = this._currentCol + leftOffset;
        const top = this._nRows;
        this._gridLayout.attach(key,
            left * KEY_SIZE, top * KEY_SIZE,
            width * KEY_SIZE, height * KEY_SIZE);

        this._currentCol += leftOffset + width;
        this._maxCols = Math.max(this._currentCol, this._maxCols);
    }

    getRatio() {
        return [this._maxCols, this._nRows];
    }
});

const Suggestions = GObject.registerClass(
class Suggestions extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'word-suggestions',
            orientation: Clutter.Orientation.HORIZONTAL,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this.show();
    }

    add(word, callback) {
        let button = new St.Button({label: word});
        button.connect('button-press-event', () => {
            callback();
            return Clutter.EVENT_STOP;
        });
        button.connect('touch-event', (actor, event) => {
            if (event.type() !== Clutter.EventType.TOUCH_BEGIN)
                return Clutter.EVENT_PROPAGATE;

            callback();
            return Clutter.EVENT_STOP;
        });
        this.add_child(button);
    }

    clear() {
        this.remove_all_children();
    }

    setVisible(visible) {
        for (const child of this)
            child.visible = visible;
    }
});

class LanguageSelectionPopup extends PopupMenu.PopupMenu {
    constructor(actor) {
        super(actor, 0.5, St.Side.BOTTOM);

        let inputSourceManager = InputSourceManager.getInputSourceManager();
        let inputSources = inputSourceManager.inputSources;

        let item;
        for (let i in inputSources) {
            let is = inputSources[i];

            item = this.addAction(is.displayName, () => {
                inputSourceManager.activateInputSource(is, true);
            });
            item.can_focus = false;
            item.setOrnament(is === inputSourceManager.currentSource
                ? PopupMenu.Ornament.DOT
                : PopupMenu.Ornament.NO_DOT);
        }

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        item = this.addSettingsAction(_('Keyboard Settings'), 'gnome-keyboard-panel.desktop');
        item.can_focus = false;

        actor.connectObject('notify::mapped', () => {
            if (!actor.is_mapped())
                this.close(true);
        }, this);
    }

    _onCapturedEvent(actor, event) {
        const targetActor = global.stage.get_event_actor(event);

        if (targetActor === this.actor ||
            this.actor.contains(targetActor))
            return Clutter.EVENT_PROPAGATE;

        if (event.type() === Clutter.EventType.BUTTON_RELEASE || event.type() === Clutter.EventType.TOUCH_END)
            this.close(true);

        return Clutter.EVENT_STOP;
    }

    open(animate) {
        super.open(animate);
        global.stage.connectObject(
            'captured-event', this._onCapturedEvent.bind(this), this);
    }

    close(animate) {
        super.close(animate);
        global.stage.disconnectObject(this);
    }

    destroy() {
        global.stage.disconnectObject(this);
        this.sourceActor.disconnectObject(this);
        super.destroy();
    }
}

const Key = GObject.registerClass({
    Signals: {
        'long-press': {},
        'pressed': {},
        'released': {},
        'keyval': {param_types: [GObject.TYPE_UINT]},
        'commit': {param_types: [GObject.TYPE_STRING]},
    },
}, class Key extends St.BoxLayout {
    _init(params, extendedKeys = []) {
        const {label, iconName, commitString, keyval, hasAction} = {keyval: 0, ...params};
        super._init({style_class: 'key-container'});

        this._keyval = parseInt(keyval, 16);
        this.keyButton = this._makeKey(commitString, label, iconName);

        /* Add the key in a container, so keys can be padded without losing
         * logical proportions between those.
         */
        this.add_child(this.keyButton);
        this.connect('destroy', this._onDestroy.bind(this));

        this._extendedKeys = extendedKeys;
        this._extendedKeyboard = null;
        this._pressTimeoutId = 0;
        this._capturedPress = false;
        this._hasAction = hasAction;
    }

    get iconName() {
        return this._icon.icon_name;
    }

    set iconName(value) {
        this._icon.icon_name = value;
    }

    _onDestroy() {
        if (this._boxPointer) {
            this._boxPointer.destroy();
            this._boxPointer = null;
        }

        this.cancel();
    }

    _ensureExtendedKeysPopup() {
        if (this._extendedKeys.length === 0)
            return;

        if (this._boxPointer)
            return;

        this._boxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM);
        this._boxPointer.hide();
        Main.layoutManager.addTopChrome(this._boxPointer);
        this._boxPointer.setPosition(this.keyButton, 0.5);

        // Adds style to existing keyboard style to avoid repetition
        this._boxPointer.add_style_class_name('keyboard-subkeys-boxpointer');
        this._getExtendedKeys();
        this.keyButton._extendedKeys = this._extendedKeyboard;
    }

    _press(button) {
        if (button === this.keyButton) {
            this._pressTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                KEY_LONG_PRESS_TIME,
                () => {
                    this._pressTimeoutId = 0;

                    this.emit('long-press');

                    if (this._extendedKeys.length > 0) {
                        this._touchPressSlot = null;
                        this._ensureExtendedKeysPopup();
                        this.keyButton.set_hover(false);
                        this.keyButton.fake_release();
                        this._showSubkeys();
                    }

                    return GLib.SOURCE_REMOVE;
                });
        }

        this.emit('pressed');
        this._pressed = true;
    }

    _release(button, commitString) {
        if (this._pressTimeoutId !== 0) {
            GLib.source_remove(this._pressTimeoutId);
            this._pressTimeoutId = 0;
        }

        if (this._pressed) {
            if (this._keyval && button === this.keyButton)
                this.emit('keyval', this._keyval);
            else if (commitString)
                this.emit('commit', commitString);
            else if (!this._hasAction)
                console.error('Need keyval, commitString or an action');
        }

        this.emit('released');
        this._hideSubkeys();
        this._pressed = false;
    }

    cancel() {
        if (this._pressTimeoutId !== 0) {
            GLib.source_remove(this._pressTimeoutId);
            this._pressTimeoutId = 0;
        }
        this._touchPressSlot = null;
        this.keyButton.set_hover(false);
        this.keyButton.fake_release();
    }

    _onCapturedEvent(actor, event) {
        let type = event.type();
        let press = type === Clutter.EventType.BUTTON_PRESS || type === Clutter.EventType.TOUCH_BEGIN;
        let release = type === Clutter.EventType.BUTTON_RELEASE || type === Clutter.EventType.TOUCH_END;
        const targetActor = global.stage.get_event_actor(event);

        if (targetActor === this._boxPointer.bin ||
            this._boxPointer.bin.contains(targetActor))
            return Clutter.EVENT_PROPAGATE;

        if (press)
            this._capturedPress = true;
        else if (release && this._capturedPress)
            this._hideSubkeys();

        return Clutter.EVENT_STOP;
    }

    _showSubkeys() {
        this._boxPointer.open(BoxPointer.PopupAnimation.FULL);
        global.stage.connectObject(
            'captured-event', this._onCapturedEvent.bind(this), this);
        this.keyButton.connectObject('notify::mapped', () => {
            if (!this.keyButton.is_mapped())
                this._hideSubkeys();
        }, this);
    }

    _hideSubkeys() {
        if (this._boxPointer)
            this._boxPointer.close(BoxPointer.PopupAnimation.FULL);
        global.stage.disconnectObject(this);
        this.keyButton.disconnectObject(this);
        this._capturedPress = false;
    }

    _makeKey(commitString, label, icon) {
        let button = new St.Button({
            style_class: 'keyboard-key',
            x_expand: true,
        });

        if (icon) {
            const child = new St.Icon({icon_name: icon});
            button.set_child(child);
            this._icon = child;
        } else if (label) {
            button.set_label(label);
        } else if (commitString) {
            button.set_label(commitString);
        }

        button.connect('button-press-event', () => {
            this._press(button, commitString);
            button.add_style_pseudo_class('active');
            return Clutter.EVENT_STOP;
        });
        button.connect('button-release-event', () => {
            this._release(button, commitString);
            button.remove_style_pseudo_class('active');
            return Clutter.EVENT_STOP;
        });
        button.connect('touch-event', (actor, event) => {
            // We only handle touch events here on wayland. On X11
            // we do get emulated pointer events, which already works
            // for single-touch cases. Besides, the X11 passive touch grab
            // set up by Mutter will make us see first the touch events
            // and later the pointer events, so it will look like two
            // unrelated series of events, we want to avoid double handling
            // in these cases.
            if (!Meta.is_wayland_compositor())
                return Clutter.EVENT_PROPAGATE;

            const slot = event.get_event_sequence().get_slot();

            if (!this._touchPressSlot &&
                event.type() === Clutter.EventType.TOUCH_BEGIN) {
                this._touchPressSlot = slot;
                this._press(button, commitString);
                button.add_style_pseudo_class('active');
            } else if (event.type() === Clutter.EventType.TOUCH_END) {
                if (!this._touchPressSlot ||
                    this._touchPressSlot === slot) {
                    this._release(button, commitString);
                    button.remove_style_pseudo_class('active');
                }

                if (this._touchPressSlot === slot)
                    this._touchPressSlot = null;
            }
            return Clutter.EVENT_STOP;
        });

        return button;
    }

    _getExtendedKeys() {
        this._extendedKeyboard = new St.BoxLayout({
            style_class: 'key-container',
            orientation: Clutter.Orientation.HORIZONTAL,
        });
        for (let i = 0; i < this._extendedKeys.length; ++i) {
            let extendedKey = this._extendedKeys[i];
            let key = this._makeKey(extendedKey);

            key.extendedKey = extendedKey;
            this._extendedKeyboard.add_child(key);

            key.set_size(...this.keyButton.allocation.get_size());
            this.keyButton.connect('notify::allocation',
                () => key.set_size(...this.keyButton.allocation.get_size()));
        }
        this._boxPointer.bin.add_child(this._extendedKeyboard);
    }

    get subkeys() {
        return this._boxPointer;
    }

    setLatched(latched) {
        if (latched)
            this.keyButton.add_style_pseudo_class('latched');
        else
            this.keyButton.remove_style_pseudo_class('latched');
    }
});

class KeyboardModel {
    constructor(groupName) {
        this._model = this._loadModel(groupName);
    }

    _loadModel(groupName) {
        const file = Gio.File.new_for_uri(
            `resource:///org/gnome/shell/osk-layouts/${groupName}.json`);
        let [success_, contents] = file.load_contents(null);

        const decoder = new TextDecoder();
        return JSON.parse(decoder.decode(contents));
    }

    getLevels() {
        return this._model.levels;
    }

    getKeysForLevel(levelName) {
        return this._model.levels.find(level => level === levelName);
    }
}

class FocusTracker extends Signals.EventEmitter {
    constructor() {
        super();

        this._rect = null;

        global.display.connectObject(
            'notify::focus-window', () => {
                this._setCurrentWindow(global.display.focus_window);
                this.emit('window-changed', this._currentWindow);
            },
            'grab-op-begin', (display, window, op) => {
                if (window === this._currentWindow &&
                    (op === Meta.GrabOp.MOVING || op === Meta.GrabOp.KEYBOARD_MOVING))
                    this.emit('window-grabbed');
            }, this);

        this._setCurrentWindow(global.display.focus_window);

        /* Valid for wayland clients */
        Main.inputMethod.connectObject('cursor-location-changed',
            (o, rect) => this._setCurrentRect(rect), this);

        this._ibusManager = IBusManager.getIBusManager();
        this._ibusManager.connectObject(
            'set-cursor-location', (manager, rect) => {
                /* Valid for X11 clients only */
                if (Main.inputMethod.currentFocus)
                    return;

                const grapheneRect = new Graphene.Rect();
                grapheneRect.init(rect.x, rect.y, rect.width, rect.height);

                this._setCurrentRect(grapheneRect);
            },
            'focus-in', () => this.emit('focus-changed', true),
            'focus-out', () => this.emit('focus-changed', false),
            this);
    }

    destroy() {
        this._currentWindow?.disconnectObject(this);
        global.display.disconnectObject(this);
        Main.inputMethod.disconnectObject(this);
        this._ibusManager.disconnectObject(this);
    }

    get currentWindow() {
        return this._currentWindow;
    }

    _setCurrentWindow(window) {
        this._currentWindow?.disconnectObject(this);

        this._currentWindow = window;

        if (this._currentWindow) {
            this._currentWindow.connectObject(
                'position-changed', () => this.emit('window-moved'), this);
        }
    }

    _setCurrentRect(rect) {
        // Some clients give us 0-sized rects, in that case set size to 1
        if (rect.size.width <= 0)
            rect.size.width = 1;
        if (rect.size.height <= 0)
            rect.size.height = 1;

        if (this._currentWindow) {
            const frameRect = this._currentWindow.get_frame_rect();
            const grapheneFrameRect = new Graphene.Rect();
            grapheneFrameRect.init(frameRect.x, frameRect.y,
                frameRect.width, frameRect.height);

            const rectInsideFrameRect = grapheneFrameRect.intersection(rect)[0];
            if (!rectInsideFrameRect)
                return;
        }

        if (this._rect && this._rect.equal(rect))
            return;

        this._rect = rect;
        this.emit('position-changed');
    }

    getCurrentRect() {
        const rect = {
            x: this._rect.origin.x,
            y: this._rect.origin.y,
            width: this._rect.size.width,
            height: this._rect.size.height,
        };

        return rect;
    }
}

const EmojiPager = GObject.registerClass({
    Properties: {
        'delta': GObject.ParamSpec.int(
            'delta', null, null,
            GObject.ParamFlags.READWRITE,
            GLib.MININT32, GLib.MAXINT32, 0),
    },
    Signals: {
        'emoji': {param_types: [GObject.TYPE_STRING]},
        'page-changed': {
            param_types: [GObject.TYPE_INT, GObject.TYPE_INT, GObject.TYPE_INT],
        },
    },
}, class EmojiPager extends St.Widget {
    _init(sections) {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            reactive: true,
            clip_to_allocation: true,
            y_expand: true,
        });
        this._sections = sections;

        this._pages = [];
        this._panel = null;
        this._curPage = null;
        this._followingPage = null;
        this._followingPanel = null;
        this._currentKey = null;
        this._delta = 0;
        this._width = null;

        const swipeTracker = new SwipeTracker.SwipeTracker(this,
            Clutter.Orientation.HORIZONTAL,
            Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
            {
                allowDrag: true,
                allowScroll: true,
                name: 'EmojiPager swipe tracker',
            });
        swipeTracker.connect('begin', this._onSwipeBegin.bind(this));
        swipeTracker.connect('update', this._onSwipeUpdate.bind(this));
        swipeTracker.connect('end', this._onSwipeEnd.bind(this));
        this._swipeTracker = swipeTracker;

        this.connect('destroy', () => this._onDestroy());

        this.bind_property(
            'visible', this._swipeTracker, 'enabled',
            GObject.BindingFlags.DEFAULT);
    }

    _onDestroy() {
        if (this._swipeTracker) {
            this._swipeTracker.destroy();
            delete this._swipeTracker;
        }
    }

    get delta() {
        return this._delta;
    }

    set delta(value) {
        if (this._delta === value)
            return;

        this._delta = value;
        this.notify('delta');

        let followingPage = this.getFollowingPage();

        if (this._followingPage !== followingPage) {
            if (this._followingPanel) {
                this._followingPanel.destroy();
                this._followingPanel = null;
            }

            if (followingPage != null) {
                this._followingPanel = this._generatePanel(followingPage);
                this.add_child(this._followingPanel);
            }

            this._followingPage = followingPage;
        }

        const multiplier = this.text_direction === Clutter.TextDirection.RTL
            ? -1 : 1;

        this._panel.translation_x = value * multiplier;
        if (this._followingPanel) {
            const translation = value < 0
                ? this._width + EMOJI_PAGE_SEPARATION
                : -this._width - EMOJI_PAGE_SEPARATION;

            this._followingPanel.translation_x =
                (value * multiplier) + (translation * multiplier);
        }
    }

    _prevPage(nPage) {
        return (nPage + this._pages.length - 1) % this._pages.length;
    }

    _nextPage(nPage) {
        return (nPage + 1) % this._pages.length;
    }

    getFollowingPage() {
        if (this.delta === 0)
            return null;

        if (this.delta < 0)
            return this._nextPage(this._curPage);
        else
            return this._prevPage(this._curPage);
    }

    _onSwipeUpdate(tracker, progress) {
        this.delta = -progress * this._width;

        if (this._currentKey != null) {
            this._currentKey.cancel();
            this._currentKey = null;
        }

        return false;
    }

    _onSwipeBegin(tracker) {
        this._width = this.width;
        const points = [-1, 0, 1];
        tracker.confirmSwipe(this._width, points, 0, 0);
    }

    _onSwipeEnd(tracker, duration, endProgress) {
        this.remove_all_transitions();
        if (endProgress === 0) {
            this.ease_property('delta', 0, {duration});
        } else {
            const value = endProgress < 0
                ? this._width + EMOJI_PAGE_SEPARATION
                : -this._width - EMOJI_PAGE_SEPARATION;
            this.ease_property('delta', value, {
                duration,
                onComplete: () => {
                    this.setCurrentPage(this.getFollowingPage());
                },
            });
        }
    }

    _initPagingInfo() {
        this._pages = [];

        for (let i = 0; i < this._sections.length; i++) {
            let section = this._sections[i];
            let itemsPerPage = this._nCols * this._nRows;
            let nPages = Math.ceil(section.keys.length / itemsPerPage);
            let page = -1;
            let pageKeys;

            for (let j = 0; j < section.keys.length; j++) {
                if (j % itemsPerPage === 0) {
                    page++;
                    pageKeys = [];
                    this._pages.push({pageKeys, nPages, page, section: this._sections[i]});
                }

                pageKeys.push(section.keys[j]);
            }
        }
    }

    _lookupSection(section, nPage) {
        for (let i = 0; i < this._pages.length; i++) {
            let page = this._pages[i];

            if (page.section === section && page.page === nPage)
                return i;
        }

        return -1;
    }

    _generatePanel(nPage) {
        const gridLayout = new Clutter.GridLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            column_homogeneous: true,
            row_homogeneous: true,
        });
        const panel = new St.Widget({
            layout_manager: gridLayout,
            style_class: 'emoji-page',
            x_expand: true,
            y_expand: true,
        });

        /* Set an expander actor so all proportions are right despite the panel
         * not having all rows/cols filled in.
         */
        let expander = new Clutter.Actor();
        gridLayout.attach(expander, 0, 0, this._nCols, this._nRows);

        let page = this._pages[nPage];
        let col = 0;
        let row = 0;

        for (let i = 0; i < page.pageKeys.length; i++) {
            let modelKey = page.pageKeys[i];
            let key = new Key({commitString: modelKey.label}, modelKey.variants);

            key.keyButton.set_button_mask(0);

            key.connect('pressed', () => {
                this._currentKey = key;
            });
            key.connect('commit', (actor, str) => {
                if (this._currentKey !== key)
                    return;
                this._currentKey = null;
                this.emit('emoji', str);
            });

            gridLayout.attach(key, col, row, 1, 1);

            col++;
            if (col >= this._nCols) {
                col = 0;
                row++;
            }
        }

        return panel;
    }

    setCurrentPage(nPage) {
        if (this._curPage === nPage)
            return;

        this._curPage = nPage;

        if (this._panel) {
            this._panel.destroy();
            this._panel = null;
        }

        /* Reuse followingPage if possible */
        if (nPage === this._followingPage) {
            this._panel = this._followingPanel;
            this._followingPanel = null;
        }

        if (this._followingPanel)
            this._followingPanel.destroy();

        this._followingPanel = null;
        this._followingPage = null;
        this._delta = 0;

        if (!this._panel) {
            this._panel = this._generatePanel(nPage);
            this.add_child(this._panel);
        }

        let page = this._pages[nPage];
        this.emit('page-changed', page.section.label, page.page, page.nPages);
    }

    setCurrentSection(section, nPage) {
        for (let i = 0; i < this._pages.length; i++) {
            let page = this._pages[i];

            if (page.section === section && page.page === nPage) {
                this.setCurrentPage(i);
                break;
            }
        }
    }

    setRatio(nCols, nRows) {
        this._nCols = nCols;
        this._nRows = nRows;
        this._initPagingInfo();
    }
});

const EmojiSelection = GObject.registerClass({
    Signals: {
        'emoji-selected': {param_types: [GObject.TYPE_STRING]},
        'close-request': {},
        'toggle': {},
    },
}, class EmojiSelection extends St.Widget {
    _init() {
        const gridLayout = new Clutter.GridLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            column_homogeneous: true,
            row_homogeneous: true,
        });
        super._init({
            layout_manager: gridLayout,
            style_class: 'emoji-panel',
            x_expand: true,
            y_expand: true,
            text_direction: global.stage.text_direction,
        });

        this._sections = [
            {first: 'grinning face', label: '🙂️'},
            {first: 'selfie', label: '👍️'},
            {first: 'monkey face', label: '🌷️'},
            {first: 'grapes', label: '🍴️'},
            {first: 'globe showing Europe-Africa', label: '✈️'},
            {first: 'jack-o-lantern', label: '🏃️'},
            {first: 'muted speaker', label: '🔔️'},
            {first: 'ATM sign', label: '❤️'},
            {first: 'chequered flag', label: '🚩️'},
        ];

        this._gridLayout = gridLayout;
        this._populateSections();

        this._pagerBox = new Clutter.Actor({
            layout_manager: new Clutter.BoxLayout({
                orientation: Clutter.Orientation.VERTICAL,
            }),
        });

        this._emojiPager = new EmojiPager(this._sections);
        this._emojiPager.connect('page-changed', (pager, sectionLabel, page, nPages) => {
            this._onPageChanged(sectionLabel, page, nPages);
        });
        this._emojiPager.connect('emoji', (pager, str) => {
            this.emit('emoji-selected', str);
        });
        this._pagerBox.add_child(this._emojiPager);

        this._pageIndicator = new PageIndicators.PageIndicators(
            Clutter.Orientation.HORIZONTAL);
        this._pageIndicator.y_expand = false;
        this._pageIndicator.y_align = Clutter.ActorAlign.START;
        this._pagerBox.add_child(this._pageIndicator);
        this._pageIndicator.setReactive(false);

        this._emojiPager.connect('notify::delta', () => {
            this._updateIndicatorPosition();
        });

        this._bottomRow = this._createBottomRow();

        this._curPage = 0;
    }

    vfunc_map() {
        this._emojiPager.setCurrentPage(0);
        super.vfunc_map();
    }

    _onPageChanged(sectionLabel, page, nPages) {
        this._curPage = page;
        this._pageIndicator.setNPages(nPages);
        this._updateIndicatorPosition();

        for (let i = 0; i < this._sections.length; i++) {
            let sect = this._sections[i];
            sect.button.setLatched(sectionLabel === sect.label);
        }
    }

    _updateIndicatorPosition() {
        this._pageIndicator.setCurrentPosition(this._curPage -
            this._emojiPager.delta / this._emojiPager.width);
    }

    _findSection(emoji) {
        for (let i = 0; i < this._sections.length; i++) {
            if (this._sections[i].first === emoji)
                return this._sections[i];
        }

        return null;
    }

    _populateSections() {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/osk-layouts/emoji.json');
        let [success_, contents] = file.load_contents(null);

        let emoji = JSON.parse(new TextDecoder().decode(contents));

        let variants = [];
        let currentKey = 0;
        let currentSection = null;

        for (let i = 0; i < emoji.length; i++) {
            /* Group variants of a same emoji so they appear on the key popover */
            if (emoji[i].name.startsWith(emoji[currentKey].name)) {
                variants.push(emoji[i].char);
                if (i < emoji.length - 1)
                    continue;
            }

            let newSection = this._findSection(emoji[currentKey].name);
            if (newSection != null) {
                currentSection = newSection;
                currentSection.keys = [];
            }

            /* Create the key */
            let label = emoji[currentKey].char + String.fromCharCode(0xFE0F);
            currentSection.keys.push({label, variants});
            currentKey = i;
            variants = [];
        }
    }

    _createBottomRow() {
        let row = new KeyContainer();
        let key;

        row.appendRow();

        key = new Key({label: 'ABC', hasAction: true}, []);
        key.keyButton.add_style_class_name('default-key');
        key.connect('released', () => this.emit('toggle'));
        row.appendKey(key, 1.5);

        for (let i = 0; i < this._sections.length; i++) {
            let section = this._sections[i];

            key = new Key({label: section.label}, []);
            key.connect('released', () => this._emojiPager.setCurrentSection(section, 0));
            row.appendKey(key);

            section.button = key;
        }

        key = new Key({iconName: 'osk-hide-symbolic', hasAction: true});
        key.keyButton.add_style_class_name('default-key');
        key.keyButton.add_style_class_name('hide-key');
        key.connect('released', () => {
            this.emit('close-request');
        });
        row.appendKey(key);

        const actor = new AspectContainer({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        actor.add_child(row);

        return actor;
    }

    setRatio(nCols, nRows) {
        this._emojiPager.setRatio(Math.floor(nCols), Math.floor(nRows) - 1);
        this._bottomRow.setRatio(nCols, 1);

        // (Re)attach actors so the emoji panel fits the ratio and
        // the bottom row is ensured to take 1 row high.
        if (this._pagerBox.get_parent())
            this.remove_child(this._pagerBox);
        if (this._bottomRow.get_parent())
            this.remove_child(this._bottomRow);

        this._gridLayout.attach(this._pagerBox, 0, 0, 1, Math.floor(nRows) - 1);
        this._gridLayout.attach(this._bottomRow, 0, Math.floor(nRows) - 1, 1, 1);
    }
});

export class KeyboardManager extends Signals.EventEmitter {
    constructor() {
        super();

        this._keyboard = null;
        this._a11yApplicationsSettings = new Gio.Settings({schema_id: A11Y_APPLICATIONS_SCHEMA});
        this._a11yApplicationsSettings.connect('changed', this._syncEnabled.bind(this));

        this._seat = global.stage.context.get_backend().get_default_seat();
        this._seat.connect('notify::touch-mode', this._syncEnabled.bind(this));

        this._lastDevice = null;
        global.backend.connect('last-device-changed', (backend, device) => {
            if (device.device_type === Clutter.InputDeviceType.KEYBOARD_DEVICE)
                return;

            this._lastDevice = device;
            this._syncEnabled();
        });

        const allowedModes = Shell.ActionMode.ALL & ~Shell.ActionMode.LOCK_SCREEN;
        const bottomDragGesture = new Shell.EdgeDragGesture({
            name: 'OSK show bottom drag',
            side: St.Side.BOTTOM,
        });
        bottomDragGesture.connect('may-recognize', () => {
            return allowedModes & Main.actionMode;
        });
        bottomDragGesture.connect('progress', (_action, progress) => {
            this._keyboard?.gestureProgress(progress);
        });
        bottomDragGesture.connect('end', () => {
            this._keyboard?.gestureActivate(Main.layoutManager.bottomIndex);
        });
        bottomDragGesture.connect('cancel', () => {
            this._keyboard?.gestureCancel();
        });
        global.stage.add_action(bottomDragGesture);
        this._bottomDragGesture = bottomDragGesture;

        this._syncEnabled();
    }

    _lastDeviceIsTouchscreen() {
        if (!this._lastDevice)
            return false;

        let deviceType = this._lastDevice.get_device_type();
        return deviceType === Clutter.InputDeviceType.TOUCHSCREEN_DEVICE;
    }

    _syncEnabled() {
        let enableKeyboard = this._a11yApplicationsSettings.get_boolean(SHOW_KEYBOARD);
        let autoEnabled = this._seat.get_touch_mode() && this._lastDeviceIsTouchscreen();
        let enabled = enableKeyboard || autoEnabled;

        if (!enabled && !this._keyboard)
            return;

        if (enabled && !this._keyboard) {
            this._keyboard = new Keyboard();
            this._keyboard.connect('visibility-changed', () => {
                this.emit('visibility-changed');
                this._bottomDragGesture.enabled = !this._keyboard.visible;
            });
        } else if (!enabled && this._keyboard) {
            this._keyboard.setCursorLocation(null);
            this._keyboard.destroy();
            this._keyboard = null;
            this._bottomDragGesture.enabled = true;
        }
    }

    get keyboardActor() {
        return this._keyboard;
    }

    get visible() {
        return this._keyboard && this._keyboard.visible;
    }

    open(monitor) {
        Main.layoutManager.keyboardIndex = monitor;

        if (this._keyboard)
            this._keyboard.open();
    }

    close() {
        if (this._keyboard)
            this._keyboard.close();
    }

    addSuggestion(text, callback) {
        if (this._keyboard)
            this._keyboard.addSuggestion(text, callback);
    }

    resetSuggestions() {
        if (this._keyboard)
            this._keyboard.resetSuggestions();
    }

    setSuggestionsVisible(visible) {
        this._keyboard?.setSuggestionsVisible(visible);
    }

    maybeHandleEvent(event) {
        if (!this._keyboard)
            return false;

        const actor = global.stage.get_event_actor(event);

        if (Main.layoutManager.keyboardBox.contains(actor) ||
            !!actor._extendedKeys || !!actor.extendedKey) {
            actor.event(event, true);
            actor.event(event, false);
            return true;
        }

        return false;
    }
}

export const Keyboard = GObject.registerClass({
    Signals: {
        'visibility-changed': {},
    },
}, class Keyboard extends St.BoxLayout {
    _init() {
        super._init({
            name: 'keyboard',
            reactive: true,
            // Keyboard models are defined in LTR, we must override
            // the locale setting in order to avoid flipping the
            // keyboard on RTL locales.
            text_direction: Clutter.TextDirection.LTR,
            orientation: Clutter.Orientation.VERTICAL,
        });
        this._focusInExtendedKeys = false;
        this._emojiActive = false;

        this._languagePopup = null;
        this._focusWindow = null;
        this._focusWindowStartY = null;

        this._latched = false; // current level is latched
        this._modifiers = new Set();
        this._modifierKeys = new Map();

        this._suggestions = null;

        this._focusTracker = new FocusTracker();
        this._focusTracker.connectObject(
            'position-changed', this._onFocusPositionChanged.bind(this),
            'window-grabbed', this._onFocusWindowMoving.bind(this), this);

        this._windowMovedId = this._focusTracker.connect('window-moved',
            this._onFocusWindowMoving.bind(this));

        // Valid only for X11
        if (!Meta.is_wayland_compositor()) {
            this._focusTracker.connectObject('focus-changed', (_tracker, focused) => {
                if (focused)
                    this.open(Main.layoutManager.focusIndex);
                else
                    this.close();
            }, this);
        }

        this._showIdleId = 0;

        this._keyboardVisible = false;
        this._keyboardRequested = false;
        this._keyboardRestingId = 0;

        Main.layoutManager.connectObject('monitors-changed',
            this._relayout.bind(this), this);

        this._setupKeyboard();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    get visible() {
        return this._keyboardVisible && super.visible;
    }

    set visible(visible) {
        super.visible = visible;
    }

    _onFocusPositionChanged(focusTracker) {
        let rect = focusTracker.getCurrentRect();
        this.setCursorLocation(focusTracker.currentWindow, rect.x, rect.y, rect.width, rect.height);
        this._updateLevelFromHints(true);
    }

    _onDestroy() {
        if (this._windowMovedId) {
            this._focusTracker.disconnect(this._windowMovedId);
            delete this._windowMovedId;
        }

        if (this._focusTracker) {
            this._focusTracker.destroy();
            delete this._focusTracker;
        }

        this._clearShowIdle();

        this._keyboardController.setOskCompletion(false);
        this._keyboardController.destroy();

        Main.layoutManager.untrackChrome(this);
        Main.layoutManager.keyboardBox.remove_child(this);
        Main.layoutManager.keyboardBox.hide();

        if (this._languagePopup) {
            this._languagePopup.destroy();
            this._languagePopup = null;
        }
    }

    _setupKeyboard() {
        Main.layoutManager.keyboardBox.add_child(this);
        Main.layoutManager.trackChrome(this);

        this._keyboardController = new KeyboardController();

        this._currentPage = null;

        this._suggestions = new Suggestions();
        this.add_child(this._suggestions);

        this._aspectContainer = new AspectContainer({
            layout_manager: new Clutter.BinLayout(),
            y_expand: true,
        });
        this.add_child(this._aspectContainer);

        this._emojiSelection = new EmojiSelection();
        this._emojiSelection.connect('toggle', this._toggleEmoji.bind(this));
        this._emojiSelection.connect('close-request', () => this.close(true));
        this._emojiSelection.connect('emoji-selected', (selection, emoji) => {
            this._keyboardController.commit(emoji).catch(console.error);
        });

        this._emojiSelection.hide();
        this._aspectContainer.add_child(this._emojiSelection);

        this._updateKeys();

        this._keyboardController.connectObject(
            'group-changed', this._onGroupChanged.bind(this),
            'panel-state', this._onKeyboardStateChanged.bind(this),
            'purpose-changed', () => this._updateKeys(),
            'content-hints-changed', this._onContentHintsChanged.bind(this),
            this);
        global.stage.connectObject('notify::key-focus',
            this._onKeyFocusChanged.bind(this), this);

        this._relayout();
    }

    _onContentHintsChanged(controller, contentHint) {
        this._contentHint = contentHint;
        this._updateLevelFromHints(false);
    }

    _updateLevelFromHints(userInputHappened) {
        // If the latch is enabled, avoid level changes
        if (this._latched)
            return;

        if ((this._contentHint & Clutter.InputContentHintFlags.LOWERCASE) !== 0) {
            this._setActiveLevel('default');
            return;
        }

        if (!this._layers['shift'])
            return;

        if ((this._contentHint & Clutter.InputContentHintFlags.UPPERCASE) !== 0) {
            this._setActiveLevel('shift');
            return;
        }

        if ((this._contentHint &
             (Clutter.InputContentHintFlags.AUTO_CAPITALIZATION |
              Clutter.InputContentHintFlags.TITLECASE)) !== 0) {
            if (this._surroundingTextId)
                return;

            this._surroundingTextId =
                Main.inputMethod.connect('surrounding-text-set', () => {
                    const [text, cursor] = Main.inputMethod.getSurroundingText();
                    if (!text || cursor === 0) {
                        // First character in the buffer
                        this._setActiveLevel('shift');
                        return;
                    }

                    const beforeCursor = GLib.utf8_substring(text, 0, cursor);

                    if ((this._contentHint & Clutter.InputContentHintFlags.TITLECASE) !== 0) {
                        if (beforeCursor.charAt(beforeCursor.length - 1) === ' ')
                            this._setActiveLevel('shift');
                        else
                            this._setActiveLevel('default');
                    } else if ((this._contentHint & Clutter.InputContentHintFlags.AUTO_CAPITALIZATION) !== 0) {
                        if (beforeCursor.charAt(beforeCursor.trimEnd().length - 1) === '.')
                            this._setActiveLevel('shift');
                        else
                            this._setActiveLevel('default');
                    }

                    Main.inputMethod.disconnect(this._surroundingTextId);
                    this._surroundingTextId = 0;
                });
            Main.inputMethod.request_surrounding();
            return;
        }

        if (userInputHappened && this._currentPage === this._layers['shift'])
            this._setActiveLevel('default');
    }

    _onKeyFocusChanged() {
        let focus = global.stage.key_focus;

        // Showing an extended key popup and clicking a key from the extended keys
        // will grab focus, but ignore that
        let extendedKeysWereFocused = this._focusInExtendedKeys;
        this._focusInExtendedKeys = focus && (focus._extendedKeys || focus.extendedKey);
        if (this._focusInExtendedKeys || extendedKeysWereFocused)
            return;

        if (!(focus instanceof Clutter.Text)) {
            this.close();
            return;
        }

        if (!this._showIdleId) {
            this._showIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
                this.open(Main.layoutManager.focusIndex);
                this._showIdleId = 0;
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(this._showIdleId, '[gnome-shell] this.open');
        }
    }

    _updateLayout(groupName, purpose) {
        let keyboardModel = null;
        let layers = {};
        let layout = new Clutter.Actor({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });

        if (purpose === Clutter.InputContentPurpose.DIGITS) {
            keyboardModel = new KeyboardModel('digits');
        } else if (purpose === Clutter.InputContentPurpose.NUMBER) {
            keyboardModel = new KeyboardModel('number');
        } else if (purpose === Clutter.InputContentPurpose.PHONE) {
            keyboardModel = new KeyboardModel('phone');
        } else if (purpose === Clutter.InputContentPurpose.EMAIL) {
            keyboardModel = new KeyboardModel('email');
        } else if (purpose === Clutter.InputContentPurpose.URL) {
            keyboardModel = new KeyboardModel('url');
        } else {
            let groups = [groupName];
            if (groupName.includes('+'))
                groups.push(groupName.replace(/\+.*/, ''));
            groups.push('us');

            if (purpose === Clutter.InputContentPurpose.TERMINAL)
                groups = groups.map(g => `${g}-extended`);

            for (const group of groups) {
                try {
                    keyboardModel = new KeyboardModel(group);
                    break;
                } catch {
                    // Ignore this error and fall back to next model
                }
            }

            if (!keyboardModel)
                return;
        }

        const emojiVisible = Meta.is_wayland_compositor() &&
            (purpose === Clutter.InputContentPurpose.NORMAL ||
             purpose === Clutter.InputContentPurpose.ALPHA ||
             purpose === Clutter.InputContentPurpose.PASSWORD ||
             purpose === Clutter.InputContentPurpose.TERMINAL);

        keyboardModel.getLevels().forEach(currentLevel => {
            let levelLayout = new KeyContainer();
            levelLayout.shiftKeys = [];
            levelLayout.mode = currentLevel.mode;

            const rows = currentLevel.rows;
            rows.forEach(row => {
                levelLayout.appendRow();
                this._addRowKeys(row, levelLayout, emojiVisible);
            });

            layers[currentLevel.level] = levelLayout;
            layout.add_child(levelLayout);
            levelLayout.hide();
        });

        this._aspectContainer.add_child(layout);
        this._currentLayout?.destroy();
        this._currentLayout = layout;
        this._layers = layers;
    }

    _addRowKeys(keys, layout, emojiVisible) {
        let accumulatedWidth = 0;
        for (let i = 0; i < keys.length; ++i) {
            const key = keys[i];
            const {strings} = key;
            const commitString = strings?.shift();

            if (key.action === 'emoji' && !emojiVisible) {
                accumulatedWidth = key.width ?? 1;
                continue;
            }

            if (accumulatedWidth > 0) {
                // Pass accumulated width onto the next key
                key.width = (key.width ?? 1) + accumulatedWidth;
                accumulatedWidth = 0;
            }

            let button = new Key({
                commitString,
                label: key.label,
                iconName: key.iconName,
                keyval: key.keyval,
                hasAction: !!key.action,
            }, strings);

            if (key.action) {
                button.connect('released', () => {
                    if (key.action === 'hide') {
                        this.close(true);
                        this._updateLevelFromHints(true);
                    } else if (key.action === 'languageMenu') {
                        this._popupLanguageMenu(button);
                    } else if (key.action === 'emoji') {
                        this._toggleEmoji();
                    } else if (key.action === 'modifier') {
                        this._toggleModifier(key.keyval);
                    } else if (key.action === 'delete') {
                        this._keyboardController.toggleDelete(true);
                        this._keyboardController.toggleDelete(false);
                        this._updateLevelFromHints(true);
                    } else if (!this._longPressed && key.action === 'levelSwitch') {
                        this._setActiveLevel(key.level);
                        this._setLatched(
                            key.level === 1 &&
                                key.iconName === 'osk-caps-lock-symbolic');
                    }

                    this._longPressed = false;
                });
            } else if (key.keyval) {
                button.connect('keyval', (_actor, keyval) => {
                    this._keyboardController.keyvalPress(keyval);
                    this._keyboardController.keyvalRelease(keyval);
                    this._updateLevelFromHints(true);
                });
            } else {
                button.connect('commit', (_actor, str) => {
                    this._keyboardController.commit(str, this._modifiers).then(() => {
                        this._disableAllModifiers();
                        this._updateLevelFromHints(true);
                    }).catch(console.error);
                });
            }

            if (key.action === 'levelSwitch' &&
                key.iconName === 'osk-shift-symbolic') {
                layout.shiftKeys.push(button);
                if (key.level === 'shift') {
                    button.connect('long-press', () => {
                        this._setActiveLevel(key.level);
                        this._setLatched(true);
                        this._longPressed = true;
                    });
                }
            }

            if (key.action === 'delete') {
                button.connect('long-press',
                    () => this._keyboardController.toggleDelete(true));
            }

            if (key.action === 'modifier') {
                let modifierKeys = this._modifierKeys[key.keyval] || [];
                modifierKeys.push(button);
                this._modifierKeys[key.keyval] = modifierKeys;
            }

            if (key.action || key.keyval)
                button.keyButton.add_style_class_name('default-key');

            layout.appendKey(button, key.width, key.height, key.leftOffset);
        }
    }

    _setLatched(latched) {
        this._latched = latched;
        this._setCurrentLevelLatched(this._currentPage, this._latched);
    }

    _setModifierEnabled(keyval, enabled) {
        if (enabled)
            this._modifiers.add(keyval);
        else
            this._modifiers.delete(keyval);

        for (const key of this._modifierKeys[keyval])
            key.setLatched(enabled);
    }

    _toggleModifier(keyval) {
        const isActive = this._modifiers.has(keyval);
        this._setModifierEnabled(keyval, !isActive);
    }

    _disableAllModifiers() {
        for (const keyval of this._modifiers)
            this._setModifierEnabled(keyval, false);
    }

    _popupLanguageMenu(keyActor) {
        if (this._languagePopup)
            this._languagePopup.destroy();

        this._languagePopup = new LanguageSelectionPopup(keyActor);
        Main.layoutManager.addTopChrome(this._languagePopup.actor);
        this._languagePopup.open(true);
    }

    _updateCurrentPageVisible() {
        if (this._currentPage)
            this._currentPage.visible = !this._emojiActive;
    }

    _setEmojiActive(active) {
        this._emojiActive = active;
        this._emojiSelection.visible = this._emojiActive;
        this._updateCurrentPageVisible();
    }

    _toggleEmoji() {
        this._setEmojiActive(!this._emojiActive);
    }

    _setCurrentLevelLatched(layout, latched) {
        for (let i = 0; i < layout.shiftKeys.length; i++) {
            let key = layout.shiftKeys[i];
            key.setLatched(latched);
            key.iconName = latched
                ? 'osk-caps-lock-symbolic' : 'osk-shift-symbolic';
        }
    }

    _getGridSlots() {
        let numOfHorizSlots = 0, numOfVertSlots;
        let rows = this._currentPage.get_children();
        numOfVertSlots = rows.length;

        for (let i = 0; i < rows.length; ++i) {
            let keyboardRow = rows[i];
            let keys = keyboardRow.get_children();

            numOfHorizSlots = Math.max(numOfHorizSlots, keys.length);
        }

        return [numOfHorizSlots, numOfVertSlots];
    }

    _relayout() {
        let monitor = Main.layoutManager.keyboardMonitor;

        if (!monitor)
            return;

        this.width = monitor.width;

        if (monitor.width > monitor.height)
            this.height = monitor.height / 3;
        else
            this.height = monitor.height / 4;
    }

    _updateKeys() {
        const group = this._keyboardController.getCurrentGroup();
        const {purpose} = this._keyboardController;
        this._updateLayout(group, purpose);
        this._setActiveLevel('default');
    }

    _onGroupChanged() {
        this._updateKeys();
    }

    _onKeyboardStateChanged(controller, state) {
        let enabled;
        if (state === Clutter.InputPanelState.OFF)
            enabled = false;
        else if (state === Clutter.InputPanelState.ON)
            enabled = true;
        else if (state === Clutter.InputPanelState.TOGGLE)
            enabled = this._keyboardVisible === false;
        else
            return;

        if (enabled)
            this.open(Main.layoutManager.focusIndex);
        else
            this.close(true);
    }

    _setActiveLevel(activeLevel) {
        const layers = this._layers;
        let currentPage = layers[activeLevel];

        if (this._currentPage === currentPage) {
            this._updateCurrentPageVisible();
            return;
        }

        if (this._currentPage != null) {
            this._setCurrentLevelLatched(this._currentPage, false);
            this._currentPage.disconnect(this._currentPage._destroyID);
            this._currentPage.hide();
            delete this._currentPage._destroyID;
        }

        this._disableAllModifiers();
        this._currentPage = currentPage;
        this._currentPage._destroyID = this._currentPage.connect('destroy', () => {
            this._currentPage = null;
        });
        this._updateCurrentPageVisible();
        this._aspectContainer.setRatio(...this._currentPage.getRatio());
        this._emojiSelection.setRatio(...this._currentPage.getRatio());
    }

    _clearKeyboardRestTimer() {
        if (!this._keyboardRestingId)
            return;
        GLib.source_remove(this._keyboardRestingId);
        this._keyboardRestingId = 0;
    }

    open(immediate = false) {
        this._clearShowIdle();
        this._keyboardRequested = true;

        if (this._keyboardVisible) {
            this._relayout();
            return;
        }

        this._keyboardController.setOskCompletion(true);
        this._clearKeyboardRestTimer();

        if (immediate) {
            this._open();
            return;
        }

        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            KEYBOARD_REST_TIME,
            () => {
                this._clearKeyboardRestTimer();
                this._open();
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    }

    _open() {
        if (!this._keyboardRequested)
            return;

        this._relayout();
        this._animateShow();

        this._setEmojiActive(false);
    }

    close(immediate = false) {
        this._clearShowIdle();
        this._keyboardRequested = false;

        if (!this._keyboardVisible)
            return;

        this._keyboardController.setOskCompletion(false);
        this._clearKeyboardRestTimer();

        if (immediate) {
            this._close();
            return;
        }

        this._keyboardRestingId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            KEYBOARD_REST_TIME,
            () => {
                this._clearKeyboardRestTimer();
                this._close();
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._keyboardRestingId, '[gnome-shell] this._clearKeyboardRestTimer');
    }

    _close() {
        if (this._keyboardRequested)
            return;

        this._animateHide();
        this.setCursorLocation(null);
        this._disableAllModifiers();
    }

    _animateShow() {
        global.compositor.disable_unredirect();

        if (this._focusWindow)
            this._animateWindow(this._focusWindow, true);

        Main.layoutManager.keyboardBox.show();
        this.ease({
            translation_y: -this.height,
            opacity: 255,
            duration: KEYBOARD_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._animateShowComplete();
            },
        });
        this._keyboardVisible = true;
        this.emit('visibility-changed');
    }

    _animateShowComplete() {
        let keyboardBox = Main.layoutManager.keyboardBox;
        this._keyboardHeightNotifyId = keyboardBox.connect('notify::height', () => {
            this.translation_y = -this.height;
        });

        // Toggle visibility so the keyboardBox can update its chrome region.
        if (!Meta.is_wayland_compositor()) {
            keyboardBox.hide();
            keyboardBox.show();
        }
    }

    _animateHide() {
        if (this._focusWindow)
            this._animateWindow(this._focusWindow, false);

        if (this._keyboardHeightNotifyId) {
            Main.layoutManager.keyboardBox.disconnect(this._keyboardHeightNotifyId);
            this._keyboardHeightNotifyId = 0;
        }
        this.ease({
            translation_y: 0,
            opacity: 0,
            duration: KEYBOARD_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
            onComplete: () => {
                this._animateHideComplete();
            },
        });

        this._keyboardVisible = false;
        this.emit('visibility-changed');
    }

    _animateHideComplete() {
        Main.layoutManager.keyboardBox.hide();
        global.compositor.enable_unredirect();
    }

    gestureProgress(delta) {
        this._gestureInProgress = true;
        Main.layoutManager.keyboardBox.show();
        let progress = Math.min(delta, this.height) / this.height;
        this.translation_y = -this.height * progress;
        this.opacity = 255 * progress;
        const windowActor = this._focusWindow?.get_compositor_private();
        if (windowActor)
            windowActor.y = this._focusWindowStartY - (this.height * progress);
    }

    gestureActivate() {
        this.open(true);
        this._gestureInProgress = false;
    }

    gestureCancel() {
        if (this._gestureInProgress)
            this._animateHide();
        this._gestureInProgress = false;
    }

    resetSuggestions() {
        if (this._suggestions)
            this._suggestions.clear();
    }

    setSuggestionsVisible(visible) {
        this._suggestions?.setVisible(visible);
    }

    addSuggestion(text, callback) {
        if (!this._suggestions)
            return;
        this._suggestions.add(text, callback);
        this._suggestions.show();
    }

    _clearShowIdle() {
        if (!this._showIdleId)
            return;
        GLib.source_remove(this._showIdleId);
        this._showIdleId = 0;
    }

    _windowSlideAnimationComplete(window, finalY) {
        // Synchronize window positions again.
        const frameRect = window.get_frame_rect();
        const bufferRect = window.get_buffer_rect();

        finalY += frameRect.y - bufferRect.y;

        frameRect.y = finalY;

        this._focusTracker.disconnect(this._windowMovedId);
        window.move_frame(true, frameRect.x, frameRect.y);
        this._windowMovedId = this._focusTracker.connect('window-moved',
            this._onFocusWindowMoving.bind(this));
    }

    _animateWindow(window, show) {
        let windowActor = window.get_compositor_private();
        if (!windowActor)
            return;

        const finalY = show
            ? this._focusWindowStartY - Main.layoutManager.keyboardBox.height
            : this._focusWindowStartY;

        windowActor.ease({
            y: finalY,
            duration: KEYBOARD_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => {
                windowActor.y = finalY;
                this._windowSlideAnimationComplete(window, finalY);
            },
        });
    }

    _onFocusWindowMoving() {
        if (this._focusTracker.currentWindow === this._focusWindow) {
            // Don't use _setFocusWindow() here because that would move the
            // window while the user has grabbed it. Instead we simply "let go"
            // of the window.
            this._focusWindow = null;
            this._focusWindowStartY = null;
        }

        this.close(true);
    }

    _setFocusWindow(window) {
        if (this._focusWindow === window)
            return;

        if (this._keyboardVisible && this._focusWindow)
            this._animateWindow(this._focusWindow, false);

        const windowActor = window?.get_compositor_private();
        windowActor?.remove_transition('y');
        this._focusWindowStartY = windowActor ? windowActor.y : null;

        if (this._keyboardVisible && window)
            this._animateWindow(window, true);

        this._focusWindow = window;
    }

    setCursorLocation(window, x, y, w, h) {
        let monitor = Main.layoutManager.keyboardMonitor;

        if (window && monitor) {
            const keyboardHeight = Main.layoutManager.keyboardBox.height;
            const keyboardY1 = (monitor.y + monitor.height) - keyboardHeight;

            if (this._focusWindow === window) {
                if (y + h + keyboardHeight < keyboardY1)
                    this._setFocusWindow(null);

                return;
            }

            if (y + h >= keyboardY1)
                this._setFocusWindow(window);
            else
                this._setFocusWindow(null);
        } else {
            this._setFocusWindow(null);
        }
    }
});

class KeyboardController extends Signals.EventEmitter {
    constructor() {
        super();

        let seat = global.stage.context.get_backend().get_default_seat();
        this._virtualDevice = seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

        this._inputSourceManager = InputSourceManager.getInputSourceManager();
        this._inputSourceManager.connectObject(
            'current-source-changed', this._onSourceChanged.bind(this),
            'sources-changed', this._onSourcesModified.bind(this), this);
        this._currentSource = this._inputSourceManager.currentSource;
        this._purpose = Main.inputMethod.contentPurpose;

        Main.inputMethod.connectObject(
            'notify::content-purpose', this._onPurposeHintsChanged.bind(this),
            'notify::content-hints', this._onContentHintsChanged.bind(this),
            'input-panel-state', (o, state) => this.emit('panel-state', state), this);
    }

    get purpose() {
        return this._purpose;
    }

    destroy() {
        this._inputSourceManager.disconnectObject(this);
        Main.inputMethod.disconnectObject(this);

        // Make sure any buttons pressed by the virtual device are released
        // immediately instead of waiting for the next GC cycle
        this._virtualDevice.run_dispose();
    }

    _onSourcesModified() {
        this.emit('group-changed');
    }

    _onSourceChanged(inputSourceManager, _oldSource) {
        let source = inputSourceManager.currentSource;
        this._currentSource = source;
        this.emit('group-changed');
    }

    _onPurposeHintsChanged(method) {
        const purpose = method.content_purpose;
        this._purpose = purpose;
        this.emit('purpose-changed', purpose);
    }

    _onContentHintsChanged(method) {
        const contentHints = method.content_hints;
        this._contentHints = contentHints;
        this.emit('content-hints-changed', contentHints);
    }

    getCurrentGroup() {
        // Special case for Korean, if Hangul mode is disabled, use the 'us' keymap
        if (this._currentSource.id === 'hangul') {
            const inputSourceManager = InputSourceManager.getInputSourceManager();
            const currentSource = inputSourceManager.currentSource;
            let prop;
            for (let i = 0; (prop = currentSource.properties.get(i)) !== null; ++i) {
                if (prop.get_key() === 'InputMode' &&
                    prop.get_prop_type() === IBus.PropType.TOGGLE &&
                    prop.get_state() !== IBus.PropState.CHECKED)
                    return 'us';
            }
        }

        return this._currentSource.xkbId;
    }

    _forwardModifiers(modifiers, type) {
        for (const keyval of modifiers) {
            if (type === Clutter.EventType.KEY_PRESS)
                this.keyvalPress(keyval);
            else if (type === Clutter.EventType.KEY_RELEASE)
                this.keyvalRelease(keyval);
        }
    }

    _getKeyvalsFromString(string) {
        const keyvals = [];
        for (const unicode of string) {
            const keyval = Clutter.unicode_to_keysym(unicode.codePointAt(0));
            // If the unicode character is unknown, try to avoid keyvals at all
            if (keyval === (unicode || 0x01000000))
                return [];

            keyvals.push(keyval);
        }

        return keyvals;
    }

    async commit(str, modifiers) {
        const keyvals = this._getKeyvalsFromString(str);

        // If there is no IM focus (e.g. with X11 clients), or modifiers
        // are in use, send raw key events.
        if (!Main.inputMethod.currentFocus || modifiers?.size > 0) {
            if (modifiers)
                this._forwardModifiers(modifiers, Clutter.EventType.KEY_PRESS);

            for (const keyval of keyvals) {
                this.keyvalPress(keyval);
                this.keyvalRelease(keyval);
            }

            if (modifiers)
                this._forwardModifiers(modifiers, Clutter.EventType.KEY_RELEASE);

            return;
        }

        // If OSK completion is enabled, or there is an active source requiring
        // IBus to receive input, prefer to feed the events directly to the IM
        if (this._oskCompletionEnabled ||
            this._currentSource.type === InputSourceManager.INPUT_SOURCE_TYPE_IBUS) {
            for (const keyval of keyvals) {
                // eslint-disable-next-line no-await-in-loop
                if (!await Main.inputMethod.handleVirtualKey(keyval)) {
                    this.keyvalPress(keyval);
                    this.keyvalRelease(keyval);
                }
            }
            return;
        }

        Main.inputMethod.commit(str);
    }

    async setOskCompletion(enabled) {
        if (this._oskCompletionEnabled === enabled)
            return;

        this._oskCompletionEnabled =
            await IBusManager.getIBusManager().setCompletionEnabled(enabled);

        Main.inputMethod.update();
    }

    keyvalPress(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time() * 1000,
            keyval, Clutter.KeyState.PRESSED);
    }

    keyvalRelease(keyval) {
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time() * 1000,
            keyval, Clutter.KeyState.RELEASED);
    }

    _previousWordPosition(text, cursor) {
        const upToCursor = [...text].slice(0, cursor).join('');
        const jsStringPos = Math.max(0, upToCursor.search(/\s+\S+\s*$/));
        const charPos = GLib.utf8_strlen(text.slice(0, jsStringPos), -1);
        return charPos;
    }

    toggleDelete(enabled) {
        if (this._deleteEnabled === enabled)
            return;

        this._deleteEnabled = enabled;
        this._timesDeleted = 0;

        /* If there is no IM focus or are in the middle of preedit, fallback to
         * keypresses */
        if (enabled &&
            (!Main.inputMethod.currentFocus ||
             Main.inputMethod.hasPreedit() ||
             this._purpose === Clutter.InputContentPurpose.TERMINAL)) {
            this.keyvalPress(Clutter.KEY_BackSpace);
            this._backspacePressed = true;
            return;
        }

        if (!enabled && this._backspacePressed) {
            this.keyvalRelease(Clutter.KEY_BackSpace);
            delete this._backspacePressed;
            return;
        }

        if (enabled) {
            let func = (text, cursor, anchor) => {
                if (cursor === 0 && anchor === 0)
                    return;

                let offset, len;
                if (cursor > anchor) {
                    offset = anchor - cursor;
                    len = -offset;
                } else if (cursor < anchor) {
                    offset = 0;
                    len = anchor - cursor;
                } else if (this._timesDeleted < BACKSPACE_WORD_DELETE_THRESHOLD) {
                    offset = -1;
                    len = 1;
                } else {
                    const wordLength = cursor - this._previousWordPosition(text, cursor);
                    offset = -wordLength;
                    len = wordLength;
                }

                this._timesDeleted++;
                Main.inputMethod.delete_surrounding(offset, len);
            };

            this._surroundingUpdateId = Main.inputMethod.connect(
                'surrounding-text-set', () => {
                    let [text, cursor, anchor] = Main.inputMethod.getSurroundingText();
                    if (this._timesDeleted === 0) {
                        func(text, cursor, anchor);
                    } else {
                        if (this._surroundingUpdateTimeoutId > 0) {
                            GLib.source_remove(this._surroundingUpdateTimeoutId);
                            this._surroundingUpdateTimeoutId = 0;
                        }
                        this._surroundingUpdateTimeoutId =
                            GLib.timeout_add(GLib.PRIORITY_DEFAULT, KEY_RELEASE_TIMEOUT, () => {
                                func(text, cursor, cursor);
                                this._surroundingUpdateTimeoutId = 0;
                                return GLib.SOURCE_REMOVE;
                            });
                    }
                });

            let [text, cursor, anchor] = Main.inputMethod.getSurroundingText();
            if (text)
                func(text, cursor, anchor);
            else
                Main.inputMethod.request_surrounding();
        } else {
            if (this._surroundingUpdateId > 0) {
                Main.inputMethod.disconnect(this._surroundingUpdateId);
                this._surroundingUpdateId = 0;
            }
            if (this._surroundingUpdateTimeoutId > 0) {
                GLib.source_remove(this._surroundingUpdateTimeoutId);
                this._surroundingUpdateTimeoutId = 0;
            }
        }
    }
}
