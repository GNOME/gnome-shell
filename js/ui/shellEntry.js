// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported addContextMenu CapsLockWarning */

const { Clutter, GObject, Pango, Shell, St } = imports.gi;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;

var EntryMenu = class extends PopupMenu.PopupMenu {
    constructor(entry) {
        super(entry, 0, St.Side.TOP);

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

        if (entry instanceof St.PasswordEntry)
            this._makePasswordItem();

        Main.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }

    _makePasswordItem() {
        let item = new PopupMenu.PopupMenuItem('');
        item.connect('activate', this._onPasswordActivated.bind(this));
        this.addMenuItem(item);
        this._passwordItem = item;

        this._entry.bind_property('show-peek-icon',
            this._passwordItem, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
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
        if (!this._entry.password_visible)
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
        this._entry.password_visible  = !this._entry.password_visible;
    }
};

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
    let cursorPosition = entry.clutter_text.get_cursor_position();
    let [success, textX, textY_, lineHeight_] = entry.clutter_text.position_to_coords(cursorPosition);
    if (success)
        entry.menu.setSourceAlignment(textX / entry.width);
    entry.menu.open(BoxPointer.PopupAnimation.FULL);
}

function addContextMenu(entry, params) {
    if (entry.menu)
        return;

    params = Params.parse(params, { actionMode: Shell.ActionMode.POPUP });

    entry.menu = new EntryMenu(entry);
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

var CapsLockWarning = GObject.registerClass(
class CapsLockWarning extends St.Label {
    _init(params) {
        let defaultParams = { style_class: 'caps-lock-warning-label' };
        super._init(Object.assign(defaultParams, params));

        this.text = _('Caps lock is on.');

        this.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.clutter_text.line_wrap = true;

        let seat = Clutter.get_default_backend().get_default_seat();
        this._keymap = seat.get_keymap();

        this.connect('notify::mapped', () => {
            if (this.is_mapped()) {
                this._keymap.connectObject(
                    'state-changed', () => this._sync(true), this);
            } else {
                this._keymap.disconnectObject(this);
            }

            this._sync(false);
        });
    }

    _sync(animate) {
        let capsLockOn = this._keymap.get_caps_lock_state();

        this.remove_all_transitions();

        const { naturalHeightSet } = this;
        this.natural_height_set = false;
        let [, height] = this.get_preferred_height(-1);
        this.natural_height_set = naturalHeightSet;

        this.ease({
            height: capsLockOn ? height : 0,
            opacity: capsLockOn ? 255 : 0,
            duration: animate ? 200 : 0,
            onComplete: () => {
                if (capsLockOn)
                    this.height = -1;
            },
        });
    }
});
