/* exported main */

const { DBusService } = imports.dbusService;
const { ExtensionsService } = imports.extensionsService;

function main() {
    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    service.run();
}
