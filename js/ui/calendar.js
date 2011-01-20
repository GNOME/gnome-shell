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
 * Sunday are non-work days (not true in e.g. Israel, it's Sunday and
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

// Abstraction for an appointment/task in a calendar

function CalendarTask(date, summary) {
    this._init(date, summary);
}

CalendarTask.prototype = {
    _init: function(date, summary) {
        this.date = date;
        this.summary = summary;
    }
};

// Interface for appointments/tasks - e.g. the contents of a calendar
//
// TODO: write e.g. EvolutionEventSource

// First, an implementation with no events
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
function FakeEventSource() {
    this._init();
}

FakeEventSource.prototype = {
    _init: function() {

        this._fakeTasks = [];

        // Generate fake events
        //
        let now = new Date();
        let summary = '';
        now.setHours(0);
        now.setMinutes(0);
        now.setSeconds(0);

        // '10-oclock pow-wow' is an event occuring IN THE PAST every four days at 10am
        for(let n = 0; n < 10; n++) {
            let t = new Date(now.getTime() - n * 4 * 86400 * 1000);
            t.setHours(10);
            summary = '10-oclock pow-wow (n=' + n + ')';
            this._fakeTasks.push(new CalendarTask(t, summary));
        }

        // '11-oclock thing' is an event occuring every three days at 11am
        for(let n = 0; n < 10; n++) {
            let t = new Date(now.getTime() + n * 3 * 86400 * 1000);
            t.setHours(11);
            summary = '11-oclock thing (n=' + n + ')';
            this._fakeTasks.push(new CalendarTask(t, summary));
        }

        // 'Weekly Meeting' is an event occuring every seven days at 1:45pm (two days displaced)
        for(let n = 0; n < 5; n++) {
            let t = new Date(now.getTime() + (n * 7 + 2) * 86400 * 1000);
            t.setHours(13);
            t.setMinutes(45);
            summary = 'Weekly Meeting (n=' + n + ')';
            this._fakeTasks.push(new CalendarTask(t, summary));
        }

        // 'Get Married' is an event that actually reflects reality (Dec 4, 2010) :-)
        this._fakeTasks.push(new CalendarTask(new Date(2010, 11, 4, 16, 0), 'Get Married'));

        // ditto for 'NE Patriots vs NY Jets'
        this._fakeTasks.push(new CalendarTask(new Date(2010, 11, 6, 20, 30), 'NE Patriots vs NY Jets'));

        // An event for tomorrow @6:30pm that is added/removed every five
        // seconds (to check that the ::changed signal works)
        let transientEventDate = new Date(now.getTime() + 86400*1000);
        transientEventDate.setHours(18);
        transientEventDate.setMinutes(30);
        transientEventDate.setSeconds(0);
        Mainloop.timeout_add(5000, Lang.bind(this, this._updateTransientEvent));
        this._includeTransientEvent = false;
        this._transientEvent = new CalendarTask(transientEventDate, 'A Transient Event');
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
        for(let n = 0; n < this._fakeTasks.length; n++) {
            let task = this._fakeTasks[n];
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

// Calendar:
// @eventSource: is an object implementing the EventSource API, e.g. the
// getTasks(), hasTasks() methods and the ::changed signal.
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

        let now = new Date();

        let row = 2;
        while (true) {
            let button = new St.Button({ label: iter.getDate().toString() });

            let iterStr = iter.toUTCString();
            button.connect('clicked', Lang.bind(this, function() {
                let newly_selectedDate = new Date(iterStr);
                this.setDate(newly_selectedDate);
            }));

            let styleClass;
            let hasTasks;
            hasTasks = this._eventSource.hasTasks(iter);
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

            if (hasTasks)
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

    _addPeriod: function(header, begin, end, includeDayName) {
        let tasks = this._eventSource.getTasks(begin, end);

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

    _showOtherDay: function(day) {
        this.actor.destroy_children();

        let dayBegin = new Date(day.getTime());
        let dayEnd = new Date(day.getTime());
        dayBegin.setHours(0);
        dayBegin.setMinutes(1);
        dayEnd.setHours(23);
        dayEnd.setMinutes(59);
        this._addPeriod(day.toLocaleFormat('%A, %B %d, %Y'), dayBegin, dayEnd, false);
    },

    _showToday: function() {
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
