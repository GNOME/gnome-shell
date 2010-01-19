/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Tweener = imports.ui.tweener;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const Calendar = imports.ui.calendar;
const Main = imports.ui.main;
const StatusMenu = imports.ui.statusMenu;

const PANEL_HEIGHT = 26;

const DEFAULT_PADDING = 4;

const PANEL_ICON_SIZE = 24;

// See comments around _recomputeTraySize
const TRAY_SPACING = 14;
const TRAY_SPACING_MIN = 8;

// Used for the tray icon container with gtk pre-2.16, which doesn't
// fully support tray icon transparency
const TRAY_BACKGROUND_COLOR = new Clutter.Color();
TRAY_BACKGROUND_COLOR.from_pixel(0x0b0b0bff);
const TRAY_BORDER_COLOR = new Clutter.Color();
TRAY_BORDER_COLOR.from_pixel(0x00000033);
const TRAY_CORNER_RADIUS = 5;
const TRAY_BORDER_WIDTH = 0;

const HOT_CORNER_ACTIVATION_TIMEOUT = 0.5;

const STANDARD_TRAY_ICON_ORDER = ['keyboard', 'volume', 'bluetooth', 'network', 'battery']
const STANDARD_TRAY_ICON_IMPLEMENTATIONS = {
    'bluetooth-applet': 'bluetooth',
    'gnome-volume-control-applet': 'volume',
    'nm-applet': 'network',
    'gnome-power-manager': 'battery'
};

function TextShadower() {
    this._init();
}

TextShadower.prototype = {
    _init: function() {
        this.actor = new Shell.GenericContainer();
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this._label = new St.Label();
        this.actor.add_actor(this._label);
        for (let i = 0; i < 4; i++) {
            let actor = new St.Label({ style_class: 'label-shadow' });
            this.actor.add_actor(actor);
        }
        this._label.raise_top();
    },

    setText: function(text) {
        let children = this.actor.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].set_text(text);
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [minWidth, natWidth] = this._label.get_preferred_width(forHeight);
        alloc.min_size = minWidth;
        alloc.natural_size = natWidth;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [minHeight, natHeight] = this._label.get_preferred_height(forWidth);
        alloc.min_size = minHeight;
        alloc.natural_size = natHeight;
    },

    _allocate: function(actor, box, flags) {
        let children = this.actor.get_children();

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [minChildWidth, minChildHeight, natChildWidth, natChildHeight] =
            this._label.get_preferred_size();

        let childWidth = Math.min(natChildWidth, availWidth);
        let childHeight = Math.min(natChildHeight, availHeight);

        for (let i = 0; i < children.length; i++) {
            let child = children[i];
            let childBox = new Clutter.ActorBox();
            // The order of the labels here is arbitrary, except
            // we know the "real" label is at the end because Clutter.Group
            // sorts by Z order
            switch (i) {
                case 0: // top
                    childBox.x1 = 1;
                    childBox.y1 = 0;
                    break;
                case 1: // right
                    childBox.x1 = 2;
                    childBox.y1 = 1;
                    break;
                case 2: // bottom
                    childBox.x1 = 1;
                    childBox.y1 = 2;
                    break;
                case 3: // left
                    childBox.x1 = 0;
                    childBox.y1 = 1;
                    break;
                case 4: // center
                    childBox.x1 = 1;
                    childBox.y1 = 1;
                    break;
            }
            childBox.x2 = childBox.x1 + childWidth;
            childBox.y2 = childBox.y1 + childHeight;
            child.allocate(childBox, flags);
        }
    }
}

/**
 * AppPanelMenu:
 *
 * This class manages the "application menu" component.  It tracks the
 * currently focused application.  However, when an app is launched,
 * this menu also handles startup notification for it.  So when we
 * have an active startup notification, we switch modes to display that.
 */
function AppPanelMenu() {
    this._init();
}

AppPanelMenu.prototype = {
    _init: function() {
        this._metaDisplay = global.screen.get_display();

        this._focusedApp = null;
        this._activeSequence = null;
        this._startupSequences = {};

        this.actor = new St.BoxLayout({ name: 'appMenu' });
        this._iconBox = new Shell.Slicer({ name: 'appMenuIcon' });
        this.actor.add(this._iconBox);
        this._label = new TextShadower();
        this.actor.add(this._label.actor, { expand: true, y_fill: true });
        this.actor.connect('notify::allocation', Lang.bind(this, this._repositionLabel));

        this._startupBox = new St.BoxLayout();
        this.actor.add(this._startupBox);

        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.actor.opacity = 255;
        }));
        Main.overview.connect('showing', Lang.bind(this, function () {
            this.actor.opacity = 192;
        }));

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('notify::focus-app', Lang.bind(this, this._sync));
        tracker.connect('startup-sequence-changed', Lang.bind(this, this._sync));
        // For now just resync on all running state changes; this is mainly to handle
        // cases where the focused window's application changes without the focus
        // changing.  An example case is how we map Firefox based on the window
        // title which is a dynamic property.
        tracker.connect('app-running-changed', Lang.bind(this, this._sync));

        this._sync();
    },

    _repositionLabel: function() {
        this._label.actor.x = Math.floor(AppDisplay.APPICON_SIZE / 2);
        let actorAlloc = this.actor.allocation;
        let actorHeight = actorAlloc.y2 - actorAlloc.y1;
        let labelAlloc = this._label.actor.allocation;
        let labelHeight = labelAlloc.y2 - labelAlloc.y1;
        this._label.actor.y = Math.floor((actorHeight - labelHeight) / 2);
        this._label.actor.fixed_position_set = true;
    },

    _sync: function() {
        let tracker = Shell.WindowTracker.get_default();

        let focusedApp = tracker.focus_app;

        let lastSequence = null;
        if (focusedApp == null) {
            let sequences = tracker.get_startup_sequences();
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

        if (this._iconBox.child != null)
            this._iconBox.child.destroy();
        this._iconBox.hide();
        this._label.setText('');
        let icon;
        if (this._focusedApp != null) {
            icon = this._focusedApp.create_icon_texture(AppDisplay.APPICON_SIZE);
            this._label.setText(this._focusedApp.get_name());
        } else if (this._activeSequence != null) {
            icon = this._activeSequence.create_icon(AppDisplay.APPICON_SIZE);
            this._label.setText(this._activeSequence.get_name());
        } else {
            icon = null;
        }
        
        if (icon != null) {
            let faded = Shell.fade_app_icon(icon); /* TODO consider caching */
            this._iconBox.set_child(faded);
            this._iconBox.show();
        }
        this._repositionLabel();

        this.emit('changed');
    }
}

Signals.addSignalMethods(AppPanelMenu.prototype);

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {

        this.actor = new St.BoxLayout({ name: 'panel' });
        this.actor._delegate = this;

        this._leftBox = new St.BoxLayout({ name: 'panelLeft' });
        this._centerBox = new St.BoxLayout({ name: 'panelCenter' });
        this._rightBox = new St.BoxLayout({ name: 'panelRight' });

        /* This box container ensures that the centerBox is positioned in the *absolute*
         * center, but can be pushed aside if necessary. */
        this._boxContainer = new Shell.GenericContainer();
        this.actor.add(this._boxContainer, { expand: true });
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

        /* Button on the left side of the panel. */
        /* Translators: If there is no suitable word for "Activities" in your language, you can use the word for "Overview". */
        let label = new St.Label({ text: _("Activities") });
        this.button = new St.Clickable({ name: 'panelActivities',
                                          style_class: 'panel-button',
                                          reactive: true });
        this.button.set_child(label);
        this.button.height = PANEL_HEIGHT;

        this._leftBox.add(this.button);

        // We use this flag to mark the case where the user has entered the
        // hot corner and has not left both the hot corner and a surrounding
        // guard area (the "environs"). This avoids triggering the hot corner
        // multiple times due to an accidental jitter.
        this._hotCornerEntered = false;

        this._hotCornerEnvirons = new Clutter.Rectangle({ x: 0,
                                                          y: 0,
                                                          width: 3,
                                                          height: 3,
                                                          opacity: 0,
                                                          reactive: true });

        this._hotCorner = new Clutter.Rectangle({ x: 0,
                                                  y: 0,
                                                  width: 1,
                                                  height: 1,
                                                  opacity: 0,
                                                  reactive: true });

        this._hotCornerActivationTime = 0;

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

        this._leftBox.add(this._hotCornerEnvirons);
        this._leftBox.add(this._hotCorner);

        let appMenu = new AppPanelMenu();
        this._leftBox.add(appMenu.actor);

        /* center */

        let clockButton = new St.Button({ style_class: "panel-button",
                                          toggle_mode: true,
                                          x_fill: true,
                                          y_fill: true });
        this._centerBox.add(clockButton, { y_fill: false });
        clockButton.connect('clicked', Lang.bind(this, this._toggleCalendar));

        this._clock = new St.Label();
        clockButton.set_child(this._clock);
        this._clockButton = clockButton;

        this._calendarPopup = null;

        /* right */

        // The tray icons live in trayBox within trayContainer.
        // The trayBox is hidden when there are no tray icons.
        let trayContainer = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          y_align: Big.BoxAlignment.CENTER });
        this._rightBox.add(trayContainer);
        let trayBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    height: PANEL_ICON_SIZE,
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
        this._traymanager.connect('tray-icon-added', Lang.bind(this, this._onTrayIconAdded));
        this._traymanager.connect('tray-icon-removed',
            Lang.bind(this, function(o, icon) {
                trayBox.remove_actor(icon);

                if (trayBox.get_children().length == 0)
                    trayBox.hide();
                this._recomputeTraySize();
            }));
        this._traymanager.manage_stage(global.stage);

        let statusmenu = this._statusmenu = new StatusMenu.StatusMenu();
        let statusbutton = new St.Clickable({ name: 'panelStatus',
                                               style_class: 'panel-button',
                                               reactive: true });
        statusbutton.set_child(statusmenu.actor);
        statusbutton.height = PANEL_HEIGHT;
        statusbutton.connect('clicked', function (b, event) {
            statusmenu.toggle(event);
            // The statusmenu might not pop up if it couldn't get a pointer grab
            if (statusmenu.isActive())
                statusbutton.active = true;
            return true;
        });
        this._rightBox.add(statusbutton);
        // We get a deactivated event when the popup disappears
        this._statusmenu.connect('deactivated', function (sm) {
            statusbutton.active = false;
        });

        // TODO: decide what to do with the rest of the panel in the Overview mode (make it fade-out, become non-reactive, etc.)
        // We get into the Overview mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the Overview act like a menu that allows the user to release the mouse on the activity the user wants
        // to switch to.
        this.button.connect('clicked', Lang.bind(this, function(b, event) {
            if (!Main.overview.animationInProgress) {
                this._maybeToggleOverviewOnClick();
                return true;
            } else {
                return false;
            }
        }));
        // In addition to pressing the button, the Overview can be entered and exited by other means, such as
        // pressing the System key, Alt+F1 or Esc. We want the button to be pressed in when the Overview is entered
        // and to be released when it is exited regardless of how it was triggered.
        Main.overview.connect('showing', Lang.bind(this, function() {
            this.button.active = true;
        }));
        Main.overview.connect('hiding', Lang.bind(this, function() {
            this.button.active = false;
        }));

        Main.chrome.addActor(this.actor, { visibleInOverview: true });

        // Start the clock
        this._updateClock();
    },

    hideCalendar: function() {
        if (this._calendarPopup != null) {
            this._clockButton.checked = false;
            this._calendarPopup.actor.hide();
        }
    },

    startupAnimation: function() {
        this.actor.y = -this.actor.height;
        Tweener.addTween(this.actor,
                         { y: 0,
                           time: 0.2,
                           transition: "easeOutQuad"
                         });
    },

    _onTrayIconAdded: function(o, icon, wmClass) {
        let role = STANDARD_TRAY_ICON_IMPLEMENTATIONS[wmClass];
        if (!role) {
            // Unknown icons go first in undefined order
            this._trayBox.prepend(icon, Big.BoxPackFlags.NONE);
        } else {
            icon._role = role;
            // Figure out the index in our well-known order for this icon
            let position = STANDARD_TRAY_ICON_ORDER.indexOf(role);
            icon._rolePosition = position;
            let children = this._trayBox.get_children();
            let i;
            // Walk children backwards, until we find one that isn't
            // well-known, or one where we should follow
            for (i = children.length - 1; i >= 0; i--) {
                let rolePosition = children[i]._rolePosition;
                if (!rolePosition || position > rolePosition) {
                    this._trayBox.insert_after(icon, children[i], Big.BoxPackFlags.NONE);
                    break;
                }
            }
            if (i == -1) {
                // If we didn't find a position, we must be first
                this._trayBox.prepend(icon, Big.BoxPackFlags.NONE);
            }
        }

        // Make sure the trayBox is shown.
        this._trayBox.show();
        this._recomputeTraySize();
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

    _toggleCalendar: function(clockButton) {
        if (clockButton.checked) {
            if (this._calendarPopup == null)
                this._calendarPopup = new CalendarPopup();
            this._calendarPopup.show();
        } else {
            this._calendarPopup.hide();
        }
    },

    _onHotCornerEntered : function() {
        if (!this._hotCornerEntered) {
            this._hotCornerEntered = true;
            if (!Main.overview.animationInProgress) {
                this._hotCornerActivationTime = Date.now() / 1000;
                Main.overview.toggle();
            }
        }
        return false;
    },

    _onHotCornerClicked : function() {
         if (!Main.overview.animationInProgress) {
             this._maybeToggleOverviewOnClick();
         }
         return false;
    },

    _onHotCornerLeft : function(actor, event) {
        if (event.get_related() != this._hotCornerEnvirons) {
            this._hotCornerEntered = false;
        }
        return false;
    },

    _onHotCornerEnvironsLeft : function(actor, event) {
        if (event.get_related() != this._hotCorner) {
            this._hotCornerEntered = false;
        }
        return false;
    },

    // Toggles the overview unless this is the first click on the Activities button within the HOT_CORNER_ACTIVATION_TIMEOUT time
    // of the hot corner being triggered. This check avoids opening and closing the overview if the user both triggered the hot corner
    // and clicked the Activities button.
    _maybeToggleOverviewOnClick: function() {
        if (this._hotCornerActivationTime == 0 || Date.now() / 1000 - this._hotCornerActivationTime > HOT_CORNER_ACTIVATION_TIMEOUT)
            Main.overview.toggle();
        this._hotCornerActivationTime = 0;
    }
};

function CalendarPopup() {
    this._init();
}

CalendarPopup.prototype = {
    _init: function() {
        let panelActor = Main.panel.actor;

        this.actor = new St.BoxLayout({ name: 'calendarPopup' });

        this.calendar = new Calendar.Calendar();
        this.actor.add(this.calendar.actor);

        Main.chrome.addActor(this.actor, { visibleInOverview: true,
                                           affectsStruts: false });
        this.actor.y = (panelActor.y + panelActor.height - this.actor.height);
    },

    show: function() {
        let panelActor = Main.panel.actor;

        // Reset the calendar to today's date
        this.calendar.setDate(new Date());

        this.actor.x = Math.round(panelActor.x + (panelActor.width - this.actor.width) / 2);
        this.actor.lower(panelActor);
        this.actor.show();
        Tweener.addTween(this.actor,
                         { y: panelActor.y + panelActor.height,
                           time: 0.2,
                           transition: "easeOutQuad"
                         });
    },

    hide: function() {
        let panelActor = Main.panel.actor;

        Tweener.addTween(this.actor,
                         { y: panelActor.y + panelActor.height - this.actor.height,
                           time: 0.2,
                           transition: "easeOutQuad",
                           onComplete: function() { this.actor.hide(); },
                           onCompleteScope: this
                         });
    }
};
