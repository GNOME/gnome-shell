// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GdkPixbuf = imports.gi.GdkPixbuf;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;

const IconGrid = imports.ui.iconGrid;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Panel = imports.ui.panel;

const FISH_NAME = 'wanda';
const FISH_FILENAME = 'wanda.png';
const FISH_SPEED = 300;
const FISH_COMMAND = 'fortune';
// The size of an individual frame in the animation
const FISH_HEIGHT = 22;
const FISH_WIDTH = 36;

const FISH_GROUP = 'Fish Animation';

const MAGIC_FISH_KEY = 'free the fish';

const WandaIcon = new Lang.Class({
    Name: 'WandaIcon',
    Extends: IconGrid.BaseIcon,

    _init : function(fish, label, params) {
        this.parent(label, params);

        this._fish = fish;
        this._imageFile = GLib.build_filenamev([global.datadir, fish + '.png']);

        this._imgHeight = FISH_HEIGHT;
        this._imgWidth = FISH_WIDTH;
    },

    createIcon: function(iconSize) {
        this._animations = new Panel.Animation(this._imageFile, this._imgWidth, this._imgHeight, FISH_SPEED);
        this._animations.play();
        return this._animations.actor;
    },

    _createIconTexture: function(size) {
        if (size == this.iconSize)
            return;

        this.parent(size);
    }
});

const WandaIconBin = new Lang.Class({
    Name: 'WandaIconBin',

    _init: function(fish, label, params) {
        this.actor = new St.Bin({ reactive: true,
                                  track_hover: true });
        this.icon = new WandaIcon(fish, label, params);

        this.actor.child = this.icon.actor;
        this.actor.label_actor = this.icon.label;
    },
});

const FortuneDialog = new Lang.Class({
    Name: 'FortuneDialog',

    _init: function(name, command) {
        let text;

        try {
            let [res, stdout, stderr, status] = GLib.spawn_command_line_sync(command);
            text = String.fromCharCode.apply(null, stdout);
        } catch(e) {
            text = _("Sorry, no wisdom for you today:\n%s").format(e.message);
        }

        this._title = new St.Label({ style_class: 'prompt-dialog-headline',
                                     text: _("%s the Oracle says").format(name) });
        this._label = new St.Label({ style_class: 'prompt-dialog-description',
                                     text: text });
        this._label.clutter_text.line_wrap = true;

        this._box = new St.BoxLayout({ vertical: true,
                                       style_class: 'prompt-dialog' // this is just to force a reasonable width
                                     });
        this._box.add(this._title, { align: St.Align.MIDDLE });
        this._box.add(this._label, { expand: true });

        this._button = new St.Button({ button_mask: St.ButtonMask.ONE,
                                       style_class: 'modal-dialog',
                                       reactive: true });
        this._button.connect('clicked', Lang.bind(this, this.destroy));
        this._button.child = this._box;

        this._bin = new St.Bin({ x_align: St.Align.MIDDLE,
                                 y_align: St.Align.MIDDLE });
        this._bin.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        this._bin.add_actor(this._button);

        Main.layoutManager.addChrome(this._bin);

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, Lang.bind(this, this.destroy));
    },

    destroy: function() {
        this._bin.destroy();
    }
});

function capitalize(str) {
    return str[0].toUpperCase() + str.substring(1, str.length);
}

const WandaSearchProvider = new Lang.Class({
    Name: 'WandaSearchProvider',

    _init: function() {
        this.id = 'wanda';
    },

    getResultMetas: function(fish, callback) {
        callback([{ 'id': fish[0], // there may be many fish in the sea, but
                    // only one which speaks the truth!
                    'name': capitalize(fish[0]),
                    'createIcon': function(iconSize) {
                        return new St.Icon({ gicon: Gio.icon_new_for_string('face-smile'),
                                             icon_size: iconSize });
                    }
                  }]);
    },

    filterResults: function(results) {
        return results;
    },

    getInitialResultSet: function(terms) {
        if (terms.join(' ') == MAGIC_FISH_KEY) {
            this.searchSystem.setResults(this, [ FISH_NAME ]);
        } else {
            this.searchSystem.setResults(this, []);
        }
    },

    getSubsearchResultSet: function(previousResults, terms) {
        this.getInitialResultSet(terms);
    },

    activateResult: function(fish) {
        if (this._dialog)
            this._dialog.destroy();
        this._dialog = new FortuneDialog(capitalize(fish), FISH_COMMAND);
    },

    createResultObject: function (resultMeta, terms) {
        return new WandaIconBin(resultMeta.id, resultMeta.name);
    }
});
