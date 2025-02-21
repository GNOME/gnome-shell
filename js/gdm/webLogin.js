// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// A widget showing a URL for web login
/* exported WebLoginPrompt */

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Gio from 'gi://Gio';
import Shell from 'gi://Shell';
import Pango from 'gi://Pango';
import St from 'gi://St';
import {Spinner} from '../ui/animation.js';
import * as ModalDialog from '../ui/modalDialog.js';
import * as Params from '../misc/params.js';

const QR_CODE_SIZE = 150;
const WEB_LOGIN_SPINNER_SIZE = 35;

Gio._promisify(Shell.QrCodeGenerator.prototype, 'generate_qr_code');

export const QrCode = GObject.registerClass(
class QrCode extends St.Bin {
    _init(params) {
        const themeContext = St.ThemeContext.get_for_stage(global.stage);
        const {iconSize, url} = Params.parse(params, {
            iconSize: QR_CODE_SIZE,
            url: null,
        });

        super._init({
            width: QR_CODE_SIZE,
            height: QR_CODE_SIZE,
            x_align: Clutter.ActorAlign.CENTER,
        });

        this._qrCodeGenerator = new Shell.QrCodeGenerator();
        this._iconSize = iconSize;
        this._url = url;
        this.child = new St.Icon({
            icon_size: this._iconSize,
            style_class: 'qr-code',
        });

        themeContext.connectObject('notify::scale-factor',
            () => this.update().catch(logError), this);
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();

        const node = this.child.get_theme_node();
        const [found, iconSize] = node.lookup_length('icon-size', false);

        if (!found)
            return;

        const themeContext = St.ThemeContext.get_for_stage(global.stage);

        this._iconSize = iconSize / themeContext.scaleFactor;
        this.update().catch(logError);
    }

    async update() {
        const {scaleFactor} = St.ThemeContext.get_for_stage(global.stage);
        this.set_size(
            this._iconSize * scaleFactor,
            this._iconSize * scaleFactor);

        this._cancellable?.cancel();
        const cancellable = new Gio.Cancellable();
        let destroyId = this.connect('destroy', () => {
            cancellable.cancel();
            destroyId = 0;
        });

        try {
            this._cancellable = cancellable;
            this.child.gicon = await this._qrCodeGenerator.generate_qr_code(
                this._url, this._iconSize, this._iconSize, null, null,
                cancellable);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }

        if (destroyId) {
            this.disconnect(destroyId);
            this._cancellable = null;
        }
    }
});

export const WebLoginPrompt = GObject.registerClass(
class WebLoginPrompt extends St.BoxLayout {
    _init(params) {
        const {iconSize, message, url, code} = Params.parse(params, {
            iconSize: QR_CODE_SIZE,
            message: null,
            url: null,
            code: null,
        });

        super._init({
            styleClass: 'web-login-prompt',
            vertical: true,
        });

        this._urlTitleLabel = new St.Label({
            text: message,
            style_class: 'web-login-title-label',
        });
        this._urlTitleLabel.clutterText.set({
            lineWrap: true,
            ellipsize: Pango.EllipsizeMode.NONE,
        });
        this.add_child(this._urlTitleLabel);

        this._qrCode = new QrCode({iconSize, url});
        this.add_child(this._qrCode);

        this._urlLabel = new St.Label({
            style_class: 'web-login-url-label',
            text: this._formatURLForDisplay(url),
            x_expand: true,
        });
        this.add_child(this._urlLabel);

        if (code) {
            this._codeBox = new St.BoxLayout({
                x_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            this.add_child(this._codeBox);

            this._codeTitleLabel = new St.Label({
                text: _('Login code: '),
                style_class: 'web-login-code-title-label',
            });
            this._codeBox.add_child(this._codeTitleLabel);

            this._codeLabel = new St.Label({
                text: code,
                style_class: 'web-login-code-label',
            });
            this._codeBox.add_child(this._codeLabel);
        }
    }

    _formatURLForDisplay(url) {
        const http = 'http://';
        const https = 'https://';

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
        'done': {},
    },
}, class WebLoginDialog extends ModalDialog.ModalDialog {
    _init(params) {
        const {message, url, code} = Params.parse(params, {
            message: null,
            url: null,
            code: null,
        });

        super._init({
            shouldFadeOut: false,
            styleClass: 'web-login-dialog',
        });

        this._webLoginPrompt = new WebLoginPrompt({code, message, url});
        this._webLoginPrompt.set({
            y_align: Clutter.ActorAlign.CENTER,
        });

        this.contentLayout.reactive = false;
        this.contentLayout.can_focus = false;
        this.contentLayout.add_child(this._webLoginPrompt);
        this._updateButtons();

        this._contentOverlay = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            style_class: 'web-login-dialog-content-overlay',
        });
        this._contentOverlay.hide();

        this.backgroundStack.add_child(this._contentOverlay);

        const constraint = new Clutter.BindConstraint({
            source: this.dialogLayout,
            coordinate: Clutter.BindCoordinate.ALL,
        });

        this._contentOverlay.add_constraint(constraint);

        this._spinnerFrame = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            styleClass: 'web-login-spinner',
        });
        this._contentOverlay.add_child(this._spinnerFrame);

        this._spinner = new Spinner(WEB_LOGIN_SPINNER_SIZE, {
            hideOnStop: true,
        });
        this._spinnerFrame.add_child(this._spinner);
    }

    done() {
        this._doneButton.reactive = false;
        this._doneButton.can_focus = false;

        this._contentOverlay.show();
        global.stage.set_key_focus(this.dialogLayout);
        this._spinner.play();

        this.emit('done');
    }

    _updateButtons() {
        this.clearButtons();

        this._cancelButton = this.addButton({
            action: this.cancel.bind(this),
            label: _('Cancel'),
            key: Clutter.KEY_Escape,
        });

        this._doneButton = this.addButton({
            action: this.done.bind(this),
            default: true,
            label: _('Done'),
        });
    }

    cancel() {
        this.emit('cancel');
        this.close();
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
            style_class: 'web-login-intro-button-label',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        label.clutter_text.line_wrap = true;
        label.clutter_text.y_align = Clutter.ActorAlign.CENTER;
        label.clutter_text.x_align = Clutter.ActorAlign.CENTER;

        super._init({
            style_class: 'web-login-prompt login-dialog-button web-login-intro-button',
            accessible_name: message,
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            child: label,
        });
    }
});
