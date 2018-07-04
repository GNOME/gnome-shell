// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Mainloop = imports.mainloop;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Params = imports.misc.params;
const Shell = imports.gi.Shell;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const ShellMountOperation = imports.ui.shellMountOperation;

var GNOME_SESSION_AUTOMOUNT_INHIBIT = 16;

// GSettings keys
const SETTINGS_SCHEMA = 'org.gnome.desktop.media-handling';
const SETTING_ENABLE_AUTOMOUNT = 'automount';

var AUTORUN_EXPIRE_TIMEOUT_SECS = 10;

var AutomountManager = new Lang.Class({
    Name: 'AutomountManager',

    _init() {
        this._settings = new Gio.Settings({ schema_id: SETTINGS_SCHEMA });
        this._volumeQueue = [];
        this._session = new GnomeSession.SessionManager();
        this._session.connectSignal('InhibitorAdded',
                                    this._InhibitorsChanged.bind(this));
        this._session.connectSignal('InhibitorRemoved',
                                    this._InhibitorsChanged.bind(this));
        this._inhibited = false;

        this._volumeMonitor = Gio.VolumeMonitor.get();
    },

    enable() {
        this._volumeAddedId = this._volumeMonitor.connect('volume-added', this._onVolumeAdded.bind(this));
        this._volumeRemovedId = this._volumeMonitor.connect('volume-removed', this._onVolumeRemoved.bind(this));
        this._driveConnectedId = this._volumeMonitor.connect('drive-connected', this._onDriveConnected.bind(this));
        this._driveDisconnectedId = this._volumeMonitor.connect('drive-disconnected', this._onDriveDisconnected.bind(this));
        this._driveEjectButtonId = this._volumeMonitor.connect('drive-eject-button', this._onDriveEjectButton.bind(this));

        this._mountAllId = Mainloop.idle_add(this._startupMountAll.bind(this));
        GLib.Source.set_name_by_id(this._mountAllId, '[gnome-shell] this._startupMountAll');
    },

    disable() {
        this._volumeMonitor.disconnect(this._volumeAddedId);
        this._volumeMonitor.disconnect(this._volumeRemovedId);
        this._volumeMonitor.disconnect(this._driveConnectedId);
        this._volumeMonitor.disconnect(this._driveDisconnectedId);
        this._volumeMonitor.disconnect(this._driveEjectButtonId);

        if (this._mountAllId > 0) {
            Mainloop.source_remove(this._mountAllId);
            this._mountAllId = 0;
        }
    },

    _InhibitorsChanged(object, senderName, [inhibtor]) {
        this._session.IsInhibitedRemote(GNOME_SESSION_AUTOMOUNT_INHIBIT,
            (result, error) => {
                if (!error) {
                    this._inhibited = result[0];
                }
            });
    },

    _startupMountAll() {
        let volumes = this._volumeMonitor.get_volumes();
        volumes.forEach(volume => {
            this._checkAndMountVolume(volume, { checkSession: false,
                                                useMountOp: false,
                                                allowAutorun: false });
        });

        this._mountAllId = 0;
        return GLib.SOURCE_REMOVE;
    },

    _onDriveConnected() {
        // if we're not in the current ConsoleKit session,
        // or screensaver is active, don't play sounds
        if (!this._session.SessionIsActive)
            return;

        global.play_theme_sound(0, 'device-added-media',
                                _("External drive connected"),
                                null);
    },

    _onDriveDisconnected() {
        // if we're not in the current ConsoleKit session,
        // or screensaver is active, don't play sounds
        if (!this._session.SessionIsActive)
            return;

        global.play_theme_sound(0, 'device-removed-media',
                                _("External drive disconnected"),
                                null);
    },

    _onDriveEjectButton(monitor, drive) {
        // TODO: this code path is not tested, as the GVfs volume monitor
        // doesn't emit this signal just yet.
        if (!this._session.SessionIsActive)
            return;

        // we force stop/eject in this case, so we don't have to pass a
        // mount operation object
        if (drive.can_stop()) {
            drive.stop
                (Gio.MountUnmountFlags.FORCE, null, null,
                 (drive, res) => {
                     try {
                         drive.stop_finish(res);
                     } catch (e) {
                         log("Unable to stop the drive after drive-eject-button " + e.toString());
                     }
                 });
        } else if (drive.can_eject()) {
            drive.eject_with_operation 
                (Gio.MountUnmountFlags.FORCE, null, null,
                 (drive, res) => {
                     try {
                         drive.eject_with_operation_finish(res);
                     } catch (e) {
                         log("Unable to eject the drive after drive-eject-button " + e.toString());
                     }
                 });
        }
    },

    _onVolumeAdded(monitor, volume) {
        this._checkAndMountVolume(volume);
    },

    _checkAndMountVolume(volume, params) {
        params = Params.parse(params, { checkSession: true,
                                        useMountOp: true,
                                        allowAutorun: true });

        if (params.checkSession) {
            // if we're not in the current ConsoleKit session,
            // don't attempt automount
            if (!this._session.SessionIsActive)
                return;
        }

        if (this._inhibited)
            return;

        // Volume is already mounted, don't bother.
        if (volume.get_mount())
            return;

        if (!this._settings.get_boolean(SETTING_ENABLE_AUTOMOUNT) ||
            !volume.should_automount() ||
            !volume.can_mount()) {
            // allow the autorun to run anyway; this can happen if the
            // mount gets added programmatically later, even if 
            // should_automount() or can_mount() are false, like for
            // blank optical media.
            this._allowAutorun(volume);
            this._allowAutorunExpire(volume);

            return;
        }

        if (params.useMountOp) {
            let operation = new ShellMountOperation.ShellMountOperation(volume);
            this._mountVolume(volume, operation, params.allowAutorun);
        } else {
            this._mountVolume(volume, null, params.allowAutorun);
        }
    },

    _mountVolume(volume, operation, allowAutorun) {
        if (allowAutorun)
            this._allowAutorun(volume);

        let mountOp = operation ? operation.mountOp : null;
        volume._operation = operation;

        volume.mount(0, mountOp, null,
                     this._onVolumeMounted.bind(this));
    },

    _onVolumeMounted(volume, res) {
        this._allowAutorunExpire(volume);

        try {
            volume.mount_finish(res);
            this._closeOperation(volume);
        } catch (e) {
            // FIXME: we will always get G_IO_ERROR_FAILED from the gvfs udisks
            // backend in this case, see 
            // https://bugs.freedesktop.org/show_bug.cgi?id=51271
            if (e.message.indexOf('No key available with this passphrase') != -1) {
                this._reaskPassword(volume);
            } else {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED_HANDLED))
                    log('Unable to mount volume ' + volume.get_name() + ': ' + e.toString());

                this._closeOperation(volume);
            }
        }
    },

    _onVolumeRemoved(monitor, volume) {
        if (volume._allowAutorunExpireId && volume._allowAutorunExpireId > 0) {
            Mainloop.source_remove(volume._allowAutorunExpireId);
            delete volume._allowAutorunExpireId;
        }
        this._volumeQueue = 
            this._volumeQueue.filter(element => (element != volume));
    },

    _reaskPassword(volume) {
        let existingDialog = volume._operation ? volume._operation.borrowDialog() : null;
        let operation = 
            new ShellMountOperation.ShellMountOperation(volume,
                                                        { existingDialog: existingDialog });
        this._mountVolume(volume, operation);
    },

    _closeOperation(volume) {
        if (volume._operation)
            volume._operation.close();
    },

    _allowAutorun(volume) {
        volume.allowAutorun = true;
    },

    _allowAutorunExpire(volume) {
        let id = Mainloop.timeout_add_seconds(AUTORUN_EXPIRE_TIMEOUT_SECS, () => {
            volume.allowAutorun = false;
            delete volume._allowAutorunExpireId;
            return GLib.SOURCE_REMOVE;
        });
        volume._allowAutorunExpireId = id;
        GLib.Source.set_name_by_id(id, '[gnome-shell] volume.allowAutorun');
    }
});
var Component = AutomountManager;
