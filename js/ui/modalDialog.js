import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Dialog from './dialog.js';
import * as Layout from './layout.js';
import * as Lightbox from './lightbox.js';
import * as Main from './main.js';
import * as Params from '../misc/params.js';

const OPEN_AND_CLOSE_TIME = 100;
const FADE_OUT_DIALOG_TIME = 1000;

/** @enum {number} */
export const State = {
    OPENED: 0,
    CLOSED: 1,
    OPENING: 2,
    CLOSING: 3,
    FADED_OUT: 4,
};

export const ModalDialog = GObject.registerClass({
    Properties: {
        'state': GObject.ParamSpec.int(
            'state', null, null,
            GObject.ParamFlags.READABLE,
            Math.min(...Object.values(State)),
            Math.max(...Object.values(State)),
            State.CLOSED),
    },
    Signals: {'opened': {}, 'closed': {}},
}, class ModalDialog extends St.Widget {
    _init(params) {
        super._init({
            visible: false,
            reactive: true,
            x: 0,
            y: 0,
            accessible_role: Atk.Role.DIALOG,
        });

        params = Params.parse(params, {
            shellReactive: false,
            styleClass: null,
            actionMode: Shell.ActionMode.SYSTEM_MODAL,
            shouldFadeIn: true,
            shouldFadeOut: true,
            destroyOnClose: true,
        });

        this._state = State.CLOSED;
        this._hasModal = false;
        this._actionMode = params.actionMode;
        this._shellReactive = params.shellReactive;
        this._shouldFadeIn = params.shouldFadeIn;
        this._shouldFadeOut = params.shouldFadeOut;
        this._destroyOnClose = params.destroyOnClose;

        Main.layoutManager.modalDialogGroup.add_child(this);

        const constraint = new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        });
        this.add_constraint(constraint);

        this.backgroundStack = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        this._backgroundBin = new St.Bin({child: this.backgroundStack});
        this._monitorConstraint = new Layout.MonitorConstraint();
        this._backgroundBin.add_constraint(this._monitorConstraint);
        this.add_child(this._backgroundBin);

        this.dialogLayout = new Dialog.Dialog(this.backgroundStack, params.styleClass);
        this.contentLayout = this.dialogLayout.contentLayout;
        this.buttonLayout = this.dialogLayout.buttonLayout;

        if (!this._shellReactive) {
            this._lightbox = new Lightbox.Lightbox(this, {
                inhibitEvents: true,
                radialEffect: true,
            });
            this._lightbox.highlight(this._backgroundBin);

            this._eventBlocker = new Clutter.Actor({reactive: true});
            this.backgroundStack.add_child(this._eventBlocker);
        }

        global.focus_manager.add_group(this.dialogLayout);
        this._initialKeyFocus = null;
        this._initialKeyFocusDestroyId = 0;
        this._savedKeyFocus = null;
    }

    get state() {
        return this._state;
    }

    _setState(state) {
        if (this._state === state)
            return;

        this._state = state;
        this.notify('state');
    }

    vfunc_key_press_event(event) {
        if (global.focus_manager.navigate_from_event(event))
            return Clutter.EVENT_STOP;

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_captured_event(event) {
        if (Main.keyboard.maybeHandleEvent(event))
            return Clutter.EVENT_STOP;

        return Clutter.EVENT_PROPAGATE;
    }

    clearButtons() {
        this.dialogLayout.clearButtons();
    }

    setButtons(buttons) {
        this.clearButtons();

        for (const buttonInfo of buttons)
            this.addButton(buttonInfo);
    }

    addButton(buttonInfo) {
        return this.dialogLayout.addButton(buttonInfo);
    }

    _fadeOpen() {
        this._monitorConstraint.index = global.display.get_current_monitor();

        this._setState(State.OPENING);

        this.dialogLayout.opacity = 255;
        if (this._lightbox)
            this._lightbox.lightOn();
        this.opacity = 0;
        this.show();
        this.ease({
            opacity: 255,
            duration: this._shouldFadeIn ? OPEN_AND_CLOSE_TIME : 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._setState(State.OPENED);
                this.emit('opened');
            },
        });
    }

    setInitialKeyFocus(actor) {
        this._initialKeyFocus?.disconnectObject(this);

        this._initialKeyFocus = actor;

        actor.connectObject('destroy',
            () => (this._initialKeyFocus = null), this);
    }

    open() {
        if (this.state === State.OPENED || this.state === State.OPENING)
            return true;

        if (!this.pushModal())
            return false;

        this._fadeOpen();
        return true;
    }

    _closeComplete() {
        this._setState(State.CLOSED);
        this.hide();
        this.emit('closed');

        if (this._destroyOnClose)
            this.destroy();
    }

    close() {
        if (this.state === State.CLOSED || this.state === State.CLOSING)
            return;

        this._setState(State.CLOSING);
        this.popModal();
        this._savedKeyFocus = null;

        if (this._shouldFadeOut) {
            this.ease({
                opacity: 0,
                duration: OPEN_AND_CLOSE_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._closeComplete(),
            });
        } else {
            this._closeComplete();
        }
    }

    // Drop modal status without closing the dialog; this makes the
    // dialog insensitive as well, so it needs to be followed shortly
    // by either a close() or a pushModal()
    popModal() {
        if (!this._hasModal)
            return;

        const focus = global.stage.key_focus;
        if (focus && this.contains(focus))
            this._savedKeyFocus = focus;
        else
            this._savedKeyFocus = null;
        Main.popModal(this._grab);
        this._grab = null;
        this._hasModal = false;

        if (!this._shellReactive)
            this.backgroundStack.set_child_above_sibling(this._eventBlocker, null);
    }

    pushModal() {
        if (this._hasModal)
            return true;

        const grab = Main.pushModal(this, {actionMode: this._actionMode});
        if (grab.get_seat_state() !== Clutter.GrabState.ALL) {
            Main.popModal(grab);
            return false;
        }

        this._grab = grab;
        Main.layoutManager.emit('system-modal-opened');

        this._hasModal = true;
        if (this._savedKeyFocus) {
            this._savedKeyFocus.grab_key_focus();
            this._savedKeyFocus = null;
        } else {
            const focus = this._initialKeyFocus || this.dialogLayout.initialKeyFocus;
            focus.grab_key_focus();
        }

        if (!this._shellReactive)
            this.backgroundStack.set_child_below_sibling(this._eventBlocker, null);
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
    _fadeOutDialog() {
        if (this.state === State.CLOSED || this.state === State.CLOSING)
            return;

        if (this.state === State.FADED_OUT)
            return;

        this.popModal();
        this.dialogLayout.ease({
            opacity: 0,
            duration: FADE_OUT_DIALOG_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._setState(State.FADED_OUT),
        });
    }
});
