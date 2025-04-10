import GLib from 'gi://GLib';
import GnomeDesktop from 'gi://GnomeDesktop';

import * as Main from '../ui/main.js';

export const DEFAULT_LOCALE = 'en_US';
export const DEFAULT_LAYOUT = 'us';
export const DEFAULT_VARIANT = '';

let _xkbInfo = null;

/**
 * @returns {GnomeDesktop.XkbInfo}
 */
export function getXkbInfo() {
    if (_xkbInfo == null)
        _xkbInfo = new GnomeDesktop.XkbInfo();
    return _xkbInfo;
}

let _keyboardManager = null;

/**
 * @returns {KeyboardManager}
 */
export function getKeyboardManager() {
    if (_keyboardManager == null)
        _keyboardManager = new KeyboardManager();
    return _keyboardManager;
}

export function releaseKeyboard() {
    if (Main.modalCount > 0)
        global.backend.unfreeze_keyboard(global.get_current_time());
    else
        global.backend.ungrab_keyboard(global.get_current_time());
}

export function holdKeyboard() {
    global.backend.freeze_keyboard(global.get_current_time());
}

class KeyboardManager {
    constructor() {
        // The XKB protocol doesn't allow for more that 4 layouts in a
        // keymap. Wayland doesn't impose this limit and libxkbcommon can
        // handle up to 32 layouts but since we need to support X clients
        // even as a Wayland compositor, we can't bump this.
        this.MAX_LAYOUTS_PER_GROUP = 4;

        this._xkbInfo = getXkbInfo();
        this._current = null;
        this._localeLayoutInfo = this._getLocaleLayout();
        this._layoutInfos = {};
        this._currentKeymap = null;
    }

    async _applyLayoutGroup(group) {
        let options = this._buildOptionsString();
        let [layouts, variants] = this._buildGroupStrings(group);
        let model = this._xkbModel;

        if (this._currentKeymap &&
            this._currentKeymap.layouts === layouts &&
            this._currentKeymap.variants === variants &&
            this._currentKeymap.options === options &&
            this._currentKeymap.model === model)
            return;

        this._currentKeymap = {layouts, variants, options, model};
        await global.backend.set_keymap_async(layouts, variants, options, model, null);
    }

    async _applyLayoutGroupIndex(idx) {
        await global.backend.set_keymap_layout_group_async(idx, null);
    }

    async _doApply(info) {
        await this._applyLayoutGroup(info.group);
        await this._applyLayoutGroupIndex(info.groupIndex);
    }

    apply(id) {
        let info = this._layoutInfos[id];
        if (!info)
            return;

        if (this._current && this._current.group === info.group) {
            if (this._current.groupIndex !== info.groupIndex)
                this._applyLayoutGroupIndex(info.groupIndex).catch(logError);
        } else {
            this._doApply(info).catch(logError);
        }

        this._current = info;
    }

    reapply() {
        if (!this._current)
            return;

        this._doApply(this._current).catch(logError);
    }

    setUserLayouts(ids) {
        this._current = null;
        this._layoutInfos = {};

        for (let i = 0; i < ids.length; ++i) {
            let [found, , , _layout, _variant] = this._xkbInfo.get_layout_info(ids[i]);
            if (found)
                this._layoutInfos[ids[i]] = {id: ids[i], layout: _layout, variant: _variant};
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
            if (groupIndex === 0)
                group = [];

            let info = this._layoutInfos[id];
            group[groupIndex] = info;
            info.group = group;
            info.groupIndex = groupIndex;

            i += 1;
        }
    }

    _getLocaleLayout() {
        let locale = GLib.get_language_names()[0];
        if (!locale.includes('_'))
            locale = DEFAULT_LOCALE;

        let [found, , id] = GnomeDesktop.get_input_source_from_locale(locale);
        if (!found)
            [, , id] = GnomeDesktop.get_input_source_from_locale(DEFAULT_LOCALE);

        let _layout, _variant;
        [found, , , _layout, _variant] = this._xkbInfo.get_layout_info(id);
        if (found)
            return {layout: _layout, variant: _variant};
        else
            return {layout: DEFAULT_LAYOUT, variant: DEFAULT_VARIANT};
    }

    _buildGroupStrings(_group) {
        let group = _group.concat(this._localeLayoutInfo);
        let layouts = group.map(g => g.layout).join(',');
        let variants = group.map(g => g.variant).join(',');
        return [layouts, variants];
    }

    setKeyboardOptions(options) {
        this._xkbOptions = options;
    }

    setKeyboardModel(model) {
        this._xkbModel = model;
    }

    _buildOptionsString() {
        let options = this._xkbOptions.join(',');
        return options;
    }

    get currentLayout() {
        return this._current;
    }
}
