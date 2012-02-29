const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Lang = imports.lang;

const CheckBoxContainer = new Lang.Class({
    Name: 'CheckBoxContainer',

    _init: function() {
        this.actor = new Shell.GenericContainer();
        this.actor.connect('get-preferred-width',
                           Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height',
                           Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate',
                           Lang.bind(this, this._allocate));
        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                this._spacing = node.get_length('spacing');
            }));
        this.actor.request_mode = Clutter.RequestMode.HEIGHT_FOR_WIDTH;

        this._box = new St.Bin();
        this.actor.add_actor(this._box);

        this.label = new St.Label();
        this.label.clutter_text.set_line_wrap(true);
        this.label.clutter_text.set_ellipsize(Pango.EllipsizeMode.NONE);
        this.actor.add_actor(this.label);

        this._spacing = 0;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [minWidth, natWidth] = this._box.get_preferred_width(forHeight);

        alloc.min_size = minWidth + this._spacing;
        alloc.natural_size = natWidth + this._spacing;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [minBoxHeight, natBoxHeight] =
            this._box.get_preferred_height(forWidth);
        let [minLabelHeight, natLabelHeight] =
            this.label.get_preferred_height(forWidth);

        alloc.min_size = Math.max(minBoxHeight, minLabelHeight);
        alloc.natural_size = Math.max(natBoxHeight, natLabelHeight);
    },

    _allocate: function(actor, box, flags) {
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let childBox = new Clutter.ActorBox();
        let [minBoxWidth, natBoxWidth] =
            this._box.get_preferred_width(-1);
        let [minBoxHeight, natBoxHeight] =
            this._box.get_preferred_height(-1);
        childBox.x1 = box.x1;
        childBox.x2 = box.x1 + natBoxWidth;
        childBox.y1 = box.y1;
        childBox.y2 = box.y1 + natBoxHeight;
        this._box.allocate(childBox, flags);

        childBox.x1 = box.x1 + natBoxWidth + this._spacing;
        childBox.x2 = availWidth - childBox.x1;
        childBox.y1 = box.y1;
        childBox.y2 = box.y2;
        this.label.allocate(childBox, flags);
    }
});

const CheckBox = new Lang.Class({
    Name: 'CheckBox',

    _init: function(label) {
        this.actor = new St.Button({ style_class: 'check-box',
                                     button_mask: St.ButtonMask.ONE,
                                     toggle_mode: true,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });
        this._container = new CheckBoxContainer();
        this.actor.set_child(this._container.actor);

        if (label)
            this.setLabel(label);
    },

    setLabel: function(label) {
        this._container.label.set_text(label);
    },

    getLabelActor: function() {
        return this._container.label;
    }
});
