// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, GLib, GnomeDesktop,
        GObject, GWeather, Shell, St } = imports.gi;

const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const Calendar = imports.ui.calendar;
const Weather = imports.misc.weather;
const System = imports.system;

const MAX_FORECASTS = 5;

function _isToday(date) {
    let now = new Date();
    return now.getYear() == date.getYear() &&
           now.getMonth() == date.getMonth() &&
           now.getDate() == date.getDate();
}

var TodayButton = class TodayButton {
    constructor(calendar) {
        // Having the ability to go to the current date if the user is already
        // on the current date can be confusing. So don't make the button reactive
        // until the selected date changes.
        this.actor = new St.Button({ style_class: 'datemenu-today-button',
                                     x_expand: true, x_align: St.Align.START,
                                     can_focus: true,
                                     reactive: false
                                   });
        this.actor.connect('clicked', () => {
            this._calendar.setDate(new Date(), false);
        });

        let hbox = new St.BoxLayout({ vertical: true });
        this.actor.add_actor(hbox);

        this._dayLabel = new St.Label({ style_class: 'day-label',
                                        x_align: Clutter.ActorAlign.START });
        hbox.add_actor(this._dayLabel);

        this._dateLabel = new St.Label({ style_class: 'date-label' });
        hbox.add_actor(this._dateLabel);

        this._calendar = calendar;
        this._calendar.connect('selected-date-changed', (calendar, date) => {
            // Make the button reactive only if the selected date is not the
            // current date.
            this.actor.reactive = !_isToday(date)
        });
    }

    setDate(date) {
        this._dayLabel.set_text(date.toLocaleFormat('%A'));

        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the top bar (e.g.,
         * "Tue 9:29 AM").  The string itself should become a full date, e.g.,
         * "February 17 2015".
         */
        let dateFormat = Shell.util_translate_time_string (N_("%B %-d %Y"));
        this._dateLabel.set_text(date.toLocaleFormat(dateFormat));

        /* Translators: This is the accessible name of the date button shown
         * below the time in the shell; it should combine the weekday and the
         * date, e.g. "Tuesday February 17 2015".
         */
        dateFormat = Shell.util_translate_time_string (N_("%A %B %e %Y"));
        this.actor.accessible_name = date.toLocaleFormat(dateFormat);
    }
};

var WorldClocksSection = class WorldClocksSection {
    constructor() {
        this._clock = new GnomeDesktop.WallClock();
        this._clockNotifyId = 0;

        this._locations = [];

        this.actor = new St.Button({ style_class: 'world-clocks-button',
                                     x_fill: true,
                                     can_focus: true });
        this.actor.connect('clicked', () => {
            this._clockAppMon.activateApp();

            Main.overview.hide();
            Main.panel.closeCalendar();
        });

        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        this._grid = new St.Widget({ style_class: 'world-clocks-grid',
                                     layout_manager: layout });
        layout.hookup_style(this._grid);

        this.actor.child = this._grid;

        this._clockAppMon = new Util.AppSettingsMonitor('org.gnome.clocks.desktop',
                                                        'org.gnome.clocks');
        this._clockAppMon.connect('available-changed',
                                  this._sync.bind(this));
        this._clockAppMon.watchSetting('world-clocks',
                                       this._clocksChanged.bind(this));
        this._sync();
    }

    _sync() {
        this.actor.visible = this._clockAppMon.available;
    }

    _clocksChanged(settings) {
        this._grid.destroy_all_children();
        this._locations = [];

        let world = GWeather.Location.get_world();
        let clocks = settings.get_value('world-clocks').deep_unpack();
        for (let i = 0; i < clocks.length; i++) {
            if (!clocks[i].location)
                continue;
            let l = world.deserialize(clocks[i].location);
            if (l)
                this._locations.push({ location: l });
        }

        this._locations.sort((a, b) => {
            return a.location.get_timezone().get_offset() -
                   b.location.get_timezone().get_offset();
        });

        let layout = this._grid.layout_manager;
        let title = (this._locations.length == 0) ? _("Add world clocks…")
                                                  : _("World Clocks");
        let header = new St.Label({ style_class: 'world-clocks-header',
                                    x_align: Clutter.ActorAlign.START,
                                    text: title });
        layout.attach(header, 0, 0, 2, 1);
        this.actor.label_actor = header;

        let localOffset = GLib.DateTime.new_now_local().get_utc_offset();

        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i].location;

            let name = l.get_city_name() || l.get_name();
            let label = new St.Label({ style_class: 'world-clocks-city',
                                       text: name,
                                       x_align: Clutter.ActorAlign.START,
                                       y_align: Clutter.ActorAlign.CENTER,
                                       x_expand: true });

            let time = new St.Label({ style_class: 'world-clocks-time' });

            let otherOffset = this._getTimeAtLocation(l).get_utc_offset();
            let offset = (otherOffset - localOffset) / GLib.TIME_SPAN_HOUR;
            let fmt = (Math.trunc(offset) == offset) ? '%s%.0f' : '%s%.1f';
            let prefix = (offset >= 0) ? '+' : '-';
            let tz = new St.Label({ style_class: 'world-clocks-timezone',
                                    text: fmt.format(prefix, Math.abs(offset)),
                                    x_align: Clutter.ActorAlign.END,
                                    y_align: Clutter.ActorAlign.CENTER });

            if (this._grid.text_direction == Clutter.TextDirection.RTL) {
                layout.attach(tz, 0, i + 1, 1, 1);
                layout.attach(time, 1, i + 1, 1, 1);
                layout.attach(label, 2, i + 1, 1, 1);
            } else {
                layout.attach(label, 0, i + 1, 1, 1);
                layout.attach(time, 1, i + 1, 1, 1);
                layout.attach(tz, 2, i + 1, 1, 1);
            }

            this._locations[i].actor = time;
        }

        if (this._grid.get_n_children() > 1) {
            if (!this._clockNotifyId)
                this._clockNotifyId =
                    this._clock.connect('notify::clock', this._updateLabels.bind(this));
            this._updateLabels();
        } else {
            if (this._clockNotifyId)
                this._clock.disconnect(this._clockNotifyId);
            this._clockNotifyId = 0;
        }
    }

    _getTimeAtLocation(location) {
        let tz = GLib.TimeZone.new(location.get_timezone().get_tzid());
        return GLib.DateTime.new_now(tz);
    }

    _updateLabels() {
        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i];
            let now = this._getTimeAtLocation(l.location);
            l.actor.text = Util.formatTime(now, { timeOnly: true });
        }
    }
};

var WeatherSection = class WeatherSection {
    constructor() {
        this._weatherClient = new Weather.WeatherClient();

        this.actor = new St.Button({ style_class: 'weather-button',
                                     x_fill: true,
                                     can_focus: true });
        this.actor.connect('clicked', () => {
            this._weatherClient.activateApp();

            Main.overview.hide();
            Main.panel.closeCalendar();
        });
        this.actor.connect('notify::mapped', () => {
            if (this.actor.mapped)
                this._weatherClient.update();
        });

        let box = new St.BoxLayout({ style_class: 'weather-box',
                                      vertical: true });

        this.actor.child = box;

        let titleBox = new St.BoxLayout();
        titleBox.add_child(new St.Label({ style_class: 'weather-header',
                                          x_align: Clutter.ActorAlign.START,
                                          x_expand: true,
                                          text: _("Weather") }));
        box.add_child(titleBox);

        this._titleLocation = new St.Label({ style_class: 'weather-header location',
                                             x_align: Clutter.ActorAlign.END });
        titleBox.add_child(this._titleLocation);

        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        this._forecastGrid = new St.Widget({ style_class: 'weather-grid',
                                             layout_manager: layout });
        layout.hookup_style(this._forecastGrid);
        box.add_child(this._forecastGrid);

        this._weatherClient.connect('changed', this._sync.bind(this));
        this._sync();
    }

    _getInfos() {
        let info = this._weatherClient.info;
        let forecasts = info.get_forecast_list();

        let current = info;
        let infos = [info];
        for (let i = 0; i < forecasts.length; i++) {
            let [ok, timestamp] = forecasts[i].get_value_update();
            let datetime = new Date(timestamp * 1000);
            if (!_isToday(datetime))
                continue; // Ignore forecasts from other days

            [ok, timestamp] = current.get_value_update();
            let currenttime = new Date(timestamp * 1000);
            if (currenttime.getHours() == datetime.getHours())
                continue; // Enforce a minimum interval of 1h

            current = forecasts[i];
            if (infos.push(current) == MAX_FORECASTS)
                break; // Use a maximum of five forecasts
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
            let [ok, timestamp] = fc.get_value_update();
            let timeStr = Util.formatTime(new Date(timestamp * 1000), {
                timeOnly: true
            });

            let icon = new St.Icon({ style_class: 'weather-forecast-icon',
                                     icon_name: fc.get_symbolic_icon_name(),
                                     x_align: Clutter.ActorAlign.CENTER,
                                     x_expand: true });
            let temp = new St.Label({ style_class: 'weather-forecast-temp',
                                      text: fc.get_temp_summary(),
                                      x_align: Clutter.ActorAlign.CENTER });
            let time = new St.Label({ style_class: 'weather-forecast-time',
                                      text: timeStr,
                                      x_align: Clutter.ActorAlign.CENTER });

            layout.attach(icon, col, 0, 1, 1);
            layout.attach(temp, col, 1, 1, 1);
            layout.attach(time, col, 2, 1, 1);
            col++;
        });
    }

    _setStatusLabel(text) {
        let layout = this._forecastGrid.layout_manager;
        let label = new St.Label({ text });
        layout.attach(label, 0, 0, 1, 1);
    }

    _updateForecasts() {
        this._forecastGrid.destroy_all_children();

        if (!this._weatherClient.hasLocation) {
            this._setStatusLabel(_("Select a location…"));
            return;
        }

        let info = this._weatherClient.info;
        this._titleLocation.text = info.get_location().get_name();

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
        this.actor.visible = this._weatherClient.available;

        if (!this.actor.visible)
            return;

        this._titleLocation.visible = this._weatherClient.hasLocation;

        this._updateForecasts();
    }
};

var MessagesIndicator = class MessagesIndicator {
    constructor() {
        this.actor = new St.Icon({ icon_name: 'message-indicator-symbolic',
                                   icon_size: 16,
                                   visible: false, y_expand: true,
                                   y_align: Clutter.ActorAlign.CENTER });

        this._sources = [];

        Main.messageTray.connect('source-added', this._onSourceAdded.bind(this));
        Main.messageTray.connect('source-removed', this._onSourceRemoved.bind(this));
        Main.messageTray.connect('queue-changed', this._updateCount.bind(this));

        let sources = Main.messageTray.getSources();
        sources.forEach(source => { this._onSourceAdded(null, source); });
    }

    _onSourceAdded(tray, source) {
        source.connect('count-updated', this._updateCount.bind(this));
        this._sources.push(source);
        this._updateCount();
    }

    _onSourceRemoved(tray, source) {
        this._sources.splice(this._sources.indexOf(source), 1);
        this._updateCount();
    }

    _updateCount() {
        let count = 0;
        this._sources.forEach(source => { count += source.unseenCount; });
        count -= Main.messageTray.queueCount;

        this.actor.visible = (count > 0);
    }
};

var IndicatorPad = GObject.registerClass(
class IndicatorPad extends St.Widget {
    _init(actor) {
        this._source = actor;
        this._source.connect('notify::visible', () => { this.queue_relayout(); });
        this._source.connect('notify::size', () => { this.queue_relayout(); });
        super._init();
    }

    vfunc_get_preferred_width(forHeight) {
        if (this._source.visible)
            return this._source.get_preferred_width(forHeight);
        return [0, 0];
    }

    vfunc_get_preferred_height(forWidth) {
        if (this._source.visible)
            return this._source.get_preferred_height(forWidth);
        return [0, 0];
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

    vfunc_allocate(container, allocation, flags) {
        super.vfunc_allocate(container, allocation, flags);

        let [width, height] = allocation.get_size();
        this._savedWidth = [width, width];
        this._savedHeight = [height, height];
    }
});

var CalendarColumnLayout = GObject.registerClass(
class CalendarColumnLayout extends Clutter.BoxLayout {
    _init(actor) {
        super._init({ orientation: Clutter.Orientation.VERTICAL });
        this._calActor = actor;
    }

    vfunc_get_preferred_width(container, forHeight) {
        if (!this._calActor || this._calActor.get_parent() != container)
            return super.vfunc_get_preferred_width(container, forHeight);
        return this._calActor.get_preferred_width(forHeight);
    }
});

var DateMenuButton = GObject.registerClass(
class DateMenuButton extends PanelMenu.Button {
    _init() {
        let item;
        let hbox;
        let vbox;

        let menuAlignment = 0.5;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            menuAlignment = 1.0 - menuAlignment;
        super._init(menuAlignment);

        this._clockDisplay = new St.Label({ y_align: Clutter.ActorAlign.CENTER });
        this._indicator = new MessagesIndicator();

        let box = new St.BoxLayout();
        box.add_actor(new IndicatorPad(this._indicator.actor));
        box.add_actor(this._clockDisplay);
        box.add_actor(this._indicator.actor);

        this.actor.label_actor = this._clockDisplay;
        this.actor.add_actor(box);
        this.actor.add_style_class_name ('clock-display');


        let layout = new FreezableBinLayout();
        let bin = new St.Widget({ layout_manager: layout });
        // For some minimal compatibility with PopupMenuItem
        bin._delegate = this;
        this.menu.box.add_child(bin);

        hbox = new St.BoxLayout({ name: 'calendarArea' });
        bin.add_actor(hbox);

        this._calendar = new Calendar.Calendar();
        this._calendar.connect('selected-date-changed',
                               (calendar, date) => {
                                   layout.frozen = !_isToday(date);
                                   this._messageList.setDate(date);
                               });

        this.menu.connect('open-state-changed', (menu, isOpen) => {
            // Whenever the menu is opened, select today
            if (isOpen) {
                let now = new Date();
                this._calendar.setDate(now);
                this._date.setDate(now);
                this._messageList.setDate(now);
            }
        });

        // Fill up the first column
        this._messageList = new Calendar.CalendarMessageList();
        hbox.add(this._messageList.actor, { expand: true, y_fill: false, y_align: St.Align.START });

        // Fill up the second column
        let boxLayout = new CalendarColumnLayout(this._calendar.actor);
        vbox = new St.Widget({ style_class: 'datemenu-calendar-column',
                               layout_manager: boxLayout });
        boxLayout.hookup_style(vbox);
        hbox.add(vbox);

        this._date = new TodayButton(this._calendar);
        vbox.add_actor(this._date.actor);

        vbox.add_actor(this._calendar.actor);

        this._displaysSection = new St.ScrollView({ style_class: 'datemenu-displays-section vfade',
                                                    x_expand: true, x_fill: true,
                                                    overlay_scrollbars: true });
        this._displaysSection.set_policy(St.PolicyType.NEVER, St.PolicyType.AUTOMATIC);
        vbox.add_actor(this._displaysSection);

        let displaysBox = new St.BoxLayout({ vertical: true,
                                             style_class: 'datemenu-displays-box' });
        this._displaysSection.add_actor(displaysBox);

        this._clocksItem = new WorldClocksSection();
        displaysBox.add(this._clocksItem.actor, { x_fill: true });

        this._weatherItem = new WeatherSection();
        displaysBox.add(this._weatherItem.actor, { x_fill: true });

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
        this._messageList.setEventSource(eventSource);

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
        if (showEvents) {
            eventSource = this._getEventSource();
        } else {
            eventSource = new Calendar.EmptyEventSource();
        }
        this._setEventSource(eventSource);

        // Displays are not actually expected to launch Settings when activated
        // but the corresponding app (clocks, weather); however we can consider
        // that display-specific settings, so re-use "allowSettings" here ...
        this._displaysSection.visible = Main.sessionMode.allowSettings;
    }
});
