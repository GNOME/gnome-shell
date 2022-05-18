/* exported main */

const Config = imports.misc.config;
const { DBusService } = imports.dbusService;

function main() {
    if (!Config.HAVE_RECORDER)
        return;

    const { ScreencastService } = imports.screencastService;
    const service = new DBusService(
        'org.gnome.Shell.Screencast',
        new ScreencastService());
    service.run();
}
