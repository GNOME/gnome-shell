// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported EditableLabel */

const { Clutter, GObject, Pango, St } = imports.gi;

const GrabHelper = imports.ui.grabHelper;

var EditableLabelMode = {
    DISPLAY: 0,
    HIGHLIGHT: 1,
    EDIT: 2
};

var EditableLabel = GObject.registerClass({
    Signals: {
        'label-edit-update': { param_types: [GObject.TYPE_STRING] },
        'label-edit-cancel': {}
    }
}, class EditableLabel extends St.Entry {
    _init(params) {
        super._init(params);

        this.clutter_text.set({
            editable: false,
            ellipsize: Pango.EllipsizeMode.END,
            selectable: false,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER
        });

        this.clutter_text.bind_property('editable',
            this.clutter_text, 'selectable',
            GObject.BindingFlags.BIDIRECTIONAL |
            GObject.BindingFlags.SYNC_CREATE);

        this._activateId = 0;
        this._keyFocusId = 0;
        this._labelMode = EditableLabelMode.DISPLAY;
        this._oldLabelText = null;

        this.connect('button-press-event', this._onButtonPressEvent.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));

        this._grabHelper = new GrabHelper.GrabHelper(this);
        this._grabHelper.addActor(this);
    }

    _onDestroy() {
        this._cancelEditing();
    }

    _onButtonPressEvent(label, event) {
        if (event.get_button() != Clutter.BUTTON_PRIMARY)
            return false;

        if (event.get_click_count() > 1 &&
            this._labelMode != EditableLabelMode.HIGHLIGHT)
            return false;

        // enter highlight mode if this is the first click
        if (this._labelMode == EditableLabelMode.DISPLAY) {
            this.setMode(EditableLabelMode.HIGHLIGHT);
            return true;
        }

        if (this._labelMode == EditableLabelMode.HIGHLIGHT) {
            // while in highlight mode, another extra click enters the
            // actual edit mode
            this.setMode(EditableLabelMode.EDIT);
            return true;
        }

        // ensure focus stays in the text field when clicking
        // on the entry empty space
        this.grab_key_focus();

        let [stageX] = event.get_coords();
        let [textX] = this.clutter_text.get_transformed_position();

        if (stageX < textX) {
            this.clutter_text.cursor_position = 0;
            this.clutter_text.set_selection(0, 0);
        } else {
            this.clutter_text.cursor_position = -1;
            this.clutter_text.selection_bound = -1;
        }

        // eat button press events on the entry empty space in this mode
        return true;
    }

    _onHighlightUngrab(isUser) {
        // exit highlight mode
        this.remove_style_pseudo_class('highlighted');

        // clicked outside the label - cancel the edit
        if (isUser) {
            this.setMode(EditableLabelMode.DISPLAY);
            this.emit('label-edit-cancel');
            return;
        }

        // now prepare for editing...
        this._keyFocusId = this.connect('key-focus-in', this._startEditing.bind(this));
        this._grabHelper.grab({ actor: this,
                                focus: this,
                                onUngrab: this._onEditUngrab.bind(this) });
    }

    _onEditUngrab(isUser) {
        // edit has already been completed and this is an explicit
        // ungrab from endEditing()
        if (!isUser)
            return;

        let event = Clutter.get_current_event();
        let eventType;

        if (event)
            eventType = event.type();

        if (eventType == Clutter.EventType.KEY_PRESS) {
            let symbol = event.get_key_symbol();

            // abort editing
            if (symbol == Clutter.KEY_Escape)
                this._cancelEditing();

            return;
        }

        // confirm editing when clicked outside the label
        if (eventType == Clutter.EventType.BUTTON_PRESS) {
            this._confirmEditing();
            return;
        }

        // abort editing for other grab-breaking events
        this._cancelEditing();
    }

    _startEditing() {
        let text = this.get_text();

        // Select the current text when editing starts
        this.clutter_text.set({
            cursor_position: 0,
            editable: true,
            selection_bound: text.length
        });

        // Save the current contents of the label, in case we
        // need to roll back
        this._oldLabelText = text;

        this._activateId = this.clutter_text.connect('activate', this._confirmEditing.bind(this));
    }

    _endEditing() {
        this.clutter_text.editable = false;

        this._oldLabelText = null;

        if (this._activateId) {
            this.clutter_text.disconnect(this._activateId);
            this._activateId = 0;
        }

        if (this._keyFocusId) {
            this.disconnect(this._keyFocusId);
            this._keyFocusId = 0;
        }

        if (this._grabHelper.isActorGrabbed(this))
            this._grabHelper.ungrab({ actor: this });

        // Ensure the focus style is removed, because moving the focus
        // programmatically (without a click in another actor) doesn't
        // apparently remove it
        this.remove_style_pseudo_class('focus');
    }

    _cancelEditing() {
        // setting the mode to DISPLAY below will unset oldLabelText
        let oldText = this._oldLabelText;

        this.setMode(EditableLabelMode.DISPLAY);

        this.set_text(oldText);
        this.emit('label-edit-cancel');
    }

    _confirmEditing() {
        // setting the mode to DISPLAY below will unset oldLabelText
        let oldText = this._oldLabelText;
        let text = this.get_text();

        if (!text || text == oldText) {
            this._cancelEditing();
            return;
        }

        this.setMode(EditableLabelMode.DISPLAY);
        this.emit('label-edit-update', text);
    }

    setMode(mode) {
        if (this._labelMode == mode)
            return;

        switch (mode) {
        case EditableLabelMode.DISPLAY:
            this._endEditing();
            break;

        case EditableLabelMode.HIGHLIGHT:
            this.add_style_pseudo_class('highlighted');
            this._grabHelper.grab({
                actor: this,
                onUngrab: this._onHighlightUngrab.bind(this)
            });
            break;

        case EditableLabelMode.EDIT:
            // we need to be in highlight mode before reaching the edit mode
            if (this._labelMode != EditableLabelMode.HIGHLIGHT)
                this.setMode(EditableLabelMode.HIGHLIGHT);

            // enter the edit mode on highlight ungrab
            if (this._grabHelper.grabbed)
                this._grabHelper.ungrab({ actor: this });
            this.grab_key_focus();
            break;

        default:
            throw new Error(`Invalid mode for EditableLabel: ${mode}`);
        }

        this._labelMode = mode;
    }
});
