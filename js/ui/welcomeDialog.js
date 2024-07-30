import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Config from '../misc/config.js';
import * as Main from './main.js';
import * as Dialog from './dialog.js';
import * as ModalDialog from './modalDialog.js';

/** @enum {number} */
const DialogResponse = {
    SKIP: 0,
    TAKE_TOUR: 1,
};

export const WelcomeDialog = GObject.registerClass(
class WelcomeDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({styleClass: 'welcome-dialog'});

        const appSystem = Shell.AppSystem.get_default();
        this._tourAppInfo = appSystem.lookup_app('org.gnome.Tour.desktop');

        this._buildLayout();
    }

    open() {
        if (!this._tourAppInfo)
            return false;

        return super.open();
    }

    _getOSName() {
        const prettyName = GLib.get_os_info('PRETTY_NAME');
        if (prettyName)
            return prettyName;

        const name = GLib.get_os_info('NAME');
        const version = GLib.get_os_info('VERSION');
        if (name)
            return version ? `${name} ${version}` : name;

        const [majorVersion] = Config.PACKAGE_VERSION.split('.');
        return _('GNOME %s').format(majorVersion);
    }

    _buildLayout() {
        const title = _('Welcome to %s').format(this._getOSName());
        const description = _('If you want to learn your way around, check out the tour');
        const content = new Dialog.MessageDialogContent({title, description});

        const icon = new St.Widget({style_class: 'welcome-dialog-image'});
        content.insert_child_at_index(icon, 0);

        this.contentLayout.add_child(content);

        this.addButton({
            label: _('Skip'),
            action: () => this._sendResponse(DialogResponse.SKIP),
            key: Clutter.KEY_Escape,
        });
        this.addButton({
            label: _('Take Tour'),
            action: () => this._sendResponse(DialogResponse.TAKE_TOUR),
        });
    }

    _sendResponse(response) {
        if (response === DialogResponse.TAKE_TOUR) {
            this._tourAppInfo.launch(0, -1, Shell.AppLaunchGpu.APP_PREF);
            Main.overview.hide();
        }

        this.close();
    }
});
