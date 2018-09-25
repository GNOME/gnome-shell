// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
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

const Lang = imports.lang;
const Mainloop = imports.mainloop;

const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Pango = imports.gi.Pango;
const Polkit = imports.gi.Polkit;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const CheckBox = imports.ui.checkBox;
const GnomeSession = imports.misc.gnomeSession;
const LoginManager = imports.misc.loginManager;
const ModalDialog = imports.ui.modalDialog;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

let _endSessionDialog = null;

const _ITEM_ICON_SIZE = 48;
const _DIALOG_ICON_SIZE = 48;

var GSM_SESSION_MANAGER_LOGOUT_FORCE = 2;

const EndSessionDialogIface = '<node> \
<interface name="org.gnome.SessionManager.EndSessionDialog"> \
<method name="Open"> \
    <arg type="u" direction="in" /> \
    <arg type="u" direction="in" /> \
    <arg type="u" direction="in" /> \
    <arg type="ao" direction="in" /> \
</method> \
<method name="Close" /> \
<signal name="ConfirmedLogout" /> \
<signal name="ConfirmedReboot" /> \
<signal name="ConfirmedShutdown" /> \
<signal name="Canceled" /> \
<signal name="Closed" /> \
</interface> \
</node>';

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
                       label:  C_("button", "Log Out") }],
    iconStyleClass: 'end-session-dialog-logout-icon',
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
                       label:  C_("button", "Restart") },
                     { signal: 'ConfirmedShutdown',
                       label:  C_("button", "Power Off") }],
    iconName: 'system-shutdown-symbolic',
    iconStyleClass: 'end-session-dialog-shutdown-icon',
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
                       label:  C_("button", "Restart") }],
    iconName: 'view-refresh-symbolic',
    iconStyleClass: 'end-session-dialog-shutdown-icon',
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
                       label:  C_("button", "Restart &amp; Install") }],
    unusedFutureButtonForTranslation: C_("button", "Install &amp; Power Off"),
    unusedFutureCheckBoxForTranslation: C_("checkbox", "Power off after updates are installed"),
    iconName: 'view-refresh-symbolic',
    iconStyleClass: 'end-session-dialog-shutdown-icon',
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
                       label:  C_("button", "Restart &amp; Install") }],
    iconName: 'view-refresh-symbolic',
    iconStyleClass: 'end-session-dialog-shutdown-icon',
    showOtherSessions: true,
};

const DialogType = {
  LOGOUT: 0 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_LOGOUT */,
  SHUTDOWN: 1 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_SHUTDOWN */,
  RESTART: 2 /* GSM_SHELL_END_SESSION_DIALOG_TYPE_RESTART */,
  UPDATE_RESTART: 3,
  UPGRADE_RESTART: 4
};

const DialogContent = {
    0 /* DialogType.LOGOUT */: logoutDialogContent,
    1 /* DialogType.SHUTDOWN */: shutdownDialogContent,
    2 /* DialogType.RESTART */: restartDialogContent,
    3 /* DialogType.UPDATE_RESTART */: restartUpdateDialogContent,
    4 /* DialogType.UPGRADE_RESTART */: restartUpgradeDialogContent
};

var MAX_USERS_IN_SESSION_DIALOG = 5;

const LogindSessionIface = '<node> \
<interface name="org.freedesktop.login1.Session"> \
    <property name="Id" type="s" access="read"/> \
    <property name="Remote" type="b" access="read"/> \
    <property name="Class" type="s" access="read"/> \
    <property name="Type" type="s" access="read"/> \
    <property name="State" type="s" access="read"/> \
</interface> \
</node>';

const LogindSession = Gio.DBusProxy.makeProxyWrapper(LogindSessionIface);

const PkOfflineIface = '<node> \
<interface name="org.freedesktop.PackageKit.Offline"> \
    <property name="UpdatePrepared" type="b" access="read"/> \
    <property name="UpdateTriggered" type="b" access="read"/> \
    <property name="UpgradePrepared" type="b" access="read"/> \
    <property name="UpgradeTriggered" type="b" access="read"/> \
    <property name="PreparedUpgrade" type="a{sv}" access="read"/> \
    <method name="Trigger"> \
        <arg type="s" name="action" direction="in"/> \
    </method> \
    <method name="Cancel"/> \
</interface> \
</node>';

const PkOfflineProxy = Gio.DBusProxy.makeProxyWrapper(PkOfflineIface);

const UPowerIface = '<node> \
<interface name="org.freedesktop.UPower"> \
    <property name="OnBattery" type="b" access="read"/> \
</interface> \
</node>';

const UPowerProxy = Gio.DBusProxy.makeProxyWrapper(UPowerIface);

function findAppFromInhibitor(inhibitor) {
    let desktopFile;
    try {
        [desktopFile] = inhibitor.GetAppIdSync();
    } catch(e) {
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

function _setLabelText(label, text) {
    if (text) {
        label.set_text(text);
        label.show();
    } else {
        label.set_text('');
        label.hide();
    }
}

function _setCheckBoxLabel(checkBox, text) {
    let label = checkBox.getLabelActor();

    if (text) {
        label.set_text(text);
        checkBox.actor.show();
    } else {
        label.set_text('');
        checkBox.actor.hide();
    }
}

function init() {
    // This always returns the same singleton object
    // By instantiating it initially, we register the
    // bus object, etc.
    _endSessionDialog = new EndSessionDialog();
}

var EndSessionDialog = new Lang.Class({
    Name: 'EndSessionDialog',
    Extends: ModalDialog.ModalDialog,

    _init() {
        this.parent({ styleClass: 'end-session-dialog',
                      destroyOnClose: false });

        this._loginManager = LoginManager.getLoginManager();
        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());

        this._pkOfflineProxy = new PkOfflineProxy(Gio.DBus.system,
                                                  'org.freedesktop.PackageKit',
                                                  '/org/freedesktop/PackageKit',
                                                  (proxy, error) => {
                                                      if (error)
                                                          log(error.message);
                                                  });
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

        this._userLoadedId = this._user.connect('notify::is_loaded', this._sync.bind(this));
        this._userChangedId = this._user.connect('changed', this._sync.bind(this));

        let mainContentLayout = new St.BoxLayout({ vertical: false });
        this.contentLayout.add(mainContentLayout,
                               { x_fill: true,
                                 y_fill: false });

        this._iconBin = new St.Bin();
        mainContentLayout.add(this._iconBin,
                              { x_fill:  true,
                                y_fill:  false,
                                x_align: St.Align.END,
                                y_align: St.Align.START });

        let messageLayout = new St.BoxLayout({ vertical: true,
                                               style_class: 'end-session-dialog-layout' });
        mainContentLayout.add(messageLayout,
                              { y_align: St.Align.START });

        this._subjectLabel = new St.Label({ style_class: 'end-session-dialog-subject' });

        messageLayout.add(this._subjectLabel,
                          { x_fill: false,
                            y_fill:  false,
                            x_align: St.Align.START,
                            y_align: St.Align.START });

        this._descriptionLabel = new St.Label({ style_class: 'end-session-dialog-description' });
        this._descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._descriptionLabel.clutter_text.line_wrap = true;

        messageLayout.add(this._descriptionLabel,
                          { y_fill:  true,
                            y_align: St.Align.START });

        this._checkBox = new CheckBox.CheckBox();
        this._checkBox.actor.connect('clicked', this._sync.bind(this));
        messageLayout.add(this._checkBox.actor);

        this._batteryWarning = new St.Label({ style_class: 'end-session-dialog-warning',
                                              text: _("Running on battery power: please plug in before installing updates.") });
        this._batteryWarning.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._batteryWarning.clutter_text.line_wrap = true;
        messageLayout.add(this._batteryWarning);

        this._scrollView = new St.ScrollView({ style_class: 'end-session-dialog-list' });
        this._scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        this.contentLayout.add(this._scrollView,
                               { x_fill: true,
                                 y_fill: true });
        this._scrollView.hide();

        this._inhibitorSection = new St.BoxLayout({ vertical: true,
                                                    style_class: 'end-session-dialog-inhibitor-layout' });
        this._scrollView.add_actor(this._inhibitorSection);

        this._applicationHeader = new St.Label({ style_class: 'end-session-dialog-list-header',
                                                 text: _("Some applications are busy or have unsaved work.") });
        this._applicationList = new St.BoxLayout({ style_class: 'end-session-dialog-app-list',
                                                   vertical: true });
        this._inhibitorSection.add_actor(this._applicationHeader);
        this._inhibitorSection.add_actor(this._applicationList);

        this._sessionHeader = new St.Label({ style_class: 'end-session-dialog-list-header',
                                             text: _("Other users are logged in.") });
        this._sessionList = new St.BoxLayout({ style_class: 'end-session-dialog-session-list',
                                               vertical: true });
        this._inhibitorSection.add_actor(this._sessionHeader);
        this._inhibitorSection.add_actor(this._sessionList);

        try {
            this._updatesPermission = Polkit.Permission.new_sync("org.freedesktop.packagekit.trigger-offline-update", null, null);
        } catch(e) {
            log('No permission to trigger offline updates: %s'.format(e.toString()));
        }

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(EndSessionDialogIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/SessionManager/EndSessionDialog');
    },

    _onDestroy() {
        this._user.disconnect(this._userLoadedId);
        this._user.disconnect(this._userChangedId);
    },

    _sync() {
        let open = (this.state == ModalDialog.State.OPENING || this.state == ModalDialog.State.OPENED);
        if (!open)
            return;

        let dialogContent = DialogContent[this._type];

        let subject = dialogContent.subject;

        // Use different title when we are installing updates
        if (dialogContent.subjectWithUpdates && this._checkBox.actor.checked)
            subject = dialogContent.subjectWithUpdates;

        if (dialogContent.showBatteryWarning) {
            // Warn when running on battery power
            if (this._powerProxy.OnBattery && this._checkBox.actor.checked)
                this._batteryWarning.opacity = 255;
            else
                this._batteryWarning.opacity = 0;
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
        if (dialogContent.upgradeDescription) {
            let name = this._pkOfflineProxy.PreparedUpgrade['name'].deep_unpack();
            let version = this._pkOfflineProxy.PreparedUpgrade['version'].deep_unpack();

            if (name != null && version != null)
                description = dialogContent.upgradeDescription(name, version);
        }

        // Fall back to regular description
        if (!description)
            description = dialogContent.description(displayTime);

        _setLabelText(this._descriptionLabel, description);
        _setLabelText(this._subjectLabel, subject);

        if (dialogContent.iconName) {
            this._iconBin.child = new St.Icon({ icon_name: dialogContent.iconName,
                                                icon_size: _DIALOG_ICON_SIZE,
                                                style_class: dialogContent.iconStyleClass });
        } else {
            let avatarWidget = new UserWidget.Avatar(this._user,
                                                     { iconSize: _DIALOG_ICON_SIZE,
                                                       styleClass: dialogContent.iconStyleClass });
            this._iconBin.child = avatarWidget.actor;
            avatarWidget.update();
        }

        let hasApplications = this._applications.length > 0;
        let hasSessions = this._sessions.length > 0;
        this._scrollView.visible = hasApplications || hasSessions;
        this._applicationHeader.visible = hasApplications;
        this._sessionHeader.visible = hasSessions;
    },

    _updateButtons() {
        let dialogContent = DialogContent[this._type];
        let buttons = [{ action: this.cancel.bind(this),
                         label:  _("Cancel"),
                         key:    Clutter.Escape }];

        for (let i = 0; i < dialogContent.confirmButtons.length; i++) {
            let signal = dialogContent.confirmButtons[i].signal;
            let label = dialogContent.confirmButtons[i].label;
            buttons.push({ action: () => {
                               this.close(true);
                               let signalId = this.connect('closed', () => {
                                   this.disconnect(signalId);
                                   this._confirm(signal);
                               });
                           },
                           label: label });
        }

        this.setButtons(buttons);
    },

    close(skipSignal) {
        this.parent();

        if (!skipSignal)
            this._dbusImpl.emit_signal('Closed', null);
    },

    cancel() {
        this._stopTimer();
        this._dbusImpl.emit_signal('Canceled', null);
        this.close();
    },

    _confirm(signal) {
        let callback = () => {
            this._fadeOutDialog();
            this._stopTimer();
            this._dbusImpl.emit_signal(signal, null);
        };

        // Offline update not available; just emit the signal
        if (!this._checkBox.actor.visible) {
            callback();
            return;
        }

        // Trigger the offline update as requested
        if (this._checkBox.actor.checked) {
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
    },

    _onOpened() {
        this._sync();
    },

    _triggerOfflineUpdateReboot(callback) {
        this._pkOfflineProxy.TriggerRemote('reboot', (result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    },

    _triggerOfflineUpdateShutdown(callback) {
        this._pkOfflineProxy.TriggerRemote('power-off', (result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    },

    _triggerOfflineUpdateCancel(callback) {
        this._pkOfflineProxy.CancelRemote((result, error) => {
            if (error)
                log(error.message);

            callback();
        });
    },

    _startTimer() {
        let startTime = GLib.get_monotonic_time();
        this._secondsLeft = this._totalSecondsToStayOpen;

        this._timerId = Mainloop.timeout_add_seconds(1, () => {
            let currentTime = GLib.get_monotonic_time();
            let secondsElapsed = ((currentTime - startTime) / 1000000);

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
    },

    _stopTimer() {
        if (this._timerId > 0) {
            Mainloop.source_remove(this._timerId);
            this._timerId = 0;
        }

        this._secondsLeft = 0;
    },

    _constructListItemForApp(inhibitor, app) {
        let actor = new St.BoxLayout({ style_class: 'end-session-dialog-app-list-item',
                                       can_focus: true });
        actor.add(app.create_icon_texture(_ITEM_ICON_SIZE));

        let textLayout = new St.BoxLayout({ vertical: true,
                                            y_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER });
        actor.add(textLayout);

        let nameLabel = new St.Label({ text: app.get_name(),
                                       style_class: 'end-session-dialog-app-list-item-name' });
        textLayout.add(nameLabel);
        actor.label_actor = nameLabel;

        let [reason] = inhibitor.GetReasonSync();
        if (reason) {
            let reasonLabel = new St.Label({ text: reason,
                                             style_class: 'end-session-dialog-app-list-item-description' });
            textLayout.add(reasonLabel);
        }

        return actor;
    },

    _onInhibitorLoaded(inhibitor) {
        if (this._applications.indexOf(inhibitor) < 0) {
            // Stale inhibitor
            return;
        }

        let app = findAppFromInhibitor(inhibitor);

        if (app) {
            let actor = this._constructListItemForApp(inhibitor, app);
            this._applicationList.add(actor);
        } else {
            // inhibiting app is a service, not an application
            this._applications.splice(this._applications.indexOf(inhibitor), 1);
        }

        this._sync();
    },

    _constructListItemForSession(session) {
        let avatar = new UserWidget.Avatar(session.user, { iconSize: _ITEM_ICON_SIZE });
        avatar.update();

        let userName = session.user.get_real_name() ? session.user.get_real_name() : session.username;
        let userLabelText;

        if (session.remote)
            /* Translators: Remote here refers to a remote session, like a ssh login */
            userLabelText = _("%s (remote)").format(userName);
        else if (session.type == "tty")
            /* Translators: Console here refers to a tty like a VT console */
            userLabelText = _("%s (console)").format(userName);
        else
            userLabelText = userName;

        let actor = new St.BoxLayout({ style_class: 'end-session-dialog-session-list-item',
                                       can_focus: true });
        actor.add(avatar.actor);

        let nameLabel = new St.Label({ text: userLabelText,
                                       style_class: 'end-session-dialog-session-list-item-name',
                                       y_expand: true,
                                       y_align: Clutter.ActorAlign.CENTER });
        actor.add(nameLabel);
        actor.label_actor = nameLabel;

        return actor;
    },

    _loadSessions() {
        this._loginManager.listSessions(result => {
            let n = 0;
            for (let i = 0; i < result.length; i++) {
                let[id, uid, userName, seat, sessionPath] = result[i];
                let proxy = new LogindSession(Gio.DBus.system, 'org.freedesktop.login1', sessionPath);

                if (proxy.Class != 'user')
                    continue;

                if (proxy.State == 'closing')
                    continue;

                if (proxy.Id == GLib.getenv('XDG_SESSION_ID'))
                    continue;

                let session = { user: this._userManager.get_user(userName),
                                username: userName,
                                type: proxy.Type,
                                remote: proxy.Remote };
                this._sessions.push(session);

                let actor = this._constructListItemForSession(session);
                this._sessionList.add(actor);

                // limit the number of entries
                n++;
                if (n == MAX_USERS_IN_SESSION_DIALOG)
                    break;
            }

            this._sync();
        });
    },

    OpenAsync(parameters, invocation) {
        let [type, timestamp, totalSecondsToStayOpen, inhibitorObjectPaths] = parameters;
        this._totalSecondsToStayOpen = totalSecondsToStayOpen;
        this._type = type;

        if (this._type == DialogType.RESTART) {
            if (this._pkOfflineProxy.UpdateTriggered)
                this._type = DialogType.UPDATE_RESTART;
            else if (this._pkOfflineProxy.UpgradeTriggered)
                this._type = DialogType.UPGRADE_RESTART;
        }

        this._applications = [];
        this._applicationList.destroy_all_children();

        this._sessions = [];
        this._sessionList.destroy_all_children();

        if (!(this._type in DialogContent)) {
            invocation.return_dbus_error('org.gnome.Shell.ModalDialog.TypeError',
                                         "Unknown dialog type requested");
            return;
        }

        let dialogContent = DialogContent[this._type];

        for (let i = 0; i < inhibitorObjectPaths.length; i++) {
            let inhibitor = new GnomeSession.Inhibitor(inhibitorObjectPaths[i], (proxy, error) => {
                this._onInhibitorLoaded(proxy);
            });

            this._applications.push(inhibitor);
        }

        if (dialogContent.showOtherSessions)
            this._loadSessions();

        let updateTriggered = this._pkOfflineProxy.UpdateTriggered;
        let updatePrepared = this._pkOfflineProxy.UpdatePrepared;
        let updatesAllowed = this._updatesPermission && this._updatesPermission.allowed;

        _setCheckBoxLabel(this._checkBox, dialogContent.checkBoxText || '');
        this._checkBox.actor.visible = (dialogContent.checkBoxText && updatePrepared && updatesAllowed);
        this._checkBox.actor.checked = (updatePrepared && updateTriggered);

        // We show the warning either together with the checkbox, or when
        // updates have already been triggered, but the user doesn't have
        // enough permissions to cancel them.
        this._batteryWarning.visible = (dialogContent.showBatteryWarning &&
                                        (this._checkBox.actor.visible || updatePrepared && updateTriggered && !updatesAllowed));

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
    },

    Close(parameters, invocation) {
        this.close();
    }
});
