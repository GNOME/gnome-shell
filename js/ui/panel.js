/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;

const Config = imports.misc.config;
const CtrlAltTab = imports.ui.ctrlAltTab;
const Overview = imports.ui.overview;
const PopupMenu = imports.ui.popupMenu;
const PanelMenu = imports.ui.panelMenu;
const StatusMenu = imports.ui.statusMenu;
const DateMenu = imports.ui.dateMenu;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const PANEL_ICON_SIZE = 24;

const STARTUP_ANIMATION_TIME = 0.2;

const HOT_CORNER_ACTIVATION_TIMEOUT = 0.5;

const BUTTON_DND_ACTIVATION_TIMEOUT = 250;

const ANIMATED_ICON_UPDATE_TIMEOUT = 100;
const SPINNER_ANIMATION_TIME = 0.2;

const STANDARD_TRAY_ICON_ORDER = ['a11y', 'display', 'keyboard', 'volume', 'bluetooth', 'network', 'battery'];
const STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION = {
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'volume': imports.ui.status.volume.Indicator,
    'battery': imports.ui.status.power.Indicator,
    'keyboard': imports.ui.status.keyboard.XKBIndicator
};

if (Config.HAVE_BLUETOOTH)
    STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION['bluetooth'] = imports.ui.status.bluetooth.Indicator;

try {
    STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION['network'] = imports.ui.status.network.NMApplet;
} catch(e) {
    log('NMApplet is not supported. It is possible that your NetworkManager version is too old');
}

// To make sure the panel corners blend nicely with the panel,
// we draw background and borders the same way, e.g. drawing
// them as filled shapes from the outside inwards instead of
// using cairo stroke(). So in order to give the border the
// appearance of being drawn on top of the background, we need
// to blend border and background color together.
// For that purpose we use the following helper methods, taken
// from st-theme-node-drawing.c
function _norm(x) {
    return Math.round(x / 255);
}

function _over(srcColor, dstColor) {
    let src = _premultiply(srcColor);
    let dst = _premultiply(dstColor);
    let result = new Clutter.Color();

    result.alpha = src.alpha + _norm((255 - src.alpha) * dst.alpha);
    result.red = src.red + _norm((255 - src.alpha) * dst.red);
    result.green = src.green + _norm((255 - src.alpha) * dst.green);
    result.blue = src.blue + _norm((255 - src.alpha) * dst.blue);

    return _unpremultiply(result);
}

function _premultiply(color) {
    return new Clutter.Color({ red: _norm(color.red * color.alpha),
                               green: _norm(color.green * color.alpha),
                               blue: _norm(color.blue * color.alpha),
                               alpha: color.alpha });
};

function _unpremultiply(color) {
    if (color.alpha == 0)
        return new Clutter.Color();

    let red = Math.min((color.red * 255 + 127) / color.alpha, 255);
    let green = Math.min((color.green * 255 + 127) / color.alpha, 255);
    let blue = Math.min((color.blue * 255 + 127) / color.alpha, 255);
    return new Clutter.Color({ red: red, green: green,
                               blue: blue, alpha: color.alpha });
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
        PanelMenu.Button.prototype._init.call(this, 0.0);
        this._metaDisplay = global.screen.get_display();
        this._startingApps = [];

        this._targetApp = null;

        let bin = new St.Bin({ name: 'appMenu' });
        this.actor.set_child(bin);

        this.actor.reactive = false;
        this._targetIsCurrent = false;

        this._container = new Shell.GenericContainer();
        bin.set_child(this._container);
        this._container.connect('get-preferred-width', Lang.bind(this, this._getContentPreferredWidth));
        this._container.connect('get-preferred-height', Lang.bind(this, this._getContentPreferredHeight));
        this._container.connect('allocate', Lang.bind(this, this._contentAllocate));

        this._iconBox = new Shell.Slicer({ name: 'appMenuIcon' });
        this._iconBox.connect('style-changed',
                              Lang.bind(this, this._onIconBoxStyleChanged));
        this._iconBox.connect('notify::allocation',
                              Lang.bind(this, this._updateIconBoxClip));
        this._container.add_actor(this._iconBox);
        this._label = new TextShadower();
        this._container.add_actor(this._label.actor);

        this._iconBottomClip = 0;

        this._quitMenu = new PopupMenu.PopupMenuItem('');
        this.menu.addMenuItem(this._quitMenu);
        this._quitMenu.connect('activate', Lang.bind(this, this._onQuit));

        this._visible = !Main.overview.visible;
        if (!this._visible)
            this.actor.hide();
        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.show();
        }));
        Main.overview.connect('showing', Lang.bind(this, function () {
            this.hide();
        }));

        this._stop = true;

        this._spinner = new AnimatedIcon('process-working.svg',
                                         PANEL_ICON_SIZE);
        this._container.add_actor(this._spinner.actor);
        this._spinner.actor.lower_bottom();

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('notify::focus-app', Lang.bind(this, this._sync));
        tracker.connect('app-state-changed', Lang.bind(this, this._onAppStateChanged));

        global.window_manager.connect('switch-workspace', Lang.bind(this, this._sync));

        this._sync();
    },

    show: function() {
        if (this._visible)
            return;

        this._visible = true;
        this.actor.show();
        this.actor.reactive = true;

        if (!this._targetIsCurrent)
            return;

        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad' });
    },

    hide: function() {
        if (!this._visible)
            return;

        this._visible = false;
        this.actor.reactive = false;
        if (!this._targetIsCurrent) {
            this.actor.hide();
            return;
        }

        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: Overview.ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: function() {
                               this.actor.hide();
                           },
                           onCompleteScope: this });
    },

    _onIconBoxStyleChanged: function() {
        let node = this._iconBox.get_theme_node();
        this._iconBottomClip = node.get_length('app-icon-bottom-clip');
        this._updateIconBoxClip();
    },

    _updateIconBoxClip: function() {
        let allocation = this._iconBox.allocation;
        if (this._iconBottomClip > 0)
            this._iconBox.set_clip(0, 0,
                                   allocation.x2 - allocation.x1,
                                   allocation.y2 - allocation.y1 - this._iconBottomClip);
        else
            this._iconBox.remove_clip();
    },

    stopAnimation: function() {
        if (this._stop)
            return;

        this._stop = true;
        Tweener.addTween(this._spinner.actor,
                         { opacity: 0,
                           time: SPINNER_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onCompleteScope: this,
                           onComplete: function() {
                               this._spinner.actor.opacity = 255;
                               this._spinner.actor.hide();
                           }
                         });
    },

    startAnimation: function() {
        this._stop = false;
        this._spinner.actor.show();
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
        [minSize, naturalSize] = this._label.actor.get_preferred_height(forWidth);
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
            childBox.x1 = Math.floor(iconWidth / 2) + this._label.actor.width;
            childBox.x2 = childBox.x1 + this._spinner.actor.width;
            childBox.y1 = box.y1;
            childBox.y2 = box.y2 - 1;
            this._spinner.actor.allocate(childBox, flags);
        } else {
            childBox.x1 = -this._spinner.actor.width;
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

        if (!focusedApp) {
            // If the app has just lost focus to the panel, pretend
            // nothing happened; otherwise you can't keynav to the
            // app menu.
            if (global.stage_input_mode == Shell.StageInputMode.FOCUSED)
                return;
        }

        let targetApp = focusedApp != null ? focusedApp : lastStartedApp;

        if (targetApp == null) {
            if (!this._targetIsCurrent)
                return;

            this.actor.reactive = false;
            this._targetIsCurrent = false;

            Tweener.removeTweens(this.actor);
            Tweener.addTween(this.actor, { opacity: 0,
                                           time: Overview.ANIMATION_TIME,
                                           transition: 'easeOutQuad' });
            return;
        }

        if (!this._targetIsCurrent) {
            this.actor.reactive = true;
            this._targetIsCurrent = true;

            Tweener.removeTweens(this.actor);
            Tweener.addTween(this.actor, { opacity: 255,
                                           time: Overview.ANIMATION_TIME,
                                           transition: 'easeOutQuad' });
        }

        if (targetApp == this._targetApp) {
            if (targetApp && targetApp.get_state() != Shell.AppState.STARTING)
                this.stopAnimation();
            return;
        }

        this._spinner.actor.hide();
        if (this._iconBox.child != null)
            this._iconBox.child.destroy();
        this._iconBox.hide();
        this._label.setText('');

        this._targetApp = targetApp;
        let icon = targetApp.get_faded_icon(2 * PANEL_ICON_SIZE);

        this._label.setText(targetApp.get_name());
        // TODO - _quit() doesn't really work on apps in state STARTING yet
        this._quitMenu.label.set_text(_("Quit %s").format(targetApp.get_name()));

        this._iconBox.set_child(icon);
        this._iconBox.show();

        if (targetApp.get_state() == Shell.AppState.STARTING)
            this.startAnimation();

        this.emit('changed');
    }
};

Signals.addSignalMethods(AppMenuButton.prototype);


function PanelCorner(panel, side) {
    this._init(panel, side);
}

PanelCorner.prototype = {
    _init: function(panel, side) {
        this._panel = panel;
        this._side = side;
        this.actor = new St.DrawingArea({ style_class: 'panel-corner' });
        this.actor.connect('repaint', Lang.bind(this, this._repaint));
        this.actor.connect('style-changed', Lang.bind(this, this.relayout));
    },

    _repaint: function() {
        let node = this.actor.get_theme_node();

        let cornerRadius = node.get_length("-panel-corner-radius");
        let innerBorderWidth = node.get_length('-panel-corner-inner-border-width');
        let outerBorderWidth = node.get_length('-panel-corner-outer-border-width');

        let backgroundColor = node.get_color('-panel-corner-background-color');
        let innerBorderColor = node.get_color('-panel-corner-inner-border-color');
        let outerBorderColor = node.get_color('-panel-corner-outer-border-color');

        let cr = this.actor.get_context();
        cr.setOperator(Cairo.Operator.SOURCE);

        cr.moveTo(0, 0);
        if (this._side == St.Side.LEFT)
            cr.arc(cornerRadius,
                   innerBorderWidth + cornerRadius,
                   cornerRadius, Math.PI, 3 * Math.PI / 2);
        else
            cr.arc(0,
                   innerBorderWidth + cornerRadius,
                   cornerRadius, 3 * Math.PI / 2, 2 * Math.PI);
        cr.lineTo(cornerRadius, 0);
        cr.closePath();

        let savedPath = cr.copyPath();

        let over = _over(innerBorderColor,
                         _over(outerBorderColor, backgroundColor));
        Clutter.cairo_set_source_color(cr, over);
        cr.fill();

        let xOffsetDirection = this._side == St.Side.LEFT ? -1 : 1;
        let offset = outerBorderWidth;
        over = _over(innerBorderColor, backgroundColor);
        Clutter.cairo_set_source_color(cr, over);

        cr.save();
        cr.translate(xOffsetDirection * offset, - offset);
        cr.appendPath(savedPath);
        cr.fill();
        cr.restore();

        if (this._side == St.Side.LEFT)
            cr.rectangle(cornerRadius - offset, 0, offset, outerBorderWidth);
        else
            cr.rectangle(0, 0, offset, outerBorderWidth);
        cr.fill();

        offset = innerBorderWidth;
        Clutter.cairo_set_source_color(cr, backgroundColor);

        cr.save();
        cr.translate(xOffsetDirection * offset, - offset);
        cr.appendPath(savedPath);
        cr.fill();
        cr.restore();
    },

    relayout: function() {
        let node = this.actor.get_theme_node();

        let cornerRadius = node.get_length("-panel-corner-radius");
        let innerBorderWidth = node.get_length('-panel-corner-inner-border-width');

        this.actor.set_size(cornerRadius,
                            innerBorderWidth + cornerRadius);
        if (this._side == St.Side.LEFT)
            this.actor.set_position(this._panel.actor.x,
                                    this._panel.actor.y + this._panel.actor.height - innerBorderWidth);
        else
            this.actor.set_position(this._panel.actor.x + this._panel.actor.width - cornerRadius,
                                    this._panel.actor.y + this._panel.actor.height - innerBorderWidth);
    }
};

/**
 * HotCorner:
 *
 * This class manages the "hot corner" that can toggle switching to
 * overview.
 */
function HotCorner(button) {
    this._init(button);
}

HotCorner.prototype = {
    _init : function(button) {
        // This is the activities button associated with this hot corner,
        // if this is on the primary monitor (or null with the corner is
        // on a different monitor)
        this._button = button;

        // We use this flag to mark the case where the user has entered the
        // hot corner and has not left both the hot corner and a surrounding
        // guard area (the "environs"). This avoids triggering the hot corner
        // multiple times due to an accidental jitter.
        this._entered = false;

        this.actor = new Clutter.Group({ width: 3,
                                         height: 3,
                                         reactive: true });

        this._corner = new Clutter.Rectangle({ width: 1,
                                               height: 1,
                                               opacity: 0,
                                               reactive: true });

        this.actor.add_actor(this._corner);

        if (St.Widget.get_default_direction() == St.TextDirection.RTL) {
            this._corner.set_position(this.actor.width - this._corner.width, 0);
            this.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
        } else {
            this._corner.set_position(0, 0);
        }

        this._activationTime = 0;

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onEnvironsEntered));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onEnvironsLeft));
        // Clicking on the hot corner environs should result in the same bahavior
        // as clicking on the hot corner.
        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onCornerClicked));

        // In addition to being triggered by the mouse enter event, the hot corner
        // can be triggered by clicking on it. This is useful if the user wants to
        // undo the effect of triggering the hot corner once in the hot corner.
        this._corner.connect('enter-event',
                             Lang.bind(this, this._onCornerEntered));
        this._corner.connect('button-release-event',
                             Lang.bind(this, this._onCornerClicked));
        this._corner.connect('leave-event',
                             Lang.bind(this, this._onCornerLeft));

        this._corner._delegate = this._corner;
        this._corner.handleDragOver = Lang.bind(this,
            function(source, actor, x, y, time) {
                 if (source == Main.xdndHandler) {
                    if(!Main.overview.visible && !Main.overview.animationInProgress) {
                        this.rippleAnimation();
                        Main.overview.showTemporarily();
                        Main.overview.beginItemDrag(actor);
                    }
                 }
            });

        Main.chrome.addActor(this.actor, { affectsStruts: false });
    },

    destroy: function() {
        this.actor.destroy();
    },

    _addRipple : function(delay, time, startScale, startOpacity, finalScale, finalOpacity) {
        // We draw a ripple by using a source image and animating it scaling
        // outwards and fading away. We want the ripples to move linearly
        // or it looks unrealistic, but if the opacity of the ripple goes
        // linearly to zero it fades away too quickly, so we use Tweener's
        // 'onUpdate' to give a non-linear curve to the fade-away and make
        // it more visible in the middle section.

        let [x, y] = this._corner.get_transformed_position();
        let ripple = new St.BoxLayout({ style_class: 'ripple-box',
                                        opacity: 255 * Math.sqrt(startOpacity),
                                        scale_x: startScale,
                                        scale_y: startScale,
                                        x: x,
                                        y: y });
        ripple._opacity =  startOpacity;
        if (ripple.get_direction() == St.TextDirection.RTL)
            ripple.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
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

    rippleAnimation: function() {
        // Show three concentric ripples expanding outwards; the exact
        // parameters were found by trial and error, so don't look
        // for them to make perfect sense mathematically

        //              delay  time  scale opacity => scale opacity
        this._addRipple(0.0,   0.83,  0.25,  1.0,    1.5,  0.0);
        this._addRipple(0.05,  1.0,   0.0,   0.7,    1.25, 0.0);
        this._addRipple(0.35,  1.0,   0.0,   0.3,    1,    0.0);
    },

    _onEnvironsEntered : function() {
        if (this._button)
            this._button.hover = true;
    },

    _onCornerEntered : function() {
        if (!this._entered) {
            this._entered = true;
            if (!Main.overview.animationInProgress) {
                this._activationTime = Date.now() / 1000;

                this.rippleAnimation();
                Main.overview.toggle();
            }
        }
        return false;
    },

    _onCornerClicked : function() {
         if (!Main.overview.animationInProgress)
             this.maybeToggleOverviewOnClick();
         return true;
    },

    _onCornerLeft : function(actor, event) {
        if (event.get_related() != this.actor)
            this._entered = false;
        // Consume event, otherwise this will confuse onEnvironsLeft
        return true;
    },

    _onEnvironsLeft : function(actor, event) {
        if (this._button)
            this._button.hover = false;

        if (event.get_related() != this._corner)
            this._entered = false;
        return false;
    },

    // Toggles the overview unless this is the first click on the Activities button within the HOT_CORNER_ACTIVATION_TIMEOUT time
    // of the hot corner being triggered. This check avoids opening and closing the overview if the user both triggered the hot corner
    // and clicked the Activities button.
    maybeToggleOverviewOnClick: function() {
        if (this._activationTime == 0 || Date.now() / 1000 - this._activationTime > HOT_CORNER_ACTIVATION_TIMEOUT)
            Main.overview.toggle();
        this._activationTime = 0;
    }
}


function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
        this.actor = new St.BoxLayout({ style_class: 'menu-bar',
                                        name: 'panel',
                                        reactive: true });
        this.actor._delegate = this;

        this._statusArea = {};

        Main.overview.connect('shown', Lang.bind(this, function () {
            this.actor.add_style_class_name('in-overview');
        }));
        Main.overview.connect('hiding', Lang.bind(this, function () {
            this.actor.remove_style_class_name('in-overview');
        }));

        this._leftPointerBarrier = 0;
        this._rightPointerBarrier = 0;
        this._menus = new PopupMenu.PopupMenuManager(this);

        this._leftBox = new St.BoxLayout({ name: 'panelLeft' });
        this._centerBox = new St.BoxLayout({ name: 'panelCenter' });
        this._rightBox = new St.BoxLayout({ name: 'panelRight' });

        this._leftCorner = new PanelCorner(this, St.Side.LEFT);
        this._rightCorner = new PanelCorner(this, St.Side.RIGHT);

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
        this.button = new St.Button({ name: 'panelActivities',
                                      style_class: 'panel-button',
                                      reactive: true,
                                      can_focus: true });
        this._activities = this.button;
        this.button.set_child(label);
        this.button._delegate = this.button;
        this.button._xdndTimeOut = 0;
        this.button.handleDragOver = Lang.bind(this,
            function(source, actor, x, y, time) {
                 if (source == Main.xdndHandler) {
                    if (this.button._xdndTimeOut != 0)
                        Mainloop.source_remove(this.button._xdndTimeOut);
                    this.button._xdndTimeOut = Mainloop.timeout_add(BUTTON_DND_ACTIVATION_TIMEOUT,
                                                                    Lang.bind(this,
                                                                                function() {
                                                                                    this._xdndShowOverview(actor);
                                                                                }));
                 }
            });
        this._leftBox.add(this.button);

        // Synchronize the buttons pseudo classes with its corner
        this.button.connect('style-changed', Lang.bind(this,
            function(actor) {
                let rtl = actor.get_direction() == St.TextDirection.RTL;
                let corner = rtl ? this._rightCorner : this._leftCorner;
                let pseudoClass = actor.get_style_pseudo_class();
                corner.actor.set_style_pseudo_class(pseudoClass);
            }));

        this._hotCorner = null;

        this._appMenu = new AppMenuButton();
        this._leftBox.add(this._appMenu.actor);

        this._menus.addMenu(this._appMenu.menu);

        /* center */
        this._dateMenu = new DateMenu.DateMenuButton();
        this._centerBox.add(this._dateMenu.actor, { y_fill: true });
        this._menus.addMenu(this._dateMenu.menu);

        /* right */

        // System status applets live in statusBox, while legacy tray icons
        // live in trayBox
        // The trayBox is hidden when there are no tray icons.
        this._trayBox = new St.BoxLayout({ name: 'legacyTray' });
        this._statusBox = new St.BoxLayout({ name: 'statusTray' });

        this._trayBox.hide();
        this._rightBox.add(this._trayBox);
        this._rightBox.add(this._statusBox);

        this._userMenu = new StatusMenu.StatusMenuButton();
        this._userMenu.actor.name = 'panelStatus';
        this._rightBox.add(this._userMenu.actor);

        // Synchronize the buttons pseudo classes with its corner
        this._userMenu.actor.connect('style-changed', Lang.bind(this,
            function(actor) {
                let rtl = actor.get_direction() == St.TextDirection.RTL;
                let corner = rtl ? this._leftCorner : this._rightCorner;
                let pseudoClass = actor.get_style_pseudo_class();
                corner.actor.set_style_pseudo_class(pseudoClass);
            }));

        Main.statusIconDispatcher.connect('status-icon-added', Lang.bind(this, this._onTrayIconAdded));
        Main.statusIconDispatcher.connect('status-icon-removed', Lang.bind(this, this._onTrayIconRemoved));

        // TODO: decide what to do with the rest of the panel in the Overview mode (make it fade-out, become non-reactive, etc.)
        // We get into the Overview mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the Overview act like a menu that allows the user to release the mouse on the activity the user wants
        // to switch to.
        this.button.connect('clicked', Lang.bind(this, function(b) {
            if (!Main.overview.animationInProgress) {
                this._hotCorner.maybeToggleOverviewOnClick();
                return true;
            } else {
                return false;
            }
        }));
        // In addition to pressing the button, the Overview can be entered and exited by other means, such as
        // pressing the System key, Alt+F1 or Esc. We want the button to be pressed in when the Overview is entered
        // and to be released when it is exited regardless of how it was triggered.
        Main.overview.connect('showing', Lang.bind(this, function() {
            this.button.checked = true;
        }));
        Main.overview.connect('hiding', Lang.bind(this, function() {
            this.button.checked = false;
        }));

        Main.chrome.addActor(this.actor);
        Main.chrome.addActor(this._leftCorner.actor, { affectsStruts: false,
                                                       affectsInputRegion: false });
        Main.chrome.addActor(this._rightCorner.actor, { affectsStruts: false,
                                                        affectsInputRegion: false });

        Main.ctrlAltTabManager.addGroup(this.actor, _("Top Bar"), 'start-here',
                                        { sortGroup: CtrlAltTab.SortGroup.TOP });
    },

    _xdndShowOverview: function (actor) {
        let [x, y, mask] = global.get_pointer();
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);

        if (pickedActor != this.button) {
            Mainloop.source_remove(this.button._xdndTimeOut);
            this.button._xdndTimeOut = 0;
            return;
        }

        if(!Main.overview.visible && !Main.overview.animationInProgress) {
            Main.overview.showTemporarily();
            Main.overview.beginItemDrag(actor);
        }

        Mainloop.source_remove(this.button._xdndTimeOut);
        this.button._xdndTimeOut = 0;
    },


    // While there can be multiple hotcorners (one per monitor), the hot corner
    // that is on top of the Activities button is special since it needs special
    // coordination with clicking on that button
    setHotCorner: function(corner) {
        this._hotCorner = corner;
    },

    startStatusArea: function() {
        for (let i = 0; i < STANDARD_TRAY_ICON_ORDER.length; i++) {
            let role = STANDARD_TRAY_ICON_ORDER[i];
            let constructor = STANDARD_TRAY_ICON_SHELL_IMPLEMENTATION[role];
            if (!constructor) {
                // This icon is not implemented (this is a bug)
                continue;
            }
            let indicator = new constructor();
            this._statusBox.add(indicator.actor);
            this._menus.addMenu(indicator.menu);

            this._statusArea[role] = indicator;
        }

        // PopupMenuManager depends on menus being added in order for
        // keyboard navigation
        this._menus.addMenu(this._userMenu.menu);
    },

    startupAnimation: function() {
        let oldY = this.actor.y;
        this.actor.y = oldY - this.actor.height;
        Tweener.addTween(this.actor,
                         { y: oldY,
                           time: STARTUP_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });

        let oldCornerY = this._leftCorner.actor.y;
        this._leftCorner.actor.y = oldCornerY - this.actor.height;
        this._rightCorner.actor.y = oldCornerY - this.actor.height;
        Tweener.addTween(this._leftCorner.actor,
                         { y: oldCornerY,
                           time: STARTUP_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
        Tweener.addTween(this._rightCorner.actor,
                         { y: oldCornerY,
                           time: STARTUP_ANIMATION_TIME,
                           transition: 'easeOutQuad'
                         });
    },

    relayout: function() {
        let primary = global.get_primary_monitor();

        this.actor.set_position(primary.x, primary.y);
        this.actor.set_size(primary.width, -1);

        if (this._leftPointerBarrier)
            global.destroy_pointer_barrier(this._leftPointerBarrier);
        if (this._rightPointerBarrier)
            global.destroy_pointer_barrier(this._rightPointerBarrier);

        this._leftPointerBarrier =
            global.create_pointer_barrier(primary.x, primary.y,
                                          primary.x, primary.y + this.actor.height,
                                          1 /* BarrierPositiveX */);
        this._rightPointerBarrier =
            global.create_pointer_barrier(primary.x + primary.width, primary.y,
                                          primary.x + primary.width, primary.y + this.actor.height,
                                          4 /* BarrierNegativeX */);

        this._leftCorner.relayout();
        this._rightCorner.relayout();
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

};
