// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported HistoryManager */

const Signals = imports.misc.signals;
const Clutter = imports.gi.Clutter;
const Params = imports.misc.params;

var DEFAULT_LIMIT = 512;

var HistoryManager = class extends Signals.EventEmitter {
    constructor(params) {
        super();

        params = Params.parse(params, {
            gsettingsKey: null,
            limit: DEFAULT_LIMIT,
            entry: null,
        });

        this._key = params.gsettingsKey;
        this._limit = params.limit;

        this._historyIndex = 0;
        if (this._key) {
            this._history = global.settings.get_strv(this._key);
            global.settings.connect(`changed::${this._key}`,
                                    this._historyChanged.bind(this));
        } else {
            this._history = [];
        }

        this._entry = params.entry;

        if (this._entry) {
            this._entry.connect('key-press-event',
                                this._onEntryKeyPress.bind(this));
        }
    }

    _historyChanged() {
        this._history = global.settings.get_strv(this._key);
        this._historyIndex = this._history.length;
    }

    _setPrevItem(text) {
        if (this._historyIndex <= 0)
            return false;

        if (text)
            this._history[this._historyIndex] = text;
        this._historyIndex--;
        this._indexChanged();
        return true;
    }

    _setNextItem(text) {
        if (this._historyIndex >= this._history.length)
            return false;

        if (text)
            this._history[this._historyIndex] = text;
        this._historyIndex++;
        this._indexChanged();
        return true;
    }

    lastItem() {
        if (this._historyIndex !== this._history.length) {
            this._historyIndex = this._history.length;
            this._indexChanged();
        }

        return this._historyIndex ? this._history[this._historyIndex - 1] : null;
    }

    addItem(input) {
        input = input.trim();
        if (input &&
            (this._history.length === 0 ||
             this._history[this._history.length - 1] !== input)) {
            this._history = this._history.filter(entry => entry !== input);
            this._history.push(input);
            this._save();
        }
        this._historyIndex = this._history.length;
        return input; // trimmed
    }

    _onEntryKeyPress(entry, event) {
        let symbol = event.get_key_symbol();
        if (symbol === Clutter.KEY_Up)
            return this._setPrevItem(entry.get_text().trim());
        else if (symbol === Clutter.KEY_Down)
            return this._setNextItem(entry.get_text().trim());

        return Clutter.EVENT_PROPAGATE;
    }

    _indexChanged() {
        let current = this._history[this._historyIndex] || '';
        this.emit('changed', current);

        if (this._entry)
            this._entry.set_text(current);
    }

    _save() {
        if (this._history.length > this._limit)
            this._history.splice(0, this._history.length - this._limit);

        if (this._key)
            global.settings.set_strv(this._key, this._history);
    }
};
