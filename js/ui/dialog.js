// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Dialog, MessageDialogContent */

const { Clutter, GObject, Pango, St } = imports.gi;

var Dialog = GObject.registerClass(
class Dialog extends St.Widget {
    _init(parentActor, styleClass) {
        super._init({ layout_manager: new Clutter.BinLayout() });
        this.connect('destroy', this._onDestroy.bind(this));

        this._initialKeyFocus = null;
        this._initialKeyFocusDestroyId = 0;
        this._pressedKey = null;
        this._buttonKeys = {};
        this._createDialog();
        this.add_child(this._dialog);

        if (styleClass != null)
            this._dialog.add_style_class_name(styleClass);

        this._parentActor = parentActor;
        this._eventId = this._parentActor.connect('event', this._modalEventHandler.bind(this));
        this._parentActor.add_child(this);
    }

    _createDialog() {
        this._dialog = new St.BoxLayout({
            style_class: 'modal-dialog',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            vertical: true,
        });

        // modal dialogs are fixed width and grow vertically; set the request
        // mode accordingly so wrapped labels are handled correctly during
        // size requests.
        this._dialog.request_mode = Clutter.RequestMode.HEIGHT_FOR_WIDTH;
        this._dialog.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

        this.contentLayout = new St.BoxLayout({
            vertical: true,
            style_class: 'modal-dialog-content-box',
            y_expand: true,
        });
        this._dialog.add_child(this.contentLayout);

        this.buttonLayout = new St.Widget({
            layout_manager: new Clutter.BoxLayout({ homogeneous: true }),
        });
        this._dialog.add_child(this.buttonLayout);
    }

    _onDestroy() {
        if (this._eventId != 0)
            this._parentActor.disconnect(this._eventId);
        this._eventId = 0;
    }

    _modalEventHandler(actor, event) {
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
    }

    _setInitialKeyFocus(actor) {
        if (this._initialKeyFocus)
            this._initialKeyFocus.disconnect(this._initialKeyFocusDestroyId);

        this._initialKeyFocus = actor;

        this._initialKeyFocusDestroyId = actor.connect('destroy', () => {
            this._initialKeyFocus = null;
            this._initialKeyFocusDestroyId = 0;
        });
    }

    get initialKeyFocus() {
        return this._initialKeyFocus || this;
    }

    addButton(buttonInfo) {
        let { label, action, key } = buttonInfo;
        let isDefault = buttonInfo['default'];
        let keys;

        if (key)
            keys = [key];
        else if (isDefault)
            keys = [Clutter.KEY_Return, Clutter.KEY_KP_Enter, Clutter.KEY_ISO_Enter];
        else
            keys = [];

        let button = new St.Button({
            style_class: 'modal-dialog-linked-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            x_expand: true,
            y_expand: true,
            label,
        });
        button.connect('clicked', action);

        buttonInfo['button'] = button;

        if (isDefault)
            button.add_style_pseudo_class('default');

        if (this._initialKeyFocus == null || isDefault)
            this._setInitialKeyFocus(button);

        for (let i in keys)
            this._buttonKeys[keys[i]] = buttonInfo;

        this.buttonLayout.add_actor(button);

        return button;
    }

    clearButtons() {
        this.buttonLayout.destroy_all_children();
        this._buttonKeys = {};
    }
});

var MessageDialogContent = GObject.registerClass({
    Properties: {
        'title': GObject.ParamSpec.string('title', 'title', 'title',
                                          GObject.ParamFlags.READWRITE |
                                          GObject.ParamFlags.CONSTRUCT,
                                          null),
        'body': GObject.ParamSpec.string('body', 'body', 'body',
                                         GObject.ParamFlags.READWRITE |
                                         GObject.ParamFlags.CONSTRUCT,
                                         null),
    },
}, class MessageDialogContent extends St.BoxLayout {
    _init(params) {
        this._title = new St.Label({ style_class: 'message-dialog-title' });
        this._body = new St.Label({ style_class: 'message-dialog-body' });

        this._body.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._body.clutter_text.line_wrap = true;

        let defaultParams = { style_class: 'message-dialog-main-layout' };
        super._init(Object.assign(defaultParams, params));

        this.messageBox = new St.BoxLayout({
            style_class: 'message-dialog-content',
            x_expand: true,
            vertical: true,
        });
        this.messageBox.add_actor(this._title);
        this.messageBox.add_actor(this._body);
        this.add_actor(this.messageBox);
    }

    get title() {
        return this._title.text;
    }

    get body() {
        return this._body.text;
    }

    set title(title) {
        this._setLabel(this._title, 'title', title);
    }

    set body(body) {
        this._setLabel(this._body, 'body', body);
    }

    _setLabel(label, prop, value) {
        label.set({
            text: value || '',
            visible: value != null,
        });
        this.notify(prop);
    }

    insertBeforeBody(actor) {
        this.messageBox.insert_child_below(actor, this._body);
    }
});
