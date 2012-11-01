// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Atk = imports.gi.Atk;

const Params = imports.misc.params;

const Layout = imports.ui.layout;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const OPEN_AND_CLOSE_TIME = 0.1;
const FADE_OUT_DIALOG_TIME = 1.0;

const State = {
    OPENED: 0,
    CLOSED: 1,
    OPENING: 2,
    CLOSING: 3,
    FADED_OUT: 4
};

const ModalDialog = new Lang.Class({
    Name: 'ModalDialog',

    _init: function(params) {
        params = Params.parse(params, { shellReactive: false,
                                        styleClass: null,
                                        parentActor: Main.uiGroup,
                                        keybindingMode: Main.KeybindingMode.SYSTEM_MODAL,
                                        shouldFadeIn: true });

        this.state = State.CLOSED;
        this._hasModal = false;
        this._keybindingMode = params.keybindingMode;
        this._shellReactive = params.shellReactive;
        this._shouldFadeIn = params.shouldFadeIn;

        this._group = new St.Widget({ visible: false,
                                      x: 0,
                                      y: 0,
                                      accessible_role: Atk.Role.DIALOG });
        params.parentActor.add_actor(this._group);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._group.add_constraint(constraint);

        this._group.connect('destroy', Lang.bind(this, this._onGroupDestroy));

        this._actionKeys = {};
        this._group.connect('key-release-event', Lang.bind(this, this._onKeyReleaseEvent));

        this._backgroundBin = new St.Bin();
        this._monitorConstraint = new Layout.MonitorConstraint();
        this._backgroundBin.add_constraint(this._monitorConstraint);
        this._group.add_actor(this._backgroundBin);

        this.dialogLayout = new St.BoxLayout({ style_class: 'modal-dialog',
                                               vertical:    true });
        if (params.styleClass != null) {
            this.dialogLayout.add_style_class_name(params.styleClass);
        }

        if (!this._shellReactive) {
            this._lightbox = new Lightbox.Lightbox(this._group,
                                                   { inhibitEvents: true });
            this._lightbox.highlight(this._backgroundBin);

            let stack = new Shell.Stack();
            this._backgroundBin.child = stack;

            this._eventBlocker = new Clutter.Group({ reactive: true });
            stack.add_actor(this._eventBlocker);
            stack.add_actor(this.dialogLayout);
        } else {
            this._backgroundBin.child = this.dialogLayout;
        }


        this.contentLayout = new St.BoxLayout({ vertical: true });
        this.dialogLayout.add(this.contentLayout,
                              { x_fill:  true,
                                y_fill:  true,
                                x_align: St.Align.MIDDLE,
                                y_align: St.Align.START });

        this.buttonLayout = new St.BoxLayout({ style_class: 'modal-dialog-button-box',
                                                visible:     false,
                                                vertical:    false });
        this.dialogLayout.add(this.buttonLayout,
                              { expand:  true,
                                x_align: St.Align.MIDDLE,
                                y_align: St.Align.END });

        global.focus_manager.add_group(this.dialogLayout);
        this._initialKeyFocus = this.dialogLayout;
        this._initialKeyFocusDestroyId = 0;
        this._savedKeyFocus = null;
    },

    destroy: function() {
        this._group.destroy();
    },

    setActionKey: function(key, action) {
        this._actionKeys[key] = action;
    },

    clearButtons: function() {
        this.buttonLayout.destroy_all_children();
        this._actionKeys = {};
    },

    setButtons: function(buttons) {
        this.clearButtons();
        this.buttonLayout.visible = (buttons.length > 0);

        for (let i = 0; i < buttons.length; i++) {
            let buttonInfo = buttons[i];

            let x_alignment;
            if (buttons.length == 1)
                x_alignment = St.Align.END;
            else if (i == 0)
                x_alignment = St.Align.START;
            else if (i == buttons.length - 1)
                x_alignment = St.Align.END;
            else
                x_alignment = St.Align.MIDDLE;

            let button = this.addButton(buttonInfo, { expand: true,
                                                      x_fill: false,
                                                      y_fill: false,
                                                      x_align: x_alignment,
                                                      y_align: St.Align.MIDDLE });
            buttonInfo.button = button;
        }
    },

    addButton: function(buttonInfo, layoutInfo) {
        let label = buttonInfo['label'];
        let action = buttonInfo['action'];
        let key = buttonInfo['key'];
        let isDefault = buttonInfo['default'];

        if (isDefault && !key) {
            this._actionKeys[Clutter.KEY_KP_Enter] = action;
            this._actionKeys[Clutter.KEY_ISO_Enter] = action;
            key = Clutter.KEY_Return;
        }

        let button = new St.Button({ style_class: 'modal-dialog-button',
                                     reactive:    true,
                                     can_focus:   true,
                                     label:       label });

        button.connect('clicked', action);

        if (isDefault)
            button.add_style_pseudo_class('default');

        if (!this._initialKeyFocusDestroyId)
            this._initialKeyFocus = button;

        if (key)
            this._actionKeys[key] = action;

        this.buttonLayout.add(button, layoutInfo);

        return button;
    },

    _onKeyReleaseEvent: function(object, event) {
        let symbol = event.get_key_symbol();
        let action = this._actionKeys[symbol];

        if (action) {
            action();
            return true;
        }

        return false;
    },

    _onGroupDestroy: function() {
        this.emit('destroy');
    },

    _fadeOpen: function(onPrimary) {
        if (onPrimary)
            this._monitorConstraint.primary = true;
        else
            this._monitorConstraint.index = global.screen.get_current_monitor();

        this.state = State.OPENING;

        this.dialogLayout.opacity = 255;
        if (this._lightbox)
            this._lightbox.show();
        this._group.opacity = 0;
        this._group.show();
        Tweener.addTween(this._group,
                         { opacity: 255,
                           time: this._shouldFadeIn ? OPEN_AND_CLOSE_TIME : 0,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                               function() {
                                   this.state = State.OPENED;
                                   this.emit('opened');
                               })
                         });
    },

    setInitialKeyFocus: function(actor) {
        if (this._initialKeyFocusDestroyId)
            this._initialKeyFocus.disconnect(this._initialKeyFocusDestroyId);

        this._initialKeyFocus = actor;

        this._initialKeyFocusDestroyId = actor.connect('destroy', Lang.bind(this, function() {
            this._initialKeyFocus = this.dialogLayout;
            this._initialKeyFocusDestroyId = 0;
        }));
    },

    open: function(timestamp, onPrimary) {
        if (this.state == State.OPENED || this.state == State.OPENING)
            return true;

        if (!this.pushModal({ timestamp: timestamp }))
            return false;

        this._fadeOpen(onPrimary);
        return true;
    },

    close: function(timestamp) {
        if (this.state == State.CLOSED || this.state == State.CLOSING)
            return;

        this.state = State.CLOSING;
        this.popModal(timestamp);
        this._savedKeyFocus = null;

        Tweener.addTween(this._group,
                         { opacity: 0,
                           time: OPEN_AND_CLOSE_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                               function() {
                                   this.state = State.CLOSED;
                                   this._group.hide();
                               })
                         });
    },

    // Drop modal status without closing the dialog; this makes the
    // dialog insensitive as well, so it needs to be followed shortly
    // by either a close() or a pushModal()
    popModal: function(timestamp) {
        if (!this._hasModal)
            return;

        let focus = global.stage.key_focus;
        if (focus && this._group.contains(focus))
            this._savedKeyFocus = focus;
        else
            this._savedKeyFocus = null;
        Main.popModal(this._group, timestamp);
        global.gdk_screen.get_display().sync();
        this._hasModal = false;

        if (!this._shellReactive)
            this._eventBlocker.raise_top();
    },

    pushModal: function (timestamp) {
        if (this._hasModal)
            return true;
        if (!Main.pushModal(this._group, { timestamp: timestamp,
                                           keybindingMode: this._keybindingMode }))
            return false;

        this._hasModal = true;
        if (this._savedKeyFocus) {
            this._savedKeyFocus.grab_key_focus();
            this._savedKeyFocus = null;
        } else
            this._initialKeyFocus.grab_key_focus();

        if (!this._shellReactive)
            this._eventBlocker.lower_bottom();
        return true;
    },

    // This method is like close, but fades the dialog out much slower,
    // and leaves the lightbox in place. Once in the faded out state,
    // the dialog can be brought back by an open call, or the lightbox
    // can be dismissed by a close call.
    //
    // The main point of this method is to give some indication to the user
    // that the dialog reponse has been acknowledged but will take a few
    // moments before being processed.
    // e.g., if a user clicked "Log Out" then the dialog should go away
    // imediately, but the lightbox should remain until the logout is
    // complete.
    _fadeOutDialog: function(timestamp) {
        if (this.state == State.CLOSED || this.state == State.CLOSING)
            return;

        if (this.state == State.FADED_OUT)
            return;

        this.popModal(timestamp);
        Tweener.addTween(this.dialogLayout,
                         { opacity: 0,
                           time:    FADE_OUT_DIALOG_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                               function() {
                                   this.state = State.FADED_OUT;
                               })
                         });
    }
});
Signals.addSignalMethods(ModalDialog.prototype);
