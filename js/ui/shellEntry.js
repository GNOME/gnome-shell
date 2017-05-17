// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported addContextMenu */

const { Clutter, Gio, GObject, Gtk, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;

const Util = imports.misc.util;

const SPINNER_ICON_SIZE = 24;
const SPINNER_MIN_DURATION = 1000;

const OVERVIEW_ENTRY_BLINK_DURATION = 400;
const OVERVIEW_ENTRY_BLINK_BRIGHTNESS = 1.4;

var EntryMenu = class extends PopupMenu.PopupMenu {
    constructor(entry) {
        super(entry, 0.025, St.Side.TOP);

        this._entry = entry;
        this._clipboard = St.Clipboard.get_default();

        // Populate menu
        let item;
        item = new PopupMenu.PopupMenuItem(_("Copy"));
        item.connect('activate', this._onCopyActivated.bind(this));
        this.addMenuItem(item);
        this._copyItem = item;

        item = new PopupMenu.PopupMenuItem(_("Paste"));
        item.connect('activate', this._onPasteActivated.bind(this));
        this.addMenuItem(item);
        this._pasteItem = item;

        this._passwordItem = null;

        Main.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }

    _makePasswordItem() {
        let item = new PopupMenu.PopupMenuItem('');
        item.connect('activate', this._onPasswordActivated.bind(this));
        this.addMenuItem(item);
        this._passwordItem = item;
    }

    get isPassword() {
        return this._passwordItem != null;
    }

    set isPassword(v) {
        if (v == this.isPassword)
            return;

        if (v) {
            this._makePasswordItem();
            this._entry.input_purpose = Clutter.InputContentPurpose.PASSWORD;
        } else {
            this._passwordItem.destroy();
            this._passwordItem = null;
            this._entry.input_purpose = Clutter.InputContentPurpose.NORMAL;
        }
    }

    open(animate) {
        this._updatePasteItem();
        this._updateCopyItem();
        if (this._passwordItem)
            this._updatePasswordItem();

        super.open(animate);
        this._entry.add_style_pseudo_class('focus');

        let direction = St.DirectionType.TAB_FORWARD;
        if (!this.actor.navigate_focus(null, direction, false))
            this.actor.grab_key_focus();
    }

    _updateCopyItem() {
        let selection = this._entry.clutter_text.get_selection();
        this._copyItem.setSensitive(!this._entry.clutter_text.password_char &&
                                    selection && selection != '');
    }

    _updatePasteItem() {
        this._clipboard.get_text(St.ClipboardType.CLIPBOARD,
            (clipboard, text) => {
                this._pasteItem.setSensitive(text && text != '');
            });
    }

    _updatePasswordItem() {
        let textHidden = (this._entry.clutter_text.password_char);
        if (textHidden)
            this._passwordItem.label.set_text(_("Show Text"));
        else
            this._passwordItem.label.set_text(_("Hide Text"));
    }

    _onCopyActivated() {
        let selection = this._entry.clutter_text.get_selection();
        this._clipboard.set_text(St.ClipboardType.CLIPBOARD, selection);
    }

    _onPasteActivated() {
        this._clipboard.get_text(St.ClipboardType.CLIPBOARD,
            (clipboard, text) => {
                if (!text)
                    return;
                this._entry.clutter_text.delete_selection();
                let pos = this._entry.clutter_text.get_cursor_position();
                this._entry.clutter_text.insert_text(text, pos);
            });
    }

    _onPasswordActivated() {
        let visible = !!(this._entry.clutter_text.password_char);
        this._entry.clutter_text.set_password_char(visible ? '' : '\u25cf');
    }
};

var OverviewEntry = GObject.registerClass({
    Signals: {
        'search-activated': {},
        'search-active-changed': {},
        'search-navigate-focus': { param_types: [GObject.TYPE_INT] },
        'search-terms-changed': {},
    },
    Properties: {
        'blinkBrightness': GObject.ParamSpec.double(
            'blinkBrightness',
            'blinkBrightness',
            'blinkBrightness',
            GObject.ParamFlags.READWRITE,
            0, 10, 0)
    }
}, class OverviewEntry extends St.Entry {
    _init() {
        this._active = false;

        this._capturedEventId = 0;

        let primaryIcon = new St.Icon({
            icon_name: 'edit-find-symbolic',
            style_class: 'search-icon',
            icon_size: 16,
            track_hover: true,
        });

        let iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working-dark.svg')
        this._spinnerAnimation = new Animation.AnimatedIcon(iconFile, SPINNER_ICON_SIZE);
        this._spinnerAnimation.actor.hide();

        // Set the search entry's text based on the current search engine
        let entryText;
        let searchEngine = Util.getSearchEngineName();

        if (searchEngine != null)
            entryText = _("Search %s and more…").format(searchEngine);
        else
            entryText = _("Search the internet and more…");

        let hintActor = new St.Label({ text: entryText,
                                       style_class: 'search-entry-text-hint' });

        super._init({
            name: 'searchEntry',
            track_hover: true,
            reactive: true,
            can_focus: true,
            hint_text: '',
            hint_actor: hintActor,
            primary_icon: primaryIcon,
            secondary_icon: this._spinnerAnimation.actor,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._blinkBrightnessEffect = new Clutter.BrightnessContrastEffect({
            enabled: false,
        });
        this.add_effect(this._blinkBrightnessEffect);

        addContextMenu(this);

        this.connect('primary-icon-clicked', () => {
            this.grab_key_focus();
        });
        this.connect('notify::mapped', this._onMapped.bind(this));
        this.clutter_text.connect('key-press-event', this._onKeyPress.bind(this));
        this.clutter_text.connect('text-changed', this._onTextChanged.bind(this));
        global.stage.connect('notify::key-focus', this._onStageKeyFocusChanged.bind(this));
    }

    _isActivated() {
        return !this.hint_actor.visible;
    }

    _getTermsForSearchString(searchString) {
        searchString = searchString.replace(/^\s+/g, '').replace(/\s+$/g, '');
        if (searchString == '')
            return [];

        let terms = searchString.split(/\s+/);
        return terms;
    }

    _onCapturedEvent(actor, event) {
        if (event.type() != Clutter.EventType.BUTTON_PRESS)
            return false;

        let source = event.get_source();
        if (source != this.clutter_text &&
            !Main.layoutManager.keyboardBox.contains(source)) {
            // If the user clicked outside after activating the entry,
            // drop the focus from the search bar, but avoid resetting
            // the entry state.
            // If no search terms entered were entered, also reset the
            // entry to its initial state.
            if (this.clutter_text.text == '')
                this.resetSearch();
            else
                this._stopSearch();
        }

        return false;
    }

    _onKeyPress(entry, event) {
        let symbol = event.get_key_symbol();
        if (symbol == Clutter.Escape && this._isActivated()) {
            this.resetSearch();
            return true;
        }

        if (!this.active)
            return false;

        let arrowNext, nextDirection;
        if (entry.get_text_direction() == Clutter.TextDirection.RTL) {
            arrowNext = Clutter.Left;
            nextDirection = Gtk.DirectionType.LEFT;
        } else {
            arrowNext = Clutter.Right;
            nextDirection = Gtk.DirectionType.RIGHT;
        }

        if (symbol == Clutter.Down)
            nextDirection = Gtk.DirectionType.DOWN;

        if ((symbol == arrowNext && this.clutter_text.position == -1) ||
            (symbol == Clutter.Down)) {
            this.emit('search-navigate-focus', nextDirection);
            return true;
        } else if (symbol == Clutter.Return || symbol == Clutter.KP_Enter) {
            this._activateSearch();
            return true;
        }

        return false;
    }

    _onMapped() {
        // The entry might get mapped because of the clone, so we also
        // have to check if the actual overview actor is visible.
        if (this.mapped && Main.layoutManager.overviewGroup.visible) {
            // Enable 'find-as-you-type'
            this._capturedEventId = global.stage.connect('captured-event',
                                                         this._onCapturedEvent.bind(this));

            this.clutter_text.set_cursor_visible(true);
            // Move the cursor at the end of the current text
            let buffer = this.clutter_text.get_buffer();
            let nChars = buffer.get_length();
            this.clutter_text.set_selection(nChars, nChars);
        } else {
            // Disable 'find-as-you-type'
            if (this._capturedEventId > 0) {
                global.stage.disconnect(this._capturedEventId);
                this._capturedEventId = 0;
            }
        }
    }

    _onStageKeyFocusChanged() {
        let focus = global.stage.get_key_focus();
        let appearFocused = this.contains(focus);

        this.clutter_text.set_cursor_visible(appearFocused);

        if (appearFocused)
            this.add_style_pseudo_class('focus');
        else
            this.remove_style_pseudo_class('focus');
    }

    _onTextChanged (se, prop) {
        this.emit('search-terms-changed');
        let terms = this._getTermsForSearchString(this.get_text());
        this.active = (terms.length > 0);
    }

    _searchCancelled() {
        // Leave the entry focused when it doesn't have any text;
        // when replacing a selected search term, Clutter emits
        // two 'text-changed' signals, one for deleting the previous
        // text and one for the new one - the second one is handled
        // incorrectly when we remove focus
        // (https://bugzilla.gnome.org/show_bug.cgi?id=636341) */
        if (this.clutter_text.text != '')
            this.resetSearch();
    }

    _shouldTriggerSearch(symbol) {
        let unicode = Clutter.keysym_to_unicode(symbol);
        if (unicode == 0)
            return symbol == Clutter.BackSpace && this.active;

        return this._getTermsForSearchString(String.fromCharCode(unicode)).length > 0;
    }

    _activateSearch() {
        this.emit('search-activated');
    }

    _stopSearch() {
        global.stage.set_key_focus(null);
    }

    _startSearch(event) {
        global.stage.set_key_focus(this.clutter_text);
        this.clutter_text.event(event, false);
    }

    resetSearch () {
        this._stopSearch();
        this.text = '';

        this.clutter_text.set_cursor_visible(true);
        this.clutter_text.set_selection(0, 0);
    }

    handleStageEvent(event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.Escape && this.active) {
            this.resetSearch();
            return true;
        }

        if (this._shouldTriggerSearch(symbol)) {
            this._startSearch(event);
            return true;
        }

        return false;
    }

    setSpinning(visible) {
        if (visible) {
            this._spinnerAnimation.play();
            this._spinnerAnimation.actor.show();
        } else {
            this._spinnerAnimation.stop();
            this._spinnerAnimation.actor.hide();
        }
    }

    get blinkBrightness() {
        return this._blinkBrightness;
    }

    set blinkBrightness(v) {
        this._blinkBrightness = v;
        this._blinkBrightnessEffect.enabled = this._blinkBrightness !== 1;
        let colorval = this._blinkBrightness * 127;
        this._blinkBrightnessEffect.brightness = new Clutter.Color({
            red: colorval,
            green: colorval,
            blue: colorval,
        });
    }

    blink() {
        this.blinkBrightness = 1;
        this.ease({
            blinkBrightness: OVERVIEW_ENTRY_BLINK_BRIGHTNESS,
            duration: OVERVIEW_ENTRY_BLINK_DURATION / 2,
            mode: Clutter.AnimationMode.EASE_IN_QUAD,
            onComplete: () => {
                this.ease({
                    blinkBrightness: 1,
                    duration: OVERVIEW_ENTRY_BLINK_DURATION / 2,
                    mode: Clutter.AnimationMode.EASE_IN_QUAD,
                });
            },
        });

    }

    set active(value) {
        if (value == this._active)
            return;

        this._active = value;
        this._ongoing = false;

        if (!this._active)
            this._searchCancelled();

        this.emit('search-active-changed');
    }

    get active() {
        return this._active;
    }

    getSearchTerms() {
        return this._getTermsForSearchString(this.get_text());
    }
});

function _setMenuAlignment(entry, stageX) {
    let [success, entryX] = entry.transform_stage_point(stageX, 0);
    if (success)
        entry.menu.setSourceAlignment(entryX / entry.width);
}

function _onButtonPressEvent(actor, event, entry) {
    if (entry.menu.isOpen) {
        entry.menu.close(BoxPointer.PopupAnimation.FULL);
        return Clutter.EVENT_STOP;
    } else if (event.get_button() == 3) {
        let [stageX] = event.get_coords();
        _setMenuAlignment(entry, stageX);
        entry.menu.open(BoxPointer.PopupAnimation.FULL);
        return Clutter.EVENT_STOP;
    }
    return Clutter.EVENT_PROPAGATE;
}

function _onPopup(actor, entry) {
    let [success, textX, textY_, lineHeight_] = entry.clutter_text.position_to_coords(-1);
    if (success)
        entry.menu.setSourceAlignment(textX / entry.width);
    entry.menu.open(BoxPointer.PopupAnimation.FULL);
}

function addContextMenu(entry, params) {
    if (entry.menu)
        return;

    params = Params.parse (params, { isPassword: false, actionMode: Shell.ActionMode.POPUP });

    entry.menu = new EntryMenu(entry);
    entry.menu.isPassword = params.isPassword;
    entry._menuManager = new PopupMenu.PopupMenuManager(entry,
                                                        { actionMode: params.actionMode });
    entry._menuManager.addMenu(entry.menu);

    // Add an event handler to both the entry and its clutter_text; the former
    // so padding is included in the clickable area, the latter because the
    // event processing of ClutterText prevents event-bubbling.
    entry.clutter_text.connect('button-press-event', (actor, event) => {
        _onButtonPressEvent(actor, event, entry);
    });
    entry.connect('button-press-event', (actor, event) => {
        _onButtonPressEvent(actor, event, entry);
    });

    entry.connect('popup-menu', actor => _onPopup(actor, entry));

    entry.connect('destroy', () => {
        entry.menu.destroy();
        entry.menu = null;
        entry._menuManager = null;
    });
}
