// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const Hash = imports.misc.hash;
const Main = imports.ui.main;

const ScreencastIface = '<node> \
<interface name="org.gnome.Shell.Screencast"> \
<method name="Screencast"> \
    <arg type="s" direction="in" name="file_template"/> \
    <arg type="a{sv}" direction="in" name="options"/> \
    <arg type="b" direction="out" name="success"/> \
    <arg type="s" direction="out" name="filename_used"/> \
</method> \
<method name="ScreencastArea"> \
    <arg type="i" direction="in" name="x"/> \
    <arg type="i" direction="in" name="y"/> \
    <arg type="i" direction="in" name="width"/> \
    <arg type="i" direction="in" name="height"/> \
    <arg type="s" direction="in" name="file_template"/> \
    <arg type="a{sv}" direction="in" name="options"/> \
    <arg type="b" direction="out" name="success"/> \
    <arg type="s" direction="out" name="filename_used"/> \
</method> \
<method name="StopScreencast"> \
    <arg type="b" direction="out" name="success"/> \
</method> \
</interface> \
</node>';

const ScreencastService = new Lang.Class({
    Name: 'ScreencastService',

    _init: function() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(ScreencastIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Screencast');

        Gio.DBus.session.own_name('org.gnome.Shell.Screencast', Gio.BusNameOwnerFlags.REPLACE, null, null);

        this._recorders = new Hash.Map();

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
    },

    get isRecording() {
        return this._recorders.size() > 0;
    },

    _ensureRecorderForSender: function(sender) {
        let recorder = this._recorders.get(sender);
        if (!recorder) {
            recorder = new Shell.Recorder({ stage: global.stage,
                                            screen: global.screen });
            recorder._watchNameId =
                Gio.bus_watch_name(Gio.BusType.SESSION, sender, 0, null,
                                   Lang.bind(this, this._onNameVanished));
            this._recorders.set(sender, recorder);
            this.emit('updated');
        }
        return recorder;
    },

    _sessionUpdated: function() {
        if (Main.sessionMode.allowScreencast)
            return;

        for (let sender in this._recorders.keys())
            this._recorders.delete(sender);
        this.emit('updated');
    },

    _onNameVanished: function(connection, name) {
        this._stopRecordingForSender(name);
    },

    _stopRecordingForSender: function(sender) {
        let recorder = this._recorders.get(sender);
        if (!recorder)
            return false;

        Gio.bus_unwatch_name(recorder._watchNameId);
        recorder.close();
        this._recorders.delete(sender);
        this.emit('updated');

        return true;
    },

    _applyOptionalParameters: function(recorder, options) {
        for (let option in options)
            options[option] = options[option].deep_unpack();

        if (options['pipeline'])
            recorder.set_pipeline(options['pipeline']);
        if (options['framerate'])
            recorder.set_framerate(options['framerate']);
        if (options['draw-cursor'])
            recorder.set_draw_cursor(options['draw-cursor']);
    },

    ScreencastAsync: function(params, invocation) {
        let returnValue = [false, ''];
        if (!Main.sessionMode.allowScreencast)
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));

        let sender = invocation.get_sender();
        let recorder = this._ensureRecorderForSender(sender);
        if (!recorder.is_recording()) {
            let [fileTemplate, options] = params;

            recorder.set_file_template(fileTemplate);
            this._applyOptionalParameters(recorder, options);
            let [success, fileName] = recorder.record();
            returnValue = [success, fileName ? fileName : ''];
        }

        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
    },

    ScreencastAreaAsync: function(params, invocation) {
        let returnValue = [false, ''];
        if (!Main.sessionMode.allowScreencast)
            invocation.return_value(GLib.Variant.new('(bs)', returnValue));

        let sender = invocation.get_sender();
        let recorder = this._ensureRecorderForSender(sender);

        if (!recorder.is_recording()) {
            let [x, y, width, height, fileTemplate, options] = params;

            recorder.set_file_template(fileTemplate);
            recorder.set_area(x, y, width, height);
            this._applyOptionalParameters(recorder, options);
            let [success, fileName] = recorder.record();
            returnValue = [success, fileName ? fileName : ''];
        }

        invocation.return_value(GLib.Variant.new('(bs)', returnValue));
    },

    StopScreencastAsync: function(params, invocation) {
        let success = this._stopRecordingForSender(invocation.get_sender());
        invocation.return_value(GLib.Variant.new('(b)', [success]));
    }
});
Signals.addSignalMethods(ScreencastService.prototype);
