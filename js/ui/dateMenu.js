// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GnomeDesktop = imports.gi.GnomeDesktop;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const GWeather = imports.gi.GWeather;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Params = imports.misc.params;
const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Calendar = imports.ui.calendar;
const Weather = imports.misc.weather;
const System = imports.system;

function _isToday(date) {
    let now = new Date();
    return now.getYear() == date.getYear() &&
           now.getMonth() == date.getMonth() &&
           now.getDate() == date.getDate();
}

var TodayButton = new Lang.Class({
    Name: 'TodayButton',

    _init(calendar) {
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
    },

    setDate(date) {
        this._dayLabel.set_text(date.toLocaleFormat('%A'));

        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the shell (e.g. "Tue 9:29 AM").
         */
        let dateFormat = Shell.util_translate_time_string (N_("%B %e %Y"));
        this._dateLabel.set_text(date.toLocaleFormat(dateFormat));

        /* Translators: This is the accessible name of the date button shown
         * below the time in the shell; it should combine the weekday and the
         * date, e.g. "Tuesday February 17 2015".
         */
        dateFormat = Shell.util_translate_time_string (N_("%A %B %e %Y"));
        this.actor.accessible_name = date.toLocaleFormat(dateFormat);
    }
});

var WorldClocksSection = new Lang.Class({
    Name: 'WorldClocksSection',

    _init() {
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
    },

    _sync() {
        this.actor.visible = this._clockAppMon.available;
    },

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

        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i].location;

            let name = l.get_level() == GWeather.LocationLevel.NAMED_TIMEZONE ? l.get_name()
                                                                              : l.get_city_name();
            let label = new St.Label({ style_class: 'world-clocks-city',
                                       text: name,
                                       x_align: Clutter.ActorAlign.START,
                                       x_expand: true });

            let time = new St.Label({ style_class: 'world-clocks-time',
                                      x_align: Clutter.ActorAlign.END,
                                      x_expand: true });

            if (this._grid.text_direction == Clutter.TextDirection.RTL) {
                layout.attach(time, 0, i + 1, 1, 1);
                layout.attach(label, 1, i + 1, 1, 1);
            } else {
                layout.attach(label, 0, i + 1, 1, 1);
                layout.attach(time, 1, i + 1, 1, 1);
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
    },

    _updateLabels() {
        for (let i = 0; i < this._locations.length; i++) {
            let l = this._locations[i];
            let tz = GLib.TimeZone.new(l.location.get_timezone().get_tzid());
            let now = GLib.DateTime.new_now(tz);
            l.actor.text = Util.formatTime(now, { timeOnly: true });
        }
    }
});

var WeatherSection = new Lang.Class({
    Name: 'WeatherSection',

    _init() {
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

        box.add_child(new St.Label({ style_class: 'weather-header',
                                     x_align: Clutter.ActorAlign.START,
                                     text: _("Weather") }));

        this._conditionsLabel = new St.Label({ style_class: 'weather-conditions',
                                               x_align: Clutter.ActorAlign.START });
        this._conditionsLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._conditionsLabel.clutter_text.line_wrap = true;
        box.add_child(this._conditionsLabel);

        this._weatherClient.connect('changed', this._sync.bind(this));
        this._sync();
    },

    _getSummary(info, capitalize=false) {
        let options = capitalize ? GWeather.FormatOptions.SENTENCE_CAPITALIZATION
                                 : GWeather.FormatOptions.NO_CAPITALIZATION;

        let [ok, phenomenon, qualifier] = info.get_value_conditions();
        if (ok)
            return new GWeather.Conditions({ significant: true,
                                             phenomenon,
                                             qualifier }).to_string_full(options);

        let [, sky] = info.get_value_sky();
        return GWeather.Sky.to_string_full(sky, options);
    },

    _sameSummary(info1, info2) {
        let [ok1, phenom1, qualifier1] = info1.get_value_conditions();
        let [ok2, phenom2, qualifier2] = info2.get_value_conditions();
        if (ok1 || ok2)
            return ok1 == ok2 && phenom1 == phenom2 && qualifier1 == qualifier2;

        let [, sky1] = info1.get_value_sky();
        let [, sky2] = info2.get_value_sky();
        return sky1 == sky2;
    },

    _getSummaryText() {
        let info = this._weatherClient.info;
        let forecasts = info.get_forecast_list();
        if (forecasts.length == 0) // No forecasts, just current conditions
            return '%s.'.format(this._getSummary(info, true));

        let current = info;
        let infos = [info];
        for (let i = 0; i < forecasts.length; i++) {
            let [ok, timestamp] = forecasts[i].get_value_update();
            if (!_isToday(new Date(timestamp * 1000)))
                continue; // Ignore forecasts from other days

            if (this._sameSummary(current, forecasts[i]))
                continue; // Ignore consecutive runs of equal summaries

            current = forecasts[i];
            if (infos.push(current) == 3)
                break; // Use a maximum of three summaries
        }

        let fmt;
        switch(infos.length) {
            /* Translators: %s is a weather condition like "Clear sky"; see
               libgweather for the possible condition strings. If at all
               possible, the sentence should match the grammatical case etc. of
               the inserted conditions. */
            case 1: fmt = _("%s all day."); break;

            /* Translators: %s is a weather condition like "Clear sky"; see
               libgweather for the possible condition strings. If at all
               possible, the sentence should match the grammatical case etc. of
               the inserted conditions. */
            case 2: fmt = _("%s, then %s later."); break;

            /* Translators: %s is a weather condition like "Clear sky"; see
               libgweather for the possible condition strings. If at all
               possible, the sentence should match the grammatical case etc. of
               the inserted conditions. */
            case 3: fmt = _("%s, then %s, followed by %s later."); break;
        }
        let summaries = infos.map((info, i) => {
            let capitalize = i == 0 && fmt.startsWith('%s');
            return this._getSummary(info, capitalize);
        });
        return String.prototype.format.apply(fmt, summaries);
    },

    _getLabelText() {
        if (!this._weatherClient.hasLocation)
            return _("Select a location…");

        if (this._weatherClient.loading)
            return _("Loading…");

        let info = this._weatherClient.info;
        if (info.is_valid())
            return this._getSummaryText() + ' ' +
                   /* Translators: %s is a temperature with unit, e.g. "23℃" */
                   _("Feels like %s.").format(info.get_apparent());

        if (info.network_error())
            return _("Go online for weather information");

        return _("Weather information is currently unavailable");
    },

    _sync() {
        this.actor.visible = this._weatherClient.available;

        if (!this.actor.visible)
            return;

        this._conditionsLabel.text = this._getLabelText();
    }
});

var MessagesIndicator = new Lang.Class({
    Name: 'MessagesIndicator',

    _init() {
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
    },

    _onSourceAdded(tray, source) {
        source.connect('count-updated', this._updateCount.bind(this));
        this._sources.push(source);
        this._updateCount();
    },

    _onSourceRemoved(tray, source) {
        this._sources.splice(this._sources.indexOf(source), 1);
        this._updateCount();
    },

    _updateCount() {
        let count = 0;
        this._sources.forEach(source => { count += source.unseenCount; });
        count -= Main.messageTray.queueCount;

        this.actor.visible = (count > 0);
    }
});

var IndicatorPad = new Lang.Class({
    Name: 'IndicatorPad',
    Extends: St.Widget,

    _init(actor) {
        this._source = actor;
        this._source.connect('notify::visible', () => { this.queue_relayout(); });
        this.parent();
    },

    vfunc_get_preferred_width(container, forHeight) {
        if (this._source.visible)
            return this._source.get_preferred_width(forHeight);
        return [0, 0];
    },

    vfunc_get_preferred_height(container, forWidth) {
        if (this._source.visible)
            return this._source.get_preferred_height(forWidth);
        return [0, 0];
    }
});

var FreezableBinLayout = new Lang.Class({
    Name: 'FreezableBinLayout',
    Extends: Clutter.BinLayout,

    _init() {
        this.parent();

        this._frozen = false;
        this._savedWidth = [NaN, NaN];
        this._savedHeight = [NaN, NaN];
    },

    set frozen(v) {
        if (this._frozen == v)
            return;

        this._frozen = v;
        if (!this._frozen)
            this.layout_changed();
    },

    vfunc_get_preferred_width(container, forHeight) {
        if (!this._frozen || this._savedWidth.some(isNaN))
            return this.parent(container, forHeight);
        return this._savedWidth;
    },

    vfunc_get_preferred_height(container, forWidth) {
        if (!this._frozen || this._savedHeight.some(isNaN))
            return this.parent(container, forWidth);
        return this._savedHeight;
    },

    vfunc_allocate(container, allocation, flags) {
        this.parent(container, allocation, flags);

        let [width, height] = allocation.get_size();
        this._savedWidth = [width, width];
        this._savedHeight = [height, height];
    }
});

var CalendarColumnLayout = new Lang.Class({
    Name: 'CalendarColumnLayout',
    Extends: Clutter.BoxLayout,

    _init(actor) {
        this.parent({ orientation: Clutter.Orientation.VERTICAL });
        this._calActor = actor;
    },

    vfunc_get_preferred_width(container, forHeight) {
        if (!this._calActor || this._calActor.get_parent() != container)
            return this.parent(container, forHeight);
        return this._calActor.get_preferred_width(forHeight);
    }
});

var DateMenuButton = new Lang.Class({
    Name: 'DateMenuButton',
    Extends: PanelMenu.Button,

    _init() {
        let item;
        let hbox;
        let vbox;

        let menuAlignment = 0.5;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            menuAlignment = 1.0 - menuAlignment;
        this.parent(menuAlignment);

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
        this._displaysSection.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
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
    },

    _getEventSource() {
        return new Calendar.DBusEventSource();
    },

    _setEventSource(eventSource) {
        if (this._eventSource)
            this._eventSource.destroy();

        this._calendar.setEventSource(eventSource);
        this._messageList.setEventSource(eventSource);

        this._eventSource = eventSource;
    },

    _updateTimeZone() {
        // SpiderMonkey caches the time zone so we must explicitly clear it
        // before we can update the calendar, see
        // https://bugzilla.gnome.org/show_bug.cgi?id=678507
        System.clearDateCaches();

        // To reduce the number of times that Shell opens /etc/localtime on
        // the main thread to parse timezone info, we cache the result. That
        // needs to be cleared when we change timezones.
        Shell.util_clear_timezone_cache();

        this._calendar.updateTimeZone();
    },

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
