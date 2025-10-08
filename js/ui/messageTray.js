import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as GnomeSession from '../misc/gnomeSession.js';
import * as Layout from './layout.js';
import * as Main from './main.js';
import * as MessageList from './messageList.js';
import * as SignalTracker from '../misc/signalTracker.js';

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';

export const ANIMATION_TIME = 200;

const NOTIFICATION_TIMEOUT = 4000;

const HIDE_TIMEOUT = 200;
const LONGER_HIDE_TIMEOUT = 600;

const MAX_NOTIFICATIONS_IN_QUEUE = 3;
const MAX_NOTIFICATIONS_PER_SOURCE = 10;

// We delay hiding of the tray if the mouse is within MOUSE_LEFT_ACTOR_THRESHOLD
// range from the point where it left the tray.
const MOUSE_LEFT_ACTOR_THRESHOLD = 20;

const IDLE_TIME = 1000;

export const State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3,
};

// These reasons are useful when we destroy the notifications received through
// the notification daemon. We use EXPIRED for notifications that we dismiss
// and the user did not interact with, DISMISSED for all other notifications
// that were destroyed as a result of a user action, SOURCE_CLOSED for the
// notifications that were requested to be destroyed by the associated source,
// and REPLACED for notifications that were destroyed as a consequence of a
// newer version having replaced them.
/** @enum {number} */
export const NotificationDestroyedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    SOURCE_CLOSED: 3,
    REPLACED: 4,
};

// Message tray has its custom Urgency enumeration. LOW, NORMAL and CRITICAL
// urgency values map to the corresponding values for the notifications received
// through the notification daemon.
/** @enum {number} */
export const Urgency = {
    LOW: 0,
    NORMAL: 1,
    HIGH: 2,
    CRITICAL: 3,
};

// The privacy of the details of a notification. USER is for notifications which
// contain private information to the originating user account (for example,
// details of an e-mail they’ve received). SYSTEM is for notifications which
// contain information private to the physical system (for example, battery
// status) and hence the same for every user. This affects whether the content
// of a notification is shown on the lock screen.
/** @enum {number} */
export const PrivacyScope = {
    USER: 0,
    SYSTEM: 1,
};

class FocusGrabber {
    constructor(actor) {
        this._actor = actor;
        this._prevKeyFocusActor = null;
        this._focused = false;
    }

    grabFocus() {
        if (this._focused)
            return;

        this._prevKeyFocusActor = global.stage.get_key_focus();

        global.stage.connectObject('notify::key-focus',
            this._focusActorChanged.bind(this), this);

        if (!this._actor.navigate_focus(null, St.DirectionType.TAB_FORWARD, false))
            this._actor.grab_key_focus();

        this._focused = true;
    }

    _focusUngrabbed() {
        if (!this._focused)
            return false;

        global.stage.disconnectObject(this);

        this._focused = false;
        return true;
    }

    _focusActorChanged() {
        let focusedActor = global.stage.get_key_focus();
        if (!focusedActor || !this._actor.contains(focusedActor))
            this._focusUngrabbed();
    }

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
}

// NotificationPolicy:
// An object that holds all bits of configurable policy related to a notification
// source, such as whether to play sound or honour the critical bit.
//
// A notification without a policy object will inherit the default one.
export const NotificationPolicy = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
    Properties: {
        'enable': GObject.ParamSpec.boolean(
            'enable', null, null, GObject.ParamFlags.READABLE, true),
        'enable-sound': GObject.ParamSpec.boolean(
            'enable-sound', null, null,
            GObject.ParamFlags.READABLE, true),
        'show-banners': GObject.ParamSpec.boolean(
            'show-banners', null, null,
            GObject.ParamFlags.READABLE, true),
        'force-expanded': GObject.ParamSpec.boolean(
            'force-expanded', null, null,
            GObject.ParamFlags.READABLE, false),
        'show-in-lock-screen': GObject.ParamSpec.boolean(
            'show-in-lock-screen', null, null,
            GObject.ParamFlags.READABLE, false),
        'details-in-lock-screen': GObject.ParamSpec.boolean(
            'details-in-lock-screen', null, null,
            GObject.ParamFlags.READABLE, false),
    },
}, class NotificationPolicy extends GObject.Object {
    /**
     * Create a new policy for app.
     *
     * This will be a NotificationApplicationPolicy for valid apps,
     * or a NotificationGenericPolicy otherwise.
     *
     * @param {Shell.App=} app
     * @returns {NotificationPolicy}
     */
    static newForApp(app) {
        // fallback to generic policy
        if (!app?.get_app_info())
            return new NotificationGenericPolicy();

        const id = app.get_id().replace(/\.desktop$/, '');
        return new NotificationApplicationPolicy(id);
    }

    // Do nothing for the default policy. These methods are only useful for the
    // GSettings policy.
    store() { }

    destroy() {
        this.run_dispose();
    }

    get enable() {
        return true;
    }

    get enableSound() {
        return true;
    }

    get showBanners() {
        return true;
    }

    get forceExpanded() {
        return false;
    }

    get showInLockScreen() {
        return false;
    }

    get detailsInLockScreen() {
        return false;
    }
});

export const NotificationGenericPolicy = GObject.registerClass({
}, class NotificationGenericPolicy extends NotificationPolicy {
    _init() {
        super._init();
        this.id = 'generic';

        this._masterSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.notifications'});
        this._masterSettings.connect('changed', this._changed.bind(this));
    }

    destroy() {
        this._masterSettings.run_dispose();

        super.destroy();
    }

    _changed(settings, key) {
        if (this.constructor.find_property(key))
            this.notify(key);
    }

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners');
    }

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen');
    }
});

export const NotificationApplicationPolicy = GObject.registerClass({
}, class NotificationApplicationPolicy extends NotificationPolicy {
    _init(id) {
        super._init();

        this.id = id;
        this._canonicalId = this._canonicalizeId(id);

        this._masterSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.notifications'});
        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.notifications.application',
            path: `/org/gnome/desktop/notifications/application/${this._canonicalId}/`,
        });

        this._masterSettings.connect('changed', this._changed.bind(this));
        this._settings.connect('changed', this._changed.bind(this));
    }

    store() {
        this._settings.set_string('application-id', `${this.id}.desktop`);

        let apps = this._masterSettings.get_strv('application-children');
        if (!apps.includes(this._canonicalId)) {
            apps.push(this._canonicalId);
            this._masterSettings.set_strv('application-children', apps);
        }
    }

    destroy() {
        this._masterSettings.run_dispose();
        this._settings.run_dispose();

        super.destroy();
    }

    _changed(settings, key) {
        if (this.constructor.find_property(key))
            this.notify(key);
    }

    _canonicalizeId(id) {
        // Keys are restricted to lowercase alphanumeric characters and dash,
        // and two dashes cannot be in succession
        return id.toLowerCase().replace(/[^a-z0-9-]/g, '-').replace(/--+/g, '-');
    }

    get enable() {
        return this._settings.get_boolean('enable');
    }

    get enableSound() {
        return this._settings.get_boolean('enable-sound-alerts');
    }

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners') &&
            this._settings.get_boolean('show-banners');
    }

    get forceExpanded() {
        return this._settings.get_boolean('force-expanded');
    }

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen') &&
            this._settings.get_boolean('show-in-lock-screen');
    }

    get detailsInLockScreen() {
        return this._settings.get_boolean('details-in-lock-screen');
    }
});

export const Sound = GObject.registerClass(
class Sound extends GObject.Object {
    constructor(file, themedName) {
        super();

        this._soundFile = file;
        this._soundName = themedName;
    }

    play() {
        const player = global.display.get_sound_player();

        if (this._soundName)
            player.play_from_theme(this._soundName, _('Notification sound'), null);
        else if (this._soundFile)
            player.play_from_file(this._soundFile, _('Notification sound'), null);
    }
});

export const Action = GObject.registerClass(
class Action extends GObject.Object {
    constructor(label, callback) {
        super();

        this._label = label;
        this._callback = callback;
    }

    get label() {
        return this._label;
    }

    activate() {
        this._callback();
    }
});

export class Notification extends GObject.Object {
    constructor(params) {
        super(params);

        this._actions = [];

        if (!this.datetime)
            this.datetime = GLib.DateTime.new_now_local();

        // Automatically update the datetime property when the notification
        // is updated.
        this.connect('notify', (o, pspec) => {
            if (pspec.name === 'acknowledged') {
                // Don't update datetime property
            } else if (pspec.name === 'datetime') {
                if (this._updateDatetimeId)
                    GLib.source_remove(this._updateDatetimeId);
                delete this._updateDatetimeId;
            } else if (!this._updateDatetimeId) {
                this._updateDatetimeId =
                    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                        delete this._updateDatetimeId;
                        this.datetime = GLib.DateTime.new_now_local();
                        return GLib.SOURCE_REMOVE;
                    });
            }
        });
    }

    get actions() {
        return this._actions;
    }

    get iconName() {
        if (this.gicon instanceof Gio.ThemedIcon)
            return this.gicon.iconName;
        else
            return null;
    }

    set iconName(iconName) {
        this.gicon = new Gio.ThemedIcon({name: iconName});
    }

    get privacyScope() {
        return this._privacyScope;
    }

    set privacyScope(privacyScope) {
        if (!Object.values(PrivacyScope).includes(privacyScope))
            throw new Error('out of range');

        if (this._privacyScope === privacyScope)
            return;

        this._privacyScope = privacyScope;
        this.notify('privacy-scope');
    }

    get urgency() {
        return this._urgency;
    }

    set urgency(urgency) {
        if (!Object.values(Urgency).includes(urgency))
            throw new Error('out of range');

        if (this._urgency === urgency)
            return;

        this._urgency = urgency;
        this.notify('urgency');
    }

    // addAction:
    // @label: the label for the action's button
    // @callback: the callback for the action
    addAction(label, callback) {
        const action = new Action(label, () => {
            callback();

            // We don't hide a resident notification when the user invokes one of its actions,
            // because it is common for such notifications to update themselves with new
            // information based on the action. We'd like to display the updated information
            // in place, rather than pop-up a new notification.
            if (this.resident)
                return;

            this.destroy();
        });
        this._actions.push(action);
        this.emit('action-added', action);
        return action;
    }

    removeAction(action) {
        const index = this._actions.indexOf(action);
        if (index < 0)
            throw new Error('Action was already removed previously');

        this._actions.splice(index, 1);
        this.emit('action-removed', action);
    }

    clearActions() {
        if (this._actions.length === 0)
            return;

        this._actions.forEach(action => {
            this.emit('action-removed', action);
        });
        this._actions = [];
    }

    playSound() {
        if (!this.source.policy.enableSound)
            return;

        this.sound?.play(this.title);
    }

    activate() {
        this.emit('activated');

        // We don't hide a resident notification when the user invokes one of its actions,
        // because it is common for such notifications to update themselves with new
        // information based on the action. We'd like to display the updated information
        // in place, rather than pop-up a new notification.
        if (this.resident)
            return;

        this.destroy();
    }

    destroy(reason = NotificationDestroyedReason.DISMISSED) {
        this.emit('destroy', reason);

        if (this._updateDatetimeId)
            GLib.source_remove(this._updateDatetimeId);
        delete this._updateDatetimeId;

        this.run_dispose();
    }
}

export const Source = GObject.registerClass({
    Properties: {
        'count': GObject.ParamSpec.int(
            'count', null, null,
            GObject.ParamFlags.READABLE,
            0, GLib.MAXINT32, 0),
        'policy': GObject.ParamSpec.object(
            'policy', null, null,
            GObject.ParamFlags.READWRITE,
            NotificationPolicy.$gtype),
    },
    Signals: {
        'destroy': {param_types: [GObject.TYPE_UINT]},
        'notification-added': {param_types: [Notification.$gtype]},
        'notification-removed': {param_types: [Notification.$gtype]},
        'notification-request-banner': {param_types: [Notification.$gtype]},
    },
}, class Source extends MessageList.Source {
    constructor(params) {
        super(params);

        this.notifications = [];

        if (!this._policy)
            this._policy = new NotificationGenericPolicy();
    }

    get policy() {
        return this._policy;
    }

    set policy(policy) {
        if (this._policy)
            this._policy.destroy();
        this._policy = policy;
    }

    get count() {
        return this.notifications.length;
    }

    get unseenCount() {
        return this.notifications.filter(n => !n.acknowledged).length;
    }

    get countVisible() {
        return this.count > 1;
    }

    countUpdated() {
        this.notify('count');
    }

    get narrowestPrivacyScope() {
        return this.notifications.every(n => n.privacyScope === PrivacyScope.SYSTEM)
            ? PrivacyScope.SYSTEM
            : PrivacyScope.USER;
    }

    _onNotificationDestroy(notification) {
        let index = this.notifications.indexOf(notification);
        if (index < 0)
            throw new Error('Notification was already removed previously');

        this.notifications.splice(index, 1);
        this.emit('notification-removed', notification);
        this.countUpdated();

        if (!this._inDestruction && this.notifications.length === 0)
            this.destroy();
    }

    addNotification(notification) {
        if (this.notifications.includes(notification))
            return;

        while (this.notifications.length >= MAX_NOTIFICATIONS_PER_SOURCE) {
            const [oldest] = this.notifications;
            oldest.destroy(NotificationDestroyedReason.EXPIRED);
        }

        notification.connect('destroy', this._onNotificationDestroy.bind(this));
        notification.connect('notify::acknowledged', () => {
            this.countUpdated();

            // If acknowledged was set to false try to show the notification again
            if (!notification.acknowledged)
                this.emit('notification-request-banner', notification);
        });
        this.notifications.push(notification);

        this.emit('notification-added', notification);
        this.emit('notification-request-banner', notification);
        this.countUpdated();
    }

    destroy(reason) {
        this._inDestruction = true;

        while (this.notifications.length > 0) {
            const [oldest] = this.notifications;
            oldest.destroy(reason);
        }

        this.emit('destroy', reason);

        this.policy.destroy();
        this.run_dispose();
    }

    // To be overridden by subclasses
    open() {
    }

    destroyNonResidentNotifications() {
        for (let i = this.notifications.length - 1; i >= 0; i--) {
            if (!this.notifications[i].resident)
                this.notifications[i].destroy();
        }
    }
});
SignalTracker.registerDestroyableType(Source);

GObject.registerClass({
    Properties: {
        'source': GObject.ParamSpec.object(
            'source', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            Source),
        'title': GObject.ParamSpec.string(
            'title', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'body': GObject.ParamSpec.string(
            'body', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'use-body-markup': GObject.ParamSpec.boolean(
            'use-body-markup', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'gicon': GObject.ParamSpec.object(
            'gicon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'icon-name': GObject.ParamSpec.string(
            'icon-name', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'sound': GObject.ParamSpec.object(
            'sound', null, null,
            GObject.ParamFlags.READWRITE,
            Sound),
        'datetime': GObject.ParamSpec.boxed(
            'datetime', null, null,
            GObject.ParamFlags.READWRITE,
            GLib.DateTime),
        // Unfortunately we can't register new enum types in GJS
        // See: https://gitlab.gnome.org/GNOME/gjs/-/issues/573
        'privacy-scope': GObject.ParamSpec.int(
            'privacy-scope', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT,
            0, GLib.MAXINT32,
            PrivacyScope.User),
        'urgency': GObject.ParamSpec.int(
            'urgency', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT,
            0, GLib.MAXINT32,
            Urgency.NORMAL),
        'acknowledged': GObject.ParamSpec.boolean(
            'acknowledged', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'resident': GObject.ParamSpec.boolean(
            'resident', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'for-feedback': GObject.ParamSpec.boolean(
            'for-feedback', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'is-transient': GObject.ParamSpec.boolean(
            'is-transient', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    },
    Signals: {
        'action-added': {param_types: [Action]},
        'action-removed': {param_types: [Action]},
        'activated': {},
        'destroy': {param_types: [GObject.TYPE_UINT]},
    },
}, Notification);
SignalTracker.registerDestroyableType(Notification);

export const MessageTray = GObject.registerClass({
    Signals: {
        'queue-changed': {},
        'source-added': {param_types: [Source.$gtype]},
        'source-removed': {param_types: [Source.$gtype]},
    },
}, class MessageTray extends St.Widget {
    _init() {
        super._init({
            visible: false,
            clip_to_allocation: true,
            layout_manager: new Clutter.BinLayout(),
        });

        this._presence = new GnomeSession.Presence((proxy, _error) => {
            this._onStatusChanged(proxy.status);
        });
        this._busy = false;
        this._bannerBlocked = false;
        this._presence.connectSignal('StatusChanged', (proxy, senderName, [status]) => {
            this._onStatusChanged(status);
        });

        let constraint = new Layout.MonitorConstraint({primary: true});
        Main.layoutManager.panelBox.bind_property('visible',
            constraint, 'work-area',
            GObject.BindingFlags.SYNC_CREATE);
        this.add_constraint(constraint);

        this._bannerBin = new St.Widget({
            name: 'notification-container',
            reactive: true,
            track_hover: true,
            y_align: Clutter.ActorAlign.START,
            x_align: Clutter.ActorAlign.CENTER,
            y_expand: true,
            x_expand: true,
            layout_manager: new Clutter.BinLayout(),
        });
        this._bannerBin.connect('key-release-event',
            this._onNotificationKeyRelease.bind(this));
        this._bannerBin.connect('notify::hover',
            this._onNotificationHoverChanged.bind(this));
        this.add_child(this._bannerBin);

        this._notificationFocusGrabber = new FocusGrabber(this._bannerBin);
        this._notificationQueue = [];
        this._notification = null;
        this._banner = null;

        this._userActiveWhileNotificationShown = false;

        this.idleMonitor = global.backend.get_core_idle_monitor();

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

        Main.layoutManager.addChrome(this, {affectsInputRegion: false});
        Main.layoutManager.trackChrome(this._bannerBin, {affectsInputRegion: true});

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
            new Gio.Settings({schema_id: SHELL_KEYBINDINGS_SCHEMA}),
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
            this._expandActiveNotification.bind(this));

        this._sources = new Set();

        this._sessionUpdated();
    }

    _sessionUpdated() {
        this._updateState();
    }

    _onDragBegin() {
        Shell.util_set_hidden_from_pick(this, true);
    }

    _onDragEnd() {
        Shell.util_set_hidden_from_pick(this, false);
    }

    get bannerAlignment() {
        return this._bannerBin.get_x_align();
    }

    set bannerAlignment(align) {
        this._bannerBin.set_x_align(align);
    }

    _onNotificationKeyRelease(actor, event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape && event.get_state() === 0) {
            this._expireNotification();
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _expireNotification() {
        this._notificationExpired = true;
        this._updateState();
    }

    get queueCount() {
        return this._notificationQueue.length;
    }

    set bannerBlocked(v) {
        if (this._bannerBlocked === v)
            return;
        this._bannerBlocked = v;
        this._updateState();
    }

    contains(source) {
        return this._sources.has(source);
    }

    add(source) {
        if (this.contains(source)) {
            log(`Trying to re-add source ${source.title}`);
            return;
        }

        // Register that we got a notification for this source
        source.policy.store();

        source.policy.connect('notify::enable', () => {
            this._onSourceEnableChanged(source.policy, source);
        });
        source.policy.connect('notify', this._updateState.bind(this));
        this._onSourceEnableChanged(source.policy, source);
    }

    _addSource(source) {
        this._sources.add(source);

        source.connectObject(
            'notification-request-banner', this._onNotificationRequestBanner.bind(this),
            'notification-removed', this._onNotificationRemoved.bind(this),
            'destroy', () => this._removeSource(source), this);

        this.emit('source-added', source);
    }

    _removeSource(source) {
        this._sources.delete(source);
        source.disconnectObject(this);
        this.emit('source-removed', source);
    }

    getSources() {
        return [...this._sources.keys()];
    }

    _onSourceEnableChanged(policy, source) {
        let wasEnabled = this.contains(source);
        let shouldBeEnabled = policy.enable;

        if (wasEnabled !== shouldBeEnabled) {
            if (shouldBeEnabled)
                this._addSource(source);
            else
                this._removeSource(source);
        }
    }

    _onNotificationRemoved(source, notification) {
        if (this._notification === notification) {
            this._notificationRemoved = true;
            if (this._notificationState === State.SHOWN ||
                this._notificationState === State.SHOWING) {
                this._pointerInNotification = false;
                this._updateNotificationTimeout(0);
                this._updateState();
            }
        } else {
            const index = this._notificationQueue.indexOf(notification);
            if (index !== -1) {
                this._notificationQueue.splice(index, 1);
                this.emit('queue-changed');
            }
        }
    }

    _onNotificationRequestBanner(_source, notification) {
        // We never display a banner for already acknowledged notifications
        if (notification.acknowledged)
            return;

        if (notification.urgency === Urgency.LOW)
            return;

        if (!notification.source.policy.showBanners && notification.urgency !== Urgency.CRITICAL)
            return;

        if (this._notification === notification) {
            // If a notification that is being shown is updated, we update
            // how it is shown and extend the time until it auto-hides.
            // If a new notification is updated while it is being hidden,
            // we stop hiding it and show it again.
            this._updateShowingNotification();
        } else if (!this._notificationQueue.includes(notification)) {
            // If the queue is "full", we skip banner mode and just show a small
            // indicator in the panel; however do make an exception for CRITICAL
            // notifications, as only banner mode allows expansion.
            let bannerCount = this._notification ? 1 : 0;
            let full = this.queueCount + bannerCount >= MAX_NOTIFICATIONS_IN_QUEUE;
            if (!full || notification.urgency === Urgency.CRITICAL) {
                this._notificationQueue.push(notification);
                this._notificationQueue.sort(
                    (n1, n2) => n2.urgency - n1.urgency);
                this.emit('queue-changed');
            }
        }
        this._updateState();
    }

    _resetNotificationLeftTimeout() {
        this._useLongerNotificationLeftTimeout = false;
        if (this._notificationLeftTimeoutId) {
            GLib.source_remove(this._notificationLeftTimeoutId);
            this._notificationLeftTimeoutId = 0;
            this._notificationLeftMouseX = -1;
            this._notificationLeftMouseY = -1;
        }
    }

    _onNotificationHoverChanged() {
        if (this._bannerBin.hover === this._notificationHovered)
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
            let [x, y] = global.get_pointer();
            this._notificationLeftMouseX = x;
            this._notificationLeftMouseY = y;

            // We wait just a little before hiding the message tray in case the user quickly moves the mouse back into it.
            // We wait for a longer period if the notification popped up where the mouse pointer was already positioned.
            // That gives the user more time to mouse away from the notification and mouse back in in order to expand it.
            let timeout = this._useLongerNotificationLeftTimeout ? LONGER_HIDE_TIMEOUT : HIDE_TIMEOUT;
            this._notificationLeftTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, timeout, this._onNotificationLeftTimeout.bind(this));
            GLib.Source.set_name_by_id(this._notificationLeftTimeoutId, '[gnome-shell] this._onNotificationLeftTimeout');
        }
    }

    _onStatusChanged(status) {
        if (status === GnomeSession.PresenceStatus.BUSY) {
            // remove notification and allow the summary to be closed now
            this._updateNotificationTimeout(0);
            this._busy = true;
        } else if (status !== GnomeSession.PresenceStatus.IDLE) {
            // We preserve the previous value of this._busy if the status turns to IDLE
            // so that we don't start showing notifications queued during the BUSY state
            // as the screensaver gets activated.
            this._busy = false;
        }

        this._updateState();
    }

    _onNotificationLeftTimeout() {
        let [x, y] = global.get_pointer();
        // We extend the timeout once if the mouse moved no further than MOUSE_LEFT_ACTOR_THRESHOLD to either side.
        if (this._notificationLeftMouseX > -1 &&
            y < this._notificationLeftMouseY + MOUSE_LEFT_ACTOR_THRESHOLD &&
            y > this._notificationLeftMouseY - MOUSE_LEFT_ACTOR_THRESHOLD &&
            x < this._notificationLeftMouseX + MOUSE_LEFT_ACTOR_THRESHOLD &&
            x > this._notificationLeftMouseX - MOUSE_LEFT_ACTOR_THRESHOLD) {
            this._notificationLeftMouseX = -1;
            this._notificationLeftTimeoutId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                LONGER_HIDE_TIMEOUT,
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
    }

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // 'this._pointerInNotification', 'this._traySummoned', etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState() {
        let hasMonitor = Main.layoutManager.primaryMonitor != null;
        this.visible = !this._bannerBlocked && hasMonitor && this._banner != null;
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
            changed ||= n.acknowledged;
            return !n.acknowledged;
        });

        if (changed)
            this.emit('queue-changed');

        let hasNotifications = Main.sessionMode.hasNotifications;

        if (this._notificationState === State.HIDDEN) {
            let nextNotification = this._notificationQueue[0] || null;
            if (hasNotifications && nextNotification) {
                let limited = this._busy || Main.layoutManager.primaryMonitor.inFullscreen;
                let showNextNotification = !limited || nextNotification.forFeedback || nextNotification.urgency === Urgency.CRITICAL;
                if (showNextNotification)
                    this._showNotification();
            }
        } else if (this._notificationState === State.SHOWING ||
                   this._notificationState === State.SHOWN) {
            let expired = (this._userActiveWhileNotificationShown &&
                           this._notificationState === State.SHOWN &&
                           this._notificationTimeoutId === 0 &&
                           this._notification.urgency !== Urgency.CRITICAL &&
                           !this._pointerInNotification) || this._notificationExpired;
            let mustClose = this._notificationRemoved || !hasNotifications || expired;

            if (mustClose) {
                let animate = hasNotifications && !this._notificationRemoved;
                this._hideNotification(animate);
            } else if (this._notificationState === State.SHOWN &&
                       this._pointerInNotification) {
                if (!this._banner.expanded)
                    this._expandBanner(false);
                else
                    this._ensureBannerFocused();
            }
        }

        this._updatingState = false;

        // Clean transient variables that are used to communicate actions
        // to updateState()
        this._notificationExpired = false;
    }

    _onIdleMonitorBecameActive() {
        this._userActiveWhileNotificationShown = true;
        this._updateNotificationTimeout(2000);
        this._updateState();
    }

    _showNotification() {
        this._notification = this._notificationQueue.shift();
        this.emit('queue-changed');

        this._userActiveWhileNotificationShown = this.idleMonitor.get_idletime() <= IDLE_TIME;
        if (!this._userActiveWhileNotificationShown) {
            // If the user isn't active, set up a watch to let us know
            // when the user becomes active.
            this.idleMonitor.add_user_active_watch(this._onIdleMonitorBecameActive.bind(this));
        }

        this._banner = new MessageList.NotificationMessage(this._notification);
        this._banner.can_focus = false;
        this._banner._header.expandButton.visible = false;
        this._banner.add_style_class_name('notification-banner');

        this._bannerBin.add_child(this._banner);

        this._bannerBin.opacity = 0;
        this._bannerBin.y = -this._banner.height;
        this.show();

        global.compositor.disable_unredirect();
        this._updateShowingNotification();

        let [x, y] = global.get_pointer();
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
    }

    _updateShowingNotification() {
        this._notification.acknowledged = true;
        this._notification.playSound();

        // We auto-expand notifications with CRITICAL urgency, or for which the relevant setting
        // is on in the control center.
        if (this._notification.urgency === Urgency.CRITICAL ||
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

        this._notificationState = State.SHOWING;
        this._bannerBin.remove_all_transitions();
        this._bannerBin.ease({
            opacity: 255,
            duration: ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
        this._bannerBin.set_pivot_point(0.5, 0.5);
        this._bannerBin.scale_x = 0.9;
        this._bannerBin.scale_y = 0.9;
        this._bannerBin.ease({
            y: 0,
            scale_x: 1,
            scale_y: 1,
            duration: ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_BACK,
            onComplete: () => {
                this._notificationState = State.SHOWN;
                this._showNotificationCompleted();
                this._updateState();
            },
        });
    }

    _showNotificationCompleted() {
        if (this._notification.urgency !== Urgency.CRITICAL)
            this._updateNotificationTimeout(NOTIFICATION_TIMEOUT);
    }

    _updateNotificationTimeout(timeout) {
        if (this._notificationTimeoutId) {
            GLib.source_remove(this._notificationTimeoutId);
            this._notificationTimeoutId = 0;
        }
        if (timeout > 0) {
            this._notificationTimeoutId =
                GLib.timeout_add(GLib.PRIORITY_DEFAULT, timeout,
                    this._notificationTimeout.bind(this));
            GLib.Source.set_name_by_id(this._notificationTimeoutId, '[gnome-shell] this._notificationTimeout');
        }
    }

    _notificationTimeout() {
        let [x, y] = global.get_pointer();
        if (y < this._lastSeenMouseY - 10 && !this._notificationHovered) {
            // The mouse is moving towards the notification, so don't
            // hide it yet. (We just create a new timeout (and destroy
            // the old one) each time because the bookkeeping is
            // simpler.)
            this._updateNotificationTimeout(1000);
        } else if (this._useLongerNotificationLeftTimeout && !this._notificationLeftTimeoutId &&
                  (x !== this._lastSeenMouseX || y !== this._lastSeenMouseY)) {
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
    }

    _hideNotification(animate) {
        this._notificationFocusGrabber.ungrabFocus();

        this._banner.disconnectObject(this);

        this._resetNotificationLeftTimeout();
        this._bannerBin.remove_all_transitions();

        const duration = animate ? ANIMATION_TIME : 0;
        this._notificationState = State.HIDING;
        this._bannerBin.ease({
            opacity: 0,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_BACK,
        });
        this._bannerBin.ease({
            y: -this._bannerBin.height,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_BACK,
            onStopped: () => {
                this._notificationState = State.HIDDEN;
                this._hideNotificationCompleted();
                this._updateState();
            },
        });
    }

    _hideNotificationCompleted() {
        let notification = this._notification;
        this._notification = null;
        if (!this._notificationRemoved && notification.isTransient)
            notification.destroy(NotificationDestroyedReason.EXPIRED);

        this._pointerInNotification = false;
        this._notificationRemoved = false;
        global.compositor.enable_unredirect();

        this._banner.destroy();
        this._banner = null;
        this.hide();
    }

    _expandActiveNotification() {
        if (!this._banner)
            return;

        this._expandBanner(false);
    }

    _expandBanner(autoExpanding) {
        // Don't animate changes in notifications that are auto-expanding.
        this._banner.expand(!autoExpanding);

        // Don't focus notifications that are auto-expanding.
        if (!autoExpanding)
            this._ensureBannerFocused();
    }

    _ensureBannerFocused() {
        this._notificationFocusGrabber.grabFocus();
    }
});

let systemNotificationSource = null;

/**
 * The {Source} that should be used to send system notifications.
 *
 * @returns {Source}
 */
export function getSystemSource() {
    if (!systemNotificationSource) {
        systemNotificationSource = new Source({
            title: _('System'),
            iconName: 'cog-wheel-symbolic',
        });

        systemNotificationSource.connect('destroy', () => {
            systemNotificationSource = null;
        });
        Main.messageTray.add(systemNotificationSource);
    }

    return systemNotificationSource;
}
