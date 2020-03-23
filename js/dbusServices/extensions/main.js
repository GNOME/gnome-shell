/* exported main */

imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const { Gtk } = imports.gi;
const pkg = imports.package;

const { DBusService } = imports.dbusService;
const { ExtensionsService } = imports.extensionsService;

function main() {
    Gtk.init(null);
    pkg.initFormat();

    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    service.run();
}
