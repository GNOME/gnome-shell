// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// A widget showing a URL for web login
/* exported WebLoginPrompt */

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Pango from 'gi://Pango';
import St from 'gi://St';
import {Spinner} from '../ui/animation.js';
import * as Params from '../misc/params.js';
import {QrCode} from '../ui/qrCode.js';

const QR_CODE_SIZE = 150;
const WEB_LOGIN_SPINNER_SIZE = 35;

export const WebLoginPrompt = GObject.registerClass(
class WebLoginPrompt extends St.BoxLayout {
    _init(params) {
        const {qrSize: qrCodeSize, message, url, code} = Params.parse(params, {
            qrSize: QR_CODE_SIZE,
            message: null,
            url: null,
            code: null,
        });

        super._init({
            styleClass: 'web-login-prompt',
            vertical: true,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._urlTitleLabel = new St.Label({
            style_class: 'web-login-title-label',
        });
        this._urlTitleLabel.clutterText.set({
            lineWrap: true,
            ellipsize: Pango.EllipsizeMode.NONE,
        });
        this.add_child(this._urlTitleLabel);

        this._qrCode = new QrCode({
            width: qrCodeSize,
            height: qrCodeSize,
            xAlign: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._qrCode);

        this._urlLabel = new St.Label({
            style_class: 'web-login-url-label',
            x_expand: true,
        });
        this.add_child(this._urlLabel);

        this._codeBox = new St.BoxLayout({
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });

        this._codeTitleLabel = new St.Label({
            text: _('Login code: '),
            style_class: 'web-login-code-title-label',
        });
        this._codeBox.add_child(this._codeTitleLabel);

        this._codeLabel = new St.Label({
            style_class: 'web-login-code-label',
        });
        this._codeBox.add_child(this._codeLabel);

        this.update({message, url, code});
    }

    update(params) {
        const {message, url, code} = Params.parse(params, {
            message: null,
            url: null,
            code: null,
        });

        if (message !== null)
            this._urlTitleLabel.text = message;

        if (url !== null) {
            this._qrCode.set({url});

            const formattedUrl = this._formatURLForDisplay(url);
            this._urlLabel.text = formattedUrl;

            if (formattedUrl.length > 45)
                this._urlLabel.add_style_class_name('web-login-url-label-long');
            else
                this._urlLabel.remove_style_class_name('web-login-url-label-long');
        }

        if (code !== null) {
            this._codeLabel.text = code;
            if (!this._codeBox.get_parent())
                this.add_child(this._codeBox);
        } else {
            // eslint-disable-next-line no-lonely-if
            if (this._codeBox.get_parent())
                this.remove_child(this._codeBox);
        }
    }

    _formatURLForDisplay(url) {
        const http = 'http://';
        const https = 'https://';

        if (!url)
            return http;

        if (url.endsWith('/'))
            url = url.substring(0, url.length - 1);

        if (url.startsWith(http))
            return url.substring(http.length);

        if (url.startsWith(https))
            return url.substring(https.length);

        return url;
    }
});

export const WebLoginDialog = GObject.registerClass({
    Signals: {
        'cancel': {},
        'loading': {},
    },
}, class WebLoginDialog extends St.Widget {
    _init(params) {
        const {message, url, code, buttons} = Params.parse(params, {
            message: null,
            url: null,
            code: null,
            buttons: [],
        });

        super._init({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            visible: false,
        });
        this.bind_property_full('reactive',
            this, 'opacity',
            GObject.BindingFlags.SYNC_CREATE,
            (_bind, source) => [true, source ? 255 : 128],
            null);

        this._contentBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            y_expand: true,
        });
        this.add_child(this._contentBox);

        this._webLoginPrompt = new WebLoginPrompt({code, message, url});
        this._contentBox.add_child(this._webLoginPrompt);

        this._buttonBox = new St.BoxLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });
        this._contentBox.add_child(this._buttonBox);

        this._updateButtons(buttons);

        this._spinnerFrame = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            style_class: 'web-login-spinner',
        });
        this.add_child(this._spinnerFrame);

        this._spinner = new Spinner(WEB_LOGIN_SPINNER_SIZE, {
            hideOnStop: true,
        });
        this._spinnerFrame.add_child(this._spinner);
    }

    _updateButtons(buttons) {
        this._buttonBox.get_children().forEach(b => b.disconnectObject(this));
        this._buttonBox.remove_all_children();

        this._cancelButton = this._addButton({
            label: _('Cancel'),
            action: () => this.emit('cancel'),
        });

        buttons?.forEach(b => this._addButton(b));
    }

    _addButton(b) {
        const button = new St.Button({
            style_class: 'web-login-prompt-button',
            can_focus: true,
            accessible_name: b.label,
            child: new St.Label({
                text: b.label,
                style_class: 'web-login-button-label',
            }),
        });

        button.connectObject('clicked', () => {
            if (b.needsLoading)
                this._startLoading();
            b.action();
        }, this);

        button.default = b.default;

        this._buttonBox.add_child(button);

        this.bind_property('reactive',
            button, 'reactive',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('reactive',
            button, 'can_focus',
            GObject.BindingFlags.SYNC_CREATE);

        return button;
    }

    vfunc_key_focus_in() {
        this._buttonBox.get_children().find(b => b.default)?.grab_key_focus();
    }

    update(params) {
        const {message, url, code, buttons} = Params.parse(params, {
            message: null,
            url: null,
            code: null,
            buttons: [],
        });

        this.stopLoading();

        this._webLoginPrompt.update({message, url, code});

        this._updateButtons(buttons);
    }

    _startLoading() {
        this._webLoginPrompt.opacity = 128;

        this._buttonBox.get_children()
            .filter(b => b !== this._cancelButton)
            .forEach(b => {
                b.opacity = 128;
                b.can_focus = false;
                b.reactive = false;
            });
        this._cancelButton.grab_key_focus();

        this._spinnerFrame.show();
        this._spinner.play();

        this.isLoading = true;
        this.emit('loading');
    }

    stopLoading() {
        this._webLoginPrompt.opacity = 255;

        this._buttonBox.get_children()
            .filter(b => b !== this._cancelButton)
            .forEach(b => {
                b.opacity = 255;
                b.can_focus = this.reactive;
                b.reactive = this.reactive;
            });

        this._spinnerFrame.hide();
        this._spinner.stop();

        this.isLoading = false;
        this.emit('loading');
    }
});

export var WebLoginIntro = GObject.registerClass(
class WebLoginIntro extends St.Button {
    _init(params) {
        const {message} = Params.parse(params, {
            message: null,
        });

        const label = new St.Label({
            text: message,
            style_class: 'web-login-button-label',
        });

        super._init({
            style_class: 'web-login-intro-button',
            accessible_name: message,
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            child: label,
        });
    }

    setMessage(message) {
        this.child.text = message;
        this.accessible_name = message;
    }
});
