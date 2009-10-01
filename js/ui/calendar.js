/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

const Gettext_gtk20 = imports.gettext.domain('gtk20');

const MSECS_IN_DAY = 24 * 60 * 60 * 1000;

function _sameDay(dateA, dateB) {
    return (dateA.getDate() == dateB.getDate() &&
            dateA.getMonth() == dateB.getMonth() &&
            dateA.getYear() == dateB.getYear());
}

function Calendar() {
    this._init();
};

Calendar.prototype = {
    _init: function() {
        // FIXME: This is actually the fallback method for GTK+ for the week start;
        // GTK+ by preference uses nl_langinfo (NL_TIME_FIRST_WEEKDAY). We probably
        // should add a C function so we can do the full handling.
        this._weekStart = NaN;
        let weekStartString = Gettext_gtk20.gettext("calendar:week_start:0");
        if (weekStartString.indexOf("calendar:week_start:") == 0) {
            this._weekStart = parseInt(weekStartString.substring(20));
        }

        if (isNaN(this._weekStart) || this._weekStart < 0 || this._weekStart > 6) {
            log("Translation of 'calendar:week_start:0' in GTK+ is not correct");
            this.weekStart = 0;
        }

        // Find the ordering for month/year in the calendar heading
        switch (Gettext_gtk20.gettext("calendar:MY")) {
        case "calendar:MY":
            this._headerFormat = "%B %Y";
            break;
        case "calendar:YM":
            this._headerFormat = "%Y %B";
            break;
        default:
            log("Translation of 'calendar:MY' in GTK+ is not correct");
            this._headerFormat = "%B %Y";
            break;
        }

        // Start off with the current date
        this.date = new Date();

        this.actor = new St.Table({ homogeneous: false,
                                    style_class: "calendar",
                                    reactive: true });

        this.actor.connect('scroll-event',
                           Lang.bind(this, this._onScroll));

        // Top line of the calendar '<| September 2009 |>'
        this._topBox = new St.BoxLayout();
        this.actor.add(this._topBox,
                       { row: 0, col: 0, col_span: 7 });

        let back = new St.Button({ label: "&lt;", style_class: 'calendar-change-month'  });
        this._topBox.add(back);
        back.connect("clicked", Lang.bind(this, this._prevMonth));

        this._dateLabel = new St.Label();
        this._topBox.add(this._dateLabel, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });

        let forward = new St.Button({ label: "&gt;", style_class: 'calendar-change-month' });
        this._topBox.add(forward);
        forward.connect("clicked", Lang.bind(this, this._nextMonth));

        // We need to figure out the abbreviated localized names for the days of the week;
        // we do this by just getting the next 7 days starting from right now and then putting
        // them in the right cell in the table. It doesn't matter if we add them in order
        let iter = new Date(this.date);
        iter.setSeconds(0); // Leap second protection. Hah!
        for (let i = 0; i < 7; i++) {
            this.actor.add(new St.Label({ text: iter.toLocaleFormat("%a") }),
                           { row: 1,
                             col: (7 + iter.getDay() - this._weekStart) % 7,
                             x_fill: false, x_align: 1.0 });
            iter.setTime(iter.getTime() + MSECS_IN_DAY);
        }

        this._update();
    },

    // Sets the calendar to show a specific date
    setDate: function(date) {
        if (!_sameDay(date, this.date)) {
            this.date = date;
            this._update();
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

    _update: function() {
        this._dateLabel.text = this.date.toLocaleFormat("%B %Y");

        // Remove everything but the topBox and the weekday labels
        let children = this.actor.get_children();
        for (let i = 8; i < children.length; i++)
            children[i].destroy();

        // Start at the beginning of the week before the start of the month
        let iter = new Date(this.date);
        iter.setDate(1);
        iter.setSeconds(0);
        iter.setTime(iter.getTime() - (iter.getDay() - this._weekStart) * MSECS_IN_DAY);

        let now = new Date();

        let row = 2;
        while (true) {
            let label = new St.Label({ text: iter.getDate().toString() });
            if (_sameDay(now, iter))
                label.style_class = "calendar-day calendar-today";
            else if (iter.getMonth() != this.date.getMonth())
                label.style_class = "calendar-day calendar-other-month-day";
            else
                label.style_class = "calendar-day";
            this.actor.add(label,
                           { row: row, col: (7 + iter.getDay() - this._weekStart) % 7,
                             x_fill: false, x_align: 1.0 });

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
