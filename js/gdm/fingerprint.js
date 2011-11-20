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

const FprintManagerProxy = Gio.DBusProxy.makeProxyWrapper(FprintManagerIface);

function FprintManager() {
    return new FprintManagerProxy(Gio.DBus.system,
                           'net.reactivated.Fprint',
                           '/net/reactivated/Fprint/Manager');
};
