/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;

const Lang = imports.lang;
const Signals = imports.signals;

const Gettext = imports.gettext;

const FORMAT_KEY       = 'format';
const SHOW_DATE_KEY    = 'show-date';
const SHOW_SECONDS_KEY = 'show-seconds';


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

        this._settings = new Gio.Settings({ schema: 'org.gnome.shell.clock' });
        this._notifyId = this._settings.connect('changed',
                                                Lang.bind(this,
                                                          this._updateDialog));

        this._12hrRadio.connect('toggled', Lang.bind(this,
            function() {
                let format = this._12hrRadio.active ? '12-hour' : '24-hour';
                this._settings.set_string(FORMAT_KEY, format);
            }));
        this._dateCheck.connect('toggled', Lang.bind(this,
            function() {
                this._settings.set_boolean(SHOW_DATE_KEY,
                                           this._dateCheck.active);
            }));
        this._secondsCheck.connect('toggled', Lang.bind(this,
            function() {
                this._settings.set_boolean(SHOW_SECONDS_KEY,
                                           this._secondsCheck.active);
            }));

        this._updateDialog();
    },

    show: function() {
        this._dialog.show_all();
    },

    _updateDialog: function() {
        let format = this._settings.get_string(FORMAT_KEY);
        this._12hrRadio.active = (format == "12-hour");
        this._24hrRadio.active = (format == "24-hour");

        this._dateCheck.active = this._settings.get_boolean(SHOW_DATE_KEY);
        this._secondsCheck.active = this._settings.get_boolean(SHOW_SECONDS_KEY);
    },

    _onResponse: function() {
        this._dialog.destroy();
        this._settings.disconnect(this._notifyId);
        this.emit('destroy');
    }
};
Signals.addSignalMethods(ClockPreferences.prototype);

function main(params) {
    if ('progName' in params)
        GLib.set_prgname(params['progName']);
    if ('localeDir' in params)
        Gettext.bindtextdomain('gnome-shell', params['localeDir']);

    Gtk.init(null, null);

    let clockPrefs = new ClockPreferences(params['uiFile']);
    clockPrefs.connect('destroy',
                       function() {
                           Gtk.main_quit();
                       });
    clockPrefs.show();

    Gtk.main();
}
