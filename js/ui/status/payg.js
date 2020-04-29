// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported Indicator */

const { Gio, GLib, GObject } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const Payg = imports.ui.payg;
const PopupMenu = imports.ui.popupMenu;
const PaygManager = imports.misc.paygManager;

const BUS_NAME = 'com.endlessm.Payg1';
const OBJECT_PATH = '/com/endlessm/Payg1';
const INTERFACE = 'com.endlessm.Payg1';
const REFRESH_TIME_SECS = 60;

var Indicator = GObject.registerClass(
class PaygIndicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._paygManager = new PaygManager.PaygManager();
        this._indicator = this._addIndicator();
        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._paygNotifier = new Payg.PaygNotifier();
        this._item.menu.addAction(_('Enter unlock code…'), () => {
            this._paygNotifier.notify(-1);
        });
        this.menu.addMenuItem(this._item);

        // show this status applet if PAYG is enabled and fill in
        // "determining time..." label and icon
        this._sync();

        // fill in current values as soon as _paygManager is initialized
        this._updateTimeRemainingAndSyncWhenReady();

        // update immediately when the user extends their time (so they don't
        // have to wait for the up to REFRESH_TIME_SECS seconds which would
        // likely be long enough that they might worry something went wrong)
        this._expiryTimeChangedId = this._paygManager.connect('expiry-time-changed', () => {
            this._updateTimeRemainingAndSyncWhenReady();
        });

        // refresh the displayed icon and "time remaining" label periodically
        this._timeoutRefreshId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            REFRESH_TIME_SECS,
            () => this._timeoutRefresh());
        GLib.Source.set_name_by_id(this._timeoutRefreshId, '[gnome-shell] this._timeoutRefresh');
    }

    _timeoutRefresh() {
        this._updateTimeRemainingAndSyncWhenReady();
        return GLib.SOURCE_CONTINUE;
    }

    _onDestroy() {
        super._onDestroy();

        if (this._expiryTimeChangedId !== 0)
            this._paygManager.disconnect(this._expiryTimeChangedId);

        if (this._timeoutRefreshId !== 0)
            GLib.source_remove(this._timeoutRefreshId);
    }

    _getMenuGicon() {
        const URGENT_EXPIRATION_S = 15 * 60;
        let timeLeftSeconds = this._paygManager.timeRemainingSecs();

        let iconUri = 'resource:///org/gnome/shell/theme/payg-normal-symbolic.svg';
        // if time left <= 0, we haven't yet determined it, so fall back to
        // "normal" icon
        if (timeLeftSeconds >= 0 && timeLeftSeconds <= URGENT_EXPIRATION_S)
            iconUri = 'resource:///org/gnome/shell/theme/payg-near-expiration-symbolic.svg';

        return new Gio.FileIcon({ file: Gio.File.new_for_uri(iconUri) });
    }

    _getTimeRemainingString() {
        // the time will be invalid if the manager hasn't been
        // intitialized yet so return with a default message in that case
        if (!this._paygManager.initialized)
            return _('Getting time…');

        // if PAYG is disabled, nothing should be showing this label
        if (!this._paygManager.enabled)
            return '';

        let seconds = this._paygManager.timeRemainingSecs();
        if (seconds == 0)
            return _('Subscription expired');
        if (seconds < 60)
            return _('Less than 1 minute');

        return Payg.timeToString(seconds);
    }

    _sync() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter && this._paygManager.enabled;
        this.menu.setSensitive(sensitive);
        this._item.actor.visible = this._indicator.visible = this._paygManager.enabled;
        this._item.label.text = this._getTimeRemainingString();
        this._item.icon.gicon = this._getMenuGicon();
        this._indicator.gicon = this._item.icon.gicon;
    }

    _updateTimeRemainingAndSyncWhenReady() {
        // We can't use the PaygManager until it's initialized
        if (this._paygManager.initialized) {
            this._sync();
        } else {
            let paygManagerId = this._paygManager.connect('initialized', () => {
                this._sync();
                this._initializeAccountIDMenu();
                this._paygManager.disconnect(paygManagerId);
            });
        }
    }

    // Differently from the remaining time counter, we just want to show the account ID
    // when the backend properly supports it.
    _initializeAccountIDMenu() {
        if (this._verifyValidAccountID()) {
            this._paygItem = new PopupMenu.PopupSubMenuMenuItem('', true);
            this._paygItem.setSensitive(false);
            this._paygItem.actor.visible = this._indicator.visible = this._paygManager.enabled;
            this._paygItem.label.text = _('Account ID: %s').format(this._paygManager.accountID);
            this._paygItem.icon.gicon = this._getMenuGicon();

            this.menu.addMenuItem(this._paygItem);
        }
    }

    _verifyValidAccountID() {
        return Main.paygManager.accountID !== '0';
    }
});
