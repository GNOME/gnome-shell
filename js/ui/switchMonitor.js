import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import St from 'gi://St';

import * as SwitcherPopup from './switcherPopup.js';

const APP_ICON_SIZE = 96;

export const SwitchMonitorPopup = GObject.registerClass(
class SwitchMonitorPopup extends SwitcherPopup.SwitcherPopup {
    _init() {
        const items = [];

        items.push({
            icon: 'shell-display-mirror-symbolic',
            /* Translators: this is for display mirroring i.e. cloning.
             * Try to keep it under around 15 characters.
             */
            label: _('Mirror'),
            configType: Meta.MonitorSwitchConfigType.ALL_MIRROR,
        });

        items.push({
            icon: 'shell-display-extend-all-symbolic',
            /* Translators: this is for the desktop spanning displays.
             * Try to keep it under around 15 characters.
             */
            label: _('Join Displays'),
            configType: Meta.MonitorSwitchConfigType.ALL_LINEAR,
        });

        if (global.backend.get_monitor_manager().has_builtin_panel) {
            items.push({
                icon: 'shell-display-external-only-symbolic',
                /* Translators: this is for using only external displays.
                 * Try to keep it under around 15 characters.
                 */
                label: _('External Only'),
                configType: Meta.MonitorSwitchConfigType.EXTERNAL,
            });
            items.push({
                icon: 'shell-display-built-in-only-symbolic',
                /* Translators: this is for using only the laptop display.
                 * Try to keep it under around 15 characters.
                 */
                label: _('Built-in Only'),
                configType: Meta.MonitorSwitchConfigType.BUILTIN,
            });
        }

        super._init(items);

        this._switcherList = new SwitchMonitorSwitcher(items);
    }

    show(backward, binding, mask) {
        if (!global.backend.get_monitor_manager().can_switch_config())
            return false;

        return super.show(backward, binding, mask);
    }

    _initialSelection() {
        const currentConfig = global.backend.get_monitor_manager().get_switch_config();
        const selectConfig = (currentConfig + 1) % this._items.length;
        this._select(selectConfig);
    }

    _keyPressHandler(keysym, action) {
        if (action === Meta.KeyBindingAction.SWITCH_MONITOR)
            this._select(this._next());
        else if (keysym === Clutter.KEY_Left)
            this._select(this._previous());
        else if (keysym === Clutter.KEY_Right)
            this._select(this._next());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    }

    _finish() {
        super._finish();

        const monitorManager = global.backend.get_monitor_manager();
        const item = this._items[this._selectedIndex];

        monitorManager.switch_config(item.configType);
    }
});

const SwitchMonitorSwitcher = GObject.registerClass(
class SwitchMonitorSwitcher extends SwitcherPopup.SwitcherList {
    _init(items) {
        super._init(true);

        for (let i = 0; i < items.length; i++)
            this._addIcon(items[i]);
    }

    _addIcon(item) {
        const box = new St.BoxLayout({
            style_class: 'alt-tab-app',
            orientation: Clutter.Orientation.VERTICAL,
        });

        const icon = new St.Icon({
            icon_name: item.icon,
            icon_size: APP_ICON_SIZE,
        });
        box.add_child(icon);

        const text = new St.Label({
            text: item.label,
            x_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(text);

        this.addItem(box, text);
    }
});
