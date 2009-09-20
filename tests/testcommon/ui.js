/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Nbtk = imports.gi.Nbtk;
const Shell = imports.gi.Shell;

const Environment = imports.ui.environment;

function init() {
    Clutter.init(null, null);
    Environment.init();

    let stage = Clutter.Stage.get_default();
    let context = Shell.ThemeContext.get_for_stage (stage);
    let stylesheetPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/test.css";
    let theme = new Shell.Theme ({ application_stylesheet: stylesheetPath });
    context.set_theme (theme);
}
