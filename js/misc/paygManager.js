// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2018 Endless Mobile, Inc.
//
// This is a GNOME Shell component to wrap the interactions over
// D-Bus with the eos-payg system daemon.
//
// Licensed under the GNU General Public License Version 2
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

/* exported PaygManager */

const { Gio, GLib, GnomeDesktop, GObject, Shell } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;

const Gettext = imports.gettext;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const EOS_PAYG_NAME = 'com.endlessm.Payg1';
const EOS_PAYG_PATH = '/com/endlessm/Payg1';

const EOS_PAYG_IFACE = loadInterfaceXML('com.endlessm.Payg1');

var PaygErrorDomain = GLib.quark_from_string('payg-error');

var PaygError = {
    INVALID_CODE: 0,
    CODE_ALREADY_USED: 1,
    TOO_MANY_ATTEMPTS: 2,
    DISABLED: 3,
};

const DBusErrorsMapping = {
    INVALID_CODE: 'com.endlessm.Payg1.Error.InvalidCode',
    CODE_ALREADY_USED: 'com.endlessm.Payg1.Error.CodeAlreadyUsed',
    TOO_MANY_ATTEMPTS: 'com.endlessm.Payg1.Error.TooManyAttempts',
    DISABLED: 'com.endlessm.Payg1.Error.Disabled',
};

// Title and description text to be shown in the periodic reminders.
const NOTIFICATION_TITLE_TEXT = _('Pay as You Go');
const NOTIFICATION_DETAILED_FORMAT_STRING = _('Subscription runs out in %s.');

// This list defines the different instants in time where we would
// want to show notifications to the user reminding that the payg
// subscription will be expiring soon, up to a max GLib.MAXUINT32.
//
// It contains a list of integers representing the number of seconds
// earlier to the expiration time when we want to show a notification,
// which needs to be sorted in DESCENDING order.
const notificationAlertTimesSecs = [
    60 * 60 * 48, // 2 days
    60 * 60 * 24, // 1 day
    60 * 60 * 2,  // 2 hours
    60 * 60,      // 1 hour
    60 * 30,      // 30 minutes
    60 * 2,       // 2 minutes
    30,           // 30 seconds
];

// Takes an UNIX timestamp (in seconds) and returns a string
// with a precision level appropriate to show to the user.
//
// The returned string will be formatted just in seconds for times
// under 1 minute, in minutes for times under 2 hours, in hours and
// minutes (if applicable) for times under 1 day, and then in days
// and hours (if applicable) for anything longer than that in days.
//
// Some examples:
//   - 45 seconds => "45 seconds"
//   - 60 seconds => "1 minute"
//   - 95 seconds => "1 minute"
//   - 120 seconds => "2 minutes"
//   - 3600 seconds => "60 minutes"
//   - 4500 seconds => "75 minutes"
//   - 7200 seconds => "2 hours"
//   - 8640 seconds => "2 hours 24 minutes"
//   - 86400 seconds => "1 day"
//   - 115200 seconds => "1 day 8 hours"
//   - 172800 seconds => "2 days"
function timeToString(seconds) {
    if (seconds < 60)
        return Gettext.ngettext('%s second', '%s seconds', seconds).format(Math.floor(seconds));

    let minutes = Math.floor(seconds / 60);
    if (minutes < 120)
        return Gettext.ngettext('%s minute', '%s minutes', minutes).format(minutes);

    let hours = Math.floor(minutes / 60);
    if (hours < 24) {
        let hoursStr = Gettext.ngettext('%s hour', '%s hours', hours).format(hours);

        let minutesPast = minutes % 60;
        if (minutesPast == 0)
            return hoursStr;

        let minutesStr = Gettext.ngettext('%s minute', '%s minutes', minutesPast).format(minutesPast);
        return ("%s %s").format(hoursStr, minutesStr);
    }

    let days = Math.floor(hours / 24);
    let daysStr = Gettext.ngettext('%s day', '%s days', days).format(days);

    let hoursPast = hours % 24;
    if (hoursPast == 0)
        return daysStr;

    let hoursStr = Gettext.ngettext('%s hour', '%s hours', hoursPast).format(hoursPast);
    return ("%s %s").format(daysStr, hoursStr);
}

var PaygManager = GObject.registerClass({
    Signals: {
        'code-expired': {},
        'code-format-changed': {},
        'enabled-changed': { param_types: [GObject.TYPE_BOOLEAN] },
        'expiry-time-changed': { param_types: [GObject.TYPE_INT64] },
        'initialized': {},
    },
}, class PaygManager extends GObject.Object {

    _init() {
        super._init();

        this._initialized = false;
        this._proxy = null;

        this._enabled = false;
        this._expiryTime = 0;
        this._rateLimitEndTime = 0;
        this._codeFormat = '';
        this._codeFormatRegex = null;
        this._notification = null;

        // Keep track of clock changes to update notifications.
        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', this._clockUpdated.bind(this));

        // D-Bus related initialization code only below this point.
        this._proxyInfo = Gio.DBusInterfaceInfo.new_for_xml(EOS_PAYG_IFACE);

        this._codeExpiredId = 0;
        this._propertiesChangedId = 0;
        this._expirationReminderId = 0;

        this._proxy = new Gio.DBusProxy({
            g_connection: Gio.DBus.system,
            g_interface_name: this._proxyInfo.name,
            g_interface_info: this._proxyInfo,
            g_name: EOS_PAYG_NAME,
            g_object_path: EOS_PAYG_PATH,
            g_flags: Gio.DBusProxyFlags.NONE,
        });

        this._proxy.init_async(GLib.PRIORITY_DEFAULT, null, this._onProxyConstructed.bind(this));

        for (let errorCode in DBusErrorsMapping)
            Gio.DBusError.register_error(PaygErrorDomain, PaygError[errorCode], DBusErrorsMapping[errorCode]);
    }

    _onProxyConstructed(object, res) {
        let success = false;
        try {
            success = object.init_finish(res);
        } catch (e) {
            logError(e, 'Error while constructing D-Bus proxy for %s'.format(EOS_PAYG_NAME));
        }

        if (success) {
            // Don't use the setters here to prevent emitting a -changed signal
            // on startup, which is useless and confuses the screenshield when
            // selecting the session mode to construct the right unlock dialog.
            this._enabled = this._proxy.Enabled;
            this._expiryTime = this._proxy.ExpiryTime;
            this._rateLimitEndTime = this._proxy.RateLimitEndTime;
            this._setCodeFormat(this._proxy.CodeFormat || '^[0-9]{8}$');

            this._propertiesChangedId = this._proxy.connect('g-properties-changed', this._onPropertiesChanged.bind(this));
            this._codeExpiredId = this._proxy.connectSignal('Expired', this._onCodeExpired.bind(this));

            this._maybeNotifyUser();
            this._updateExpirationReminders();
        }

        this._initialized = true;
        this.emit('initialized');
    }

    _onPropertiesChanged(proxy, changedProps) {
        let propsDict = changedProps.deep_unpack();
        if (propsDict.hasOwnProperty('Enabled'))
            this._setEnabled(this._proxy.Enabled);

        if (propsDict.hasOwnProperty('ExpiryTime'))
            this._setExpiryTime(this._proxy.ExpiryTime);

        if (propsDict.hasOwnProperty('RateLimitEndTime'))
            this._setRateLimitEndTime(this._proxy.RateLimitEndTime);

        if (propsDict.hasOwnProperty('CodeFormat'))
            this._setCodeFormat(this._proxy.CodeFormat, true);
    }

    _setEnabled(value) {
        if (this._enabled === value)
            return;

        this._enabled = value;
        this.emit('enabled-changed', this._enabled);
    }

    _setExpiryTime(value) {
        if (this._expiryTime === value)
            return;

        this._expiryTime = value;
        this._updateExpirationReminders();

        this.emit('expiry-time-changed', this._expiryTime);
    }

    _setRateLimitEndTime(value) {
        if (this._rateLimitEndTime === value)
            return;

        this._rateLimitEndTime = value;
        this.emit('rate-limit-end-time-changed', this._rateLimitEndTime);
    }

    _setCodeFormat(value, notify = false) {
        if (this._codeFormat === value)
            return;

        this._codeFormat = value;
        try {
            this._codeFormatRegex = new GLib.Regex(
                this._codeFormat,
                GLib.RegexCompileFlags.DOLLAR_ENDONLY,
                GLib.RegexMatchFlags.PARTIAL);
        } catch (e) {
            logError(e, 'Error compiling CodeFormat regex: %s'.format(this._codeFormat));
            this._codeFormatRegex = null;
        }

        if (notify)
            this.emit('code-format-changed');
    }

    _onCodeExpired() {
        this.emit('code-expired');
    }

    _clockUpdated() {
        this._updateExpirationReminders();
    }

    _notifyPaygReminder(secondsLeft) {
        // Only notify when in an regular session, not in GDM or initial-setup.
        if (Main.sessionMode.currentMode != 'user' &&
            Main.sessionMode.currentMode != 'endless' &&
            Main.sessionMode.currentMode != 'user-coding')
            return;

        if (this._notification)
            this._notification.destroy();

        let source = new MessageTray.SystemNotificationSource();
        Main.messageTray.add(source);

        let timeLeft = timeToString(secondsLeft);
        this._notification = new MessageTray.Notification(
            source,
            NOTIFICATION_TITLE_TEXT,
            NOTIFICATION_DETAILED_FORMAT_STRING.format(timeLeft));
        this._notification.setUrgency(MessageTray.Urgency.HIGH);
        this._notification.setTransient(false);
        source.notify(this._notification);

        this._notification.connect('destroy', () => this._notification = null);
    }

    _maybeNotifyUser() {
        // Sanity check.
        if (notificationAlertTimesSecs.length === 0)
            return;

        let secondsLeft = this.timeRemainingSecs();
        if (secondsLeft > 0 && secondsLeft <= notificationAlertTimesSecs[0])
            this._notifyPaygReminder(secondsLeft);
    }

    _updateExpirationReminders() {
        if (this._expirationReminderId > 0) {
            GLib.source_remove(this._expirationReminderId);
            this._expirationReminderId = 0;
        }

        let secondsLeft = this.timeRemainingSecs();

        // The interval passed to timeout_add_seconds needs to be a 32-bit
        // unsigned integer, so don't bother with notifications otherwise.
        if (secondsLeft <= 0 || secondsLeft >= GLib.MAXUINT32)
            return;

        // Look for the right time to set the alarm for.
        let targetAlertTime = 0;
        for (let alertTime of notificationAlertTimesSecs) {
            if (secondsLeft > alertTime) {
                targetAlertTime = alertTime;
                break;
            }
        }

        // Too late to set up an alarm now.
        if (targetAlertTime == 0)
            return;

        this._expirationReminderId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            secondsLeft - targetAlertTime,
            () => {
                // We want to show "round" numbers in the notification, matching
                // whatever is specified in the notificationAlertTimeSecs array.
                this._notifyPaygReminder(targetAlertTime);

                // Reset _expirationReminderId before _updateExpirationReminders()
                // to prevent an attempt to remove the same GSourceFunc twice.
                this._expirationReminderId = 0;
                this._updateExpirationReminders();

                return GLib.SOURCE_REMOVE;
            });
    }

    timeRemainingSecs() {
        if (!this._enabled)
            return Number.MAX_SAFE_INTEGER;

        return Math.max(0, this._expiryTime - (Shell.util_get_boottime() / GLib.USEC_PER_SEC));
    }

    addCode(code, callback) {
        if (!this._proxy) {
            log('Unable to add PAYG code: No D-Bus proxy for %s'.format(EOS_PAYG_NAME));
            return;
        }

        this._proxy.AddCodeRemote(code, (result, error) => {
            if (callback)
                callback(error);
        });
    }

    clearCode() {
        if (!this._proxy) {
            log('Unable to clear PAYG code: No D-Bus proxy for %s'.format(EOS_PAYG_NAME));
            return;
        }

        this._proxy.ClearCodeRemote();
    }

    validateCode(code, partial = false) {
        if (!this._codeFormatRegex) {
            log('Unable to validate PAYG code: no regex');
            return false;
        }

        let [isMatch, matchInfo] = this._codeFormatRegex.match(code, 0);
        return isMatch || (partial && matchInfo.is_partial_match());
    }

    get initialized() {
        return this._initialized;
    }

    get enabled() {
        return this._enabled;
    }

    get expiryTime() {
        return this._expiryTime;
    }

    get rateLimitEndTime() {
        return this._rateLimitEndTime;
    }

    get isLocked() {
        if (!this.enabled)
            return false;

        return this.timeRemainingSecs() <= 0;
    }
});
