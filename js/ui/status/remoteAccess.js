// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported RemoteAccessApplet */

const { GObject, Meta } = imports.gi;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

var RemoteAccessApplet = GObject.registerClass(
class RemoteAccessApplet extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        let backend = Meta.get_backend();
        let controller = backend.get_remote_access_controller();

        if (!controller)
            return;

        // We can't possibly know about all types of screen sharing on X11, so
        // showing these controls on X11 might give a false sense of security.
        // Thus, only enable these controls when using Wayland, where we are
        // in control of sharing.
        if (!Meta.is_wayland_compositor())
            return;

        this._handles = new Set();
        this._indicator = null;
        this._menuSection = null;

        controller.connect('new-handle', (o, handle) => {
            this._onNewHandle(handle);
        });
    }

    _ensureControls() {
        if (this._indicator)
            return;

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'screen-shared-symbolic';
        this._indicator.add_style_class_name('remote-access-indicator');
        this._item =
            new PopupMenu.PopupSubMenuMenuItem(_("Screen is Being Shared"),
                                               true);
        this._item.menu.addAction(_("Turn off"),
                                  () => {
                                      for (let handle of this._handles)
                                          handle.stop();
                                  });
        this._item.icon.icon_name = 'screen-shared-symbolic';
        this.menu.addMenuItem(this._item);
    }

    _sync() {
        if (this._handles.size == 0) {
            this._indicator.visible = false;
            this._item.visible = false;
        } else {
            this._indicator.visible = true;
            this._item.visible = true;
        }
    }

    _onStopped(handle) {
        this._handles.delete(handle);
        this._sync();
    }

    _onNewHandle(handle) {
        this._handles.add(handle);
        handle.connect('stopped', this._onStopped.bind(this));

        if (this._handles.size == 1) {
            this._ensureControls();
            this._sync();
        }
    }
});
