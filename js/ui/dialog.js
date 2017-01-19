// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Lang = imports.lang;

const Dialog = new Lang.Class({
    Name: 'Dialog',
    Extends: St.Widget,

    _init: function (parentActor, styleClass) {
        this.parent({ layout_manager: new Clutter.BinLayout() });
        this.connect('destroy', Lang.bind(this, this._onDestroy));

        this._pressedKey = null;
        this._buttonKeys = {};
        this._createDialog();
        this.add_child(this._dialog);

        if (styleClass != null)
            this._dialog.add_style_class_name(styleClass);

        this._parentActor = parentActor;
        this._eventId = this._parentActor.connect('event', Lang.bind(this, this._modalEventHandler));
        this._parentActor.add_child(this);
    },

    _createDialog: function () {
        this._dialog = new St.BoxLayout({ style_class: 'modal-dialog',
                                          x_align:     Clutter.ActorAlign.CENTER,
                                          y_align:     Clutter.ActorAlign.CENTER,
                                          vertical:    true });

        // modal dialogs are fixed width and grow vertically; set the request
        // mode accordingly so wrapped labels are handled correctly during
        // size requests.
        this._dialog.request_mode = Clutter.RequestMode.HEIGHT_FOR_WIDTH;

        this.contentLayout = new St.BoxLayout({ vertical: true,
                                                style_class: "modal-dialog-content-box" });
        this._dialog.add(this.contentLayout,
                         { expand:  true,
                           x_fill:  true,
                           y_fill:  true,
                           x_align: St.Align.MIDDLE,
                           y_align: St.Align.START });

        this.buttonLayout = new St.Widget ({ layout_manager: new Clutter.BoxLayout({ homogeneous:true }) });
        this._dialog.add(this.buttonLayout,
                         { x_align: St.Align.MIDDLE,
                           y_align: St.Align.START });
    },

    _onDestroy: function () {
        if (this._eventId != 0)
            this._parentActor.disconnect(this._eventId);
        this._eventId = 0;
    },

    _modalEventHandler: function (actor, event) {
        if (event.type() == Clutter.EventType.KEY_PRESS) {
            this._pressedKey = event.get_key_symbol();
        } else if (event.type() == Clutter.EventType.KEY_RELEASE) {
            let pressedKey = this._pressedKey;
            this._pressedKey = null;

            let symbol = event.get_key_symbol();
            if (symbol != pressedKey)
                return Clutter.EVENT_PROPAGATE;

            let buttonInfo = this._buttonKeys[symbol];
            if (!buttonInfo)
                return Clutter.EVENT_PROPAGATE;

            let { button, action } = buttonInfo;

            if (action && button.reactive) {
                action();
                return Clutter.EVENT_STOP;
            }
        }

        return Clutter.EVENT_PROPAGATE;
    },

    addContent: function (actor) {
        this.contentLayout.add (actor, { expand: true });
    },

    addButton: function (buttonInfo) {
        let { label, action, key } = buttonInfo;
        let isDefault = buttonInfo['default'];
        let keys;

        if (key)
            keys = [key];
        else if (isDefault)
            keys = [Clutter.KEY_Return, Clutter.KEY_KP_Enter, Clutter.KEY_ISO_Enter];
        else
            keys = [];

        let button = new St.Button({ style_class: 'modal-dialog-linked-button',
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                     reactive:    true,
                                     can_focus:   true,
                                     x_expand:    true,
                                     y_expand:    true,
                                     label:       label });
        button.connect('clicked', action);

        buttonInfo['button'] = button;

        if (isDefault)
            button.add_style_pseudo_class('default');

        if (!this._initialKeyFocusDestroyId)
            this._initialKeyFocus = button;

        for (let i in keys)
            this._buttonKeys[keys[i]] = buttonInfo;

        this.buttonLayout.add_actor(button);

        return button;
    },

    clearButtons: function () {
        this.buttonLayout.destroy_all_children();
        this._buttonKeys = {};
    },
});
