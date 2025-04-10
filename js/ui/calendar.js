import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as MessageList from './messageList.js';
import {ensureActorVisibleInScrollView} from '../misc/animationUtils.js';

import {formatDateWithCFormatString} from '../misc/dateUtils.js';
import {loadInterfaceXML} from '../misc/fileUtils.js';

const SHOW_WEEKDATE_KEY = 'show-weekdate';

const NC_ = (context, str) => `${context}\u0004${str}`;

function sameYear(dateA, dateB) {
    return dateA.getYear() === dateB.getYear();
}

function sameMonth(dateA, dateB) {
    return sameYear(dateA, dateB) && (dateA.getMonth() === dateB.getMonth());
}

function sameDay(dateA, dateB) {
    return sameMonth(dateA, dateB) && (dateA.getDate() === dateB.getDate());
}

function _isWorkDay(date) {
    /* Translators: Enter 0-6 (Sunday-Saturday) for non-work days. Examples: "0" (Sunday) "6" (Saturday) "06" (Sunday and Saturday). */
    let days = C_('calendar-no-work', '06');
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
        NC_('grid sunday', 'S'),
        /* Translators: Calendar grid abbreviation for Monday */
        NC_('grid monday', 'M'),
        /* Translators: Calendar grid abbreviation for Tuesday */
        NC_('grid tuesday', 'T'),
        /* Translators: Calendar grid abbreviation for Wednesday */
        NC_('grid wednesday', 'W'),
        /* Translators: Calendar grid abbreviation for Thursday */
        NC_('grid thursday', 'T'),
        /* Translators: Calendar grid abbreviation for Friday */
        NC_('grid friday', 'F'),
        /* Translators: Calendar grid abbreviation for Saturday */
        NC_('grid saturday', 'S'),
    ];
    return Shell.util_translate_time_string(abbreviations[dayNumber]);
}

// Abstraction for an appointment/event in a calendar

class CalendarEvent {
    constructor(id, date, end, summary) {
        this.id = id;
        this.date = date;
        this.end = end;
        this.summary = summary;
    }
}

// Interface for appointments/events - e.g. the contents of a calendar
//

export const EventSourceBase = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
    Properties: {
        'has-calendars': GObject.ParamSpec.boolean(
            'has-calendars', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'is-loading': GObject.ParamSpec.boolean(
            'is-loading', null, null,
            GObject.ParamFlags.READABLE,
            false),
    },
    Signals: {'changed': {}},
}, class EventSourceBase extends GObject.Object {
    /**
     * @returns {boolean}
     */
    get isLoading() {
        throw new GObject.NotImplementedError(`isLoading in ${this.constructor.name}`);
    }

    /**
     * @returns {boolean}
     */
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

    /**
     * @param {Date} _day
     * @returns {boolean}
     */
    hasEvents(_day) {
        throw new GObject.NotImplementedError(`hasEvents in ${this.constructor.name}`);
    }
});

export const EmptyEventSource = GObject.registerClass(
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
export const DBusEventSource = GObject.registerClass(
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
                const parentId = id.substring(0, id.lastIndexOf('\n') + 1);
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

        const {done} = this._getFilteredEvents(dayBegin, dayEnd).next();
        return !done;
    }
});

export const Calendar = GObject.registerClass({
    Signals: {'selected-date-changed': {param_types: [GLib.DateTime.$gtype]}},
}, class Calendar extends St.Widget {
    _init() {
        this._weekStart = Shell.util_get_week_start();
        this._settings = new Gio.Settings({schema_id: 'org.gnome.desktop.calendar'});

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

        let datetime = GLib.DateTime.new_from_unix_local(
            this._selectedDate.getTime() / 1000);
        this.emit('selected-date-changed', datetime);

        this._update();
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
        this._topBox = new St.BoxLayout({style_class: 'calendar-month-header'});
        layout.attach(this._topBox, 0, 0, offsetCols + 7, 1);

        this._backButton = new St.Button({
            style_class: 'calendar-change-month-back pager-button',
            icon_name: 'pan-start-symbolic',
            accessible_name: _('Previous month'),
            can_focus: true,
        });
        this._topBox.add_child(this._backButton);
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
        this._topBox.add_child(this._forwardButton);
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
            // Could use formatDateWithCFormatString(iter, '%a') but that normally gives three characters
            // and we want, ideally, a single character for e.g. S M T W T F S
            let customDayAbbrev = _getCalendarDayAbbreviation(iter.getDay());
            let label = new St.Label({
                style_class: 'calendar-day-heading',
                text: customDayAbbrev,
                can_focus: true,
            });
            label.accessible_name = formatDateWithCFormatString(iter, '%A');
            let col;
            if (this.get_text_direction() === Clutter.TextDirection.RTL)
                col = 6 - (7 + iter.getDay() - this._weekStart) % 7;
            else
                col = offsetCols + (7 + iter.getDay() - this._weekStart) % 7;
            layout.attach(label, col, 1, 1, 1);
            iter.setDate(iter.getDate() + 1);
        }

        // All the children after this are days, and get removed when we update the calendar
        this._firstDayIndex = this.get_n_children();
    }

    vfunc_scroll_event(event) {
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
    }

    _onPrevMonthButtonClicked() {
        let newDate = new Date(this._selectedDate);
        let oldMonth = newDate.getMonth();
        if (oldMonth === 0) {
            newDate.setMonth(11);
            newDate.setFullYear(newDate.getFullYear() - 1);
            if (newDate.getMonth() !== 11) {
                let day = 32 - new Date(newDate.getFullYear() - 1, 11, 32).getDate();
                newDate = new Date(newDate.getFullYear() - 1, 11, day);
            }
        } else {
            newDate.setMonth(oldMonth - 1);
            if (newDate.getMonth() !== oldMonth - 1) {
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
        if (oldMonth === 11) {
            newDate.setMonth(0);
            newDate.setFullYear(newDate.getFullYear() + 1);
            if (newDate.getMonth() !== 0) {
                let day = 32 - new Date(newDate.getFullYear() + 1, 0, 32).getDate();
                newDate = new Date(newDate.getFullYear() + 1, 0, day);
            }
        } else {
            newDate.setMonth(oldMonth + 1);
            if (newDate.getMonth() !== oldMonth + 1) {
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
        let startsOnWeekStart = daysToWeekStart === 0;
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
                label: formatDateWithCFormatString(iter, C_('date day number format', '%d')),
                can_focus: true,
            });
            let rtl = button.get_text_direction() === Clutter.TextDirection.RTL;

            if (this._eventSource instanceof EmptyEventSource)
                button.reactive = false;

            button._date = new Date(iter);
            button.connect('clicked', () => {
                this._shouldDateGrabFocus = true;
                this.setDate(button._date);
                this._shouldDateGrabFocus = false;
            });

            let hasEvents = this._eventSource.hasEvents(iter);
            let styleClass = 'calendar-day';

            if (_isWorkDay(iter))
                styleClass += ' calendar-weekday';
            else
                styleClass += ' calendar-weekend';

            // Hack used in lieu of border-collapse - see gnome-shell.css
            if (row === 2)
                styleClass = `calendar-day-top ${styleClass}`;

            let leftMost = rtl
                ? iter.getDay() === (this._weekStart + 6) % 7
                : iter.getDay() === this._weekStart;
            if (leftMost)
                styleClass = `calendar-day-left ${styleClass}`;

            if (sameDay(now, iter))
                styleClass += ' calendar-today';
            else if (iter.getMonth() !== this._selectedDate.getMonth())
                styleClass += ' calendar-other-month';

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

            if (this._useWeekdate && iter.getDay() === 4) {
                const label = new St.Label({
                    text: formatDateWithCFormatString(iter, '%V'),
                    style_class: 'calendar-week-number',
                    can_focus: true,
                });
                let weekFormat = Shell.util_translate_time_string(N_('Week %V'));
                label.clutter_text.y_align = Clutter.ActorAlign.CENTER;
                label.accessible_name = formatDateWithCFormatString(iter, weekFormat);
                layout.attach(label, rtl ? 7 : 0, row, 1, 1);
            }

            iter.setDate(iter.getDate() + 1);

            if (iter.getDay() === this._weekStart)
                row++;
        }

        // Signal to the event source that we are interested in events
        // only from this date range
        this._eventSource.requestRange(beginDate, iter);
    }

    _update() {
        let now = new Date();

        if (sameYear(this._selectedDate, now))
            this._monthLabel.text = formatDateWithCFormatString(this._selectedDate, this._headerFormatWithoutYear);
        else
            this._monthLabel.text = formatDateWithCFormatString(this._selectedDate, this._headerFormat);

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

const Placeholder = GObject.registerClass(
class Placeholder extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'message-list-placeholder',
            orientation: Clutter.Orientation.VERTICAL,
        });
        this._date = new Date();

        this._icon = new St.Icon({icon_name: 'no-notifications-symbolic'});
        this.add_child(this._icon);

        this._label = new St.Label({text: _('No Notifications')});
        this.add_child(this._label);
    }
});

export const CalendarMessageList = GObject.registerClass(
class CalendarMessageList extends St.Widget {
    constructor() {
        super({
            style_class: 'message-list',
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });

        this._placeholder = new Placeholder();
        this.add_child(this._placeholder);

        let box = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            y_expand: true,
        });
        this.add_child(box);

        this._messageView = new MessageList.MessageView();

        this._scrollView = new St.ScrollView({
            overlay_scrollbars: true,
            x_expand: true, y_expand: true,
            child: this._messageView,
        });
        box.add_child(this._scrollView);

        let hbox = new St.BoxLayout({style_class: 'message-list-controls'});
        box.add_child(hbox);

        this._clearButton = new St.Button({
            style_class: 'message-list-clear-button button',
            label: _('Clear'),
            can_focus: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            accessible_name: C_('action', 'Clear all notifications'),
        });
        this._clearButton.connect('clicked', () => {
            this._messageView.clear();
        });
        hbox.add_child(this._clearButton);

        this._placeholder.bind_property('visible',
            this._clearButton, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN);

        this._messageView.connectObject(
            'message-focused', (_s, messageActor) => {
                ensureActorVisibleInScrollView(this._scrollView, messageActor);
            }, this);

        this._messageView.bind_property('empty',
            this._placeholder, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._messageView.bind_property('can-clear',
            this._clearButton, 'reactive',
            GObject.BindingFlags.SYNC_CREATE);
    }

    maybeCollapseMessageGroupForEvent(event) {
        if (!this._messageView.expandedGroup)
            return Clutter.EVENT_PROPAGATE;

        if (event.type() === Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() === Clutter.KEY_Escape) {
            this._messageView.collapse();
            return Clutter.EVENT_STOP;
        }

        const targetActor = global.stage.get_event_actor(event);
        const onScrollbar =
            this._scrollView.contains(targetActor) &&
            !this._messageView.contains(targetActor);

        if ((event.type() === Clutter.EventType.BUTTON_PRESS ||
            event.type() === Clutter.EventType.TOUCH_BEGIN) &&
            !this._messageView.expandedGroup.contains(targetActor) &&
            !onScrollbar)
            this._messageView.collapse();

        return Clutter.EVENT_PROPAGATE;
    }
});
