import GLib from 'gi://GLib';
import GnomeDesktop from 'gi://GnomeDesktop';
import Meta from 'gi://Meta';

import * as Signals from './signals.js';

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

class KeyboardManager extends Signals.EventEmitter {
    constructor() {
        super();

        // The XKB protocol doesn't allow for more than 4 layouts in a
        // keymap. Wayland doesn't impose this limit and libxkbcommon can
        // handle up to 32 layouts but since we need to support X clients
        // even as a Wayland compositor, we can't bump this.
        this.MAX_LAYOUTS_PER_GROUP = 4;

        this._xkbInfo = getXkbInfo();
        this._current = null;
        this._localeLayoutInfo = this._getLocaleLayout();
        this._layoutInfos = {};
        this._currentKeymap = null;

        global.backend.connect('keymap-changed', this._onKeymapChanged.bind(this));
        global.backend.connect('keymap-layout-group-changed', this._onKeymapLayoutGroupChanged.bind(this));
        global.backend.connect('reset-keymap-description',
            () => this._ourKeymapDescription);
        global.backend.connect('reset-keymap-layout-index',
            () => this._current.groupIndex);
    }

    _updateCurrentKeymap(info) {
        const options = this._buildOptionsString();
        const [layouts, variants] = this._buildGroupStrings(info.group);
        const model = this._xkbModel;

        if (this._currentKeymap &&
            this._currentKeymap.layouts === layouts &&
            this._currentKeymap.variants === variants &&
            this._currentKeymap.options === options &&
            this._currentKeymap.model === model)
            return false;

        const displayNames = info.group.map(g => g.displayName);
        const shortNames = info.group.map(g => g.shortName);
        this._currentKeymap = {
            layouts,
            variants,
            options,
            model,
            displayNames,
            shortNames,
        };
        return true;
    }

    _createKeymapDescription() {
        return Meta.KeymapDescription.new_from_rules(this._currentKeymap.model,
            this._currentKeymap.layouts,
            this._currentKeymap.variants,
            this._currentKeymap.options,
            this._currentKeymap.displayNames,
            this._currentKeymap.shortNames
        );
    }

    _onKeymapChanged() {
        this._keymapDescription = global.backend.get_keymap_description();
        this.emit('keymap-changed');
    }

    _onKeymapLayoutGroupChanged() {
        this.emit('keymap-changed');
    }

    async _doApply(id) {
        const info = this._layoutInfos[id];
        if (!info)
            return;

        let recreate;
        if (this._updateCurrentKeymap(info))
            recreate = true;
        else if (this.isExternal())
            recreate = true;
        else
            recreate = false;

        if (recreate)
            this._ourKeymapDescription = this._createKeymapDescription();

        if (recreate || !this._current || this._current.groupIndex !== info.groupIndex) {
            await global.backend.set_keymap_async(
                this._ourKeymapDescription, info.groupIndex, null);
        }

        this._current = info;
    }

    apply(id) {
        this._doApply(id).catch(logError);
    }

    reapply() {
        if (!this._current)
            return;

        this._doApply(this._current).catch(logError);
    }

    setUserLayouts(ids) {
        this._current = null;
        this._layoutInfos = {};

        for (const id of ids) {
            const [found, displayName, shortName, layout, variant] =
                this._xkbInfo.get_layout_info(id);
            if (found) {
                this._layoutInfos[id] = {
                    id,
                    layout,
                    variant,
                    displayName,
                    shortName,
                };
            }
        }

        let i = 0;
        let group = [];
        for (const id in this._layoutInfos) {
            // We need to leave one slot on each group free so that we
            // can add a layout containing the symbols for the
            // language used in UI strings to ensure that toolkits can
            // handle mnemonics like Alt+Ф even if the user is
            // actually typing in a different layout.
            const groupIndex = i % (this.MAX_LAYOUTS_PER_GROUP - 1);
            if (groupIndex === 0)
                group = [];

            const info = this._layoutInfos[id];
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

        let layout, variant;
        [found, , , layout, variant] = this._xkbInfo.get_layout_info(id);
        if (found)
            return {layout, variant};
        else
            return {layout: DEFAULT_LAYOUT, variant: DEFAULT_VARIANT};
    }

    _buildGroupStrings(_group) {
        const group = _group.concat(this._localeLayoutInfo);
        const layouts = group.map(g => g.layout).join(',');
        const variants = group.map(g => g.variant).join(',');
        return [layouts, variants];
    }

    setKeyboardOptions(options) {
        this._xkbOptions = options;
    }

    setKeyboardModel(model) {
        this._xkbModel = model;
    }

    _buildOptionsString() {
        const options = this._xkbOptions.join(',');
        return options;
    }

    get currentLayout() {
        return this._current;
    }

    get shortName() {
        const seat = global.stage.context.get_backend().get_default_seat();
        const keymap = seat.get_keymap();
        return keymap.get_current_short_name();
    }

    get displayName() {
        const seat = global.stage.context.get_backend().get_default_seat();
        const keymap = seat.get_keymap();
        return keymap.get_current_display_name();
    }

    isLocked() {
        return this._keymapDescription?.is_locked() ?? false;
    }

    isExternal() {
        if (!this._keymapDescription)
            return false;
        if (!this._ourKeymapDescription)
            return true;
        return !this._keymapDescription.direct_equal(this._ourKeymapDescription);
    }
}
