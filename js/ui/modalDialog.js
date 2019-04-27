// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Atk, Clutter, Shell, St } = imports.gi;
const Signals = imports.signals;

const Dialog = imports.ui.dialog;
const Layout = imports.ui.layout;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

var OPEN_AND_CLOSE_TIME = 0.1;
var FADE_OUT_DIALOG_TIME = 1.0;

var State = {
    OPENED: 0,
    CLOSED: 1,
    OPENING: 2,
    CLOSING: 3,
    FADED_OUT: 4
};

var ModalDialog = class {
    constructor(params) {
        params = Params.parse(params, { shellReactive: false,
                                        styleClass: null,
                                        actionMode: Shell.ActionMode.SYSTEM_MODAL,
                                        shouldFadeIn: true,
                                        shouldFadeOut: true,
                                        destroyOnClose: true });

        this.state = State.CLOSED;
        this._hasModal = false;
        this._actionMode = params.actionMode;
        this._shellReactive = params.shellReactive;
        this._shouldFadeIn = params.shouldFadeIn;
        this._shouldFadeOut = params.shouldFadeOut;
        this._destroyOnClose = params.destroyOnClose;

        this._group = new St.Widget({ visible: false,
                                      x: 0,
                                      y: 0,
                                      accessible_role: Atk.Role.DIALOG });
        Main.layoutManager.modalDialogGroup.add_actor(this._group);

        let constraint = new Clutter.BindConstraint({ source: global.stage,
                                                      coordinate: Clutter.BindCoordinate.ALL });
        this._group.add_constraint(constraint);

        this._group.connect('destroy', this._onGroupDestroy.bind(this));

        this.backgroundStack = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._backgroundBin = new St.Bin({ child: this.backgroundStack,
                                           x_fill: true, y_fill: true });
        this._monitorConstraint = new Layout.MonitorConstraint();
        this._backgroundBin.add_constraint(this._monitorConstraint);
        this._group.add_actor(this._backgroundBin);

        this.dialogLayout = new Dialog.Dialog(this.backgroundStack, params.styleClass);
        this.contentLayout = this.dialogLayout.contentLayout;
        this.buttonLayout = this.dialogLayout.buttonLayout;

        if (!this._shellReactive) {
            this._lightbox = new Lightbox.Lightbox(this._group,
                                                   { inhibitEvents: true,
                                                     radialEffect: true });
            this._lightbox.highlight(this._backgroundBin);

            this._eventBlocker = new Clutter.Actor({ reactive: true });
            this.backgroundStack.add_actor(this._eventBlocker);
        }

        global.focus_manager.add_group(this.dialogLayout);
        this._initialKeyFocus = null;
        this._initialKeyFocusDestroyId = 0;
        this._savedKeyFocus = null;
    }

    destroy() {
        this._group.destroy();
    }

    clearButtons() {
        this.dialogLayout.clearButtons();
    }

    setButtons(buttons) {
        this.clearButtons();

        for (let buttonInfo of buttons)
            this.addButton(buttonInfo);
    }

    addButton(buttonInfo) {
        return this.dialogLayout.addButton(buttonInfo);
    }

    _onGroupDestroy() {
        this.emit('destroy');
    }

    _fadeOpen(onPrimary) {
        if (onPrimary)
            this._monitorConstraint.primary = true;
        else
            this._monitorConstraint.index = global.display.get_current_monitor();

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
                           onComplete: () => {
                               this.state = State.OPENED;
                               this.emit('opened');
                           }
                         });
    }

    setInitialKeyFocus(actor) {
        if (this._initialKeyFocusDestroyId)
            this._initialKeyFocus.disconnect(this._initialKeyFocusDestroyId);

        this._initialKeyFocus = actor;

        this._initialKeyFocusDestroyId = actor.connect('destroy', () => {
            this._initialKeyFocus = null;
            this._initialKeyFocusDestroyId = 0;
        });
    }

    open(timestamp, onPrimary) {
        if (this.state == State.OPENED || this.state == State.OPENING)
            return true;

        if (!this.pushModal(timestamp))
            return false;

        this._fadeOpen(onPrimary);
        return true;
    }

    _closeComplete() {
        this.state = State.CLOSED;
        this._group.hide();
        this.emit('closed');

        if (this._destroyOnClose)
            this.destroy();
    }

    close(timestamp) {
        if (this.state == State.CLOSED || this.state == State.CLOSING)
            return;

        this.state = State.CLOSING;
        this.popModal(timestamp);
        this._savedKeyFocus = null;

        if (this._shouldFadeOut)
            Tweener.addTween(this._group,
                             { opacity: 0,
                               time: OPEN_AND_CLOSE_TIME,
                               transition: 'easeOutQuad',
                               onComplete: this._closeComplete.bind(this)
                             })
        else
            this._closeComplete();
    }

    // Drop modal status without closing the dialog; this makes the
    // dialog insensitive as well, so it needs to be followed shortly
    // by either a close() or a pushModal()
    popModal(timestamp) {
        if (!this._hasModal)
            return;

        let focus = global.stage.key_focus;
        if (focus && this._group.contains(focus))
            this._savedKeyFocus = focus;
        else
            this._savedKeyFocus = null;
        Main.popModal(this._group, timestamp);
        this._hasModal = false;

        if (!this._shellReactive)
            this._eventBlocker.raise_top();
    }

    pushModal(timestamp) {
        if (this._hasModal)
            return true;

        let params = { actionMode: this._actionMode };
        if (timestamp)
            params['timestamp'] = timestamp;
        if (!Main.pushModal(this._group, params))
            return false;

        this._hasModal = true;
        if (this._savedKeyFocus) {
            this._savedKeyFocus.grab_key_focus();
            this._savedKeyFocus = null;
        } else {
            let focus = this._initialKeyFocus || this.dialogLayout.initialKeyFocus;
            focus.grab_key_focus();
        }

        if (!this._shellReactive)
            this._eventBlocker.lower_bottom();
        return true;
    }

    // This method is like close, but fades the dialog out much slower,
    // and leaves the lightbox in place. Once in the faded out state,
    // the dialog can be brought back by an open call, or the lightbox
    // can be dismissed by a close call.
    //
    // The main point of this method is to give some indication to the user
    // that the dialog response has been acknowledged but will take a few
    // moments before being processed.
    // e.g., if a user clicked "Log Out" then the dialog should go away
    // immediately, but the lightbox should remain until the logout is
    // complete.
    _fadeOutDialog(timestamp) {
        if (this.state == State.CLOSED || this.state == State.CLOSING)
            return;

        if (this.state == State.FADED_OUT)
            return;

        this.popModal(timestamp);
        Tweener.addTween(this.dialogLayout,
                         { opacity: 0,
                           time:    FADE_OUT_DIALOG_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                               this.state = State.FADED_OUT;
                           }
                         });
    }
};
Signals.addSignalMethods(ModalDialog.prototype);
