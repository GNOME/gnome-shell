/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Button = imports.ui.button;
const Main = imports.ui.main;

const PANEL_HEIGHT = 32;
const TRAY_HEIGHT = 28;
const SHADOW_HEIGHT = 6;

// The panel has a transparent white background with a gradient.
const PANEL_TOP_COLOR = new Clutter.Color();
PANEL_TOP_COLOR.from_pixel(0xffffff99);
const PANEL_MIDDLE_COLOR = new Clutter.Color();
PANEL_MIDDLE_COLOR.from_pixel(0xffffff88);
const PANEL_BOTTOM_COLOR = new Clutter.Color();
PANEL_BOTTOM_COLOR.from_pixel(0xffffffaa);

const SHADOW_COLOR = new Clutter.Color();
SHADOW_COLOR.from_pixel(0x00000033);
const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

// Darken (pressed) buttons; lightening has no effect on white backgrounds.
const PANEL_BUTTON_COLOR = new Clutter.Color();
PANEL_BUTTON_COLOR.from_pixel(0x00000015);
const PRESSED_BUTTON_BACKGROUND_COLOR = new Clutter.Color();
PRESSED_BUTTON_BACKGROUND_COLOR.from_pixel(0x00000030);

const TRAY_BACKGROUND_COLOR = new Clutter.Color();
TRAY_BACKGROUND_COLOR.from_pixel(0xefefefff);
const TRAY_BORDER_COLOR = new Clutter.Color();
TRAY_BORDER_COLOR.from_pixel(0x00000033);
const TRAY_CORNER_RADIUS = 5;
const TRAY_BORDER_WIDTH = 1;
const TRAY_PADDING = 2;
const TRAY_SPACING = 2;

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
        let global = Shell.Global.get();

        // Put the background under the panel within a group.
        this.actor = new Clutter.Group();

        // Create backBox, which contains two boxes, backUpper and backLower,
        // for the background gradients and one for the shadow. The shadow at
        // the bottom has a fixed height (packing flag NONE), and the rest of
        // the height above is divided evenly between backUpper and backLower
        // (with packing flag EXPAND).
        let backBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    x: 0,
                                    y: 0,
                                    width: global.screen_width,
                                    height: PANEL_HEIGHT + SHADOW_HEIGHT });
        let backUpper = global.create_vertical_gradient(PANEL_TOP_COLOR,
                                                        PANEL_MIDDLE_COLOR);
        let backLower = global.create_vertical_gradient(PANEL_MIDDLE_COLOR,
                                                        PANEL_BOTTOM_COLOR);
        let shadow = global.create_vertical_gradient(SHADOW_COLOR,
                                                     TRANSPARENT_COLOR);
        shadow.set_height(SHADOW_HEIGHT + 1);
        backBox.append(backUpper, Big.BoxPackFlags.EXPAND);
        backBox.append(backLower, Big.BoxPackFlags.EXPAND);
        backBox.append(shadow, Big.BoxPackFlags.NONE);
        this.actor.add_actor(backBox);

        let box = new Big.Box({ x: 0,
                                y: 0,
                                height: PANEL_HEIGHT,
                                width: global.screen_width,
                                orientation: Big.BoxOrientation.HORIZONTAL,
                                spacing: 4 });

        this.button = new Button.Button("Activities", PANEL_BUTTON_COLOR, PRESSED_BUTTON_BACKGROUND_COLOR, true, null, PANEL_HEIGHT);

        box.append(this.button.button, Big.BoxPackFlags.NONE);

        let statusbox = new Big.Box();
        let statusmenu = this._statusmenu = new Shell.StatusMenu();
        statusbox.append(this._statusmenu, Big.BoxPackFlags.NONE);
        let statusbutton = new Button.Button(statusbox, PANEL_BUTTON_COLOR, PRESSED_BUTTON_BACKGROUND_COLOR,
                                             true, null, PANEL_HEIGHT);
        statusbutton.button.connect('button-press-event', function (b, e) {
            statusmenu.toggle(e);
            return false;
        });
        box.append(statusbutton.button, Big.BoxPackFlags.END);
        // We get a deactivated event when the popup disappears
        this._statusmenu.connect('deactivated', function (sm) {
            statusbutton.release();
        });

        this._clock = new Clutter.Text({ font_name: "Sans Bold 16px",
                                         text: "" });
        let pad = (PANEL_HEIGHT - this._clock.height) / 2;
        let clockbox = new Big.Box({ padding_top: pad,
                                     padding_bottom: pad,
                                     padding_left: 4,
                                     padding_right: 4 });
        clockbox.append(this._clock, Big.BoxPackFlags.NONE);
        box.append(clockbox, Big.BoxPackFlags.END);

        // The tray icons live in trayBox within trayContainer.
        // With Gtk 2.16, we can also use a transparent background for this.
        // The trayBox is hidden when there are no tray icons.
        let trayContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          y_align: Big.BoxAlignment.CENTER });
        box.append(trayContainer, Big.BoxPackFlags.END);
        let trayBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    height: TRAY_HEIGHT,
                                    background_color: TRAY_BACKGROUND_COLOR,
                                    corner_radius: TRAY_CORNER_RADIUS,
                                    border: TRAY_BORDER_WIDTH,
                                    border_color: TRAY_BORDER_COLOR,
                                    padding: TRAY_PADDING,
                                    spacing: TRAY_SPACING });
        trayBox.hide();
        trayContainer.append(trayBox, Big.BoxPackFlags.NONE);

        this._traymanager = new Shell.TrayManager({ bg_color: TRAY_BACKGROUND_COLOR });
        this._traymanager.connect('tray-icon-added',
            function(o, icon) {
                trayBox.append(icon, Big.BoxPackFlags.NONE);

                // Make sure the trayBox is shown.
                trayBox.show();
            });
        this._traymanager.connect('tray-icon-removed',
            function(o, icon) {
                trayBox.remove_actor(icon);

                if (trayBox.get_children().length == 0)
                    trayBox.hide();
            });
        this._traymanager.manage_stage(global.stage);

        // TODO: decide what to do with the rest of the panel in the overlay mode (make it fade-out, become non-reactive, etc.)
        // We get into the overlay mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the overlay act like a menu that allows the user to release the mouse on the activity the user wants
        // to switch to.
        this.button.button.connect('button-press-event',
            function(o, event) {
                if (Main.overlay.visible)
                    Main.hideOverlay();
                else
                    Main.showOverlay();

                return true;
            });

        this.actor.add_actor(box);

        global.stage.add_actor(this.actor);
        // Declare just "box" (ie, not the drop shadow) as a shell actor
        Main.addShellActor(box);

        global.screen.connect('restacked', Lang.bind(this, this._restacked));
        this._restacked();

        // Start the clock
        this._updateClock();
    },

    _restacked: function() {
        let global = Shell.Global.get();
        let windows = global.get_windows();
        let i;

        // We want to be visible unless there is a window with layer
        // FULLSCREEN, or a window with layer OVERRIDE_REDIRECT that
        // completely covers us. (We can't set a non-rectangular
        // stage_input_area, so we don't let windows overlap us
        // partially.). "override_redirect" is not actually a layer
        // above all other windows, but this seems to be how mutter
        // treats it currently...
        //
        // @windows is sorted bottom to top.
        this.actor.show();
        for (i = windows.length - 1; i > -1; i--) {
            let layer = windows[i].get_meta_window().get_layer();

            if (layer == Meta.StackLayer.OVERRIDE_REDIRECT) {
                if (windows[i].x <= this.actor.x &&
                    windows[i].x + windows[i].width >= this.actor.x + this.actor.width &&
                    windows[i].y <= this.actor.y &&
                    windows[i].y + windows[i].height >= this.actor.y + PANEL_HEIGHT) {
                    this.actor.hide();
                    break;
                }
            } else if (layer == Meta.StackLayer.FULLSCREEN) {
                this.actor.hide();
                break;
            } else
                break;
        }
    },

    _updateClock: function() {
        let displayDate = new Date();
        let msecRemaining = 60000 - (1000 * displayDate.getSeconds() +
                                     displayDate.getMilliseconds());
        if (msecRemaining < 500) {
            displayDate.setMinutes(displayDate.getMinutes() + 1);
            msecRemaining += 60000;
        }
        this._clock.set_text(displayDate.toLocaleFormat("%a %b %e, %l:%M %p"));
        Mainloop.timeout_add(msecRemaining, Lang.bind(this, this._updateClock));
        return false;
    },

    overlayHidden: function() {
        this.button.release();
    }
};
