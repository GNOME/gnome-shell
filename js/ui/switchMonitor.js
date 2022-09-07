// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported SwitchMonitorPopup */

const { Clutter, GObject, Meta, St } = imports.gi;

const SwitcherPopup = imports.ui.switcherPopup;

var APP_ICON_SIZE = 96;

var SwitchMonitorPopup = GObject.registerClass(
class SwitchMonitorPopup extends SwitcherPopup.SwitcherPopup {
    _init() {
        let items = [];

        items.push({
            icon: 'view-mirror-symbolic',
            /* Translators: this is for display mirroring i.e. cloning.
             * Try to keep it under around 15 characters.
             */
            label: _('Mirror'),
            configType: Meta.MonitorSwitchConfigType.ALL_MIRROR,
        });

        items.push({
            icon: 'video-joined-displays-symbolic',
            /* Translators: this is for the desktop spanning displays.
             * Try to keep it under around 15 characters.
             */
            label: _('Join Displays'),
            configType: Meta.MonitorSwitchConfigType.ALL_LINEAR,
        });

        if (global.backend.get_monitor_manager().has_builtin_panel) {
            items.push({
                icon: 'video-single-display-symbolic',
                /* Translators: this is for using only an external display.
                 * Try to keep it under around 15 characters.
                 */
                label: _('External Only'),
                configType: Meta.MonitorSwitchConfigType.EXTERNAL,
            });
            items.push({
                icon: 'computer-symbolic',
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
        let currentConfig = global.backend.get_monitor_manager().get_switch_config();
        let selectConfig = (currentConfig + 1) % this._items.length;
        this._select(selectConfig);
    }

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.SWITCH_MONITOR)
            this._select(this._next());
        else if (keysym == Clutter.KEY_Left)
            this._select(this._previous());
        else if (keysym == Clutter.KEY_Right)
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

var SwitchMonitorSwitcher = GObject.registerClass(
class SwitchMonitorSwitcher extends SwitcherPopup.SwitcherList {
    _init(items) {
        super._init(true);

        for (let i = 0; i < items.length; i++)
            this._addIcon(items[i]);
    }

    _addIcon(item) {
        const box = new St.BoxLayout({
            style_class: 'alt-tab-app',
            vertical: true,
        });

        const icon = new St.Icon({
            icon_name: item.icon,
            icon_size: APP_ICON_SIZE,
        });
        box.add_child(icon);

        let text = new St.Label({
            text: item.label,
            x_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(text);

        this.addItem(box, text);
    }
});
