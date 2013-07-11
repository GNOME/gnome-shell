const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const St = imports.gi.St;

const Lang = imports.lang;

const CheckBox = new Lang.Class({
    Name: 'CheckBox',

    _init: function(label) {
        let container = new St.BoxLayout();
        this.actor = new St.Button({ style_class: 'check-box',
                                     child: container,
                                     button_mask: St.ButtonMask.ONE,
                                     toggle_mode: true,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });

        this._box = new St.Bin();
        this._box.set_y_align(Clutter.ActorAlign.START);
        container.add_actor(this._box);

        this._label = new St.Label();
        this._label.clutter_text.set_line_wrap(true);
        this._label.clutter_text.set_ellipsize(Pango.EllipsizeMode.NONE);
        container.add_actor(this._label);

        if (label)
            this.setLabel(label);
    },

    setLabel: function(label) {
        this._label.set_text(label);
    },

    getLabelActor: function() {
        return this._label;
    }
});
