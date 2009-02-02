/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Button = imports.ui.button;
const Main = imports.ui.main;

const PANEL_HEIGHT = 32;
const TRAY_HEIGHT = 24;
const PANEL_BACKGROUND_COLOR = new Clutter.Color();
PANEL_BACKGROUND_COLOR.from_pixel(0xeeddccff);
const PANEL_BORDER_COLOR = new Clutter.Color();
PANEL_BORDER_COLOR.from_pixel(0x000000ff);
const PRESSED_BUTTON_BACKGROUND_COLOR = new Clutter.Color();
PRESSED_BUTTON_BACKGROUND_COLOR.from_pixel(0xccbbaaff);

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
        let me = this;
        let global = Shell.Global.get();

        this._box = new Big.Box({ background_color: PANEL_BACKGROUND_COLOR,
                                  x: 0,
                                  y: 0,
                                  height: PANEL_HEIGHT + 1,
                                  width: global.screen_width,
                                  orientation: Big.BoxOrientation.HORIZONTAL,
                                  spacing: 4,
                                  border_bottom: 1,
                                  border_color: PANEL_BORDER_COLOR });

        this.button = new Button.Button("Activities", PANEL_BACKGROUND_COLOR, PRESSED_BUTTON_BACKGROUND_COLOR, true, null, PANEL_HEIGHT);

        this._box.append(this.button.button, Big.BoxPackFlags.NONE);

        this._clock = new Clutter.Label({ font_name: "Sans Bold 16px",
                                          text: "" });
        let pad = (PANEL_HEIGHT - this._clock.height) / 2;
        let clockbox = new Big.Box({ padding_top: pad,
                                     padding_bottom: pad,
                                     padding_right: 4 });
        clockbox.append(this._clock, Big.BoxPackFlags.NONE);
        this._box.append(clockbox, Big.BoxPackFlags.END);

        this._traymanager = new Shell.TrayManager({ bg_color: PANEL_BACKGROUND_COLOR });
        this._traymanager.connect('tray-icon-added',
            function(o, icon) {
                let pad = (PANEL_HEIGHT - icon.height) / 2;
                icon._panel_box = new Big.Box({ padding_top: pad,
                                                padding_bottom: pad });
                icon._panel_box.append(icon, Big.BoxPackFlags.NONE);
                me._box.append(icon._panel_box, Big.BoxPackFlags.END);
            });
        this._traymanager.connect('tray-icon-removed',
            function(o, icon) {
                me._box.remove_actor(icon._panel_box);
            });
        this._traymanager.manage_stage(global.stage);

        // TODO: decide what to do with the rest of the panel in the overlay mode (make it fade-out, become non-reactive, etc.)
        // We get into the overlay mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the overlay act like a menu that allows the user to release the mouse on the activity the user wants
        // to switch to.
        this.button.button.connect('button-press-event',
            function(o, event) {
                if (Main.overlay.visible)
                    Main.hide_overlay();
                else
                    Main.show_overlay();

                return true;
            });

        this._setStruts();
        global.screen.connect('notify::n-workspaces',
            function() {
                me._setStruts();
            });

        global.stage.add_actor(this._box);

        // Start the clock
        this._updateClock();
    },

    // Struts determine the area along each side of the screen that is reserved
    // and not available to applications
    _setStruts: function() {
        let global = Shell.Global.get();

        let struts = [
            new Meta.Strut({
                rect: {
                    x: 0,
                    y: 0,
                    width: global.screen_width,
                    height: PANEL_HEIGHT
                },
                side: Meta.Direction.TOP
            })
        ];

        let screen = global.screen;
        for (let i = 0; i < screen.n_workspaces; i++) {
            let workspace = screen.get_workspace_by_index(i);
            workspace.set_builtin_struts(struts);
        }
    },

    _updateClock: function() {
        let me = this;
        let display_date = new Date();
        let msec_remaining = 60000 - (1000 * display_date.getSeconds() +
                                      display_date.getMilliseconds());
        if (msec_remaining < 500) {
            display_date.setMinutes(display_date.getMinutes() + 1);
            msec_remaining += 60000;
        }
        this._clock.set_text(display_date.toLocaleFormat("%H:%M"));
        Mainloop.timeout_add(msec_remaining, function() {
            me._updateClock();
            return false;
        });
    },

    overlayHidden: function() {
        this.button.release();
    }
};
