import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {setConsoleLogDomain} from 'console';
const Package = imports.package;

Package.initFormat();

import {ExtensionManager} from './extensionManager.js';
import {ExtensionsWindow} from './extensionsWindow.js';

var Application = GObject.registerClass(
class Application extends Adw.Application {
    _init() {
        GLib.set_prgname('gnome-extensions-app');
        super._init({application_id: Package.name});

        this.connect('window-removed', (a, window) => window.run_dispose());
    }

    get extensionManager() {
        return this._extensionManager;
    }

    vfunc_activate() {
        this._extensionManager.checkForUpdates();
        this._window.present();
    }

    vfunc_startup() {
        super.vfunc_startup();

        this.add_action_entries(
            [{
                name: 'quit',
                activate: () => this._window.close(),
            }]);

        this.set_accels_for_action('app.quit', ['<Primary>q']);

        this._extensionManager = new ExtensionManager();

        this._window = new ExtensionsWindow({application: this});
    }
});

/**
 * Main entrypoint for the app
 *
 * @param {string[]} argv - command line arguments
 * @returns {void}
 */
export async function main(argv) {
    Package.initGettext();
    setConsoleLogDomain('Extensions');

    await new Application().runAsync(argv);
}
