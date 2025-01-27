import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from './main.js';
import * as SwitcherPopup from './switcherPopup.js';
import * as Params from '../misc/params.js';

const POPUP_APPICON_SIZE = 96;

export const SortGroup = {
    TOP:    0,
    MIDDLE: 1,
    BOTTOM: 2,
};

export class CtrlAltTabManager {
    constructor() {
        this._items = [];
        this.addGroup(global.window_group,
            _('Windows'),
            'shell-focus-windows-symbolic', {
                sortGroup: SortGroup.TOP,
                focusCallback: this._focusWindows.bind(this),
            });
    }

    addGroup(root, name, icon, params) {
        const item = Params.parse(params, {
            sortGroup: SortGroup.MIDDLE,
            proxy: root,
            focusCallback: null,
        });

        item.root = root;
        item.name = name;
        item.iconName = icon;

        this._items.push(item);
        root.connect('destroy', () => this.removeGroup(root));
        if (root instanceof St.Widget)
            global.focus_manager.add_group(root);
    }

    removeGroup(root) {
        if (root instanceof St.Widget)
            global.focus_manager.remove_group(root);
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].root === root) {
                this._items.splice(i, 1);
                return;
            }
        }
    }

    focusGroup(item, timestamp) {
        if (item.focusCallback)
            item.focusCallback(timestamp);
        else
            item.root.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
    }

    // Sort the items into a consistent order; panel first, tray last,
    // and everything else in between, sorted by X coordinate, so that
    // they will have the same left-to-right ordering in the
    // Ctrl-Alt-Tab dialog as they do onscreen.
    _sortItems(a, b) {
        if (a.sortGroup !== b.sortGroup)
            return a.sortGroup - b.sortGroup;

        let [ax] = a.proxy.get_transformed_position();
        let [bx] = b.proxy.get_transformed_position();

        return ax - bx;
    }

    popup(backward, binding, mask) {
        // Start with the set of focus groups that are currently mapped
        let items = this._items.filter(item => item.proxy.mapped);

        // And add the windows metacity would show in its Ctrl-Alt-Tab list
        if (Main.sessionMode.hasWindows && !Main.overview.visible) {
            let display = global.display;
            let workspaceManager = global.workspace_manager;
            let activeWorkspace = workspaceManager.get_active_workspace();
            let windows = display.get_tab_list(Meta.TabList.DOCKS,
                activeWorkspace);
            let windowTracker = Shell.WindowTracker.get_default();
            for (let i = 0; i < windows.length; i++) {
                let icon = null;
                let iconName = null;
                if (windows[i].get_window_type() === Meta.WindowType.DESKTOP) {
                    iconName = 'shell-focus-desktop-symbolic';
                } else {
                    const app = windowTracker.get_window_app(windows[i]);
                    icon = app.create_icon_texture(POPUP_APPICON_SIZE);
                }

                items.push({
                    name: windows[i].title,
                    proxy: windows[i].get_compositor_private(),
                    focusCallback: timestamp => {
                        Main.activateWindow(windows[i], timestamp);
                    },
                    iconActor: icon,
                    iconName,
                    sortGroup: SortGroup.MIDDLE,
                });
            }
        }

        if (!items.length)
            return;

        items.sort(this._sortItems.bind(this));

        if (!this._popup) {
            this._popup = new CtrlAltTabPopup(items);
            this._popup.show(backward, binding, mask);

            this._popup.connect('destroy', () => {
                this._popup = null;
            });
        }
    }

    _focusWindows(timestamp) {
        global.display.focus_default_window(timestamp);
    }
}

const CtrlAltTabPopup = GObject.registerClass(
class CtrlAltTabPopup extends SwitcherPopup.SwitcherPopup {
    _init(items) {
        super._init(items);

        this._switcherList = new CtrlAltTabSwitcher(this._items);
    }

    _keyPressHandler(keysym, action) {
        if (action === Meta.KeyBindingAction.SWITCH_PANELS)
            this._select(this._next());
        else if (action === Meta.KeyBindingAction.SWITCH_PANELS_BACKWARD)
            this._select(this._previous());
        else if (keysym === Clutter.KEY_Left)
            this._select(this._previous());
        else if (keysym === Clutter.KEY_Right)
            this._select(this._next());
        else
            return Clutter.EVENT_PROPAGATE;

        return Clutter.EVENT_STOP;
    }

    _finish(time) {
        super._finish(time);
        Main.ctrlAltTabManager.focusGroup(this._items[this._selectedIndex], time);
    }
});

const CtrlAltTabSwitcher = GObject.registerClass(
class CtrlAltTabSwitcher extends SwitcherPopup.SwitcherList {
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

        let icon = item.iconActor;
        if (!icon) {
            icon = new St.Icon({
                icon_name: item.iconName,
                icon_size: POPUP_APPICON_SIZE,
            });
        }
        box.add_child(icon);

        let text = new St.Label({
            text: item.name,
            x_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(text);

        this.addItem(box, text);
    }
});
