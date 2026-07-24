import GObject from 'gi://GObject';
import St from 'gi://St';

export class LoginButton extends St.Button {
    static [GObject.properties] = {
        'label': GObject.ParamSpec.string(
            'label', null, null,
            GObject.ParamFlags.READWRITE,
            ''),
    };

    static {
        GObject.registerClass(this);
    }

    constructor(params) {
        super({
            style_class: 'login-button',
            button_mask: St.ButtonMask.PRIMARY | St.ButtonMask.SECONDARY,
            reactive: true,
            can_focus: true,
            child: new St.Label({style_class: 'login-button-label'}),
            ...params,
        });
    }

    set label(label) {
        this.child.text = label;
        this.accessible_name = label;
    }

    get label() {
        return this.child.text;
    }
}
