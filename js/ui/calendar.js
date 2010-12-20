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

const MSECS_IN_DAY = 24 * 60 * 60 * 1000;
const WEEKDATE_HEADER_WIDTH_DIGITS = 3;
const SHOW_WEEKDATE_KEY = 'show-weekdate';

function _sameDay(dateA, dateB) {
    return (dateA.getDate() == dateB.getDate() &&
            dateA.getMonth() == dateB.getMonth() &&
            dateA.getYear() == dateB.getYear());
}

/* TODO: maybe needs config - right now we assume that Saturday and
 * Sunday are work days (not true in e.g. Israel, it's Sunday and
 * Monday there)
 */
function _isWorkDay(date) {
    return date.getDay() != 0 && date.getDay() != 6;
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

function _getCalendarDayAbbreviation(day_number) {
    let ret;
    switch (day_number) {
    case 0:
        /* Translators: Abbreviation used in calendar grid
         * widget. Note: all calendar abbreviations are always shown
         * together and in order, e.g. "S M T W T F S"
         */
        ret = _("S");
        break;

    case 1:
        /* Translators: Calendar abbreviation for Monday */
        ret = _("M");
        break;

    case 2:
        /* Translators: Calendar abbreviation for Tuesday */
        ret = _("T");
        break;

    case 3:
        /* Translators: Calendar abbreviation for Wednesday */
        ret = _("W");
        break;

    case 4:
        /* Translators: Calendar abbreviation for Thursday */
        ret = _("T");
        break;

    case 5:
        /* Translators: Calendar abbreviation for Friday */
        ret = _("F");
        break;

    case 6:
        /* Translators: Calendar abbreviation for Saturday */
        ret = _("S");
        break;
    }
    return ret;
}

function _getEventDayAbbreviation(day_number) {
    let ret;
    switch (day_number) {
    case 0:
        /* Translators: Abbreviation used in event list for Sunday */
        ret = _("Su");
        break;

    case 1:
        /* Translators: Abbreviation used in event list for Monday */
        ret = _("M");
        break;

    case 2:
        /* Translators: Abbreviation used in event list for Tuesday */
        ret = _("T");
        break;

    case 3:
        /* Translators: Abbreviation used in event list for Wednesday */
        ret = _("W");
        break;

    case 4:
        /* Translators: Abbreviation used in event list for Thursday */
        ret = _("Th");
        break;

    case 5:
        /* Translators: Abbreviation used in event list for Friday */
        ret = _("F");
        break;

    case 6:
        /* Translators: Abbreviation used in event list for Saturday */
        ret = _("S");
        break;
    }
    return ret;
}

// ------------------------------------------------------------------------
// Abstraction for an appointment/task in a calendar
//

function CalendarTask(date, summary) {
    this._init(date, summary);
}

CalendarTask.prototype = {
    _init: function(date, summary) {
        this.date = date;
        this.summary = summary;
    }
};

// ------------------------------------------------------------------------
// Interface for appointments/tasks - e.g. the contents of a calendar
//
// TODO: write e.g. EvolutionEventSource
//

// First, an implementation with no events
//
function EmptyEventSource() {
    this._init();
}

EmptyEventSource.prototype = {
    _init: function() {
    },

    getTasks: function(begin, end) {
        let result = [];
        return result;
    },

    hasTasks: function(day) {
        return false;
    }
};
Signals.addSignalMethods(EmptyEventSource.prototype);

// Second, an implementation with fake events
//
function FakeEventSource() {
    this._init();
}

FakeEventSource.prototype = {
    _init: function() {

        this._fake_tasks = [];

        // Generate fake events
        //
        let now = new Date();
        let summary = '';
        now.setHours(0);
        now.setMinutes(0);
        now.setSeconds(0);

        // '10-oclock pow-wow' is an event occuring IN THE PAST every four days at 10am
        for(let n = 0; n < 10; n++) {
            let t = new Date(now.getTime() - n*4*86400*1000);
            t.setHours(10);
            summary = '10-oclock pow-wow (n=' + n + ')';
            this._fake_tasks.push(new CalendarTask(t, summary));
        }

        // '11-oclock thing' is an event occuring every three days at 11am
        for(let n = 0; n < 10; n++) {
            let t = new Date(now.getTime() + n*3*86400*1000);
            t.setHours(11);
            summary = '11-oclock thing (n=' + n + ')';
            this._fake_tasks.push(new CalendarTask(t, summary));
        }

        // 'Weekly Meeting' is an event occuring every seven days at 1:45pm (two days displaced)
        for(let n = 0; n < 5; n++) {
            let t = new Date(now.getTime() + (n*7+2)*86400*1000);
            t.setHours(13);
            t.setMinutes(45);
            summary = 'Weekly Meeting (n=' + n + ')';
            this._fake_tasks.push(new CalendarTask(t, summary));
        }

        // 'Get Married' is an event that actually reflects reality (Dec 4, 2010) :-)
        this._fake_tasks.push(new CalendarTask(new Date(2010,11,4,16,0), 'Get Married'));

        // ditto for 'NE Patriots vs NY Jets'
        this._fake_tasks.push(new CalendarTask(new Date(2010,11,6,20,30), 'NE Patriots vs NY Jets'));

        // An event for tomorrow @6:30pm that is added/removed every five
        // seconds (to check that the ::changed signal works)
        let transient_event_date = new Date(now.getTime() + 86400*1000);
        transient_event_date.setHours(18);
        transient_event_date.setMinutes(30);
        transient_event_date.setSeconds(0);
        Mainloop.timeout_add(5000, Lang.bind(this, this._updateTransientEvent));
        this._includeTransientEvent = false;
        this._transientEvent = new CalendarTask(transient_event_date, 'A Transient Event');
        this._transientEventCounter = 1;
    },

    _updateTransientEvent: function() {
        this._includeTransientEvent = !this._includeTransientEvent;
        this._transientEventCounter = this._transientEventCounter + 1;
        this._transientEvent.summary = 'A Transient Event (' + this._transientEventCounter + ')';
        this.emit('changed');
        Mainloop.timeout_add(5000, Lang.bind(this, this._updateTransientEvent));
    },

    getTasks: function(begin, end) {
        let result = [];
        //log('begin:' + begin);
        //log('end:  ' + end);
        for(let n = 0; n < this._fake_tasks.length; n++) {
            let task = this._fake_tasks[n];
            if (task.date >= begin && task.date <= end) {
                result.push(task);
            }
            //log('when:' + task.date + ' summary:' + task.summary);
        }
        if (this._includeTransientEvent && this._transientEvent.date >= begin && this._transientEvent.date <= end)
            result.push(this._transientEvent);
        return result;
    },

    hasTasks: function(day) {
        let dayBegin = new Date(day.getTime());
        let dayEnd = new Date(day.getTime());
        dayBegin.setHours(0);
        dayBegin.setMinutes(1);
        dayEnd.setHours(23);
        dayEnd.setMinutes(59);

        let tasks = this.getTasks(dayBegin, dayEnd);

        if (tasks.length == 0)
            return false;

        return true;
    }

};
Signals.addSignalMethods(FakeEventSource.prototype);

/* ------------------------------------------------------------------------ */

// @event_source is an object implementing the EventSource API, e.g. the
// getTasks(), hasTasks() methods and the ::changed signal.
//
// @event_list is the EventList object to control
//
function Calendar(event_source, event_list) {
    this._init(event_source, event_list);
}

Calendar.prototype = {
    _init: function(event_source, event_list) {
        this._event_source = event_source;
        this._event_list = event_list;

        this._event_source.connect('changed', Lang.bind(this, this._onEventSourceChanged));

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
        this.selected_date = new Date();

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
        if (!_sameDay(date, this.selected_date)) {
            this.selected_date = date;
            this._update();
            this.emit('selected-date-changed', new Date(this.selected_date));
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
        //
        let iter = new Date(this.selected_date);
        iter.setSeconds(0); // Leap second protection. Hah!
        iter.setHours(12);
        for (let i = 0; i < 7; i++) {
            // Could use iter.toLocaleFormat('%a') but that normally gives three characters
            // and we want, ideally, a single character for e.g. S M T W T F S
            let custom_day_abbrev = _getCalendarDayAbbreviation(iter.getDay());
            let label = new St.Label({ text: custom_day_abbrev });
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
        let new_date = new Date(this.selected_date);
        if (new_date.getMonth() == 0) {
            new_date.setMonth(11);
            new_date.setFullYear(new_date.getFullYear() - 1);
        } else {
            new_date.setMonth(new_date.getMonth() - 1);
        }
        this.setDate(new_date);
   },

    _onNextMonthButtonClicked: function() {
        let new_date = new Date(this.selected_date);
        if (new_date.getMonth() == 11) {
            new_date.setMonth(0);
            new_date.setFullYear(new_date.getFullYear() + 1);
        } else {
            new_date.setMonth(new_date.getMonth() + 1);
        }
        this.setDate(new_date);
    },

    _onSettingsChange: function() {
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);
        this._buildHeader();
        this._update();
    },

    _onEventSourceChanged: function() {
        this._update();
    },

    _update: function() {
        this._dateLabel.text = this.selected_date.toLocaleFormat(this._headerFormat);

        // Remove everything but the topBox and the weekday labels
        let children = this.actor.get_children();
        for (let i = this._firstDayIndex; i < children.length; i++)
            children[i].destroy();

        // Start at the beginning of the week before the start of the month
        let iter = new Date(this.selected_date);
        iter.setDate(1);
        iter.setSeconds(0);
        iter.setHours(12);
        let daysToWeekStart = (7 + iter.getDay() - this._weekStart) % 7;
        iter.setTime(iter.getTime() - daysToWeekStart * MSECS_IN_DAY);

        let now = new Date();

        let row = 2;
        let dayButtons = [];
        this._dayButtons = dayButtons;
        while (true) {
            let button = new St.Button({ label: iter.getDate().toString() });

            dayButtons.push(button);

            let iterStr = iter.toUTCString();
            button.connect('clicked', Lang.bind(this, function() {
                let newly_selected_date = new Date(iterStr);
                this.setDate(newly_selected_date);
            }));

            let style_class;
            let has_tasks;
            has_tasks = this._event_source.hasTasks(iter);
            style_class = 'calendar-day-base calendar-day';
            if (_isWorkDay(iter))
                style_class += ' calendar-work-day'
            else
                style_class += ' calendar-nonwork-day'

            if (_sameDay(now, iter))
                style_class += ' calendar-today';
            else if (iter.getMonth() != this.selected_date.getMonth())
                style_class += ' calendar-other-month-day';

            if (_sameDay(this.selected_date, iter))
                button.add_style_pseudo_class('active');

            if (has_tasks)
                style_class += ' calendar-day-with-events'

            button.style_class = style_class;

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
                if (iter.getMonth() > this.selected_date.getMonth() || iter.getYear() > this.selected_date.getYear())
                    break;
                row++;
            }
        }

        // update the event list widget
        if (now.getDate() == this.selected_date.getDate() &&
            now.getMonth() == this.selected_date.getMonth() &&
            now.getFullYear() == this.selected_date.getFullYear()) {
            // Today - show: Today, Tomorrow and This Week
            this._event_list.showToday();
        } else {
            // Not Today - show only events from that day
            this._event_list.showOtherDay(this.selected_date);
        }
    }
};

Signals.addSignalMethods(Calendar.prototype);

// ------------------------------------------------------------------------

function EventsList(event_source) {
    this._init(event_source);
}

EventsList.prototype = {
    _init: function(event_source) {
        this.actor = new St.BoxLayout({ vertical: true, style_class: 'events-header-vbox'});
        // FIXME: Evolution backend is currently disabled
        // this.evolutionTasks = new EvolutionEventsSource();

        this._event_source = event_source;
    },

    _addEvent: function(dayNameBox, timeBox, eventTitleBox, includeDayName, day, time, desc) {
        if (includeDayName) {
            dayNameBox.add(new St.Label({ style_class: 'events-day-dayname', text: day}), {x_fill: false});
        }
        timeBox.add(new St.Label({ style_class: 'events-day-time', text: time}), {x_fill: false});
        eventTitleBox.add(new St.Label({ style_class: 'events-day-task', text: desc}));
    },

    _addPeriod: function(header, begin, end, includeDayName) {
        let tasks = this._event_source.getTasks(begin, end);

        if (tasks.length == 0)
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

        for (let n = 0; n < tasks.length; n++) {
            let task = tasks[n];
            let dayString = _getEventDayAbbreviation(task.date.getDay());
            let timeString = task.date.toLocaleFormat('%I:%M %p'); // TODO: locale considerations
            let summaryString = task.summary;
            this._addEvent(dayNameBox, timeBox, eventTitleBox, includeDayName, dayString, timeString, summaryString);
        }
    },

    showOtherDay: function(day) {
        this.actor.destroy_children();

        let dayBegin = new Date(day.getTime());
        let dayEnd = new Date(day.getTime());
        dayBegin.setHours(0);
        dayBegin.setMinutes(1);
        dayEnd.setHours(23);
        dayEnd.setMinutes(59);
        this._addPeriod(day.toLocaleFormat('%A, %B %d, %Y'), dayBegin, dayEnd, false);
    },

    showToday: function() {
        this.actor.destroy_children();

        let dayBegin = new Date();
        let dayEnd = new Date();
        dayBegin.setHours(0);
        dayBegin.setMinutes(1);
        dayEnd.setHours(23);
        dayEnd.setMinutes(59);
        this._addPeriod(_("Today"), dayBegin, dayEnd, false);

        dayBegin.setDate(dayBegin.getDate() + 1);
        dayEnd.setDate(dayEnd.getDate() + 1);
        this._addPeriod(_("Tomorrow"), dayBegin, dayEnd, false);

        if (dayEnd.getDay() == 6 || dayEnd.getDay() == 0) {
            dayBegin.setDate(dayEnd.getDate() + 1);
            dayEnd.setDate(dayBegin.getDate() + 6 - dayBegin.getDay());

            this._addPeriod(_("Next week"), dayBegin, dayEnd, true);
            return;
        }
        let d = 6 - dayEnd.getDay() - 1;
        dayBegin.setDate(dayBegin.getDate() + 1);
        dayEnd.setDate(dayEnd.getDate() + 1 + d);
        this._addPeriod(_("This week"), dayBegin, dayEnd, true);
    }
};
