/* exported main */

imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const { Gtk } = imports.gi;

const Format = imports.format;

const { DBusService } = imports.dbusService;
const { ExtensionsService } = imports.extensionsService;

function initEnvironment() {
    String.prototype.format = Format.format;
}

function main() {
    Gtk.init(null);
    initEnvironment();

    const service = new DBusService(
        'org.gnome.Shell.Extensions',
        new ExtensionsService());
    service.run();
}
