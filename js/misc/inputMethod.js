// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
const Clutter = imports.gi.Clutter;
const IBus = imports.gi.IBus;
const Keyboard = imports.ui.status.keyboard;
const Lang = imports.lang;
const Signals = imports.signals;

var InputMethod = new Lang.Class({
    Name: 'InputMethod',
    Extends: Clutter.InputMethod,

    _init() {
        this.parent();
        this._hints = 0;
        this._purpose = 0;
        this._enabled = true;
        this._currentFocus = null;
        this._currentEvent = null;
        this._doForwardEvent = false;
        this._preeditStr = '';
        this._preeditPos = 0;
        this._ibus = IBus.Bus.new_async();
        this._ibus.connect('connected', this._onConnected.bind(this));
        this._ibus.connect('disconnected', this._clear.bind(this));
        this.connect('notify::can-show-preedit', this._updateCapabilities.bind(this));

        this._inputSourceManager = Keyboard.getInputSourceManager();
        this._sourceChangedId = this._inputSourceManager.connect('current-source-changed',
                                                                 this._onSourceChanged.bind(this));
        this._currentSource = this._inputSourceManager.currentSource;

        let deviceManager = Clutter.DeviceManager.get_default();
        this._virtualDevice = deviceManager.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

        if (this._ibus.is_connected())
            this._onConnected();
    },

    get currentFocus() {
        return this._currentFocus;
    },

    _updateCapabilities() {
        let caps = 0;

        if (this.can_show_preedit)
            caps |= IBus.Capabilite.PREEDIT_TEXT;

        if (this._currentFocus)
            caps |= IBus.Capabilite.FOCUS | IBus.Capabilite.SURROUNDING_TEXT;
        else
            caps |= IBus.Capabilite.PREEDIT_TEXT | IBus.Capabilite.AUXILIARY_TEXT | IBus.Capabilite.LOOKUP_TABLE | IBus.Capabilite.PROPERTY;

        if (this._context)
            this._context.set_capabilities(caps);
    },

    _onSourceChanged() {
        this._currentSource = this._inputSourceManager.currentSource;
    },

    _onConnected() {
        this._ibus.create_input_context_async ('gnome-shell', -1, null,
                                               this._setContext.bind(this));
    },

    _setContext(bus, res) {
        this._context = this._ibus.create_input_context_async_finish(res);
        this._context.connect('enabled', () => { this._enabled = true });
        this._context.connect('disabled', () => { this._enabled = false });
        this._context.connect('commit-text', this._onCommitText.bind(this));
        this._context.connect('delete-surrounding-text', this._onDeleteSurroundingText.bind(this));
        this._context.connect('update-preedit-text', this._onUpdatePreeditText.bind(this));
        this._context.connect('show-preedit-text', this._onShowPreeditText.bind(this));
        this._context.connect('hide-preedit-text', this._onHidePreeditText.bind(this));
        this._context.connect('forward-key-event', this._onForwardKeyEvent.bind(this));

        this._updateCapabilities();
    },

    _clear() {
        this._context = null;
        this._hints = 0;
        this._purpose = 0;
        this._enabled = false;
        this._preeditStr = ''
        this._preeditPos = 0;
    },

    _emitRequestSurrounding() {
        if (this._context.needs_surrounding_text())
            this.emit('request-surrounding');
    },

    _onCommitText(context, text) {
        this.commit(text.get_text());
    },

    _onDeleteSurroundingText(context) {
        this.delete_surrounding();
    },

    _onUpdatePreeditText(context, text, pos, visible) {
        if (text == null)
            return;
        this._preeditStr = text.get_text();
        this._preeditPos = pos;
        if (visible)
            this.set_preedit_text(this._preeditStr, pos);
        else
            this.set_preedit_text(null, pos);
    },

    _onShowPreeditText(context) {
        this.set_preedit_text(this._preeditStr, this._preeditPos);
    },

    _onHidePreeditText(context) {
        this.set_preedit_text(null, this._preeditPos);
    },

    _onForwardKeyEvent(context, keyval, keycode, state) {
        let press = (state & IBus.ModifierType.RELEASE_MASK) == 0;

        if (this._currentEvent) {
            // If we are handling this same event in filter_key_press(),
            // just let it go through, sending the same event again will
            // be silenced away because the key counts as pressed.
            if (this._currentEvent.get_key_symbol() == keyval &&
                (this._currentEvent.type() == Clutter.EventType.KEY_PRESS) == press) {
                this._doForwardEvent = true;
                return;
            }
        }

        this._virtualDevice.notify_key(Clutter.get_current_event_time(), keycode,
                                       press ? Clutter.KeyState.PRESSED : Clutter.KeyState.RELEASED);
    },

    vfunc_focus_in(focus) {
        this._currentFocus = focus;
        if (this._context) {
            this._context.focus_in();
            this._updateCapabilities();
            this._emitRequestSurrounding();
        }
    },

    vfunc_focus_out() {
        this._currentFocus = null;
        if (this._context) {
            this._context.focus_out();
            this._updateCapabilities();
        }

        // Unset any preedit text
        this.set_preedit_text(null, 0);
    },

    vfunc_reset() {
        if (this._context) {
            this._context.reset();
            this._emitRequestSurrounding();
        }

        // Unset any preedit text
        this.set_preedit_text(null, 0);
    },

    vfunc_set_cursor_location(rect) {
        if (this._context) {
            this._context.set_cursor_location(rect.get_x(), rect.get_y(),
                                              rect.get_width(), rect.get_height());
            this._emitRequestSurrounding();
        }
    },

    vfunc_set_surrounding(text, cursor, anchor) {
        if (this._context && text)
            this._context.set_surrounding_text(text, cursor, anchor);
    },

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

        this._hints = ibusHints;
        if (this._context)
            this._context.set_content_type(this._purpose, this._hints);
    },

    vfunc_update_content_purpose(purpose) {
        let ibusPurpose = 0;
        if (purpose == Clutter.InputContentPurpose.NORMAL)
            ibusPurpose = IBus.InputPurpose.FREE_FORM;
        else if (purpose == Clutter.InputContentPurpose.ALPHA)
            ibusPurpose = IBus.InputPurpose.ALPHA;
        else if (purpose == Clutter.InputContentPurpose.DIGITS)
            ibusPurpose = IBus.InputPurpose.DIGITS;
        else if (purpose == Clutter.InputContentPurpose.NUMBER)
            ibusPurpose = IBus.InputPurpose.NUMBER;
        else if (purpose == Clutter.InputContentPurpose.PHONE)
            ibusPurpose = IBus.InputPurpose.PHONE;
        else if (purpose == Clutter.InputContentPurpose.URL)
            ibusPurpose = IBus.InputPurpose.URL;
        else if (purpose == Clutter.InputContentPurpose.EMAIL)
            ibusPurpose = IBus.InputPurpose.EMAIL;
        else if (purpose == Clutter.InputContentPurpose.NAME)
            ibusPurpose = IBus.InputPurpose.NAME;
        else if (purpose == Clutter.InputContentPurpose.PASSWORD)
            ibusPurpose = IBus.InputPurpose.PASSWORD;

        this._purpose = ibusPurpose;
        if (this._context)
            this._context.set_content_type(this._purpose, this._hints);
    },

    vfunc_filter_key_event(event) {
        if (!this._context || !this._enabled)
            return false;
        if (!this._currentSource)
            return false;

        let state = event.get_state();
        if (state & IBus.ModifierType.IGNORED_MASK)
            return false;

        if (event.type() == Clutter.EventType.KEY_RELEASE)
            state |= IBus.ModifierType.RELEASE_MASK;

        this._currentEvent = event;
        this._doForwardEvent = false;

        this._context.process_key_event_async(event.get_key_symbol(),
                                              event.get_key_code() - 8, // Convert XKB keycodes to evcodes
                                              state, -1, null,
                                              (context, res) => {
                                                  try {
                                                      let retval = context.process_key_event_async_finish(res);

                                                      if (this._doForwardEvent)
                                                          retval = false;

                                                      this.notify_key_event(event, retval);
                                                      this._doForwardEvent = false;
                                                      this._currentEvent = null;
                                                  } catch (e) {
                                                      log('Error processing key on IM: ' + e.message);
                                                  }
                                              });
        return true;
    },
});
