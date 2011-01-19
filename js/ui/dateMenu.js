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

const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Calendar = imports.ui.calendar;

// in org.gnome.desktop.interface
const CLOCK_FORMAT_KEY        = 'clock-format';

// in org.gnome.shell.clock
const CLOCK_SHOW_DATE_KEY     = 'show-date';
const CLOCK_SHOW_SECONDS_KEY  = 'show-seconds';

function on_vert_sep_repaint (area)
{
    let cr = area.get_context();
    let themeNode = area.get_theme_node();
    let [width, height] = area.get_surface_size();
    let found, margin, gradientHeight;
    [found, margin] = themeNode.lookup_length('-margin-vertical', false);
    [found, gradientWidth] = themeNode.lookup_length('-gradient-width', false);
    let startColor = new Clutter.Color();
    themeNode.lookup_color('-gradient-start', false, startColor);
    let endColor = new Clutter.Color();
    themeNode.lookup_color('-gradient-end', false, endColor);

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

function DateMenuButton() {
    this._init();
}

DateMenuButton.prototype = {
    __proto__: PanelMenu.Button.prototype,

    _init: function() {
        let item;
        let hbox;
        let vbox;

        //this._event_source = new Calendar.EmptyEventSource();
        this._event_source = new Calendar.FakeEventSource();
        // TODO: write e.g. EvolutionEventSource

        PanelMenu.Button.prototype._init.call(this, St.Align.START);

        this._clock = new St.Label();
        this.actor.set_child(this._clock);

        hbox = new St.BoxLayout({name: 'calendarHBox'});
        this.menu.addActor(hbox);

        // Fill up the first column

        vbox = new St.BoxLayout({vertical: true, name: 'calendarVBox1'});
        hbox.add(vbox);

        // Date
        this._date = new St.Label();
        this._date.style_class = 'datemenu-date-label';
        vbox.add(this._date);

        this._event_list = new Calendar.EventsList(this._event_source);

        // Calendar
        this._calendar = new Calendar.Calendar(this._event_source, this._event_list);
        vbox.add(this._calendar.actor);

        // Add vertical separator

        item = new St.DrawingArea({ style_class: 'calendar-vertical-separator',
                                    pseudo_class: 'highlighted' });
        item.set_width(25); // TODO: don't hard-code the width
        item.connect('repaint', Lang.bind(this, on_vert_sep_repaint));
        hbox.add(item);

        // Fill up the second column
        //
        // Event list
        hbox.add(this._event_list.actor);

        // Whenever the menu is opened, select today
        this.menu.connect('open-state-changed', Lang.bind(this, function(menu, is_open) {
            if (is_open) {
                let now = new Date();
                this._calendar.setDate(now);
            }
        }));

        // Done with hbox for calendar and event list

        // Add separator
        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        // Add button to get to the Date and Time settings
        this.menu.addAction(_("Date and Time Settings"),
                            Lang.bind(this, this._onPreferencesActivate));

        // Track changes to clock settings
        this._desktopSettings = new Gio.Settings({ schema: 'org.gnome.desktop.interface' });
        this._clockSettings = new Gio.Settings({ schema: 'org.gnome.shell.clock' });
        this._desktopSettings.connect('changed', Lang.bind(this, this._updateClockAndDate));
        this._clockSettings.connect('changed', Lang.bind(this, this._updateClockAndDate));

        // Start the clock
        this._updateClockAndDate();
    },

    _updateClockAndDate: function() {
        let format = this._desktopSettings.get_string(CLOCK_FORMAT_KEY);
        let showDate = this._clockSettings.get_boolean(CLOCK_SHOW_DATE_KEY);
        let showSeconds = this._clockSettings.get_boolean(CLOCK_SHOW_SECONDS_KEY);

        let clockFormat;
        let dateFormat;

        switch (format) {
            case '24h':
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
            case '12h':
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
        Util.spawnDesktop('gnome-datetime-panel');
    },
};
