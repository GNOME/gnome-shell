//
// A widget displayed instead of the unlock prompt
// when parental controls session limits are reached

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import St from 'gi://St';

import {loadInterfaceXML} from '../misc/fileUtils.js';

const TimerChildIface = loadInterfaceXML('org.freedesktop.MalcontentTimer1.Child');
const TimerChildProxy = Gio.DBusProxy.makeProxyWrapper(TimerChildIface);

export const ParentalControlsShield = GObject.registerClass(
class ParentalControlsShield extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'parental-controls-shield',
            orientation: Clutter.Orientation.VERTICAL,
            x_align: Clutter.ActorAlign.CENTER,
        });

        this._requestExtensionCookie = null;

        this._timerChildProxy = TimerChildProxy(Gio.DBus.system,
            'org.freedesktop.MalcontentTimer1',
            '/org/freedesktop/MalcontentTimer1',
            (proxy, error) => {
                if (error)
                    console.error(`Failed to get TimerChild proxy: ${error}`);
            },
            null, /* cancellable */
            Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION
        );

        this._timerChildProxy.connectSignal('ExtensionResponse', (proxy, sender, params) =>
            this._onExtensionResponse(proxy, sender, params));

        this.connect('destroy', this._onDestroy.bind(this));

        this._titleLabel = new St.Label({
            style_class: 'parental-controls-shield-title',
            text: _('Screen Time Limit Reached'),
        });
        this.add_child(this._titleLabel);

        this._descriptionLabel = new St.Label({
            style_class: 'parental-controls-shield-description',
            text: _('Daily limit for screen time on this device has been reached. Resume tomorrow.'),
        });
        this._descriptionLabel.clutter_text.line_wrap = true;
        this.add_child(this._descriptionLabel);

        this._ignoreButton = new St.Button({
            style_class: 'parental-controls-shield-button',
            // Translators: this is for ignoring a screen time limit for parental controls
            label: _('Ignore'),
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._ignoreButton.connect('clicked',
            () => this._onIgnoreButtonClicked().catch(logError));
        this.add_child(this._ignoreButton);
    }

    _onDestroy() {
        this._requestExtensionCookie = null;
    }

    async _onIgnoreButtonClicked() {
        if (this._requestExtensionCookie)
            return;

        try {
            [this._requestExtensionCookie] = await this._timerChildProxy.RequestExtensionAsync(
                'login-session',
                '',
                0,
                {},
                Gio.DBusCallFlags.ALLOW_INTERACTIVE_AUTHORIZATION
            );
        } catch (e) {
            console.warn(`Failed to obtain screen time extension: ${e.message}`);
        }
    }

    _onExtensionResponse(proxy, sender, [_, cookie]) {
        if (this._requestExtensionCookie === null ||
            cookie !== this._requestExtensionCookie)
            return;

        this._requestExtensionCookie = null;
    }
});
