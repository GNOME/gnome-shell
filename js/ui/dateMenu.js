// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Params = imports.misc.params;
const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Calendar = imports.ui.calendar;

function _onVertSepRepaint (area)
{
    let cr = area.get_context();
    let themeNode = area.get_theme_node();
    let [width, height] = area.get_surface_size();
    let stippleColor = themeNode.get_color('-stipple-color');
    let stippleWidth = themeNode.get_length('-stipple-width');
    let x = Math.floor(width/2) + 0.5;
    cr.moveTo(x, 0);
    cr.lineTo(x, height);
    Clutter.cairo_set_source_color(cr, stippleColor);
    cr.setDash([1, 3], 1); // Hard-code for now
    cr.setLineWidth(stippleWidth);
    cr.stroke();
    cr.$dispose();
};

const DateMenuButton = new Lang.Class({
    Name: 'DateMenuButton',
    Extends: PanelMenu.Button,

    _init: function() {
        let item;
        let hbox;
        let vbox;

        let menuAlignment = 0.25;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            menuAlignment = 1.0 - menuAlignment;
        this.parent(menuAlignment);

        this._clockDisplay = new St.Label({ y_align: Clutter.ActorAlign.CENTER });
        this.actor.label_actor = this._clockDisplay;
        this.actor.add_actor(this._clockDisplay);
        this.actor.add_style_class_name ('clock-display');

        hbox = new St.BoxLayout({ name: 'calendarArea' });
        this.menu.box.add_child(hbox);

        // Fill up the first column

        vbox = new St.BoxLayout({vertical: true});
        hbox.add(vbox);

        // Date
        this._date = new St.Label({ style_class: 'datemenu-date-label',
                                    can_focus: true });
        vbox.add(this._date);

        this._eventList = new Calendar.EventsList();
        this._calendar = new Calendar.Calendar();

        this._calendar.connect('selected-date-changed',
                               Lang.bind(this, function(calendar, date) {
                                  // we know this._eventList is defined here, because selected-data-changed
                                  // only gets emitted when the user clicks a date in the calendar,
                                  // and the calender makes those dates unclickable when instantiated with
                                  // a null event source
                                   this._eventList.setDate(date);
                               }));
        vbox.add(this._calendar.actor);

        let separator = new PopupMenu.PopupSeparatorMenuItem();
        vbox.add(separator.actor, { y_align: St.Align.END, expand: true, y_fill: false });

        this._openCalendarItem = new PopupMenu.PopupMenuItem(_("Open Calendar"));
        this._openCalendarItem.connect('activate', Lang.bind(this, this._onOpenCalendarActivate));
        vbox.add(this._openCalendarItem.actor, {y_align: St.Align.END, expand: true, y_fill: false});

        this._openClocksItem = new PopupMenu.PopupMenuItem(_("Open Clocks"));
        this._openClocksItem.connect('activate', Lang.bind(this, this._onOpenClocksActivate));
        vbox.add(this._openClocksItem.actor, {y_align: St.Align.END, expand: true, y_fill: false});

        Shell.AppSystem.get_default().connect('installed-changed',
                                              Lang.bind(this, this._appInstalledChanged));

        item = this.menu.addSettingsAction(_("Date & Time Settings"), 'gnome-datetime-panel.desktop');
        if (item) {
            item.actor.show_on_set_parent = false;
            item.actor.reparent(vbox);
            this._dateAndTimeSeparator = separator;
        }

        this._separator = new St.DrawingArea({ style_class: 'calendar-vertical-separator',
                                               pseudo_class: 'highlighted' });
        this._separator.connect('repaint', Lang.bind(this, _onVertSepRepaint));
        hbox.add(this._separator);

        // Fill up the second column
        hbox.add(this._eventList.actor, { expand: true, y_fill: false, y_align: St.Align.START });

        // Whenever the menu is opened, select today
        this.menu.connect('open-state-changed', Lang.bind(this, function(menu, isOpen) {
            if (isOpen) {
                let now = new Date();
                this._calendar.setDate(now);
            }
        }));

        // Done with hbox for calendar and event list

        this._clock = new GnomeDesktop.WallClock();
        this._clock.connect('notify::clock', Lang.bind(this, this._updateClockAndDate));
        this._updateClockAndDate();

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _appInstalledChanged: function() {
        this._calendarApp = undefined;
        this._updateEventsVisibility();
    },

    _updateEventsVisibility: function() {
        let visible = this._eventSource.hasCalendars;
        this._openCalendarItem.actor.visible = visible &&
            (this._getCalendarApp() != null);
        this._openClocksItem.actor.visible = visible &&
            (this._getClockApp() != null);
        this._separator.visible = visible;
        this._eventList.actor.visible = visible;
        if (visible) {
            let alignment = 0.25;
            if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
                alignment = 1.0 - alignment;
            this.menu._arrowAlignment = alignment;
        } else {
            this.menu._arrowAlignment = 0.5;
        }
    },

    _setEventSource: function(eventSource) {
        if (this._eventSource)
            this._eventSource.destroy();

        this._calendar.setEventSource(eventSource);
        this._eventList.setEventSource(eventSource);

        this._eventSource = eventSource;
        this._eventSource.connect('notify::has-calendars', Lang.bind(this, function() {
            this._updateEventsVisibility();
        }));
    },

    _sessionUpdated: function() {
        let eventSource;
        let showEvents = Main.sessionMode.showCalendarEvents;
        if (showEvents) {
            eventSource = new Calendar.DBusEventSource();
        } else {
            eventSource = new Calendar.EmptyEventSource();
        }
        this._setEventSource(eventSource);
        this._updateEventsVisibility();

        // This needs to be handled manually, as the code to
        // autohide separators doesn't work across the vbox
        this._dateAndTimeSeparator.actor.visible = Main.sessionMode.allowSettings;
    },

    _updateClockAndDate: function() {
        this._clockDisplay.set_text(this._clock.clock);
        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the shell (e.g. "Tue 9:29 AM").
         */
        let dateFormat = _("%A %B %e, %Y");
        let displayDate = new Date();
        this._date.set_text(displayDate.toLocaleFormat(dateFormat));
    },

    _getCalendarApp: function() {
        if (this._calendarApp !== undefined)
            return this._calendarApp;

        let apps = Gio.AppInfo.get_recommended_for_type('text/calendar');
        if (apps && (apps.length > 0))
            this._calendarApp = apps[0];
        else
            this._calendarApp = null;
        return this._calendarApp;
    },

    _getClockApp: function() {
        return Shell.AppSystem.get_default().lookup_app('gnome-clocks.desktop');
    },

    _onOpenCalendarActivate: function() {
        this.menu.close();

        let app = this._getCalendarApp();
        if (app.get_id() == 'evolution.desktop')
            app = Gio.DesktopAppInfo.new('evolution-calendar.desktop');
        app.launch([], global.create_app_launch_context(0, -1));
    },

    _onOpenClocksActivate: function() {
        this.menu.close();
        let app = this._getClockApp();
        app.activate();
    }
});
