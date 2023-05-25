import Adw from 'gi://Adw?version=1';
import GObject from 'gi://GObject';
const pkg = imports.package;

import {DBusService} from './dbusService.js';
import {ExtensionsService} from './extensionsService.js';

/** @returns {void} */
export async function main() {
    Adw.init();
    pkg.initFormat();

    GObject.gtypeNameBasedOnJSPath = true;

    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    await service.runAsync();
}
