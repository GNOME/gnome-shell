// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Tooltip utility for GDM components
 */

import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as Params from '../misc/params.js';

const TOOLTIP_SHOW_DELAY = 300;
const TOOLTIP_SHOW_DURATION = 150;
const TOOLTIP_HIDE_DURATION = 100;

/** @enum {number} */
export const Position = {
    TOP: 0,
    BOTTOM: 1,
    LEFT: 2,
    RIGHT: 3,
};

export const Tooltip = GObject.registerClass(
class Tooltip extends St.Widget {
    _init(widget, params) {
        params = Params.parse(params, {
            text: '',
            customLabel: null,
            style_class: '',
            position: Position.TOP,
            delay: TOOLTIP_SHOW_DELAY,
            showWhenFocused: false,
            showWhenHovered: false,
        });

        super._init({
            style_class: params.style_class,
            visible: false,
            layout_manager: new Clutter.BinLayout(),
            y_align: Clutter.ActorAlign.CENTER,
            x_align: Clutter.ActorAlign.CENTER,
        });

        if (params.customLabel)
            this.add_child(params.customLabel);
        else
            this.add_child(new St.Label({text: params.text}));

        this._widget = widget;
        this._showTimeoutId = 0;
        this._position = params.position;
        this._delay = params.delay;
        this._showWhenFocused = params.showWhenFocused;
        this._showWhenHovered = params.showWhenHovered;

        global.stage.add_child(this);

        if (this._showWhenFocused) {
            this._widget.connect('key-focus-in', () => this._updateVisibility());
            this._widget.connect('key-focus-out', () => this._updateVisibility());
        }
        if (this._showWhenHovered)
            this._widget.connect('notify::hover', () => this._updateVisibility());

        this._widget.connect('clicked', () => this.close());

        this._widget.connect('destroy', () => {
            this.close();
            this.destroy();
        });
    }

    _updatePosition() {
        const extents = this._widget.get_transformed_extents();
        const node = this.get_theme_node();
        const offsetX = node.get_length('-x-offset');
        const offsetY = node.get_length('-y-offset');

        let x, y;

        switch (this._position) {
        case Position.TOP:
            x = Math.clamp(
                extents.get_x() + Math.floor((extents.get_width() - this.width) / 2) + offsetX,
                0,
                global.stage.width - this.width
            );
            y = extents.get_y() - this.height - offsetY;
            break;

        case Position.BOTTOM:
            x = Math.clamp(
                extents.get_x() + Math.floor((extents.get_width() - this.width) / 2) + offsetX,
                0,
                global.stage.width - this.width
            );
            y = extents.get_y() + extents.get_height() + offsetY;
            break;

        case Position.LEFT:
            x = extents.get_x() - this.width - offsetX;
            y = Math.clamp(
                extents.get_y() + Math.floor((extents.get_height() - this.height) / 2) + offsetY,
                0,
                global.stage.height - this.height
            );
            break;

        case Position.RIGHT:
            x = extents.get_x() + extents.get_width() + offsetX;
            y = Math.clamp(
                extents.get_y() + Math.floor((extents.get_height() - this.height) / 2) + offsetY,
                0,
                global.stage.height - this.height
            );
            break;
        }

        this.set_position(x, y);
    }

    _updateVisibility() {
        let shouldShow = this._showWhenHovered || this._showWhenFocused;

        if (this._showWhenHovered)
            shouldShow = shouldShow && this._widget.hover;

        if (this._showWhenFocused)
            shouldShow = shouldShow && this._widget.has_key_focus();

        if (shouldShow)
            this.open();
        else
            this.close();
    }

    open() {
        if (this._showTimeoutId)
            return;

        this._showTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, this._delay, () => {
            this.opacity = 0;
            this.show();

            this._updatePosition();

            this.ease({
                opacity: 255,
                duration: TOOLTIP_SHOW_DURATION,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });

            this._showTimeoutId = 0;
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(this._showTimeoutId, '[gnome-shell] tooltip.open');
    }

    close() {
        if (this._showTimeoutId) {
            GLib.source_remove(this._showTimeoutId);
            this._showTimeoutId = null;
            return;
        }

        if (!this.visible)
            return;

        this.remove_all_transitions();
        this.ease({
            opacity: 0,
            duration: TOOLTIP_HIDE_DURATION,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this.hide(),
        });
    }
});
