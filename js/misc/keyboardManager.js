// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Meta = imports.gi.Meta;

const Main = imports.ui.main;

const DEFAULT_LOCALE = 'en_US';
const DEFAULT_LAYOUT = 'us';
const DEFAULT_VARIANT = '';

let _xkbInfo = null;

function getXkbInfo() {
    if (_xkbInfo == null)
        _xkbInfo = new GnomeDesktop.XkbInfo();
    return _xkbInfo;
}

let _keyboardManager = null;

function getKeyboardManager() {
    if (_keyboardManager == null)
        _keyboardManager = new KeyboardManager();
    return _keyboardManager;
}

function releaseKeyboard() {
    if (Main.modalCount > 0)
        global.display.unfreeze_keyboard(global.get_current_time());
    else
        global.display.ungrab_keyboard(global.get_current_time());
}

function holdKeyboard() {
    global.display.freeze_keyboard(global.get_current_time());
}

const KeyboardManager = new Lang.Class({
    Name: 'KeyboardManager',

    // The XKB protocol doesn't allow for more that 4 layouts in a
    // keymap. Wayland doesn't impose this limit and libxkbcommon can
    // handle up to 32 layouts but since we need to support X clients
    // even as a Wayland compositor, we can't bump this.
    MAX_LAYOUTS_PER_GROUP: 4,

    _init: function() {
        this._xkbInfo = getXkbInfo();
        this._current = null;
        this._localeLayoutInfo = this._getLocaleLayout();
        this._layoutInfos = {};
    },

    _applyLayoutGroup: function(group) {
        let options = this._buildOptionsString();
        let [layouts, variants] = this._buildGroupStrings(group);
        Meta.get_backend().set_keymap(layouts, variants, options);
    },

    _applyLayoutGroupIndex: function(idx) {
        Meta.get_backend().lock_layout_group(idx);
    },

    apply: function(id) {
        let info = this._layoutInfos[id];
        if (!info)
            return;

        if (this._current && this._current.group == info.group) {
            if (this._current.groupIndex != info.groupIndex)
                this._applyLayoutGroupIndex(info.groupIndex);
        } else {
            this._applyLayoutGroup(info.group);
            this._applyLayoutGroupIndex(info.groupIndex);
        }

        this._current = info;
    },

    reapply: function() {
        if (!this._current)
            return;

        this._applyLayoutGroup(this._current.group);
        this._applyLayoutGroupIndex(this._current.groupIndex);
    },

    setUserLayouts: function(ids) {
        this._current = null;
        this._layoutInfos = {};

        for (let i = 0; i < ids.length; ++i) {
            let [found, , , _layout, _variant] = this._xkbInfo.get_layout_info(ids[i]);
            if (found)
                this._layoutInfos[ids[i]] = { id: ids[i], layout: _layout, variant: _variant };
        }

        let i = 0;
        let group = [];
        for (let id in this._layoutInfos) {
            // We need to leave one slot on each group free so that we
            // can add a layout containing the symbols for the
            // language used in UI strings to ensure that toolkits can
            // handle mnemonics like Alt+Ф even if the user is
            // actually typing in a different layout.
            let groupIndex = i % (this.MAX_LAYOUTS_PER_GROUP - 1);
            if (groupIndex == 0)
                group = [];

            let info = this._layoutInfos[id];
            group[groupIndex] = info;
            info.group = group;
            info.groupIndex = groupIndex;

            i += 1;
        }
    },

    _getLocaleLayout: function() {
        let locale = GLib.get_language_names()[0];
        if (locale.indexOf('_') == -1)
            locale = DEFAULT_LOCALE;

        let [found, , id] = GnomeDesktop.get_input_source_from_locale(locale);
        if (!found)
            [, , id] = GnomeDesktop.get_input_source_from_locale(DEFAULT_LOCALE);

        let [found, , , _layout, _variant] = this._xkbInfo.get_layout_info(id);
        if (found)
            return { layout: _layout, variant: _variant };
        else
            return { layout: DEFAULT_LAYOUT, variant: DEFAULT_VARIANT };
    },

    _buildGroupStrings: function(_group) {
        let group = _group.concat(this._localeLayoutInfo);
        let layouts = group.map(function(g) { return g.layout; }).join(',');
        let variants = group.map(function(g) { return g.variant; }).join(',');
        return [layouts, variants];
    },

    setKeyboardOptions: function(options) {
        this._xkbOptions = options;
    },

    _buildOptionsString: function() {
        let options = this._xkbOptions.join(',');
        return options;
    }
});
