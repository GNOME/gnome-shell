// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init, EndSessionDialog */
/*
 * Copyright 2010-2016 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

const { AccountsService, Clutter, Gio,
        GLib, GObject, Pango, Polkit, Shell, St }  = imports.gi;

const CheckBox = imports.ui.checkBox;
const Dialog = imports.ui.dialog;
const GnomeSession = imports.misc.gnomeSession;
const LoginManager = imports.misc.loginManager;
const ModalDialog = imports.ui.modalDialog;
const UserWidget = imports.ui.userWidget;

const { loadInterfaceXML } = imports.misc.fileUtils;

const _ITEM_ICON_SIZE = 64;

const EndSessionDialogIface = loadInterfaceXML('org.gnome.SessionManager.EndSessionDialog');

const logoutDialogContent = {
    subjectWithUser: C_("title", "Log Out %s"),
    subject: C_("title", "Log Out"),
    descriptionWithUser(user, seconds) {
        return ngettext("%s will be logged out automatically in %d second.",
                        "%s will be logged out automatically in %d seconds.",
                        seconds).format(user, seconds);
    },
    description(seconds) {
        return ngettext("You will be logged out automatically in %d second.",
                        "You will be logged out automatically in %d seconds.",
                        seconds).format(seconds);
    },
    showBatteryWarning: false,
    confirmButtons: [{ signal: 'ConfirmedLogout',
                       label: C_("button", "Log Out") }],
    showOtherSessions: false,
};

const shutdownDialogContent = {
    subject: C_("title", "Power Off"),
    subjectWithUpdates: C_("title", "Install Updates & Power Off"),
    description(seconds) {
        return ngettext("The system will power off automatically in %d second.",
                        "The system will power off automatically in %d seconds.",
                        seconds).format(seconds);
    },
    checkBoxText: C_("checkbox", "Install pending software updates"),
    showBatteryWarning: true,
    confirmButtons: [{ signal: 'ConfirmedReboot',
                       label: C_("button", "Restart") },
                     { signal: 'ConfirmedShutdown',
                       label: C_("button", "Power Off") }],
    iconName: 'system-shutdown-symbolic',
    showOtherSessions: true,
};

const restartDialogContent = {
    subject: C_("title", "Restart"),
    description(seconds) {
        return ngettext("The system will restart automatically in %d second.",
                        "The system will restart automatically in %d seconds.",
                        seconds).format(seconds);
    },
    showBatteryWarning: false,
    confirmButtons: [{ signal: 'ConfirmedReboot',
                       label: C_("button", "Restart") }],
    iconName: 'view-refresh-symbolic',
    showOtherSessions: true,
};

const restartUpdateDialogContent = {

    subject: C_("title", "Restart & Install Updates"),
    description(seconds) {
        return ngettext("The system will automatically restart and install updates in %d second.",
                        "The system will automatically restart and install updates in %d seconds.",
                        seconds).format(seconds);
    },
    showBatteryWarning: true,
    confirmButtons: [{ signal: 'ConfirmedReboot',
                       label: C_("button", "Restart &amp; Install") }],
    unusedFutureButtonForTranslation: C_("button", "Install &amp; Power Off"),
    unusedFutureCheckBoxForTranslation: C_("checkbox", "Power off after updates are installed"),
    iconName: 'view-refresh-symbolic',
    showOtherSessions: true,
};

const restartUpgradeDialogContent = {

    subject: C_("title", "Restart & Install Upgrade"),
    upgradeDescription(distroName, distroVersion) {
        /* Translators: This is the text displayed for system upgrades in the
           shut down dialog. First %s gets replaced with the distro name and
           second %s with the distro version to upgrade to */
        return _("%s %s will be installed after restart. Upgrade installation can take a long time: ensure that you have backed up and that the computer is plugged in.").format(distroName, distroVersion);
    },
    disableTimer: true,
    showBatteryWarning: false,
    confirmButtons: [{ signal: 'ConfirmedReboot',
                       label: C_("button", "Restart &amp; Install") }],
    iconName: 'view-refresh-symbolic',
    showOtherSessions: true,
};

const DialogType = {
    LOGOUT: 0 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_LOGOUT */,
    SHUTDOWN: 1 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_SHUTDOWN */,
    RESTART: 2 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_RESTART */,
    UPDATE_RESTART: 3,
    UPGRADE_RESTART: 4,
};

const DialogContent = {
    0 /* DialogType.LOGOUT */: logoutDialogContent,
    1 /* DialogType.SHUTDOWN */: shutdownDialogContent,
    2 /* DialogType.RESTART */: restartDialogContent,
    3 /* DialogType.UPDATE_RESTART */: restartUpdateDialogContent,
    4 /* DialogType.UPGRADE_RESTART */: restartUpgradeDialogContent,
};

var MAX_USERS_IN_SESSION_DIALOG = 5;

const LogindSessionIface = loadInterfaceXML('org.freedesktop.login1.Session');
const LogindSession = Gio.DBusProxy.makeProxyWrapper(LogindSessionIface);

const PkOfflineIface = loadInterfaceXML('org.freedesktop.PackageKit.Offline');
const PkOfflineProxy = Gio.DBusProxy.makeProxyWrapper(PkOfflineIface);

const UPowerIface = loadInterfaceXML('org.freedesktop.UPower');
const UPowerProxy = Gio.DBusProxy.makeProxyWrapper(UPowerIface);

function findAppFromInhibitor(inhibitor) {
    let desktopFile;
    try {
        [desktopFile] = inhibitor.GetAppIdSync();
    } catch (e) {
        // XXX -- sometimes JIT inhibitors generated by gnome-session
        // get removed too soon. Don't fail in this case.
        log('gnome-session gave us a dead inhibitor: %s'.format(inhibitor.get_object_path()));
        return null;
    }

    if (!GLib.str_has_suffix(desktopFile, '.desktop'))
        desktopFile += '.desktop';

    return Shell.AppSystem.get_default().lookup_heuristic_basename(desktopFile);
}

// The logout timer only shows updates every 10 seconds
// until the last 10 seconds, then it shows updates every
// second.  This function takes a given time and returns
// what we should show to the user for that time.
function _roundSecondsToInterval(totalSeconds, secondsLeft, interval) {
    let time;

    time = Math.ceil(secondsLeft);

    // Final count down is in decrements of 1
    if (time <= interval)
        return time;

    // Round up higher than last displayable time interval
    time += interval - 1;

    // Then round down to that time interval
    if (time > totalSeconds)
        time = Math.ceil(totalSeconds);
    else
        time -= time % interval;

    return time;
}

function _setCheckBoxLabel(checkBox, text) {
    let label = checkBox.getLabelActor();

    if (text) {
        label.set_text(text);
        checkBox.show();
    } else {
        label.set_text('');
        checkBox.hide();
    }
}

function init() {
    // This always returns the same singleton object
    // By instantiating it initially, we register the
    // bus object, etc.
    new EndSessionDialog();
}

var EndSessionDialog = GObject.registerClass(
class EndSessionDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({ styleClass: 'end-session-dialog',
                      destroyOnClose: false });

        this._loginManager = LoginManager.getLoginManager();
        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());
        this._updatesPermission = null;

        this._pkOfflineProxy = new PkOfflineProxy(Gio.DBus.system,
                                                  'org.freedesktop.PackageKit',
                                                  '/org/freedesktop/PackageKit',
                                                  this._onPkOfflineProxyCreated.bind(this));

        this._powerProxy = new UPowerProxy(Gio.DBus.system,
                                           'org.freedesktop.UPower',
                                           '/org/freedesktop/UPower',
                                           (proxy, error) => {
                                               if (error) {
                                                   log(error.message);
                                                   return;
                                               }
                                               this._powerProxy.connect('g-properties-changed',
                                                                        this._sync.bind(this));
                                               this._sync();
                                           });

        this._secondsLeft = 0;
        this._totalSecondsToStayOpen = 0;
        this._applications = [];
        this._sessions = [];

        this.connect('destroy',
                     this._onDestroy.bind(this));
        this.connect('opened',
                     this._onOpened.bind(this));

        this._userLoadedId = this._user.connect('notify::is-loaded', this._sync.bind(this));
        this._userChangedId = this._user.connect('changed', this._sync.bind(this));

        this._messageDialogContent = new Dialog.MessageDialogContent();

        this._checkBox = new CheckBox.CheckBox();
        this._checkBox.connect('clicked', this._sync.bind(this));
        this._messageDialogContent.add_child(this._checkBox);

        this._batteryWarning = new St.Label({
            style_class: 'end-session-dialog-battery-warning',
            text: _('Running on battery power: Please plug in before installing updates.'),
        });
        this._batteryWarning.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._batteryWarning.clutter_text.line_wrap = true;
        this._messageDialogContent.add_child(this._batteryWarning);

        this.contentLayout.add_child(this._messageDialogContent);

        this._applicationSection = new Dialog.ListSection({
            title: _('Some applications are busy or have unsaved work'),
        });
        this.contentLayout.add_child(this._applicationSection);

        this._sessionSection = new Dialog.ListSection({
            title: _('Other users are logged in'),
        });
        this.contentLayout.add_child(this._sessionSection);

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(EndSessionDialogIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/SessionManager/EndSessionDialog');
    }

    _onPkOfflineProxyCreated(proxy, error) {
        if (error) {
            log(error.message);
            return;
        }

        // Creating a D-Bus proxy won't propagate SERVICE_UNKNOWN or NAME_HAS_NO_OWNER
        // errors if PackageKit is not available, but the GIO implementation will make
        // sure in that case that the proxy's g-name-owner is set to null, so check that.
        if (this._pkOfflineProxy.g_name_owner === null) {
            this._pkOfflineProxy = null;
            return;
        }

        // It only makes sense to check for this permission if PackageKit is available.
        Polkit.Permission.new(
            'org.freedesktop.packagekit.trigger-offline-update', null, null,
            (source, res) => {
                try {
                    this._updatesPermission = Polkit.Permission.new_finish(res);
                } catch (e) {
                    log('No permission to trigger offline updates: %s'.format(e.toString()));
                }
            });
    }

    _onDestroy() {
        this._user.disconnect(this._userLoadedId);
        this._user.disconnect(this._userChangedId);
    }

    _sync() {
        let open = this.state == ModalDialog.State.OPENING || this.state == ModalDialog.State.OPENED;
        if (!open)
            return;

        let dialogContent = DialogContent[this._type];

        let subject = dialogContent.subject;

        // Use different title when we are installing updates
        if (dialogContent.subjectWithUpdates && this._checkBox.checked)
            subject = dialogContent.subjectWithUpdates;

        if (dialogContent.showBatteryWarning) {
            this._batteryWarning.visible =
                this._powerProxy.OnBattery && this._checkBox.checked;
        }

        let description;
        let displayTime = _roundSecondsToInterval(this._totalSecondsToStayOpen,
                                                  this._secondsLeft,
                                                  10);

        if (this._user.is_loaded) {
            let realName = this._user.get_real_name();

            if (realName != null) {
                if (dialogContent.subjectWithUser)
                    subject = dialogContent.subjectWithUser.format(realName);

                if (dialogContent.descriptionWithUser)
                    description = dialogContent.descriptionWithUser(realName, displayTime);
            }
        }

        // Use a different description when we are installing a system upgrade
        // if the PackageKit proxy is available (i.e. PackageKit is available).
        if (dialogContent.upgradeDescription) {
            const { name, version } = this._updateInfo.PreparedUpgrade;
            if (name != null && version != null)
                description = dialogContent.upgradeDescription(name, version);
        }

        // Fall back to regular description
        if (!description)
            description = dialogContent.description(displayTime);

        this._messageDialogContent.title = subject;
        this._messageDialogContent.description = description;

        let hasApplications = this._applications.length > 0;
        let hasSessions = this._sessions.length > 0;

        this._applicationSection.visible = hasApplications;
        this._sessionSection.visible = hasSessions;
    }

    _updateButtons() {
        let dialogContent = DialogContent[this._type];
        let buttons = [{ action: this.cancel.bind(this),
                         label: _("Cancel"),
                         key: Clutter.KEY_Escape }];

        for (let i = 0; i < dialogContent.confirmButtons.length; i++) {
            let signal = dialogContent.confirmButtons[i].signal;
            let label = dialogContent.confirmButtons[i].label;
            buttons.push({
                action: () => {
                    this.close(true);
                    let signalId = this.connect('closed', () => {
                        this.disconnect(signalId);
                        this._confirm(signal);
                    });
                },
                label,
            });
        }

        this.setButtons(buttons);
    }

    close(skipSignal) {
        super.close();

        if (!skipSignal)
            this._dbusImpl.emit_signal('Closed', null);
    }

    cancel() {
        this._stopTimer();
        this._dbusImpl.emit_signal('Canceled', null);
        this.close();
    }

    _confirm(signal) {
        let callback = () => {
            this._fadeOutDialog();
            this._stopTimer();
            this._dbusImpl.emit_signal(signal, null);
        };

        // Offline update not available; just emit the signal
        if (!this._checkBox.visible) {
            callback();
            return;
        }

        // Trigger the offline update as requested
        if (this._checkBox.checked) {
            switch (signal) {
            case "ConfirmedReboot":
                this._triggerOfflineUpdateReboot(callback);
                break;
            case "ConfirmedShutdown":
                // To actually trigger the offline update, we need to
                // reboot to do the upgrade. When the upgrade is complete,
                // the computer will shut down automatically.
                signal = "ConfirmedReboot";
                this._triggerOfflineUpdateShutdown(callback);
                break;
            default:
                callback();
                break;
            }
        } else {
            this._triggerOfflineUpdateCancel(callback);
        }
    }

    _onOpened() {
        this._sync();
    }

    _triggerOfflineUpdateReboot(callback) {
        // Handle this gracefully if PackageKit is not available.
        if (!this._pkOfflineProxy) {
            callback();
            return;
        }

        this._pkOfflineProxy.TriggerRemote('reboot', (result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    }

    _triggerOfflineUpdateShutdown(callback) {
        // Handle this gracefully if PackageKit is not available.
        if (!this._pkOfflineProxy) {
            callback();
            return;
        }

        this._pkOfflineProxy.TriggerRemote('power-off', (result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    }

    _triggerOfflineUpdateCancel(callback) {
        // Handle this gracefully if PackageKit is not available.
        if (!this._pkOfflineProxy) {
            callback();
            return;
        }

        this._pkOfflineProxy.CancelRemote((result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    }

    _startTimer() {
        let startTime = GLib.get_monotonic_time();
        this._secondsLeft = this._totalSecondsToStayOpen;

        this._timerId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 1, () => {
            let currentTime = GLib.get_monotonic_time();
            let secondsElapsed = (currentTime - startTime) / 1000000;

            this._secondsLeft = this._totalSecondsToStayOpen - secondsElapsed;
            if (this._secondsLeft > 0) {
                this._sync();
                return GLib.SOURCE_CONTINUE;
            }

            let dialogContent = DialogContent[this._type];
            let button = dialogContent.confirmButtons[dialogContent.confirmButtons.length - 1];
            this._confirm(button.signal);
            this._timerId = 0;

            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(this._timerId, '[gnome-shell] this._confirm');
    }

    _stopTimer() {
        if (this._timerId > 0) {
            GLib.source_remove(this._timerId);
            this._timerId = 0;
        }

        this._secondsLeft = 0;
    }

    _onInhibitorLoaded(inhibitor) {
        if (!this._applications.includes(inhibitor)) {
            // Stale inhibitor
            return;
        }

        let app = findAppFromInhibitor(inhibitor);

        if (app) {
            let [description] = inhibitor.GetReasonSync();
            let listItem = new Dialog.ListSectionItem({
                icon_actor: app.create_icon_texture(_ITEM_ICON_SIZE),
                title: app.get_name(),
                description,
            });
            this._applicationSection.list.add_child(listItem);
        } else {
            // inhibiting app is a service, not an application
            this._applications.splice(this._applications.indexOf(inhibitor), 1);
        }

        this._sync();
    }

    _loadSessions() {
        this._loginManager.listSessions(result => {
            let n = 0;
            for (let i = 0; i < result.length; i++) {
                let [id_, uid_, userName, seat_, sessionPath] = result[i];
                let proxy = new LogindSession(Gio.DBus.system, 'org.freedesktop.login1', sessionPath);

                if (proxy.Class != 'user')
                    continue;

                if (proxy.State == 'closing')
                    continue;

                let sessionId = GLib.getenv('XDG_SESSION_ID');
                if (!sessionId) {
                    this._loginManager.getCurrentSessionProxy(currentSessionProxy => {
                        sessionId = currentSessionProxy.Id;
                        log('endSessionDialog: No XDG_SESSION_ID, fetched from logind: %d'.format(sessionId));
                    });
                }

                if (proxy.Id == sessionId)
                    continue;

                let session = { user: this._userManager.get_user(userName),
                                username: userName,
                                type: proxy.Type,
                                remote: proxy.Remote };
                this._sessions.push(session);

                let userAvatar = new UserWidget.Avatar(session.user, { iconSize: _ITEM_ICON_SIZE });
                userAvatar.update();

                userName = session.user.get_real_name()
                    ? session.user.get_real_name() : session.username;

                let userLabelText;
                if (session.remote)
                    /* Translators: Remote here refers to a remote session, like a ssh login */
                    userLabelText = _('%s (remote)').format(userName);
                else if (session.type === 'tty')
                    /* Translators: Console here refers to a tty like a VT console */
                    userLabelText = _('%s (console)').format(userName);
                else
                    userLabelText = userName;

                let listItem = new Dialog.ListSectionItem({
                    icon_actor: userAvatar,
                    title: userLabelText,
                });
                this._sessionSection.list.add_child(listItem);

                // limit the number of entries
                n++;
                if (n == MAX_USERS_IN_SESSION_DIALOG)
                    break;
            }

            this._sync();
        });
    }

    async _getUpdateInfo() {
        const connection = this._pkOfflineProxy.get_connection();
        const reply = await connection.call(
            this._pkOfflineProxy.g_name,
            this._pkOfflineProxy.g_object_path,
            'org.freedesktop.DBus.Properties',
            'GetAll',
            new GLib.Variant('(s)', [this._pkOfflineProxy.g_interface_name]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null);
        const [info] = reply.recursiveUnpack();
        return info;
    }

    async OpenAsync(parameters, invocation) {
        let [type, timestamp, totalSecondsToStayOpen, inhibitorObjectPaths] = parameters;
        this._totalSecondsToStayOpen = totalSecondsToStayOpen;
        this._type = type;

        try {
            this._updateInfo = await this._getUpdateInfo();
        } catch (e) {
            if (this._pkOfflineProxy !== null)
                log('Failed to get update info from PackageKit: %s'.format(e.message));

            this._updateInfo = {
                UpdateTriggered: false,
                UpdatePrepared: false,
                UpgradeTriggered: false,
                PreparedUpgrade: {},
            };
        }

        // Only consider updates and upgrades if PackageKit is available.
        if (this._pkOfflineProxy && this._type == DialogType.RESTART) {
            if (this._updateInfo.UpdateTriggered)
                this._type = DialogType.UPDATE_RESTART;
            else if (this._updateInfo.UpgradeTriggered)
                this._type = DialogType.UPGRADE_RESTART;
        }

        this._applications = [];
        this._applicationSection.list.destroy_all_children();

        this._sessions = [];
        this._sessionSection.list.destroy_all_children();

        if (!(this._type in DialogContent)) {
            invocation.return_dbus_error('org.gnome.Shell.ModalDialog.TypeError',
                                         "Unknown dialog type requested");
            return;
        }

        let dialogContent = DialogContent[this._type];

        for (let i = 0; i < inhibitorObjectPaths.length; i++) {
            let inhibitor = new GnomeSession.Inhibitor(inhibitorObjectPaths[i], proxy => {
                this._onInhibitorLoaded(proxy);
            });

            this._applications.push(inhibitor);
        }

        if (dialogContent.showOtherSessions)
            this._loadSessions();

        let updateTriggered = this._updateInfo.UpdateTriggered;
        let updatePrepared = this._updateInfo.UpdatePrepared;
        let updatesAllowed = this._updatesPermission && this._updatesPermission.allowed;

        _setCheckBoxLabel(this._checkBox, dialogContent.checkBoxText || '');
        this._checkBox.visible = dialogContent.checkBoxText && updatePrepared && updatesAllowed;
        this._checkBox.checked = this._checkBox.visible;

        // We show the warning either together with the checkbox, or when
        // updates have already been triggered, but the user doesn't have
        // enough permissions to cancel them.
        this._batteryWarning.visible = dialogContent.showBatteryWarning &&
                                        (this._checkBox.visible || updatePrepared && updateTriggered && !updatesAllowed);

        this._updateButtons();

        if (!this.open(timestamp)) {
            invocation.return_dbus_error('org.gnome.Shell.ModalDialog.GrabError',
                                         "Cannot grab pointer and keyboard");
            return;
        }

        if (!dialogContent.disableTimer)
            this._startTimer();

        this._sync();

        let signalId = this.connect('opened', () => {
            invocation.return_value(null);
            this.disconnect(signalId);
        });
    }

    Close(_parameters, _invocation) {
        this.close();
    }
});
