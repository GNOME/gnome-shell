/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;
const Pango = imports.gi.Pango;
const Gettext_gtk20 = imports.gettext.domain('gtk20');
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

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

function _getCustomDayAbrreviation(day_number) {
    let ret;
    switch (day_number) {
    case 0:
        /* Translators: One-letter abbreaviation for Sunday - note:
         * all one-letter abbreviations are always shown together and
         * in order, e.g. "S M T W T F S"
         */
        ret = _("S");
        break;

    case 1:
        /* Translators: One-letter abbreaviation for Monday */
        ret = _("M");
        break;

    case 2:
        /* Translators: One-letter abbreaviation for Tuesday */
        ret = _("T");
        break;

    case 3:
        /* Translators: One-letter abbreaviation for Wednesday */
        ret = _("W");
        break;

    case 4:
        /* Translators: One-letter abbreaviation for Thursday */
        ret = _("T");
        break;

    case 5:
        /* Translators: One-letter abbreaviation for Friday */
        ret = _("F");
        break;

    case 6:
        /* Translators: One-letter abbreaviation for Saturday */
        ret = _("S");
        break;
    }
    return ret;
}

function Calendar() {
    this._init();
}

Calendar.prototype = {
    _init: function() {
        // FIXME: This is actually the fallback method for GTK+ for the week start;
        // GTK+ by preference uses nl_langinfo (NL_TIME_FIRST_WEEKDAY). We probably
        // should add a C function so we can do the full handling.
        this._weekStart = NaN;
        this._weekdate = NaN;
        this._digitWidth = NaN;
        this._settings = new Gio.Settings({ schema: 'org.gnome.shell.calendar' });

        this._settings.connect('changed::' + SHOW_WEEKDATE_KEY, Lang.bind(this, this._onSettingsChange));
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);

        let weekStartString = Gettext_gtk20.gettext('calendar:week_start:0');
        if (weekStartString.indexOf('calendar:week_start:') == 0) {
            this._weekStart = parseInt(weekStartString.substring(20));
        }

        if (isNaN(this._weekStart) || this._weekStart < 0 || this._weekStart > 6) {
            log('Translation of "calendar:week_start:0" in GTK+ is not correct');
            this._weekStart = 0;
        }

        // Find the ordering for month/year in the calendar heading
        switch (Gettext_gtk20.gettext('calendar:MY')) {
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
        this.date = new Date();

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
        if (!_sameDay(date, this.date)) {
            this.date = date;
            this._update();
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
        let [backlabel, forwardlabel] = ['&lt;', '&gt;'];
        if (St.Widget.get_default_direction () == St.TextDirection.RTL) {
            [backlabel, forwardlabel] = [forwardlabel, backlabel];
        }

        let back = new St.Button({ label: backlabel, style_class: 'calendar-change-month'  });
        this._topBox.add(back);
        back.connect('clicked', Lang.bind(this, this._prevMonth));

        this._dateLabel = new St.Label({style_class: 'calendar-change-month'});
        this._topBox.add(this._dateLabel, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });

        let forward = new St.Button({ label: forwardlabel, style_class: 'calendar-change-month' });
        this._topBox.add(forward);
        forward.connect('clicked', Lang.bind(this, this._nextMonth));

        // Add weekday labels...
        //
        // We need to figure out the abbreviated localized names for the days of the week;
        // we do this by just getting the next 7 days starting from right now and then putting
        // them in the right cell in the table. It doesn't matter if we add them in order
        //
        let iter = new Date(this.date);
        iter.setSeconds(0); // Leap second protection. Hah!
        iter.setHours(12);
        for (let i = 0; i < 7; i++) {
            // Could use iter.toLocaleFormat('%a') but that normally gives three characters
            // and we want, ideally, a single character for e.g. S M T W T F S
            let custom_day_abbrev = _getCustomDayAbrreviation(iter.getDay());
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
            this._prevMonth();
            break;
        case Clutter.ScrollDirection.DOWN:
        case Clutter.ScrollDirection.RIGHT:
            this._nextMonth();
            break;
        }
    },

    _prevMonth: function() {
        if (this.date.getMonth() == 0) {
            this.date.setMonth(11);
            this.date.setFullYear(this.date.getFullYear() - 1);
        } else {
            this.date.setMonth(this.date.getMonth() - 1);
        }
        this._update();
   },

    _nextMonth: function() {
        if (this.date.getMonth() == 11) {
            this.date.setMonth(0);
            this.date.setFullYear(this.date.getFullYear() + 1);
        } else {
            this.date.setMonth(this.date.getMonth() + 1);
        }
        this._update();
    },

    _onSettingsChange: function() {
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);
        this._buildHeader();
        this._update();
    },

    clearButtonsState: function() {
        for (let i = 0; i < this._dayButtons.length; i++) {
            this._dayButtons[i].remove_style_pseudo_class('active');
        }
    },

    _update: function() {
        this._dateLabel.text = this.date.toLocaleFormat(this._headerFormat);

        // Remove everything but the topBox and the weekday labels
        let children = this.actor.get_children();
        for (let i = this._firstDayIndex; i < children.length; i++)
            children[i].destroy();

        // Start at the beginning of the week before the start of the month
        let iter = new Date(this.date);
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
                this.emit('activate', new Date(iterStr));
                for (let i = 0; i < dayButtons.length; i++) {
                    dayButtons[i].remove_style_pseudo_class('active');
                }
                button.add_style_pseudo_class('active');
            }));
            let style_class;

            style_class = 'calendar-day-base calendar-day';
            if (_isWorkDay(iter))
                style_class += ' calendar-work-day'
            else
                style_class += ' calendar-nonwork-day'

            if (_sameDay(now, iter))
                style_class += ' calendar-today';
            else if (iter.getMonth() != this.date.getMonth())
                style_class += ' calendar-other-month-day';

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
                if (iter.getMonth() > this.date.getMonth() || iter.getYear() > this.date.getYear())
                    break;
                row++;
            }
        }
    }
};

Signals.addSignalMethods(Calendar.prototype);

function EvolutionEventsSource() {
    this._init();
}

EvolutionEventsSource.prototype = {
    _init: function() {
        try {
            this.EDataServer = imports.gi.EDataServer;
            this.ECalendar = imports.gi.ECalendar;
        } catch (e) {log(e);}
    },

    _ISODateString: function(d) {
        function pad(n) {
            return n < 10 ? '0' + n : n;
        }
        return '' + d.getUTCFullYear()// + '-'
               + pad(d.getUTCMonth() + 1)// + '-'
               + pad(d.getUTCDate()) + 'T'
               + pad(d.getUTCHours())// + ':'
               + pad(d.getUTCMinutes())// + ':'
               + pad(d.getUTCSeconds()) + 'Z'
    },

    _getStartTime: function(str) {
        let start = /DTSTART:\d{8}T\d{6}/.exec(str);
        if (!start)
            throw new Error("Bad data");
        start = start[0].substr(8);
        let year = start.substr(0, 4), month = start.substr(4, 2), day = start.substr(6, 2);
        let hour = start.substr(9, 2), minute = start.substr(12, 2);
        return new Date(year, month - 1, day, hour, minute, 0, 0);
    },

    _getSummary: function(str) {
        let match = /\nSUMMARY:.*(?=\r\n)/.exec(str);
        if (!match)
            return "";
        return match[0].substr(9);
    },

    getForInterval: function(begin, end, callback) {
        if (!this.EDataServer) {
            callback([]);
            return;
        }
        let res = [];
        let wait = 0;
        let list = this.EDataServer.SourceList.new_for_gconf_default("/apps/evolution/calendar/sources");

        let groups = list.peek_groups();

        let query = '(occur-in-time-range? (make-time "' + this._ISODateString(begin) + '") (make-time "' + this._ISODateString(end) + '"))';

        function decrimentWait() {
            wait--;
            if (wait == 0) {
                res.sort(function (a, b) {
                    if (a.time == b.time)
                        return 0;
                    if (a.time > b.time)
                        return 1;
                    return -1;
                });
                callback(res);
            }
        }

        for (let i = 0; i < groups.length; i++) {
            let sources = groups[i].peek_sources();
            for (let k = 0; k < sources.length; k++) {
                let cal = this.ECalendar.Cal.new(sources[k], this.ECalendar.CalSourceType.EVENT);
                if (!cal)
                    continue;
                let calOpenedExId = cal.connect('cal-opened-ex', Lang.bind(this, function(s, error) {
                    if (error) {
                        decrimentWait;
                        return;
                    }
                    let [success, view] = cal.get_query(query);
                    let viewObjectsAddedId = view.connect('objects-added', Lang.bind(this, function(o, list) {
                        for (let j = 0; j < list.length; j++) {
                            let event = global.icalcomponent_to_str(list[j]);
                            let start = this._getStartTime(event);
                            res.push({ time: start, title: this._getSummary(event) });
                        }
                    }));
                    let viewCompleteId = view.connect('view-complete', function() {
                        cal.disconnect(calOpenedExId);
                        view.disconnect(viewObjectsAddedId);
                        view.disconnect(viewCompleteId);
                        decrimentWait();
                    });
                    view.start();
                }));
                cal.open_async(false);
                wait++;
            }
        }

        if (wait == 0) {
            callback([]);
        }
    }
};

function EventsList() {
    this._init();
}

EventsList.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ vertical: true });
        this.evolutionTasks = new EvolutionEventsSource();
    },

    _addPeriod: function(header, begin, end, isDay) {
        this.actor.add(new St.Label({ style_class: 'events-day-header',
                                      text: header }));
        let box = new St.BoxLayout();
        let dayNameBox = new St.BoxLayout({ vertical: true, style_class: 'events-day-name-box' });
        let timeBox = new St.BoxLayout({ vertical: true, style_class: 'events-time-box' });
        let eventTitleBox = new St.BoxLayout({ vertical: true, style_class: 'events-event-box' });
        box.add(dayNameBox);
        box.add(timeBox);
        box.add(eventTitleBox);
        this.actor.add(box);

        this.evolutionTasks.getForInterval(begin, end, Lang.bind(this, function(tasks) {
            if (!tasks.length) {
                eventTitleBox.add(new St.Label({ style_class: 'events-no-events', text: _("No events") }));
                return;
            }

            for (let i = 0; i < tasks.length; i++) {
                let time = tasks[i].time.getHours() + ':';
                if (tasks[i].time.getMinutes() < 10)
                    time += '0';
                time += tasks[i].time.getMinutes();
                timeBox.add(new St.Label({ style_class: 'events-day-time', text: time }));
                eventTitleBox.add(new St.Label({ style_class: 'events-day-task', text: tasks[i].title }), { expand: false });
                if (!isDay)
                    continue;
                dayNameBox.add(new St.Label({ text: tasks[i].time.toLocaleFormat("%a") }));
            }
        }));
    },

    showDay: function(day) {
        this.actor.destroy_children();

        this.actor.add(new St.Bin({ style_class: 'events-header' }));

        let dayBegin = new Date(day.getTime());
        let dayEnd = new Date(day.getTime());
        dayBegin.setHours(0);
        dayBegin.setMinutes(1);
        dayEnd.setHours(23);
        dayEnd.setMinutes(59);
        this._addPeriod(day.toLocaleDateString(), dayBegin, dayEnd, false);
    },

    update: function() {
        this.actor.destroy_children();

        this.actor.add(new St.Bin({ style_class: 'events-header' }));

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
