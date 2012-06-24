// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const St = imports.gi.St;

const Environment = imports.ui.environment;

function init(stage) {
    Environment.init();
    let context = St.ThemeContext.get_for_stage(stage);
    let stylesheetPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/test.css";
    let theme = new St.Theme({ application_stylesheet: stylesheetPath });
    context.set_theme(theme);
}

function main(stage) {
    stage.show();
    stage.connect('destroy', function() {
        Clutter.main_quit();
    });
    Clutter.main();
}
