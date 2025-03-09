import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import IBus from 'gi://IBus';

import * as Keyboard from '../ui/status/keyboard.js';
import * as Main from '../ui/main.js';

Gio._promisify(IBus.Bus.prototype,
    'create_input_context_async', 'create_input_context_async_finish');
Gio._promisify(IBus.InputContext.prototype,
    'process_key_event_async', 'process_key_event_async_finish');

const HIDE_PANEL_TIME = 50;

const HAVE_REQUIRE_SURROUNDING_TEXT = GObject.signal_lookup('require-surrounding-text', IBus.InputContext);

export const InputMethod = GObject.registerClass({
    Signals: {
        'surrounding-text-set': {},
    },
}, class InputMethod extends Clutter.InputMethod {
    _init() {
        super._init();
        this._hints = 0;
        this._purpose = 0;
        this._currentFocus = null;
        this._preeditStr = '';
        this._preeditPos = 0;
        this._preeditAnchor = 0;
        this._preeditVisible = false;
        this._hidePanelId = 0;
        this._surroundingText = null;
        this._surroundingTextCursor = null;
        this._surroundingTextAnchor = null;
        this._ibus = IBus.Bus.new_async();
        this._ibus.connect('connected', this._onConnected.bind(this));
        this._ibus.connect('disconnected', this._clear.bind(this));
        this.connect('notify::can-show-preedit', this._updateCapabilities.bind(this));

        this._inputSourceManager = Keyboard.getInputSourceManager();
        this._sourceChangedId = this._inputSourceManager.connect('current-source-changed',
            this._onSourceChanged.bind(this));
        this._currentSource = this._inputSourceManager.currentSource;

        if (this._ibus.is_connected())
            this._onConnected();
    }

    get currentFocus() {
        return this._currentFocus;
    }

    _updateCapabilities() {
        let caps = IBus.Capabilite.PREEDIT_TEXT | IBus.Capabilite.FOCUS;

        if (this._surroundingText !== null)
            caps |= IBus.Capabilite.SURROUNDING_TEXT;

        if (Main.keyboard.visible)
            caps |= IBus.Capabilite.OSK;

        if (this._context)
            this._context.set_capabilities(caps);
    }

    _onSourceChanged() {
        this._currentSource = this._inputSourceManager.currentSource;
    }

    async _onConnected() {
        this._cancellable = new Gio.Cancellable();
        try {
            this._context = await this._ibus.create_input_context_async(
                'gnome-shell', -1, this._cancellable);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                logError(e);
                this._clear();
            }
            return;
        }

        this._context.set_client_commit_preedit(true);
        this._context.connect('commit-text', this._onCommitText.bind(this));
        this._context.connect('delete-surrounding-text', this._onDeleteSurroundingText.bind(this));
        this._context.connect('update-preedit-text-with-mode', this._onUpdatePreeditText.bind(this));
        this._context.connect('show-preedit-text', this._onShowPreeditText.bind(this));
        this._context.connect('hide-preedit-text', this._onHidePreeditText.bind(this));
        this._context.connect('forward-key-event', this._onForwardKeyEvent.bind(this));
        this._context.connect('destroy', this._clear.bind(this));

        if (HAVE_REQUIRE_SURROUNDING_TEXT)
            this._context.connect('require-surrounding-text', this._onRequireSurroundingText.bind(this));

        Main.keyboard.connectObject('visibility-changed', () => this._updateCapabilities());

        this._updateCapabilities();
    }

    _clear() {
        Main.keyboard.disconnectObject(this);

        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        this._context = null;
        this._hints = 0;
        this._purpose = 0;
        this._preeditStr = '';
        this._preeditPos = 0;
        this._preeditAnchor = 0;
        this._preeditVisible = false;
    }

    _emitRequestSurrounding() {
        if (this._context.needs_surrounding_text())
            this.emit('request-surrounding');
    }

    _onCommitText(_context, text) {
        this.commit(text.get_text());
    }

    _onRequireSurroundingText(_context) {
        this.request_surrounding();
    }

    _onDeleteSurroundingText(_context, offset, nchars) {
        if (this._surroundingText === null) {
            log('input-method engines should not call ' +
                'the delete-surrounding-text API in case ' +
                'the input context has no SURROUNDING_TEXT capability.');
            return;
        }
        try {
            this.delete_surrounding(offset, nchars);
        } catch {
            // We may get out of bounds for negative offset on older mutter
            this.delete_surrounding(0, nchars + offset);
        }
    }

    _onUpdatePreeditText(_context, text, pos, visible, mode) {
        if (text == null)
            return;

        let preedit = text.get_text();
        if (preedit === '')
            preedit = null;

        const anchor = pos;
        if (visible)
            this.set_preedit_text(preedit, pos, anchor, mode);
        else if (this._preeditVisible)
            this.set_preedit_text(null, pos, anchor, mode);

        this._preeditStr = preedit;
        this._preeditPos = pos;
        this._preeditAnchor = anchor;
        this._preeditVisible = visible;
        this._preeditCommitMode = mode;
    }

    _onShowPreeditText() {
        this._preeditVisible = true;
        this.set_preedit_text(
            this._preeditStr, this._preeditPos, this._preeditAnchor,
            this._preeditCommitMode);
    }

    _onHidePreeditText() {
        this.set_preedit_text(
            null, this._preeditPos, this._preeditAnchor,
            this._preeditCommitMode);
        this._preeditVisible = false;
    }

    _onForwardKeyEvent(_context, keyval, keycode, state) {
        let press = (state & IBus.ModifierType.RELEASE_MASK) === 0;
        state &= ~IBus.ModifierType.RELEASE_MASK;

        let curEvent = Clutter.get_current_event();
        let time;
        if (curEvent)
            time = curEvent.get_time();
        else
            time = global.display.get_current_time_roundtrip();

        this.forward_key(keyval, keycode + 8, state & Clutter.ModifierType.MODIFIER_MASK, time, press);
    }

    vfunc_focus_in(focus) {
        this._currentFocus = focus;
        if (this._context) {
            this.update();
            this._context.focus_in();
            this._emitRequestSurrounding();
        }

        if (this._hidePanelId) {
            GLib.source_remove(this._hidePanelId);
            this._hidePanelId = 0;
        }
    }

    vfunc_focus_out() {
        this._currentFocus = null;
        if (this._context) {
            this._fullReset();
            this._context.focus_out();
        }

        if (this._preeditStr && this._preeditVisible) {
            // Unset any preedit text
            this.set_preedit_text(null, 0, 0, this._preeditCommitMode);
            this._preeditStr = null;
        }

        this._hidePanelId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, HIDE_PANEL_TIME, () => {
            this.set_input_panel_state(Clutter.InputPanelState.OFF);
            this._hidePanelId = 0;
            return GLib.SOURCE_REMOVE;
        });
    }

    vfunc_reset() {
        if (this._context) {
            this._context.reset();
            this._emitRequestSurrounding();
        }

        this._surroundingText = null;
        this._surroundingTextCursor = null;
        this._surroundingTextAnchor = null;
        this._preeditStr = null;
    }

    vfunc_set_cursor_location(rect) {
        if (this._context) {
            this._cursorRect = {
                x: rect.get_x(), y: rect.get_y(),
                width: rect.get_width(), height: rect.get_height(),
            };
            this._context.set_cursor_location(
                this._cursorRect.x, this._cursorRect.y,
                this._cursorRect.width, this._cursorRect.height);
            this._emitRequestSurrounding();
        }
    }

    vfunc_set_surrounding(text, cursor, anchor) {
        // If the previous input context supports the surrounding-text feature.
        const prevHasSurrounding = this._surroundingText !== null;
        // If the current input context supports the surrounding-text feature.
        const nowHasSurrounding = text !== null;
        // If the SURROUNDING_TEXT capability is changed.
        const updateCapabilities = prevHasSurrounding !== nowHasSurrounding;

        this._surroundingText = text;
        this._surroundingTextCursor = cursor;
        this._surroundingTextAnchor = anchor;
        this.emit('surrounding-text-set');

        if (!this._context || (!text && text !== ''))
            return;

        let ibusText = IBus.Text.new_from_string(text);

        if (updateCapabilities)
            this._updateCapabilities();

        // Call context.set_surrounding_text() after context.set_capabilities().
        this._context.set_surrounding_text(ibusText, cursor, anchor);
    }

    vfunc_update_content_hints(hints) {
        let ibusHints = 0;
        if (hints & Clutter.InputContentHintFlags.COMPLETION)
            ibusHints |= IBus.InputHints.WORD_COMPLETION;
        if (hints & Clutter.InputContentHintFlags.SPELLCHECK)
            ibusHints |= IBus.InputHints.SPELLCHECK;
        if (hints & Clutter.InputContentHintFlags.AUTO_CAPITALIZATION)
            ibusHints |= IBus.InputHints.UPPERCASE_SENTENCES;
        if (hints & Clutter.InputContentHintFlags.LOWERCASE)
            ibusHints |= IBus.InputHints.LOWERCASE;
        if (hints & Clutter.InputContentHintFlags.UPPERCASE)
            ibusHints |= IBus.InputHints.UPPERCASE_CHARS;
        if (hints & Clutter.InputContentHintFlags.TITLECASE)
            ibusHints |= IBus.InputHints.UPPERCASE_WORDS;
        if (hints & Clutter.InputContentHintFlags.SENSITIVE_DATA)
            ibusHints |= IBus.InputHints.PRIVATE;

        this._hints = ibusHints;
        if (this._context)
            this._context.set_content_type(this._purpose, this._hints);
    }

    vfunc_update_content_purpose(purpose) {
        let ibusPurpose = 0;
        if (purpose === Clutter.InputContentPurpose.NORMAL)
            ibusPurpose = IBus.InputPurpose.FREE_FORM;
        else if (purpose === Clutter.InputContentPurpose.ALPHA)
            ibusPurpose = IBus.InputPurpose.ALPHA;
        else if (purpose === Clutter.InputContentPurpose.DIGITS)
            ibusPurpose = IBus.InputPurpose.DIGITS;
        else if (purpose === Clutter.InputContentPurpose.NUMBER)
            ibusPurpose = IBus.InputPurpose.NUMBER;
        else if (purpose === Clutter.InputContentPurpose.PHONE)
            ibusPurpose = IBus.InputPurpose.PHONE;
        else if (purpose === Clutter.InputContentPurpose.URL)
            ibusPurpose = IBus.InputPurpose.URL;
        else if (purpose === Clutter.InputContentPurpose.EMAIL)
            ibusPurpose = IBus.InputPurpose.EMAIL;
        else if (purpose === Clutter.InputContentPurpose.NAME)
            ibusPurpose = IBus.InputPurpose.NAME;
        else if (purpose === Clutter.InputContentPurpose.PASSWORD)
            ibusPurpose = IBus.InputPurpose.PASSWORD;
        else if (purpose === Clutter.InputContentPurpose.TERMINAL &&
                 IBus.InputPurpose.TERMINAL)
            ibusPurpose = IBus.InputPurpose.TERMINAL;

        this._purpose = ibusPurpose;
        if (this._context)
            this._context.set_content_type(this._purpose, this._hints);
    }

    vfunc_filter_key_event(event) {
        if (!this._context)
            return false;
        if (!this._currentSource)
            return false;

        let state = event.get_state();
        if (state & IBus.ModifierType.IGNORED_MASK)
            return false;

        if (event.type() === Clutter.EventType.KEY_RELEASE)
            state |= IBus.ModifierType.RELEASE_MASK;

        this._context.process_key_event_async(
            event.get_key_symbol(),
            event.get_key_code() - 8, // Convert XKB keycodes to evcodes
            state, -1, this._cancellable,
            (context, res) => {
                if (context !== this._context)
                    return;

                try {
                    let retval = context.process_key_event_async_finish(res);
                    this.notify_key_event(event, retval);
                } catch (e) {
                    if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                        log(`Error processing key on IM: ${e.message}`);
                }
            });
        return true;
    }

    getSurroundingText() {
        return [
            this._surroundingText,
            this._surroundingTextCursor,
            this._surroundingTextAnchor,
        ];
    }

    hasPreedit() {
        return this._preeditVisible && this._preeditStr !== '' && this._preeditStr !== null;
    }

    async handleVirtualKey(keyval) {
        try {
            if (!await this._context.process_key_event_async(
                keyval, 0, 0, -1, null))
                return false;

            await this._context.process_key_event_async(
                keyval, 0, IBus.ModifierType.RELEASE_MASK, -1, null);
            return true;
        } catch {
            return false;
        }
    }

    _fullReset() {
        this._context.set_content_type(0, 0);
        this._context.set_cursor_location(0, 0, 0, 0);
        this._context.reset();
    }

    update() {
        if (!this._context)
            return;
        this._updateCapabilities();
        this._context.set_content_type(this._purpose, this._hints);
        if (this._cursorRect) {
            this._context.set_cursor_location(
                this._cursorRect.x, this._cursorRect.y,
                this._cursorRect.width, this._cursorRect.height);
        }
        this._emitRequestSurrounding();
    }
});
