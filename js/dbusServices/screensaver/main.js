import {DBusService} from './dbusService.js';
import {ScreenSaverService} from './screenSaverService.js';

/** @returns {void} */
export async function main() {
    const service = new DBusService(
        'org.gnome.ScreenSaver',
        new ScreenSaverService());
    await service.runAsync();
}
