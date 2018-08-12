// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Atk = imports.gi.Atk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Calendar = imports.ui.calendar;
const GnomeSession = imports.misc.gnomeSession;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';

var ANIMATION_TIME = 0.2;
var NOTIFICATION_TIMEOUT = 4;

var HIDE_TIMEOUT = 0.2;
var LONGER_HIDE_TIMEOUT = 0.6;

var MAX_NOTIFICATIONS_IN_QUEUE = 3;
var MAX_NOTIFICATIONS_PER_SOURCE = 3;
var MAX_NOTIFICATION_BUTTONS = 3;

// We delay hiding of the tray if the mouse is within MOUSE_LEFT_ACTOR_THRESHOLD
// range from the point where it left the tray.
var MOUSE_LEFT_ACTOR_THRESHOLD = 20;

var IDLE_TIME = 1000;

var State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3
};

// These reasons are useful when we destroy the notifications received through
// the notification daemon. We use EXPIRED for notifications that we dismiss
// and the user did not interact with, DISMISSED for all other notifications
// that were destroyed as a result of a user action, and SOURCE_CLOSED for the
// notifications that were requested to be destroyed by the associated source.
var NotificationDestroyedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    SOURCE_CLOSED: 3
};

// Message tray has its custom Urgency enumeration. LOW, NORMAL and CRITICAL
// urgency values map to the corresponding values for the notifications received
// through the notification daemon. HIGH urgency value is used for chats received
// through the Telepathy client.
var Urgency = {
    LOW: 0,
    NORMAL: 1,
    HIGH: 2,
    CRITICAL: 3
};

var FocusGrabber = new Lang.Class({
    Name: 'FocusGrabber',

    _init(actor) {
        this._actor = actor;
        this._prevKeyFocusActor = null;
        this._focusActorChangedId = 0;
        this._focused = false;
    },

    grabFocus() {
        if (this._focused)
            return;

        this._prevKeyFocusActor = global.stage.get_key_focus();

        this._focusActorChangedId = global.stage.connect('notify::key-focus', this._focusActorChanged.bind(this));

        if (!this._actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false))
            this._actor.grab_key_focus();

        this._focused = true;
    },

    _focusUngrabbed() {
        if (!this._focused)
            return false;

        if (this._focusActorChangedId > 0) {
            global.stage.disconnect(this._focusActorChangedId);
            this._focusActorChangedId = 0;
        }

        this._focused = false;
        return true;
    },

    _focusActorChanged() {
        let focusedActor = global.stage.get_key_focus();
        if (!focusedActor || !this._actor.contains(focusedActor))
            this._focusUngrabbed();
    },

    ungrabFocus() {
        if (!this._focusUngrabbed())
            return;

        if (this._prevKeyFocusActor) {
            global.stage.set_key_focus(this._prevKeyFocusActor);
            this._prevKeyFocusActor = null;
        } else {
            let focusedActor = global.stage.get_key_focus();
            if (focusedActor && this._actor.contains(focusedActor))
                global.stage.set_key_focus(null);
        }
    }
});

// NotificationPolicy:
// An object that holds all bits of configurable policy related to a notification
// source, such as whether to play sound or honour the critical bit.
//
// A notification without a policy object will inherit the default one.
var NotificationPolicy = new Lang.Class({
    Name: 'NotificationPolicy',

    _init(params) {
        params = Params.parse(params, { enable: true,
                                        enableSound: true,
                                        showBanners: true,
                                        forceExpanded: false,
                                        showInLockScreen: true,
                                        detailsInLockScreen: false
                                      });
        Lang.copyProperties(params, this);
    },

    // Do nothing for the default policy. These methods are only useful for the
    // GSettings policy.
    store() { },
    destroy() { }
});
Signals.addSignalMethods(NotificationPolicy.prototype);

var NotificationGenericPolicy = new Lang.Class({
    Name: 'NotificationGenericPolicy',
    Extends: NotificationPolicy,

    _init() {
        // Don't chain to parent, it would try setting
        // our properties to the defaults

        this.id = 'generic';

        this._masterSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.notifications' });
        this._masterSettings.connect('changed', this._changed.bind(this));
    },

    store() { },

    destroy() {
        this._masterSettings.run_dispose();
    },

    _changed(settings, key) {
        this.emit('policy-changed', key);
    },

    get enable() {
        return true;
    },

    get enableSound() {
        return true;
    },

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners');
    },

    get forceExpanded() {
        return false;
    },

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen');
    },

    get detailsInLockScreen() {
        return false;
    }
});

var NotificationApplicationPolicy = new Lang.Class({
    Name: 'NotificationApplicationPolicy',
    Extends: NotificationPolicy,

    _init(id) {
        // Don't chain to parent, it would try setting
        // our properties to the defaults

        this.id = id;
        this._canonicalId = this._canonicalizeId(id);

        this._masterSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.notifications' });
        this._settings = new Gio.Settings({ schema_id: 'org.gnome.desktop.notifications.application',
                                            path: '/org/gnome/desktop/notifications/application/' + this._canonicalId + '/' });

        this._masterSettings.connect('changed', this._changed.bind(this));
        this._settings.connect('changed', this._changed.bind(this));
    },

    store() {
        this._settings.set_string('application-id', this.id + '.desktop');

        let apps = this._masterSettings.get_strv('application-children');
        if (apps.indexOf(this._canonicalId) < 0) {
            apps.push(this._canonicalId);
            this._masterSettings.set_strv('application-children', apps);
        }
    },

    destroy() {
        this._masterSettings.run_dispose();
        this._settings.run_dispose();
    },

    _changed(settings, key) {
        this.emit('policy-changed', key);
        if (key == 'enable')
            this.emit('enable-changed');
    },

    _canonicalizeId(id) {
        // Keys are restricted to lowercase alphanumeric characters and dash,
        // and two dashes cannot be in succession
        return id.toLowerCase().replace(/[^a-z0-9\-]/g, '-').replace(/--+/g, '-');
    },

    get enable() {
        return this._settings.get_boolean('enable');
    },

    get enableSound() {
        return this._settings.get_boolean('enable-sound-alerts');
    },

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners') &&
            this._settings.get_boolean('show-banners');
    },

    get forceExpanded() {
        return this._settings.get_boolean('force-expanded');
    },

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen') &&
            this._settings.get_boolean('show-in-lock-screen');
    },

    get detailsInLockScreen() {
        return this._settings.get_boolean('details-in-lock-screen');
    }
});

// Notification:
// @source: the notification's Source
// @title: the title
// @banner: the banner text
// @params: optional additional params
//
// Creates a notification. In the banner mode, the notification
// will show an icon, @title (in bold) and @banner, all on a single
// line (with @banner ellipsized if necessary).
//
// The notification will be expandable if either it has additional
// elements that were added to it or if the @banner text did not
// fit fully in the banner mode. When the notification is expanded,
// the @banner text from the top line is always removed. The complete
// @banner text is added as the first element in the content section,
// unless 'customContent' parameter with the value 'true' is specified
// in @params.
//
// Additional notification content can be added with addActor() and
// addBody() methods. The notification content is put inside a
// scrollview, so if it gets too tall, the notification will scroll
// rather than continue to grow. In addition to this main content
// area, there is also a single-row action area, which is not
// scrolled and can contain a single actor. The action area can
// be set by calling setActionArea() method. There is also a
// convenience method addButton() for adding a button to the action
// area.
//
// If @params contains a 'customContent' parameter with the value %true,
// then @banner will not be shown in the body of the notification when the
// notification is expanded and calls to update() will not clear the content
// unless 'clear' parameter with value %true is explicitly specified.
//
// By default, the icon shown is the same as the source's.
// However, if @params contains a 'gicon' parameter, the passed in gicon
// will be used.
//
// You can add a secondary icon to the banner with 'secondaryGIcon'. There
// is no fallback for this icon.
//
// If @params contains 'bannerMarkup', with the value %true, a subset (<b>,
// <i> and <u>) of the markup in [1] will be interpreted within @banner. If
// the parameter is not present, then anything that looks like markup
// in @banner will appear literally in the output.
//
// If @params contains a 'clear' parameter with the value %true, then
// the content and the action area of the notification will be cleared.
// The content area is also always cleared if 'customContent' is false
// because it might contain the @banner that didn't fit in the banner mode.
//
// If @params contains 'soundName' or 'soundFile', the corresponding
// event sound is played when the notification is shown (if the policy for
// @source allows playing sounds).
//
// [1] https://developer.gnome.org/notification-spec/#markup 
var Notification = new Lang.Class({
    Name: 'Notification',

    _init(source, title, banner, params) {
        this.source = source;
        this.title = title;
        this.urgency = Urgency.NORMAL;
        this.resident = false;
        // 'transient' is a reserved keyword in JS, so we have to use an alternate variable name
        this.isTransient = false;
        this.forFeedback = false;
        this._acknowledged = false;
        this.bannerBodyText = null;
        this.bannerBodyMarkup = false;
        this._soundName = null;
        this._soundFile = null;
        this._soundPlayed = false;
        this.actions = [];

        // If called with only one argument we assume the caller
        // will call .update() later on. This is the case of
        // NotificationDaemon, which wants to use the same code
        // for new and updated notifications
        if (arguments.length != 1)
            this.update(title, banner, params);
    },

    // update:
    // @title: the new title
    // @banner: the new banner
    // @params: as in the Notification constructor
    //
    // Updates the notification by regenerating its icon and updating
    // the title/banner. If @params.clear is %true, it will also
    // remove any additional actors/action buttons previously added.
    update(title, banner, params) {
        params = Params.parse(params, { gicon: null,
                                        secondaryGIcon: null,
                                        bannerMarkup: false,
                                        clear: false,
                                        datetime: null,
                                        soundName: null,
                                        soundFile: null });

        this.title = title;
        this.bannerBodyText = banner;
        this.bannerBodyMarkup = params.bannerMarkup;

        if (params.datetime)
            this.datetime = params.datetime;
        else
            this.datetime = GLib.DateTime.new_now_local();

        if (params.gicon || params.clear)
            this.gicon = params.gicon;

        if (params.secondaryGIcon || params.clear)
            this.secondaryGIcon = params.secondaryGIcon;

        if (params.clear)
            this.actions = [];

        if (this._soundName != params.soundName ||
            this._soundFile != params.soundFile) {
            this._soundName = params.soundName;
            this._soundFile = params.soundFile;
            this._soundPlayed = false;
        }

        this.emit('updated', params.clear);
    },

    // addAction:
    // @label: the label for the action's button
    // @callback: the callback for the action
    addAction(label, callback) {
        this.actions.push({ label: label, callback: callback });
    },

    get acknowledged() {
        return this._acknowledged;
    },

    set acknowledged(v) {
        if (this._acknowledged == v)
            return;
        this._acknowledged = v;
        this.emit('acknowledged-changed');
    },

    setUrgency(urgency) {
        this.urgency = urgency;
    },

    setResident(resident) {
        this.resident = resident;
    },

    setTransient(isTransient) {
        this.isTransient = isTransient;
    },

    setForFeedback(forFeedback) {
        this.forFeedback = forFeedback;
    },

    playSound() {
        if (this._soundPlayed)
            return;

        if (!this.source.policy.enableSound) {
            this._soundPlayed = true;
            return;
        }

        if (this._soundName) {
            if (this.source.app) {
                let app = this.source.app;

                global.play_theme_sound_full(0, this._soundName,
                                             this.title, null,
                                             app.get_id(), app.get_name());
            } else {
                global.play_theme_sound(0, this._soundName, this.title, null);
            }
        } else if (this._soundFile) {
            if (this.source.app) {
                let app = this.source.app;

                global.play_sound_file_full(0, this._soundFile,
                                            this.title, null,
                                            app.get_id(), app.get_name());
            } else {
                global.play_sound_file(0, this._soundFile, this.title, null);
            }
        }
    },

    // Allow customizing the banner UI:
    // the default implementation defers the creation to
    // the source (which will create a NotificationBanner),
    // so customization can be done by subclassing either
    // Notification or Source
    createBanner() {
        return this.source.createBanner(this);
    },

    activate() {
        this.emit('activated');
        if (!this.resident)
            this.destroy();
    },

    destroy(reason) {
        if (!reason)
            reason = NotificationDestroyedReason.DISMISSED;
        this.emit('destroy', reason);
    }
});
Signals.addSignalMethods(Notification.prototype);

var NotificationBanner = new Lang.Class({
    Name: 'NotificationBanner',
    Extends: Calendar.NotificationMessage,

    _init(notification) {
        this.parent(notification);

        this.actor.can_focus = false;
        this.actor.add_style_class_name('notification-banner');

        this._buttonBox = null;

        this._addActions();
        this._addSecondaryIcon();

        this._activatedId = this.notification.connect('activated', () => {
            // We hide all types of notifications once the user clicks on
            // them because the common outcome of clicking should be the
            // relevant window being brought forward and the user's
            // attention switching to the window.
            this.emit('done-displaying');
        });
    },

    _onDestroy() {
        this.parent();
        this.notification.disconnect(this._activatedId);
    },

    _onUpdated(n, clear) {
        this.parent(n, clear);

        if (clear) {
            this.setSecondaryActor(null);
            this.setActionArea(null);
            this._buttonBox = null;
        }

        this._addActions();
        this._addSecondaryIcon();
    },

    _addActions() {
        this.notification.actions.forEach(action => {
            this.addAction(action.label, action.callback);
        });
    },

    _addSecondaryIcon() {
        if (this.notification.secondaryGIcon) {
            let icon = new St.Icon({ gicon: this.notification.secondaryGIcon,
                                     x_align: Clutter.ActorAlign.END });
            this.setSecondaryActor(icon);
        }
    },

    addButton(button, callback) {
        if (!this._buttonBox) {
            this._buttonBox = new St.BoxLayout({ style_class: 'notification-actions',
                                                 x_expand: true });
            this.setActionArea(this._buttonBox);
            global.focus_manager.add_group(this._buttonBox);
        }

        if (this._buttonBox.get_n_children() >= MAX_NOTIFICATION_BUTTONS)
            return null;

        this._buttonBox.add(button);
        button.connect('clicked', () => {
            callback();

            if (!this.notification.resident) {
                // We don't hide a resident notification when the user invokes one of its actions,
                // because it is common for such notifications to update themselves with new
                // information based on the action. We'd like to display the updated information
                // in place, rather than pop-up a new notification.
                this.emit('done-displaying');
                this.notification.destroy();
            }
        });

        return button;
    },

    addAction(label, callback) {
        let button = new St.Button({ style_class: 'notification-button',
                                     label: label,
                                     x_expand: true,
                                     can_focus: true });

        return this.addButton(button, callback);
    }
});

var SourceActor = new Lang.Class({
    Name: 'SourceActor',

    _init(source, size) {
        this._source = source;
        this._size = size;

        this.actor = new Shell.GenericContainer();
        this.actor.connect('get-preferred-width', this._getPreferredWidth.bind(this));
        this.actor.connect('get-preferred-height', this._getPreferredHeight.bind(this));
        this.actor.connect('allocate', this._allocate.bind(this));
        this.actor.connect('destroy', () => {
            this._source.disconnect(this._iconUpdatedId);
            this._actorDestroyed = true;
        });
        this._actorDestroyed = false;

        let scale_factor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._iconBin = new St.Bin({ x_fill: true,
                                     height: size * scale_factor,
                                     width: size * scale_factor });

        this.actor.add_actor(this._iconBin);

        this._iconUpdatedId = this._source.connect('icon-updated', this._updateIcon.bind(this));
        this._updateIcon();
    },

    setIcon(icon) {
        this._iconBin.child = icon;
        this._iconSet = true;
    },

    _getPreferredWidth(actor, forHeight, alloc) {
        let [min, nat] = this._iconBin.get_preferred_width(forHeight);
        alloc.min_size = min; alloc.nat_size = nat;
    },

    _getPreferredHeight(actor, forWidth, alloc) {
        let [min, nat] = this._iconBin.get_preferred_height(forWidth);
        alloc.min_size = min; alloc.nat_size = nat;
    },

    _allocate(actor, box, flags) {
        // the iconBin should fill our entire box
        this._iconBin.allocate(box, flags);
    },

    _updateIcon() {
        if (this._actorDestroyed)
            return;

        if (!this._iconSet)
            this._iconBin.child = this._source.createIcon(this._size);
    }
});

var SourceActorWithLabel = new Lang.Class({
    Name: 'SourceActorWithLabel',
    Extends: SourceActor,

    _init(source, size) {
        this.parent(source, size);

        this._counterLabel = new St.Label({ x_align: Clutter.ActorAlign.CENTER,
                                            x_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER,
                                            y_expand: true });

        this._counterBin = new St.Bin({ style_class: 'summary-source-counter',
                                        child: this._counterLabel,
                                        layout_manager: new Clutter.BinLayout() });
        this._counterBin.hide();

        this._counterBin.connect('style-changed', () => {
            let themeNode = this._counterBin.get_theme_node();
            this._counterBin.translation_x = themeNode.get_length('-shell-counter-overlap-x');
            this._counterBin.translation_y = themeNode.get_length('-shell-counter-overlap-y');
        });

        this.actor.add_actor(this._counterBin);

        this._countUpdatedId = this._source.connect('count-updated', this._updateCount.bind(this));
        this._updateCount();

        this.actor.connect('destroy', () => {
            this._source.disconnect(this._countUpdatedId);
        });
    },

    _allocate(actor, box, flags) {
        this.parent(actor, box, flags);

        let childBox = new Clutter.ActorBox();

        let [minWidth, minHeight, naturalWidth, naturalHeight] = this._counterBin.get_preferred_size();
        let direction = this.actor.get_text_direction();

        if (direction == Clutter.TextDirection.LTR) {
            // allocate on the right in LTR
            childBox.x1 = box.x2 - naturalWidth;
            childBox.x2 = box.x2;
        } else {
            // allocate on the left in RTL
            childBox.x1 = 0;
            childBox.x2 = naturalWidth;
        }

        childBox.y1 = box.y2 - naturalHeight;
        childBox.y2 = box.y2;

        this._counterBin.allocate(childBox, flags);
    },

    _updateCount() {
        if (this._actorDestroyed)
            return;

        this._counterBin.visible = this._source.countVisible;

        let text;
        if (this._source.count < 100)
            text = this._source.count.toString();
        else
            text = String.fromCharCode(0x22EF); // midline horizontal ellipsis

        this._counterLabel.set_text(text);
    }
});

var Source = new Lang.Class({
    Name: 'MessageTraySource',

    SOURCE_ICON_SIZE: 48,

    _init(title, iconName) {
        this.title = title;
        this.iconName = iconName;

        this.isChat = false;

        this.notifications = [];

        this.policy = this._createPolicy();
    },

    get count() {
        return this.notifications.length;
    },

    get unseenCount() {
        return this.notifications.filter(n => !n.acknowledged).length;
    },

    get countVisible() {
        return this.count > 1;
    },

    countUpdated() {
        this.emit('count-updated');
    },

    _createPolicy() {
        return new NotificationPolicy();
    },

    setTitle(newTitle) {
        this.title = newTitle;
        this.emit('title-changed');
    },

    createBanner(notification) {
        return new NotificationBanner(notification);
    },

    // Called to create a new icon actor.
    // Provides a sane default implementation, override if you need
    // something more fancy.
    createIcon(size) {
        return new St.Icon({ gicon: this.getIcon(),
                             icon_size: size });
    },

    getIcon() {
        return new Gio.ThemedIcon({ name: this.iconName });
    },

    _onNotificationDestroy(notification) {
        let index = this.notifications.indexOf(notification);
        if (index < 0)
            return;

        this.notifications.splice(index, 1);
        if (this.notifications.length == 0)
            this.destroy();

        this.countUpdated();
    },

    pushNotification(notification) {
        if (this.notifications.indexOf(notification) >= 0)
            return;

        while (this.notifications.length >= MAX_NOTIFICATIONS_PER_SOURCE)
            this.notifications.shift().destroy(NotificationDestroyedReason.EXPIRED);

        notification.connect('destroy', this._onNotificationDestroy.bind(this));
        notification.connect('acknowledged-changed', this.countUpdated.bind(this));
        this.notifications.push(notification);
        this.emit('notification-added', notification);

        this.countUpdated();
    },

    notify(notification) {
        notification.acknowledged = false;
        this.pushNotification(notification);

        if (this.policy.showBanners || notification.urgency == Urgency.CRITICAL) {
            this.emit('notify', notification);
        } else {
            notification.playSound();
        }
    },

    destroy(reason) {
        this.policy.destroy();

        let notifications = this.notifications;
        this.notifications = [];

        for (let i = 0; i < notifications.length; i++)
            notifications[i].destroy(reason);

        this.emit('destroy', reason);
    },

    iconUpdated() {
        this.emit('icon-updated');
    },

    // To be overridden by subclasses
    open() {
    },

    destroyNonResidentNotifications() {
        for (let i = this.notifications.length - 1; i >= 0; i--)
            if (!this.notifications[i].resident)
                this.notifications[i].destroy();

        this.countUpdated();
    }
});
Signals.addSignalMethods(Source.prototype);

var MessageTray = new Lang.Class({
    Name: 'MessageTray',

    _init() {
        this._presence = new GnomeSession.Presence((proxy, error) => {
            this._onStatusChanged(proxy.status);
        });
        this._busy = false;
        this._bannerBlocked = false;
        this._presence.connectSignal('StatusChanged', (proxy, senderName, [status]) => {
            this._onStatusChanged(status);
        });

        global.stage.connect('enter-event', (a, ev) => {
            // HACK: St uses ClutterInputDevice for hover tracking, which
            // misses relevant X11 events when untracked actors are
            // involved (read: the notification banner in normal mode),
            // so fix up Clutter's view of the pointer position in
            // that case.
            let related = ev.get_related();
            if (!related || this.actor.contains(related))
                global.sync_pointer();
        });

        this.actor = new St.Widget({ visible: false,
                                     clip_to_allocation: true,
                                     layout_manager: new Clutter.BinLayout() });
        let constraint = new Layout.MonitorConstraint({ primary: true });
        Main.layoutManager.panelBox.bind_property('visible',
                                                  constraint, 'work-area',
                                                  GObject.BindingFlags.SYNC_CREATE);
        this.actor.add_constraint(constraint);

        this._bannerBin = new St.Widget({ name: 'notification-container',
                                          reactive: true,
                                          track_hover: true,
                                          y_align: Clutter.ActorAlign.START,
                                          x_align: Clutter.ActorAlign.CENTER,
                                          y_expand: true,
                                          x_expand: true,
                                          layout_manager: new Clutter.BinLayout() });
        this._bannerBin.connect('key-release-event',
                                this._onNotificationKeyRelease.bind(this));
        this._bannerBin.connect('notify::hover',
                                this._onNotificationHoverChanged.bind(this));
        this.actor.add_actor(this._bannerBin);

        this._notificationFocusGrabber = new FocusGrabber(this._bannerBin);
        this._notificationQueue = [];
        this._notification = null;
        this._banner = null;
        this._bannerClickedId = 0;

        this._userActiveWhileNotificationShown = false;

        this.idleMonitor = Meta.IdleMonitor.get_core();

        this._useLongerNotificationLeftTimeout = false;

        // pointerInNotification is sort of a misnomer -- it tracks whether
        // a message tray notification should expand. The value is
        // partially driven by the hover state of the notification, but has
        // a lot of complex state related to timeouts and the current
        // state of the pointer when a notification pops up.
        this._pointerInNotification = false;

        // This tracks this._bannerBin.hover and is used to fizzle
        // out non-changing hover notifications in onNotificationHoverChanged.
        this._notificationHovered = false;

        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._notificationRemoved = false;

        Main.layoutManager.addChrome(this.actor, { affectsInputRegion: false });
        Main.layoutManager.trackChrome(this._bannerBin, { affectsInputRegion: true });

        global.display.connect('in-fullscreen-changed', this._updateState.bind(this));

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));

        Main.overview.connect('window-drag-begin',
                              this._onDragBegin.bind(this));
        Main.overview.connect('window-drag-cancelled',
                              this._onDragEnd.bind(this));
        Main.overview.connect('window-drag-end',
                              this._onDragEnd.bind(this));

        Main.overview.connect('item-drag-begin',
                              this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-cancelled',
                              this._onDragEnd.bind(this));
        Main.overview.connect('item-drag-end',
                              this._onDragEnd.bind(this));

        Main.xdndHandler.connect('drag-begin',
                                 this._onDragBegin.bind(this));
        Main.xdndHandler.connect('drag-end',
                                 this._onDragEnd.bind(this));

        Main.wm.addKeybinding('focus-active-notification',
                              new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.NONE,
                              Shell.ActionMode.NORMAL |
                              Shell.ActionMode.OVERVIEW,
                              this._expandActiveNotification.bind(this));

        this._sources = new Map();

        this._sessionUpdated();
    },

    _sessionUpdated() {
        this._updateState();
    },

    _onDragBegin() {
        Shell.util_set_hidden_from_pick(this.actor, true);
    },

    _onDragEnd() {
        Shell.util_set_hidden_from_pick(this.actor, false);
    },

    get bannerAlignment() {
        return this._bannerBin.get_x_align();
    },

    set bannerAlignment(align) {
        this._bannerBin.set_x_align(align);
    },

    _onNotificationKeyRelease(actor, event) {
        if (event.get_key_symbol() == Clutter.KEY_Escape && event.get_state() == 0) {
            this._expireNotification();
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    _expireNotification() {
        this._notificationExpired = true;
        this._updateState();
    },

    get queueCount() {
        return this._notificationQueue.length;
    },

    set bannerBlocked(v) {
        if (this._bannerBlocked == v)
            return;
        this._bannerBlocked = v;
        this._updateState();
    },

    contains(source) {
        return this._sources.has(source);
    },

    add(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.title);
            return;
        }

        // Register that we got a notification for this source
        source.policy.store();

        source.policy.connect('enable-changed', () => {
            this._onSourceEnableChanged(source.policy, source);
        });
        source.policy.connect('policy-changed', this._updateState.bind(this));
        this._onSourceEnableChanged(source.policy, source);
    },

    _addSource(source) {
        let obj = {
            source: source,
            notifyId: 0,
            destroyId: 0,
        };

        this._sources.set(source, obj);

        obj.notifyId = source.connect('notify', this._onNotify.bind(this));
        obj.destroyId = source.connect('destroy', this._onSourceDestroy.bind(this));

        this.emit('source-added', source);
    },

    _removeSource(source) {
        let obj = this._sources.get(source);
        this._sources.delete(source);

        source.disconnect(obj.notifyId);
        source.disconnect(obj.destroyId);

        this.emit('source-removed', source);
    },

    getSources() {
        return [...this._sources.keys()];
    },

    _onSourceEnableChanged(policy, source) {
        let wasEnabled = this.contains(source);
        let shouldBeEnabled = policy.enable;

        if (wasEnabled != shouldBeEnabled) {
            if (shouldBeEnabled)
                this._addSource(source);
            else
                this._removeSource(source);
        }
    },

    _onSourceDestroy(source) {
        this._removeSource(source);
    },

    _onNotificationDestroy(notification) {
        if (this._notification == notification && (this._notificationState == State.SHOWN || this._notificationState == State.SHOWING)) {
            this._updateNotificationTimeout(0);
            this._notificationRemoved = true;
            this._updateState();
            return;
        }

        let index = this._notificationQueue.indexOf(notification);
        if (index != -1) {
            this._notificationQueue.splice(index, 1);
            this.emit('queue-changed');
        }
    },

    _onNotify(source, notification) {
        if (this._notification == notification) {
            // If a notification that is being shown is updated, we update
            // how it is shown and extend the time until it auto-hides.
            // If a new notification is updated while it is being hidden,
            // we stop hiding it and show it again.
            this._updateShowingNotification();
        } else if (this._notificationQueue.indexOf(notification) < 0) {
            // If the queue is "full", we skip banner mode and just show a small
            // indicator in the panel; however do make an exception for CRITICAL
            // notifications, as only banner mode allows expansion.
            let bannerCount = this._notification ? 1 : 0;
            let full = (this.queueCount + bannerCount >= MAX_NOTIFICATIONS_IN_QUEUE);
            if (!full || notification.urgency == Urgency.CRITICAL) {
                notification.connect('destroy',
                                     this._onNotificationDestroy.bind(this));
                this._notificationQueue.push(notification);
                this._notificationQueue.sort(
                    (n1, n2) => n2.urgency - n1.urgency
                );
                this.emit('queue-changed');
            }
        }
        this._updateState();
    },

    _resetNotificationLeftTimeout() {
        this._useLongerNotificationLeftTimeout = false;
        if (this._notificationLeftTimeoutId) {
            Mainloop.source_remove(this._notificationLeftTimeoutId);
            this._notificationLeftTimeoutId = 0;
            this._notificationLeftMouseX = -1;
            this._notificationLeftMouseY = -1;
        }
    },

    _onNotificationHoverChanged() {
        if (this._bannerBin.hover == this._notificationHovered)
            return;

        this._notificationHovered = this._bannerBin.hover;
        if (this._notificationHovered) {
            this._resetNotificationLeftTimeout();

            if (this._showNotificationMouseX >= 0) {
                let actorAtShowNotificationPosition =
                    global.stage.get_actor_at_pos(Clutter.PickMode.ALL, this._showNotificationMouseX, this._showNotificationMouseY);
                this._showNotificationMouseX = -1;
                this._showNotificationMouseY = -1;
                // Don't set this._pointerInNotification to true if the pointer was initially in the area where the notification
                // popped up. That way we will not be expanding notifications that happen to pop up over the pointer
                // automatically. Instead, the user is able to expand the notification by mousing away from it and then
                // mousing back in. Because this is an expected action, we set the boolean flag that indicates that a longer
                // timeout should be used before popping down the notification.
                if (this._bannerBin.contains(actorAtShowNotificationPosition)) {
                    this._useLongerNotificationLeftTimeout = true;
                    return;
                }
            }

            this._pointerInNotification = true;
            this._updateState();
        } else {
            // We record the position of the mouse the moment it leaves the tray. These coordinates are used in
            // this._onNotificationLeftTimeout() to determine if the mouse has moved far enough during the initial timeout for us
            // to consider that the user intended to leave the tray and therefore hide the tray. If the mouse is still
            // close to its previous position, we extend the timeout once.
            let [x, y, mods] = global.get_pointer();
            this._notificationLeftMouseX = x;
            this._notificationLeftMouseY = y;

            // We wait just a little before hiding the message tray in case the user quickly moves the mouse back into it.
            // We wait for a longer period if the notification popped up where the mouse pointer was already positioned.
            // That gives the user more time to mouse away from the notification and mouse back in in order to expand it.
            let timeout = this._useLongerNotificationLeftTimeout ? LONGER_HIDE_TIMEOUT * 1000 : HIDE_TIMEOUT * 1000;
            this._notificationLeftTimeoutId = Mainloop.timeout_add(timeout, this._onNotificationLeftTimeout.bind(this));
            GLib.Source.set_name_by_id(this._notificationLeftTimeoutId, '[gnome-shell] this._onNotificationLeftTimeout');
        }
    },

    _onStatusChanged(status) {
        if (status == GnomeSession.PresenceStatus.BUSY) {
            // remove notification and allow the summary to be closed now
            this._updateNotificationTimeout(0);
            this._busy = true;
        } else if (status != GnomeSession.PresenceStatus.IDLE) {
            // We preserve the previous value of this._busy if the status turns to IDLE
            // so that we don't start showing notifications queued during the BUSY state
            // as the screensaver gets activated.
            this._busy = false;
        }

        this._updateState();
    },

    _onNotificationLeftTimeout() {
        let [x, y, mods] = global.get_pointer();
        // We extend the timeout once if the mouse moved no further than MOUSE_LEFT_ACTOR_THRESHOLD to either side.
        if (this._notificationLeftMouseX > -1 &&
            y < this._notificationLeftMouseY + MOUSE_LEFT_ACTOR_THRESHOLD &&
            y > this._notificationLeftMouseY - MOUSE_LEFT_ACTOR_THRESHOLD &&
            x < this._notificationLeftMouseX + MOUSE_LEFT_ACTOR_THRESHOLD &&
            x > this._notificationLeftMouseX - MOUSE_LEFT_ACTOR_THRESHOLD) {
            this._notificationLeftMouseX = -1;
            this._notificationLeftTimeoutId = Mainloop.timeout_add(LONGER_HIDE_TIMEOUT * 1000,
                                                             this._onNotificationLeftTimeout.bind(this));
            GLib.Source.set_name_by_id(this._notificationLeftTimeoutId, '[gnome-shell] this._onNotificationLeftTimeout');
        } else {
            this._notificationLeftTimeoutId = 0;
            this._useLongerNotificationLeftTimeout = false;
            this._pointerInNotification = false;
            this._updateNotificationTimeout(0);
            this._updateState();
        }
        return GLib.SOURCE_REMOVE;
    },

    _escapeTray() {
        this._pointerInNotification = false;
        this._updateNotificationTimeout(0);
        this._updateState();
    },

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // 'this._pointerInNotification', 'this._traySummoned', etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState() {
        let hasMonitor = Main.layoutManager.primaryMonitor != null;
        this.actor.visible = !this._bannerBlocked && hasMonitor && this._banner != null;
        if (this._bannerBlocked || !hasMonitor)
            return;

        // If our state changes caused _updateState to be called,
        // just exit now to prevent reentrancy issues.
        if (this._updatingState)
            return;

        this._updatingState = true;

        // Filter out acknowledged notifications.
        let changed = false;
        this._notificationQueue = this._notificationQueue.filter(n => {
            changed = changed || n.acknowledged;
            return !n.acknowledged;
        });

        if (changed)
            this.emit('queue-changed');

        let hasNotifications = Main.sessionMode.hasNotifications;

        if (this._notificationState == State.HIDDEN) {
            let nextNotification = this._notificationQueue[0] || null;
            if (hasNotifications && nextNotification) {
                let limited = this._busy || Main.layoutManager.primaryMonitor.inFullscreen;
                let showNextNotification = (!limited || nextNotification.forFeedback || nextNotification.urgency == Urgency.CRITICAL);
                if (showNextNotification)
                    this._showNotification();
            }
        } else if (this._notificationState == State.SHOWN) {
            let expired = (this._userActiveWhileNotificationShown &&
                           this._notificationTimeoutId == 0 &&
                           this._notification.urgency != Urgency.CRITICAL &&
                           !this._banner.focused &&
                           !this._pointerInNotification) || this._notificationExpired;
            let mustClose = (this._notificationRemoved || !hasNotifications || expired);

            if (mustClose) {
                let animate = hasNotifications && !this._notificationRemoved;
                this._hideNotification(animate);
            } else if (this._pointerInNotification && !this._banner.expanded) {
                this._expandBanner(false);
            } else if (this._pointerInNotification) {
                this._ensureBannerFocused();
            }
        }

        this._updatingState = false;

        // Clean transient variables that are used to communicate actions
        // to updateState()
        this._notificationExpired = false;
    },

    _tween(actor, statevar, value, params) {
        let onComplete = params.onComplete;
        let onCompleteScope = params.onCompleteScope;
        let onCompleteParams = params.onCompleteParams;

        params.onComplete = this._tweenComplete;
        params.onCompleteScope = this;
        params.onCompleteParams = [statevar, value, onComplete, onCompleteScope, onCompleteParams];

        // Remove other tweens that could mess with the state machine
        Tweener.removeTweens(actor);
        Tweener.addTween(actor, params);

        let valuing = (value == State.SHOWN) ? State.SHOWING : State.HIDING;
        this[statevar] = valuing;
    },

    _tweenComplete(statevar, value, onComplete, onCompleteScope, onCompleteParams) {
        this[statevar] = value;
        if (onComplete)
            onComplete.apply(onCompleteScope, onCompleteParams);
        this._updateState();
    },

    _clampOpacity() {
        this._bannerBin.opacity = Math.max(0, Math.min(this._bannerBin._opacity, 255));
    },

    _onIdleMonitorBecameActive() {
        this._userActiveWhileNotificationShown = true;
        this._updateNotificationTimeout(2000);
        this._updateState();
    },

    _showNotification() {
        this._notification = this._notificationQueue.shift();
        this.emit('queue-changed');

        this._userActiveWhileNotificationShown = this.idleMonitor.get_idletime() <= IDLE_TIME;
        if (!this._userActiveWhileNotificationShown) {
            // If the user isn't active, set up a watch to let us know
            // when the user becomes active.
            this.idleMonitor.add_user_active_watch(this._onIdleMonitorBecameActive.bind(this));
        }

        this._banner = this._notification.createBanner();
        this._bannerClickedId = this._banner.connect('done-displaying', () => {
            Meta.enable_unredirect_for_display(global.display);
            this._escapeTray();
        });
        this._bannerUnfocusedId = this._banner.connect('unfocused', () => {
            this._updateState();
        });

        this._bannerBin.add_actor(this._banner.actor);

        this._bannerBin._opacity = 0;
        this._bannerBin.opacity = 0;
        this._bannerBin.y = -this._banner.actor.height;
        this.actor.show();

        Meta.disable_unredirect_for_display(global.display);
        this._updateShowingNotification();

        let [x, y, mods] = global.get_pointer();
        // We save the position of the mouse at the time when we started showing the notification
        // in order to determine if the notification popped up under it. We make that check if
        // the user starts moving the mouse and _onNotificationHoverChanged() gets called. We don't
        // expand the notification if it just happened to pop up under the mouse unless the user
        // explicitly mouses away from it and then mouses back in.
        this._showNotificationMouseX = x;
        this._showNotificationMouseY = y;
        // We save the coordinates of the mouse at the time when we started showing the notification
        // and then we update it in _notificationTimeout(). We don't pop down the notification if
        // the mouse is moving towards it or within it.
        this._lastSeenMouseX = x;
        this._lastSeenMouseY = y;

        this._resetNotificationLeftTimeout();
    },

    _updateShowingNotification() {
        this._notification.acknowledged = true;
        this._notification.playSound();

        // We auto-expand notifications with CRITICAL urgency, or for which the relevant setting
        // is on in the control center.
        if (this._notification.urgency == Urgency.CRITICAL ||
            this._notification.source.policy.forceExpanded)
            this._expandBanner(true);

        // We tween all notifications to full opacity. This ensures that both new notifications and
        // notifications that might have been in the process of hiding get full opacity.
        //
        // We tween any notification showing in the banner mode to the appropriate height
        // (which is banner height or expanded height, depending on the notification state)
        // This ensures that both new notifications and notifications in the banner mode that might
        // have been in the process of hiding are shown with the correct height.
        //
        // We use this._showNotificationCompleted() onComplete callback to extend the time the updated
        // notification is being shown.

        let tweenParams = { y: 0,
                            _opacity: 255,
                            time: ANIMATION_TIME,
                            transition: 'easeOutBack',
                            onUpdate: this._clampOpacity,
                            onUpdateScope: this,
                            onComplete: this._showNotificationCompleted,
                            onCompleteScope: this
                          };

        this._tween(this._bannerBin, '_notificationState', State.SHOWN, tweenParams);
   },

    _showNotificationCompleted() {
        if (this._notification.urgency != Urgency.CRITICAL)
            this._updateNotificationTimeout(NOTIFICATION_TIMEOUT * 1000);
    },

    _updateNotificationTimeout(timeout) {
        if (this._notificationTimeoutId) {
            Mainloop.source_remove(this._notificationTimeoutId);
            this._notificationTimeoutId = 0;
        }
        if (timeout > 0) {
            this._notificationTimeoutId =
                Mainloop.timeout_add(timeout,
                                     this._notificationTimeout.bind(this));
            GLib.Source.set_name_by_id(this._notificationTimeoutId, '[gnome-shell] this._notificationTimeout');
        }
    },

    _notificationTimeout() {
        let [x, y, mods] = global.get_pointer();
        if (y < this._lastSeenMouseY - 10 && !this._notificationHovered) {
            // The mouse is moving towards the notification, so don't
            // hide it yet. (We just create a new timeout (and destroy
            // the old one) each time because the bookkeeping is
            // simpler.)
            this._updateNotificationTimeout(1000);
        } else if (this._useLongerNotificationLeftTimeout && !this._notificationLeftTimeoutId &&
                  (x != this._lastSeenMouseX || y != this._lastSeenMouseY)) {
            // Refresh the timeout if the notification originally
            // popped up under the pointer, and the pointer is hovering
            // inside it.
            this._updateNotificationTimeout(1000);
        } else {
            this._notificationTimeoutId = 0;
            this._updateState();
        }

        this._lastSeenMouseX = x;
        this._lastSeenMouseY = y;
        return GLib.SOURCE_REMOVE;
    },

    _hideNotification(animate) {
        this._notificationFocusGrabber.ungrabFocus();

        if (this._bannerClickedId) {
            this._banner.disconnect(this._bannerClickedId);
            this._bannerClickedId = 0;
        }
        if (this._bannerUnfocusedId) {
            this._banner.disconnect(this._bannerUnfocusedId);
            this._bannerUnfocusedId = 0;
        }

        this._resetNotificationLeftTimeout();

        if (animate) {
            this._tween(this._bannerBin, '_notificationState', State.HIDDEN,
                        { y: -this._bannerBin.height,
                          _opacity: 0,
                          time: ANIMATION_TIME,
                          transition: 'easeOutBack',
                          onUpdate: this._clampOpacity,
                          onUpdateScope: this,
                          onComplete: this._hideNotificationCompleted,
                          onCompleteScope: this
                        });
        } else {
            Tweener.removeTweens(this._bannerBin);
            this._bannerBin.y = -this._bannerBin.height;
            this._bannerBin.opacity = 0;
            this._notificationState = State.HIDDEN;
            this._hideNotificationCompleted();
        }
    },

    _hideNotificationCompleted() {
        let notification = this._notification;
        this._notification = null;
        if (notification.isTransient)
            notification.destroy(NotificationDestroyedReason.EXPIRED);

        this._pointerInNotification = false;
        this._notificationRemoved = false;

        this._banner.actor.destroy();
        this._banner = null;
        this.actor.hide();
    },

    _expandActiveNotification() {
        if (!this._banner)
            return;

        this._expandBanner(false);
    },

    _expandBanner(autoExpanding) {
        // Don't animate changes in notifications that are auto-expanding.
        this._banner.expand(!autoExpanding);

        // Don't focus notifications that are auto-expanding.
        if (!autoExpanding)
            this._ensureBannerFocused();
    },

    _ensureBannerFocused() {
        this._notificationFocusGrabber.grabFocus();
    }
});
Signals.addSignalMethods(MessageTray.prototype);

var SystemNotificationSource = new Lang.Class({
    Name: 'SystemNotificationSource',
    Extends: Source,

    _init() {
        this.parent(_("System Information"), 'dialog-information-symbolic');
    },

    open() {
        this.destroy();
    }
});
