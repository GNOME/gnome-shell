// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Config = imports.misc.config;

imports.gi.versions = { Clutter: Config.LIBMUTTER_API_VERSION, Gtk: '3.0' };

const { Clutter, Gio, GLib, St } = imports.gi;

const Environment = imports.ui.environment;

function init(stage) {
    Environment.init();
    let themeResource = Gio.Resource.load(global.datadir + '/gnome-shell-theme.gresource');
    themeResource._register();

    let context = St.ThemeContext.get_for_stage(stage);
    let stylesheetPath = GLib.getenv("GNOME_SHELL_TESTSDIR") + "/testcommon/test.css";
    let theme = new St.Theme({ application_stylesheet: Gio.File.new_for_path(stylesheetPath) });
    context.set_theme(theme);
}

function main(stage) {
    stage.show();
    stage.connect('destroy', () => {
        Clutter.main_quit();
    });
    Clutter.main();
}
