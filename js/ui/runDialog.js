/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Main = imports.ui.main;

const OVERLAY_COLOR = new Clutter.Color();
OVERLAY_COLOR.from_pixel(0x00000044);

const BOX_BACKGROUND_COLOR = new Clutter.Color();
BOX_BACKGROUND_COLOR.from_pixel(0x000000cc);

const BOX_TEXT_COLOR = new Clutter.Color();
BOX_TEXT_COLOR.from_pixel(0xffffffff);

const BOX_WIDTH = 320;
const BOX_HEIGHT = 56;

function RunDialog() {
    this._init();
};

RunDialog.prototype = {
    _init : function() {
        let global = Shell.Global.get();

        this._isOpen = false;

        this._internalCommands = { 'lg':
                                   Lang.bind(this, function() {
                                       Mainloop.idle_add(function() { Main.createLookingGlass().open(); });
                                   }),
                                   
                                   'restart': Lang.bind(this, function() {
                                       let global = Shell.Global.get();
                                       global.reexec_self();
                                   })
                                 };

        // All actors are inside _group. We create it initially
        // hidden then show it in show()
        this._group = new Clutter.Group({ visible: false });
        global.stage.add_actor(this._group);

        this._overlay = new Clutter.Rectangle({ color: OVERLAY_COLOR,
                                                width: global.screen_width,
                                                height: global.screen_height,
                                                border_width: 0,
                                                reactive: true });
        this._group.add_actor(this._overlay);

        let boxGroup = new Clutter.Group();
        boxGroup.set_position((global.screen_width - BOX_WIDTH) / 2,
                              (global.screen_height - BOX_HEIGHT) / 2);
        this._group.add_actor(boxGroup);

        let box = new Big.Box({ background_color: BOX_BACKGROUND_COLOR,
                                corner_radius: 4,
                                reactive: false,
                                width: BOX_WIDTH,
                                height: BOX_HEIGHT
                              });
        boxGroup.add_actor(box);

        let label = new Clutter.Text({ color: BOX_TEXT_COLOR,
                                       font_name: '18px Sans',
                                       text: _("Please enter a command:") });
        label.set_position(6, 6);
        boxGroup.add_actor(label);

        this._entry = new Clutter.Text({ color: BOX_TEXT_COLOR,
                                         font_name: '20px Sans Bold',
                                         editable: true,
                                         activatable: true,
                                         singleLineMode: true,
                                         text: '',
                                         width: BOX_WIDTH - 12,
                                         height: BOX_HEIGHT - 12 });
        // TODO: Implement relative positioning using Tidy.
        this._entry.set_position(6, 30);
        boxGroup.add_actor(this._entry);

        this._entry.connect('activate', Lang.bind(this, function (o, e) {
            this._run(o.get_text());
            this.close();
            return false;
        }));
        
        this._entry.connect('key-press-event', Lang.bind(this, function(o, e) {
            let symbol = Shell.get_event_key_symbol(e); 
            if (symbol == Clutter.Escape) {
                this.close();
                return true;
            }
            return false;
        }));
    },

    _run : function(command) {
        let f = this._internalCommands[command];
        if (f) {
            f();
        } else if (command) {
            var p = new Shell.Process({'args' : [command]});
            try {
                p.run();
            } catch (e) {
                // TODO: Give the user direct feedback.
                log('Could not run command ' + command + '.');
            }
        }
    },

    open : function() {
        if (this._isOpen) // Already shown
            return;

        if (!Main.startModal())
            return;
            
        this._isOpen = true;
        this._group.show();

        let global = Shell.Global.get();
        global.stage.set_key_focus(this._entry);
    },

    close : function() {
        if (!this._isOpen)
            return;

        this._isOpen = false;
        
        this._group.hide();
        this._entry.text = '';

        Main.endModal();
    }
};
Signals.addSignalMethods(RunDialog.prototype);
