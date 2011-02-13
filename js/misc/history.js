/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Signals = imports.signals;

const DEFAULT_LIMIT = 512;

function HistoryManager(settings_key) {
    this._init(settings_key);
}

HistoryManager.prototype = {
    _init: function(settings_key, limit) {
        this._limit = limit || DEFAULT_LIMIT;
        this._key = settings_key;
        this._history = global.settings.get_strv(settings_key);
        this._historyIndex = -1;

        global.settings.connect('changed::' + settings_key,
                                Lang.bind(this, this._historyChanged));
    },

    _historyChanged: function() {
        this._history = global.settings.get_strv(this._key);
        this._historyIndex = this._history.length;
    },

    prevItem: function(text) {
        this._setHistory(this._historyIndex--, text);
        return this._indexChanged();
    },

    nextItem: function(text) {
        this._setHistory(this._historyIndex++, text);
        return this._indexChanged();
    },

    lastItem: function() {
        this._historyIndex = this._history.length;
        return this._indexChanged();
    },

    addItem: function(input) {
        if (this._history.length == 0 ||
            this._history[this._history.length - 1] != input) {

            this._history.push(input);
            this._save();
        }   
    },

    _indexChanged: function() {
        let current = this._history[this._historyIndex] || '';
        this.emit('changed', current);
        return current;
    },

    _setHistory: function(index, text) {
        this._historyIndex = Math.max(this._historyIndex, 0);
        this._historyIndex = Math.min(this._historyIndex, this._history.length);

        if (text)
            this._history[index] = text;
    },

    _save: function() {
        if (this._history.length > this._limit)
            this._history.splice(0, this._history.length - this._key);
        global.settings.set_strv(this._key, this._history);
    }
};
Signals.addSignalMethods(HistoryManager.prototype);
