/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const St = imports.gi.St;

const Environment = imports.ui.environment;

function init() {
    Clutter.init(null, null);
    Environment.init();

    let style = St.Style.get_default();
    let stylesheetPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/test.css";
    style.load_from_file(stylesheetPath);
}
