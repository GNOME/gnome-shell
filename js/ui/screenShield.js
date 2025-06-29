import AccountsService from 'gi://AccountsService';
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Signals from '../misc/signals.js';

import * as GnomeSession from '../misc/gnomeSession.js';
import * as OVirt from '../gdm/oVirt.js';
import * as LoginManager from '../misc/loginManager.js';
import * as Lightbox from './lightbox.js';
import * as Main from './main.js';
import * as Overview from './overview.js';
import * as MessageTray from './messageTray.js';
import * as ShellDBus from './shellDBus.js';
import * as SmartcardManager from '../misc/smartcardManager.js';

import {adjustAnimationTime} from '../misc/animationUtils.js';

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';
const LOCK_DELAY_KEY = 'lock-delay';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_LOCK_KEY = 'disable-lock-screen';

const LOCKED_STATE_STR = 'screenShield.locked';

// ScreenShield animation time
// - STANDARD_FADE_TIME is used when the session goes idle
// - MANUAL_FADE_TIME is used for lowering the shield when asked by the user,
//   or when cancelling the dialog
// - CURTAIN_SLIDE_TIME is used when raising the shield before unlocking
const STANDARD_FADE_TIME = 10000;
const MANUAL_FADE_TIME = 300;
const CURTAIN_SLIDE_TIME = 300;

/**
 * If you are setting org.gnome.desktop.session.idle-delay directly in dconf,
 * rather than through System Settings, you also need to set
 * org.gnome.settings-daemon.plugins.power.sleep-display-ac and
 * org.gnome.settings-daemon.plugins.power.sleep-display-battery to the same value.
 * This will ensure that the screen blanks at the right time when it fades out.
 * https://bugzilla.gnome.org/show_bug.cgi?id=668703 explains the dependency.
 */
export class ScreenShield extends Signals.EventEmitter {
    constructor() {
        super();

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

        this._lockDialogGroup = new St.Widget({
            x_expand: true,
            y_expand: true,
            reactive: true,
            can_focus: true,
            pivot_point: new Graphene.Point({x: 0.5, y: 0.5}),
            name: 'lockDialogGroup',
        });

        this.actor.add_child(this._lockScreenGroup);
        this.actor.add_child(this._lockDialogGroup);

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
                    this._activateDialog();
            });

        this._credentialManagers = {};
        this.addCredentialManager(OVirt.SERVICE_NAME, OVirt.getOVirtCredentialsManager());

        this._loginManager = LoginManager.getLoginManager();
        this._loginManager.connect('prepare-for-sleep',
            this._prepareForSleep.bind(this));

        this._loginSession = null;
        this._getLoginSession();

        this._settings = new Gio.Settings({schema_id: SCREENSAVER_SCHEMA});
        this._settings.connect(`changed::${LOCK_ENABLED_KEY}`, this._syncInhibitor.bind(this));

        this._lockSettings = new Gio.Settings({schema_id: LOCKDOWN_SCHEMA});
        this._lockSettings.connect(`changed::${DISABLE_LOCK_KEY}`, this._syncInhibitor.bind(this));

        this._isModal = false;
        this._isGreeter = false;
        this._isActive = false;
        this._isLocked = false;
        this._inUnlockAnimation = false;
        this._inhibited = false;
        this._activationTime = 0;
        this._becameActiveId = 0;
        this._lockTimeoutId = 0;

        // The "long" lightbox is used for the longer (20 seconds) fade from session
        // to idle status, the "short" is used for quickly fading to black when locking
        // manually
        this._longLightbox = new Lightbox.Lightbox(Main.uiGroup, {
            inhibitEvents: true,
            fadeFactor: 1,
        });
        this._longLightbox.connect('notify::active', this._onLongLightbox.bind(this));
        this._shortLightbox = new Lightbox.Lightbox(Main.uiGroup, {
            inhibitEvents: true,
            fadeFactor: 1,
        });
        this._shortLightbox.connect('notify::active', this._onShortLightbox.bind(this));

        this.idleMonitor = global.backend.get_core_idle_monitor();
        this._cursorTracker = global.backend.get_cursor_tracker();

        this._syncInhibitor();
    }

    async _getLoginSession() {
        this._loginSession = await this._loginManager.getCurrentSessionProxy();
        this._loginSession.connectSignal('Lock',
            () => this.lock(false));
        this._loginSession.connectSignal('Unlock',
            () => this.deactivate(false));
        this._loginSession.connect('g-properties-changed',
            () => this._syncInhibitor());
        this._syncInhibitor();
    }

    _setActive(active) {
        let prevIsActive = this._isActive;
        this._isActive = active;

        if (prevIsActive !== this._isActive)
            this.emit('active-changed');

        this._syncInhibitor();
    }

    _setLocked(locked) {
        let prevIsLocked = this._isLocked;
        this._isLocked = locked;

        if (prevIsLocked !== this._isLocked)
            this.emit('locked-changed');

        if (this._loginSession)
            this._loginSession.SetLockedHintAsync(locked).catch(logError);
    }

    _activateDialog() {
        if (this._isLocked) {
            this._ensureUnlockDialog(true /* allowCancel */);
            this._dialog.activate();
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
            this._dialog.grab_key_focus();
        }
    }

    _becomeModal() {
        if (this._isModal)
            return true;

        let grab = Main.pushModal(Main.uiGroup, {actionMode: Shell.ActionMode.LOCK_SCREEN});

        // We expect at least a keyboard grab here
        this._isModal = (grab.get_seat_state() & Clutter.GrabState.KEYBOARD) !== 0;
        if (this._isModal)
            this._grab = grab;
        else
            Main.popModal(grab);

        return this._isModal;
    }

    async _syncInhibitor() {
        const lockEnabled = this._settings.get_boolean(LOCK_ENABLED_KEY);
        const lockLocked = this._lockSettings.get_boolean(DISABLE_LOCK_KEY);
        const inhibit = !!this._loginSession && this._loginSession.Active &&
                         !this._isActive && lockEnabled && !lockLocked &&
                         !!Main.sessionMode.unlockDialog;

        if (inhibit === this._inhibited)
            return;

        this._inhibited = inhibit;

        this._inhibitCancellable?.cancel();
        this._inhibitCancellable = new Gio.Cancellable();

        if (inhibit) {
            try {
                this._inhibitor = await this._loginManager.inhibit(
                    _('GNOME needs to lock the screen'),
                    this._inhibitCancellable);
            } catch (e) {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                    log('Failed to inhibit suspend: %s'.format(e.message));
            }
        } else {
            this._inhibitor?.close(null);
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

    _onStatusChanged(status) {
        if (status !== GnomeSession.PresenceStatus.IDLE)
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
            // Just tell them to stop using this app
            //
            // XXX: another option is to kick the user into the gdm login
            // screen, where we're not affected by grabs
            Main.notifyError(
                _('Unable to lock'),
                _('Lock was blocked by an app'));
            return;
        }

        if (this._activationTime === 0)
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

        if (this._becameActiveId === 0)
            this._becameActiveId = this.idleMonitor.add_user_active_watch(this._onUserBecameActive.bind(this));
    }

    _onUserBecameActive() {
        // This function gets called here when the user becomes active
        // after we activated a lightbox
        // There are two possibilities here:
        // - we're called when already locked; we just go back to the lock screen curtain
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

        if (this._isLocked) {
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
            const error = new GLib.Error(
                Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED,
                'Could not acquire modal grab for the login screen. Aborting login process.');
            global.context.terminate_with_error(error);
        }

        this.actor.show();
        this._isGreeter = Main.sessionMode.isGreeter;
        this._isLocked = true;
        this._ensureUnlockDialog(true);
    }

    _hideLockScreenComplete() {
        this._lockScreenState = MessageTray.State.HIDDEN;
        this._lockScreenGroup.hide();

        if (this._dialog) {
            this._dialog.grab_key_focus();
            this._dialog.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
        }
    }

    _showPointer() {
        if (this._cursorVisibleInhibited) {
            const seat = global.stage.context.get_backend().get_default_seat();
            this._cursorTracker.uninhibit_cursor_visibility();
            seat.uninhibit_unfocus();
            this._cursorVisibleInhibited = false;
        }

        if (this._motionId) {
            this._lockDialogGroup.disconnect(this._motionId);
            this._motionId = 0;
        }
    }

    _hidePointer() {
        if (!this._cursorVisibleInhibited) {
            const seat = global.stage.context.get_backend().get_default_seat();
            seat.inhibit_unfocus();
            this._cursorTracker.inhibit_cursor_visibility();
            this._cursorVisibleInhibited = true;
        }
    }

    _hidePointerUntilMotion() {
        const eventActor = this._lockDialogGroup;
        this._motionId = eventActor.connect('captured-event', (_, event) => {
            if (event.type() === Clutter.EventType.MOTION)
                this._showPointer();

            return Clutter.EVENT_PROPAGATE;
        });
        this._hidePointer();
    }

    _hideLockScreen(animate) {
        if (this._lockScreenState === MessageTray.State.HIDDEN)
            return;

        this._lockScreenState = MessageTray.State.HIDING;

        this._lockDialogGroup.remove_all_transitions();

        // Animate the lock screen out of screen
        // if velocity is not specified (i.e. we come here from pressing ESC),
        // use the same speed regardless of original position
        // if velocity is specified, it's in pixels per milliseconds
        const height = global.stage.height;
        const delta = height + this._lockDialogGroup.translation_y;
        const velocity = height / CURTAIN_SLIDE_TIME;
        const duration = animate ? delta / velocity : 0;

        this._lockDialogGroup.ease({
            translation_y: -height,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._hideLockScreenComplete(),
        });

        this._showPointer();
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

            if (!this._dialog.open()) {
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
        this._dialog.grab_key_focus();
        return true;
    }

    _onUnlockFailed() {
        this._resetLockScreen({
            animateLockScreen: true,
            fadeToBlack: false,
        });
    }

    _resetLockScreen(params) {
        // Don't reset the lock screen unless it is completely hidden
        // This prevents the shield going down if the lock-delay timeout
        // fires while the user is dragging (which has the potential
        // to confuse our state)
        if (this._lockScreenState !== MessageTray.State.HIDDEN)
            return;

        this._lockScreenGroup.show();
        this._lockScreenState = MessageTray.State.SHOWING;

        let fadeToBlack = params.fadeToBlack;

        if (params.animateLockScreen) {
            this._lockDialogGroup.translation_y = -global.screen_height;
            this._lockDialogGroup.remove_all_transitions();
            this._lockDialogGroup.ease({
                translation_y: 0,
                duration: Overview.ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._lockScreenShown({fadeToBlack, animateFade: true});
                },
            });
        } else {
            this._lockDialogGroup.translation_y = 0;
            this._lockScreenShown({fadeToBlack, animateFade: false});
        }

        this._dialog.grab_key_focus();
    }

    _lockScreenShown(params) {
        this._hidePointerUntilMotion();

        this._lockScreenState = MessageTray.State.SHOWN;

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

    _wakeUpScreen() {
        if (!this.active)
            return; // already woken up, or not yet asleep
        this._onUserBecameActive();
        this.emit('wake-up-screen');
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
        this._hideLockScreen(animate);

        if (Main.sessionMode.currentMode === 'unlock-dialog')
            Main.sessionMode.popMode('unlock-dialog');

        this.emit('wake-up-screen');

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
            Main.popModal(this._grab);
            this._grab = null;
            this._isModal = false;
        }

        this._longLightbox.lightOff();
        this._shortLightbox.lightOff();

        this._lockDialogGroup.ease({
            translation_y: -global.screen_height,
            duration: Overview.ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._completeDeactivate(),
        });
    }

    _completeDeactivate() {
        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this.actor.hide();

        if (this._becameActiveId !== 0) {
            this.idleMonitor.remove_watch(this._becameActiveId);
            this._becameActiveId = 0;
        }

        if (this._lockTimeoutId !== 0) {
            GLib.source_remove(this._lockTimeoutId);
            this._lockTimeoutId = 0;
        }

        this._activationTime = 0;
        this._setActive(false);
        this._setLocked(false);
        global.set_runtime_state(LOCKED_STATE_STR, null);
    }

    activate(animate) {
        if (this._activationTime === 0)
            this._activationTime = GLib.get_monotonic_time();

        if (!this._ensureUnlockDialog(true))
            return;

        this.actor.show();

        if (Main.sessionMode.currentMode !== 'unlock-dialog') {
            this._isGreeter = Main.sessionMode.isGreeter;
            if (!this._isGreeter)
                Main.sessionMode.pushMode('unlock-dialog');
        }

        this._resetLockScreen({
            animateLockScreen: animate,
            fadeToBlack: true,
        });
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

    addCredentialManager(serviceName, credentialManager) {
        if (this._credentialManagers[serviceName])
            return;

        this._credentialManagers[serviceName] = credentialManager;
        credentialManager.connectObject('user-authenticated', () => {
            if (this._isLocked)
                this._activateDialog();
        }, this);
    }

    removeCredentialManager(serviceName) {
        let credentialManager = this._credentialManagers[serviceName];
        if (!credentialManager)
            return;

        credentialManager.disconnectObject(this);
        delete this._credentialManagers[serviceName];
    }

    lock(animate) {
        if (this._lockSettings.get_boolean(DISABLE_LOCK_KEY)) {
            log('Screen lock is locked down, not locking'); // lock, lock - who's there?
            return;
        }

        // Warn the user if we can't become modal
        if (!this._becomeModal()) {
            Main.notifyError(
                _('Unable to lock'),
                _('Lock was blocked by an app'));
            return;
        }

        // Clear the clipboard - otherwise, its contents may be leaked
        // to unauthorized parties by pasting into the unlock dialog's
        // password entry and unmasking the entry
        St.Clipboard.get_default().set_text(St.ClipboardType.CLIPBOARD, '');
        St.Clipboard.get_default().set_text(St.ClipboardType.PRIMARY, '');

        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(GLib.get_user_name());

        this.activate(animate);

        const lock = this._isGreeter
            ? true
            : user.password_mode !== AccountsService.UserPasswordMode.NONE;
        this._setLocked(lock);
    }

    // If the previous shell crashed, and gnome-session restarted us, then re-lock
    lockIfWasLocked() {
        if (!this._settings.get_boolean(LOCK_ENABLED_KEY))
            return;
        let wasLocked = global.get_runtime_state('b', LOCKED_STATE_STR);
        if (wasLocked === null)
            return;
        const laters = global.compositor.get_laters();
        laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
            this.lock(false);
            return GLib.SOURCE_REMOVE;
        });
    }
}
