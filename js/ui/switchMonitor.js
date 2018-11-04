// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;

const SwitcherPopup = imports.ui.switcherPopup;

var APP_ICON_SIZE = 96;

var SwitchMonitorPopup = new Lang.Class({
    Name: 'SwitchMonitorPopup',
    Extends: SwitcherPopup.SwitcherPopup,

    _init() {
        let items = [{ icon:  'view-mirror-symbolic',
                       /* Translators: this is for display mirroring i.e. cloning.
                        * Try to keep it under around 15 characters.
                        */
                       label: _('Mirror') },
                     { icon:  'video-joined-displays-symbolic',
                       /* Translators: this is for the desktop spanning displays.
                        * Try to keep it under around 15 characters.
                        */
                       label: _('Join Displays') },
                     { icon:  'video-single-display-symbolic',
                       /* Translators: this is for using only an external display.
                        * Try to keep it under around 15 characters.
                        */
                       label: _('External Only') },
                     { icon:  'computer-symbolic',
                       /* Translators: this is for using only the laptop display.
                        * Try to keep it under around 15 characters.
                        */
                       label: _('Built-in Only') }];

        this.parent(items);

        this._switcherList = new SwitchMonitorSwitcher(items);
    },

    show(backward, binding, mask) {
        if (!Meta.MonitorManager.get().can_switch_config())
            return false;

        return this.parent(backward, binding, mask);
    },

    _initialSelection() {
        let currentConfig = Meta.MonitorManager.get().get_switch_config();
        let selectConfig = (currentConfig + 1) % Meta.MonitorSwitchConfigType.UNKNOWN;
        this._select(selectConfig);
    },

    _keyPressHandler(keysym, action) {
        if (action == Meta.KeyBindingAction.SWITCH_MONITOR)
            this._select(this._next());
        else if (keysym == Clutter.Left)
            this._select(this._previous());
        else if (keysym == Clutter.Right)
            this._select(this._next());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    },

    _finish() {
        this.parent();

        Meta.MonitorManager.get().switch_config(this._selectedIndex);
    },
});

var SwitchMonitorSwitcher = new Lang.Class({
    Name: 'SwitchMonitorSwitcher',
    Extends: SwitcherPopup.SwitcherList,

    _init(items) {
        this.parent(true);

        for (let i = 0; i < items.length; i++)
            this._addIcon(items[i]);
    },

    _addIcon(item) {
        let box = new St.BoxLayout({ style_class: 'alt-tab-app',
                                     vertical: true });

        let icon = new St.Icon({ icon_name: item.icon,
                                 icon_size: APP_ICON_SIZE });
        box.add(icon, { x_fill: false, y_fill: false } );

        let text = new St.Label({ text: item.label });
        box.add(text, { x_fill: false });

        this.addItem(box, text);
    }
});

