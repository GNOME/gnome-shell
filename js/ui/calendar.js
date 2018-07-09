// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const MessageList = imports.ui.messageList;
const MessageTray = imports.ui.messageTray;
const Mpris = imports.ui.mpris;
const Util = imports.misc.util;

var MSECS_IN_DAY = 24 * 60 * 60 * 1000;
var SHOW_WEEKDATE_KEY = 'show-weekdate';
var ELLIPSIS_CHAR = '\u2026';

var MESSAGE_ICON_SIZE = -1; // pick up from CSS

var NC_ = (context, str) => context + '\u0004' + str;

function sameYear(dateA, dateB) {
    return (dateA.getYear() == dateB.getYear());
}

function sameMonth(dateA, dateB) {
    return sameYear(dateA, dateB) && (dateA.getMonth() == dateB.getMonth());
}

function sameDay(dateA, dateB) {
    return sameMonth(dateA, dateB) && (dateA.getDate() == dateB.getDate());
}

function isToday(date) {
    return sameDay(new Date(), date);
}

function _isWorkDay(date) {
    /* Translators: Enter 0-6 (Sunday-Saturday) for non-work days. Examples: "0" (Sunday) "6" (Saturday) "06" (Sunday and Saturday). */
    let days = C_('calendar-no-work', "06");
    return days.indexOf(date.getDay().toString()) == -1;
}

function _getBeginningOfDay(date) {
    let ret = new Date(date.getTime());
    ret.setHours(0);
    ret.setMinutes(0);
    ret.setSeconds(0);
    ret.setMilliseconds(0);
    return ret;
}

function _getEndOfDay(date) {
    let ret = new Date(date.getTime());
    ret.setHours(23);
    ret.setMinutes(59);
    ret.setSeconds(59);
    ret.setMilliseconds(999);
    return ret;
}

function _getCalendarDayAbbreviation(dayNumber) {
    let abbreviations = [
        /* Translators: Calendar grid abbreviation for Sunday.
         *
         * NOTE: These grid abbreviations are always shown together
         * and in order, e.g. "S M T W T F S".
         */
        NC_("grid sunday", "S"),
        /* Translators: Calendar grid abbreviation for Monday */
        NC_("grid monday", "M"),
        /* Translators: Calendar grid abbreviation for Tuesday */
        NC_("grid tuesday", "T"),
        /* Translators: Calendar grid abbreviation for Wednesday */
        NC_("grid wednesday", "W"),
        /* Translators: Calendar grid abbreviation for Thursday */
        NC_("grid thursday", "T"),
        /* Translators: Calendar grid abbreviation for Friday */
        NC_("grid friday", "F"),
        /* Translators: Calendar grid abbreviation for Saturday */
        NC_("grid saturday", "S")
    ];
    return Shell.util_translate_time_string(abbreviations[dayNumber]);
}

// Abstraction for an appointment/event in a calendar

var CalendarEvent = new Lang.Class({
    Name: 'CalendarEvent',

    _init(id, date, end, summary, allDay) {
        this.id = id;
        this.date = date;
        this.end = end;
        this.summary = summary;
        this.allDay = allDay;
    }
});

// Interface for appointments/events - e.g. the contents of a calendar
//

// First, an implementation with no events
var EmptyEventSource = new Lang.Class({
    Name: 'EmptyEventSource',

    _init() {
        this.isLoading = false;
        this.isDummy = true;
        this.hasCalendars = false;
    },

    destroy() {
    },

    ignoreEvent(event) {
    },

    requestRange(begin, end) {
    },

    getEvents(begin, end) {
        let result = [];
        return result;
    },

    hasEvents(day) {
        return false;
    }
});
Signals.addSignalMethods(EmptyEventSource.prototype);

const CalendarServerIface = '<node> \
<interface name="org.gnome.Shell.CalendarServer"> \
<method name="GetEvents"> \
    <arg type="x" direction="in" /> \
    <arg type="x" direction="in" /> \
    <arg type="b" direction="in" /> \
    <arg type="a(sssbxxa{sv})" direction="out" /> \
</method> \
<property name="HasCalendars" type="b" access="read" /> \
<signal name="Changed" /> \
</interface> \
</node>';

const CalendarServerInfo  = Gio.DBusInterfaceInfo.new_for_xml(CalendarServerIface);

function CalendarServer() {
    return new Gio.DBusProxy({ g_connection: Gio.DBus.session,
                               g_interface_name: CalendarServerInfo.name,
                               g_interface_info: CalendarServerInfo,
                               g_name: 'org.gnome.Shell.CalendarServer',
                               g_object_path: '/org/gnome/Shell/CalendarServer' });
}

function _datesEqual(a, b) {
    if (a < b)
        return false;
    else if (a > b)
        return false;
    return true;
}

function _dateIntervalsOverlap(a0, a1, b0, b1)
{
    if (a1 <= b0)
        return false;
    else if (b1 <= a0)
        return false;
    else
        return true;
}

// an implementation that reads data from a session bus service
var DBusEventSource = new Lang.Class({
    Name: 'DBusEventSource',

    _init() {
        this._resetCache();
        this.isLoading = false;
        this.isDummy = false;

        this._ignoredEvents = new Map();

        let savedState = global.get_persistent_state('as', 'ignored_events');
        if (savedState)
            savedState.deep_unpack().forEach(eventId => {
                this._ignoredEvents.set(eventId, true);
            });

        this._initialized = false;
        this._dbusProxy = new CalendarServer();
        this._dbusProxy.init_async(GLib.PRIORITY_DEFAULT, null, (object, result) => {
            let loaded = false;

            try {
                this._dbusProxy.init_finish(result);
                loaded = true;
            } catch(e) {
                if (e.matches(Gio.DBusError, Gio.DBusError.TIMED_OUT)) {
                    // Ignore timeouts and install signals as normal, because with high
                    // probability the service will appear later on, and we will get a
                    // NameOwnerChanged which will finish loading
                    //
                    // (But still _initialized to false, because the proxy does not know
                    // about the HasCalendars property and would cause an exception trying
                    // to read it)
                } else {
                    log('Error loading calendars: ' + e.message);
                    return;
                }
            }

            this._dbusProxy.connectSignal('Changed', this._onChanged.bind(this));

            this._dbusProxy.connect('notify::g-name-owner', () => {
                if (this._dbusProxy.g_name_owner)
                    this._onNameAppeared();
                else
                    this._onNameVanished();
            });

            this._dbusProxy.connect('g-properties-changed', () => {
                this.emit('notify::has-calendars');
            });

            this._initialized = loaded;
            if (loaded) {
                this.emit('notify::has-calendars');
                this._onNameAppeared();
            }
        });
    },

    destroy() {
        this._dbusProxy.run_dispose();
    },

    get hasCalendars() {
        if (this._initialized)
            return this._dbusProxy.HasCalendars;
        else
            return false;
    },

    _resetCache() {
        this._events = [];
        this._lastRequestBegin = null;
        this._lastRequestEnd = null;
    },

    _onNameAppeared(owner) {
        this._initialized = true;
        this._resetCache();
        this._loadEvents(true);
    },

    _onNameVanished(oldOwner) {
        this._resetCache();
        this.emit('changed');
    },

    _onChanged() {
        this._loadEvents(false);
    },

    _onEventsReceived(results, error) {
        let newEvents = [];
        let appointments = results ? results[0] : null;
        if (appointments != null) {
            for (let n = 0; n < appointments.length; n++) {
                let a = appointments[n];
                let date = new Date(a[4] * 1000);
                let end = new Date(a[5] * 1000);
                let id = a[0];
                let summary = a[1];
                let allDay = a[3];
                let event = new CalendarEvent(id, date, end, summary, allDay);
                newEvents.push(event);
            }
            newEvents.sort((ev1, ev2) => ev1.date.getTime() - ev2.date.getTime());
        }

        this._events = newEvents;
        this.isLoading = false;
        this.emit('changed');
    },

    _loadEvents(forceReload) {
        // Ignore while loading
        if (!this._initialized)
            return;

        if (this._curRequestBegin && this._curRequestEnd){
            this._dbusProxy.GetEventsRemote(this._curRequestBegin.getTime() / 1000,
                                            this._curRequestEnd.getTime() / 1000,
                                            forceReload,
                                            this._onEventsReceived.bind(this),
                                            Gio.DBusCallFlags.NONE);
        }
    },

    ignoreEvent(event) {
        if (this._ignoredEvents.get(event.id))
            return;

        this._ignoredEvents.set(event.id, true);
        let savedState = new GLib.Variant('as', [...this._ignoredEvents.keys()]);
        global.set_persistent_state('ignored_events', savedState);
        this.emit('changed');
    },

    requestRange(begin, end) {
        if (!(_datesEqual(begin, this._lastRequestBegin) && _datesEqual(end, this._lastRequestEnd))) {
            this.isLoading = true;
            this._lastRequestBegin = begin;
            this._lastRequestEnd = end;
            this._curRequestBegin = begin;
            this._curRequestEnd = end;
            this._loadEvents(false);
        }
    },

    getEvents(begin, end) {
        let result = [];
        for(let n = 0; n < this._events.length; n++) {
            let event = this._events[n];

            if (this._ignoredEvents.has(event.id))
                continue;

            if (_dateIntervalsOverlap (event.date, event.end, begin, end)) {
                result.push(event);
            }
        }
        result.sort((event1, event2) => {
            // sort events by end time on ending day
            let d1 = event1.date < begin && event1.end <= end ? event1.end : event1.date;
            let d2 = event2.date < begin && event2.end <= end ? event2.end : event2.date;
            return d1.getTime() - d2.getTime();
        });
        return result;
    },

    hasEvents(day) {
        let dayBegin = _getBeginningOfDay(day);
        let dayEnd = _getEndOfDay(day);

        let events = this.getEvents(dayBegin, dayEnd);

        if (events.length == 0)
            return false;

        return true;
    }
});
Signals.addSignalMethods(DBusEventSource.prototype);

var Calendar = new Lang.Class({
    Name: 'Calendar',

    _init() {
        this._weekStart = Shell.util_get_week_start();
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.desktop.calendar' });

        this._settings.connect('changed::' + SHOW_WEEKDATE_KEY, this._onSettingsChange.bind(this));
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);

        /**
         * Translators: The header displaying just the month name
         * standalone, when this is a month of the current year.
         * "%OB" is the new format specifier introduced in glibc 2.27,
         * in most cases you should not change it.
         */
        this._headerFormatWithoutYear = _('%OB');
        /**
         * Translators: The header displaying the month name and the year
         * number, when this is a month of a different year.  You can
         * reorder the format specifiers or add other modifications
         * according to the requirements of your language.
         * "%OB" is the new format specifier introduced in glibc 2.27,
         * in most cases you should not use the old "%B" here unless you
         * absolutely know what you are doing.
         */
        this._headerFormat = _('%OB %Y');

        // Start off with the current date
        this._selectedDate = new Date();

        this._shouldDateGrabFocus = false;

        this.actor = new St.Widget({ style_class: 'calendar',
                                     layout_manager: new Clutter.TableLayout(),
                                     reactive: true });

        this.actor.connect('scroll-event',
                           this._onScroll.bind(this));

        this._buildHeader ();
    },

    // @eventSource: is an object implementing the EventSource API, e.g. the
    // requestRange(), getEvents(), hasEvents() methods and the ::changed signal.
    setEventSource(eventSource) {
        this._eventSource = eventSource;
        this._eventSource.connect('changed', () => {
            this._rebuildCalendar();
            this._update();
        });
        this._rebuildCalendar();
        this._update();
    },

    // Sets the calendar to show a specific date
    setDate(date) {
        if (sameDay(date, this._selectedDate))
            return;

        this._selectedDate = date;
        this._update();
        this.emit('selected-date-changed', new Date(this._selectedDate));
    },

    updateTimeZone() {
        // The calendar need to be rebuilt after a time zone update because
        // the date might have changed.
        this._rebuildCalendar();
        this._update();
    },

    _buildHeader() {
        let layout = this.actor.layout_manager;
        let offsetCols = this._useWeekdate ? 1 : 0;
        this.actor.destroy_all_children();

        // Top line of the calendar '<| September 2009 |>'
        this._topBox = new St.BoxLayout();
        layout.pack(this._topBox, 0, 0);
        layout.set_span(this._topBox, offsetCols + 7, 1);

        this._backButton = new St.Button({ style_class: 'calendar-change-month-back pager-button',
                                           accessible_name: _("Previous month"),
                                           can_focus: true });
        this._topBox.add(this._backButton);
        this._backButton.connect('clicked', this._onPrevMonthButtonClicked.bind(this));

        this._monthLabel = new St.Label({style_class: 'calendar-month-label',
                                         can_focus: true });
        this._topBox.add(this._monthLabel, { expand: true, x_fill: false, x_align: St.Align.MIDDLE });

        this._forwardButton = new St.Button({ style_class: 'calendar-change-month-forward pager-button',
                                              accessible_name: _("Next month"),
                                              can_focus: true });
        this._topBox.add(this._forwardButton);
        this._forwardButton.connect('clicked', this._onNextMonthButtonClicked.bind(this));

        // Add weekday labels...
        //
        // We need to figure out the abbreviated localized names for the days of the week;
        // we do this by just getting the next 7 days starting from right now and then putting
        // them in the right cell in the table. It doesn't matter if we add them in order
        let iter = new Date(this._selectedDate);
        iter.setSeconds(0); // Leap second protection. Hah!
        iter.setHours(12);
        for (let i = 0; i < 7; i++) {
            // Could use iter.toLocaleFormat('%a') but that normally gives three characters
            // and we want, ideally, a single character for e.g. S M T W T F S
            let customDayAbbrev = _getCalendarDayAbbreviation(iter.getDay());
            let label = new St.Label({ style_class: 'calendar-day-base calendar-day-heading',
                                       text: customDayAbbrev,
                                       can_focus: true });
            label.accessible_name = iter.toLocaleFormat('%A');
            let col;
            if (this.actor.get_text_direction() == Clutter.TextDirection.RTL)
                col = 6 - (7 + iter.getDay() - this._weekStart) % 7;
            else
                col = offsetCols + (7 + iter.getDay() - this._weekStart) % 7;
            layout.pack(label, col, 1);
            iter.setTime(iter.getTime() + MSECS_IN_DAY);
        }

        // All the children after this are days, and get removed when we update the calendar
        this._firstDayIndex = this.actor.get_n_children();
    },

    _onScroll(actor, event) {
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
        return Clutter.EVENT_PROPAGATE;
    },

    _onPrevMonthButtonClicked() {
        let newDate = new Date(this._selectedDate);
        let oldMonth = newDate.getMonth();
        if (oldMonth == 0) {
            newDate.setMonth(11);
            newDate.setFullYear(newDate.getFullYear() - 1);
            if (newDate.getMonth() != 11) {
                let day = 32 - new Date(newDate.getFullYear() - 1, 11, 32).getDate();
                newDate = new Date(newDate.getFullYear() - 1, 11, day);
            }
        }
        else {
            newDate.setMonth(oldMonth - 1);
            if (newDate.getMonth() != oldMonth - 1) {
                let day = 32 - new Date(newDate.getFullYear(), oldMonth - 1, 32).getDate();
                newDate = new Date(newDate.getFullYear(), oldMonth - 1, day);
            }
        }

        this._backButton.grab_key_focus();

        this.setDate(newDate);
    },

    _onNextMonthButtonClicked() {
        let newDate = new Date(this._selectedDate);
        let oldMonth = newDate.getMonth();
        if (oldMonth == 11) {
            newDate.setMonth(0);
            newDate.setFullYear(newDate.getFullYear() + 1);
            if (newDate.getMonth() != 0) {
                let day = 32 - new Date(newDate.getFullYear() + 1, 0, 32).getDate();
                newDate = new Date(newDate.getFullYear() + 1, 0, day);
            }
        }
        else {
            newDate.setMonth(oldMonth + 1);
            if (newDate.getMonth() != oldMonth + 1) {
                let day = 32 - new Date(newDate.getFullYear(), oldMonth + 1, 32).getDate();
                newDate = new Date(newDate.getFullYear(), oldMonth + 1, day);
            }
        }

        this._forwardButton.grab_key_focus();

        this.setDate(newDate);
    },

    _onSettingsChange() {
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);
        this._buildHeader();
        this._rebuildCalendar();
        this._update();
    },

    _rebuildCalendar() {
        let now = new Date();

        // Remove everything but the topBox and the weekday labels
        let children = this.actor.get_children();
        for (let i = this._firstDayIndex; i < children.length; i++)
            children[i].destroy();

        this._buttons = [];

        // Start at the beginning of the week before the start of the month
        //
        // We want to show always 6 weeks (to keep the calendar menu at the same
        // height if there are no events), so we pad it according to the following
        // policy:
        //
        // 1 - If a month has 6 weeks, we place no padding (example: Dec 2012)
        // 2 - If a month has 5 weeks and it starts on week start, we pad one week
        //     before it (example: Apr 2012)
        // 3 - If a month has 5 weeks and it starts on any other day, we pad one week
        //     after it (example: Nov 2012)
        // 4 - If a month has 4 weeks, we pad one week before and one after it
        //     (example: Feb 2010)
        //
        // Actually computing the number of weeks is complex, but we know that the
        // problematic categories (2 and 4) always start on week start, and that
        // all months at the end have 6 weeks.
        let beginDate = new Date(this._selectedDate);
        beginDate.setDate(1);
        beginDate.setSeconds(0);
        beginDate.setHours(12);

        this._calendarBegin = new Date(beginDate);
        this._markedAsToday = now;

        let year = beginDate.getYear();

        let daysToWeekStart = (7 + beginDate.getDay() - this._weekStart) % 7;
        let startsOnWeekStart = daysToWeekStart == 0;
        let weekPadding = startsOnWeekStart ? 7 : 0;

        beginDate.setTime(beginDate.getTime() - (weekPadding + daysToWeekStart) * MSECS_IN_DAY);

        let layout = this.actor.layout_manager;
        let iter = new Date(beginDate);
        let row = 2;
        // nRows here means 6 weeks + one header + one navbar
        let nRows = 8;
        while (row < 8) {
            // xgettext:no-javascript-format
            let button = new St.Button({ label: iter.toLocaleFormat(C_("date day number format", "%d")),
                                         can_focus: true });
            let rtl = button.get_text_direction() == Clutter.TextDirection.RTL;

            if (this._eventSource.isDummy)
                button.reactive = false;

            button._date = new Date(iter);
            button.connect('clicked', () => {
                this._shouldDateGrabFocus = true;
                this.setDate(button._date);
                this._shouldDateGrabFocus = false;
            });

            let hasEvents = this._eventSource.hasEvents(iter);
            let styleClass = 'calendar-day-base calendar-day';

            if (_isWorkDay(iter))
                styleClass += ' calendar-work-day';
            else
                styleClass += ' calendar-nonwork-day';

            // Hack used in lieu of border-collapse - see gnome-shell.css
            if (row == 2)
                styleClass = 'calendar-day-top ' + styleClass;

            let leftMost = rtl ? iter.getDay() == (this._weekStart + 6) % 7
                               : iter.getDay() == this._weekStart;
            if (leftMost)
                styleClass = 'calendar-day-left ' + styleClass;

            if (sameDay(now, iter))
                styleClass += ' calendar-today';
            else if (iter.getMonth() != this._selectedDate.getMonth())
                styleClass += ' calendar-other-month-day';

            if (hasEvents)
                styleClass += ' calendar-day-with-events';

            button.style_class = styleClass;

            let offsetCols = this._useWeekdate ? 1 : 0;
            let col;
            if (rtl)
                col = 6 - (7 + iter.getDay() - this._weekStart) % 7;
            else
                col = offsetCols + (7 + iter.getDay() - this._weekStart) % 7;
            layout.pack(button, col, row);

            this._buttons.push(button);

            if (this._useWeekdate && iter.getDay() == 4) {
                let label = new St.Label({ text: iter.toLocaleFormat('%V'),
                                           style_class: 'calendar-day-base calendar-week-number',
                                           can_focus: true });
                let weekFormat = Shell.util_translate_time_string(N_("Week %V"));
                label.accessible_name = iter.toLocaleFormat(weekFormat);
                layout.pack(label, rtl ? 7 : 0, row);
            }

            iter.setTime(iter.getTime() + MSECS_IN_DAY);

            if (iter.getDay() == this._weekStart)
                row++;
        }

        // Signal to the event source that we are interested in events
        // only from this date range
        this._eventSource.requestRange(beginDate, iter);
    },

    _update() {
        let now = new Date();

        if (sameYear(this._selectedDate, now))
            this._monthLabel.text = this._selectedDate.toLocaleFormat(this._headerFormatWithoutYear);
        else
            this._monthLabel.text = this._selectedDate.toLocaleFormat(this._headerFormat);

        if (!this._calendarBegin || !sameMonth(this._selectedDate, this._calendarBegin) || !sameDay(now, this._markedAsToday))
            this._rebuildCalendar();

        this._buttons.forEach(button => {
            if (sameDay(button._date, this._selectedDate)) {
                button.add_style_pseudo_class('selected');
                if (this._shouldDateGrabFocus)
                    button.grab_key_focus();
            }
            else
                button.remove_style_pseudo_class('selected');
        });
    }
});
Signals.addSignalMethods(Calendar.prototype);

var EventMessage = new Lang.Class({
    Name: 'EventMessage',
    Extends: MessageList.Message,

    _init(event, date) {
        this._event = event;
        this._date = date;

        this.parent(this._formatEventTime(), event.summary);

        this._icon = new St.Icon({ icon_name: 'x-office-calendar-symbolic' });
        this.setIcon(this._icon);

        this.actor.connect('style-changed', () => {
            let iconVisible = this.actor.get_parent().has_style_pseudo_class('first-child');
            this._icon.opacity = (iconVisible ? 255 : 0);
        });
    },

    _formatEventTime() {
        let periodBegin = _getBeginningOfDay(this._date);
        let periodEnd = _getEndOfDay(this._date);
        let allDay = (this._event.allDay || (this._event.date <= periodBegin &&
                                             this._event.end >= periodEnd));
        let title;
        if (allDay) {
            /* Translators: Shown in calendar event list for all day events
             * Keep it short, best if you can use less then 10 characters
             */
            title = C_("event list time", "All Day");
        } else {
            let date = this._event.date >= periodBegin ? this._event.date
                                                       : this._event.end;
            title = Util.formatTime(date, { timeOnly: true });
        }

        let rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;
        if (this._event.date < periodBegin && !this._event.allDay) {
            if (rtl)
                title = title + ELLIPSIS_CHAR;
            else
                title = ELLIPSIS_CHAR + title;
        }
        if (this._event.end > periodEnd && !this._event.allDay) {
            if (rtl)
                title = ELLIPSIS_CHAR + title;
            else
                title = title + ELLIPSIS_CHAR;
        }
        return title;
    },

    canClose() {
        return isToday(this._date);
    }
});

var NotificationMessage = new Lang.Class({
    Name: 'NotificationMessage',
    Extends: MessageList.Message,

    _init(notification) {
        this.notification = notification;

        this.parent(notification.title, notification.bannerBodyText);
        this.setUseBodyMarkup(notification.bannerBodyMarkup);

        this.setIcon(this._getIcon());

        this.connect('close', () => {
            this._closed = true;
            this.notification.destroy(MessageTray.NotificationDestroyedReason.DISMISSED);
        });
        this._destroyId = notification.connect('destroy', () => {
            if (!this._closed)
                this.close();
        });
        this._updatedId = notification.connect('updated',
                                               this._onUpdated.bind(this));
    },

    _getIcon() {
        if (this.notification.gicon)
            return new St.Icon({ gicon: this.notification.gicon,
                                 icon_size: MESSAGE_ICON_SIZE });
        else
            return this.notification.source.createIcon(MESSAGE_ICON_SIZE);
    },

    _onUpdated(n, clear) {
        this.setIcon(this._getIcon());
        this.setTitle(n.title);
        this.setBody(n.bannerBodyText);
        this.setUseBodyMarkup(n.bannerBodyMarkup);
    },

    _onClicked() {
        this.notification.activate();
    },

    _onDestroy() {
        this.parent();

        if (this._updatedId)
            this.notification.disconnect(this._updatedId);
        this._updatedId = 0;

        if (this._destroyId)
            this.notification.disconnect(this._destroyId);
        this._destroyId = 0;
    }
});

var EventsSection = new Lang.Class({
    Name: 'EventsSection',
    Extends: MessageList.MessageListSection,

    _init() {
        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed', this._reloadEvents.bind(this));
        this._eventSource = new EmptyEventSource();

        this._messageById = new Map();

        this.parent();

        this._title = new St.Button({ style_class: 'events-section-title',
                                      label: '',
                                      x_align: St.Align.START,
                                      can_focus: true });
        this.actor.insert_child_below(this._title, null);

        this._title.connect('clicked', this._onTitleClicked.bind(this));
        this._title.connect('key-focus-in', this._onKeyFocusIn.bind(this));

        Shell.AppSystem.get_default().connect('installed-changed',
                                              this._appInstalledChanged.bind(this));
        this._appInstalledChanged();
    },

    _ignoreEvent(event) {
        this._eventSource.ignoreEvent(event);
    },

    setEventSource(eventSource) {
        this._eventSource = eventSource;
        this._eventSource.connect('changed', this._reloadEvents.bind(this));
    },

    get allowed() {
        return Main.sessionMode.showCalendarEvents;
    },

    _updateTitle() {
        this._title.visible = !isToday(this._date);

        if (!this._title.visible)
            return;

        let dayFormat;
        let now = new Date();
        if (sameYear(this._date, now))
            /* Translators: Shown on calendar heading when selected day occurs on current year */
            dayFormat = Shell.util_translate_time_string(NC_("calendar heading",
                                                             "%A, %B %d"));
        else
            /* Translators: Shown on calendar heading when selected day occurs on different year */
            dayFormat = Shell.util_translate_time_string(NC_("calendar heading",
                                                             "%A, %B %d, %Y"));
        this._title.label = this._date.toLocaleFormat(dayFormat);
    },

    _reloadEvents() {
        if (this._eventSource.isLoading)
            return;

        this._reloading = true;

        let periodBegin = _getBeginningOfDay(this._date);
        let periodEnd = _getEndOfDay(this._date);
        let events = this._eventSource.getEvents(periodBegin, periodEnd);

        let ids = events.map(e => e.id);
        this._messageById.forEach((message, id) => {
            if (ids.includes(id))
                return;
            this._messageById.delete(id);
            this.removeMessage(message);
        });

        for (let i = 0; i < events.length; i++) {
            let event = events[i];

            let message = this._messageById.get(event.id);
            if (!message) {
                message = new EventMessage(event, this._date);
                message.connect('close', () => {
                    this._ignoreEvent(event);
                });
                this._messageById.set(event.id, message);
                this.addMessage(message, false);
            } else {
                this.moveMessage(message, i, false);
            }
        }

        this._reloading = false;
        this._sync();
    },

    _appInstalledChanged() {
        this._calendarApp = undefined;
        this._title.reactive = (this._getCalendarApp() != null);
    },

    _getCalendarApp() {
        if (this._calendarApp !== undefined)
            return this._calendarApp;

        let apps = Gio.AppInfo.get_recommended_for_type('text/calendar');
        if (apps && (apps.length > 0)) {
            let app = Gio.AppInfo.get_default_for_type('text/calendar', false);
            let defaultInRecommended = apps.some(a => a.equal(app));
            this._calendarApp = defaultInRecommended ? app : apps[0];
        } else {
            this._calendarApp = null;
        }
        return this._calendarApp;
    },

    _onTitleClicked() {
        Main.overview.hide();
        Main.panel.closeCalendar();

        let app = this._getCalendarApp();
        if (app.get_id() == 'evolution.desktop')
            app = Gio.DesktopAppInfo.new('evolution-calendar.desktop');
        app.launch([], global.create_app_launch_context(0, -1));
    },

    setDate(date) {
        this.parent(date);
        this._updateTitle();
        this._reloadEvents();
    },

    _shouldShow() {
        return !this.empty || !isToday(this._date);
    },

    _sync() {
        if (this._reloading)
            return;

        this.parent();
    }
});

var NotificationSection = new Lang.Class({
    Name: 'NotificationSection',
    Extends: MessageList.MessageListSection,

    _init() {
        this.parent();

        this._sources = new Map();
        this._nUrgent = 0;

        Main.messageTray.connect('source-added', this._sourceAdded.bind(this));
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source);
        });

        this.actor.connect('notify::mapped', this._onMapped.bind(this));
    },

    get allowed() {
        return Main.sessionMode.hasNotifications &&
               !Main.sessionMode.isGreeter;
    },

    _createTimeLabel(datetime) {
        let label = new St.Label({ style_class: 'event-time',
                                   x_align: Clutter.ActorAlign.START,
                                   y_align: Clutter.ActorAlign.END });
        label.connect('notify::mapped', () => {
            if (label.mapped)
                label.text = Util.formatTimeSpan(datetime);
        });
        return label;
    },

    _sourceAdded(tray, source) {
        let obj = {
            destroyId: 0,
            notificationAddedId: 0,
        };

        obj.destroyId = source.connect('destroy', source => {
            this._onSourceDestroy(source, obj);
        });
        obj.notificationAddedId = source.connect('notification-added',
                                                 this._onNotificationAdded.bind(this));

        this._sources.set(source, obj);
    },

    _onNotificationAdded(source, notification) {
        let message = new NotificationMessage(notification);
        message.setSecondaryActor(this._createTimeLabel(notification.datetime));

        let isUrgent = notification.urgency == MessageTray.Urgency.CRITICAL;

        let updatedId = notification.connect('updated', () => {
            message.setSecondaryActor(this._createTimeLabel(notification.datetime));
            this.moveMessage(message, isUrgent ? 0 : this._nUrgent, this.actor.mapped);
        });
        let destroyId = notification.connect('destroy', () => {
            notification.disconnect(destroyId);
            notification.disconnect(updatedId);
            if (isUrgent)
                this._nUrgent--;
        });

        if (isUrgent) {
            // Keep track of urgent notifications to keep them on top
            this._nUrgent++;
        } else if (this.mapped) {
            // Only acknowledge non-urgent notifications in case it
            // has important actions that are inaccessible when not
            // shown as banner
            notification.acknowledged = true;
        }

        let index = isUrgent ? 0 : this._nUrgent;
        this.addMessageAtIndex(message, index, this.actor.mapped);
    },

    _onSourceDestroy(source, obj) {
        source.disconnect(obj.destroyId);
        source.disconnect(obj.notificationAddedId);

        this._sources.delete(source);
    },

    _onMapped() {
        if (!this.actor.mapped)
            return;

        for (let message of this._messages.keys())
            if (message.notification.urgency != MessageTray.Urgency.CRITICAL)
                message.notification.acknowledged = true;
    },

    _shouldShow() {
        return !this.empty && isToday(this._date);
    }
});

var Placeholder = new Lang.Class({
    Name: 'Placeholder',

    _init() {
        this.actor = new St.BoxLayout({ style_class: 'message-list-placeholder',
                                        vertical: true });

        this._date = new Date();

        let todayFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/no-notifications.svg');
        let otherFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/no-events.svg');
        this._todayIcon = new Gio.FileIcon({ file: todayFile });
        this._otherIcon = new Gio.FileIcon({ file: otherFile });

        this._icon = new St.Icon();
        this.actor.add_actor(this._icon);

        this._label = new St.Label();
        this.actor.add_actor(this._label);

        this._sync();
    },

    setDate(date) {
        if (sameDay(this._date, date))
            return;
        this._date = date;
        this._sync();
    },

    _sync() {
        let today = isToday(this._date);
        if (today && this._icon.gicon == this._todayIcon)
            return;
        if (!today && this._icon.gicon == this._otherIcon)
            return;

        if (today) {
            this._icon.gicon = this._todayIcon;
            this._label.text = _("No Notifications");
        } else {
            this._icon.gicon = this._otherIcon;
            this._label.text = _("No Events");
        }
    }
});

var CalendarMessageList = new Lang.Class({
    Name: 'CalendarMessageList',

    _init() {
        this.actor = new St.Widget({ style_class: 'message-list',
                                     layout_manager: new Clutter.BinLayout(),
                                     x_expand: true, y_expand: true });

        this._placeholder = new Placeholder();
        this.actor.add_actor(this._placeholder.actor);

        let box = new St.BoxLayout({ vertical: true,
                                     x_expand: true, y_expand: true });
        this.actor.add_actor(box);

        this._scrollView = new St.ScrollView({ style_class: 'vfade',
                                               overlay_scrollbars: true,
                                               x_expand: true, y_expand: true,
                                               x_fill: true, y_fill: true });
        this._scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        box.add_actor(this._scrollView);

        this._clearButton = new St.Button({ style_class: 'message-list-clear-button button',
                                            label: _("Clear All"),
                                            can_focus: true });
        this._clearButton.set_x_align(Clutter.ActorAlign.END);
        this._clearButton.connect('clicked', () => {
            let sections = [...this._sections.keys()];
            sections.forEach((s) => { s.clear(); });
        });
        box.add_actor(this._clearButton);

        this._sectionList = new St.BoxLayout({ style_class: 'message-list-sections',
                                               vertical: true,
                                               y_expand: true,
                                               y_align: Clutter.ActorAlign.START });
        this._scrollView.add_actor(this._sectionList);
        this._sections = new Map();

        this._mediaSection = new Mpris.MediaSection();
        this._addSection(this._mediaSection);

        this._notificationSection = new NotificationSection();
        this._addSection(this._notificationSection);

        this._eventsSection = new EventsSection();
        this._addSection(this._eventsSection);

        Main.sessionMode.connect('updated', this._sync.bind(this));
    },

    _addSection(section) {
        let obj = {
            destroyId: 0,
            visibleId:  0,
            emptyChangedId: 0,
            canClearChangedId: 0,
            keyFocusId: 0
        };
        obj.destroyId = section.actor.connect('destroy', () => {
            this._removeSection(section);
        });
        obj.visibleId = section.actor.connect('notify::visible',
                                              this._sync.bind(this));
        obj.emptyChangedId = section.connect('empty-changed',
                                             this._sync.bind(this));
        obj.canClearChangedId = section.connect('can-clear-changed',
                                                this._sync.bind(this));
        obj.keyFocusId = section.connect('key-focus-in',
                                         this._onKeyFocusIn.bind(this));

        this._sections.set(section, obj);
        this._sectionList.add_actor(section.actor);
        this._sync();
    },

    _removeSection(section) {
        let obj = this._sections.get(section);
        section.actor.disconnect(obj.destroyId);
        section.actor.disconnect(obj.visibleId);
        section.disconnect(obj.emptyChangedId);
        section.disconnect(obj.canClearChangedId);
        section.disconnect(obj.keyFocusId);

        this._sections.delete(section);
        this._sectionList.remove_actor(section.actor);
        this._sync();
    },

    _onKeyFocusIn(section, actor) {
        Util.ensureActorVisibleInScrollView(this._scrollView, actor);
    },

    _sync() {
        let sections = [...this._sections.keys()];
        let visible = sections.some(s => s.allowed);
        this.actor.visible = visible;
        if (!visible)
            return;

        let empty = sections.every(s => s.empty || !s.actor.visible);
        this._placeholder.actor.visible = empty;
        this._clearButton.visible = !empty;

        let canClear = sections.some(s => s.canClear && s.actor.visible);
        this._clearButton.reactive = canClear;
    },

    setEventSource(eventSource) {
        this._eventsSection.setEventSource(eventSource);
    },

    setDate(date) {
        for (let section of this._sections.keys())
            section.setDate(date);
        this._placeholder.setDate(date);
    }
});
