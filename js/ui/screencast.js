// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, GLib, Shell } = imports.gi;
const Signals = imports.signals;

const Main = imports.ui.main;

const { loadInterfaceXML } = imports.misc.fileUtils;

const ScreencastIface = loadInterfaceXML('org.gnome.Shell.Screencast');

var ScreencastService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(ScreencastIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Screencast');

        Gio.DBus.session.own_name('org.gnome.Shell.Screencast', Gio.BusNameOwnerFlags.REPLACE, null, null);

        this._recorders = new Map();

        this._lockdownSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    }

    get isRecording() {
        return this._recorders.size > 0;
    }

    _ensureRecorderForSender(sender) {
        let recorder = this._recorders.get(sender);
        if (!recorder) {
            recorder = new Shell.Recorder({ stage: global.stage,
                                            display: global.display });
            recorder._watchNameId =
                Gio.bus_watch_name(Gio.BusType.SESSION, sender, 0, null,
                                   this._onNameVanished.bind(this));
            this._recorders.set(sender, recorder);
            this.emit('updated');
        }
        return recorder;
    }

    _sessionUpdated() {
        if (Main.sessionMode.allowScreencast)
            return;

        for (let sender of this._recorders.keys())
            this._stopRecordingForSender(sender);
    }

    _onNameVanished(connection, name) {
        this._stopRecordingForSender(name);
    }

    _stopRecordingForSender(sender) {
        let recorder = this._recorders.get(sender);
        if (!recorder)
            return false;

        Gio.bus_unwatch_name(recorder._watchNameId);
        recorder.close();
        this._recorders.delete(sender);
        this.emit('updated');

        return true;
    }

    _applyOptionalParameters(recorder, options) {
        for (let option in options)
            options[option] = options[option].deep_unpack();

        if (options['pipeline'])
            recorder.set_pipeline(options['pipeline']);
        if (options['framerate'])
            recorder.set_framerate(options['framerate']);
        if ('draw-cursor' in options)
            recorder.set_draw_cursor(options['draw-cursor']);
    }

    ScreencastAsync(params, invocation) {
        let returnValue = [false, ''];
        if (!Main.sessionMode.allowScreencast ||
            this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        let sender = invocation.get_sender();
        let recorder = this._ensureRecorderForSender(sender);
        if (!recorder.is_recording()) {
            let [fileTemplate, options] = params;

            recorder.set_file_template(fileTemplate);
            this._applyOptionalParameters(recorder, options);
            let [success, fileName] = recorder.record();
            returnValue = [success, fileName ? fileName : ''];
            if (!success)
                this._stopRecordingForSender(sender);
        }

        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
    }

    ScreencastAreaAsync(params, invocation) {
        let returnValue = [false, ''];
        if (!Main.sessionMode.allowScreencast ||
            this._lockdownSettings.get_boolean('disable-save-to-disk')) {
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));
            return;
        }

        let sender = invocation.get_sender();
        let recorder = this._ensureRecorderForSender(sender);

        if (!recorder.is_recording()) {
            let [x, y, width, height, fileTemplate, options] = params;

            if (x < 0 || y < 0 ||
                width <= 0 || height <= 0 ||
                x + width > global.screen_width ||
                y + height > global.screen_height) {
                invocation.return_error_literal(Gio.IOErrorEnum,
                                                Gio.IOErrorEnum.CANCELLED,
                                                "Invalid params");
                return;
            }

            recorder.set_file_template(fileTemplate);
            recorder.set_area(x, y, width, height);
            this._applyOptionalParameters(recorder, options);
            let [success, fileName] = recorder.record();
            returnValue = [success, fileName ? fileName : ''];
            if (!success)
                this._stopRecordingForSender(sender);
        }

        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
    }

    StopScreencastAsync(params, invocation) {
        let success = this._stopRecordingForSender(invocation.get_sender());
        invocation.return_value(GLib.Variant.new('(b)', [success]));
    }
};
Signals.addSignalMethods(ScreencastService.prototype);
