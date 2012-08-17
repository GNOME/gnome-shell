// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;

const ConsoleKitManagerIface = <interface name='org.freedesktop.ConsoleKit.Manager'>
<method name='CanRestart'>
    <arg type='b' direction='out'/>
</method>
<method name='CanStop'>
    <arg type='b' direction='out'/>
</method>
<method name='Restart' />
<method name='Stop' />
<method name='GetCurrentSession'>
    <arg type='o' direction='out' />
</method>
</interface>;

const ConsoleKitSessionIface = <interface name='org.freedesktop.ConsoleKit.Session'>
<method name='IsActive'>
    <arg type='b' direction='out' />
</method>
<signal name='ActiveChanged'>
    <arg type='b' direction='out' />
</signal>
<signal name='Lock' />
<signal name='Unlock' />
</interface>;

const ConsoleKitSessionProxy = Gio.DBusProxy.makeProxyWrapper(ConsoleKitSessionIface);

const ConsoleKitManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(ConsoleKitManagerIface);

function ConsoleKitManager() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
				   g_interface_name: ConsoleKitManagerInfo.name,
				   g_interface_info: ConsoleKitManagerInfo,
				   g_name: 'org.freedesktop.ConsoleKit',
				   g_object_path: '/org/freedesktop/ConsoleKit/Manager',
                                   g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES });
    self.init(null);

    self.GetCurrentSessionRemote(function([session]) {
        self.ckSession = new ConsoleKitSessionProxy(Gio.DBus.system, 'org.freedesktop.ConsoleKit', session);

        self.ckSession.connectSignal('ActiveChanged', function(object, senderName, [isActive]) {
            self.sessionActive = isActive;
        });
        self.ckSession.IsActiveRemote(function([isActive]) {
            self.sessionActive = isActive;
        });
    });

    return self;
}
