// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;
const TweenerEquations = imports.tweener.equations;

const GnomeSession = imports.misc.gnomeSession;
const Layout = imports.ui.layout;
const LoginManager = imports.misc.loginManager;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const MessageTray = imports.ui.messageTray;
const ShellDBus = imports.ui.shellDBus;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';

const CURTAIN_SLIDE_TIME = 0.3;
// fraction of screen height the arrow must reach before completing
// the slide up automatically
const ARROW_DRAG_THRESHOLD = 0.1;

// Parameters for the arrow animation
const N_ARROWS = 3;
const ARROW_ANIMATION_TIME = 0.6;
const ARROW_ANIMATION_PEAK_OPACITY = 0.4;

// The distance in px that the lock screen will move to when pressing
// a key that has no effect in the lock screen (bumping it)
const BUMP_SIZE = 25;
const BUMP_TIME = 0.3;

const SUMMARY_ICON_SIZE = 48;

// Lightbox fading times
// STANDARD_FADE_TIME is used when the session goes idle, while
// SHORT_FADE_TIME is used when requesting lock explicitly from the user menu
const STANDARD_FADE_TIME = 10;
const SHORT_FADE_TIME = 0.3;

function sample(offx, offy) {
    return 'texel += texture2D (sampler, tex_coord.st + pixel_step * ' +
        'vec2 (' + offx + ',' + offy + ') * 2.0);\n'
}
const GLSL_BLUR_EFFECT_DECLARATIONS = ' \
uniform vec2 pixel_step;\n \
vec4 apply_blur(in sampler2D sampler, in vec2 tex_coord) {\n \
  vec4 texel;\n \
  texel = texture2D (sampler, tex_coord.st);\n'
  + sample(-1.0, -1.0)
  + sample( 0.0, -1.0)
  + sample(+1.0, -1.0)
  + sample(-1.0,  0.0)
  + sample( 0.0,  0.0)
  + sample(+1.0,  0.0)
  + sample(-1.0, +1.0)
  + sample( 0.0, +1.0)
  + sample(+1.0, +1.0) + ' \
   texel /= 9.0;\n \
   return texel;\n \
}\n \
vec3 desaturate (const vec3 color, const float desaturation)\n \
{\n \
   const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n \
   vec3 gray = vec3 (dot (gray_conv, color));\n \
   return vec3 (mix (color.rgb, gray, desaturation));\n \
}';
const GLSL_BLUR_EFFECT_CODE = ' \
cogl_texel = apply_blur(cogl_sampler, cogl_tex_coord.st);\n \
cogl_texel.rgb = desaturate(cogl_texel.rgb, 0.6);\n';


const Clock = new Lang.Class({
    Name: 'ScreenShieldClock',

    CLOCK_FORMAT_KEY: 'clock-format',
    CLOCK_SHOW_SECONDS_KEY: 'clock-show-seconds',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'screen-shield-clock',
                                        vertical: true });

        this._time = new St.Label({ style_class: 'screen-shield-clock-time' });
        this._date = new St.Label({ style_class: 'screen-shield-clock-date' });

        this.actor.add(this._time, { x_align: St.Align.MIDDLE });
        this.actor.add(this._date, { x_align: St.Align.MIDDLE });

        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', Lang.bind(this, this._updateClock));

        this._updateClock();
    },

    _updateClock: function() {
        this._time.text = this._wallClock.clock;

        let date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        this._date.text = date.toLocaleFormat(_("%A, %B %d"));
    },

    destroy: function() {
        this.actor.destroy();
        this._wallClock.run_dispose();
    }
});

const NotificationsBox = new Lang.Class({
    Name: 'NotificationsBox',

    _init: function() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        name: 'screenShieldNotifications',
                                        style_class: 'screen-shield-notifications-box' });

        this._residentNotificationBox = new St.BoxLayout({ vertical: true,
                                                           style_class: 'screen-shield-notifications-box' });
        let scrollView = new St.ScrollView({ x_fill: false, x_align: St.Align.START });
        this._persistentNotificationBox = new St.BoxLayout({ vertical: true,
                                                             style_class: 'screen-shield-notifications-box' });
        scrollView.add_actor(this._persistentNotificationBox);

        this.actor.add(this._residentNotificationBox, { x_fill: true });
        this.actor.add(scrollView, { x_fill: true, x_align: St.Align.START });

        this._items = [];
        Main.messageTray.getSummaryItems().forEach(Lang.bind(this, function(item) {
            this._summaryItemAdded(Main.messageTray, item, true);
        }));
        this._updateVisibility();

        this._summaryAddedId = Main.messageTray.connect('summary-item-added', Lang.bind(this, this._summaryItemAdded));
    },

    destroy: function() {
        if (this._summaryAddedId) {
            Main.messageTray.disconnect(this._summaryAddedId);
            this._summaryAddedId = 0;
        }

        for (let i = 0; i < this._items.length; i++)
            this._removeItem(this._items[i]);
        this._items = [];

        this.actor.destroy();
    },

    _updateVisibility: function() {
        this._residentNotificationBox.visible = this._residentNotificationBox.get_n_children() > 0;
        this._persistentNotificationBox.visible = this._persistentNotificationBox.get_children().some(function(a) {
            return a.visible;
        });

        this.actor.visible = this._residentNotificationBox.visible || this._persistentNotificationBox.visible;
    },

    _sourceIsResident: function(source) {
        return source.hasResidentNotification() && !source.isChat;
    },

    _makeNotificationCountText: function(count, isChat) {
        if (isChat)
            return ngettext("%d new message", "%d new messages", count).format(count);
        else
            return ngettext("%d new notification", "%d new notifications", count).format(count);
    },

    _makeNotificationSource: function(source) {
        let box = new St.BoxLayout({ style_class: 'screen-shield-notification-source' });

        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add(sourceActor.actor, { y_fill: true });

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: false, y_align: St.Align.START });

        let label = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
        textBox.add(label);

        let count = source.unseenCount;
        let countLabel = new St.Label({ text: this._makeNotificationCountText(count, source.isChat),
                                        style_class: 'screen-shield-notification-count-text' });
        textBox.add(countLabel);

        box.visible = count != 0;
        return [box, countLabel];
    },

    _summaryItemAdded: function(tray, item, dontUpdateVisibility) {
        // Ignore transient sources, or sources explicitly marked not to show
        // in the lock screen
        if (item.source.isTransient || !item.source.showInLockScreen)
            return;

        let obj = {
            item: item,
            source: item.source,
            resident: this._sourceIsResident(item.source),
            contentUpdatedId: 0,
            sourceDestroyId: 0,
            sourceBox: null,
            countLabel: null,
        };

        if (obj.resident) {
            this._residentNotificationBox.add(item.notificationStackWidget);
            item.closeButton.hide();
            item.prepareNotificationStackForShowing();
        } else {
            [obj.sourceBox, obj.countLabel] = this._makeNotificationSource(item.source);
            this._persistentNotificationBox.add(obj.sourceBox, { x_fill: false, x_align: St.Align.START });
        }

        obj.contentUpdatedId = item.connect('content-updated', Lang.bind(this, this._onItemContentUpdated));
        obj.sourceCountChangedId = item.source.connect('count-updated', Lang.bind(this, this._onSourceChanged));
        obj.sourceTitleChangedId = item.source.connect('title-changed', Lang.bind(this, this._onSourceChanged));
        obj.sourceDestroyId = item.source.connect('destroy', Lang.bind(this, this._onSourceDestroy));
        this._items.push(obj);

        if (!dontUpdateVisibility)
            this._updateVisibility();
    },

    _findSource: function(source) {
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].source == source)
                return i;
        }

        return -1;
    },

    _onItemContentUpdated: function(item) {
        let obj = this._items[this._findSource(item.source)];
        this._updateItem(obj);
    },

    _onSourceChanged: function(source) {
        let obj = this._items[this._findSource(source)];
        this._updateItem(obj);
    },

    _updateItem: function(obj) {
        let itemShouldBeResident = this._sourceIsResident(obj.source);

        if (itemShouldBeResident && obj.resident) {
            // Nothing to do here, the actor is already updated
            return;
        }

        if (obj.resident && !itemShouldBeResident) {
            // make into a regular item
            obj.item.doneShowingNotificationStack();
            this._residentNotificationBox.remove_actor(obj.item.notificationStackWidget);

            [obj.sourceBox, obj.countLabel] = this._makeNotificationSource(obj.source);
            this._persistentNotificationBox.add(obj.sourceBox, { x_fill: false, x_align: St.Align.START });
        } else if (itemShouldBeResident && !obj.resident) {
            // make into a resident item
            obj.sourceBox.destroy();
            obj.sourceBox = obj.countLabel = null;
            obj.resident = true;

            this._residentNotificationBox.add(obj.item.notificationStackWidget);
            obj.item.closeButton.hide();
            obj.item.prepareNotificationStackForShowing();
        } else {
            // just update the counter
            let count = obj.source.unseenCount;
            obj.countLabel.text = this._makeNotificationCountText(count, obj.source.isChat);
            obj.sourceBox.visible = count != 0;
        }

        this._updateVisibility();
    },

    _onSourceDestroy: function(source) {
        let idx = this._findSource(source);

        this._removeItem(this._items[idx]);
        this._items.splice(idx, 1);

        this._updateVisibility();
    },

    _removeItem: function(obj) {
        if (obj.resident) {
            obj.item.doneShowingNotificationStack();
            this._residentNotificationBox.remove_actor(obj.item.notificationStackWidget);
        } else {
            obj.sourceBox.destroy();
        }

        obj.item.disconnect(obj.contentUpdatedId);
        obj.source.disconnect(obj.sourceDestroyId);
        obj.source.disconnect(obj.sourceCountChangedId);
    },
});

const Arrow = new Lang.Class({
    Name: 'Arrow',
    Extends: St.Bin,

    _init: function(params) {
        this.parent(params);
        this.x_fill = this.y_fill = true;
        this.set_offscreen_redirect(Clutter.OffscreenRedirect.ALWAYS);

        this._drawingArea = new St.DrawingArea();
        this._drawingArea.connect('repaint', Lang.bind(this, this._drawArrow));
        this.child = this._drawingArea;

        this._shadowHelper = null;
        this._shadowWidth = this._shadowHeight = 0;
    },

    _drawArrow: function(arrow) {
        let cr = arrow.get_context();
        let [w, h] = arrow.get_surface_size();
        let node = this.get_theme_node();
        let thickness = node.get_length('-arrow-thickness');

        Clutter.cairo_set_source_color(cr, node.get_foreground_color());

        cr.setLineCap(Cairo.LineCap.ROUND);
        cr.setLineWidth(thickness);

        cr.moveTo(thickness / 2, h - thickness / 2);
        cr.lineTo(w/2, thickness);
        cr.lineTo(w - thickness / 2, h - thickness / 2);
        cr.stroke();
    },

    vfunc_style_changed: function() {
        let node = this.get_theme_node();
        this._shadow = node.get_shadow('-arrow-shadow');
        if (this._shadow)
            this._shadowHelper = St.ShadowHelper.new(this._shadow);
        else
            this._shadowHelper = null;
    },

    vfunc_paint: function() {
        if (this._shadowHelper) {
            this._shadowHelper.update(this._drawingArea);

            let allocation = this._drawingArea.get_allocation_box();
            let paintOpacity = this._drawingArea.get_paint_opacity();
            this._shadowHelper.paint(allocation, paintOpacity);
        }

        this._drawingArea.paint();
    }
});

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

/**
 * To test screen shield, make sure to kill gnome-screensaver.
 *
 * If you are setting org.gnome.desktop.session.idle-delay directly in dconf,
 * rather than through System Settings, you also need to set
 * org.gnome.settings-daemon.plugins.power.sleep-display-ac and
 * org.gnome.settings-daemon.plugins.power.sleep-display-battery to the same value.
 * This will ensure that the screen blanks at the right time when it fades out.
 * https://bugzilla.gnome.org/show_bug.cgi?id=668703 explains the dependance.
 */
const ScreenShield = new Lang.Class({
    Name: 'ScreenShield',

    _init: function() {
        this.actor = Main.layoutManager.screenShieldGroup;

        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                reactive: true,
                                                can_focus: true,
                                                name: 'lockScreenGroup',
                                              });
        this._lockScreenGroup.connect('key-release-event',
                                      Lang.bind(this, this._onLockScreenKeyRelease));
        this._lockScreenGroup.connect('scroll-event',
                                      Lang.bind(this, this._onLockScreenScroll));

        this._lockScreenContents = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                                   name: 'lockScreenContents' });
        this._lockScreenContents.add_constraint(new Layout.MonitorConstraint({ primary: true }));

        let backgroundActor = Meta.BackgroundActor.new_for_screen(global.screen);
        backgroundActor.add_glsl_snippet(Meta.SnippetHook.TEXTURE_LOOKUP,
                                         GLSL_BLUR_EFFECT_DECLARATIONS,
                                         GLSL_BLUR_EFFECT_CODE,
                                         true);
        this._background = new St.Bin({ style_class: 'screen-shield-background',
                                        child: backgroundActor });
        this._lockScreenGroup.add_actor(this._background);
        this._lockScreenGroup.add_actor(this._lockScreenContents);

        this._arrowContainer = new St.BoxLayout({ style_class: 'screen-shield-arrows',
                                                  vertical: true,
                                                  x_align: Clutter.ActorAlign.CENTER,
                                                  y_align: Clutter.ActorAlign.END,
                                                  // HACK: without these, ClutterBinLayout
                                                  // ignores alignment properties on the actor
                                                  x_expand: true,
                                                  y_expand: true });

        for (let i = 0; i < N_ARROWS; i++) {
            let arrow = new Arrow({ opacity: 0 });
            this._arrowContainer.add_actor(arrow);
        }
        this._lockScreenContents.add_actor(this._arrowContainer);

        this._dragAction = new Clutter.GestureAction();
        this._dragAction.connect('gesture-begin', Lang.bind(this, this._onDragBegin));
        this._dragAction.connect('gesture-progress', Lang.bind(this, this._onDragMotion));
        this._dragAction.connect('gesture-end', Lang.bind(this, this._onDragEnd));
        this._lockScreenGroup.add_action(this._dragAction);

        this._lockDialogGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                pivot_point: new Clutter.Point({ x: 0.5, y: 0.5 }),
                                                name: 'lockDialogGroup' });

        this.actor.add_actor(this._lockDialogGroup);
        this.actor.add_actor(this._lockScreenGroup);

        this._presence = new GnomeSession.Presence(Lang.bind(this, function(proxy, error) {
            if (error) {
                logError(error, 'Error while reading gnome-session presence');
                return;
            }

            this._onStatusChanged(proxy.status);
        }));
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._onStatusChanged(status);
        }));

        this._screenSaverDBus = new ShellDBus.ScreenSaverDBus(this);

        this._loginManager = LoginManager.getLoginManager();
        this._loginSession = this._loginManager.getCurrentSessionProxy();
        this._loginSession.connectSignal('Lock', Lang.bind(this, function() { this.lock(false); }));
        this._loginSession.connectSignal('Unlock', Lang.bind(this, function() { this.unlock(); }));

        this._settings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });

        this._isModal = false;
        this._hasLockScreen = false;
        this._isGreeter = false;
        this._isActive = false;
        this._inUnlockAnimation = false;
        this._activationTime = 0;

        this._lightbox = new Lightbox.Lightbox(Main.uiGroup,
                                               { inhibitEvents: true,
                                                 fadeInTime: STANDARD_FADE_TIME,
                                                 fadeFactor: 1 });

        this.idleMonitor = new GnomeDesktop.IdleMonitor();
    },

    _onLockScreenKeyRelease: function(actor, event) {
        let symbol = event.get_key_symbol();

        // Do nothing if the lock screen is not fully shown.
        // This avoids reusing the previous (and stale) unlock
        // dialog if esc is pressed while the curtain is going
        // down after cancel.
        // Similarly, don't bump if the lock screen is not showing or is
        // animating, as the bump overrides the animation and would
        // remove any onComplete handler.

        if (this._lockScreenState != MessageTray.State.SHOWN)
            return false;

        if (symbol == Clutter.KEY_Escape ||
            symbol == Clutter.KEY_Return ||
            symbol == Clutter.KEY_KP_Enter) {
            this._ensureUnlockDialog(true);
            this._hideLockScreen(true, 0);
            return true;
        }

        this._bumpLockScreen();
        return true;
    },

    _onLockScreenScroll: function(actor, event) {
        if (this._lockScreenState != MessageTray.State.SHOWN)
            return false;

        let delta = 0;
        if (event.get_scroll_direction() == Clutter.ScrollDirection.UP)
            delta = 5;
        else if (event.get_scroll_direction() == Clutter.ScrollDirection.SMOOTH)
            delta = Math.max(0, event.get_scroll_delta()[0]);

        this._lockScreenScrollCounter += delta;

        // 7 standard scrolls to lift up
        if (this._lockScreenScrollCounter > 35) {
            this._ensureUnlockDialog(false);
            this._hideLockScreen(true, 0);
        }

        return true;
    },

    _animateArrows: function() {
        let arrows = this._arrowContainer.get_children();
        let unitaryDelay = ARROW_ANIMATION_TIME / (arrows.length + 1);
        let maxOpacity = 255 * ARROW_ANIMATION_PEAK_OPACITY;
        for (let i = 0; i < arrows.length; i++) {
            arrows.opacity = 0;
            Tweener.addTween(arrows[i],
                             { opacity: 0,
                               delay: unitaryDelay * (N_ARROWS - (i + 1)),
                               time: ARROW_ANIMATION_TIME,
                               transition: function(t, b, c, d) {
                                 if (t < d/2)
                                     return TweenerEquations.easeOutQuad(t, 0, maxOpacity, d/2);
                                 else
                                     return TweenerEquations.easeInQuad(t - d/2, maxOpacity, -maxOpacity, d/2);
                               }
                             });
        }

        return true;
    },

    _onDragBegin: function() {
        Tweener.removeTweens(this._lockScreenGroup);
        this._lockScreenState = MessageTray.State.HIDING;
        this._ensureUnlockDialog(false);

        return true;
    },

    _onDragMotion: function() {
	let [origX, origY] = this._dragAction.get_press_coords(0);
	let [currentX, currentY] = this._dragAction.get_motion_coords(0);

	let newY = currentY - origY;
	newY = clamp(newY, -global.stage.height, 0);

	this._lockScreenGroup.y = newY;

	return true;
    },

    _onDragEnd: function(action, actor, eventX, eventY, modifiers) {
        if (this._lockScreenGroup.y < -(ARROW_DRAG_THRESHOLD * global.stage.height)) {
            // Complete motion automatically
	    let [velocity, velocityX, velocityY] = this._dragAction.get_velocity(0);
	    this._hideLockScreen(true, -velocityY);
        } else {
            // restore the lock screen to its original place
            // try to use the same speed as the normal animation
            let h = global.stage.height;
            let time = CURTAIN_SLIDE_TIME * (-this._lockScreenGroup.y) / h;
            Tweener.removeTweens(this._lockScreenGroup);
            Tweener.addTween(this._lockScreenGroup,
                             { y: 0,
                               time: time,
                               transition: 'easeInQuad',
                               onComplete: function() {
                                   this._lockScreenGroup.fixed_position_set = false;
                                   this._lockScreenState = MessageTray.State.SHOWN;
                               },
                               onCompleteScope: this,
                             });

            // If we have a unlock dialog, cancel it
            if (this._dialog) {
                this._dialog.cancel();
                if (!this._isGreeter) {
                    this._dialog = null;
                }
            }
        }
    },

    _onStatusChanged: function(status) {
        if (status != GnomeSession.PresenceStatus.IDLE)
            return;

        if (this._dialog) {
            this._dialog.cancel();
            if (!this._isGreeter) {
                this._dialog = null;
            }
        }

        if (!this._isModal) {
            Main.pushModal(this.actor);
            this._isModal = true;
        }

        if (!this._isActive) {
            this._lightbox.show();

            if (this._activationTime == 0)
                this._activationTime = GLib.get_monotonic_time();

            this._becameActiveId = this.idleMonitor.connect('became-active', Lang.bind(this, function() {
                this.idleMonitor.disconnect(this._becameActiveId);

                let lightboxWasShown = this._lightbox.shown;
                this._lightbox.hide();

                let shouldLock = lightboxWasShown && this._settings.get_boolean(LOCK_ENABLED_KEY);
                if (shouldLock || this._isLocked) {
                    this.lock(false);
                } else if (this._isActive) {
                    this.unlock();
                }
            }));

            this._isActive = true;
            this.emit('lock-status-changed');
        }
    },

    showDialog: function() {
        // Ensure that the stage window is mapped, before taking a grab
        // otherwise X errors out
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
            if (!this._isModal) {
                Main.pushModal(this.actor);
                this._isModal = true;
            }

            return false;
        }));

        this.actor.show();
        this._isGreeter = Main.sessionMode.isGreeter;
        this._ensureUnlockDialog(true);
        this._hideLockScreen(false, 0);
    },

    _bumpLockScreen: function() {
        Tweener.removeTweens(this._lockScreenGroup);
        Tweener.addTween(this._lockScreenGroup,
                         { y: -BUMP_SIZE,
                           time: BUMP_TIME / 2,
                           transition: 'easeOutQuad',
                           onComplete: function() {
                               Tweener.addTween(this,
                                                { y: 0,
                                                  time: BUMP_TIME / 2,
                                                  transition: 'easeInQuad' });
                           }
                         });
    },

    _hideLockScreen: function(animate, velocity) {
        this._lockScreenState = MessageTray.State.HIDING;

        if (animate) {
            // Tween the lock screen out of screen
            // if velocity is not specified (i.e. we come here from pressing ESC),
            // use the same speed regardless of original position
            // if velocity is specified, it's in pixels per milliseconds
            let h = global.stage.height;
            let delta = (h + this._lockScreenGroup.y);
            let min_velocity = global.stage.height / (CURTAIN_SLIDE_TIME * 1000);

            velocity = Math.max(min_velocity, velocity);
            let time = (delta / velocity) / 1000;

            Tweener.removeTweens(this._lockScreenGroup);
            Tweener.addTween(this._lockScreenGroup,
                             { y: -h,
                               time: time,
                               transition: 'easeInQuad',
                               onComplete: function() {
                                   this._lockScreenState = MessageTray.State.HIDDEN;
                                   this._lockScreenGroup.hide();
                               },
                               onCompleteScope: this,
                             });
        } else {
            this._lockScreenState = MessageTray.State.HIDDEN;
            this._lockScreenGroup.hide();
        }

        global.stage.show_cursor();

        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');
    },

    _ensureUnlockDialog: function(onPrimary) {
        if (!this._dialog) {
            let constructor = Main.sessionMode.unlockDialog;
            if (!constructor) {
                // This session mode has no locking capabilities
                this.unlock();
                return;
            }

            this._dialog = new constructor(this._lockDialogGroup);


            let time = global.get_current_time();
            this._dialog.connect('loaded', Lang.bind(this, function() {
                if (!this._dialog.open(time, onPrimary)) {
                    log('Could not open login dialog: failed to acquire grab');
                    this.unlock();
                }
            }));

            this._dialog.connect('failed', Lang.bind(this, this._onUnlockFailed));
            this._dialog.connect('unlocked', Lang.bind(this, this._onUnlockSucceded));
        }
    },

    _onUnlockFailed: function() {
        this._resetLockScreen(true, false);
    },

    _onUnlockSucceded: function() {
        this._tweenUnlocked();
    },

    _resetLockScreen: function(animateLockScreen, animateLockDialog) {
        this._ensureLockScreen();
        this._lockDialogGroup.scale_x = 1;
        this._lockDialogGroup.scale_y = 1;

        this._lockScreenGroup.show();
        this._lockScreenState = MessageTray.State.SHOWING;

        if (animateLockScreen) {
            this._lockScreenGroup.y = -global.screen_height;
            Tweener.removeTweens(this._lockScreenGroup);
            Tweener.addTween(this._lockScreenGroup,
                             { y: 0,
                               time: SHORT_FADE_TIME,
                               transition: 'easeOutQuad',
                               onComplete: function() {
                                   this._lockScreenShown();
                               },
                               onCompleteScope: this
                             });
        } else {
            this._lockScreenGroup.fixed_position_set = false;
            this._lockScreenShown();
        }

        if (animateLockDialog) {
            this._lockDialogGroup.opacity = 0;
            Tweener.removeTweens(this._lockDialogGroup);
            Tweener.addTween(this._lockDialogGroup,
                             { opacity: 255,
                               time: SHORT_FADE_TIME,
                               transition: 'easeOutQuad' });
        } else {
            this._lockDialogGroup.opacity = 255;
        }

        this._lockScreenGroup.grab_key_focus();

        if (Main.sessionMode.currentMode != 'lock-screen')
            Main.sessionMode.pushMode('lock-screen');
    },

    _lockScreenShown: function() {
        if (this._dialog && !this._isGreeter) {
            this._dialog.destroy();
            this._dialog = null;
        }

        if (this._arrowAnimationId)
            Mainloop.source_remove(this._arrowAnimationId);
        this._arrowAnimationId = Mainloop.timeout_add(6000, Lang.bind(this, this._animateArrows));
        this._animateArrows();

        let motionId = global.stage.connect('captured-event', function(stage, event) {
            if (event.type() == Clutter.EventType.MOTION) {
                global.stage.show_cursor();
                global.stage.disconnect(motionId);
            }

            return false;
        });
        global.stage.hide_cursor();

        this._lockScreenState = MessageTray.State.SHOWN;
        this._lockScreenGroup.fixed_position_set = false;
        this._lockScreenScrollCounter = 0;

        this.emit('lock-screen-shown');
    },

    // Some of the actors in the lock screen are heavy in
    // resources, so we only create them when needed
    _ensureLockScreen: function() {
        if (this._hasLockScreen)
            return;

        this._lockScreenContentsBox = new St.BoxLayout({ x_align: Clutter.ActorAlign.CENTER,
                                                         y_align: Clutter.ActorAlign.CENTER,
                                                         x_expand: true,
                                                         y_expand: true,
                                                         vertical: true,
                                                         style_class: 'screen-shield-contents-box' });
        this._clock = new Clock();
        this._lockScreenContentsBox.add(this._clock.actor, { x_fill: true,
                                                             y_fill: true });

        this._lockScreenContents.add_actor(this._lockScreenContentsBox);

        if (this._settings.get_boolean('show-notifications')) {
            this._notificationsBox = new NotificationsBox();
            this._lockScreenContentsBox.add(this._notificationsBox.actor, { x_fill: true,
                                                                            y_fill: true,
                                                                            expand: true });
        }

        this._hasLockScreen = true;
    },

    _clearLockScreen: function() {
        this._clock.destroy();
        this._clock = null;

        if (this._notificationsBox) {
            this._notificationsBox.destroy();
            this._notificationsBox = null;
        }

        this._lockScreenContentsBox.destroy();

        if (this._arrowAnimationId) {
            Mainloop.source_remove(this._arrowAnimationId);
            this._arrowAnimationId = 0;
        }

        this._hasLockScreen = false;
    },

    get locked() {
        return this._isActive;
    },

    get activationTime() {
        return this._activationTime;
    },

    _tweenUnlocked: function() {
        this._inUnlockAnimation = true;
        this.unlock();
        Tweener.addTween(this._lockDialogGroup, {
            scale_x: 0,
            scale_y: 0,
            time: Overview.ANIMATION_TIME,
            transition: 'easeOutQuad',
            onComplete: function() {
                if (this._dialog) {
                    this._dialog.destroy();
                    this._dialog = null;
                }
                this.actor.hide();
                this._inUnlockAnimation = false;
            },
            onCompleteScope: this
        });
    },

    unlock: function() {
        if (this._hasLockScreen)
            this._clearLockScreen();

        if (this._dialog && !this._isGreeter) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._lightbox.hide();

        if (this._isModal) {
            Main.popModal(this.actor);
            this._isModal = false;
        }

        if (!this._inUnlockAnimation)
            this.actor.hide();

        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');
        if (Main.sessionMode.currentMode == 'unlock-dialog')
            Main.sessionMode.popMode('unlock-dialog');

        this._activationTime = 0;
        this._isActive = false;
        this._isLocked = false;
        this.emit('lock-status-changed');
    },

    lock: function(animate) {
        if (!this._isModal) {
            Main.pushModal(this.actor);
            this._isModal = true;
        }

        if (this._activationTime == 0)
            this._activationTime = GLib.get_monotonic_time();

        this.actor.show();

        if (Main.sessionMode.currentMode != 'unlock-dialog' &&
            Main.sessionMode.currentMode != 'lock-screen') {
            this._isGreeter = Main.sessionMode.isGreeter;
            if (!this._isGreeter)
                Main.sessionMode.pushMode('unlock-dialog');
        }

        this._resetLockScreen(animate, animate);

        this._isActive = true;
        this._isLocked = true;
        this.emit('lock-status-changed');
    },
});
Signals.addSignalMethods(ScreenShield.prototype);

/* Fallback code to handle session locking using gnome-screensaver,
   in case the required GDM dependency is not there
*/
const ScreenShieldFallback = new Lang.Class({
    Name: 'ScreenShieldFallback',

    _init: function() {
        Util.spawn(['gnome-screensaver']);

        this._proxy = new Gio.DBusProxy({ g_connection: Gio.DBus.session,
                                          g_name: 'org.gnome.ScreenSaver',
                                          g_object_path: '/org/gnome/ScreenSaver',
                                          g_interface_name: 'org.gnome.ScreenSaver',
                                          g_flags: (Gio.DBusProxyFlags.DO_NOT_AUTO_START |
                                                    Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES),
                                        });
        this._proxy.init(null);

        this._proxy.connect('g-signal', Lang.bind(this, this._onSignal));
        this._proxy.connect('notify::g-name-owner', Lang.bind(this, this._onNameOwnerChanged));
    },

    _onNameOwnerChanged: function(object, pspec) {
        if (this._proxy.g_name_owner)
            [this._locked] = this._proxy.call_sync('GetActive', null,
                                                   Gio.DBusCallFlags.NONE, -1, null).deep_unpack();
        else
            this._locked = false;

        this.emit('lock-status-changed', this._locked);
    },

    _onSignal: function(proxy, senderName, signalName, params) {
        if (signalName == 'ActiveChanged') {
            [this._locked] = params.deep_unpack();
            this.emit('lock-status-changed', this._locked);
        }
    },

    get locked() {
        return this._locked;
    },

    lock: function() {
        this._proxy.call('Lock', null, Gio.DBusCallFlags.NONE, -1, null,
                         Lang.bind(this, function(proxy, result) {
                             proxy.call_finish(result);

                             this.emit('lock-screen-shown');
                         }));
    },

    unlock: function() {
        this._proxy.call('SetActive', GLib.Variant.new('(b)', false),
                         Gio.DBusCallFlags.NONE, -1, null, null);
    },
});
Signals.addSignalMethods(ScreenShieldFallback.prototype);
