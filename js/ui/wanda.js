// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Search = imports.ui.search;

// we could make these gsettings
const FISH_NAME = 'wanda';
const FISH_SPEED = 300;
const FISH_COMMAND = 'fortune';

const GNOME_PANEL_PIXMAPDIR = '../gnome-panel/fish';
const FISH_GROUP = 'Fish Animation';

const MAGIC_FISH_KEY = 'free the fish';

const WandaIcon = new Lang.Class({
    Name: 'WandaIcon',
    Extends: IconGrid.BaseIcon,

    _init : function(fish, label, params) {
        this._fish = fish;
        let file = GLib.build_filenamev([global.datadir, GNOME_PANEL_PIXMAPDIR, fish + '.fish']);

        if (GLib.file_test(file, GLib.FileTest.EXISTS)) {
            this._keyfile = new GLib.KeyFile();
            this._keyfile.load_from_file(file, GLib.KeyFileFlags.NONE);

            this._imageFile = GLib.build_filenamev([global.datadir, GNOME_PANEL_PIXMAPDIR,
                                                    this._keyfile.get_string(FISH_GROUP, 'image')]);

            let tmpPixbuf = GdkPixbuf.Pixbuf.new_from_file(this._imageFile);

            this._imgHeight = tmpPixbuf.height;
            this._imgWidth = tmpPixbuf.width / this._keyfile.get_integer(FISH_GROUP, 'frames');
        } else {
            this._imageFile = null;
        }

        this.parent(label, params);
    },

    createIcon: function(iconSize) {
        if (this._animations)
            this._animations.destroy();

        if (!this._imageFile) {
            return new St.Icon({ icon_name: 'face-smile',
                                 icon_type: St.IconType.FULLCOLOR,
                                 icon_size: iconSize
                               });
        }

        this._animations = St.TextureCache.get_default().load_sliced_image(this._imageFile, this._imgWidth, this._imgHeight);
        this._animations.connect('destroy', Lang.bind(this, function() {
            if (this._timeoutId)
                GLib.source_remove(this._timeoutId);
            this._timeoutId = 0;
            this._animations = null;
        }));
        this._animations.connect('notify::mapped', Lang.bind(this, function() {
            if (this._animations.mapped && !this._timeoutId) {
                this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, FISH_SPEED, Lang.bind(this, this._update));

                this._i = 0;
                this._update();
            } else if (!this._animations.mapped && this._timeoutId) {
                GLib.source_remove(this._timeoutId);
                this._timeoutId = 0;
            }
        }));

        this._i = 0;

        return this._animations;
    },

    _update: function() {
        let n = this._animations.get_n_children();
        if (n == 0) {
            return true;
        }

        this._animations.get_child_at_index(this._i).hide();
        this._i = (this._i + 1) % n;
        this._animations.get_child_at_index(this._i).show();

        return true;
    },
});

const WandaIconBin = new Lang.Class({
    Name: 'WandaIconBin',

    _init: function(fish, label, params) {
        this.actor = new St.Bin({ style_class: 'search-result-content',
                                  reactive: true,
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

        let monitor = Main.layoutManager.primaryMonitor;

        Main.layoutManager.addChrome(this._button);
        this._button.set_position(Math.floor(monitor.width / 2 - this._button.width / 2),
                                  Math.floor(monitor.height / 2 - this._button.height / 2));

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, Lang.bind(this, this.destroy));
    },

    destroy: function() {
        this._button.destroy();
    }
});

function capitalize(str) {
    return str[0].toUpperCase() + str.substring(1, str.length);
}

const WandaSearchProvider = new Lang.Class({
    Name: 'WandaSearchProvider',
    Extends: Search.SearchProvider,

    _init: function() {
        this.parent(_("Your favorite Easter Egg"));
    },

    getResultMetas: function(fish) {
        return [{ 'id': fish[0], // there may be many fish in the sea, but
                                 // only one which speaks the truth!
                  'name': capitalize(fish[0]),
                  'createIcon': function(iconSize) {
                      // for DND only (maybe could be improved)
                      // DON'T use St.Icon here, it crashes the shell
                      // (dnd.js code assumes it can query the actor size
                      // without parenting it, while StWidget accesses
                      // StThemeNode in get_preferred_width/height, which
                      // triggers an assertion failure)
                      return St.TextureCache.get_default().load_icon_name(null,
                                                                          'face-smile',
                                                                          St.IconType.FULLCOLOR,
                                                                          iconSize);
                  }
                }];
    },

    getInitialResultSet: function(terms) {
        if (terms.join(' ') == MAGIC_FISH_KEY) {
            return [ FISH_NAME ];
        }
        return [];
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this.getInitialResultSet(terms);
    },

    activateResult: function(fish, params) {
        if (this._dialog)
            this._dialog.destroy();
        this._dialog = new FortuneDialog(capitalize(fish), FISH_COMMAND);
    },

    createResultActor: function (resultMeta, terms) {
        let icon = new WandaIconBin(resultMeta.id, resultMeta.name);
        return icon.actor;
    }
});
