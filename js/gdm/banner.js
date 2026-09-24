/*
 * Copyright 2026 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Pango from 'gi://Pango';
import St from 'gi://St';

import * as Settings from './settings.js';

Gio._promisify(Gio.File.prototype, 'load_contents_async');

const _FADE_ANIMATION_TIME = 250;
const _AUTO_MODE_BODY_LENGTH_THRESHOLD = 300;
const _AUTO_MODE_BODY_LINE_THRESHOLD = 3;

export const BannerMessageMode = {
    AUTO: 'auto',
    INLINE: 'inline',
    ACKNOWLEDGEMENT: 'acknowledgement',
};

function _resolveMessageFile(settings) {
    if (settings.get_string(Settings.BANNER_MESSAGE_SOURCE_KEY) !== 'file')
        return null;

    const path = settings.get_string(Settings.BANNER_MESSAGE_PATH_KEY);
    return path ? Gio.File.new_for_path(path) : null;
}

async function _readBannerBody(settings, file) {
    if (file) {
        try {
            const [contents] = await file.load_contents_async(null);
            return new TextDecoder().decode(contents);
        } catch (e) {
            console.error(`Failed to read banner from ${file.get_path()}: ${e.message}`);
            return '';
        }
    }

    return settings.get_string(Settings.BANNER_MESSAGE_TEXT_KEY);
}

/**
 * Resolves 'auto' down to either 'inline' or 'acknowledgement' based on the
 * length of the banner body (either in characters or in line breaks), so
 * callers never have to special-case 'auto'. Explicit 'inline'/
 * 'acknowledgement' modes are returned as-is.
 *
 * @param {Gio.Settings} settings the login-screen settings
 * @returns {Promise<string>} the resolved BannerMessageMode
 */
export async function resolveMode(settings) {
    const mode = settings.get_string(Settings.BANNER_MESSAGE_MODE_KEY);

    if (mode !== BannerMessageMode.AUTO)
        return mode;

    const body = await _readBannerBody(settings, _resolveMessageFile(settings));

    const isTooLong = body.length > _AUTO_MODE_BODY_LENGTH_THRESHOLD;
    const hasTooManyLines = body.split('\n').length > _AUTO_MODE_BODY_LINE_THRESHOLD;

    return isTooLong || hasTooManyLines
        ? BannerMessageMode.ACKNOWLEDGEMENT
        : BannerMessageMode.INLINE;
}

export class Banner extends St.BoxLayout {
    static {
        GObject.registerClass(this);
    }

    constructor(settings, params = {}) {
        super({
            orientation: Clutter.Orientation.VERTICAL,
            opacity: 0,
            visible: false,
            ...params,
        });

        this._settings = settings;
        this._presented = false;
        this._destroyed = false;
        this.connect('destroy', () => {
            this._destroyed = true;
        });

        this._titleLabel = new St.Label({
            style_class: 'banner-title',
            text: '',
        });
        this._titleLabel.clutter_text.line_wrap = true;
        this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.add_child(this._titleLabel);

        this._bodyLabel = new St.Label({
            style_class: 'banner-body',
            text: '',
        });
        this._bodyLabel.clutter_text.line_wrap = true;
        this._bodyLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        const bodyBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
        });
        bodyBox.add_child(this._bodyLabel);

        this._scrollView = new St.ScrollView({child: bodyBox});
        this.add_child(this._scrollView);

        this._settings.connectObject(
            `changed::${Settings.BANNER_MESSAGE_KEY}`, () => this._update().catch(logError),
            `changed::${Settings.BANNER_MESSAGE_TITLE_KEY}`, () => this._update().catch(logError),
            `changed::${Settings.BANNER_MESSAGE_TEXT_KEY}`, () => this._update().catch(logError),
            `changed::${Settings.BANNER_MESSAGE_SOURCE_KEY}`, () => {
                if (this._updateMessageFile())
                    this._update().catch(logError);
            },
            `changed::${Settings.BANNER_MESSAGE_PATH_KEY}`, () => {
                if (this._updateMessageFile())
                    this._update().catch(logError);
            }, this);

        this._updateMessageFile();
    }

    present() {
        this._presented = true;
        this._syncVisibility();
    }

    unpresent() {
        this._presented = false;
        this._syncVisibility();
    }

    _syncVisibility() {
        if (this._presented && this.enabled)
            this.show();
        else
            this.hide();
    }

    vfunc_show() {
        super.vfunc_show();
        this.ease({
            opacity: 255,
            duration: _FADE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    vfunc_hide() {
        this.remove_all_transitions();
        this.opacity = 0;
        super.vfunc_hide();
    }

    _updateMessageFile() {
        const file = _resolveMessageFile(this._settings);

        if (!file && !this._messageFile)
            return false;

        if (file && this._messageFile && this._messageFile.equal(file))
            return false;

        this._messageMonitor?.disconnectObject(this);
        this._messageMonitor = null;

        this._messageFile = file;

        if (file) {
            this._messageMonitor = file.monitor_file(Gio.FileMonitorFlags.NONE, null);
            this._messageMonitor.connectObject(
                'changed', () => this._update().catch(logError), this);
        }

        return true;
    }

    get enabled() {
        return this._settings.get_boolean(Settings.BANNER_MESSAGE_KEY);
    }

    _getTitleText() {
        return this._settings.get_string(Settings.BANNER_MESSAGE_TITLE_KEY);
    }

    _getBodyText() {
        return _readBannerBody(this._settings, this._messageFile);
    }

    async _update() {
        if (this._destroyed)
            return;

        const title = this._getTitleText();
        this._titleLabel.text = title;
        this._titleLabel.visible = !!title;

        const body = await this._getBodyText();
        if (this._destroyed)
            return;

        this._bodyLabel.text = body;
        this._bodyLabel.visible = !!body;

        this._syncVisibility();
    }
}

export class InlineBanner extends Banner {
    static {
        GObject.registerClass(this);
    }

    constructor(settings) {
        super(settings, {style_class: 'inline-banner'});

        this._update().catch(logError);
    }
}

export class AcknowledgementBanner extends Banner {
    static [GObject.signals] = {
        'acknowledged': {},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(settings) {
        super(settings, {
            style_class: 'acknowledgement-banner',
            accessible_role: Atk.Role.ALERT,
        });

        this._button = new St.Button({
            style_class: 'banner-button',
            button_mask: St.ButtonMask.PRIMARY | St.ButtonMask.SECONDARY,
            can_focus: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._button.connect('clicked', () => this.emit('acknowledged'));
        this.add_child(this._button);

        this._settings.connectObject(
            `changed::${Settings.BANNER_MESSAGE_BUTTON_KEY}`,
            () => this._update().catch(logError), this);

        this._update().catch(logError);
    }

    vfunc_show() {
        super.vfunc_show();
        this._button.grab_key_focus();
    }

    async _update() {
        this._button.label = this._settings.get_string(Settings.BANNER_MESSAGE_BUTTON_KEY) || _('OK');

        await super._update();
    }
}
