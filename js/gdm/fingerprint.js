// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const FprintManagerIface = <interface name='net.reactivated.Fprint.Manager'>
<method name='GetDefaultDevice'>
    <arg type='o' direction='out' />
</method>
</interface>;

const FprintManager = new Gio.DBusProxyClass({
    Name: 'FprintManager',
    Interface: FprintManagerIface,

    _init: function() {
        this.parent({ g_bus_type: Gio.BusType.SYSTEM,
                      g_name: 'net.reactivated.Fprint',
                      g_object_path: '/net/reactivated/Fprint/Manager',
                      g_flags: (Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES) });
    }
});
