/* exported main */

const { DBusService } = imports.dbusService;
const { ScreencastService } = imports.screencastService;

function main() {
    const service = new DBusService(
        'org.gnome.Shell.Screencast',
        new ScreencastService());
    service.run();
}
