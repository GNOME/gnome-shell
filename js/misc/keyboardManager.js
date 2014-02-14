// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;

const Main = imports.ui.main;

let _xkbInfo = null;

function getXkbInfo() {
    if (_xkbInfo == null)
        _xkbInfo = new GnomeDesktop.XkbInfo();
    return _xkbInfo;
}

let _keyboardManager = null;

function getKeyboardManager() {
    if (_keyboardManager == null)
        _keyboardManager = new KeyboardManager();
    return _keyboardManager;
}

function releaseKeyboard() {
    if (Main.modalCount > 0)
        global.display.unfreeze_keyboard(global.get_current_time());
    else
        global.display.ungrab_keyboard(global.get_current_time());
}

function holdKeyboard() {
    global.freeze_keyboard(global.get_current_time());
}

const KeyboardManager = new Lang.Class({
    Name: 'KeyboardManager',

    // This is the longest we'll keep the keyboard frozen until an input
    // source is active.
    _MAX_INPUT_SOURCE_ACTIVATION_TIME: 4000, // ms

    _BUS_NAME: 'org.gnome.SettingsDaemon.Keyboard',
    _OBJECT_PATH: '/org/gnome/SettingsDaemon/Keyboard',

    _INTERFACE: '\
        <node> \
        <interface name="org.gnome.SettingsDaemon.Keyboard"> \
            <method name="SetInputSource"> \
                <arg type="u" direction="in" /> \
            </method> \
        </interface> \
        </node>',

    _init: function() {
        let Proxy = Gio.DBusProxy.makeProxyWrapper(this._INTERFACE);
        this._proxy = new Proxy(Gio.DBus.session,
                                this._BUS_NAME,
                                this._OBJECT_PATH,
                                function(proxy, error) {
                                    if (error)
                                        log(error.message);
                                });
        this._proxy.g_default_timeout = this._MAX_INPUT_SOURCE_ACTIVATION_TIME;
    },

    SetInputSource: function(is) {
        holdKeyboard();
        this._proxy.SetInputSourceRemote(is.index, releaseKeyboard);
    }
});
