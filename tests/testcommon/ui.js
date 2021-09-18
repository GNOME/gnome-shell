// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import 'gi://Clutter?version=9';
import 'gi://Gtk?version=3.0';

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import St from 'gi://St';

import * as Environment from '../../js/ui/environment.js';

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
