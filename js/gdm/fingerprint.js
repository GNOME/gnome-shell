// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported FprintManager */

const Gio = imports.gi.Gio;

const FprintManagerIface = `
<node>
<interface name="net.reactivated.Fprint.Manager">
<method name="GetDefaultDevice">
    <arg type="o" direction="out" />
</method>
</interface>
</node>`;

const FprintManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(FprintManagerIface);

function FprintManager() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
                                   g_interface_name: FprintManagerInfo.name,
                                   g_interface_info: FprintManagerInfo,
                                   g_name: 'net.reactivated.Fprint',
                                   g_object_path: '/net/reactivated/Fprint/Manager',
                                   g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES });

    try {
        self.init(null);
    } catch (e) {
        log(`Failed to connect to Fprint service: ${e.message}`);
        return null;
    }

    return self;
}
