// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

var RemoteAccessApplet = new Lang.Class({
    Name: 'RemoteAccessApplet',
    Extends: PanelMenu.SystemIndicator,

    _init(controller) {
        this.parent();

        this._handles = new Set();
        this._indicator = null;
        this._menuSection = null;

        controller.connect('new-handle', (controller, handle) => {
            this._onNewHandle(handle);
        });
    },

    _turnOffSharings() {
        for (let handle of this._handles) {
            handle.stop();
        }
    },

    _ensureControls() {
        if (this._indicator)
            return;

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'screen-shared-symbolic';
        this._item =
            new PopupMenu.PopupSubMenuMenuItem(_("Screen is Being Shared"),
                                               true);
        this._item.menu.addAction(_("Turn off"),
                                  this._turnOffSharings.bind(this));
        this._item.icon.icon_name = 'screen-shared-symbolic';
        this.menu.addMenuItem(this._item);
    },

    _onStopped(handle) {
        this._handles.delete(handle);
        if (this._handles.size == 0) {
            this._indicator.visible = false;
            this._item.actor.visible = false;
        }
    },

    _onNewHandle(handle) {
        this._handles.add(handle);
        handle.connect('stopped', this._onStopped.bind(this));

        if (this._handles.size == 1) {
            this._ensureControls();
            this._indicator.visible = true;
            this._item.actor.visible = true;
        }
    },
});
