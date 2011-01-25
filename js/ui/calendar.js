/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;
const Pango = imports.gi.Pango;
const Gettext_gtk30 = imports.gettext.domain('gtk30');
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;

const MSECS_IN_DAY = 24 * 60 * 60 * 1000;
const WEEKDATE_HEADER_WIDTH_DIGITS = 3;
const SHOW_WEEKDATE_KEY = 'show-weekdate';

// in org.gnome.desktop.interface
const CLOCK_FORMAT_KEY        = 'clock-format';

function _sameDay(dateA, dateB) {
    return (dateA.getDate() == dateB.getDate() &&
            dateA.getMonth() == dateB.getMonth() &&
            dateA.getYear() == dateB.getYear());
}

function _sameYear(dateA, dateB) {
    return (dateA.getYear() == dateB.getYear());
}

/* TODO: maybe needs config - right now we assume that Saturday and
 * Sunday are non-work days (not true in e.g. Israel, it's Sunday and
 * Monday there)
 */
function _isWorkDay(date) {
    return date.getDay() != 0 && date.getDay() != 6;
}

function _getBeginningOfDay(date) {
    let ret = new Date(date.getTime());
    ret.setHours(0);
    ret.setMinutes(0);
    ret.setMilliseconds(0);
    return ret;
}

function _getEndOfDay(date) {
    let ret = new Date(date.getTime());
    ret.setHours(23);
    ret.setMinutes(59);
    ret.setMilliseconds(999);
    return ret;
}

function _formatEventTime(event, clockFormat) {
    let ret;
    if (event.allDay) {
        /* Translators: Shown in calendar event list for all day events */
        ret = _("All Day");
    } else {
        switch (clockFormat) {
        case '24h':
            ret = event.date.toLocaleFormat('%H:%M');
            break;

        default:
            /* explicit fall-through */
        case '12h':
            ret = event.date.toLocaleFormat('%l:%M %p');
            break;
        }
    }
    return ret;
}

function _getCalendarWeekForDate(date) {
    // Based on the algorithms found here:
    // http://en.wikipedia.org/wiki/Talk:ISO_week_date
    let midnightDate = new Date(date.getFullYear(), date.getMonth(), date.getDate());
    // Need to get Monday to be 1 ... Sunday to be 7
    let dayOfWeek = 1 + ((midnightDate.getDay() + 6) % 7);
    let nearestThursday = new Date(midnightDate.getFullYear(), midnightDate.getMonth(),
                                   midnightDate.getDate() + (4 - dayOfWeek));

    let jan1st = new Date(nearestThursday.getFullYear(), 0, 1);
    let diffDate = nearestThursday - jan1st;
    let dayNumber = Math.floor(Math.abs(diffDate) / MSECS_IN_DAY);
    let weekNumber = Math.floor(dayNumber / 7) + 1;

    return weekNumber;
}

function _getDigitWidth(actor){
    let context = actor.get_pango_context();
    let themeNode = actor.get_theme_node();
    let font = themeNode.get_font();
    let metrics = context.get_metrics(font, context.get_language());
    let width = metrics.get_approximate_digit_width();
    return width;
}

function _getCalendarDayAbbreviation(dayNumber) {
    let abbreviations = [
        /* Translators: Calendar grid abbreviation for Sunday.
         *
         * NOTE: These abbreviations are always shown together and in
         * order, e.g. "S M T W T F S".
         */
        _("S"),
        /* Translators: Calendar grid abbreviation for Monday */
        _("M"),
        /* Translators: Calendar grid abbreviation for Tuesday */
        _("T"),
        /* Translators: Calendar grid abbreviation for Wednesday */
        _("W"),
        /* Translators: Calendar grid abbreviation for Thursday */
        _("T"),
        /* Translators: Calendar grid abbreviation for Friday */
        _("F"),
        /* Translators: Calendar grid abbreviation for Saturday */
        _("S")
    ];
    return abbreviations[dayNumber];
}

function _getEventDayAbbreviation(dayNumber) {
    let abbreviations = [
        /* Translators: Event list abbreviation for Sunday.
         *
         * NOTE: These abbreviations are normally not shown together
         * so they need to be unique (e.g. Tuesday and Thursday cannot
         * both be 'T').
         */
        _("Su"),
        /* Translators: Event list abbreviation for Monday */
        _("M"),
        /* Translators: Event list abbreviation for Tuesday */
        _("T"),
        /* Translators: Event list abbreviation for Wednesday */
        _("W"),
        /* Translators: Event list abbreviation for Thursday */
        _("Th"),
        /* Translators: Event list abbreviation for Friday */
        _("F"),
        /* Translators: Event list abbreviation for Saturday */
        _("S")
    ];
    return abbreviations[dayNumber];
}

// Abstraction for an appointment/event in a calendar

function CalendarEvent(date, summary, allDay) {
    this._init(date, summary, allDay);
}

CalendarEvent.prototype = {
    _init: function(date, summary, allDay) {
        this.date = date;
        this.summary = summary;
        this.allDay = allDay;
    }
};

// Interface for appointments/events - e.g. the contents of a calendar
//

// First, an implementation with no events
function EmptyEventSource() {
    this._init();
}

EmptyEventSource.prototype = {
    _init: function() {
    },

    getEvents: function(begin, end) {
        let result = [];
        return result;
    },

    hasEvents: function(day) {
        return false;
    }
};
Signals.addSignalMethods(EmptyEventSource.prototype);

// Second, wrap native Evolution event source
function EvolutionEventSource() {
    this._init();
}

EvolutionEventSource.prototype = {
    _init: function() {
        this._native = new Shell.EvolutionEventSource();
        this._native.connect('changed', Lang.bind(this, function() {
            this.emit('changed');
        }));
    },

    getEvents: function(begin, end) {
        let result = [];
        let nativeEvents = this._native.get_events(begin.getTime(), end.getTime());
        for (let n = 0; n < nativeEvents.length; n++) {
            let nativeEvent = nativeEvents[n];
            result.push(new CalendarEvent(new Date(nativeEvent.date), nativeEvent.summary, nativeEvent.all_day));
        }
        return result;
    },

    hasEvents: function(day) {
        let dayBegin = _getBeginningOfDay(day);
        let dayEnd = _getEndOfDay(day);

        let events = this.getEvents(dayBegin, dayEnd);

        if (events.length == 0)
            return false;

        return true;
    }
};
Signals.addSignalMethods(EvolutionEventSource.prototype);

// Finally, an implementation with fake events
function FakeEventSource() {
    this._init();
}

FakeEventSource.prototype = {
    _init: function() {

        this._fakeEvents = [];

        // Generate fake events
        //
        let midnightToday = _getBeginningOfDay(new Date());
        let summary = '';

        // '10-oclock pow-wow' is an event occuring IN THE PAST every four days at 10am
        for (let n = 0; n < 10; n++) {
            let t = new Date(midnightToday.getTime() - n * 4 * 86400 * 1000);
            t.setHours(10);
            summary = '10-oclock pow-wow (n=' + n + ')';
            this._fakeEvents.push(new CalendarEvent(t, summary, false));
        }

        // '11-oclock thing' is an event occuring every three days at 11am
        for (let n = 0; n < 10; n++) {
            let t = new Date(midnightToday.getTime() + n * 3 * 86400 * 1000);
            t.setHours(11);
            summary = '11-oclock thing (n=' + n + ')';
            this._fakeEvents.push(new CalendarEvent(t, summary, false));
        }

        // 'Weekly Meeting' is an event occuring every seven days at 1:45pm (two days displaced)
        for (let n = 0; n < 5; n++) {
            let t = new Date(midnightToday.getTime() + (n * 7 + 2) * 86400 * 1000);
            t.setHours(13);
            t.setMinutes(45);
            summary = 'Weekly Meeting (n=' + n + ')';
            this._fakeEvents.push(new CalendarEvent(t, summary, false));
        }

        // 'Fun All Day' is an all-day event occuring every fortnight (three days displayed)
        for (let n = 0; n < 10; n++) {
            let t = new Date(midnightToday.getTime() + (n * 14 + 3) * 86400 * 1000);
            summary = 'Fun All Day (n=' + n + ')';
            this._fakeEvents.push(new CalendarEvent(t, summary, true));
        }

        // 'Get Married' is an event that actually reflects reality (Dec 4, 2010) :-)
        this._fakeEvents.push(new CalendarEvent(new Date(2010, 11, 4, 16, 0), 'Get Married', false));

        // ditto for 'NE Patriots vs NY Jets'
        this._fakeEvents.push(new CalendarEvent(new Date(2010, 11, 6, 20, 30), 'NE Patriots vs NY Jets', false));

        // An event for tomorrow @6:30pm that is added/removed every five
        // seconds (to check that the ::changed signal works)
        let transientEventDate = new Date(midnightToday.getTime() + 86400 * 1000);
        transientEventDate.setHours(18);
        transientEventDate.setMinutes(30);
        transientEventDate.setSeconds(0);
        Mainloop.timeout_add(5000, Lang.bind(this, this._updateTransientEvent));
        this._includeTransientEvent = false;
        this._transientEvent = new CalendarEvent(transientEventDate, 'A Transient Event', false);
        this._transientEventCounter = 1;
    },

    _updateTransientEvent: function() {
        this._includeTransientEvent = !this._includeTransientEvent;
        this._transientEventCounter = this._transientEventCounter + 1;
        this._transientEvent.summary = 'A Transient Event (' + this._transientEventCounter + ')';
        this.emit('changed');
        Mainloop.timeout_add(5000, Lang.bind(this, this._updateTransientEvent));
    },

    getEvents: function(begin, end) {
        let result = [];
        //log('begin:' + begin);
        //log('end:  ' + end);
        for(let n = 0; n < this._fakeEvents.length; n++) {
            let event = this._fakeEvents[n];
            if (event.date >= begin && event.date <= end) {
                result.push(event);
            }
            //log('when:' + event.date + ' summary:' + event.summary);
        }
        if (this._includeTransientEvent && this._transientEvent.date >= begin && this._transientEvent.date <= end)
            result.push(this._transientEvent);
        result.sort(function(event1, event2) {
            return event1.date.getTime() - event2.date.getTime();
        });
        return result;
    },

    hasEvents: function(day) {
        let dayBegin = _getBeginningOfDay(day);
        let dayEnd = _getEndOfDay(day);

        let events = this.getEvents(dayBegin, dayEnd);

        if (events.length == 0)
            return false;

        return true;
    }

};
Signals.addSignalMethods(FakeEventSource.prototype);

// Calendar:
// @eventSource: is an object implementing the EventSource API, e.g. the
// getEvents(), hasEvents() methods and the ::changed signal.
function Calendar(eventSource) {
    this._init(eventSource);
}

Calendar.prototype = {
    _init: function(eventSource) {
        this._eventSource = eventSource;

        this._eventSource.connect('changed', Lang.bind(this, this._update));

        // FIXME: This is actually the fallback method for GTK+ for the week start;
        // GTK+ by preference uses nl_langinfo (NL_TIME_FIRST_WEEKDAY). We probably
        // should add a C function so we can do the full handling.
        this._weekStart = NaN;
        this._weekdate = NaN;
        this._digitWidth = NaN;
        this._settings = new Gio.Settings({ schema: 'org.gnome.shell.calendar' });

        this._settings.connect('changed::' + SHOW_WEEKDATE_KEY, Lang.bind(this, this._onSettingsChange));
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);

        let weekStartString = Gettext_gtk30.gettext('calendar:week_start:0');
        if (weekStartString.indexOf('calendar:week_start:') == 0) {
            this._weekStart = parseInt(weekStartString.substring(20));
        }

        if (isNaN(this._weekStart) || this._weekStart < 0 || this._weekStart > 6) {
            log('Translation of "calendar:week_start:0" in GTK+ is not correct');
            this._weekStart = 0;
        }

        // Find the ordering for month/year in the calendar heading
        this._headerFormatWithoutYear = '%B';
        switch (Gettext_gtk30.gettext('calendar:MY')) {
        case 'calendar:MY':
            this._headerFormat = '%B %Y';
            break;
        case 'calendar:YM':
            this._headerFormat = '%Y %B';
            break;
        default:
            log('Translation of "calendar:MY" in GTK+ is not correct');
            this._headerFormat = '%B %Y';
            break;
        }

        // Start off with the current date
        this.selectedDate = new Date();

        this.actor = new St.Table({ homogeneous: false,
                                    style_class: 'calendar',
                                    reactive: true });

        this.actor.connect('scroll-event',
                           Lang.bind(this, this._onScroll));

        this._buildHeader ();
        this._update();
    },

    // Sets the calendar to show a specific date
    setDate: function(date) {
        if (!_sameDay(date, this.selectedDate)) {
            this.selectedDate = date;
            this._update();
            this.emit('selected-date-changed', new Date(this.selectedDate));
        }
    },

    _buildHeader: function() {
        let offsetCols = this._useWeekdate ? 1 : 0;
        this.actor.destroy_children();

        // Top line of the calendar '<| September 2009 |>'
        this._topBox = new St.BoxLayout();
        this.actor.add(this._topBox,
                       { row: 0, col: 0, col_span: offsetCols + 7 });

        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChange));

        let back = new St.Button({ style_class: 'calendar-change-month-back' });
        this._topBox.add(back);
        back.connect('clicked', Lang.bind(this, this._onPrevMonthButtonClicked));

        this._dateLabel = new St.Label({style_class: 'calendar-change-month'});
        this._topBox.add(this._dateLabel, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });

        let forward = new St.Button({ style_class: 'calendar-change-month-forward' });
        this._topBox.add(forward);
        forward.connect('clicked', Lang.bind(this, this._onNextMonthButtonClicked));

        // Add weekday labels...
        //
        // We need to figure out the abbreviated localized names for the days of the week;
        // we do this by just getting the next 7 days starting from right now and then putting
        // them in the right cell in the table. It doesn't matter if we add them in order
        let iter = new Date(this.selectedDate);
        iter.setSeconds(0); // Leap second protection. Hah!
        iter.setHours(12);
        for (let i = 0; i < 7; i++) {
            // Could use iter.toLocaleFormat('%a') but that normally gives three characters
            // and we want, ideally, a single character for e.g. S M T W T F S
            let customDayAbbrev = _getCalendarDayAbbreviation(iter.getDay());
            let label = new St.Label({ text: customDayAbbrev });
            label.style_class = 'calendar-day-base calendar-day-heading';
            this.actor.add(label,
                           { row: 1,
                             col: offsetCols + (7 + iter.getDay() - this._weekStart) % 7,
                             x_fill: false, x_align: St.Align.MIDDLE });
            iter.setTime(iter.getTime() + MSECS_IN_DAY);
        }

        // All the children after this are days, and get removed when we update the calendar
        this._firstDayIndex = this.actor.get_children().length;
    },

    _onStyleChange: function(actor, event) {
        // width of a digit in pango units
        this._digitWidth = _getDigitWidth(this.actor) / Pango.SCALE;
        this._setWeekdateHeaderWidth();
    },

    _setWeekdateHeaderWidth: function() {
        if (this.digitWidth != NaN && this._useWeekdate && this._weekdateHeader) {
            this._weekdateHeader.set_width (this._digitWidth * WEEKDATE_HEADER_WIDTH_DIGITS);
        }
    },

    _onScroll : function(actor, event) {
        switch (event.get_scroll_direction()) {
        case Clutter.ScrollDirection.UP:
        case Clutter.ScrollDirection.LEFT:
            this._onPrevMonthButtonClicked();
            break;
        case Clutter.ScrollDirection.DOWN:
        case Clutter.ScrollDirection.RIGHT:
            this._onNextMonthButtonClicked();
            break;
        }
    },

    _onPrevMonthButtonClicked: function() {
        let newDate = new Date(this.selectedDate);
        if (newDate.getMonth() == 0) {
            newDate.setMonth(11);
            newDate.setFullYear(newDate.getFullYear() - 1);
        } else {
            newDate.setMonth(newDate.getMonth() - 1);
        }
        this.setDate(newDate);
   },

    _onNextMonthButtonClicked: function() {
        let newDate = new Date(this.selectedDate);
        if (newDate.getMonth() == 11) {
            newDate.setMonth(0);
            newDate.setFullYear(newDate.getFullYear() + 1);
        } else {
            newDate.setMonth(newDate.getMonth() + 1);
        }
        this.setDate(newDate);
    },

    _onSettingsChange: function() {
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);
        this._buildHeader();
        this._update();
    },

    _update: function() {
        let now = new Date();

        if (_sameYear(this.selectedDate, now))
            this._dateLabel.text = this.selectedDate.toLocaleFormat(this._headerFormatWithoutYear);
        else
            this._dateLabel.text = this.selectedDate.toLocaleFormat(this._headerFormat);

        // Remove everything but the topBox and the weekday labels
        let children = this.actor.get_children();
        for (let i = this._firstDayIndex; i < children.length; i++)
            children[i].destroy();

        // Start at the beginning of the week before the start of the month
        let iter = new Date(this.selectedDate);
        iter.setDate(1);
        iter.setSeconds(0);
        iter.setHours(12);
        let daysToWeekStart = (7 + iter.getDay() - this._weekStart) % 7;
        iter.setTime(iter.getTime() - daysToWeekStart * MSECS_IN_DAY);

        let row = 2;
        while (true) {
            let button = new St.Button({ label: iter.getDate().toString() });

            let iterStr = iter.toUTCString();
            button.connect('clicked', Lang.bind(this, function() {
                let newly_selectedDate = new Date(iterStr);
                this.setDate(newly_selectedDate);
            }));

            let styleClass;
            let hasEvents;
            hasEvents = this._eventSource.hasEvents(iter);
            styleClass = 'calendar-day-base calendar-day';
            if (_isWorkDay(iter))
                styleClass += ' calendar-work-day'
            else
                styleClass += ' calendar-nonwork-day'

            if (_sameDay(now, iter))
                styleClass += ' calendar-today';
            else if (iter.getMonth() != this.selectedDate.getMonth())
                styleClass += ' calendar-other-month-day';

            if (_sameDay(this.selectedDate, iter))
                button.add_style_pseudo_class('active');

            if (hasEvents)
                styleClass += ' calendar-day-with-events'

            button.style_class = styleClass;

            let offsetCols = this._useWeekdate ? 1 : 0;
            this.actor.add(button,
                           { row: row, col: offsetCols + (7 + iter.getDay() - this._weekStart) % 7 });

            if (this._useWeekdate && iter.getDay() == 4) {
                let label = new St.Label({ text: _getCalendarWeekForDate(iter).toString(),
                                           style_class: 'calendar-day-base calendar-week-number'});
                this.actor.add(label,
                               { row: row, col: 0, y_align: St.Align.MIDDLE });
            }

            iter.setTime(iter.getTime() + MSECS_IN_DAY);
            if (iter.getDay() == this._weekStart) {
                // We stop on the first "first day of the week" after the month we are displaying
                if (iter.getMonth() > this.selectedDate.getMonth() || iter.getYear() > this.selectedDate.getYear())
                    break;
                row++;
            }
        }
    }
};

Signals.addSignalMethods(Calendar.prototype);

function EventsList(eventSource) {
    this._init(eventSource);
}

EventsList.prototype = {
    _init: function(eventSource) {
        this.actor = new St.BoxLayout({ vertical: true, style_class: 'events-header-vbox'});
        this._date = new Date();
        this._eventSource = eventSource;
        this._eventSource.connect('changed', Lang.bind(this, this._update));
        this._desktopSettings = new Gio.Settings({ schema: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed', Lang.bind(this, this._update));
        this._update();
    },

    _addEvent: function(dayNameBox, timeBox, eventTitleBox, includeDayName, day, time, desc) {
        if (includeDayName) {
            dayNameBox.add(new St.Label( { style_class: 'events-day-dayname',
                                           text: day } ),
                           { x_fill: false } );
        }
        timeBox.add(new St.Label( { style_class: 'events-day-time',
                                    text: time} ),
                    { x_fill: false } );
        eventTitleBox.add(new St.Label( { style_class: 'events-day-task',
                                          text: desc} ));
    },

    _addPeriod: function(header, begin, end, includeDayName, showNothingScheduled) {
        let events = this._eventSource.getEvents(begin, end);

        let clockFormat = this._desktopSettings.get_string(CLOCK_FORMAT_KEY);;

        if (events.length == 0 && !showNothingScheduled)
            return;

        let vbox = new St.BoxLayout( {vertical: true} );
        this.actor.add(vbox);

        vbox.add(new St.Label({ style_class: 'events-day-header', text: header }));
        let box = new St.BoxLayout({style_class: 'events-header-hbox'});
        let dayNameBox = new St.BoxLayout({ vertical: true, style_class: 'events-day-name-box' });
        let timeBox = new St.BoxLayout({ vertical: true, style_class: 'events-time-box' });
        let eventTitleBox = new St.BoxLayout({ vertical: true, style_class: 'events-event-box' });
        box.add(dayNameBox, {x_fill: false});
        box.add(timeBox, {x_fill: false});
        box.add(eventTitleBox, {expand: true});
        vbox.add(box);

        for (let n = 0; n < events.length; n++) {
            let event = events[n];
            let dayString = _getEventDayAbbreviation(event.date.getDay());
            let timeString = _formatEventTime(event, clockFormat);
            let summaryString = event.summary;
            this._addEvent(dayNameBox, timeBox, eventTitleBox, includeDayName, dayString, timeString, summaryString);
        }

        if (events.length == 0 && showNothingScheduled) {
            let now = new Date();
            /* Translators: Text to show if there are no events */
            let nothingEvent = new CalendarEvent(now, _("Nothing Scheduled"), true);
            let timeString = _formatEventTime(nothingEvent, clockFormat);
            this._addEvent(dayNameBox, timeBox, eventTitleBox, false, "", timeString, nothingEvent.summary);
        }
    },

    _showOtherDay: function(day) {
        this.actor.destroy_children();

        let dayBegin = _getBeginningOfDay(day);
        let dayEnd = _getEndOfDay(day);

        let dayString;
        let now = new Date();
        if (_sameYear(day, now))
            dayString = day.toLocaleFormat('%A, %B %d');
        else
            dayString = day.toLocaleFormat('%A, %B %d, %Y');
        this._addPeriod(dayString, dayBegin, dayEnd, false, true);
    },

    _showToday: function() {
        this.actor.destroy_children();

        let now = new Date();
        let dayBegin = _getBeginningOfDay(now);
        let dayEnd = _getEndOfDay(now);
        this._addPeriod(_("Today"), dayBegin, dayEnd, false, true);

        dayBegin.setDate(dayBegin.getDate() + 1);
        dayEnd.setDate(dayEnd.getDate() + 1);
        this._addPeriod(_("Tomorrow"), dayBegin, dayEnd, false, true);

        if (dayEnd.getDay() == 6 || dayEnd.getDay() == 0) {
            dayBegin.setDate(dayEnd.getDate() + 1);
            dayEnd.setDate(dayBegin.getDate() + 6 - dayBegin.getDay());

            this._addPeriod(_("Next week"), dayBegin, dayEnd, true, true);
            return;
        } else {
            let d = 6 - dayEnd.getDay() - 1;
            dayBegin.setDate(dayBegin.getDate() + 1);
            dayEnd.setDate(dayEnd.getDate() + 1 + d);
            this._addPeriod(_("This week"), dayBegin, dayEnd, true, true);
        }
    },

    // Sets the event list to show events from a specific date
    setDate: function(date) {
        if (!_sameDay(date, this._date)) {
            this._date = date;
            this._update();
        }
    },

    _update: function() {
        let today = new Date();
        if (_sameDay (this._date, today)) {
            this._showToday();
        } else {
            this._showOtherDay(this._date);
        }
    }
};
