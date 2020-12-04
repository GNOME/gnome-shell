/* exported main */

const { DBusService } = imports.dbusService;
const { ScreenSaverService } = imports.screenSaverService;

function main() {
    const service = new DBusService(
        'org.gnome.ScreenSaver',
        new ScreenSaverService());
    service.run();
}
