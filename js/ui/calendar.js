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

var SHOW_WEEKDATE_KEY = 'show-weekdate';

var MESSAGE_ICON_SIZE = -1; // pick up from CSS

var NC_ = (context, str) => `${context}\u0004${str}`;

function sameYear(dateA, dateB) {
    return dateA.getYear() == dateB.getYear();
}

function sameMonth(dateA, dateB) {
    return sameYear(dateA, dateB) && (dateA.getMonth() == dateB.getMonth());
}

function sameDay(dateA, dateB) {
    return sameMonth(dateA, dateB) && (dateA.getDate() == dateB.getDate());
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
    const ret = _getBeginningOfDay(date);
    ret.setDate(ret.getDate() + 1);
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
    constructor(id, date, end, summary) {
        this.id = id;
        this.date = date;
        this.end = end;
        this.summary = summary;
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
        throw new GObject.NotImplementedError(`isLoading in ${this.constructor.name}`);
    }

    get hasCalendars() {
        throw new GObject.NotImplementedError(`hasCalendars in ${this.constructor.name}`);
    }

    destroy() {
    }

    requestRange(_begin, _end) {
        throw new GObject.NotImplementedError(`requestRange in ${this.constructor.name}`);
    }

    getEvents(_begin, _end) {
        throw new GObject.NotImplementedError(`getEvents in ${this.constructor.name}`);
    }

    hasEvents(_day) {
        throw new GObject.NotImplementedError(`hasEvents in ${this.constructor.name}`);
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
    return new Gio.DBusProxy({
        g_connection: Gio.DBus.session,
        g_interface_name: CalendarServerInfo.name,
        g_interface_info: CalendarServerInfo,
        g_name: 'org.gnome.Shell.CalendarServer',
        g_object_path: '/org/gnome/Shell/CalendarServer',
    });
}

function _datesEqual(a, b) {
    if (a < b)
        return false;
    else if (a > b)
        return false;
    return true;
}

/**
 * Checks whether an event overlaps a given interval
 *
 * @param {Date} e0 Beginning of the event
 * @param {Date} e1 End of the event
 * @param {Date} i0 Beginning of the interval
 * @param {Date} i1 End of the interval
 * @returns {boolean} Whether there was an overlap
 */
function _eventOverlapsInterval(e0, e1, i0, i1) {
    // This also ensures zero-length events are included
    if (e0 >= i0 && e1 < i1)
        return true;

    if (e1 <= i0)
        return false;
    if (i1 <= e0)
        return false;

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
        this._initProxy();
    }

    async _initProxy() {
        let loaded = false;

        try {
            await this._dbusProxy.init_async(GLib.PRIORITY_DEFAULT, null);
            loaded = true;
        } catch (e) {
            // Ignore timeouts and install signals as normal, because with high
            // probability the service will appear later on, and we will get a
            // NameOwnerChanged which will finish loading
            //
            // (But still _initialized to false, because the proxy does not know
            // about the HasCalendars property and would cause an exception trying
            // to read it)
            if (!e.matches(Gio.DBusError, Gio.DBusError.TIMED_OUT)) {
                log(`Error loading calendars: ${e.message}`);
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

    _removeMatching(uidPrefix) {
        let changed = false;
        for (const id of this._events.keys()) {
            if (id.startsWith(uidPrefix))
                changed = this._events.delete(id) || changed;
        }
        return changed;
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
        const handledRemovals = new Set();

        for (let n = 0; n < appointments.length; n++) {
            const [id, summary, startTime, endTime] = appointments[n];
            const date = new Date(startTime * 1000);
            const end = new Date(endTime * 1000);
            let event = new CalendarEvent(id, date, end, summary);
            /* It's a recurring event */
            if (!id.endsWith('\n')) {
                const parentId = id.substr(0, id.lastIndexOf('\n') + 1);
                if (!handledRemovals.has(parentId)) {
                    handledRemovals.add(parentId);
                    this._removeMatching(parentId);
                }
            }
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
            changed = this._removeMatching(id) || changed;

        if (changed)
            this.emit('changed');
    }

    _onClientDisappeared(dbusProxy, nameOwner, argArray) {
        let [sourceUid = ''] = argArray;
        sourceUid += '\n';

        if (this._removeMatching(sourceUid))
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
            this._dbusProxy.SetTimeRangeAsync(
                this._curRequestBegin.getTime() / 1000,
                this._curRequestEnd.getTime() / 1000,
                forceReload,
                Gio.DBusCallFlags.NONE).catch(logError);
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
            if (_eventOverlapsInterval(event.date, event.end, begin, end))
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

        this._settings.connect(`changed::${SHOW_WEEKDATE_KEY}`, this._onSettingsChange.bind(this));
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
        this._topBox = new St.BoxLayout({ style_class: 'calendar-month-header' });
        layout.attach(this._topBox, 0, 0, offsetCols + 7, 1);

        this._backButton = new St.Button({
            style_class: 'calendar-change-month-back pager-button',
            icon_name: 'pan-start-symbolic',
            accessible_name: _('Previous month'),
            can_focus: true,
        });
        this._topBox.add(this._backButton);
        this._backButton.connect('clicked', this._onPrevMonthButtonClicked.bind(this));

        this._monthLabel = new St.Label({
            style_class: 'calendar-month-label',
            can_focus: true,
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._topBox.add_child(this._monthLabel);

        this._forwardButton = new St.Button({
            style_class: 'calendar-change-month-forward pager-button',
            icon_name: 'pan-end-symbolic',
            accessible_name: _('Next month'),
            can_focus: true,
        });
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
            let label = new St.Label({
                style_class: 'calendar-day-base calendar-day-heading',
                text: customDayAbbrev,
                can_focus: true,
            });
            label.accessible_name = iter.toLocaleFormat('%A');
            let col;
            if (this.get_text_direction() == Clutter.TextDirection.RTL)
                col = 6 - (7 + iter.getDay() - this._weekStart) % 7;
            else
                col = offsetCols + (7 + iter.getDay() - this._weekStart) % 7;
            layout.attach(label, col, 1, 1, 1);
            iter.setDate(iter.getDate() + 1);
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
        let beginDate = new Date(
            this._selectedDate.getFullYear(), this._selectedDate.getMonth(), 1);

        this._calendarBegin = new Date(beginDate);
        this._markedAsToday = now;

        let daysToWeekStart = (7 + beginDate.getDay() - this._weekStart) % 7;
        let startsOnWeekStart = daysToWeekStart == 0;
        let weekPadding = startsOnWeekStart ? 7 : 0;

        beginDate.setDate(beginDate.getDate() - (weekPadding + daysToWeekStart));

        let layout = this.layout_manager;
        let iter = new Date(beginDate);
        let row = 2;
        // nRows here means 6 weeks + one header + one navbar
        let nRows = 8;
        while (row < nRows) {
            let button = new St.Button({
                // xgettext:no-javascript-format
                label: iter.toLocaleFormat(C_('date day number format', '%d')),
                can_focus: true,
            });
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
                styleClass = `calendar-day-top ${styleClass}`;

            let leftMost = rtl
                ? iter.getDay() == (this._weekStart + 6) % 7
                : iter.getDay() == this._weekStart;
            if (leftMost)
                styleClass = `calendar-day-left ${styleClass}`;

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
                const label = new St.Label({
                    text: iter.toLocaleFormat('%V'),
                    style_class: 'calendar-week-number',
                    can_focus: true,
                });
                let weekFormat = Shell.util_translate_time_string(N_("Week %V"));
                label.clutter_text.y_align = Clutter.ActorAlign.CENTER;
                label.accessible_name = iter.toLocaleFormat(weekFormat);
                layout.attach(label, rtl ? 7 : 0, row, 1, 1);
            }

            iter.setDate(iter.getDate() + 1);

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
        notification.connectObject(
            'updated', this._onUpdated.bind(this),
            'destroy', () => {
                this.notification = null;
                if (!this._closed)
                    this.close();
            }, this);
    }

    _getIcon() {
        if (this.notification.gicon) {
            return new St.Icon({
                gicon: this.notification.gicon,
                icon_size: MESSAGE_ICON_SIZE,
            });
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

    canClose() {
        return true;
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
        source.connectObject('notification-added',
            this._onNotificationAdded.bind(this), this);
    }

    _onNotificationAdded(source, notification) {
        let message = new NotificationMessage(notification);
        message.setSecondaryActor(new TimeLabel(notification.datetime));

        let isUrgent = notification.urgency == MessageTray.Urgency.CRITICAL;

        notification.connectObject(
            'destroy', () => {
                if (isUrgent)
                    this._nUrgent--;
            },
            'updated', () => {
                message.setSecondaryActor(new TimeLabel(notification.datetime));
                this.moveMessage(message, isUrgent ? 0 : this._nUrgent, this.mapped);
            }, this);

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

    vfunc_map() {
        this._messages.forEach(message => {
            if (message.notification.urgency != MessageTray.Urgency.CRITICAL)
                message.notification.acknowledged = true;
        });
        super.vfunc_map();
    }
});

var Placeholder = GObject.registerClass(
class Placeholder extends St.BoxLayout {
    _init() {
        super._init({ style_class: 'message-list-placeholder', vertical: true });
        this._date = new Date();

        this._icon = new St.Icon({ icon_name: 'no-notifications-symbolic' });
        this.add_actor(this._icon);

        this._label = new St.Label({ text: _('No Notifications') });
        this.add_actor(this._label);
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
            Gio.Settings.unbind(this, 'state');
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

        let box = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            y_expand: true,
        });
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
            style_class: 'dnd-button',
            can_focus: true,
            toggle_mode: true,
            child: this._dndSwitch,
            label_actor: dndLabel,
            y_align: Clutter.ActorAlign.CENTER,
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

        this._sectionList = new St.BoxLayout({
            style_class: 'message-list-sections',
            vertical: true,
            x_expand: true,
            y_expand: true,
            y_align: Clutter.ActorAlign.START,
        });
        this._sectionList.connectObject(
            'actor-added', this._sync.bind(this),
            'actor-removed', this._sync.bind(this),
            this);
        this._scrollView.add_actor(this._sectionList);

        this._mediaSection = new Mpris.MediaSection();
        this._addSection(this._mediaSection);

        this._notificationSection = new NotificationSection();
        this._addSection(this._notificationSection);

        Main.sessionMode.connect('updated', this._sync.bind(this));
    }

    _addSection(section) {
        section.connectObject(
            'notify::visible', this._sync.bind(this),
            'notify::empty', this._sync.bind(this),
            'notify::can-clear', this._sync.bind(this),
            'destroy', () => this._sectionList.remove_actor(section),
            'message-focused', (_s, messageActor) => {
                Util.ensureActorVisibleInScrollView(this._scrollView, messageActor);
            }, this);
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
});
