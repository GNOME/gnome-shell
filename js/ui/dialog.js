// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Lang = imports.lang;

var Dialog = new Lang.Class({
    Name: 'Dialog',
    Extends: St.Widget,

    _init(parentActor, styleClass) {
        this.parent({ layout_manager: new Clutter.BinLayout() });
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
    },

    _createDialog() {
        this._dialog = new St.BoxLayout({ style_class: 'modal-dialog',
                                          x_align:     Clutter.ActorAlign.CENTER,
                                          y_align:     Clutter.ActorAlign.CENTER,
                                          vertical:    true });

        // modal dialogs are fixed width and grow vertically; set the request
        // mode accordingly so wrapped labels are handled correctly during
        // size requests.
        this._dialog.request_mode = Clutter.RequestMode.HEIGHT_FOR_WIDTH;
        this._dialog.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

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

    _onDestroy() {
        if (this._eventId != 0)
            this._parentActor.disconnect(this._eventId);
        this._eventId = 0;
    },

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
    },

    _setInitialKeyFocus(actor) {
        if (this._initialKeyFocus)
            this._initialKeyFocus.disconnect(this._initialKeyFocusDestroyId);

        this._initialKeyFocus = actor;

        this._initialKeyFocusDestroyId = actor.connect('destroy', () => {
            this._initialKeyFocus = null;
            this._initialKeyFocusDestroyId = 0;
        });
    },

    get initialKeyFocus() {
        return this._initialKeyFocus || this;
    },

    addContent(actor) {
        this.contentLayout.add (actor, { expand: true });
    },

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

        if (this._initialKeyFocus == null || isDefault)
            this._setInitialKeyFocus(button);

        for (let i in keys)
            this._buttonKeys[keys[i]] = buttonInfo;

        this.buttonLayout.add_actor(button);

        return button;
    },

    clearButtons() {
        this.buttonLayout.destroy_all_children();
        this._buttonKeys = {};
    },
});

var MessageDialogContent = new Lang.Class({
    Name: 'MessageDialogContent',
    Extends: St.BoxLayout,
    Properties: {
        'icon': GObject.ParamSpec.object('icon', 'icon', 'icon',
                                         GObject.ParamFlags.READWRITE |
                                         GObject.ParamFlags.CONSTRUCT,
                                         Gio.Icon.$gtype),
        'title': GObject.ParamSpec.string('title', 'title', 'title',
                                          GObject.ParamFlags.READWRITE |
                                          GObject.ParamFlags.CONSTRUCT,
                                          null),
        'subtitle': GObject.ParamSpec.string('subtitle', 'subtitle', 'subtitle',
                                             GObject.ParamFlags.READWRITE |
                                             GObject.ParamFlags.CONSTRUCT,
                                             null),
        'body': GObject.ParamSpec.string('body', 'body', 'body',
                                         GObject.ParamFlags.READWRITE |
                                         GObject.ParamFlags.CONSTRUCT,
                                         null)
    },

    _init(params) {
        this._icon = new St.Icon({ y_align: Clutter.ActorAlign.START });
        this._title = new St.Label({ style_class: 'headline' });
        this._subtitle = new St.Label();
        this._body = new St.Label();

        ['icon', 'title', 'subtitle', 'body'].forEach(prop => {
            this[`_${prop}`].add_style_class_name(`message-dialog-${prop}`);
        });

        let textProps = { ellipsize_mode: Pango.EllipsizeMode.NONE,
                          line_wrap: true };
        Object.assign(this._subtitle.clutter_text, textProps);
        Object.assign(this._body.clutter_text, textProps);

        if (!params.hasOwnProperty('style_class'))
            params.style_class = 'message-dialog-main-layout';

        this.parent(params);

        this.messageBox = new St.BoxLayout({ style_class: 'message-dialog-content',
                                             x_expand: true,
                                             vertical: true });

        this.messageBox.add_actor(this._title);
        this.messageBox.add_actor(this._subtitle);
        this.messageBox.add_actor(this._body);

        this.add_actor(this._icon);
        this.add_actor(this.messageBox);
    },

    get icon() {
        return this._icon.gicon;
    },

    get title() {
        return this._title.text;
    },

    get subtitle() {
        return this._subtitle.text;
    },

    get body() {
        return this._body.text;
    },

    set icon(icon) {
        Object.assign(this._icon, { gicon: icon, visible: icon != null });
        this.notify('icon');
    },

    set title(title) {
        this._setLabel(this._title, 'title', title);
    },

    set subtitle(subtitle) {
        this._setLabel(this._subtitle, 'subtitle', subtitle);
    },

    set body(body) {
        this._setLabel(this._body, 'body', body);
    },

    _setLabel(label, prop, value) {
        Object.assign(label, { text: value || '', visible: value != null });
        this.notify(prop);
    },

    insertBeforeBody(actor) {
        this.messageBox.insert_child_below(actor, this._body);
    }
});
