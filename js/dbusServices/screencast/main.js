import {DBusService} from './dbusService.js';
import {ScreencastService} from './screencastService.js';

/** @returns {void} */
export async function main() {
    if (!ScreencastService.canScreencast())
        return;

    const service = new DBusService(
        'org.gnome.Shell.Screencast',
        new ScreencastService());
    await service.runAsync();
}
