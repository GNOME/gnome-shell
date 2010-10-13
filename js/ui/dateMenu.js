/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Calendar = imports.ui.calendar;

const CLOCK_FORMAT_KEY        = 'format';
const CLOCK_CUSTOM_FORMAT_KEY = 'custom-format';
const CLOCK_SHOW_DATE_KEY     = 'show-date';
const CLOCK_SHOW_SECONDS_KEY  = 'show-seconds';

function DateMenuButton() {
    this._init();
}

function on_vert_sep_repaint (area)
{
    let cr = area.get_context();
    let themeNode = area.get_theme_node();
    let [width, height] = area.get_surface_size();
    let found, margin, gradientHeight;
    [found, margin] = themeNode.get_length('-margin-vertical', false);
    [found, gradientWidth] = themeNode.get_length('-gradient-width', false);
    let startColor = new Clutter.Color();
    themeNode.get_color('-gradient-start', false, startColor);
    let endColor = new Clutter.Color();
    themeNode.get_color('-gradient-end', false, endColor);

    let gradientHeight = (height - margin * 2);
    let gradientOffset = (width - gradientWidth) / 2;
    let pattern = new Cairo.LinearGradient(gradientOffset, margin, gradientOffset + gradientWidth, height - margin);
    pattern.addColorStopRGBA(0, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
    pattern.addColorStopRGBA(0.5, endColor.red / 255, endColor.green / 255, endColor.blue / 255, endColor.alpha / 255);
    pattern.addColorStopRGBA(1, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
    cr.setSource(pattern);
    cr.rectangle(gradientOffset, margin, gradientWidth, gradientHeight);
    cr.fill();
};

DateMenuButton.prototype = {
    __proto__: PanelMenu.Button.prototype,

    _init: function() {
        let item;

        PanelMenu.Button.prototype._init.call(this, St.Align.START);

        this._clock = new St.Label();
        this.actor.set_child(this._clock);

        this._date = new St.Label();
        this._date.style_class = 'datemenu-date-label';
        this.menu._box.add(this._date);

        this._calendar = new Calendar.Calendar();
        this.menu._box.add(this._calendar.actor);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Date and Time Settings"), 'gnome-shell-clock-preferences');
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        this.menu.addMenuItem(item);

        this._clockSettings = new Gio.Settings({ schema: 'org.gnome.shell.clock' });
        this._clockSettings.connect('changed', Lang.bind(this, this._clockSettingsChanged));

        this._vertSep = new St.DrawingArea({ style_class: 'calendar-vertical-separator',
                                             pseudo_class: 'highlighted' });
        //this._vertSep.set_width (25);
        this._vertSep.connect('repaint', Lang.bind(this, on_vert_sep_repaint));

        let hbox;
        let orig_menu_box;
        orig_menu_box = this.menu._box;
        this.menu._boxPointer.bin.remove_actor(orig_menu_box);
        hbox = new St.BoxLayout();
        hbox.add(orig_menu_box);
        hbox.add(this._vertSep);
        hbox.add(new St.Label({text: "foo0"}));
        this.menu._boxPointer.bin.set_child(hbox);
        this.menu._box = hbox;

        // Start the clock
        this._updateClockAndDate();
    },

    _clockSettingsChanged: function() {
        this._updateClockAndDate();
    },

    _updateClockAndDate: function() {
        let format = this._clockSettings.get_string(CLOCK_FORMAT_KEY);
        let showDate = this._clockSettings.get_boolean(CLOCK_SHOW_DATE_KEY);
        let showSeconds = this._clockSettings.get_boolean(CLOCK_SHOW_SECONDS_KEY);

        let clockFormat;
        let dateFormat;

        switch (format) {
            case 'unix':
                // force updates every second
                showSeconds = true;
                clockFormat = '%s';
                break;
            case 'custom':
                // force updates every second
                showSeconds = true;
                clockFormat = this._clockSettings.get_string(CLOCK_CUSTOM_FORMAT_KEY);
                break;
            case '24-hour':
                if (showDate)
	            /* Translators: This is the time format with date used
                       in 24-hour mode. */
                    clockFormat = showSeconds ? _("%a %b %e, %R:%S")
                                              : _("%a %b %e, %R");
                else
	            /* Translators: This is the time format without date used
                       in 24-hour mode. */
                    clockFormat = showSeconds ? _("%a %R:%S")
                                              : _("%a %R");
                break;
            case '12-hour':
            default:
                if (showDate)
	            /* Translators: This is a time format with date used
                       for AM/PM. */
                    clockFormat = showSeconds ? _("%a %b %e, %l:%M:%S %p")
                                              : _("%a %b %e, %l:%M %p");
                else
	            /* Translators: This is a time format without date used
                       for AM/PM. */
                    clockFormat = showSeconds ? _("%a %l:%M:%S %p")
                                              : _("%a %l:%M %p");
                break;
        }

        let displayDate = new Date();
        let msecRemaining;
        if (showSeconds) {
            msecRemaining = 1000 - displayDate.getMilliseconds();
            if (msecRemaining < 50) {
                displayDate.setSeconds(displayDate.getSeconds() + 1);
                msecRemaining += 1000;
            }
        } else {
            msecRemaining = 60000 - (1000 * displayDate.getSeconds() +
                                     displayDate.getMilliseconds());
            if (msecRemaining < 500) {
                displayDate.setMinutes(displayDate.getMinutes() + 1);
                msecRemaining += 60000;
            }
        }

        this._clock.set_text(displayDate.toLocaleFormat(clockFormat));

        /* Translators: This is the date format to use */
        dateFormat = _("%B %e, %Y");
        this._date.set_text(displayDate.toLocaleFormat(dateFormat));

        Mainloop.timeout_add(msecRemaining, Lang.bind(this, this._updateClockAndDate));
        return false;
    },

    _onPreferencesActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-shell-clock-preferences']);
    },

    _spawn: function(args) {
        // FIXME: once Shell.Process gets support for signalling
        // errors we should pop up an error dialog or something here
        // on failure
        let p = new Shell.Process({'args' : args});
        p.run();
    }
};
