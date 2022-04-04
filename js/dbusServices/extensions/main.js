/* exported main */

imports.gi.versions.Adw = '1';
imports.gi.versions.Gdk = '4.0';
imports.gi.versions.Gtk = '4.0';

const { Adw, GObject } = imports.gi;
const pkg = imports.package;

const { DBusService } = imports.dbusService;
const { ExtensionsService } = imports.extensionsService;

function main() {
    Adw.init();
    pkg.initFormat();

    GObject.gtypeNameBasedOnJSPath = true;

    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    service.run();
}
