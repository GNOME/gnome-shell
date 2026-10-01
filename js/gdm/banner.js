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

export class Content extends GObject.Object {
    static [GObject.properties] = {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'title-text': GObject.ParamSpec.string(
            'title-text', null, null,
            GObject.ParamFlags.READWRITE,
            ''),
        'body-text': GObject.ParamSpec.string(
            'body-text', null, null,
            GObject.ParamFlags.READWRITE,
            ''),
    };

    static {
        GObject.registerClass(this);
    }

    constructor(settings) {
        super();

        this._settings = settings;

        this.enabled = this._settings.get_boolean(Settings.BANNER_MESSAGE_KEY);
        this.titleText = this._settings.get_string(Settings.BANNER_MESSAGE_TITLE_KEY);
        this.bodyText = '';

        this._settings.connectObject(
            `changed::${Settings.BANNER_MESSAGE_KEY}`, () => this._onEnabledChanged(),
            `changed::${Settings.BANNER_MESSAGE_TITLE_KEY}`, () => this._onTitleTextChanged(),
            `changed::${Settings.BANNER_MESSAGE_TEXT_KEY}`, () => this._updateBodyText().catch(logError),
            `changed::${Settings.BANNER_MESSAGE_SOURCE_KEY}`, () => this._onMessageFileChanged(),
            `changed::${Settings.BANNER_MESSAGE_PATH_KEY}`, () => this._onMessageFileChanged(),
            this);

        this._updateMessageFile();
        this._updateBodyText().catch(logError);
    }

    _onEnabledChanged() {
        this.enabled = this._settings.get_boolean(Settings.BANNER_MESSAGE_KEY);
    }

    _onTitleTextChanged() {
        this.titleText = this._settings.get_string(Settings.BANNER_MESSAGE_TITLE_KEY);
    }

    _onMessageFileChanged() {
        if (this._updateMessageFile())
            this._updateBodyText().catch(logError);
    }

    _updateMessageFile() {
        const file = this._resolveMessageFile();

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
                'changed', () => this._updateBodyText().catch(logError), this);
        }

        return true;
    }

    _resolveMessageFile() {
        if (this._settings.get_string(Settings.BANNER_MESSAGE_SOURCE_KEY) !== 'file')
            return null;

        const path = this._settings.get_string(Settings.BANNER_MESSAGE_PATH_KEY);
        return path ? Gio.File.new_for_path(path) : null;
    }

    async _updateBodyText() {
        this._bodyTextUpdated = this._readBannerBody();

        this.bodyText = await this._bodyTextUpdated;
    }

    async _readBannerBody() {
        if (this._messageFile) {
            try {
                const [contents] = await this._messageFile.load_contents_async(null);
                return new TextDecoder().decode(contents);
            } catch (e) {
                console.error(`Failed to read banner from ${this._messageFile.get_path()}: ${e.message}`);
                return '';
            }
        }

        return this._settings.get_string(Settings.BANNER_MESSAGE_TEXT_KEY);
    }
}

export class Banner extends St.BoxLayout {
    static {
        GObject.registerClass(this);
    }

    constructor(content) {
        super({
            style_class: 'banner',
            orientation: Clutter.Orientation.VERTICAL,
            opacity: 0,
            visible: false,
            accessible_role: Atk.Role.ALERT,
        });

        this._content = content;
        this._opened = false;

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

        this._content.connectObject(
            'notify::enabled', () => {
                this.enabled = this._content.enabled;
                this._syncVisibility();
            }, this);
        this.enabled = this._content.enabled;

        this._content.bind_property_full('title-text', this._titleLabel, 'text',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, titleText) => {
                this._titleLabel.visible = !!titleText;
                return [true, titleText];
            }, null);

        this._content.bind_property_full('body-text', this._bodyLabel, 'text',
            GObject.BindingFlags.SYNC_CREATE,
            (_bind, bodyText) => {
                this._bodyLabel.visible = !!bodyText;
                return [true, bodyText];
            }, null);

        this._syncVisibility();
    }

    open() {
        this._opened = true;
        this._syncVisibility();
    }

    close() {
        this._opened = false;
        this._syncVisibility();
    }

    _syncVisibility() {
        if (this._opened && this.enabled)
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
}
