/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;

const BOX_BACKGROUND_COLOR = new Clutter.Color();
BOX_BACKGROUND_COLOR.from_pixel(0x000000cc);

const BOX_TEXT_COLOR = new Clutter.Color();
BOX_TEXT_COLOR.from_pixel(0xffffffff);

const DIALOG_WIDTH = 320;
const DIALOG_PADDING = 6;
const ICON_SIZE = 24;
const ICON_BOX_SIZE = 36;

function RunDialog() {
    this._init();
};

RunDialog.prototype = {
    _init : function() {
        this._isOpen = false;

        let gconf = Shell.GConf.get_default();
        gconf.connect('changed', Lang.bind(this, function (gconf, key) {
            if (key == 'development_tools')
                this._enableInternalCommands = gconf.get_bool('development_tools');
        }));
        this._enableInternalCommands = gconf.get_boolean('development_tools');

        this._internalCommands = { 'lg':
                                   Lang.bind(this, function() {
                                       Main.createLookingGlass().open();
                                   }),

                                   'r': Lang.bind(this, function() {
                                       global.reexec_self();
                                   }),

                                   // Developer brain backwards compatibility
                                   'restart': Lang.bind(this, function() {
                                       global.reexec_self();
                                   }),

                                   'debugexit': Lang.bind(this, function() {
                                       Meta.exit(Meta.ExitCode.ERROR);
                                   })
                                 };

        // All actors are inside _group. We create it initially
        // hidden then show it in show()
        this._group = new Clutter.Group({ visible: false });
        global.stage.add_actor(this._group);

        this._lightbox = new Lightbox.Lightbox(this._group);

        let boxH = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                 x_align: Big.BoxAlignment.CENTER,
                                 y_align: Big.BoxAlignment.CENTER,
                                 width: global.screen_width,
                                 height: global.screen_height });

        this._group.add_actor(boxH);
        this._lightbox.highlight(boxH);

        let boxV = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                 y_align: Big.BoxAlignment.CENTER });

        boxH.append(boxV, Big.BoxPackFlags.NONE);


        let dialogBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      background_color: BOX_BACKGROUND_COLOR,
                                      corner_radius: 4,
                                      reactive: false,
                                      padding: DIALOG_PADDING,
                                      width: DIALOG_WIDTH });

        boxH.append(dialogBox, Big.BoxPackFlags.NONE);

        let label = new Clutter.Text({ color: BOX_TEXT_COLOR,
                                       font_name: '18px Sans',
                                       text: _("Please enter a command:") });

        dialogBox.append(label, Big.BoxPackFlags.EXPAND);

        this._entry = new Clutter.Text({ color: BOX_TEXT_COLOR,
                                         font_name: '20px Sans Bold',
                                         editable: true,
                                         activatable: true,
                                         singleLineMode: true });

        dialogBox.append(this._entry, Big.BoxPackFlags.EXPAND);

        this._errorBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                       padding_top: DIALOG_PADDING });

        dialogBox.append(this._errorBox, Big.BoxPackFlags.EXPAND);

        let iconBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    y_align: Big.BoxAlignment.CENTER,
                                    x_align: Big.BoxAlignment.CENTER,
                                    width: ICON_BOX_SIZE,
                                    height: ICON_BOX_SIZE });

        this._errorBox.append(iconBox, Big.BoxPackFlags.NONE);

        this._commandError = false;

        let errorIcon = Shell.TextureCache.get_default().load_icon_name("gtk-dialog-error", ICON_SIZE);
        iconBox.append(errorIcon, Big.BoxPackFlags.EXPAND);

        this._errorMessage = new Clutter.Text({ color: BOX_TEXT_COLOR,
                                                font_name: '18px Sans Bold',
                                                line_wrap: true });

        this._errorBox.append(this._errorMessage, Big.BoxPackFlags.EXPAND);

        this._errorBox.hide();

        this._entry.connect('activate', Lang.bind(this, function (o, e) {
            this._run(o.get_text());
            if (!this._commandError)
                this.close();
        }));

        this._entry.connect('key-press-event', Lang.bind(this, function(o, e) {
            let symbol = e.get_key_symbol();
            if (symbol == Clutter.Escape) {
                this.close();
                return true;
            }
            return false;
        }));
    },

    _run : function(command) {
        let f;
        if (this._enableInternalCommands)
            f = this._internalCommands[command];
        else
            f = null;
        if (f) {
            f();
        } else if (command) {
            try {
                this._commandError = false;
                let [ok, len, args] = GLib.shell_parse_argv(command);
                let p = new Shell.Process({'args' : args});
                p.run();
            } catch (e) {
                this._commandError = true;
                /*
                 * The exception contains an error string like:
                 * Error invoking Shell.run: Failed to execute child process "foo"
                 * (No such file or directory)
                 * We are only interested in the actual error, so parse that out.
                 */
                let m = /.+\((.+)\)/.exec(e);
                let errorStr = "Execution of '" + command + "' failed:\n" + m[1];
                this._errorMessage.set_text(errorStr);
                this._errorBox.show();
            }
        }
    },

    open : function() {
        if (this._isOpen) // Already shown
            return;

        if (!Main.pushModal(this._group))
            return;

        this._isOpen = true;
        this._group.show();

        global.stage.set_key_focus(this._entry);
    },

    close : function() {
        if (!this._isOpen)
            return;

        this._isOpen = false;
        
        this._errorBox.hide();
        this._commandError = false;

        this._group.hide();
        this._entry.text = '';

        Main.popModal(this._group);
    }
};
Signals.addSignalMethods(RunDialog.prototype);
