/* exported main */

const {DBusService} = imports.dbusService;

function main() {
    const {ScreencastService} = imports.screencastService;
    if (!ScreencastService.canScreencast())
        return;

    const service = new DBusService(
        'org.gnome.Shell.Screencast',
        new ScreencastService());
    service.run();
}
