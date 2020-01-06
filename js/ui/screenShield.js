// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { AccountsService, Clutter, Gio,
        GLib, Graphene, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

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

const { adjustAnimationTime } = imports.ui.environment;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';
const LOCK_DELAY_KEY = 'lock-delay';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_LOCK_KEY = 'disable-lock-screen';

const LOCKED_STATE_STR = 'screenShield.locked';

const BLUR_BRIGHTNESS = 0.55;
const BLUR_RADIUS = 200;

// fraction of screen height the arrow must reach before completing
// the slide up automatically
var ARROW_DRAG_THRESHOLD = 0.1;

// ScreenShield animation time
// - STANDARD_FADE_TIME is used when the session goes idle
// - MANUAL_FADE_TIME is used for lowering the shield when asked by the user,
//   or when cancelling the dialog
// - CURTAIN_SLIDE_TIME is used when raising the shield before unlocking
var STANDARD_FADE_TIME = 10000;
var MANUAL_FADE_TIME = 300;
var CURTAIN_SLIDE_TIME = 300;

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
var ScreenShield = class {
    constructor() {
        this.actor = Main.layoutManager.screenShieldGroup;

        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup = new St.Widget({
            x_expand: true,
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
        this._lockScreenGroup.set_child_below_sibling(this._backgroundGroup, null);
        this._bgManagers = [];

        this._updateBackgrounds();
        Main.layoutManager.connect('monitors-changed', this._updateBackgrounds.bind(this));

        this._dragAction = new Clutter.GestureAction();
        this._dragAction.connect('gesture-begin', this._onDragBegin.bind(this));
        this._dragAction.connect('gesture-progress', this._onDragMotion.bind(this));
        this._dragAction.connect('gesture-end', this._onDragEnd.bind(this));
        this._lockScreenGroup.add_action(this._dragAction);

        this._lockDialogGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                reactive: true,
                                                pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
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
                                               this._liftShield(0);
                                       });

        this._oVirtCredentialsManager = OVirt.getOVirtCredentialsManager();
        this._oVirtCredentialsManager.connect('user-authenticated',
                                              () => {
                                                  if (this._isLocked)
                                                      this._liftShield(0);
                                              });

        this._loginManager = LoginManager.getLoginManager();
        this._loginManager.connect('prepare-for-sleep',
                                   this._prepareForSleep.bind(this));

        this._loginSession = null;
        this._loginManager.getCurrentSessionProxy(sessionProxy => {
            this._loginSession = sessionProxy;
            this._loginSession.connectSignal('Lock',
                                             () => this.lock(false));
            this._loginSession.connectSignal('Unlock',
                                             () => this.deactivate(false));
            this._loginSession.connect('g-properties-changed', this._syncInhibitor.bind(this));
            this._syncInhibitor();
        });

        this._settings = new Gio.Settings({ schema_id: SCREENSAVER_SCHEMA });
        this._settings.connect(`changed::${LOCK_ENABLED_KEY}`, this._syncInhibitor.bind(this));

        this._lockSettings = new Gio.Settings({ schema_id: LOCKDOWN_SCHEMA });
        this._lockSettings.connect(`changed::${DISABLE_LOCK_KEY}`, this._syncInhibitor.bind(this));

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
        this._longLightbox.connect('notify::active', this._onLongLightbox.bind(this));
        this._shortLightbox = new Lightbox.Lightbox(Main.uiGroup,
                                                    { inhibitEvents: true,
                                                      fadeFactor: 1 });
        this._shortLightbox.connect('notify::active', this._onShortLightbox.bind(this));

        this.idleMonitor = Meta.IdleMonitor.get_core();
        this._cursorTracker = Meta.CursorTracker.get_for_display(global.display);

        this._syncInhibitor();
    }

    _setActive(active) {
        let prevIsActive = this._isActive;
        this._isActive = active;

        if (prevIsActive != this._isActive)
            this.emit('active-changed');

        if (this._loginSession)
            this._loginSession.SetLockedHintRemote(active);

        this._syncInhibitor();
    }

    _createBackground(monitorIndex) {
        let monitor = Main.layoutManager.monitors[monitorIndex];
        let widget = new St.Widget({ style_class: 'screen-shield-background',
                                     x: monitor.x,
                                     y: monitor.y,
                                     width: monitor.width,
                                     height: monitor.height });

        let bgManager = new Background.BackgroundManager({ container: widget,
                                                           monitorIndex,
                                                           controlPosition: false,
                                                           settingsSchema: SCREENSAVER_SCHEMA });

        this._bgManagers.push(bgManager);

        this._backgroundGroup.add_child(widget);

        widget.add_effect(new Shell.BlurEffect({
            brightness: BLUR_BRIGHTNESS,
            blur_radius: BLUR_RADIUS,
        }));
    }

    _updateBackgrounds() {
        for (let i = 0; i < this._bgManagers.length; i++)
            this._bgManagers[i].destroy();

        this._bgManagers = [];
        this._backgroundGroup.destroy_all_children();

        for (let i = 0; i < Main.layoutManager.monitors.length; i++)
            this._createBackground(i);
    }

    _liftShield(velocity) {
        if (this._isLocked) {
            if (this._ensureUnlockDialog(true /* allowCancel */))
                this._hideLockScreen(true /* animate */, velocity);
        } else {
            this.deactivate(true /* animate */);
        }
    }

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
    }

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
    }

    _onLockScreenKeyPress(actor, event) {
        let symbol = event.get_key_symbol();
        let unichar = event.get_key_unicode();

        // Do nothing if the lock screen is not fully shown.
        // This avoids reusing the previous (and stale) unlock
        // dialog if esc is pressed while the curtain is going
        // down after cancel.

        if (this._lockScreenState != MessageTray.State.SHOWN)
            return Clutter.EVENT_PROPAGATE;

        let isEnter = symbol == Clutter.KEY_Return ||
                       symbol == Clutter.KEY_KP_Enter ||
                       symbol == Clutter.KEY_ISO_Enter;
        let isEscape = symbol == Clutter.KEY_Escape;
        let isLiftChar = GLib.unichar_isprint(unichar) &&
                          (this._isLocked || !GLib.unichar_isgraph(unichar));
        if (!isEnter && !isEscape && !isLiftChar)
            return Clutter.EVENT_PROPAGATE;

        if (this._isLocked &&
            this._ensureUnlockDialog(true) &&
            GLib.unichar_isgraph(unichar))
            this._dialog.addCharacter(unichar);

        this._liftShield(0);
        return Clutter.EVENT_STOP;
    }

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
        if (this._lockScreenScrollCounter > 35)
            this._liftShield(0);

        return Clutter.EVENT_STOP;
    }

    _syncInhibitor() {
        let lockEnabled = this._settings.get_boolean(LOCK_ENABLED_KEY);
        let lockLocked = this._lockSettings.get_boolean(DISABLE_LOCK_KEY);
        let inhibit = this._loginSession && this._loginSession.Active &&
                       !this._isActive && lockEnabled && !lockLocked;
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
    }

    _prepareForSleep(loginManager, aboutToSuspend) {
        if (aboutToSuspend) {
            if (this._settings.get_boolean(LOCK_ENABLED_KEY))
                this.lock(true);
        } else {
            this._wakeUpScreen();
        }
    }

    _onDragBegin() {
        this._lockScreenGroup.remove_all_transitions();
        this._lockScreenState = MessageTray.State.HIDING;

        if (this._isLocked)
            this._ensureUnlockDialog(false);

        return true;
    }

    _onDragMotion() {
        let [, origY] = this._dragAction.get_press_coords(0);
        let [, currentY] = this._dragAction.get_motion_coords(0);

        let newY = currentY - origY;
        newY = clamp(newY, -global.stage.height, 0);

        this._lockScreenGroup.translation_y = newY;

        return true;
    }

    _onDragEnd(_action, _actor, _eventX, _eventY, _modifiers) {
        if (this._lockScreenState != MessageTray.State.HIDING)
            return;
        if (this._lockScreenGroup.translation_y < -(ARROW_DRAG_THRESHOLD * global.stage.height)) {
            // Complete motion automatically
            let [velocity_, velocityX_, velocityY] = this._dragAction.get_velocity(0);
            this._liftShield(-velocityY);
        } else {
            // restore the lock screen to its original place
            // try to use the same speed as the normal animation
            let h = global.stage.height;
            let duration = MANUAL_FADE_TIME * -this._lockScreenGroup.translation_y / h;
            this._lockScreenGroup.remove_all_transitions();
            this._lockScreenGroup.ease({
                translation_y: 0,
                duration,
                mode: Clutter.AnimationMode.EASE_IN_QUAD,
                onComplete: () => {
                    this._lockScreenGroup.fixed_position_set = false;
                    this._lockScreenState = MessageTray.State.SHOWN;
                },
            });

            this._maybeCancelDialog();
        }
    }

    _onStatusChanged(status) {
        if (status != GnomeSession.PresenceStatus.IDLE)
            return;

        this._maybeCancelDialog();

        if (this._longLightbox.visible) {
            // We're in the process of showing.
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
            let lockTimeout = Math.max(
                adjustAnimationTime(STANDARD_FADE_TIME),
                this._settings.get_uint(LOCK_DELAY_KEY) * 1000);
            this._lockTimeoutId = GLib.timeout_add(
                GLib.PRIORITY_DEFAULT,
                lockTimeout,
                () => {
                    this._lockTimeoutId = 0;
                    this.lock(false);
                    return GLib.SOURCE_REMOVE;
                });
            GLib.Source.set_name_by_id(this._lockTimeoutId, '[gnome-shell] this.lock');
        }

        this._activateFade(this._longLightbox, STANDARD_FADE_TIME);
    }

    _activateFade(lightbox, time) {
        Main.uiGroup.set_child_above_sibling(lightbox, null);
        lightbox.lightOn(time);

        if (this._becameActiveId == 0)
            this._becameActiveId = this.idleMonitor.add_user_active_watch(this._onUserBecameActive.bind(this));
    }

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
            this._longLightbox.lightOff();
            this._shortLightbox.lightOff();
        } else {
            this.deactivate(false);
        }
    }

    _onLongLightbox(lightBox) {
        if (lightBox.active)
            this.activate(false);
    }

    _onShortLightbox(lightBox) {
        if (lightBox.active)
            this._completeLockScreenShown();
    }

    showDialog() {
        if (!this._becomeModal()) {
            // In the login screen, this is a hard error. Fail-whale
            log('Could not acquire modal grab for the login screen. Aborting login process.');
            Meta.quit(Meta.ExitCode.ERROR);
        }

        this.actor.show();
        this._isGreeter = Main.sessionMode.isGreeter;
        this._isLocked = true;
        if (this._ensureUnlockDialog(true))
            this._hideLockScreen(false, 0);
    }

    _hideLockScreenComplete() {
        if (Main.sessionMode.currentMode == 'lock-screen')
            Main.sessionMode.popMode('lock-screen');

        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup.hide();

        if (this._dialog) {
            this._dialog.grab_key_focus();
            this._dialog.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
        }
    }

    _hideLockScreen(animate, velocity) {
        if (this._lockScreenState == MessageTray.State.HIDDEN)
            return;

        this._lockScreenState = MessageTray.State.HIDING;

        this._lockScreenGroup.remove_all_transitions();

        if (animate) {
            // Tween the lock screen out of screen
            // if velocity is not specified (i.e. we come here from pressing ESC),
            // use the same speed regardless of original position
            // if velocity is specified, it's in pixels per milliseconds
            let h = global.stage.height;
            let delta = h + this._lockScreenGroup.translation_y;
            let minVelocity = global.stage.height / CURTAIN_SLIDE_TIME;

            velocity = Math.max(minVelocity, velocity);
            let duration = delta / velocity;

            this._lockScreenGroup.ease({
                translation_y: -h,
                duration,
                mode: Clutter.AnimationMode.EASE_IN_QUAD,
                onComplete: () => this._hideLockScreenComplete(),
            });
        } else {
            this._hideLockScreenComplete();
        }

        this._cursorTracker.set_pointer_visible(true);
    }

    _ensureUnlockDialog(allowCancel) {
        if (!this._dialog) {
            let constructor = Main.sessionMode.unlockDialog;
            if (!constructor) {
                // This session mode has no locking capabilities
                this.deactivate(true);
                return false;
            }

            this._dialog = new constructor(this._lockDialogGroup);

            let time = global.get_current_time();
            if (!this._dialog.open(time)) {
                // This is kind of an impossible error: we're already modal
                // by the time we reach this...
                log('Could not open login dialog: failed to acquire grab');
                this.deactivate(true);
                return false;
            }

            this._dialog.connect('failed', this._onUnlockFailed.bind(this));
            this._wakeUpScreenId = this._dialog.connect(
                'wake-up-screen', this._wakeUpScreen.bind(this));
        }

        this._dialog.allowCancel = allowCancel;
        return true;
    }

    _onUnlockFailed() {
        this._resetLockScreen({ animateLockScreen: true,
                                fadeToBlack: false });
    }

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
            this._lockScreenGroup.translation_y = -global.screen_height;
            this._lockScreenGroup.remove_all_transitions();
            this._lockScreenGroup.ease({
                translation_y: 0,
                duration: MANUAL_FADE_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._lockScreenShown({ fadeToBlack, animateFade: true });
                },
            });
        } else {
            this._lockScreenGroup.fixed_position_set = false;
            this._lockScreenShown({ fadeToBlack, animateFade: false });
        }

        this._lockScreenGroup.grab_key_focus();

        if (Main.sessionMode.currentMode != 'lock-screen')
            Main.sessionMode.pushMode('lock-screen');
    }

    _lockScreenShown(params) {
        if (this._dialog && !this._isGreeter) {
            this._dialog.destroy();
            this._dialog = null;
        }

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

            let id = GLib.timeout_add(GLib.PRIORITY_DEFAULT, MANUAL_FADE_TIME, () => {
                this._activateFade(this._shortLightbox, MANUAL_FADE_TIME);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._activateFade');
        } else {
            if (params.fadeToBlack)
                this._activateFade(this._shortLightbox, 0);

            this._completeLockScreenShown();
        }
    }

    _completeLockScreenShown() {
        this._setActive(true);
        this.emit('lock-screen-shown');
    }

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

        this._lockScreenContents.add_actor(this._lockScreenContentsBox);

        this._hasLockScreen = true;
    }

    _wakeUpScreen() {
        this._onUserBecameActive();
        this.emit('wake-up-screen');
    }

    _clearLockScreen() {
        this._lockScreenContentsBox.destroy();

        this._hasLockScreen = false;
    }

    get locked() {
        return this._isLocked;
    }

    get active() {
        return this._isActive;
    }

    get activationTime() {
        return this._activationTime;
    }

    deactivate(animate) {
        if (this._dialog)
            this._dialog.finish(() => this._continueDeactivate(animate));
        else
            this._continueDeactivate(animate);
    }

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

        this._lockDialogGroup.ease({
            scale_x: 0,
            scale_y: 0,
            duration: animate ? Overview.ANIMATION_TIME : 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._completeDeactivate(),
        });
    }

    _completeDeactivate() {
        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._longLightbox.lightOff();
        this._shortLightbox.lightOff();
        this.actor.hide();

        if (this._becameActiveId != 0) {
            this.idleMonitor.remove_watch(this._becameActiveId);
            this._becameActiveId = 0;
        }

        if (this._lockTimeoutId != 0) {
            GLib.source_remove(this._lockTimeoutId);
            this._lockTimeoutId = 0;
        }

        this._activationTime = 0;
        this._setActive(false);
        this._isLocked = false;
        this.emit('locked-changed');
        global.set_runtime_state(LOCKED_STATE_STR, null);
    }

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
    }

    lock(animate) {
        if (this._lockSettings.get_boolean(DISABLE_LOCK_KEY)) {
            log('Screen lock is locked down, not locking'); // lock, lock - who's there?
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
    }

    // If the previous shell crashed, and gnome-session restarted us, then re-lock
    lockIfWasLocked() {
        if (!this._settings.get_boolean(LOCK_ENABLED_KEY))
            return;
        let wasLocked = global.get_runtime_state('b', LOCKED_STATE_STR);
        if (wasLocked === null)
            return;
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this.lock(false);
            return GLib.SOURCE_REMOVE;
        });
    }
};
Signals.addSignalMethods(ScreenShield.prototype);
