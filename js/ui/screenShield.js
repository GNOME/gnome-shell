// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const TweenerEquations = imports.tweener.equations;

const Background = imports.ui.background;
const GnomeSession = imports.misc.gnomeSession;
const Layout = imports.ui.layout;
const OVirt = imports.gdm.oVirt;
const LoginManager = imports.misc.loginManager;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const MessageTray = imports.ui.messageTray;
const ShellDBus = imports.ui.shellDBus;
const SmartcardManager = imports.misc.smartcardManager;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';
const LOCK_DELAY_KEY = 'lock-delay';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_LOCK_KEY = 'disable-lock-screen';

const LOCKED_STATE_STR = 'screenShield.locked';
// fraction of screen height the arrow must reach before completing
// the slide up automatically
var ARROW_DRAG_THRESHOLD = 0.1;

// Parameters for the arrow animation
var N_ARROWS = 3;
var ARROW_ANIMATION_TIME = 0.6;
var ARROW_ANIMATION_PEAK_OPACITY = 0.4;
var ARROW_IDLE_TIME = 30000; // ms

var SUMMARY_ICON_SIZE = 48;

// ScreenShield animation time
// - STANDARD_FADE_TIME is used when the session goes idle
// - MANUAL_FADE_TIME is used for lowering the shield when asked by the user,
//   or when cancelling the dialog
// - BACKGROUND_FADE_TIME is used when the background changes to crossfade to new background
// - CURTAIN_SLIDE_TIME is used when raising the shield before unlocking
var STANDARD_FADE_TIME = 10;
var MANUAL_FADE_TIME = 0.3;
var BACKGROUND_FADE_TIME = 1.0;
var CURTAIN_SLIDE_TIME = 0.3;

var Clock = new Lang.Class({
    Name: 'ScreenShieldClock',

    _init() {
        this.actor = new St.BoxLayout({ style_class: 'screen-shield-clock',
                                        vertical: true });

        this._time = new St.Label({ style_class: 'screen-shield-clock-time' });
        this._date = new St.Label({ style_class: 'screen-shield-clock-date' });

        this.actor.add(this._time, { x_align: St.Align.MIDDLE });
        this.actor.add(this._date, { x_align: St.Align.MIDDLE });

        this._wallClock = new GnomeDesktop.WallClock({ time_only: true });
        this._wallClock.connect('notify::clock', this._updateClock.bind(this));

        this._updateClock();
    },

    _updateClock() {
        this._time.text = this._wallClock.clock;

        let date = new Date();
        /* Translators: This is a time format for a date in
           long format */
        let dateFormat = Shell.util_translate_time_string(N_("%A, %B %d"));
        this._date.text = date.toLocaleFormat(dateFormat);
    },

    destroy() {
        this.actor.destroy();
        this._wallClock.run_dispose();
    }
});

var NotificationsBox = new Lang.Class({
    Name: 'NotificationsBox',

    _init() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        name: 'screenShieldNotifications',
                                        style_class: 'screen-shield-notifications-container' });

        this._scrollView = new St.ScrollView({ x_fill: false, x_align: St.Align.START,
                                               hscrollbar_policy: Gtk.PolicyType.NEVER });
        this._notificationBox = new St.BoxLayout({ vertical: true,
                                                   style_class: 'screen-shield-notifications-container' });
        this._scrollView.add_actor(this._notificationBox);

        this.actor.add(this._scrollView, { x_fill: true, x_align: St.Align.START });

        this._sources = new Map();
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source, true);
        });
        this._updateVisibility();

        this._sourceAddedId = Main.messageTray.connect('source-added', this._sourceAdded.bind(this));
    },

    destroy() {
        if (this._sourceAddedId) {
            Main.messageTray.disconnect(this._sourceAddedId);
            this._sourceAddedId = 0;
        }

        let items = this._sources.entries();
        for (let [source, obj] of items) {
            this._removeSource(source, obj);
        }

        this.actor.destroy();
    },

    _updateVisibility() {
        this._notificationBox.visible =
            this._notificationBox.get_children().some(a => a.visible);

        this.actor.visible = this._notificationBox.visible;
    },

    _makeNotificationCountText(count, isChat) {
        if (isChat)
            return ngettext("%d new message", "%d new messages", count).format(count);
        else
            return ngettext("%d new notification", "%d new notifications", count).format(count);
    },

    _makeNotificationSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        box.add(sourceActor, { y_fill: true });

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

    _makeNotificationDetailedSource(source, box) {
        let sourceActor = new MessageTray.SourceActor(source, SUMMARY_ICON_SIZE);
        let sourceBin = new St.Bin({ y_align: St.Align.START,
                                     x_align: St.Align.START,
                                     child: sourceActor });
        box.add(sourceBin);

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: false, y_align: St.Align.START });

        let title = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
        textBox.add(title);

        let visible = false;
        for (let i = 0; i < source.notifications.length; i++) {
            let n = source.notifications[i];

            if (n.acknowledged)
                continue;

            let body = '';
            if (n.bannerBodyText) {
                body = n.bannerBodyMarkup ? n.bannerBodyText
                                          : GLib.markup_escape_text(n.bannerBodyText, -1);
            }

            let label = new St.Label({ style_class: 'screen-shield-notification-count-text' });
            label.clutter_text.set_markup('<b>' + n.title + '</b> ' + body);
            textBox.add(label);

            visible = true;
        }

        box.visible = visible;
        return [title, null];
    },

    _showSource(source, obj, box) {
        if (obj.detailed) {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationDetailedSource(source, box);
        } else {
            [obj.titleLabel, obj.countLabel] = this._makeNotificationSource(source, box);
        }

        box.visible = obj.visible && (source.unseenCount > 0);
    },

    _sourceAdded(tray, source, initial) {
        let obj = {
            visible: source.policy.showInLockScreen,
            detailed: source.policy.detailsInLockScreen,
            sourceDestroyId: 0,
            sourceCountChangedId: 0,
            sourceTitleChangedId: 0,
            sourceUpdatedId: 0,
            sourceBox: null,
            titleLabel: null,
            countLabel: null,
        };

        obj.sourceBox = new St.BoxLayout({ style_class: 'screen-shield-notification-source',
                                           x_expand: true });
        this._showSource(source, obj, obj.sourceBox);
        this._notificationBox.add(obj.sourceBox, { x_fill: false, x_align: St.Align.START });

        obj.sourceCountChangedId = source.connect('count-updated', source => {
            this._countChanged(source, obj);
        });
        obj.sourceTitleChangedId = source.connect('title-changed', source => {
            this._titleChanged(source, obj);
        });
        obj.policyChangedId = source.policy.connect('policy-changed', (policy, key) => {
            if (key == 'show-in-lock-screen')
                this._visibleChanged(source, obj);
            else
                this._detailedChanged(source, obj);
        });
        obj.sourceDestroyId = source.connect('destroy', source => {
            this._onSourceDestroy(source, obj);
        });

        this._sources.set(source, obj);

        if (!initial) {
            // block scrollbars while animating, if they're not needed now
            let boxHeight = this._notificationBox.height;
            if (this._scrollView.height >= boxHeight)
                this._scrollView.vscrollbar_policy = Gtk.PolicyType.NEVER;

            let widget = obj.sourceBox;
            let [, natHeight] = widget.get_preferred_height(-1);
            widget.height = 0;
            Tweener.addTween(widget,
                             { height: natHeight,
                               transition: 'easeOutQuad',
                               time: 0.25,
                               onComplete() {
                                   this._scrollView.vscrollbar_policy = Gtk.PolicyType.AUTOMATIC;
                                   widget.set_height(-1);
                               },
                               onCompleteScope: this
                             });

            this._updateVisibility();
            if (obj.sourceBox.visible)
                this.emit('wake-up-screen');
        }
    },

    _titleChanged(source, obj) {
        obj.titleLabel.text = source.title;
    },

    _countChanged(source, obj) {
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

        obj.sourceBox.visible = obj.visible && (source.unseenCount > 0);

        this._updateVisibility();
        if (obj.sourceBox.visible)
            this.emit('wake-up-screen');
    },

    _visibleChanged(source, obj) {
        if (obj.visible == source.policy.showInLockScreen)
            return;

        obj.visible = source.policy.showInLockScreen;
        obj.sourceBox.visible = obj.visible && source.unseenCount > 0;

        this._updateVisibility();
        if (obj.sourceBox.visible)
            this.emit('wake-up-screen');
    },

    _detailedChanged(source, obj) {
        if (obj.detailed == source.policy.detailsInLockScreen)
            return;

        obj.detailed = source.policy.detailsInLockScreen;

        obj.sourceBox.destroy_all_children();
        obj.titleLabel = obj.countLabel = null;
        this._showSource(source, obj, obj.sourceBox);
    },

    _onSourceDestroy(source, obj) {
        this._removeSource(source, obj);
        this._updateVisibility();
    },

    _removeSource(source, obj) {
        obj.sourceBox.destroy();
        obj.sourceBox = obj.titleLabel = obj.countLabel = null;

        source.disconnect(obj.sourceDestroyId);
        source.disconnect(obj.sourceCountChangedId);
        source.disconnect(obj.sourceTitleChangedId);
        source.policy.disconnect(obj.policyChangedId);

        this._sources.delete(source);
    },
});
Signals.addSignalMethods(NotificationsBox.prototype);

var Arrow = new Lang.Class({
    Name: 'Arrow',
    Extends: St.Bin,

    _init(params) {
        this.parent(params);
        this.x_fill = this.y_fill = true;

        this._drawingArea = new St.DrawingArea();
        this._drawingArea.connect('repaint', this._drawArrow.bind(this));
        this.child = this._drawingArea;

        this._shadowHelper = null;
        this._shadowWidth = this._shadowHeight = 0;
    },

    _drawArrow(arrow) {
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
        cr.$dispose();
    },

    vfunc_get_paint_volume(volume) {
        if (!this.parent(volume))
            return false;

        if (!this._shadow)
            return true;

        let shadow_box = new Clutter.ActorBox();
        this._shadow.get_box(this._drawingArea.get_allocation_box(), shadow_box);

        volume.set_width(Math.max(shadow_box.x2 - shadow_box.x1, volume.get_width()));
        volume.set_height(Math.max(shadow_box.y2 - shadow_box.y1, volume.get_height()));

        return true;
    },

    vfunc_style_changed() {
        let node = this.get_theme_node();
        this._shadow = node.get_shadow('-arrow-shadow');
        if (this._shadow)
            this._shadowHelper = St.ShadowHelper.new(this._shadow);
        else
            this._shadowHelper = null;

        this.parent();
    },

    vfunc_paint() {
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
 * If you are setting org.gnome.desktop.session.idle-delay directly in dconf,
 * rather than through System Settings, you also need to set
 * org.gnome.settings-daemon.plugins.power.sleep-display-ac and
 * org.gnome.settings-daemon.plugins.power.sleep-display-battery to the same value.
 * This will ensure that the screen blanks at the right time when it fades out.
 * https://bugzilla.gnome.org/show_bug.cgi?id=668703 explains the dependency.
 */
var ScreenShield = new Lang.Class({
    Name: 'ScreenShield',

    _init() {
        this.actor = Main.layoutManager.screenShieldGroup;

        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                reactive: true,
                                                can_focus: true,
                                                name: 'lockScreenGroup',
                                                visible: false,
                                              });
        this._lockScreenGroup.connect('key-press-event',
                                      this._onLockScreenKeyPress.bind(this));
        this._lockScreenGroup.connect('scroll-event',
                                      this._onLockScreenScroll.bind(this));
        Main.ctrlAltTabManager.addGroup(this._lockScreenGroup, _("Lock"), 'changes-prevent-symbolic');

        this._lockScreenContents = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                                   name: 'lockScreenContents' });
        this._lockScreenContents.add_constraint(new Layout.MonitorConstraint({ primary: true }));

        this._lockScreenGroup.add_actor(this._lockScreenContents);

        this._backgroundGroup = new Clutter.Actor();

        this._lockScreenGroup.add_actor(this._backgroundGroup);
        this._backgroundGroup.lower_bottom();
        this._bgManagers = [];

        this._updateBackgrounds();
        Main.layoutManager.connect('monitors-changed', this._updateBackgrounds.bind(this));

        this._arrowAnimationId = 0;
        this._arrowWatchId = 0;
        this._arrowActiveWatchId = 0;
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
        this._dragAction.connect('gesture-begin', this._onDragBegin.bind(this));
        this._dragAction.connect('gesture-progress', this._onDragMotion.bind(this));
        this._dragAction.connect('gesture-end', this._onDragEnd.bind(this));
        this._lockScreenGroup.add_action(this._dragAction);

        this._lockDialogGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                reactive: true,
                                                pivot_point: new Clutter.Point({ x: 0.5, y: 0.5 }),
                                                name: 'lockDialogGroup' });

        this.actor.add_actor(this._lockDialogGroup);
        this.actor.add_actor(this._lockScreenGroup);

        this._presence = new GnomeSession.Presence((proxy, error) => {
            if (error) {
                logError(error, 'Error while reading gnome-session presence');
                return;
            }

            this._onStatusChanged(proxy.status);
        });
        this._presence.connectSignal('StatusChanged', (proxy, senderName, [status]) => {
            this._onStatusChanged(status);
        });

        this._screenSaverDBus = new ShellDBus.ScreenSaverDBus(this);

        this._smartcardManager = SmartcardManager.getSmartcardManager();
        this._smartcardManager.connect('smartcard-inserted',
                                       (manager, token) => {
                                           if (this._isLocked && token.UsedToLogin)
                                               this._liftShield(true, 0);
                                       });

        this._oVirtCredentialsManager = OVirt.getOVirtCredentialsManager();
        this._oVirtCredentialsManager.connect('user-authenticated',
                                              () => {
                                                  if (this._isLocked)
                                                      this._liftShield(true, 0);
                                              });

        this._loginManager = LoginManager.getLoginManager();
        this._loginManager.connect('prepare-for-sleep',
                                   this._prepareForSleep.bind(this));

        this._loginSession = null;
        this._loginManager.getCurrentSessionProxy(sessionProxy => {
            this._loginSession = sessionProxy;
            this._loginSession.connectSignal('Lock',
                                             () => { this.lock(false); });
            this._loginSession.connectSignal('Unlock',
                                             () => { this.deactivate(false); });
            this._loginSession.connect('g-properties-changed', this._syncInhibitor.bind(this));
            this._syncInhibitor();
        });

        this._settings = new Gio.Settings({ schema_id: SCREENSAVER_SCHEMA });
        this._settings.connect('changed::' + LOCK_ENABLED_KEY, this._syncInhibitor.bind(this));

        this._lockSettings = new Gio.Settings({ schema_id: LOCKDOWN_SCHEMA });
        this._lockSettings.connect('changed::' + DISABLE_LOCK_KEY, this._syncInhibitor.bind(this));

        this._isModal = false;
        this._hasLockScreen = false;
        this._isGreeter = false;
        this._isActive = false;
        this._isLocked = false;
        this._inUnlockAnimation = false;
        this._activationTime = 0;
        this._becameActiveId = 0;
        this._lockTimeoutId = 0;

        // The "long" lightbox is used for the longer (20 seconds) fade from session
        // to idle status, the "short" is used for quickly fading to black when locking
        // manually
        this._longLightbox = new Lightbox.Lightbox(Main.uiGroup,
                                                   { inhibitEvents: true,
                                                     fadeFactor: 1 });
        this._longLightbox.connect('shown', this._onLongLightboxShown.bind(this));
        this._shortLightbox = new Lightbox.Lightbox(Main.uiGroup,
                                                    { inhibitEvents: true,
                                                      fadeFactor: 1 });
        this._shortLightbox.connect('shown', this._onShortLightboxShown.bind(this));

        this.idleMonitor = Meta.IdleMonitor.get_core();
        this._cursorTracker = Meta.CursorTracker.get_for_display(global.display);

        this._syncInhibitor();
    },

    _setActive(active) {
        let prevIsActive = this._isActive;
        this._isActive = active;

        if (prevIsActive != this._isActive)
            this.emit('active-changed');

        if (this._loginSession)
            this._loginSession.SetLockedHintRemote(active);

        this._syncInhibitor();
    },

    _createBackground(monitorIndex) {
        let monitor = Main.layoutManager.monitors[monitorIndex];
        let widget = new St.Widget({ style_class: 'screen-shield-background',
                                     x: monitor.x,
                                     y: monitor.y,
                                     width: monitor.width,
                                     height: monitor.height });

        let bgManager = new Background.BackgroundManager({ container: widget,
                                                           monitorIndex: monitorIndex,
                                                           controlPosition: false,
                                                           settingsSchema: SCREENSAVER_SCHEMA });

        this._bgManagers.push(bgManager);

        this._backgroundGroup.add_child(widget);
    },

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];
        this._backgroundGroup.destroy_all_children();

        for (let i = 0; i < Main.layoutManager.monitors.length; i++)
            this._createBackground(i);
    },

    _liftShield(onPrimary, velocity) {
        if (this._isLocked) {
            if (this._ensureUnlockDialog(onPrimary, true /* allowCancel */))
                this._hideLockScreen(true /* animate */, velocity);
        } else {
            this.deactivate(true /* animate */);
        }
    },

    _maybeCancelDialog() {
        if (!this._dialog)
            return;

        this._dialog.cancel();
        if (this._isGreeter) {
            // LoginDialog.cancel() will grab the key focus
            // on its own, so ensure it stays on lock screen
            // instead
            this._lockScreenGroup.grab_key_focus();
        } else {
            this._dialog = null;
        }
    },

    _becomeModal() {
        if (this._isModal)
            return true;

        this._isModal = Main.pushModal(this.actor, { actionMode: Shell.ActionMode.LOCK_SCREEN });
        if (this._isModal)
            return true;

        // We failed to get a pointer grab, it means that
        // something else has it. Try with a keyboard grab only
        this._isModal = Main.pushModal(this.actor, { options: Meta.ModalOptions.POINTER_ALREADY_GRABBED,
                                                     actionMode: Shell.ActionMode.LOCK_SCREEN });
        return this._isModal;
    },

    _onLockScreenKeyPress(actor, event) {
        let symbol = event.get_key_symbol();
        let unichar = event.get_key_unicode();

        // Do nothing if the lock screen is not fully shown.
        // This avoids reusing the previous (and stale) unlock
        // dialog if esc is pressed while the curtain is going
        // down after cancel.

        if (this._lockScreenState != MessageTray.State.SHOWN)
            return Clutter.EVENT_PROPAGATE;

        let isEnter = (symbol == Clutter.KEY_Return ||
                       symbol == Clutter.KEY_KP_Enter ||
                       symbol == Clutter.KEY_ISO_Enter);
        let isEscape = (symbol == Clutter.KEY_Escape);
        let isLiftChar = (GLib.unichar_isprint(unichar) &&
                          (this._isLocked || !GLib.unichar_isgraph(unichar)));
        if (!isEnter && !isEscape && !isLiftChar)
            return Clutter.EVENT_PROPAGATE;

        if (this._isLocked &&
            this._ensureUnlockDialog(true, true) &&
            GLib.unichar_isgraph(unichar))
            this._dialog.addCharacter(unichar);

        this._liftShield(true, 0);
        return Clutter.EVENT_STOP;
    },

    _onLockScreenScroll(actor, event) {
        if (this._lockScreenState != MessageTray.State.SHOWN)
            return Clutter.EVENT_PROPAGATE;

        let delta = 0;
        if (event.get_scroll_direction() == Clutter.ScrollDirection.SMOOTH)
            delta = Math.abs(event.get_scroll_delta()[0]);
        else
            delta = 5;

        this._lockScreenScrollCounter += delta;

        // 7 standard scrolls to lift up
        if (this._lockScreenScrollCounter > 35) {
            this._liftShield(true, 0);
        }

        return Clutter.EVENT_STOP;
    },

    _syncInhibitor() {
        let lockEnabled = this._settings.get_boolean(LOCK_ENABLED_KEY);
        let lockLocked = this._lockSettings.get_boolean(DISABLE_LOCK_KEY);
        let inhibit = (this._loginSession && this._loginSession.Active &&
                       !this._isActive && lockEnabled && !lockLocked);
        if (inhibit) {
            this._loginManager.inhibit(_("GNOME needs to lock the screen"),
                inhibitor => {
                    if (this._inhibitor)
                        this._inhibitor.close(null);
                    this._inhibitor = inhibitor;
                });
        } else {
            if (this._inhibitor)
                this._inhibitor.close(null);
            this._inhibitor = null;
        }
    },

    _prepareForSleep(loginManager, aboutToSuspend) {
        if (aboutToSuspend) {
            if (this._settings.get_boolean(LOCK_ENABLED_KEY))
                this.lock(true);
        } else {
            this._wakeUpScreen();
        }
    },

    _animateArrows() {
        let arrows = this._arrowContainer.get_children();
        let unitaryDelay = ARROW_ANIMATION_TIME / (arrows.length + 1);
        let maxOpacity = 255 * ARROW_ANIMATION_PEAK_OPACITY;
        for (let i = 0; i < arrows.length; i++) {
            arrows[i].opacity = 0;
            Tweener.addTween(arrows[i],
                             { opacity: 0,
                               delay: unitaryDelay * (N_ARROWS - (i + 1)),
                               time: ARROW_ANIMATION_TIME,
                               transition(t, b, c, d) {
                                 if (t < d/2)
                                     return TweenerEquations.easeOutQuad(t, 0, maxOpacity, d/2);
                                 else
                                     return TweenerEquations.easeInQuad(t - d/2, maxOpacity, -maxOpacity, d/2);
                               }
                             });
        }

        return GLib.SOURCE_CONTINUE;
    },

    _onDragBegin() {
        Tweener.removeTweens(this._lockScreenGroup);
        this._lockScreenState = MessageTray.State.HIDING;

        if (this._isLocked)
            this._ensureUnlockDialog(false, false);

        return true;
    },

    _onDragMotion() {
	let [origX, origY] = this._dragAction.get_press_coords(0);
	let [currentX, currentY] = this._dragAction.get_motion_coords(0);

	let newY = currentY - origY;
	newY = clamp(newY, -global.stage.height, 0);

	this._lockScreenGroup.y = newY;

	return true;
    },

    _onDragEnd(action, actor, eventX, eventY, modifiers) {
        if (this._lockScreenState != MessageTray.State.HIDING)
            return;
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
                               onComplete() {
                                   this._lockScreenGroup.fixed_position_set = false;
                                   this._lockScreenState = MessageTray.State.SHOWN;
                               },
                               onCompleteScope: this,
                             });

            this._maybeCancelDialog();
        }
    },

    _onStatusChanged(status) {
        if (status != GnomeSession.PresenceStatus.IDLE)
            return;

        this._maybeCancelDialog();

        if (this._longLightbox.actor.visible ||
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

        if (this._activationTime == 0)
            this._activationTime = GLib.get_monotonic_time();

        let shouldLock = this._settings.get_boolean(LOCK_ENABLED_KEY) && !this._isLocked;

        if (shouldLock) {
            let lockTimeout = Math.max(STANDARD_FADE_TIME, this._settings.get_uint(LOCK_DELAY_KEY));
            this._lockTimeoutId = Mainloop.timeout_add(lockTimeout * 1000,
                                                       () => {
                                                           this._lockTimeoutId = 0;
                                                           this.lock(false);
                                                           return GLib.SOURCE_REMOVE;
                                                       });
            GLib.Source.set_name_by_id(this._lockTimeoutId, '[gnome-shell] this.lock');
        }

        this._activateFade(this._longLightbox, STANDARD_FADE_TIME);
    },

    _activateFade(lightbox, time) {
        Main.uiGroup.set_child_above_sibling(lightbox.actor, null);
        lightbox.show(time);

        if (this._becameActiveId == 0)
            this._becameActiveId = this.idleMonitor.add_user_active_watch(this._onUserBecameActive.bind(this));
    },

    _onUserBecameActive() {
        // This function gets called here when the user becomes active
        // after we activated a lightbox
        // There are two possibilities here:
        // - we're called when already locked/active; isLocked or isActive is true,
        //   we just go back to the lock screen curtain
        //   (isActive == isLocked == true: normal case
        //    isActive == false, isLocked == true: during the fade for manual locking
        //    isActive == true, isLocked == false: after session idle, before lock-delay)
        // - we're called because the session is IDLE but before the lightbox
        //   is fully shown; at this point isActive is false, so we just hide
        //   the lightbox, reset the activationTime and go back to the unlocked
        //   desktop
        //   using deactivate() is a little of overkill, but it ensures we
        //   don't forget of some bit like modal, DBus properties or idle watches
        //
        // Note: if the (long) lightbox is shown then we're necessarily
        // active, because we call activate() without animation.

        this.idleMonitor.remove_watch(this._becameActiveId);
        this._becameActiveId = 0;

        if (this._isActive || this._isLocked) {
            this._longLightbox.hide();
            this._shortLightbox.hide();
        } else {
            this.deactivate(false);
        }
    },

    _onLongLightboxShown() {
        this.activate(false);
    },

    _onShortLightboxShown() {
        this._completeLockScreenShown();
    },

    showDialog() {
        if (!this._becomeModal()) {
            // In the login screen, this is a hard error. Fail-whale
            log('Could not acquire modal grab for the login screen. Aborting login process.');
            Meta.quit(Meta.ExitCode.ERROR);
        }

        this.actor.show();
        this._isGreeter = Main.sessionMode.isGreeter;
        this._isLocked = true;
        if (this._ensureUnlockDialog(true, true))
            this._hideLockScreen(false, 0);
    },

    _hideLockScreenComplete() {
        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');

        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup.hide();

        if (this._dialog) {
            this._dialog.actor.grab_key_focus();
            this._dialog.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        }
    },

    _hideLockScreen(animate, velocity) {
        if (this._lockScreenState == MessageTray.State.HIDDEN)
            return;

        this._lockScreenState = MessageTray.State.HIDING;

        Tweener.removeTweens(this._lockScreenGroup);

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

            Tweener.addTween(this._lockScreenGroup,
                             { y: -h,
                               time: time,
                               transition: 'easeInQuad',
                               onComplete: this._hideLockScreenComplete.bind(this),
                             });
        } else {
            this._hideLockScreenComplete();
        }

        this._cursorTracker.set_pointer_visible(true);
    },

    _ensureUnlockDialog(onPrimary, allowCancel) {
        if (!this._dialog) {
            let constructor = Main.sessionMode.unlockDialog;
            if (!constructor) {
                // This session mode has no locking capabilities
                this.deactivate(true);
                return false;
            }

            this._dialog = new constructor(this._lockDialogGroup);


            let time = global.get_current_time();
            if (!this._dialog.open(time, onPrimary)) {
                // This is kind of an impossible error: we're already modal
                // by the time we reach this...
                log('Could not open login dialog: failed to acquire grab');
                this.deactivate(true);
                return false;
            }

            this._dialog.connect('failed', this._onUnlockFailed.bind(this));
        }

        this._dialog.allowCancel = allowCancel;
        return true;
    },

    _onUnlockFailed() {
        this._resetLockScreen({ animateLockScreen: true,
                                fadeToBlack: false });
    },

    _resetLockScreen(params) {
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

        let fadeToBlack = params.fadeToBlack;

        if (params.animateLockScreen) {
            this._lockScreenGroup.y = -global.screen_height;
            Tweener.removeTweens(this._lockScreenGroup);
            Tweener.addTween(this._lockScreenGroup,
                             { y: 0,
                               time: MANUAL_FADE_TIME,
                               transition: 'easeOutQuad',
                               onComplete() {
                                   this._lockScreenShown({ fadeToBlack: fadeToBlack,
                                                           animateFade: true });
                               },
                               onCompleteScope: this
                             });
        } else {
            this._lockScreenGroup.fixed_position_set = false;
            this._lockScreenShown({ fadeToBlack: fadeToBlack,
                                    animateFade: false });
        }

        this._lockScreenGroup.grab_key_focus();

        if (Main.sessionMode.currentMode != 'lock-screen')
            Main.sessionMode.pushMode('lock-screen');
    },

    _startArrowAnimation() {
        this._arrowActiveWatchId = 0;

        if (!this._arrowAnimationId) {
            this._arrowAnimationId = Mainloop.timeout_add(6000, this._animateArrows.bind(this));
            GLib.Source.set_name_by_id(this._arrowAnimationId, '[gnome-shell] this._animateArrows');
            this._animateArrows();
        }

        if (!this._arrowWatchId)
            this._arrowWatchId = this.idleMonitor.add_idle_watch(ARROW_IDLE_TIME,
                                                                 this._pauseArrowAnimation.bind(this));
    },

    _pauseArrowAnimation() {
        if (this._arrowAnimationId) {
            Mainloop.source_remove(this._arrowAnimationId);
            this._arrowAnimationId = 0;
        }

        if (!this._arrowActiveWatchId)
            this._arrowActiveWatchId = this.idleMonitor.add_user_active_watch(this._startArrowAnimation.bind(this));
    },

    _stopArrowAnimation() {
        if (this._arrowAnimationId) {
            Mainloop.source_remove(this._arrowAnimationId);
            this._arrowAnimationId = 0;
        }
        if (this._arrowActiveWatchId) {
            this.idleMonitor.remove_watch(this._arrowActiveWatchId);
            this._arrowActiveWatchId = 0;
        }
        if (this._arrowWatchId) {
            this.idleMonitor.remove_watch(this._arrowWatchId);
            this._arrowWatchId = 0;
        }
    },

    _checkArrowAnimation() {
        let idleTime = this.idleMonitor.get_idletime();

        if (idleTime < ARROW_IDLE_TIME)
            this._startArrowAnimation();
        else
            this._pauseArrowAnimation();
    },

    _lockScreenShown(params) {
        if (this._dialog && !this._isGreeter) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._checkArrowAnimation();

        let motionId = global.stage.connect('captured-event', (stage, event) => {
            if (event.type() == Clutter.EventType.MOTION) {
                this._cursorTracker.set_pointer_visible(true);
                global.stage.disconnect(motionId);
            }

            return Clutter.EVENT_PROPAGATE;
        });
        this._cursorTracker.set_pointer_visible(false);

        this._lockScreenState = MessageTray.State.SHOWN;
        this._lockScreenGroup.fixed_position_set = false;
        this._lockScreenScrollCounter = 0;

        if (params.fadeToBlack && params.animateFade) {
            // Take a beat

            let id = Mainloop.timeout_add(1000 * MANUAL_FADE_TIME, () => {
                this._activateFade(this._shortLightbox, MANUAL_FADE_TIME);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._activateFade');
        } else {
            if (params.fadeToBlack)
                this._activateFade(this._shortLightbox, 0);

            this._completeLockScreenShown();
        }
    },

    _completeLockScreenShown() {
        this._setActive(true);
        this.emit('lock-screen-shown');
    },

    // Some of the actors in the lock screen are heavy in
    // resources, so we only create them when needed
    _ensureLockScreen() {
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
        this._wakeUpScreenId = this._notificationsBox.connect('wake-up-screen', this._wakeUpScreen.bind(this));
        this._lockScreenContentsBox.add(this._notificationsBox.actor, { x_fill: true,
                                                                        y_fill: true,
                                                                        expand: true });

        this._hasLockScreen = true;
    },

    _wakeUpScreen() {
        this._onUserBecameActive();
        this.emit('wake-up-screen');
    },

    _clearLockScreen() {
        this._clock.destroy();
        this._clock = null;

        if (this._notificationsBox) {
            this._notificationsBox.disconnect(this._wakeUpScreenId);
            this._notificationsBox.destroy();
            this._notificationsBox = null;
        }

        this._stopArrowAnimation();

        this._lockScreenContentsBox.destroy();

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

    deactivate(animate) {
        if (this._dialog)
            this._dialog.finish(() => { this._continueDeactivate(animate); });
        else
            this._continueDeactivate(animate);
    },

    _continueDeactivate(animate) {
        this._hideLockScreen(animate, 0);

        if (this._hasLockScreen)
            this._clearLockScreen();

        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');
        if (Main.sessionMode.currentMode == 'unlock-dialog')
            Main.sessionMode.popMode('unlock-dialog');

        if (this._isGreeter) {
            // We don't want to "deactivate" any more than
            // this. In particular, we don't want to drop
            // the modal, hide ourselves or destroy the dialog
            // But we do want to set isActive to false, so that
            // gnome-session will reset the idle counter, and
            // gnome-settings-daemon will stop blanking the screen

            this._activationTime = 0;
            this._setActive(false);
            return;
        }

        if (this._dialog && !this._isGreeter)
            this._dialog.popModal();

        if (this._isModal) {
            Main.popModal(this.actor);
            this._isModal = false;
        }

        Tweener.addTween(this._lockDialogGroup, {
            scale_x: 0,
            scale_y: 0,
            time: animate ? Overview.ANIMATION_TIME : 0,
            transition: 'easeOutQuad',
            onComplete: this._completeDeactivate.bind(this),
            onCompleteScope: this
        });
    },

    _completeDeactivate() {
        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._longLightbox.hide();
        this._shortLightbox.hide();
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
        this._setActive(false);
        this._isLocked = false;
        this.emit('locked-changed');
        global.set_runtime_state(LOCKED_STATE_STR, null);
    },

    activate(animate) {
        if (this._activationTime == 0)
            this._activationTime = GLib.get_monotonic_time();

        this.actor.show();

        if (Main.sessionMode.currentMode != 'unlock-dialog' &&
            Main.sessionMode.currentMode != 'lock-screen') {
            this._isGreeter = Main.sessionMode.isGreeter;
            if (!this._isGreeter)
                Main.sessionMode.pushMode('unlock-dialog');
        }

        this._resetLockScreen({ animateLockScreen: animate,
                                fadeToBlack: true });
        // On wayland, a crash brings down the entire session, so we don't
        // need to defend against being restarted unlocked
        if (!Meta.is_wayland_compositor())
            global.set_runtime_state(LOCKED_STATE_STR, GLib.Variant.new('b', true));

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

    lock(animate) {
        if (this._lockSettings.get_boolean(DISABLE_LOCK_KEY)) {
            log('Screen lock is locked down, not locking') // lock, lock - who's there?
            return;
        }

        // Warn the user if we can't become modal
        if (!this._becomeModal()) {
            Main.notifyError(_("Unable to lock"),
                             _("Lock was blocked by an application"));
            return;
        }

        // Clear the clipboard - otherwise, its contents may be leaked
        // to unauthorized parties by pasting into the unlock dialog's
        // password entry and unmasking the entry
        St.Clipboard.get_default().set_text(St.ClipboardType.CLIPBOARD, '');
        St.Clipboard.get_default().set_text(St.ClipboardType.PRIMARY, '');

        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(GLib.get_user_name());

        if (this._isGreeter)
            this._isLocked = true;
        else
            this._isLocked = user.password_mode != AccountsService.UserPasswordMode.NONE;

        this.activate(animate);

        this.emit('locked-changed');
    },

    // If the previous shell crashed, and gnome-session restarted us, then re-lock
    lockIfWasLocked() {
        if (!this._settings.get_boolean(LOCK_ENABLED_KEY))
            return;
        let wasLocked = global.get_runtime_state('b', LOCKED_STATE_STR);
        if (wasLocked === null)
            return;
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this.lock(false);
        });
    }
});
Signals.addSignalMethods(ScreenShield.prototype);
