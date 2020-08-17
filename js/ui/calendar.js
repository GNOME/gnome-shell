// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Calendar, CalendarMessageList, DBusEventSource */

const { Clutter, Gio, GLib, GObject, Shell, St } = imports.gi;

const Main = imports.ui.main;
const MessageList = imports.ui.messageList;
const MessageTray = imports.ui.messageTray;
const Mpris = imports.ui.mpris;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

const { loadInterfaceXML } = imports.misc.fileUtils;

var MSECS_IN_DAY = 24 * 60 * 60 * 1000;
var SHOW_WEEKDATE_KEY = 'show-weekdate';
var ELLIPSIS_CHAR = '\u2026';

var MESSAGE_ICON_SIZE = -1; // pick up from CSS

var NC_ = (context, str) => '%s\u0004%s'.format(context, str);

function sameYear(dateA, dateB) {
    return dateA.getYear() == dateB.getYear();
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
    return !days.includes(date.getDay().toString());
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
        NC_("grid saturday", "S"),
    ];
    return Shell.util_translate_time_string(abbreviations[dayNumber]);
}

// Abstraction for an appointment/event in a calendar

var CalendarEvent = class CalendarEvent {
    constructor(id, date, end, summary, allDay) {
        this.id = id;
        this.date = date;
        this.end = end;
        this.summary = summary;
        this.allDay = allDay;
    }
};

// Interface for appointments/events - e.g. the contents of a calendar
//

var EventSourceBase = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
    Properties: {
        'has-calendars': GObject.ParamSpec.boolean(
            'has-calendars', 'has-calendars', 'has-calendars',
            GObject.ParamFlags.READABLE,
            false),
        'is-loading': GObject.ParamSpec.boolean(
            'is-loading', 'is-loading', 'is-loading',
            GObject.ParamFlags.READABLE,
            false),
    },
    Signals: { 'changed': {} },
}, class EventSourceBase extends GObject.Object {
    get isLoading() {
        throw new GObject.NotImplementedError('isLoading in %s'.format(this.constructor.name));
    }

    get hasCalendars() {
        throw new GObject.NotImplementedError('hasCalendars in %s'.format(this.constructor.name));
    }

    destroy() {
    }

    requestRange(_begin, _end) {
        throw new GObject.NotImplementedError('requestRange in %s'.format(this.constructor.name));
    }

    getEvents(_begin, _end) {
        throw new GObject.NotImplementedError('getEvents in %s'.format(this.constructor.name));
    }

    hasEvents(_day) {
        throw new GObject.NotImplementedError('hasEvents in %s'.format(this.constructor.name));
    }
});

var EmptyEventSource = GObject.registerClass(
class EmptyEventSource extends EventSourceBase {
    get isLoading() {
        return false;
    }

    get hasCalendars() {
        return false;
    }

    requestRange(_begin, _end) {
    }

    getEvents(_begin, _end) {
        let result = [];
        return result;
    }

    hasEvents(_day) {
        return false;
    }
});

const CalendarServerIface = loadInterfaceXML('org.gnome.Shell.CalendarServer');

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

function _dateIntervalsOverlap(a0, a1, b0, b1) {
    if (a1 <= b0)
        return false;
    else if (b1 <= a0)
        return false;
    else
        return true;
}

// an implementation that reads data from a session bus service
var DBusEventSource = GObject.registerClass(
class DBusEventSource extends EventSourceBase {
    _init() {
        super._init();
        this._resetCache();
        this._isLoading = false;

        this._initialized = false;
        this._dbusProxy = new CalendarServer();
        this._dbusProxy.init_async(GLib.PRIORITY_DEFAULT, null, (object, result) => {
            let loaded = false;

            try {
                this._dbusProxy.init_finish(result);
                loaded = true;
            } catch (e) {
                if (e.matches(Gio.DBusError, Gio.DBusError.TIMED_OUT)) {
                    // Ignore timeouts and install signals as normal, because with high
                    // probability the service will appear later on, and we will get a
                    // NameOwnerChanged which will finish loading
                    //
                    // (But still _initialized to false, because the proxy does not know
                    // about the HasCalendars property and would cause an exception trying
                    // to read it)
                } else {
                    log('Error loading calendars: %s'.format(e.message));
                    return;
                }
            }

            this._dbusProxy.connectSignal('EventsAddedOrUpdated',
                this._onEventsAddedOrUpdated.bind(this));
            this._dbusProxy.connectSignal('EventsRemoved',
                this._onEventsRemoved.bind(this));
            this._dbusProxy.connectSignal('ClientDisappeared',
                this._onClientDisappeared.bind(this));

            this._dbusProxy.connect('notify::g-name-owner', () => {
                if (this._dbusProxy.g_name_owner)
                    this._onNameAppeared();
                else
                    this._onNameVanished();
            });

            this._dbusProxy.connect('g-properties-changed', () => {
                this.notify('has-calendars');
            });

            this._initialized = loaded;
            if (loaded) {
                this.notify('has-calendars');
                this._onNameAppeared();
            }
        });
    }

    destroy() {
        this._dbusProxy.run_dispose();
    }

    get hasCalendars() {
        if (this._initialized)
            return this._dbusProxy.HasCalendars;
        else
            return false;
    }

    get isLoading() {
        return this._isLoading;
    }

    _resetCache() {
        this._events = new Map();
        this._lastRequestBegin = null;
        this._lastRequestEnd = null;
    }

    _onNameAppeared() {
        this._initialized = true;
        this._resetCache();
        this._loadEvents(true);
    }

    _onNameVanished() {
        this._resetCache();
        this.emit('changed');
    }

    _onEventsAddedOrUpdated(dbusProxy, nameOwner, argArray) {
        const [appointments = []] = argArray;
        let changed = false;

        for (let n = 0; n < appointments.length; n++) {
            const [id, summary, allDay, startTime, endTime] = appointments[n];
            const date = new Date(startTime * 1000);
            const end = new Date(endTime * 1000);
            let event = new CalendarEvent(id, date, end, summary, allDay);
            this._events.set(event.id, event);

            changed = true;
        }

        if (changed)
            this.emit('changed');
    }

    _onEventsRemoved(dbusProxy, nameOwner, argArray) {
        const [ids = []] = argArray;

        let changed = false;
        for (const id of ids)
            changed |= this._events.delete(id);

        if (changed)
            this.emit('changed');
    }

    _onClientDisappeared(dbusProxy, nameOwner, argArray) {
        let [sourceUid = ''] = argArray;
        sourceUid += '\n';

        let changed = false;
        for (const id of this._events.keys()) {
            if (id.startsWith(sourceUid))
                changed |= this._events.delete(id);
        }

        if (changed)
            this.emit('changed');
    }

    _loadEvents(forceReload) {
        // Ignore while loading
        if (!this._initialized)
            return;

        if (this._curRequestBegin && this._curRequestEnd) {
            if (forceReload) {
                this._events.clear();
                this.emit('changed');
            }
            this._dbusProxy.SetTimeRangeRemote(
                this._curRequestBegin.getTime() / 1000,
                this._curRequestEnd.getTime() / 1000,
                forceReload,
                Gio.DBusCallFlags.NONE);
        }
    }

    requestRange(begin, end) {
        if (!(_datesEqual(begin, this._lastRequestBegin) && _datesEqual(end, this._lastRequestEnd))) {
            this._lastRequestBegin = begin;
            this._lastRequestEnd = end;
            this._curRequestBegin = begin;
            this._curRequestEnd = end;
            this._loadEvents(true);
        }
    }

    *_getFilteredEvents(begin, end) {
        for (const event of this._events.values()) {
            if (_dateIntervalsOverlap(event.date, event.end, begin, end))
                yield event;
        }
    }

    getEvents(begin, end) {
        let result = [...this._getFilteredEvents(begin, end)];

        result.sort((event1, event2) => {
            // sort events by end time on ending day
            let d1 = event1.date < begin && event1.end <= end ? event1.end : event1.date;
            let d2 = event2.date < begin && event2.end <= end ? event2.end : event2.date;
            return d1.getTime() - d2.getTime();
        });
        return result;
    }

    hasEvents(day) {
        let dayBegin = _getBeginningOfDay(day);
        let dayEnd = _getEndOfDay(day);

        const { done } = this._getFilteredEvents(dayBegin, dayEnd).next();
        return !done;
    }
});

var Calendar = GObject.registerClass({
    Signals: { 'selected-date-changed': { param_types: [GLib.DateTime.$gtype] } },
}, class Calendar extends St.Widget {
    _init() {
        this._weekStart = Shell.util_get_week_start();
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.desktop.calendar' });

        this._settings.connect('changed::%s'.format(SHOW_WEEKDATE_KEY), this._onSettingsChange.bind(this));
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

        super._init({
            style_class: 'calendar',
            layout_manager: new Clutter.GridLayout(),
            reactive: true,
        });

        this._buildHeader();
    }

    setEventSource(eventSource) {
        if (!(eventSource instanceof EventSourceBase))
            throw new Error('Event source is not valid type');

        this._eventSource = eventSource;
        this._eventSource.connect('changed', () => {
            this._rebuildCalendar();
            this._update();
        });
        this._rebuildCalendar();
        this._update();
    }

    // Sets the calendar to show a specific date
    setDate(date) {
        if (sameDay(date, this._selectedDate))
            return;

        this._selectedDate = date;
        this._update();

        let datetime = GLib.DateTime.new_from_unix_local(
            this._selectedDate.getTime() / 1000);
        this.emit('selected-date-changed', datetime);
    }

    updateTimeZone() {
        // The calendar need to be rebuilt after a time zone update because
        // the date might have changed.
        this._rebuildCalendar();
        this._update();
    }

    _buildHeader() {
        let layout = this.layout_manager;
        let offsetCols = this._useWeekdate ? 1 : 0;
        this.destroy_all_children();

        // Top line of the calendar '<| September 2009 |>'
        this._topBox = new St.BoxLayout();
        layout.attach(this._topBox, 0, 0, offsetCols + 7, 1);

        this._backButton = new St.Button({ style_class: 'calendar-change-month-back pager-button',
                                           accessible_name: _("Previous month"),
                                           can_focus: true });
        this._backButton.add_actor(new St.Icon({ icon_name: 'pan-start-symbolic' }));
        this._topBox.add(this._backButton);
        this._backButton.connect('clicked', this._onPrevMonthButtonClicked.bind(this));

        this._monthLabel = new St.Label({
            style_class: 'calendar-month-label',
            can_focus: true,
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });
        this._topBox.add_child(this._monthLabel);

        this._forwardButton = new St.Button({ style_class: 'calendar-change-month-forward pager-button',
                                              accessible_name: _("Next month"),
                                              can_focus: true });
        this._forwardButton.add_actor(new St.Icon({ icon_name: 'pan-end-symbolic' }));
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
            if (this.get_text_direction() == Clutter.TextDirection.RTL)
                col = 6 - (7 + iter.getDay() - this._weekStart) % 7;
            else
                col = offsetCols + (7 + iter.getDay() - this._weekStart) % 7;
            layout.attach(label, col, 1, 1, 1);
            iter.setTime(iter.getTime() + MSECS_IN_DAY);
        }

        // All the children after this are days, and get removed when we update the calendar
        this._firstDayIndex = this.get_n_children();
    }

    vfunc_scroll_event(scrollEvent) {
        switch (scrollEvent.direction) {
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
    }

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
        } else {
            newDate.setMonth(oldMonth - 1);
            if (newDate.getMonth() != oldMonth - 1) {
                let day = 32 - new Date(newDate.getFullYear(), oldMonth - 1, 32).getDate();
                newDate = new Date(newDate.getFullYear(), oldMonth - 1, day);
            }
        }

        this._backButton.grab_key_focus();

        this.setDate(newDate);
    }

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
        } else {
            newDate.setMonth(oldMonth + 1);
            if (newDate.getMonth() != oldMonth + 1) {
                let day = 32 - new Date(newDate.getFullYear(), oldMonth + 1, 32).getDate();
                newDate = new Date(newDate.getFullYear(), oldMonth + 1, day);
            }
        }

        this._forwardButton.grab_key_focus();

        this.setDate(newDate);
    }

    _onSettingsChange() {
        this._useWeekdate = this._settings.get_boolean(SHOW_WEEKDATE_KEY);
        this._buildHeader();
        this._rebuildCalendar();
        this._update();
    }

    _rebuildCalendar() {
        let now = new Date();

        // Remove everything but the topBox and the weekday labels
        let children = this.get_children();
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

        let daysToWeekStart = (7 + beginDate.getDay() - this._weekStart) % 7;
        let startsOnWeekStart = daysToWeekStart == 0;
        let weekPadding = startsOnWeekStart ? 7 : 0;

        beginDate.setTime(beginDate.getTime() - (weekPadding + daysToWeekStart) * MSECS_IN_DAY);

        let layout = this.layout_manager;
        let iter = new Date(beginDate);
        let row = 2;
        // nRows here means 6 weeks + one header + one navbar
        let nRows = 8;
        while (row < nRows) {
            // xgettext:no-javascript-format
            let button = new St.Button({ label: iter.toLocaleFormat(C_("date day number format", "%d")),
                                         can_focus: true });
            let rtl = button.get_text_direction() == Clutter.TextDirection.RTL;

            if (this._eventSource instanceof EmptyEventSource)
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
                styleClass = 'calendar-day-top %s'.format(styleClass);

            let leftMost = rtl
                ? iter.getDay() == (this._weekStart + 6) % 7
                : iter.getDay() == this._weekStart;
            if (leftMost)
                styleClass = 'calendar-day-left %s'.format(styleClass);

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
            layout.attach(button, col, row, 1, 1);

            this._buttons.push(button);

            if (this._useWeekdate && iter.getDay() == 4) {
                let label = new St.Label({ text: iter.toLocaleFormat('%V'),
                                           style_class: 'calendar-day-base calendar-week-number',
                                           can_focus: true });
                let weekFormat = Shell.util_translate_time_string(N_("Week %V"));
                label.clutter_text.y_align = Clutter.ActorAlign.CENTER;
                label.accessible_name = iter.toLocaleFormat(weekFormat);
                layout.attach(label, rtl ? 7 : 0, row, 1, 1);
            }

            iter.setTime(iter.getTime() + MSECS_IN_DAY);

            if (iter.getDay() == this._weekStart)
                row++;
        }

        // Signal to the event source that we are interested in events
        // only from this date range
        this._eventSource.requestRange(beginDate, iter);
    }

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
            } else {
                button.remove_style_pseudo_class('selected');
            }
        });
    }
});

var EventMessage = GObject.registerClass(
class EventMessage extends MessageList.Message {
    _init(event, date) {
        super._init('', '');

        this._date = date;

        this.update(event);

        this._icon = new St.Icon({ icon_name: 'x-office-calendar-symbolic' });
        this.setIcon(this._icon);
    }

    vfunc_style_changed() {
        let iconVisible = this.get_parent().has_style_pseudo_class('first-child');
        this._icon.opacity = iconVisible ? 255 : 0;
        super.vfunc_style_changed();
    }

    update(event) {
        this._event = event;

        this.setTitle(this._formatEventTime());
        this.setBody(event.summary);
    }

    _formatEventTime() {
        let periodBegin = _getBeginningOfDay(this._date);
        let periodEnd = _getEndOfDay(this._date);
        let allDay = this._event.allDay || (this._event.date <= periodBegin &&
                                             this._event.end >= periodEnd);
        let title;
        if (allDay) {
            /* Translators: Shown in calendar event list for all day events
             * Keep it short, best if you can use less then 10 characters
             */
            title = C_("event list time", "All Day");
        } else {
            let date = this._event.date >= periodBegin
                ? this._event.date
                : this._event.end;
            title = Util.formatTime(date, { timeOnly: true });
        }

        let rtl = Clutter.get_default_text_direction() == Clutter.TextDirection.RTL;
        if (this._event.date < periodBegin && !this._event.allDay) {
            if (rtl)
                title = '%s%s'.format(title, ELLIPSIS_CHAR);
            else
                title = '%s%s'.format(ELLIPSIS_CHAR, title);
        }
        if (this._event.end > periodEnd && !this._event.allDay) {
            if (rtl)
                title = '%s%s'.format(ELLIPSIS_CHAR, title);
            else
                title = '%s%s'.format(title, ELLIPSIS_CHAR);
        }
        return title;
    }
});

var NotificationMessage = GObject.registerClass(
class NotificationMessage extends MessageList.Message {
    _init(notification) {
        super._init(notification.title, notification.bannerBodyText);
        this.setUseBodyMarkup(notification.bannerBodyMarkup);

        this.notification = notification;

        this.setIcon(this._getIcon());

        this.connect('close', () => {
            this._closed = true;
            if (this.notification)
                this.notification.destroy(MessageTray.NotificationDestroyedReason.DISMISSED);
        });
        this._destroyId = notification.connect('destroy', () => {
            this._disconnectNotificationSignals();
            this.notification = null;
            if (!this._closed)
                this.close();
        });
        this._updatedId = notification.connect('updated',
                                               this._onUpdated.bind(this));
    }

    _getIcon() {
        if (this.notification.gicon) {
            return new St.Icon({ gicon: this.notification.gicon,
                                 icon_size: MESSAGE_ICON_SIZE });
        } else {
            return this.notification.source.createIcon(MESSAGE_ICON_SIZE);
        }
    }

    _onUpdated(n, _clear) {
        this.setIcon(this._getIcon());
        this.setTitle(n.title);
        this.setBody(n.bannerBodyText);
        this.setUseBodyMarkup(n.bannerBodyMarkup);
    }

    vfunc_clicked() {
        this.notification.activate();
    }

    _onDestroy() {
        super._onDestroy();
        this._disconnectNotificationSignals();
    }

    _disconnectNotificationSignals() {
        if (this._updatedId)
            this.notification.disconnect(this._updatedId);
        this._updatedId = 0;

        if (this._destroyId)
            this.notification.disconnect(this._destroyId);
        this._destroyId = 0;
    }

    canClose() {
        return true;
    }
});

var EventsSection = GObject.registerClass(
class EventsSection extends MessageList.MessageListSection {
    _init() {
        super._init();

        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed', this._reloadEvents.bind(this));
        this._eventSource = new EmptyEventSource();

        this._messageById = new Map();

        this._title = new St.Button({ style_class: 'events-section-title',
                                      label: '',
                                      can_focus: true });
        this._title.child.x_align = Clutter.ActorAlign.START;
        this.insert_child_below(this._title, null);

        this._title.connect('clicked', this._onTitleClicked.bind(this));
        this._title.connect('key-focus-in', this._onKeyFocusIn.bind(this));

        this._appSys = Shell.AppSystem.get_default();
        this._appSys.connect('installed-changed',
            this._appInstalledChanged.bind(this));
        this._appInstalledChanged();
    }

    setEventSource(eventSource) {
        if (!(eventSource instanceof EventSourceBase))
            throw new Error('Event source is not valid type');

        this._eventSource = eventSource;
        this._eventSource.connect('changed', this._reloadEvents.bind(this));
    }

    get allowed() {
        return Main.sessionMode.showCalendarEvents;
    }

    _updateTitle() {
        this._title.visible = !isToday(this._date);

        if (!this._title.visible)
            return;

        let dayFormat;
        let now = new Date();
        if (sameYear(this._date, now)) {
            /* Translators: Shown on calendar heading when selected day occurs on current year */
            dayFormat = Shell.util_translate_time_string(NC_("calendar heading", "%A, %B %-d"));
        } else {
            /* Translators: Shown on calendar heading when selected day occurs on different year */
            dayFormat = Shell.util_translate_time_string(NC_("calendar heading", "%A, %B %-d, %Y"));
        }
        this._title.label = this._date.toLocaleFormat(dayFormat);
    }

    _reloadEvents() {
        if (this._eventSource.isLoading || this._reloading)
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
                this._messageById.set(event.id, message);
                this.addMessage(message, false);
            } else {
                message.update(event);
                this.moveMessage(message, i, false);
            }
        }

        this._reloading = false;
        this._sync();
    }

    _appInstalledChanged() {
        this._calendarApp = undefined;
        this._title.reactive = this._getCalendarApp() != null;
    }

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
    }

    _onTitleClicked() {
        Main.overview.hide();
        Main.panel.closeCalendar();

        let appInfo = this._getCalendarApp();
        if (appInfo.get_id() === 'org.gnome.Evolution.desktop') {
            let app = this._appSys.lookup_app('evolution-calendar.desktop');
            if (app)
                appInfo = app.app_info;
        }
        appInfo.launch([], global.create_app_launch_context(0, -1));
    }

    setDate(date) {
        super.setDate(date);
        this._updateTitle();
        this._reloadEvents();
    }

    _shouldShow() {
        return !this.empty || !isToday(this._date);
    }

    _sync() {
        if (this._reloading)
            return;

        super._sync();
    }
});

var TimeLabel = GObject.registerClass(
class NotificationTimeLabel extends St.Label {
    _init(datetime) {
        super._init({
            style_class: 'event-time',
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.END,
        });
        this._datetime = datetime;
    }

    vfunc_map() {
        this.text = Util.formatTimeSpan(this._datetime);
        super.vfunc_map();
    }
});

var NotificationSection = GObject.registerClass(
class NotificationSection extends MessageList.MessageListSection {
    _init() {
        super._init();

        this._sources = new Map();
        this._nUrgent = 0;

        Main.messageTray.connect('source-added', this._sourceAdded.bind(this));
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source);
        });
    }

    get allowed() {
        return Main.sessionMode.hasNotifications &&
               !Main.sessionMode.isGreeter;
    }

    _sourceAdded(tray, source) {
        let obj = {
            destroyId: 0,
            notificationAddedId: 0,
        };

        obj.destroyId = source.connect('destroy', () => {
            this._onSourceDestroy(source, obj);
        });
        obj.notificationAddedId = source.connect('notification-added',
                                                 this._onNotificationAdded.bind(this));

        this._sources.set(source, obj);
    }

    _onNotificationAdded(source, notification) {
        let message = new NotificationMessage(notification);
        message.setSecondaryActor(new TimeLabel(notification.datetime));

        let isUrgent = notification.urgency == MessageTray.Urgency.CRITICAL;

        let updatedId = notification.connect('updated', () => {
            message.setSecondaryActor(new TimeLabel(notification.datetime));
            this.moveMessage(message, isUrgent ? 0 : this._nUrgent, this.mapped);
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
        this.addMessageAtIndex(message, index, this.mapped);
    }

    _onSourceDestroy(source, obj) {
        source.disconnect(obj.destroyId);
        source.disconnect(obj.notificationAddedId);

        this._sources.delete(source);
    }

    vfunc_map() {
        this._messages.forEach(message => {
            if (message.notification.urgency != MessageTray.Urgency.CRITICAL)
                message.notification.acknowledged = true;
        });
        super.vfunc_map();
    }

    _shouldShow() {
        return !this.empty && isToday(this._date);
    }
});

var Placeholder = GObject.registerClass(
class Placeholder extends St.BoxLayout {
    _init() {
        super._init({ style_class: 'message-list-placeholder', vertical: true });
        this._date = new Date();

        let todayFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/no-notifications.svg');
        let otherFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/no-events.svg');
        this._todayIcon = new Gio.FileIcon({ file: todayFile });
        this._otherIcon = new Gio.FileIcon({ file: otherFile });

        this._icon = new St.Icon();
        this.add_actor(this._icon);

        this._label = new St.Label();
        this.add_actor(this._label);

        this._sync();
    }

    setDate(date) {
        if (sameDay(this._date, date))
            return;
        this._date = date;
        this._sync();
    }

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

const DoNotDisturbSwitch = GObject.registerClass(
class DoNotDisturbSwitch extends PopupMenu.Switch {
    _init() {
        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.notifications',
        });

        super._init(this._settings.get_boolean('show-banners'));

        this._settings.bind('show-banners',
            this, 'state',
            Gio.SettingsBindFlags.INVERT_BOOLEAN);

        this.connect('destroy', () => {
            this._settings.run_dispose();
            this._settings = null;
        });
    }
});

var CalendarMessageList = GObject.registerClass(
class CalendarMessageList extends St.Widget {
    _init() {
        super._init({
            style_class: 'message-list',
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });

        this._placeholder = new Placeholder();
        this.add_actor(this._placeholder);

        let box = new St.BoxLayout({ vertical: true,
                                     x_expand: true, y_expand: true });
        this.add_actor(box);

        this._scrollView = new St.ScrollView({
            style_class: 'vfade',
            overlay_scrollbars: true,
            x_expand: true, y_expand: true,
        });
        this._scrollView.set_policy(St.PolicyType.NEVER, St.PolicyType.AUTOMATIC);
        box.add_actor(this._scrollView);

        let hbox = new St.BoxLayout({ style_class: 'message-list-controls' });
        box.add_child(hbox);

        const dndLabel = new St.Label({
            text: _('Do Not Disturb'),
            y_align: Clutter.ActorAlign.CENTER,
        });
        hbox.add_child(dndLabel);

        this._dndSwitch = new DoNotDisturbSwitch();
        this._dndButton = new St.Button({
            can_focus: true,
            toggle_mode: true,
            child: this._dndSwitch,
            label_actor: dndLabel,
        });
        this._dndSwitch.bind_property('state',
            this._dndButton, 'checked',
            GObject.BindingFlags.BIDIRECTIONAL | GObject.BindingFlags.SYNC_CREATE);
        hbox.add_child(this._dndButton);

        this._clearButton = new St.Button({
            style_class: 'message-list-clear-button button',
            label: _('Clear'),
            can_focus: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
        });
        this._clearButton.connect('clicked', () => {
            this._sectionList.get_children().forEach(s => s.clear());
        });
        hbox.add_actor(this._clearButton);

        this._placeholder.bind_property('visible',
            this._clearButton, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN);

        this._sectionList = new St.BoxLayout({ style_class: 'message-list-sections',
                                               vertical: true,
                                               x_expand: true,
                                               y_expand: true,
                                               y_align: Clutter.ActorAlign.START });
        this._sectionList.connect('actor-added', this._sync.bind(this));
        this._sectionList.connect('actor-removed', this._sync.bind(this));
        this._scrollView.add_actor(this._sectionList);

        this._mediaSection = new Mpris.MediaSection();
        this._addSection(this._mediaSection);

        this._notificationSection = new NotificationSection();
        this._addSection(this._notificationSection);

        this._eventsSection = new EventsSection();
        this._addSection(this._eventsSection);

        Main.sessionMode.connect('updated', this._sync.bind(this));
    }

    _addSection(section) {
        let connectionsIds = [];

        for (let prop of ['visible', 'empty', 'can-clear']) {
            connectionsIds.push(
                section.connect('notify::%s'.format(prop), this._sync.bind(this)));
        }
        connectionsIds.push(section.connect('message-focused', (_s, messageActor) => {
            Util.ensureActorVisibleInScrollView(this._scrollView, messageActor);
        }));

        connectionsIds.push(section.connect('destroy', () => {
            connectionsIds.forEach(id => section.disconnect(id));
            this._sectionList.remove_actor(section);
        }));

        this._sectionList.add_actor(section);
    }

    _sync() {
        let sections = this._sectionList.get_children();
        let visible = sections.some(s => s.allowed);
        this.visible = visible;
        if (!visible)
            return;

        let empty = sections.every(s => s.empty || !s.visible);
        this._placeholder.visible = empty;

        let canClear = sections.some(s => s.canClear && s.visible);
        this._clearButton.reactive = canClear;
    }

    setEventSource(eventSource) {
        this._eventsSection.setEventSource(eventSource);
    }

    setDate(date) {
        this._sectionList.get_children().forEach(s => s.setDate(date));
        this._placeholder.setDate(date);
    }
});
