//
// A widget displayed instead of the unlock prompt
// when parental controls session limits are reached

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import St from 'gi://St';

export const ParentalControlsShield = GObject.registerClass(
class ParentalControlsShield extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'parental-controls-shield',
            orientation: Clutter.Orientation.VERTICAL,
            x_align: Clutter.ActorAlign.CENTER,
        });

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
    }
});
