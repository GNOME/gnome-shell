// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const TweenerEquations = imports.tweener.equations;

const GnomeSession = imports.misc.gnomeSession;
const Hash = imports.misc.hash;
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
const LOCK_DELAY_KEY = 'lock-delay';

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

// ScreenShield animation time
// - STANDARD_FADE_TIME is used when the session goes idle
// - MANUAL_FADE_TIME is used for lowering the shield when asked by the user,
//   or when cancelling the dialog
// - CURTAIN_SLIDE_TIME is used when raising the shield before unlocking
const STANDARD_FADE_TIME = 10;
const MANUAL_FADE_TIME = 0.8;
const CURTAIN_SLIDE_TIME = 0.3;

function sample(offx, offy) {
    return 'texel += texture2D (sampler, tex_coord.st + pixel_step * ' +
        'vec2 (' + offx + ',' + offy + '));\n'
}
const GLSL_BLUR_EFFECT_DECLARATIONS = ' \
uniform vec2 pixel_step;\n \
uniform float desaturation;\n \
vec4 apply_blur(in sampler2D sampler, in vec2 tex_coord) {\n \
  vec4 texel;\n \
  texel = texture2D (sampler, tex_coord.st);\n'
  + sample(-1.0, -1.0)
  + sample( 0.0, -1.0)
  + sample(+1.0, -1.0)
  + sample(-1.0,  0.0)
  + sample(+1.0,  0.0)
  + sample(-1.0, +1.0)
  + sample( 0.0, +1.0)
  + sample(+1.0, +1.0) + ' \
   texel /= 9.0;\n \
   return texel;\n \
}\n \
vec3 desaturate (const vec3 color)\n \
{\n \
   const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n \
   vec3 gray = vec3 (dot (gray_conv, color));\n \
   return vec3 (mix (color.rgb, gray, desaturation));\n \
}';
const GLSL_BLUR_EFFECT_CODE = ' \
cogl_texel = apply_blur(cogl_sampler, cogl_tex_coord.st);\n \
cogl_texel.rgb = desaturate(cogl_texel.rgb);\n';


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

        this._musicBin = new St.Bin({ style_class: 'screen-shield-notifications-box',
                                      visible: false });

        let scrollView = new St.ScrollView({ x_fill: false, x_align: St.Align.START });
        this._notificationBox = new St.BoxLayout({ vertical: true,
                                                   style_class: 'screen-shield-notifications-box' });
        scrollView.add_actor(this._notificationBox);

        this.actor.add(this._musicBin);
        this.actor.add(scrollView, { x_fill: true, x_align: St.Align.START });

        this._sources = new Hash.Map();
        Main.messageTray.getSources().forEach(Lang.bind(this, function(source) {
            this._sourceAdded(Main.messageTray, source, true);
        }));
        this._updateVisibility();

        this._sourceAddedId = Main.messageTray.connect('source-added', Lang.bind(this, this._sourceAdded));
    },

    destroy: function() {
        if (this._sourceAddedId) {
            Main.messageTray.disconnect(this._sourceAddedId);
            this._sourceAddedId = 0;
        }

        let items = this._sources.items();
        for (let i = 0; i < items.length; i++) {
            let [source, obj] = items[i];
            this._removeSource(source, obj);
        }

        this.actor.destroy();
    },

    _updateVisibility: function() {
        this._musicBin.visible = this._musicBin.child != null && this._musicBin.child.visible;
        this._notificationBox.visible = this._notificationBox.get_children().some(function(a) {
            return a.visible;
        });

        this.actor.visible = this._musicBin.visible || this._notificationBox.visible;
    },

    _makeNotificationCountText: function(count, isChat) {
        if (isChat)
            return ngettext("%d new message", "%d new messages", count).format(count);
        else
            return ngettext("%d new notification", "%d new notifications", count).format(count);
    },

    _makeNotificationSource: function(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add(sourceActor.actor, { y_fill: true });

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: false, y_align: St.Align.START });

        let title = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
        textBox.add(title);

        let count = source.unseenCount;
        let countLabel = new St.Label({ text: this._makeNotificationCountText(count, source.isChat),
                                        style_class: 'screen-shield-notification-count-text' });
        textBox.add(countLabel);

        box.visible = count != 0;
        return [title, countLabel];
    },

    _makeNotificationDetailedSource: function(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        let sourceBin = new St.Bin({ y_align: St.Align.START,
                                     x_align: St.Align.START,
                                     child: sourceActor.actor });
        box.add(sourceBin);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: false, y_align: St.Align.START });

        let title = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
        textBox.add(title);

        let visible = false;
        for (let i = 0; i < source.notifications.length; i++) {
            let n = source.notifications[i];

            if (n.acknowledged || n.isMusic)
                continue;

            let body = '';
            if (n.bannerBodyText) {
                body = n.bannerBodyMarkup ? n.bannerBodyText :
                GLib.markup_escape_text(n.bannerBodyMarkup, -1);
            }

            let label = new St.Label({ style_class: 'screen-shield-notification-count-text' });
            label.clutter_text.set_markup('<b>' + n.title + '</b> ' + body);
            textBox.add(label);

            visible = true;
        }

        box.visible = visible;
        return [title, null];
    },

    _showSource: function(source, obj, box) {
        let musicNotification = source.getMusicNotification();

        if (musicNotification != null &&
            this._musicBin.child == null) {
            if (musicNotification.actor.get_parent() != null)
                musicNotification.actor.get_parent().remove_actor(musicNotification.actor);
            this._musicBin.child = musicNotification.actor;
            this._musicBin.child.visible = obj.visible;

            musicNotification.expand(false /* animate */);

            obj.musicNotification = musicNotification;
        }

        if (obj.detailed) {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationDetailedSource(source, box);
        } else {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationSource(source, box);
        }

        box.visible = obj.visible &&
            (source.unseenCount > (musicNotification ? 1 : 0));
    },

    _sourceAdded: function(tray, source, dontUpdateVisibility) {
        // Ignore transient sources
        if (source.isTransient)
            return;

        let obj = {
            visible: source.policy.showInLockScreen,
            detailed: source.policy.detailsInLockScreen,
            sourceDestroyId: 0,
            sourceCountChangedId: 0,
            sourceTitleChangedId: 0,
            sourceUpdatedId: 0,
            musicNotification: null,
            sourceBox: null,
            titleLabel: null,
            countLabel: null,
        };

        obj.sourceBox = new St.BoxLayout({ style_class: 'screen-shield-notification-source' });
        this._showSource(source, obj, obj.sourceBox);
        this._notificationBox.add(obj.sourceBox, { x_fill: false, x_align: St.Align.START });

        obj.sourceCountChangedId = source.connect('count-updated', Lang.bind(this, function(source) {
            this._countChanged(source, obj);
        }));
        obj.sourceTitleChangedId = source.connect('title-changed', Lang.bind(this, function(source) {
            this._titleChanged(source, obj);
        }));
        obj.policyChangedId = source.policy.connect('policy-changed', Lang.bind(this, function(policy, key) {
            if (key == 'show-in-lock-screen')
                this._visibleChanged(source, obj);
            else
                this._detailedChanged(source, obj);
        }));
        obj.sourceDestroyId = source.connect('destroy', Lang.bind(this, function(source) {
            this._onSourceDestroy(source, obj);
        }));

        this._sources.set(source, obj);

        if (!dontUpdateVisibility)
            this._updateVisibility();
    },

    _titleChanged: function(source, obj) {
        obj.titleLabel.text = source.title;
    },

    _countChanged: function(source, obj) {
        if (obj.detailed) {
            // A new notification was pushed, or a previous notification was destroyed.
            // Give up, and build the list again.

            obj.sourceBox.destroy_all_children();
            obj.titleLabel = obj.countLabel = null;
            this._showSource(source, obj, obj.sourceBox);
        } else {
            let count = source.unseenCount;
            obj.countLabel.text = this._makeNotificationCountText(count, source.isChat);
        }

        obj.sourceBox.visible = obj.visible &&
            (source.unseenCount > (obj.musicNotification ? 1 : 0));
        this._updateVisibility();
    },

    _visibleChanged: function(source, obj) {
        if (obj.visible == source.policy.showInLockScreen)
            return;

        obj.visible = source.policy.showInLockScreen;
        if (obj.musicNotification)
            obj.musicNotification.actor.visible = obj.visible;
        obj.sourceBox.visible = obj.visible &&
            source.unseenCount > (obj.musicNotification ? 1 : 0);

        this._updateVisibility();
    },

    _detailedChanged: function(source, obj) {
        if (obj.detailed == source.policy.detailsInLockScreen)
            return;

        obj.detailed = source.policy.detailsInLockScreen;

        obj.sourceBox.destroy_all_children();
        obj.titleLabel = obj.countLabel = null;
        this._showSource(source, obj, obj.sourceBox);
    },

    _onSourceDestroy: function(source, obj) {
        this._removeSource(source, obj);
        this._updateVisibility();
    },

    _removeSource: function(source, obj) {
        obj.sourceBox.destroy();
        obj.sourceBox = obj.titleLabel = obj.countLabel = null;

        if (obj.musicNotification) {
            this._musicBin.child = null;
            obj.musicNotification = null;
        }

        source.disconnect(obj.sourceDestroyId);
        source.disconnect(obj.sourceCountChangedId);
        source.disconnect(obj.sourceTitleChangedId);
        source.policy.disconnect(obj.policyChangedId);

        this._sources.delete(source);
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
                                                visible: false,
                                              });
        this._lockScreenGroup.connect('key-release-event',
                                      Lang.bind(this, this._onLockScreenKeyRelease));
        this._lockScreenGroup.connect('scroll-event',
                                      Lang.bind(this, this._onLockScreenScroll));
        Main.ctrlAltTabManager.addGroup(this._lockScreenGroup, _("Lock"), 'changes-prevent-symbolic');

        this._lockScreenContents = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                                   name: 'lockScreenContents' });
        this._lockScreenContents.add_constraint(new Layout.MonitorConstraint({ primary: true }));

        let backgroundActor = Meta.BackgroundActor.new_for_screen(global.screen);
        backgroundActor.add_glsl_snippet(Meta.SnippetHook.TEXTURE_LOOKUP,
                                         GLSL_BLUR_EFFECT_DECLARATIONS,
                                         GLSL_BLUR_EFFECT_CODE,
                                         true);
        backgroundActor.set_uniform_float('desaturation',
                                          1, 1, [0.6]);
        backgroundActor.connect('notify::size', function(actor) {
            actor.set_uniform_float('pixel_step', 2, 1, [1/actor.width, 1/actor.height]);
        });

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

        this._inhibitor = null;
        this._loginManager = LoginManager.getLoginManager();
        this._loginManager.connect('prepare-for-sleep',
                                   Lang.bind(this, this._prepareForSleep));
        this._inhibitSuspend();

        this._loginSession = this._loginManager.getCurrentSessionProxy();
        this._loginSession.connectSignal('Lock', Lang.bind(this, function() { this.lock(false); }));
        this._loginSession.connectSignal('Unlock', Lang.bind(this, function() { this.deactivate(false); }));

        this._settings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });

        this._isModal = false;
        this._hasLockScreen = false;
        this._isGreeter = false;
        this._isActive = false;
        this._isLocked = false;
        this._inUnlockAnimation = false;
        this._activationTime = 0;
        this._becameActiveId = 0;
        this._lockTimeoutId = 0;

        this._lightbox = new Lightbox.Lightbox(Main.uiGroup,
                                               { inhibitEvents: true,
                                                 fadeInTime: STANDARD_FADE_TIME,
                                                 fadeFactor: 1 });
        this._lightbox.connect('shown', Lang.bind(this, this._onLightboxShown));

        this.idleMonitor = new GnomeDesktop.IdleMonitor();
    },

    _liftShield: function(onPrimary, velocity) {
        if (this._isLocked) {
            this._ensureUnlockDialog(onPrimary, true /* allowCancel */);
            this._hideLockScreen(true /* animate */, velocity);
        } else {
            this.deactivate(true /* animate */);
        }
    },

    _becomeModal: function() {
        if (this._isModal)
            return true;

        this._isModal = Main.pushModal(this.actor, { keybindingMode: Shell.KeyBindingMode.LOCK_SCREEN });
        if (this._isModal)
            return true;

        // We failed to get a pointer grab, it means that
        // something else has it. Try with a keyboard grab only
        this._isModal = Main.pushModal(this.actor, { options: Meta.ModalOptions.POINTER_ALREADY_GRABBED,
                                                     keybindingMode: Shell.KeyBindingMode.LOCK_SCREEN });
        return this._isModal;
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
            this._liftShield(false, 0);
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
            this._liftShield(true, 0);
        }

        return true;
    },

    _inhibitSuspend: function() {
        this._loginManager.inhibit(_("GNOME needs to lock the screen"),
                                   Lang.bind(this, function(inhibitor) {
                                       this._inhibitor = inhibitor;
                                   }));
    },

    _uninhibitSuspend: function() {
        if (this._inhibitor)
            this._inhibitor.close(null);
        this._inhibitor = null;
    },

    _prepareForSleep: function(loginManager, aboutToSuspend) {
        if (aboutToSuspend) {
            if (!this._settings.get_boolean(LOCK_ENABLED_KEY)) {
                this._uninhibitSuspend();
                return;
            }
            this.lock(true);
        } else {
            this._inhibitSuspend();
        }
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

        if (this._isLocked)
            this._ensureUnlockDialog(false, false);

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
            this._liftShield(true, -velocityY);
        } else {
            // restore the lock screen to its original place
            // try to use the same speed as the normal animation
            let h = global.stage.height;
            let time = MANUAL_FADE_TIME * (-this._lockScreenGroup.y) / h;
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

        if (!this._becomeModal()) {
            // We could not become modal, so we can't activate the
            // screenshield. The user is probably very upset at this
            // point, but any application using global grabs is broken
            // Just tell him to stop using this app
            // 
            // XXX: another option is to kick the user into the gdm login
            // screen, where we're not affected by grabs
            Main.notifyError(_("Unable to lock"),
                             _("Lock was blocked by an application"));
            return;
        }

        if (this._lightbox.actor.visible ||
            this._isActive) {
            // We're either shown and active, or in the process of
            // showing.
            // The latter is a very unlikely condition (it requires
            // idle-delay < 20), but in any case we have nothing
            // to do at this point: either isActive is true, or
            // it will soon be.
            // isActive can also be true if the lightbox is hidden,
            // in case the shield is down and the user hasn't unlocked yet
            return;
        }

        this._lightbox.show();

        if (this._activationTime == 0)
            this._activationTime = GLib.get_monotonic_time();

        this._becameActiveId = this.idleMonitor.add_user_active_watch(Lang.bind(this, this._onUserBecameActive));

        let shouldLock = this._settings.get_boolean(LOCK_ENABLED_KEY) && !this._isLocked;

        if (shouldLock) {
            let lockTimeout = Math.max(STANDARD_FADE_TIME, this._settings.get_uint(LOCK_DELAY_KEY));
            this._lockTimeoutId = Mainloop.timeout_add(lockTimeout * 1000,
                                                       Lang.bind(this, function() {
                                                           this._lockTimeoutId = 0;
                                                           this.lock(true);
                                                           return false;
                                                       }));
        }
    },

    _onUserBecameActive: function() {
        // This function gets called here when the user becomes active
        // after gnome-session changed the status to IDLE
        // There are four possibilities here:
        // - we're called when already locked; isActive and isLocked are true,
        //   we just go back to the lock screen curtain
        // - we're called before the lightbox is fully shown; at this point
        //   isActive is false, so we just hide the ligthbox, reset the activationTime
        //   and go back to the unlocked desktop
        // - we're called after showing the lightbox, but before the lock
        //   delay; this is mostly like the case above, but isActive is true now
        //   so we need to notify gnome-settings-daemon to go back to the normal
        //   policies for blanking
        //   (they're handled by the same code, and we emit one extra ActiveChanged
        //   signal in the case above)
        // - we're called after showing the lightbox and after lock-delay; the
        //   session is effectivelly locked now, it's time to build and show
        //   the lock screen

        this.idleMonitor.remove_watch(this._becameActiveId);
        this._becameActiveId = 0;

        let lightboxWasShown = this._lightbox.shown;
        this._lightbox.hide();

        // Shortcircuit in case the mouse was moved before the fade completed
        if (!lightboxWasShown) {
            this.deactivate(false);
            return;
        }
    },

    _onLightboxShown: function() {
        this.activate(false);
    },

    showDialog: function() {
        // Ensure that the stage window is mapped, before taking a grab
        // otherwise X errors out
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
            if (!this._becomeModal()) {
                // In the login screen, this is a hard error. Fail-whale
                log('Could not acquire modal grab for the login screen. Aborting login process.');
                Meta.quit(Meta.ExitCode.ERROR);
            }

            return false;
        }));

        this.actor.show();
        this._isGreeter = Main.sessionMode.isGreeter;
        this._isLocked = true;
        this._ensureUnlockDialog(true, true);
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
        if (this._lockScreenState == MessageTray.State.HIDDEN)
            return;

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

    _ensureUnlockDialog: function(onPrimary, allowCancel) {
        if (!this._dialog) {
            let constructor = Main.sessionMode.unlockDialog;
            if (!constructor) {
                // This session mode has no locking capabilities
                this.deactivate(true);
                return;
            }

            this._dialog = new constructor(this._lockDialogGroup);


            let time = global.get_current_time();
            this._dialog.connect('loaded', Lang.bind(this, function() {
                if (!this._dialog.open(time, onPrimary)) {
                    // This is kind of an impossible error: we're already modal
                    // by the time we reach this...
                    log('Could not open login dialog: failed to acquire grab');
                    this.deactivate(true);
                }
            }));

            this._dialog.connect('failed', Lang.bind(this, this._onUnlockFailed));
            this._dialog.connect('unlocked', Lang.bind(this, this._onUnlockSucceded));
        }

        this._dialog.allowCancel = allowCancel;
    },

    _onUnlockFailed: function() {
        this._resetLockScreen(true, false);
    },

    _onUnlockSucceded: function() {
        this.deactivate(true);
    },

    _resetLockScreen: function(animateLockScreen, animateLockDialog) {
        // Don't reset the lock screen unless it is completely hidden
        // This prevents the shield going down if the lock-delay timeout
        // fires while the user is dragging (which has the potential
        // to confuse our state)
        if (this._lockScreenState != MessageTray.State.HIDDEN)
            return;

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
                               time: MANUAL_FADE_TIME,
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
                               time: MANUAL_FADE_TIME,
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

        let prevIsActive = this._isActive;
        this._isActive = true;

        if (prevIsActive != this._isActive)
            this.emit('active-changed');

        this._uninhibitSuspend();

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

        this._notificationsBox = new NotificationsBox();
        this._lockScreenContentsBox.add(this._notificationsBox.actor, { x_fill: true,
                                                                        y_fill: true,
                                                                        expand: true });

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
        return this._isLocked;
    },

    get active() {
        return this._isActive;
    },

    get activationTime() {
        return this._activationTime;
    },

    deactivate: function(animate) {
        this._hideLockScreen(animate, 0);

        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');
        if (Main.sessionMode.currentMode == 'unlock-dialog')
            Main.sessionMode.popMode('unlock-dialog');

        Tweener.addTween(this._lockDialogGroup, {
            scale_x: 0,
            scale_y: 0,
            time: animate ? Overview.ANIMATION_TIME : 0,
            transition: 'easeOutQuad',
            onComplete: Lang.bind(this, this._completeDeactivate),
            onCompleteScope: this
        });
    },

    _completeDeactivate: function() {
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

        this.actor.hide();

        if (this._becameActiveId != 0) {
            this.idleMonitor.remove_watch(this._becameActiveId);
            this._becameActiveId = 0;
        }

        if (this._lockTimeoutId != 0) {
            Mainloop.source_remove(this._lockTimeoutId);
            this._lockTimeoutId = 0;
        }

        this._activationTime = 0;
        this._isActive = false;
        this._isLocked = false;
        this.emit('active-changed');
        this.emit('locked-changed');
    },

    activate: function(animate) {
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

        // We used to set isActive and emit active-changed here,
        // but now we do that from lockScreenShown, which means
        // there is a 0.3 seconds window during which the lock
        // screen is effectively visible and the screen is locked, but
        // the DBus interface reports the screensaver is off.
        // This is because when we emit ActiveChanged(true),
        // gnome-settings-daemon blanks the screen, and we don't want
        // blank during the animation.
        // This is not a problem for the idle fade case, because we
        // activate without animation in that case.
    },

    lock: function(animate) {
        // Warn the user if we can't become modal
        if (!this._becomeModal()) {
            Main.notifyError(_("Unable to lock"),
                             _("Lock was blocked by an application"));
            return;
        }

        this._isLocked = true;
        this.activate(animate);

        this.emit('locked-changed');
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

        this.emit('active-changed', this._locked);
    },

    _onSignal: function(proxy, senderName, signalName, params) {
        if (signalName == 'ActiveChanged') {
            [this._locked] = params.deep_unpack();
            this.emit('active-changed', this._locked);
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
