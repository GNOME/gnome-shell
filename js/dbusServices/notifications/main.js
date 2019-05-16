/* exported main */

const { DBusService } = imports.dbusService;
const { NotificationDaemon } = imports.notificationDaemon;

function main() {
    const service = new DBusService(
        'org.gnome.Shell.Notifications',
        new NotificationDaemon());
    service.run();
}
