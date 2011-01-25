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

function _onVertSepRepaint (area)
{
    let cr = area.get_context();
    let themeNode = area.get_theme_node();
    let [width, height] = area.get_surface_size();
    let found;
    let stippleWidth = 1.0;
    let stippleColor = new Clutter.Color();
    [found, stippleWidth] = themeNode.lookup_length('-stipple-width', false);
    themeNode.lookup_color('-stipple-color', false, stippleColor);
    let x = Math.floor(width/2) + 0.5;
    cr.moveTo(x, 0);
    cr.lineTo(x, height);
    Clutter.cairo_set_source_color(cr, stippleColor);
    cr.setDash([1, 3], 1); // Hard-code for now
    cr.setLineWidth(stippleWidth);
    cr.stroke();
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

        //this._eventSource = new Calendar.EmptyEventSource();
        this._eventSource = new Calendar.FakeEventSource();
        // TODO: write e.g. EvolutionEventSource

        PanelMenu.Button.prototype._init.call(this, St.Align.START);

        this._clock = new St.Label();
        this.actor.set_child(this._clock);

        hbox = new St.BoxLayout();
        this.menu.addActor(hbox);

        // Fill up the first column

        vbox = new St.BoxLayout({vertical: true});
        hbox.add(vbox);

        // Date
        this._date = new St.Label();
        this._date.style_class = 'datemenu-date-label';
        vbox.add(this._date);

        this._eventList = new Calendar.EventsList(this._eventSource);

        // Calendar
        this._calendar = new Calendar.Calendar(this._eventSource);
        this._calendar.connect('selected-date-changed',
                               Lang.bind(this, function(calendar, date) {
                                   this._eventList.setDate(date);
                               }));
        vbox.add(this._calendar.actor);

        //item = new St.Button({style_class: 'popup-menu-item', label: 'foobar'});
        //vbox.add(item);
        item = new PopupMenu.PopupSeparatorMenuItem();
        item.actor.remove_actor(item._drawingArea);
        vbox.add(item._drawingArea);
        item = new PopupMenu.PopupMenuItem(_("Date and Time Settings"));
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        vbox.add(item.actor);

        // Add vertical separator

        item = new St.DrawingArea({ style_class: 'calendar-vertical-separator',
                                    pseudo_class: 'highlighted' });
        item.connect('repaint', Lang.bind(this, _onVertSepRepaint));
        hbox.add(item);

        // Fill up the second column
        //
        vbox = new St.BoxLayout({vertical: true});
        hbox.add(vbox);

        // Event list
        vbox.add(this._eventList.actor);

        item = new PopupMenu.PopupMenuItem(_("Open Calendar"));
        item.connect('activate', Lang.bind(this, this._onOpenCalendarActivate));
        vbox.add(item.actor, {y_align : St.Align.END, expand : true, y_fill : false});

        // Whenever the menu is opened, select today
        this.menu.connect('open-state-changed', Lang.bind(this, function(menu, isOpen) {
            if (isOpen) {
                let now = new Date();
                this._calendar.setDate(now);
                // No need to update this._eventList as ::selected-date-changed
                // signal will fire
            }
        }));

        // Done with hbox for calendar and event list

        // Add separator
        //item = new PopupMenu.PopupSeparatorMenuItem();
        //this.menu.addMenuItem(item);

        // Add button to get to the Date and Time settings
        //this.menu.addAction(_("Date and Time Settings"),
        //                    Lang.bind(this, this._onPreferencesActivate));

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

        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the shell (e.g. "Tue 9:29 AM").
         */
        dateFormat = _("%A %B %e, %Y");
        this._date.set_text(displayDate.toLocaleFormat(dateFormat));

        Mainloop.timeout_add(msecRemaining, Lang.bind(this, this._updateClockAndDate));
        return false;
    },

    _onPreferencesActivate: function() {
        this.menu.close();
        Util.spawnDesktop('gnome-datetime-panel');
    },

    _onOpenCalendarActivate: function() {
        this.menu.close();
        // TODO: pass '-c calendar' (to force the calendar at startup)
        // TODO: pass the selected day
        Util.spawnDesktop('evolution');
    },
};
