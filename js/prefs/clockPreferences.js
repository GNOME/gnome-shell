/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const GConf = imports.gi.GConf;

const Lang = imports.lang;
const Signals = imports.signals;

const GCONF_DIR        = '/desktop/gnome/shell/clock';
const FORMAT_KEY       = GCONF_DIR + '/format';
const SHOW_DATE_KEY    = GCONF_DIR + '/show_date';
const SHOW_SECONDS_KEY = GCONF_DIR + '/show_seconds';


function ClockPreferences(uiFile) {
    this._init(uiFile);
};

ClockPreferences.prototype = {
    _init: function(uiFile) {
        let builder = new Gtk.Builder();
        builder.add_from_file(uiFile);

        this._dialog = builder.get_object('prefs-dialog');
        this._dialog.connect('response', Lang.bind(this, this._onResponse));

        this._12hrRadio = builder.get_object('12hr_radio');
        this._24hrRadio = builder.get_object('24hr_radio');
        this._dateCheck = builder.get_object('date_check');
        this._secondsCheck = builder.get_object('seconds_check');

        delete builder;

        this._gconf = GConf.Client.get_default();
        this._gconf.add_dir(GCONF_DIR, GConf.ClientPreloadType.PRELOAD_NONE);
        this._notifyId = this._gconf.notify_add(GCONF_DIR,
                                                Lang.bind(this,
                                                          this._updateDialog));

        this._12hrRadio.connect('toggled', Lang.bind(this,
            function() {
                let format = this._12hrRadio.active ? '12-hour' : '24-hour';
                this._gconf.set_string(FORMAT_KEY, format);
            }));
        this._dateCheck.connect('toggled', Lang.bind(this,
            function() {
                this._gconf.set_bool(SHOW_DATE_KEY, this._dateCheck.active);
            }));
        this._secondsCheck.connect('toggled', Lang.bind(this,
            function() {
                this._gconf.set_bool(SHOW_SECONDS_KEY,
                                     this._secondsCheck.active);
            }));

        this._updateDialog();
    },

    show: function() {
        this._dialog.show_all();
    },

    _updateDialog: function() {
        let format = this._gconf.get_string(FORMAT_KEY);
        this._12hrRadio.active = (format == "12-hour");
        this._24hrRadio.active = (format == "24-hour");

        this._dateCheck.active = this._gconf.get_bool(SHOW_DATE_KEY);
        this._secondsCheck.active = this._gconf.get_bool(SHOW_SECONDS_KEY);
    },

    _onResponse: function() {
        this._dialog.destroy();
        this._gconf.notify_remove(this._notifyId);
        this.emit('destroy');
    }
};
Signals.addSignalMethods(ClockPreferences.prototype);

function main(params) {
    if ('progName' in params)
        GLib.set_prgname(params['progName']);
    Gtk.init(null, null);

    let clockPrefs = new ClockPreferences(params['uiFile']);
    clockPrefs.connect('destroy',
                       function() {
                           Gtk.main_quit();
                       });
    clockPrefs.show();

    Gtk.main();
}
