/* exported CheckBox */
const { Atk, Clutter, GObject, Pango, St } = imports.gi;

var CheckBox = GObject.registerClass(
class CheckBox extends St.Button {
    _init(label) {
        let container = new St.BoxLayout({
            x_expand: true,
            y_expand: true,
        });
        super._init({
            style_class: 'check-box',
            child: container,
            button_mask: St.ButtonMask.ONE,
            toggle_mode: true,
            can_focus: true,
        });
        this.set_accessible_role(Atk.Role.CHECK_BOX);

        this._box = new St.Bin({ y_align: Clutter.ActorAlign.START });
        container.add_actor(this._box);

        this._label = new St.Label({ y_align: Clutter.ActorAlign.CENTER });
        this._label.clutter_text.set_line_wrap(true);
        this._label.clutter_text.set_ellipsize(Pango.EllipsizeMode.NONE);
        this.set_label_actor(this._label);
        container.add_actor(this._label);

        if (label)
            this.setLabel(label);
    }

    setLabel(label) {
        this._label.set_text(label);
    }

    getLabelActor() {
        return this._label;
    }
});
