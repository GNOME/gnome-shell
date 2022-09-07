// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported DateMenuButton */

const {
    Clutter, Gio, GLib, GnomeDesktop,
    GObject, GWeather, Pango, Shell, St,
} = imports.gi;

const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const Calendar = imports.ui.calendar;
const Weather = imports.misc.weather;
const System = imports.system;

const { loadInterfaceXML } = imports.misc.fileUtils;

const NC_ = (context, str) => `${context}\u0004${str}`;
const T_ = Shell.util_translate_time_string;

const MAX_FORECASTS = 5;
const EN_CHAR = '\u2013';

const ClocksIntegrationIface = loadInterfaceXML('org.gnome.Shell.ClocksIntegration');
const ClocksProxy = Gio.DBusProxy.makeProxyWrapper(ClocksIntegrationIface);

function _isToday(date) {
    let now = new Date();
    return now.getYear() == date.getYear() &&
           now.getMonth() == date.getMonth() &&
           now.getDate() == date.getDate();
}

function _gDateTimeToDate(datetime) {
    return new Date(datetime.to_unix() * 1000 + datetime.get_microsecond() / 1000);
}

var TodayButton = GObject.registerClass(
class TodayButton extends St.Button {
    _init(calendar) {
        // Having the ability to go to the current date if the user is already
        // on the current date can be confusing. So don't make the button reactive
        // until the selected date changes.
        super._init({
            style_class: 'datemenu-today-button',
            x_expand: true,
            can_focus: true,
            reactive: false,
        });

        let hbox = new St.BoxLayout({ vertical: true });
        this.add_actor(hbox);

        this._dayLabel = new St.Label({
            style_class: 'day-label',
            x_align: Clutter.ActorAlign.START,
        });
        hbox.add_actor(this._dayLabel);

        this._dateLabel = new St.Label({ style_class: 'date-label' });
        hbox.add_actor(this._dateLabel);

        this._calendar = calendar;
        this._calendar.connect('selected-date-changed', (_calendar, datetime) => {
            // Make the button reactive only if the selected date is not the
            // current date.
            this.reactive = !_isToday(_gDateTimeToDate(datetime));
        });
    }

    vfunc_clicked() {
        this._calendar.setDate(new Date(), false);
    }

    setDate(date) {
        this._dayLabel.set_text(date.toLocaleFormat('%A'));

        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the top bar (e.g.,
         * "Tue 9:29 AM").  The string itself should become a full date, e.g.,
         * "February 17 2015".
         */
        let dateFormat = Shell.util_translate_time_string(N_("%B %-d %Y"));
        this._dateLabel.set_text(date.toLocaleFormat(dateFormat));

        /* Translators: This is the accessible name of the date button shown
         * below the time in the shell; it should combine the weekday and the
         * date, e.g. "Tuesday February 17 2015".
         */
        dateFormat = Shell.util_translate_time_string(N_("%A %B %e %Y"));
        this.accessible_name = date.toLocaleFormat(dateFormat);
    }
});

var EventsSection = GObject.registerClass(
class EventsSection extends St.Button {
    _init() {
        super._init({
            style_class: 'events-button',
            can_focus: true,
            x_expand: true,
            child: new St.BoxLayout({
                style_class: 'events-box',
                vertical: true,
                x_expand: true,
            }),
        });

        this._startDate = null;
        this._endDate = null;

        this._eventSource = null;
        this._calendarApp = null;

        this._title = new St.Label({
            style_class: 'events-title',
        });
        this.child.add_child(this._title);

        this._eventsList = new St.BoxLayout({
            style_class: 'events-list',
            vertical: true,
            x_expand: true,
        });
        this.child.add_child(this._eventsList);

        this._appSys = Shell.AppSystem.get_default();
        this._appSys.connect('installed-changed',
            this._appInstalledChanged.bind(this));
        this._appInstalledChanged();
    }

    setDate(date) {
        this._startDate =
            new Date(date.getFullYear(), date.getMonth(), date.getDate());
        this._endDate =
            new Date(date.getFullYear(), date.getMonth(), date.getDate() + 1);

        this._updateTitle();
        this._reloadEvents();
    }

    setEventSource(eventSource) {
        if (!(eventSource instanceof Calendar.EventSourceBase))
            throw new Error('Event source is not valid type');

        this._eventSource = eventSource;
        this._eventSource.connect('changed', this._reloadEvents.bind(this));
        this._eventSource.connect('notify::has-calendars',
            this._sync.bind(this));
        this._sync();
    }

    _updateTitle() {
        /* Translators: Shown on calendar heading when selected day occurs on current year */
        const sameYearFormat = T_(NC_('calendar heading', '%B %-d'));

        /* Translators: Shown on calendar heading when selected day occurs on different year */
        const otherYearFormat = T_(NC_('calendar heading', '%B %-d %Y'));

        const timeSpanDay = GLib.TIME_SPAN_DAY / 1000;
        const now = new Date();

        if (this._startDate <= now && now < this._endDate)
            this._title.text = _('Today');
        else if (this._endDate <= now && now - this._endDate < timeSpanDay)
            this._title.text = _('Yesterday');
        else if (this._startDate > now && this._startDate - now <= timeSpanDay)
            this._title.text = _('Tomorrow');
        else if (this._startDate.getFullYear() === now.getFullYear())
            this._title.text = this._startDate.toLocaleFormat(sameYearFormat);
        else
            this._title.text = this._startDate.toLocaleFormat(otherYearFormat);
    }

    _isAtMidnight(eventTime) {
        return eventTime.getHours() === 0 && eventTime.getMinutes() === 0 && eventTime.getSeconds() === 0;
    }

    _formatEventTime(event) {
        const eventStart = event.date;
        let eventEnd = event.end;

        const allDay =
            eventStart.getTime() === this._startDate.getTime() && eventEnd.getTime() === this._endDate.getTime();

        const startsBeforeToday = eventStart < this._startDate;
        const endsAfterToday = eventEnd > this._endDate;

        const startTimeOnly = Util.formatTime(eventStart, { timeOnly: true });
        const endTimeOnly = Util.formatTime(eventEnd, { timeOnly: true });

        const rtl = Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;

        let title;
        if (allDay) {
            /* Translators: Shown in calendar event list for all day events
             * Keep it short, best if you can use less then 10 characters
             */
            title = C_('event list time', 'All Day');
        } else if (startsBeforeToday || endsAfterToday) {
            const now = new Date();
            const thisYear = now.getFullYear();

            const startsAtMidnight = this._isAtMidnight(eventStart);
            const endsAtMidnight = this._isAtMidnight(eventEnd);

            const startYear = eventStart.getFullYear();

            if (endsAtMidnight) {
                eventEnd = new Date(eventEnd);
                eventEnd.setDate(eventEnd.getDate() - 1);
            }

            const endYear = eventEnd.getFullYear();

            let format;
            if (startYear === thisYear && thisYear === endYear)
                /* Translators: Shown in calendar event list as the start/end of events
                 * that only show day and month
                 */
                format = T_(N_('%m/%d'));
            else
                format = '%x';

            const startDateOnly = eventStart.toLocaleFormat(format);
            const endDateOnly = eventEnd.toLocaleFormat(format);

            if (startsAtMidnight && endsAtMidnight)
                title = `${rtl ? endDateOnly : startDateOnly} ${EN_CHAR} ${rtl ? startDateOnly : endDateOnly}`;
            else if (rtl)
                title = `${endTimeOnly} ${endDateOnly} ${EN_CHAR} ${startTimeOnly} ${startDateOnly}`;
            else
                title = `${startDateOnly} ${startTimeOnly} ${EN_CHAR} ${endDateOnly} ${endTimeOnly}`;
        } else if (eventStart === eventEnd) {
            title = startTimeOnly;
        } else {
            title = `${rtl ? endTimeOnly : startTimeOnly} ${EN_CHAR} ${rtl ? startTimeOnly : endTimeOnly}`;
        }

        return title;
    }

    _reloadEvents() {
        if (this._eventSource.isLoading || this._reloading)
            return;

        this._reloading = true;

        [...this._eventsList].forEach(c => c.destroy());

        const events =
            this._eventSource.getEvents(this._startDate, this._endDate);

        for (let event of events) {
            const box = new St.BoxLayout({
                style_class: 'event-box',
                vertical: true,
            });
            box.add(new St.Label({
                text: event.summary,
                style_class: 'event-summary',
            }));
            box.add(new St.Label({
                text: this._formatEventTime(event),
                style_class: 'event-time',
            }));
            this._eventsList.add_child(box);
        }

        if (this._eventsList.get_n_children() === 0) {
            const placeholder = new St.Label({
                text: _('No Events'),
                style_class: 'event-placeholder',
            });
            this._eventsList.add_child(placeholder);
        }

        this._reloading = false;
        this._sync();
    }

    vfunc_clicked() {
        Main.overview.hide();
        Main.panel.closeCalendar();

        const appInfo = this._calendarApp;
        const context = global.create_app_launch_context(0, -1);
        if (appInfo.get_id() === 'org.gnome.Evolution.desktop')
            appInfo.launch_action('calendar', context);
        else
            appInfo.launch([], context);
    }

    _appInstalledChanged() {
        const apps = Gio.AppInfo.get_recommended_for_type('text/calendar');
        if (apps && (apps.length > 0)) {
            const app = Gio.AppInfo.get_default_for_type('text/calendar', false);
            const defaultInRecommended = apps.some(a => a.equal(app));
            this._calendarApp = defaultInRecommended ? app : apps[0];
        } else {
            this._calendarApp = null;
        }

        return this._sync();
    }

    _sync() {
        this.visible = this._eventSource && this._eventSource.hasCalendars;
        this.reactive = this._calendarApp !== null;
    }
});

var WorldClocksSection = GObject.registerClass(
class WorldClocksSection extends St.Button {
    _init() {
        super._init({
            style_class: 'world-clocks-button',
            can_focus: true,
            x_expand: true,
        });
        this._clock = new GnomeDesktop.WallClock();
        this._clockNotifyId = 0;
        this._tzNotifyId = 0;

        this._locations = [];

        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        this._grid = new St.Widget({
            style_class: 'world-clocks-grid',
            x_expand: true,
            layout_manager: layout,
        });
        layout.hookup_style(this._grid);

        this.child = this._grid;

        this._clocksApp = null;
        this._clocksProxy = new ClocksProxy(
            Gio.DBus.session,
            'org.gnome.clocks',
            '/org/gnome/clocks',
            this._onProxyReady.bind(this),
            null /* cancellable */,
            Gio.DBusProxyFlags.DO_NOT_AUTO_START | Gio.DBusProxyFlags.GET_INVALIDATED_PROPERTIES);

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.shell.world-clocks',
        });
        this._settings.connect('changed', this._clocksChanged.bind(this));
        this._clocksChanged();

        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('installed-changed',
            this._sync.bind(this));
        this._sync();
    }

    vfunc_clicked() {
        if (this._clocksApp)
            this._clocksApp.activate();

        Main.overview.hide();
        Main.panel.closeCalendar();
    }

    _sync() {
        this._clocksApp = this._appSystem.lookup_app('org.gnome.clocks.desktop');
        this.visible = this._clocksApp != null;
    }

    _clocksChanged() {
        this._grid.destroy_all_children();
        this._locations = [];

        let world = GWeather.Location.get_world();
        let clocks = this._settings.get_value('locations').deepUnpack();
        for (let i = 0; i < clocks.length; i++) {
            let l = world.deserialize(clocks[i]);
            if (l && l.get_timezone() != null)
                this._locations.push({ location: l });
        }

        const unixtime = GLib.DateTime.new_now_local().to_unix();
        this._locations.sort((a, b) => {
            const tzA = a.location.get_timezone();
            const tzB = b.location.get_timezone();
            const intA = tzA.find_interval(GLib.TimeType.STANDARD, unixtime);
            const intB = tzB.find_interval(GLib.TimeType.STANDARD, unixtime);
            return tzA.get_offset(intA) - tzB.get_offset(intB);
        });

        let layout = this._grid.layout_manager;
        let title = this._locations.length == 0
            ? _("Add world clocks…")
            : _("World Clocks");
        const header = new St.Label({
            style_class: 'world-clocks-header',
            x_align: Clutter.ActorAlign.START,
            text: title,
        });
        if (this._grid.text_direction === Clutter.TextDirection.RTL)
            layout.attach(header, 2, 0, 1, 1);
        else
            layout.attach(header, 0, 0, 2, 1);
        this.label_actor = header;

        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i].location;

            let name = l.get_city_name() || l.get_name();
            const label = new St.Label({
                style_class: 'world-clocks-city',
                text: name,
                x_align: Clutter.ActorAlign.START,
                y_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });

            let time = new St.Label({ style_class: 'world-clocks-time' });

            const tz = new St.Label({
                style_class: 'world-clocks-timezone',
                x_align: Clutter.ActorAlign.END,
                y_align: Clutter.ActorAlign.CENTER,
            });

            time.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            tz.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

            if (this._grid.text_direction == Clutter.TextDirection.RTL) {
                layout.attach(tz, 0, i + 1, 1, 1);
                layout.attach(time, 1, i + 1, 1, 1);
                layout.attach(label, 2, i + 1, 1, 1);
            } else {
                layout.attach(label, 0, i + 1, 1, 1);
                layout.attach(time, 1, i + 1, 1, 1);
                layout.attach(tz, 2, i + 1, 1, 1);
            }

            this._locations[i].timeLabel = time;
            this._locations[i].tzLabel = tz;
        }

        if (this._grid.get_n_children() > 1) {
            if (!this._clockNotifyId) {
                this._clockNotifyId =
                    this._clock.connect('notify::clock', this._updateTimeLabels.bind(this));
            }
            if (!this._tzNotifyId) {
                this._tzNotifyId =
                    this._clock.connect('notify::timezone', this._updateTimezoneLabels.bind(this));
            }
            this._updateTimeLabels();
            this._updateTimezoneLabels();
        } else {
            if (this._clockNotifyId)
                this._clock.disconnect(this._clockNotifyId);
            this._clockNotifyId = 0;

            if (this._tzNotifyId)
                this._clock.disconnect(this._tzNotifyId);
            this._tzNotifyId = 0;
        }
    }

    _getTimezoneOffsetAtLocation(location) {
        const tz = location.get_timezone();
        const localOffset = GLib.DateTime.new_now_local().get_utc_offset();
        const utcOffset = GLib.DateTime.new_now(tz).get_utc_offset();
        const offsetCurrentTz = utcOffset - localOffset;
        const offsetHours =
            Math.floor(Math.abs(offsetCurrentTz) / GLib.TIME_SPAN_HOUR);
        const offsetMinutes =
            (Math.abs(offsetCurrentTz) % GLib.TIME_SPAN_HOUR) /
            GLib.TIME_SPAN_MINUTE;

        const prefix = offsetCurrentTz >= 0 ? '+' : '-';
        const text = offsetMinutes === 0
            ? `${prefix}${offsetHours}`
            : `${prefix}${offsetHours}\u2236${offsetMinutes}`;
        return text;
    }

    _updateTimeLabels() {
        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i];
            const now = GLib.DateTime.new_now(l.location.get_timezone());
            l.timeLabel.text = Util.formatTime(now, { timeOnly: true });
        }
    }

    _updateTimezoneLabels() {
        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i];
            l.tzLabel.text = this._getTimezoneOffsetAtLocation(l.location);
        }
    }

    _onProxyReady(proxy, error) {
        if (error) {
            log(`Failed to create GNOME Clocks proxy: ${error}`);
            return;
        }

        this._clocksProxy.connect('g-properties-changed',
            this._onClocksPropertiesChanged.bind(this));
        this._onClocksPropertiesChanged();
    }

    _onClocksPropertiesChanged() {
        if (this._clocksProxy.g_name_owner == null)
            return;

        this._settings.set_value('locations',
            new GLib.Variant('av', this._clocksProxy.Locations));
    }
});

var WeatherSection = GObject.registerClass(
class WeatherSection extends St.Button {
    _init() {
        super._init({
            style_class: 'weather-button',
            can_focus: true,
            x_expand: true,
        });

        this._weatherClient = new Weather.WeatherClient();

        let box = new St.BoxLayout({
            style_class: 'weather-box',
            vertical: true,
            x_expand: true,
        });

        this.child = box;

        let titleBox = new St.BoxLayout({ style_class: 'weather-header-box' });
        this._titleLabel = new St.Label({
            style_class: 'weather-header',
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
            y_align: Clutter.ActorAlign.END,
        });
        titleBox.add_child(this._titleLabel);
        box.add_child(titleBox);

        this._titleLocation = new St.Label({
            style_class: 'weather-header location',
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.END,
        });
        titleBox.add_child(this._titleLocation);

        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        this._forecastGrid = new St.Widget({
            style_class: 'weather-grid',
            layout_manager: layout,
        });
        layout.hookup_style(this._forecastGrid);
        box.add_child(this._forecastGrid);

        this._weatherClient.connect('changed', this._sync.bind(this));
        this._sync();
    }

    vfunc_map() {
        this._weatherClient.update();
        super.vfunc_map();
    }

    vfunc_clicked() {
        this._weatherClient.activateApp();

        Main.overview.hide();
        Main.panel.closeCalendar();
    }

    _getInfos() {
        let forecasts = this._weatherClient.info.get_forecast_list();

        let now = GLib.DateTime.new_now_local();
        let current = GLib.DateTime.new_from_unix_local(0);
        let infos = [];
        for (let i = 0; i < forecasts.length; i++) {
            const [valid, timestamp] = forecasts[i].get_value_update();
            if (!valid || timestamp === 0)
                continue;  // 0 means 'never updated'

            const datetime = GLib.DateTime.new_from_unix_local(timestamp);
            if (now.difference(datetime) > 0)
                continue; // Ignore earlier forecasts

            if (datetime.difference(current) < GLib.TIME_SPAN_HOUR)
                continue; // Enforce a minimum interval of 1h

            if (infos.push(forecasts[i]) == MAX_FORECASTS)
                break; // Use a maximum of five forecasts

            current = datetime;
        }
        return infos;
    }

    _addForecasts() {
        let layout = this._forecastGrid.layout_manager;

        let infos = this._getInfos();
        if (this._forecastGrid.text_direction == Clutter.TextDirection.RTL)
            infos.reverse();

        let col = 0;
        infos.forEach(fc => {
            const [valid_, timestamp] = fc.get_value_update();
            let timeStr = Util.formatTime(new Date(timestamp * 1000), {
                timeOnly: true,
                ampm: false,
            });
            const [, tempValue] = fc.get_value_temp(GWeather.TemperatureUnit.DEFAULT);
            const tempPrefix = Math.round(tempValue) >= 0 ? ' ' : '';

            let time = new St.Label({
                style_class: 'weather-forecast-time',
                text: timeStr,
                x_align: Clutter.ActorAlign.CENTER,
            });
            let icon = new St.Icon({
                style_class: 'weather-forecast-icon',
                icon_name: fc.get_symbolic_icon_name(),
                x_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            let temp = new St.Label({
                style_class: 'weather-forecast-temp',
                text: `${tempPrefix}${Math.round(tempValue)}°`,
                x_align: Clutter.ActorAlign.CENTER,
            });

            temp.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            time.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

            layout.attach(time, col, 0, 1, 1);
            layout.attach(icon, col, 1, 1, 1);
            layout.attach(temp, col, 2, 1, 1);
            col++;
        });
    }

    _setStatusLabel(text) {
        let layout = this._forecastGrid.layout_manager;
        let label = new St.Label({ text });
        layout.attach(label, 0, 0, 1, 1);
    }

    _findBestLocationName(loc) {
        const locName = loc.get_name();

        if (loc.get_level() === GWeather.LocationLevel.CITY ||
            !loc.has_coords())
            return locName;

        const world = GWeather.Location.get_world();
        const city = world.find_nearest_city(...loc.get_coords());
        const cityName = city.get_name();

        return locName.includes(cityName) ? cityName : locName;
    }

    _updateForecasts() {
        this._forecastGrid.destroy_all_children();

        if (!this._weatherClient.hasLocation)
            return;

        const { info } = this._weatherClient;
        this._titleLocation.text = this._findBestLocationName(info.location);

        if (this._weatherClient.loading) {
            this._setStatusLabel(_("Loading…"));
            return;
        }

        if (info.is_valid()) {
            this._addForecasts();
            return;
        }

        if (info.network_error())
            this._setStatusLabel(_("Go online for weather information"));
        else
            this._setStatusLabel(_("Weather information is currently unavailable"));
    }

    _sync() {
        this.visible = this._weatherClient.available;

        if (!this.visible)
            return;

        if (this._weatherClient.hasLocation)
            this._titleLabel.text = _('Weather');
        else
            this._titleLabel.text = _('Select weather location…');

        this._forecastGrid.visible = this._weatherClient.hasLocation;
        this._titleLocation.visible = this._weatherClient.hasLocation;

        this._updateForecasts();
    }
});

var MessagesIndicator = GObject.registerClass(
class MessagesIndicator extends St.Icon {
    _init() {
        super._init({
            icon_size: 16,
            visible: false,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._sources = [];
        this._count = 0;

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.notifications',
        });
        this._settings.connect('changed::show-banners', this._sync.bind(this));

        Main.messageTray.connect('source-added', this._onSourceAdded.bind(this));
        Main.messageTray.connect('source-removed', this._onSourceRemoved.bind(this));
        Main.messageTray.connect('queue-changed', this._updateCount.bind(this));

        let sources = Main.messageTray.getSources();
        sources.forEach(source => this._onSourceAdded(null, source));

        this._sync();

        this.connect('destroy', () => {
            this._settings.run_dispose();
            this._settings = null;
        });
    }

    _onSourceAdded(tray, source) {
        source.connect('notify::count', this._updateCount.bind(this));
        this._sources.push(source);
        this._updateCount();
    }

    _onSourceRemoved(tray, source) {
        this._sources.splice(this._sources.indexOf(source), 1);
        this._updateCount();
    }

    _updateCount() {
        let count = 0;
        this._sources.forEach(source => (count += source.unseenCount));
        this._count = count - Main.messageTray.queueCount;

        this._sync();
    }

    _sync() {
        let doNotDisturb = !this._settings.get_boolean('show-banners');
        this.icon_name = doNotDisturb
            ? 'notifications-disabled-symbolic'
            : 'message-indicator-symbolic';
        this.visible = doNotDisturb || this._count > 0;
    }
});

var FreezableBinLayout = GObject.registerClass(
class FreezableBinLayout extends Clutter.BinLayout {
    _init() {
        super._init();

        this._frozen = false;
        this._savedWidth = [NaN, NaN];
        this._savedHeight = [NaN, NaN];
    }

    set frozen(v) {
        if (this._frozen == v)
            return;

        this._frozen = v;
        if (!this._frozen)
            this.layout_changed();
    }

    vfunc_get_preferred_width(container, forHeight) {
        if (!this._frozen || this._savedWidth.some(isNaN))
            return super.vfunc_get_preferred_width(container, forHeight);
        return this._savedWidth;
    }

    vfunc_get_preferred_height(container, forWidth) {
        if (!this._frozen || this._savedHeight.some(isNaN))
            return super.vfunc_get_preferred_height(container, forWidth);
        return this._savedHeight;
    }

    vfunc_allocate(container, allocation) {
        super.vfunc_allocate(container, allocation);

        let [width, height] = allocation.get_size();
        this._savedWidth = [width, width];
        this._savedHeight = [height, height];
    }
});

var CalendarColumnLayout = GObject.registerClass(
class CalendarColumnLayout extends Clutter.BoxLayout {
    _init(actors) {
        super._init({ orientation: Clutter.Orientation.VERTICAL });
        this._colActors = actors;
    }

    vfunc_get_preferred_width(container, forHeight) {
        const actors =
            this._colActors.filter(a => a.get_parent() === container);
        if (actors.length === 0)
            return super.vfunc_get_preferred_width(container, forHeight);
        return actors.reduce(([minAcc, natAcc], child) => {
            const [min, nat] = child.get_preferred_width(forHeight);
            return [Math.max(minAcc, min), Math.max(natAcc, nat)];
        }, [0, 0]);
    }
});

var DateMenuButton = GObject.registerClass(
class DateMenuButton extends PanelMenu.Button {
    _init() {
        let hbox;

        super._init(0.5);

        this._clockDisplay = new St.Label({ style_class: 'clock' });
        this._clockDisplay.clutter_text.y_align = Clutter.ActorAlign.CENTER;
        this._clockDisplay.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        this._indicator = new MessagesIndicator();

        const indicatorPad = new St.Widget();
        this._indicator.bind_property('visible',
            indicatorPad, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        indicatorPad.add_constraint(new Clutter.BindConstraint({
            source: this._indicator,
            coordinate: Clutter.BindCoordinate.SIZE,
        }));

        let box = new St.BoxLayout({ style_class: 'clock-display-box' });
        box.add_actor(indicatorPad);
        box.add_actor(this._clockDisplay);
        box.add_actor(this._indicator);

        this.label_actor = this._clockDisplay;
        this.add_actor(box);
        this.add_style_class_name('clock-display');

        let layout = new FreezableBinLayout();
        let bin = new St.Widget({ layout_manager: layout });
        // For some minimal compatibility with PopupMenuItem
        bin._delegate = this;
        this.menu.box.add_child(bin);

        hbox = new St.BoxLayout({ name: 'calendarArea' });
        bin.add_actor(hbox);

        this._calendar = new Calendar.Calendar();
        this._calendar.connect('selected-date-changed', (_calendar, datetime) => {
            let date = _gDateTimeToDate(datetime);
            layout.frozen = !_isToday(date);
            this._eventsItem.setDate(date);
        });
        this._date = new TodayButton(this._calendar);

        this.menu.connect('open-state-changed', (menu, isOpen) => {
            // Whenever the menu is opened, select today
            if (isOpen) {
                let now = new Date();
                this._calendar.setDate(now);
                this._date.setDate(now);
                this._eventsItem.setDate(now);
            }
        });

        // Fill up the first column
        this._messageList = new Calendar.CalendarMessageList();
        hbox.add_child(this._messageList);

        // Fill up the second column
        const boxLayout = new CalendarColumnLayout([this._calendar, this._date]);
        const vbox = new St.Widget({
            style_class: 'datemenu-calendar-column',
            layout_manager: boxLayout,
        });
        boxLayout.hookup_style(vbox);
        hbox.add(vbox);

        vbox.add_actor(this._date);
        vbox.add_actor(this._calendar);

        this._displaysSection = new St.ScrollView({
            style_class: 'datemenu-displays-section vfade',
            x_expand: true,
            overlay_scrollbars: true,
        });
        this._displaysSection.set_policy(St.PolicyType.NEVER, St.PolicyType.EXTERNAL);
        vbox.add_actor(this._displaysSection);

        const displaysBox = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            style_class: 'datemenu-displays-box',
        });
        this._displaysSection.add_actor(displaysBox);

        this._eventsItem = new EventsSection();
        displaysBox.add_child(this._eventsItem);

        this._clocksItem = new WorldClocksSection();
        displaysBox.add_child(this._clocksItem);

        this._weatherItem = new WeatherSection();
        displaysBox.add_child(this._weatherItem);

        // Done with hbox for calendar and event list

        this._clock = new GnomeDesktop.WallClock();
        this._clock.bind_property('clock', this._clockDisplay, 'text', GObject.BindingFlags.SYNC_CREATE);
        this._clock.connect('notify::timezone', this._updateTimeZone.bind(this));

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _getEventSource() {
        return new Calendar.DBusEventSource();
    }

    _setEventSource(eventSource) {
        if (this._eventSource)
            this._eventSource.destroy();

        this._calendar.setEventSource(eventSource);
        this._eventsItem.setEventSource(eventSource);

        this._eventSource = eventSource;
    }

    _updateTimeZone() {
        // SpiderMonkey caches the time zone so we must explicitly clear it
        // before we can update the calendar, see
        // https://bugzilla.gnome.org/show_bug.cgi?id=678507
        System.clearDateCaches();

        this._calendar.updateTimeZone();
    }

    _sessionUpdated() {
        let eventSource;
        let showEvents = Main.sessionMode.showCalendarEvents;
        if (showEvents)
            eventSource = this._getEventSource();
        else
            eventSource = new Calendar.EmptyEventSource();

        this._setEventSource(eventSource);

        // Displays are not actually expected to launch Settings when activated
        // but the corresponding app (clocks, weather); however we can consider
        // that display-specific settings, so re-use "allowSettings" here ...
        this._displaysSection.visible = Main.sessionMode.allowSettings;
    }
});
