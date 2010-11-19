/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Calendar = imports.ui.calendar;
const Overview = imports.ui.overview;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const StatusMenu = imports.ui.statusMenu;
const DateMenu = imports.ui.dateMenu;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const PANEL_HEIGHT = 26;

const PANEL_ICON_SIZE = 24;

const HOT_CORNER_ACTIVATION_TIMEOUT = 0.5;

const ANIMATED_ICON_UPDATE_TIMEOUT = 100;
const SPINNER_UPDATE_TIMEOUT = 130;
const SPINNER_SPEED = 0.02;

const STANDARD_TRAY_ICON_ORDER = ['a11y', 'display', 'keyboard', 'volume', 'bluetooth', 'network', 'battery'];
const STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION = {
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'volume': imports.ui.status.volume.Indicator,
    'battery': imports.ui.status.power.Indicator
};

function AnimatedIcon(name, size) {
    this._init(name, size);
}

AnimatedIcon.prototype = {
    _init: function(name, size) {
        this.actor = new St.Bin({ visible: false });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this.actor.connect('notify::visible', Lang.bind(this, function() {
            if (this.actor.visible) {
                this._timeoutId = Mainloop.timeout_add(ANIMATED_ICON_UPDATE_TIMEOUT, Lang.bind(this, this._update));
            } else {
                if (this._timeoutId)
                    Mainloop.source_remove(this._timeoutId);
                this._timeoutId = 0;
            }
        }));

        this._timeoutId = 0;
        this._i = 0;
        this._animations = St.TextureCache.get_default().load_sliced_image (global.datadir + '/theme/' + name, size, size);
        this.actor.set_child(this._animations);
    },

    _update: function() {
        this._animations.hide_all();
        this._animations.show();
        if (this._i && this._i < this._animations.get_n_children())
            this._animations.get_nth_child(this._i++).show();
        else {
            this._i = 1;
            if (this._animations.get_n_children())
                this._animations.get_nth_child(0).show();
        }
        return true;
    },

    _onDestroy: function() {
        if (this._timeoutId)
            Mainloop.source_remove(this._timeoutId);
    }
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
            actor.clutter_text.ellipsize = Pango.EllipsizeMode.END;
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
        alloc.min_size = minWidth + 2;
        alloc.natural_size = natWidth + 2;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [minHeight, natHeight] = this._label.get_preferred_height(forWidth);
        alloc.min_size = minHeight + 2;
        alloc.natural_size = natHeight + 2;
    },

    _allocate: function(actor, box, flags) {
        let children = this.actor.get_children();

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [minChildWidth, minChildHeight, natChildWidth, natChildHeight] =
            this._label.get_preferred_size();

        let childWidth = Math.min(natChildWidth, availWidth - 2);
        let childHeight = Math.min(natChildHeight, availHeight - 2);

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
};

/**
 * AppMenuButton:
 *
 * This class manages the "application menu" component.  It tracks the
 * currently focused application.  However, when an app is launched,
 * this menu also handles startup notification for it.  So when we
 * have an active startup notification, we switch modes to display that.
 */
function AppMenuButton() {
    this._init();
}

AppMenuButton.prototype = {
    __proto__: PanelMenu.Button.prototype,

    _init: function() {
        PanelMenu.Button.prototype._init.call(this, St.Align.START);
        this._metaDisplay = global.screen.get_display();
        this._startingApps = [];

        this._targetApp = null;

        let bin = new St.Bin({ name: 'appMenu' });
        this.actor.set_child(bin);
        this._container = new Shell.GenericContainer();
        bin.set_child(this._container);
        this._container.connect('get-preferred-width', Lang.bind(this, this._getContentPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._getContentPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._contentAllocate));

        this._iconBox = new Shell.Slicer({ name: 'appMenuIcon' });
        this._container.add_actor(this._iconBox);
        this._label = new TextShadower();
        this._container.add_actor(this._label.actor);

        this._quitMenu = new PopupMenu.PopupMenuItem('');
        this.menu.addMenuItem(this._quitMenu);
        this._quitMenu.connect('activate', Lang.bind(this, this._onQuit));

        this._visible = !Main.overview.visible;
        if (!this._visible)
            this.hide();
        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.show();
        }));
        Main.overview.connect('showing', Lang.bind(this, function () {
            this.hide();
        }));

        this._updateId = 0;
        this._animationStep = 0;
        this._clipWidth = PANEL_ICON_SIZE;
        this._direction = SPINNER_SPEED;

        this._spinner = new AnimatedIcon('process-working.png',
                                         PANEL_ICON_SIZE);
        this._container.add_actor(this._spinner.actor);
        this._spinner.actor.lower_bottom();

        this._shadow = new St.Bin({ style_class: 'label-real-shadow' });
        this._shadow.hide();
        this._container.add_actor(this._shadow);

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('notify::focus-app', Lang.bind(this, this._sync));
        tracker.connect('app-state-changed', Lang.bind(this, this._onAppStateChanged));

        global.window_manager.connect('switch-workspace', Lang.bind(this, this._sync));

        this._sync();
    },

    show: function() {
        if (this._visible)
            return;

        this.actor.show();
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: function() {
                               this._visible = true;
                           },
                           onCompleteScope: this });
    },

    hide: function() {
        if (!this._visible)
            return;

        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: function() {
                               this.actor.hide();
                               this._visible = false;
                           },
                           onCompleteScope: this });
    },

    _stopAnimation: function(animate) {
        this._label.actor.remove_clip();
        if (this._updateId) {
            this._shadow.hide();
            if (animate) {
                Tweener.addTween(this._spinner.actor,
                                 { opacity: 0,
                                   time: 0.2,
                                   transition: "easeOutQuad",
                                   onCompleteScope: this,
                                   onComplete: function() {
                                       this._spinner.actor.opacity = 255;
                                       this._spinner.actor.hide();
                                   }
                                 });
            }
            Mainloop.source_remove(this._updateId);
            this._updateId = 0;
        }
        if (!animate)
            this._spinner.actor.hide();
    },

    stopAnimation: function() {
        this._direction = SPINNER_SPEED * 3;
        this._stop = true;
    },

    _update: function() {
        this._animationStep += this._direction;
        if (this._animationStep > 1 && this._stop) {
            this._animationStep = 1;
            this._stopAnimation(true);
            return false;
        }
        if (this._animationStep > 1)
            this._animationStep = 1;
        this._clipWidth = this._label.actor.width - (this._label.actor.width - PANEL_ICON_SIZE) * (1 - this._animationStep);
        if (this.actor.get_direction() == St.TextDirection.LTR) {
            this._label.actor.set_clip(0, 0, this._clipWidth + this._shadow.width, this.actor.height);
        } else {
            this._label.actor.set_clip(this._label.actor.width - this._clipWidth, 0, this._clipWidth, this.actor.height);
        }
        this._container.queue_relayout();
        return true;
    },

    startAnimation: function() {
        this._direction = SPINNER_SPEED;
        this._stopAnimation(false);
        this._animationStep = 0;
        this._update();
        this._stop = false;
        this._updateId = Mainloop.timeout_add(SPINNER_UPDATE_TIMEOUT, Lang.bind(this, this._update));
        this._spinner.actor.show();
        this._shadow.show();
    },

    _getContentPreferredWidth: function(actor, forHeight, alloc) {
        let [minSize, naturalSize] = this._iconBox.get_preferred_width(forHeight);
        alloc.min_size = minSize;
        alloc.natural_size = naturalSize;
        [minSize, naturalSize] = this._label.actor.get_preferred_width(forHeight);
        alloc.min_size = alloc.min_size + Math.max(0, minSize - Math.floor(alloc.min_size / 2));
        alloc.natural_size = alloc.natural_size + Math.max(0, naturalSize - Math.floor(alloc.natural_size / 2));
    },

    _getContentPreferredHeight: function(actor, forWidth, alloc) {
        let [minSize, naturalSize] = this._iconBox.get_preferred_height(forWidth);
        alloc.min_size = minSize;
        alloc.natural_size = naturalSize;
        [minSizfe, naturalSize] = this._label.actor.get_preferred_height(forWidth);
        if (minSize > alloc.min_size)
            alloc.min_size = minSize;
        if (naturalSize > alloc.natural_size)
            alloc.natural_size = naturalSize;
    },

    _contentAllocate: function(actor, box, flags) {
        let allocWidth = box.x2 - box.x1;
        let allocHeight = box.y2 - box.y1;
        let childBox = new Clutter.ActorBox();

        let [minWidth, minHeight, naturalWidth, naturalHeight] = this._iconBox.get_preferred_size();

        let direction = this.actor.get_direction();

        let yPadding = Math.floor(Math.max(0, allocHeight - naturalHeight) / 2);
        childBox.y1 = yPadding;
        childBox.y2 = childBox.y1 + Math.min(naturalHeight, allocHeight);
        if (direction == St.TextDirection.LTR) {
            childBox.x1 = 0;
            childBox.x2 = childBox.x1 + Math.min(naturalWidth, allocWidth);
        } else {
            childBox.x1 = Math.max(0, allocWidth - naturalWidth);
            childBox.x2 = allocWidth;
        }
        this._iconBox.allocate(childBox, flags);

        let iconWidth = childBox.x2 - childBox.x1;

        [minWidth, minHeight, naturalWidth, naturalHeight] = this._label.actor.get_preferred_size();

        yPadding = Math.floor(Math.max(0, allocHeight - naturalHeight) / 2);
        childBox.y1 = yPadding;
        childBox.y2 = childBox.y1 + Math.min(naturalHeight, allocHeight);

        if (direction == St.TextDirection.LTR) {
            childBox.x1 = Math.floor(iconWidth / 2);
            childBox.x2 = Math.min(childBox.x1 + naturalWidth, allocWidth);
        } else {
            childBox.x2 = allocWidth - Math.floor(iconWidth / 2);
            childBox.x1 = Math.max(0, childBox.x2 - naturalWidth);
        }
        this._label.actor.allocate(childBox, flags);

        if (direction == St.TextDirection.LTR) {
            childBox.x1 = Math.floor(iconWidth / 2) + this._clipWidth + this._shadow.width;
            childBox.x2 = childBox.x1 + this._spinner.actor.width;
            childBox.y1 = box.y1;
            childBox.y2 = box.y2 - 1;
            this._spinner.actor.allocate(childBox, flags);
            childBox.x1 = Math.floor(iconWidth / 2) + this._clipWidth + 2;
            childBox.x2 = childBox.x1 + this._shadow.width;
            childBox.y1 = box.y1;
            childBox.y2 = box.y2 - 1;
            this._shadow.allocate(childBox, flags);
        } else {
            childBox.x1 = this._label.actor.width - this._clipWidth - this._spinner.actor.width;
            childBox.x2 = childBox.x1 + this._spinner.actor.width;
            childBox.y1 = box.y1;
            childBox.y2 = box.y2 - 1;
            this._spinner.actor.allocate(childBox, flags);
        }
    },

    _onQuit: function() {
        if (this._targetApp == null)
            return;
        this._targetApp.request_quit();
    },

    _onAppStateChanged: function(tracker, app) {
        let state = app.state;
        if (state != Shell.AppState.STARTING) {
            this._startingApps = this._startingApps.filter(function(a) {
                return a != app;
            });
        } else if (state == Shell.AppState.STARTING) {
            this._startingApps.push(app);
        }
        // For now just resync on all running state changes; this is mainly to handle
        // cases where the focused window's application changes without the focus
        // changing.  An example case is how we map OpenOffice.org based on the window
        // title which is a dynamic property.
        this._sync();
    },

    _sync: function() {
        let tracker = Shell.WindowTracker.get_default();
        let lastStartedApp = null;
        let workspace = global.screen.get_active_workspace();
        for (let i = 0; i < this._startingApps.length; i++)
            if (this._startingApps[i].is_on_workspace(workspace))
                lastStartedApp = this._startingApps[i];

        let focusedApp = tracker.focus_app;
        let targetApp = focusedApp != null ? focusedApp : lastStartedApp;
        if (targetApp == this._targetApp) {
            if (targetApp && targetApp.get_state() != Shell.AppState.STARTING)
                this.stopAnimation();
            return;
        }
        this._stopAnimation();

        if (!focusedApp) {
            // If the app has just lost focus to the panel, pretend
            // nothing happened; otherwise you can't keynav to the
            // app menu.
            if (global.stage_input_mode == Shell.StageInputMode.FOCUSED)
                return;
        }

        if (this._iconBox.child != null)
            this._iconBox.child.destroy();
        this._iconBox.hide();
        this._label.setText('');
        this.actor.reactive = false;

        this._targetApp = targetApp;
        if (targetApp != null) {
            let icon = targetApp.get_faded_icon(2 * PANEL_ICON_SIZE);

            this._label.setText(targetApp.get_name());
            // TODO - _quit() doesn't really work on apps in state STARTING yet
            this._quitMenu.label.set_text(_("Quit %s").format(targetApp.get_name()));

            this.actor.reactive = true;
            this._iconBox.set_child(icon);
            this._iconBox.show();

            if (targetApp.get_state() == Shell.AppState.STARTING)
                this.startAnimation();
        }

        this.emit('changed');
    }
};

Signals.addSignalMethods(AppMenuButton.prototype);

function ClockButton() {
    this._init();
}

ClockButton.prototype = {
    __proto__: PanelMenu.Button.prototype,

    _init: function() {
        PanelMenu.Button.prototype._init.call(this, St.Align.START);
        this.menu.addAction(_("Preferences"), Lang.bind(this, this._onPrefs));

        this._clock = new St.Label();
        this.actor.set_child(this._clock);

        this._calendarPopup = null;

        this._clockSettings = new Gio.Settings({ schema: 'org.gnome.shell.clock' });
        this._clockSettings.connect('changed', Lang.bind(this, this._updateClock));

        // Start the clock
        this._updateClock();
    },

    _onButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 3 &&
            (!this._calendarPopup || !this._calendarPopup.isOpen))
            this.menu.toggle();
        else
            this._toggleCalendar();
    },

    closeCalendar: function() {
        if (!this._calendarPopup || !this._calendarPopup.isOpen)
            return;

        this._calendarPopup.hide();

        this.menu.isOpen = false;
        this.actor.remove_style_pseudo_class('pressed');
    },

    openCalendar: function() {
        this._calendarPopup.show();

        // simulate an open menu, so it won't appear beneath the calendar
        this.menu.isOpen = true;
        this.actor.add_style_pseudo_class('pressed');
    },

    _onPrefs: function() {
        let args = ['gnome-shell-clock-preferences'];
        let p = new Shell.Process({ args: args });

        p.run();
    },

    _toggleCalendar: function() {
        if (this._calendarPopup == null) {
            this._calendarPopup = new CalendarPopup();
            this._calendarPopup.actor.hide();
        }

        if (this.menu.isOpen && !this._calendarPopup.isOpen) {
            this.menu.close();
            return;
        }

        if (!this._calendarPopup.isOpen)
            this.openCalendar();
        else
            this.closeCalendar();
    },

    _updateClock: function() {
        let format = this._clockSettings.get_string(CLOCK_FORMAT_KEY);
        let showDate = this._clockSettings.get_boolean(CLOCK_SHOW_DATE_KEY);
        let showSeconds = this._clockSettings.get_boolean(CLOCK_SHOW_SECONDS_KEY);

        let clockFormat;
        switch (format) {
            case 'unix':
                // force updates every second
                showSeconds = true;
                clockFormat = '%s';
                break;
            case 'custom':
                // force updates every second
                showSeconds = true;
                clockFormat = this._clockSettings.get_string(CLOCK_CUSTOM_FORMAT_KEY);
                break;
            case '24-hour':
                if (showDate)
	            /* Translators: This is the time format with date used
                       in 24-hour mode. */
                    clockFormat = showSeconds ? _("%a %b %e, %R:%S")
                                              : _("%a %b %e, %R");
                else
	            /* Translators: This is the time format without date used
                       in 24-hour mode. */
                    clockFormat = showSeconds ? _("%a %R:%S")
                                              : _("%a %R");
                break;
            case '12-hour':
            default:
                if (showDate)
	            /* Translators: This is a time format with date used
                       for AM/PM. */
                    clockFormat = showSeconds ? _("%a %b %e, %l:%M:%S %p")
                                              : _("%a %b %e, %l:%M %p");
                else
	            /* Translators: This is a time format without date used
                       for AM/PM. */
                    clockFormat = showSeconds ? _("%a %l:%M:%S %p")
                                              : _("%a %l:%M %p");
                break;
        }

        let displayDate = new Date();
        let msecRemaining;
        if (showSeconds) {
            msecRemaining = 1000 - displayDate.getMilliseconds();
            if (msecRemaining < 50) {
                displayDate.setSeconds(displayDate.getSeconds() + 1);
                msecRemaining += 1000;
            }
        } else {
            msecRemaining = 60000 - (1000 * displayDate.getSeconds() +
                                     displayDate.getMilliseconds());
            if (msecRemaining < 500) {
                displayDate.setMinutes(displayDate.getMinutes() + 1);
                msecRemaining += 60000;
            }
        }

        this._clock.set_text(displayDate.toLocaleFormat(clockFormat));
        Mainloop.timeout_add(msecRemaining, Lang.bind(this, this._updateClock));
        return false;
    }
};

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
        this.actor = new St.BoxLayout({ style_class: 'menu-bar',
                                        name: 'panel',
                                        reactive: true });
        this.actor._delegate = this;

        Main.overview.connect('shown', Lang.bind(this, function () {
            this.actor.add_style_class_name('in-overview');
        }));
        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.actor.remove_style_class_name('in-overview');
        }));

        this._menus = new PopupMenu.PopupMenuManager(this);

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

            let sideWidth, centerWidth;
            centerWidth = centerNaturalWidth;
            sideWidth = (allocWidth - centerWidth) / 2;

            let childBox = new Clutter.ActorBox();
            childBox.y1 = 0;
            childBox.y2 = this._hotCornerEnvirons.height;
            if (this.actor.get_direction() == St.TextDirection.RTL) {
                childBox.x1 = allocWidth - this._hotCornerEnvirons.width;
                childBox.x2 = allocWidth;
            } else {
                childBox.x1 = 0;
                childBox.x2 = this._hotCornerEnvirons.width;
            }
            this._hotCornerEnvirons.allocate(childBox, flags);

            childBox.y1 = 0;
            childBox.y2 = this._hotCorner.height;
            if (this.actor.get_direction() == St.TextDirection.RTL) {
                childBox.x1 = allocWidth - this._hotCorner.width;
                childBox.x2 = allocWidth;
            } else {
                childBox.x1 = 0;
                childBox.x2 = this._hotCorner.width;
            }
            this._hotCorner.allocate(childBox, flags);

            childBox.y1 = 0;
            childBox.y2 = allocHeight;
            if (this.actor.get_direction() == St.TextDirection.RTL) {
                childBox.x1 = allocWidth - Math.min(Math.floor(sideWidth),
                                                    leftNaturalWidth);
                childBox.x2 = allocWidth;
            } else {
                childBox.x1 = 0;
                childBox.x2 = Math.min(Math.floor(sideWidth),
                                       leftNaturalWidth);
            }
            this._leftBox.allocate(childBox, flags);

            childBox.x1 = Math.ceil(sideWidth);
            childBox.y1 = 0;
            childBox.x2 = childBox.x1 + centerWidth;
            childBox.y2 = allocHeight;
            this._centerBox.allocate(childBox, flags);

            childBox.y1 = 0;
            childBox.y2 = allocHeight;
            if (this.actor.get_direction() == St.TextDirection.RTL) {
                childBox.x1 = 0;
                childBox.x2 = Math.min(Math.floor(sideWidth),
                                       rightNaturalWidth);
            } else {
                childBox.x1 = allocWidth - Math.min(Math.floor(sideWidth),
                                                    rightNaturalWidth);
                childBox.x2 = allocWidth;
            }
            this._rightBox.allocate(childBox, flags);
        }));

        /* Button on the left side of the panel. */
        /* Translators: If there is no suitable word for "Activities" in your language, you can use the word for "Overview". */
        let label = new St.Label({ text: _("Activities") });
        this.button = new St.Clickable({ name: 'panelActivities',
                                         style_class: 'panel-button',
                                         reactive: true,
                                         can_focus: true });
        this.button.set_child(label);

        this._leftBox.add(this.button);

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

        this._boxContainer.add_actor(this._hotCornerEnvirons);
        this._boxContainer.add_actor(this._hotCorner);

        let appMenuButton = new AppMenuButton();
        this._leftBox.add(appMenuButton.actor);

        this._menus.addMenu(appMenuButton.menu);

        /* center */
        this._dateMenu = new DateMenu.DateMenuButton();
        this._centerBox.add(this._dateMenu.actor, { y_fill: true });
        this._menus.addMenu(this._dateMenu.menu);

        /* right */

        // System status applets live in statusBox, while legacy tray icons
        // live in trayBox
        // The trayBox is hidden when there are no tray icons.
        let statusBox = new St.BoxLayout({ name: 'statusTray' });
        let trayBox = new St.BoxLayout({ name: 'legacyTray' });
        this._trayBox = trayBox;
        this._statusBox = statusBox;

        trayBox.hide();
        this._rightBox.add(trayBox);
        this._rightBox.add(statusBox);

        for (let i = 0; i < STANDARD_TRAY_ICON_ORDER.length; i++) {
            let role = STANDARD_TRAY_ICON_ORDER[i];
            let constructor = STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION[role];
            if (!constructor) {
                // This icon is not implemented (this is a bug)
                continue;
            }
            let indicator = new constructor();
            statusBox.add(indicator.actor);
            this._menus.addMenu(indicator.menu);
        }

        Main.statusIconDispatcher.connect('status-icon-added', Lang.bind(this, this._onTrayIconAdded));
        Main.statusIconDispatcher.connect('status-icon-removed', Lang.bind(this, this._onTrayIconRemoved));

        this._statusmenu = new StatusMenu.StatusMenuButton();
        this._menus.addMenu(this._statusmenu.menu);
        this._rightBox.add(this._statusmenu.actor);

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
    },

    startupAnimation: function() {
        this.actor.y = -this.actor.height;
        Tweener.addTween(this.actor,
                         { y: 0,
                           time: 0.2,
                           transition: 'easeOutQuad'
                         });
    },

    _onTrayIconAdded: function(o, icon, role) {
        icon.height = PANEL_ICON_SIZE;

        if (STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION[role]) {
            // This icon is legacy, and replaced by a Shell version
            // Hide it
            return;
        }
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
                this._trayBox.insert_actor(icon, i + 1);
                break;
            }
        }
        if (i == -1) {
            // If we didn't find a position, we must be first
            this._trayBox.insert_actor(icon, 0);
        }

        // Make sure the trayBox is shown.
        this._trayBox.show();
    },

    _onTrayIconRemoved: function(o, icon) {
        if (icon.get_parent() != null)
            this._trayBox.remove_actor(icon);
    },

    _addRipple : function(delay, time, startScale, startOpacity, finalScale, finalOpacity) {
        // We draw a ripple by using a source image and animating it scaling
        // outwards and fading away. We want the ripples to move linearly
        // or it looks unrealistic, but if the opacity of the ripple goes
        // linearly to zero it fades away too quickly, so we use Tweener's
        // 'onUpdate' to give a non-linear curve to the fade-away and make
        // it more visible in the middle section.

        let [x, y] = this._hotCorner.get_transformed_position();
        let ripple = new St.BoxLayout({ style_class: 'ripple-box',
                                        opacity: 255 * Math.sqrt(startOpacity),
                                        scale_x: startScale,
                                        scale_y: startScale,
                                        x: x,
                                        y: y });
        ripple._opacity =  startOpacity;
        Tweener.addTween(ripple, { _opacity: finalOpacity,
                                   scale_x: finalScale,
                                   scale_y: finalScale,
                                   delay: delay,
                                   time: time,
                                   transition: 'linear',
                                   onUpdate: function() { ripple.opacity = 255 * Math.sqrt(ripple._opacity); },
                                   onComplete: function() { ripple.destroy(); } });
        Main.uiGroup.add_actor(ripple);
    },

    _onHotCornerEntered : function() {
        if (this._menus.grabbed)
            return false;
        if (!this._hotCornerEntered) {
            this._hotCornerEntered = true;
            if (!Main.overview.animationInProgress) {
                this._hotCornerActivationTime = Date.now() / 1000;

                // Show three concentric ripples expanding outwards; the exact
                // parameters were found by trial and error, so don't look
                // for them to make perfect sense mathematically

                //              delay  time  scale opacity => scale opacity
                this._addRipple(0.0,   0.83,  0.25,  1.0,    1.5,  0.0);
                this._addRipple(0.05,  1.0,   0.0,   0.7,    1.25, 0.0);
                this._addRipple(0.35,  1.0,   0.0,   0.3,    1,    0.0);
                Main.overview.toggle();
            }
        }
        return false;
    },

    _onHotCornerClicked : function() {
        if (this._menus.grabbed)
            return false;
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

        this.actor = new St.Bin({ name: 'calendarPopup' });

        this.calendar = new Calendar.Calendar();
        this.actor.set_child(this.calendar.actor);

        this.isOpen = false;

        Main.chrome.addActor(this.actor, { visibleInOverview: true,
                                           affectsStruts: false });
        this.actor.y = (panelActor.y + panelActor.height - this.actor.height);
        this.calendar.actor.connect('notify::width', Lang.bind(this, this._centerPopup));
    },

    show: function() {
        let panelActor = Main.panel.actor;

        if (this.isOpen)
            return;
        this.isOpen = true;

        // Reset the calendar to today's date
        this.calendar.setDate(new Date());

        this._centerPopup();
        this.actor.lower(panelActor);
        this.actor.show();
        Tweener.addTween(this.actor,
                         { y: panelActor.y + panelActor.height,
                           time: 0.2,
                           transition: 'easeOutQuad'
                         });
    },

    hide: function() {
        let panelActor = Main.panel.actor;

        if (!this.isOpen)
            return;
        this.isOpen = false;

        Tweener.addTween(this.actor,
                         { y: panelActor.y + panelActor.height - this.actor.height,
                           time: 0.2,
                           transition: 'easeOutQuad',
                           onComplete: function() { this.actor.hide(); },
                           onCompleteScope: this
                         });
    },

    _centerPopup: function() {
        let panelActor = Main.panel.actor;
        this.actor.x = Math.round(panelActor.x + (panelActor.width - this.actor.width) / 2);
    }
};
