import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import GObject from 'gi://GObject';
import Gio from 'gi://Gio';
import GnomeQR from 'gi://GnomeQR';
import St from 'gi://St';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';

Gio._promisify(GnomeQR, 'generate_qr_code_async');

const QR_CODE_DEFAULT_SIZE = 150;
const QR_CODE_TRANSPARENT_COLOR = new GnomeQR.Color({alpha: 0});

export class QrCode extends St.Bin {
    static [GObject.GTypeName] = 'QrCode';
    static [GObject.properties] = {
        'url': GObject.ParamSpec.string(
            'url', null, null,
            GObject.ParamFlags.READWRITE,
            null),
    };

    static {
        GObject.registerClass(this);
    }

    constructor(params) {
        const qrSize = Math.max(params.width ?? 0, params.height ?? 0) ||
            QR_CODE_DEFAULT_SIZE;

        super({
            styleClass: 'qr-code',
            width: qrSize,
            height: qrSize,
            ...params,
        });

        this.set_child(new Clutter.Actor({
            xExpand: true,
            yExpand: true,
            xAlign: Clutter.ActorAlign.FILL,
            yAlign: Clutter.ActorAlign.FILL,
            minificationFilter: Clutter.ScalingFilter.NEAREST,
            magnificationFilter: Clutter.ScalingFilter.NEAREST,
        }));

        this._fgColor = null;

        this.connect('notify::url', () =>
            this._update().catch(logErrorUnlessCancelled));
    };

    vfunc_allocate(box) {
        const width = box.get_width();
        const height = box.get_height();

        box.set_size(Math.min(width, height), Math.min(width, height));
        super.vfunc_allocate(box);
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();

        if (!this.get_parent())
            return;

        const node = this.get_theme_node();
        const fgColor = node.get_foreground_color();

        if (!this._fgColor?.equal(fgColor)) {
            this._fgColor = fgColor;
            this._update().catch(logErrorUnlessCancelled);
        }
    }

    async _update() {
        const fgColor = QrCode.#getGnomeQRColor(this._fgColor);

        this._cancellable?.cancel();
        const cancellable = new Gio.Cancellable();
        this._cancellable = cancellable;

        const [pixelData, qrSize] = await GnomeQR.generate_qr_code_async(
            this.url,
            0 /* size, are fine with the minimum value */,
            QR_CODE_TRANSPARENT_COLOR,
            fgColor,
            GnomeQR.PixelFormat.RGBA_8888,
            GnomeQR.EccLevel.LOW,
            cancellable);

        const coglContext = global.stage.context.get_backend().get_cogl_context();
        const bpp = Cogl.pixel_format_get_bytes_per_pixel(Cogl.PixelFormat.RGBA_8888, 0);
        const rowStride = qrSize * bpp;

        const content = St.ImageContent.new_with_preferred_size(qrSize, qrSize);
        content.set_bytes(
            coglContext,
            pixelData,
            Cogl.PixelFormat.RGBA_8888,
            qrSize,
            qrSize,
            rowStride);

        this.child.set_content(content);
    }

    static #getGnomeQRColor(color) {
        if (!color)
            return null;

        return new GnomeQR.Color({
            red: color.red,
            green: color.green,
            blue: color.blue,
            alpha: color.alpha,
        });
    }
};
