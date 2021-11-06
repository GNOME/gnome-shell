/* exported main */

imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { GObject, Gtk } = imports.gi;
const pkg = imports.package;

const { DBusService } = imports.dbusService;
const { ExtensionsService } = imports.extensionsService;

function main() {
    Gtk.init();
    pkg.initFormat();

    GObject.gtypeNameBasedOnJSPath = true;

    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    service.run();
}
