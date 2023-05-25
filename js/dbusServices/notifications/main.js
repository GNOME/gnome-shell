import {DBusService} from './dbusService.js';
import {NotificationDaemon} from './notificationDaemon.js';

/** @returns {void} */
export async function main() {
    const service = new DBusService(
        'org.gnome.Shell.Notifications',
        new NotificationDaemon());
    await service.runAsync();
}
