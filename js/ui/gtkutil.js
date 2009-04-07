/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;

function loadIconToTexture(gicon, size, fallback) {
    let iconTheme = Gtk.IconTheme.get_default();

    let path = null;
    let icon = null;
    if (gicon != null) {
        let iconinfo = iconTheme.lookup_by_gicon(gicon, size, Gtk.IconLookupFlags.NO_SVG);
        if (iconinfo)
            path = iconinfo.get_filename();
    }
    if (path) {
        try {
            icon = new Clutter.Texture({ width: size, height: size, load_async: true });
            icon.set_from_file(path);
            return icon;
        } catch (e) {
            icon = null;
        }
    }
    if (icon == null && fallback)
        icon = new Clutter.Texture({ width: size, height: size });
    return icon;
}