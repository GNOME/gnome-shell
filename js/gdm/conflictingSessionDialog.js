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

import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import GObject from 'gi://GObject';
import Pango from 'gi://Pango';
import St from 'gi://St';

import {Spinner} from '../ui/animation.js';
import {ModalDialog} from '../ui/modalDialog.js';

const SPINNER_SIZE = 32;
const DIM_BRIGHTNESS = 0.7;
const DIM_BRIGHTNESS_NAME = 'conflicting-session-dialog-dim';
const SPINNER_DELAY = 3000;
const SPINNER_ANIMATION_TIME = 200;

export class ConflictingSessionDialog extends ModalDialog {
    static [GObject.signals] = {
        'cancel': {},
        'force-stop': {},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(conflictingSession, greeterSession) {
        super();

        const userName = conflictingSession.Name;
        let bannerText;
        if (greeterSession.Remote && conflictingSession.Remote)
            /* Translators: is running for <username> */
            bannerText = _('Remote login is not possible because a remote session is already running for %s. To login remotely, you must log out from the remote session or force stop it.').format(userName);
        else if (!greeterSession.Remote && conflictingSession.Remote)
            /* Translators: is running for <username> */
            bannerText = _('Login is not possible because a remote session is already running for %s. To login, you must log out from the remote session or force stop it.').format(userName);
        else if (greeterSession.Remote && !conflictingSession.Remote)
            /* Translators: is running for <username> */
            bannerText = _('Remote login is not possible because a local session is already running for %s. To login remotely, you must log out from the local session or force stop it.').format(userName);
        else
            /* Translators: is running for <username> */
            bannerText = _('Login is not possible because a session is already running for %s. To login, you must log out from the session or force stop it.').format(userName);

        const textLayout = new St.BoxLayout({
            style_class: 'conflicting-session-dialog-content',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });

        const title = new St.Label({
            text: _('Session Already Running'),
            style_class: 'conflicting-session-dialog-title',
        });

        const banner = new St.Label({
            text: bannerText,
            style_class: 'conflicting-session-dialog-desc',
        });
        banner.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        banner.clutter_text.line_wrap = true;

        const warningBanner = new St.Label({
            text: _('Force stopping will quit any running apps and processes, and could result in data loss'),
            style_class: 'conflicting-session-dialog-desc-warning',
        });
        warningBanner.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        warningBanner.clutter_text.line_wrap = true;

        textLayout.add_child(title);
        textLayout.add_child(banner);
        textLayout.add_child(warningBanner);
        this.contentLayout.add_child(textLayout);

        this._createSpinner();

        this._cancelButton = this.addButton({
            label: _('Cancel'),
            action: () => this.emit('cancel'),
            key: Clutter.KEY_Escape,
            default: true,
        });

        this._forceStopButton = this.addButton({
            label: _('Force Stop'),
            action: () => {
                this._startSpinning();
                this.emit('force-stop');
            },
        });
    }

    _createSpinner() {
        this._dimEffect = new Clutter.BrightnessContrastEffect({
            name: DIM_BRIGHTNESS_NAME,
            enabled: false,
        });
        this.dialogLayout.add_effect(this._dimEffect);

        this._spinner = new Spinner(SPINNER_SIZE, {hideOnStop: true});
        this._spinner.add_constraint(new Clutter.BindConstraint({
            source: this.dialogLayout,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._spinner.add_constraint(new Clutter.AlignConstraint({
            source: this.dialogLayout,
            align_axis: Clutter.AlignAxis.BOTH,
            factor: 0.5,
        }));
        this.backgroundStack.add_child(this._spinner);
    }

    _startSpinning() {
        this.popModal();

        this._cancelButton.reactive = false;
        this._cancelButton.can_focus = false;
        this._forceStopButton.reactive = false;
        this._forceStopButton.can_focus = false;

        this._spinner.play();
        this._spinner.opacity = 0;
        this._spinner.ease({
            opacity: 255,
            delay: SPINNER_DELAY,
            duration: SPINNER_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this._animateDimEffect();
    }

    _animateDimEffect() {
        const val = 127 * DIM_BRIGHTNESS;
        const color = new Cogl.Color({
            red: val,
            green: val,
            blue: val,
            alpha: 255,
        });
        this._dimEffect.actor.ease_property(`@effects.${DIM_BRIGHTNESS_NAME}.brightness`, color, {
            delay: SPINNER_DELAY,
            duration: SPINNER_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
        this._dimEffect.set_enabled(true);
    }
}
