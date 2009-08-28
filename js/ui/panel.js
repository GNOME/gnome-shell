/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Tweener = imports.ui.tweener;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Button = imports.ui.button;
const Main = imports.ui.main;

const PANEL_HEIGHT = 26;
const TRAY_HEIGHT = PANEL_HEIGHT - 1;

const DEFAULT_PADDING = 4;

const PANEL_ICON_SIZE = 24;

const BACKGROUND_TOP = new Clutter.Color();
BACKGROUND_TOP.from_pixel(0x161616ff);
const BACKGROUND_BOTTOM = new Clutter.Color();
BACKGROUND_BOTTOM.from_pixel(0x000000ff);

const PANEL_FOREGROUND_COLOR = new Clutter.Color();
PANEL_FOREGROUND_COLOR.from_pixel(0xffffffff);
const SN_BACKGROUND_COLOR = new Clutter.Color();
SN_BACKGROUND_COLOR.from_pixel(0xffff00a0);

const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

// Don't make the mouse hover effect visible to the user for a menu feel.
const PANEL_BUTTON_COLOR = new Clutter.Color();
PANEL_BUTTON_COLOR.from_pixel(0x00000000);

// Lighten pressed buttons; darkening has no effect on a black background.
const PRESSED_BUTTON_BACKGROUND_COLOR = new Clutter.Color();
PRESSED_BUTTON_BACKGROUND_COLOR.from_pixel(0x324c6ffa);

const DEFAULT_FONT = 'Sans 16px';

const TRAY_PADDING = 0;
// See comments around _recomputeTraySize
const TRAY_SPACING = 14;
const TRAY_SPACING_MIN = 8;

// Used for the tray icon container with gtk pre-2.16, which doesn't
// fully support tray icon transparency
const TRAY_BACKGROUND_COLOR = new Clutter.Color();
TRAY_BACKGROUND_COLOR.from_pixel(0xefefefff);
const TRAY_BORDER_COLOR = new Clutter.Color();
TRAY_BORDER_COLOR.from_pixel(0x00000033);
const TRAY_CORNER_RADIUS = 5;
const TRAY_BORDER_WIDTH = 0;


function AppPanelMenu() {
    this._init();
}

AppPanelMenu.prototype = {
    _init: function() {
        this._metaDisplay = Shell.Global.get().screen.get_display();

        this._focusedApp = null;
        this._activeSequence = null;
        this._startupSequences = {};

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: DEFAULT_PADDING,
                                   y_align: Big.BoxAlignment.CENTER });
        this._iconBox = new Big.Box({ width: PANEL_ICON_SIZE, height: PANEL_ICON_SIZE,
                                      x_align: Big.BoxAlignment.CENTER,
                                      y_align: Big.BoxAlignment.CENTER });
        this.actor.append(this._iconBox, Big.BoxPackFlags.NONE);
        let labelBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                     y_align: Big.BoxAlignment.CENTER });
        this._label = new Clutter.Text({ font_name: DEFAULT_FONT,
                                         color: PANEL_FOREGROUND_COLOR,
                                         text: "" });
        labelBox.append(this._label, Big.BoxPackFlags.EXPAND);
        this.actor.append(labelBox, Big.BoxPackFlags.NONE);

        this._startupBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                         y_align: Big.BoxAlignment.CENTER });
        this.actor.append(this._startupBox, Big.BoxPackFlags.NONE);

        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.actor.opacity = 255;
        }));
        Main.overview.connect('showing', Lang.bind(this, function () {
            this.actor.opacity = 192;
        }));

        this._metaDisplay.connect('notify::focus-window', Lang.bind(this, this._sync));

        let appMonitor = Shell.AppMonitor.get_default();
        appMonitor.connect('startup-sequence-changed', Lang.bind(this, this._sync));
        // For now just resync on application add/remove; this is mainly to handle
        // cases where the focused window's application changes without the focus
        // changing.  An example case is how we map Firefox based on the window
        // title which is a dynamic property.
        appMonitor.connect('app-added', Lang.bind(this, this._sync));
        appMonitor.connect('app-removed', Lang.bind(this, this._sync));

        this._sync();
    },

    _sync: function() {
        let appMonitor = Shell.AppMonitor.get_default();

        let focusWindow = this._metaDisplay.get_focus_window();
        let focusedApp;
        if (focusWindow == null) {
           focusedApp = null;
        } else {
           focusedApp = appMonitor.get_window_app(focusWindow);
        }

        let lastSequence = null;
        if (focusedApp == null) {
            let sequences = appMonitor.get_startup_sequences();
            if (sequences.length > 0)
                lastSequence = sequences[sequences.length - 1];
        }

        // If the currently focused app hasn't changed and the current
        // startup sequence hasn't changed, we have nothing to do
        if (focusedApp == this._focusedApp
            && ((lastSequence == null && this._activeSequence == null)
                || (lastSequence != null && this._activeSequence != null
                    && lastSequence.get_id() == this._activeSequence.get_id())))
            return;

        this._focusedApp = focusedApp;
        this._activeSequence = lastSequence;

        this._iconBox.remove_all();
        this._iconBox.hide();
        this._label.text = '';
        if (this._focusedApp != null) {
            let icon = focusedApp.create_icon_texture(PANEL_ICON_SIZE);
            this._iconBox.append(icon, Big.BoxPackFlags.NONE);
            this._iconBox.show();
            this._label.text = focusedApp.get_name();
        } else if (this._activeSequence != null) {
            let icon = this._activeSequence.create_icon(PANEL_ICON_SIZE);
            this._iconBox.append(icon, Big.BoxPackFlags.NONE);
            this._iconBox.show();
            this._label.text = this._activeSequence.get_name();
        }

        this.emit('changed');
    }
}

Signals.addSignalMethods(AppPanelMenu.prototype);

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
        let global = Shell.Global.get();


        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL
                                 });
        let backgroundGradient = Shell.create_vertical_gradient(BACKGROUND_TOP,
                                                                BACKGROUND_BOTTOM);
        this.actor.connect('notify::allocation', Lang.bind(this, function () {
            let [width, height] = this.actor.get_size();
            backgroundGradient.set_size(width, height);
        }));
        this.actor.add_actor(backgroundGradient);

        this._leftBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                      y_align: Big.BoxAlignment.CENTER,
                                      spacing: DEFAULT_PADDING,
                                      padding_right: DEFAULT_PADDING });
        this._centerBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                        y_align: Big.BoxAlignment.CENTER });
        this._rightBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                       y_align: Big.BoxAlignment.CENTER,
                                       padding_left: DEFAULT_PADDING });

        /* This box container ensures that the centerBox is positioned in the *absolute*
         * center, but can be pushed aside if necessary. */
        this._boxContainer = new Shell.GenericContainer();
        this.actor.append(this._boxContainer, Big.BoxPackFlags.EXPAND);
        this._boxContainer.add_actor(this._leftBox);
        this._boxContainer.add_actor(this._centerBox);
        this._boxContainer.add_actor(this._rightBox);
        this._boxContainer.connect('get-preferred-width', Lang.bind(this, function(box, forHeight, alloc) {
            let children = box.get_children();
            for (let i = 0; i < children.length; i++) {
                let [childMin, childNatural] = children[i].get_preferred_width(forHeight);
                alloc.min_size += childMin;
                alloc.natural_size += childNatural;
            }
        }));
        this._boxContainer.connect('get-preferred-height', Lang.bind(this, function(box, forWidth, alloc) {
            let children = box.get_children();
            for (let i = 0; i < children.length; i++) {
                let [childMin, childNatural] = children[i].get_preferred_height(forWidth);
                if (childMin > alloc.min_size)
                    alloc.min_size = childMin;
                if (childNatural > alloc.natural_size)
                    alloc.natural_size = childNatural;
            }
        }));
        this._boxContainer.connect('allocate', Lang.bind(this, function(container, box, flags) {
            let allocWidth = box.x2 - box.x1;
            let allocHeight = box.y2 - box.y1;
            let [leftMinWidth, leftNaturalWidth] = this._leftBox.get_preferred_width(-1);
            let [centerMinWidth, centerNaturalWidth] = this._centerBox.get_preferred_width(-1);
            let [rightMinWidth, rightNaturalWidth] = this._rightBox.get_preferred_width(-1);
            let leftWidth, centerWidth, rightWidth;
            if (allocWidth < (leftNaturalWidth + centerNaturalWidth + rightNaturalWidth)) {
                leftWidth = leftMinWidth;
                centerWidth = centerMinWidth;
                rightWidth = rightMinWidth;
            } else {
                leftWidth = leftNaturalWidth;
                centerWidth = centerNaturalWidth;
                rightWidth = rightNaturalWidth;
            }

            let x;
            let childBox = new Clutter.ActorBox();
            childBox.x1 = box.x1;
            childBox.y1 = box.y1;
            childBox.x2 = x = childBox.x1 + leftWidth;
            childBox.y2 = box.y2;
            this._leftBox.allocate(childBox, flags);

            let centerNaturalX = Math.floor((box.x2 - box.x1) / 2 - (centerWidth / 2));
            /* Check left side */
            if (x < centerNaturalX) {
                /* We didn't overflow the left, use the natural. */
                x = centerNaturalX;
            }
            /* Check right side */
            if (x + centerWidth > (box.x2 - rightWidth)) {
                x = box.x2 - rightWidth - centerWidth;
            }
            childBox = new Clutter.ActorBox();
            childBox.x1 = x;
            childBox.y1 = box.y1;
            childBox.x2 = x = childBox.x1 + centerWidth;
            childBox.y2 = box.y2;
            this._centerBox.allocate(childBox, flags);

            childBox = new Clutter.ActorBox();
            childBox.x1 = box.x2 - rightWidth;
            childBox.y1 = box.y1;
            childBox.x2 = box.x2;
            childBox.y2 = box.y2;
            this._rightBox.allocate(childBox, flags);
        }));

        /* left side */

        this.button = new Button.Button(_("Activities"), PANEL_BUTTON_COLOR, PRESSED_BUTTON_BACKGROUND_COLOR,
                                        PANEL_FOREGROUND_COLOR, true, DEFAULT_FONT);
        this.button.button.height = PANEL_HEIGHT;

        this._leftBox.append(this.button.button, Big.BoxPackFlags.NONE);

        // We use this flag to mark the case where the user has entered the
        // hot corner and has not left both the hot corner and a surrounding
        // guard area (the "environs"). This avoids triggering the hot corner
        // multiple times due to an accidental jitter.
        this._hotCornerEntered = false;

        this._hotCornerEnvirons = new Clutter.Rectangle({ width: 3,
                                                          height: 3,
                                                          opacity: 0,
                                                          reactive: true });

        this._hotCorner = new Clutter.Rectangle({ width: 1,
                                                  height: 1,
                                                  opacity: 0,
                                                  reactive: true });

        this._hotCornerEnvirons.connect('leave-event',
                                        Lang.bind(this, this._onHotCornerEnvironsLeft));
        // Clicking on the hot corner environs should result in the same bahavior
        // as clicking on the hot corner.
        this._hotCornerEnvirons.connect('button-release-event',
                                        Lang.bind(this, this._onHotCornerClicked));

        // In addition to being triggered by the mouse enter event, the hot corner
        // can be triggered by clicking on it. This is useful if the user wants to 
        // undo the effect of triggering the hot corner once in the hot corner.
        this._hotCorner.connect('enter-event',
                                Lang.bind(this, this._onHotCornerEntered));
        this._hotCorner.connect('button-release-event',
                                Lang.bind(this, this._onHotCornerClicked));
        this._hotCorner.connect('leave-event',
                                Lang.bind(this, this._onHotCornerLeft));

        this._leftBox.append(this._hotCornerEnvirons, Big.BoxPackFlags.FIXED);
        this._leftBox.append(this._hotCorner, Big.BoxPackFlags.FIXED);

        let appMenu = new AppPanelMenu();
        this._leftBox.append(appMenu.actor, Big.BoxPackFlags.NONE);

        /* center */

        this._clock = new Clutter.Text({ font_name: DEFAULT_FONT,
                                         color: PANEL_FOREGROUND_COLOR,
                                         text: "" });
        this._centerBox.append(this._clock, Big.BoxPackFlags.NONE);

        /* right */

        // The tray icons live in trayBox within trayContainer.
        // The trayBox is hidden when there are no tray icons.
        let trayContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          y_align: Big.BoxAlignment.START });
        this._rightBox.append(trayContainer, Big.BoxPackFlags.NONE);
        let trayBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    height: TRAY_HEIGHT,
                                    padding: TRAY_PADDING,
                                    spacing: TRAY_SPACING });
        this._trayBox = trayBox;

        // gtk+ < 2.16 doesn't have fully-working icon transparency,
        // so we want trayBox to be opaque in that case (the icons
        // will at least pick up its background color).
        if (Gtk.MAJOR_VERSION == 2 && Gtk.MINOR_VERSION < 16) {
            trayBox.background_color = TRAY_BACKGROUND_COLOR;
            trayBox.corner_radius = TRAY_CORNER_RADIUS;
            trayBox.border = TRAY_BORDER_WIDTH;
            trayBox.border_color = TRAY_BORDER_COLOR;
        }

        trayBox.hide();
        trayContainer.append(trayBox, Big.BoxPackFlags.NONE);

        this._traymanager = new Shell.TrayManager({ bg_color: TRAY_BACKGROUND_COLOR });
        this._traymanager.connect('tray-icon-added',
            Lang.bind(this, function(o, icon) {
                trayBox.append(icon, Big.BoxPackFlags.NONE);

                // Make sure the trayBox is shown.
                trayBox.show();
                this._recomputeTraySize();
            }));
        this._traymanager.connect('tray-icon-removed',
            Lang.bind(this, function(o, icon) {
                trayBox.remove_actor(icon);

                if (trayBox.get_children().length == 0)
                    trayBox.hide();
                this._recomputeTraySize();
            }));
        this._traymanager.manage_stage(global.stage);

        let statusbox = new Big.Box();
        let statusmenu = this._statusmenu = new Shell.StatusMenu();
        statusmenu.get_icon().hide();
        statusmenu.get_name().fontName = DEFAULT_FONT;
        statusmenu.get_name().color = PANEL_FOREGROUND_COLOR;
        statusbox.append(this._statusmenu, Big.BoxPackFlags.NONE);
        let statusbutton = new Button.Button(statusbox,
                                             PANEL_BUTTON_COLOR,
                                             PRESSED_BUTTON_BACKGROUND_COLOR,
                                             PANEL_FOREGROUND_COLOR,
                                             true);
        statusbutton.button.height = PANEL_HEIGHT;
        statusbutton.button.connect('button-press-event', function (b, e) {
            statusmenu.toggle(e);
            return false;
        });
        this._rightBox.append(statusbutton.button, Big.BoxPackFlags.NONE);
        // We get a deactivated event when the popup disappears
        this._statusmenu.connect('deactivated', function (sm) {
            statusbutton.release();
        });

        // TODO: decide what to do with the rest of the panel in the Overview mode (make it fade-out, become non-reactive, etc.)
        // We get into the Overview mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the Overview act like a menu that allows the user to release the mouse on the activity the user wants
        // to switch to.
        this.button.button.connect('button-press-event',
                                   Lang.bind(Main.overview, Main.overview.toggle));
        // In addition to pressing the button, the Overview can be entered and exited by other means, such as
        // pressing the System key, Alt+F1 or Esc. We want the button to be pressed in when the Overview is entered
        // and to be released when it is exited regardless of how it was triggered.
        Main.overview.connect('showing', Lang.bind(this.button, this.button.pressIn));
        Main.overview.connect('hiding', Lang.bind(this.button, this.button.release));

        Main.chrome.addActor(this.actor);
        Main.chrome.setVisibleInOverview(this.actor, true);

        // Start the clock
        this._updateClock();
    },

    startupAnimation: function() {
        this.actor.y = -this.actor.height;
        Tweener.addTween(this.actor,
                         { y: 0,
                           time: 0.2,
                           transition: "easeOutQuad"
                         });
    },

    // By default, tray icons have a spacing of TRAY_SPACING.  However this
    // starts to fail if we have too many as can sadly happen; just jump down
    // to a spacing of 8 if we're over 6.
    // http://bugzilla.gnome.org/show_bug.cgi?id=590495
    _recomputeTraySize: function () {
        if (this._trayBox.get_children().length > 6)
            this._trayBox.spacing = TRAY_SPACING_MIN;
        else
            this._trayBox.spacing = TRAY_SPACING;
    },

    _updateClock: function() {
        let displayDate = new Date();
        let msecRemaining = 60000 - (1000 * displayDate.getSeconds() +
                                     displayDate.getMilliseconds());
        if (msecRemaining < 500) {
            displayDate.setMinutes(displayDate.getMinutes() + 1);
            msecRemaining += 60000;
        }
        /* Translators: This is a time format.  */
        this._clock.set_text(displayDate.toLocaleFormat(_("%a %l:%M %p")));
        Mainloop.timeout_add(msecRemaining, Lang.bind(this, this._updateClock));
        return false;
    },

    _onHotCornerEntered : function() {
        if (!this._hotCornerEntered) {
            this._hotCornerEntered = true;
            if (!Main.overview.animationInProgress) {
                Main.overview.toggle();
            }
        }
        return false;
    },

    _onHotCornerClicked : function() {
         if (!Main.overview.animationInProgress) {
             Main.overview.toggle();
         }
         return false;
    },

    _onHotCornerLeft : function(actor, event) {
        if (Shell.get_event_related(event) != this._hotCornerEnvirons) {
            this._hotCornerEntered = false;
        }
        return false;
    },

    _onHotCornerEnvironsLeft : function(actor, event) {
        if (Shell.get_event_related(event) != this._hotCorner) {
            this._hotCornerEntered = false;
        }
        return false;
    }
};
