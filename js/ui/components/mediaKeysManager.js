// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const ShellMountOperation  = imports.ui.shellMountOperation;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const INTERFACE_SETTINGS = 'org.gnome.desktop.interface';
const POWER_SETTINGS = 'org.gnome.settings-daemon.plugins.power';
const XSETTINGS_SETTINGS = 'org.gnome.settings-daemon.plugins.xsettings';
const TOUCHPAD_SETTINGS = 'org.gnome.settings-daemon.peripherals.touchpad';
const KEYBINDING_SETTINGS = 'org.gnome.settings-daemon.plugins.media-keys';
const CUSTOM_KEYBINDING_SETTINGS = 'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding';
const A11Y_SETTINGS = 'org.gnome.desktop.a11y.applications';
const MAGNIFIER_SETTINGS = 'org.gnome.desktop.a11y.magnifier';
const INPUT_SOURCE_SETTINGS = 'org.gnome.desktop.input-sources';

const MediaKeysInterface = <interface name='org.gnome.SettingsDaemon.MediaKeys'>
<method name='GrabMediaPlayerKeys'>
    <arg name='application' direction='in' type='s'/>
    <arg name='time' direction='in' type='u'/>
</method>
<method name='ReleaseMediaPlayerKeys'>
    <arg name='application' direction='in' type='s'/>
</method>
<signal name='MediaPlayerKeyPressed'>
    <arg name='application' type='s'/>
    <arg name='key' type='s'/>
</signal>
</interface>;

/* [ actionName, setting, hardcodedKeysym, overviewOnly, args ] */
/* (overviewOnly means that the keybinding is handled when the shell is not
   modal, or when the overview is active, but not when other modal operations
   are active; otherwise the keybinding is always handled) */
const DEFAULT_KEYBINDINGS = [
    [ 'doTouchpadToggle', null, 'XF86TouchpadToggle', false ],
    [ 'doTouchpadSet', null, 'XF86TouchpadOn', false, [ true ] ],
    [ 'doTouchpadSet', null, 'XF86TouchpadOff', false, [ false ] ],
    [ 'doMute', 'volume-mute', null, false, [ false ] ],
    [ 'doVolumeAdjust', 'volume-down', null, false, [ Clutter.ScrollDirection.DOWN, false ] ],
    [ 'doVolumeAdjust', 'volume-up', null, false, [ Clutter.ScrollDirection.UP, false ] ],
    [ 'doMute', null, '<Alt>XF86AudioMute', false, [ true ] ],
    [ 'doVolumeAdjust', null, '<Alt>XF86AudioLowerVolume', false, [ Clutter.ScrollDirection.DOWN, true ] ],
    [ 'doVolumeAdjust', null, '<Alt>XF86AudioRaiseVolume', false, [ Clutter.ScrollDirection.UP, true ] ],
    [ 'doLogout', 'logout', null, true ],
    [ 'doEject', 'eject', null, false ],
    [ 'doHome', 'home', null, true ],
    [ 'doLaunchMimeHandler', 'media', null, true, [ 'application/x-vorbis+ogg' ] ],
    [ 'doLaunchApp', 'calculator', null, true, [ 'gcalcltool.desktop' ] ],
    [ 'doLaunchApp', 'search', null, true, [ 'tracker-needle.desktop' ] ],
    [ 'doLaunchMimeHandler', 'email', null, true, [ 'x-scheme-handler/mailto' ] ],
    [ 'doScreensaver', 'screensaver', null, true ],
    [ 'doScreensaver', null, 'XF86ScreenSaver', true ],
    [ 'doLaunchApp', 'help', null, true, [ 'yelp.desktop' ] ],
    [ 'doSpawn', 'screenshot', null, true, [ ['gnome-screenshot'] ] ],
    [ 'doSpawn', 'window-screenshot', null, true, [ ['gnome-screenshot', '--window'] ] ],
    [ 'doSpawn', 'area-screenshot', null, true, [ ['gnome-screenshot', '--area'] ] ],
    [ 'doSpawn', 'screenshot-clip', null, true, [ ['gnome-screenshot', '--clipboard'] ] ],
    [ 'doSpawn', 'window-screenshot-clip', null, true, [ ['gnome-screenshot', '--window', '--clipboard'] ] ],
    [ 'doSpawn', 'area-screenshot-clip', null, true, [ ['gnome-screenshot', '--area', '--clipboard'] ] ],
    [ 'doLaunchMimeHandler', 'www', null, true, [ 'x-scheme-handler/http' ] ],
    [ 'doMediaKey', 'play', null, true, [ 'Play' ] ],
    [ 'doMediaKey', 'pause', null, true, [ 'Pause' ] ],
    [ 'doMediaKey', 'stop', null, true, [ 'Stop' ] ],
    [ 'doMediaKey', 'previous', null, true, [ 'Previous' ] ],
    [ 'doMediaKey', 'next', null, true, [ 'Next' ] ],
    [ 'doMediaKey', null, 'XF86AudioRewind', true, [ 'Rewind' ] ],
    [ 'doMediaKey', null, 'XF86AudioForward', true, [ 'FastForward' ] ],
    [ 'doMediaKey', null, 'XF86AudioRepeat', true, [ 'Repeat' ] ],
    [ 'doMediaKey', null, 'XF86AudioRandomPlay', true, [ 'Shuffle' ] ],
    [ 'doXRandRAction', null, '<Super>p', false, [ 'VideoModeSwitch' ] ],
    /* Key code of the XF86Display key (Fn-F7 on Thinkpads, Fn-F4 on HP machines, etc.) */
    [ 'doXRandRAction', null, 'XF86Display', false, [ 'VideoModeSwitch' ] ],
    /* Key code of the XF86RotateWindows key (present on some tablets) */
    [ 'doXRandRAction', null, 'XF86RotateWindows', false, [ 'Rotate' ] ],
    [ 'doA11yAction', 'magnifier', null, true, [ 'screen-magnifier-enabled' ] ],
    [ 'doA11yAction', 'screenreader', null, true, [ 'screen-reader-enabled' ] ],
    [ 'doA11yAction', 'on-screen-keyboard', null, true, [ 'screen-keyboard-enabled' ] ],
    [ 'doTextSize', 'increase-text-size', null, true, [ 1 ] ],
    [ 'doTextSize', 'decrease-text-size', null, true, [ -1 ] ],
    [ 'doToggleContrast', 'toggle-contrast', null, true ],
    [ 'doMagnifierZoom', 'magnifier-zoom-in', null, true, [ 1 ] ],
    [ 'doMagnifierZoom', 'magnifier-zoom-out', null, true, [ -1 ] ],
    [ 'doPowerAction', null, 'XF86PowerOff', true, [ 'button-power' ] ],
    /* the kernel / Xorg names really are like this... */
    [ 'doPowerAction', null, 'XF86Suspend', false, [ 'button-sleep' ] ],
    [ 'doPowerAction', null, 'XF86Sleep', false, [ 'button-suspend' ] ],
    [ 'doPowerAction', null, 'XF86Hibernate', false, [ 'button-hibernate' ] ],
    [ 'doBrightness', null, 'XF86MonBrightnessUp', false, [ 'Screen', 'StepUp' ] ],
    [ 'doBrightness', null, 'XF86MonBrightnessDown', false, [ 'Screen', 'StepDown' ] ],
    [ 'doBrightness', null, 'XF86KbdBrightnessUp', false, [ 'Keyboard', 'StepUp' ] ],
    [ 'doBrightness', null, 'XF86KbdBrightnessDown', false, [ 'Keyboard', 'StepDown' ] ],
    [ 'doBrightnessToggle', null, 'XF86KbdLightOnOff', false, ],
    [ 'doInputSource', 'switch-input-source', null, false, [ +1 ] ],
    [ 'doInputSource', 'switch-input-source-backward', null, false, [ -1 ] ],
    [ 'doLaunchApp', null, 'XF86Battery', true, [ 'gnome-power-statistics.desktop' ] ]
];

var osdWin;
const OSDWindow = new Lang.Class({
    Name: 'OSDWindow',

    FADE_TIMEOUT: 1500,
    FADE_DURATION: 100,

    _init: function(iconName, value) {
        /* assume 130x130 on a 640x480 display and scale from there */
        let monitor = Main.layoutManager.primaryMonitor;
        let scalew = monitor.width / 640.0;
        let scaleh = monitor.height / 480.0;
        let scale = Math.min(scalew, scaleh);
        let size = 130 * Math.max(1, scale);

        this.actor = new St.BoxLayout({ style_class: 'osd-window',
                                        vertical: true,
                                        reactive: false,
                                        visible: false,
                                        width: size,
                                        height: size,
                                      });

        this._icon = new St.Icon({ icon_name: iconName,
                                   icon_size: size / 2,
                                 });
        this.actor.add(this._icon, { expand: true,
                                     x_align: St.Align.MIDDLE,
                                     y_align: St.Align.MIDDLE });

        this._value = value;
        this._progressBar = new St.DrawingArea({ style_class: 'osd-progress-bar' });
        this._progressBar.connect('repaint', Lang.bind(this, this._drawProgress));
        this.actor.add(this._progressBar, { expand: true, x_fill: true, y_fill: false });
        this._progressBar.visible = value !== undefined;

        Main.layoutManager.addChrome(this.actor);

        /* Position in the middle of primary monitor */
        let [width, height] = this.actor.get_size();
        this.actor.x = ((monitor.width - width) / 2) + monitor.x;
        this.actor.y = monitor.y + (monitor.height / 2) + (monitor.height / 2 - height) / 2;
    },

    show: function() {
        this.actor.show();
        Tweener.addTween(this.actor,
                         { opacity: 255,
                           time: this.FADE_DURATION / 1000,
                           transition: 'easeInQuad' });

        if (this._timeoutId)
            GLib.source_remove(this._timeoutId);

        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, this.FADE_TIMEOUT, Lang.bind(this, this.hide));
    },

    hide: function() {
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           time: this.FADE_DURATION / 1000,
                           transition: 'easeOutQuad',
                           onComplete: function() {
                               this.actor.destroy();
                               this.actor = null;
                               osdWin = null;
                           },
                           onCompleteScope: this });

        return false;
    },

    setIcon: function(name) {
        this._icon.icon_name = name;
    },

    setValue: function(value) {
        if (value == this._value)
            return;

        this._value = value;
        this._progressBar.visible = value !== undefined;
        this._progressBar.queue_repaint();
    },

    _drawProgress: function(area) {
        let cr = area.get_context();

        let themeNode = this.actor.get_theme_node();
        let color = themeNode.get_foreground_color();
        Clutter.cairo_set_source_color(cr, color);

        let [width, height] = area.get_surface_size();
        width = width * this._value;

        cr.moveTo(0,0);
        cr.lineTo(width, 0);
        cr.lineTo(width, height);
        cr.lineTo(0, height);
        cr.fill();
    }
});

function showOSD(icon, value) {
    if (osdWin) {
        osdWin.setIcon(icon);
        osdWin.setValue(value);
    } else {
        osdWin = new OSDWindow(icon, value);
    }

    osdWin.show();
}

const MediaKeysGrabber = new Lang.Class({
    Name: 'MediaKeysGrabber',

    _init: function() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(MediaKeysInterface, this);
        this._apps = [];
    },

    enable: function() {
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/SettingsDaemon/MediaKeys');
    },

    disable: function() {
        this._dbusImpl.unexport();
    },

    GrabMediaPlayerKeysAsync: function(parameters, invocation) {
        let [appName, time] = parameters;

        /* I'm not sure of this code, but it is in gnome-settings-daemon
           (letting alone that the introspection is wrong in glib...)
        */
        if (time == Gdk.CURRENT_TIME) {
            let tv = new GLib.TimeVal;
            GLib.get_current_time(tv);
            time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        }

        let pos = -1;
        for (let i = 0; i < this._apps.length; i++) {
            if (this._apps[i].appName == appName) {
                pos = i;
                break;
            }
        }

        if (pos != -1)
            this._freeMediaPlayer(pos);

        let app = {
            appName: appName,
            name: invocation.get_sender(),
            time: time,
            watchId: Gio.DBus.session.watch_name(invocation.get_sender(),
                                                 Gio.BusNameWatcherFlags.NONE,
                                                 null,
                                                 Lang.bind(this, this._onNameVanished)),
        };
        Util.insertSorted(this._apps, app, function(a, b) {
            return b.time-a.time;
        });

        invocation.return_value(GLib.Variant.new('()', []));
    },

    ReleaseMediaPlayerAsync: function(parameters, invocation) {
        let name = invocation.get_sender();
        let [appName] = parameters;

        let pos = -1;
        for (let i = 0; i < this._apps.length; i++) {
            if (this._apps[i].appName == appName) {
                pos = i;
                break;
            }
        }

        if (pos == -1) {
            for (let i = 0; i < this._apps.length; i++) {
                if (this._apps[i].name == name) {
                    pos = i;
                    break;
                }
            }
        }

        if (pos != -1)
            this._freeMediaPlayer(pos);

        invocation.return_value(GLib.Variant.new('()', []));
    },

    _freeMediaPlayer: function(pos) {
        let app = this._apps[pos];
        Gio.bus_unwatch_name(app.watchId)

        this._apps.splice(pos, 1);
    },

    mediaKeyPressed: function(key) {
        if (this._apps.length == 0) {
            showOSD('action-unavailable-symbolic');
            return;
        }

        let app = this._apps[0];
        Gio.DBus.session.emit_signal(app.name,
                                     '/org/gnome/SettingsDaemon/MediaKeys',
                                     'org.gnome.SettingsDaemon.MediaKeys',
                                     'MediaPlayerKeyPressed',
                                     GLib.Variant.new('(ss)', [app.appName || '',
                                                               key]));
    },
});

const MediaKeysManager = new Lang.Class({
    Name: 'MediaKeysManager',

    _init: function() {
        this._a11yControl = Main.panel.statusArea.a11y;
        this._volumeControl = Main.panel.statusArea.volume;
        this._userMenu = Main.panel.statusArea.userMenu;
        this._mediaPlayerKeys = new MediaKeysGrabber();

        this._keybindingSettings = new Gio.Settings({ schema: KEYBINDING_SETTINGS });
    },

    enable: function() {
        for (let i = 0; i < DEFAULT_KEYBINDINGS.length; i++) {
            let [action, setting, keyval, overviewOnly, args] = DEFAULT_KEYBINDINGS[i];
            let func = this[action];
            if (!func) {
                log('Keybinding action %s is missing'.format(action));
                continue;
            }

            let name = setting ? setting : 'media-keys-keybindings-%d'.format(i);
            let ok;
            func = Util.wrapKeybinding(Lang.bind.apply(null, [this, func].concat(args)), overviewOnly);
            if (setting)
                ok = global.display.add_keybinding(setting, this._keybindingSettings,
                                                   Meta.KeyBindingFlags.BUILTIN |
                                                   Meta.KeyBindingFlags.IS_SINGLE |
                                                   Meta.KeyBindingFlags.HANDLE_WHEN_GRABBED, func);
            else
                ok = global.display.add_grabbed_key(name, keyval,
                                                    Meta.KeyBindingFlags.HANDLE_WHEN_GRABBED, func);

            if (!ok)
                log('Installing keybinding %s failed'.format(name));
        }

        this._customKeybindings = [];
        this._changedId = this._keybindingSettings.connect('changed::custom-keybindings',
                                                           Lang.bind(this, this._reloadCustomKeybindings));
        this._reloadCustomKeybindings();

        this._mediaPlayerKeys.enable();
    },

    disable: function() {
        for (let i = 0; i < DEFAULT_KEYBINDINGS.length; i++) {
            let [action, setting, keyval, overviewOnly, args] = DEFAULT_KEYBINDINGS[i];

            let name = setting ? setting : 'media-keys-keybindings-%d'.format(i);
            if (setting)
                global.display.remove_keybinding(setting, this._keybindingSettings);
            else
                global.display.remove_grabbed_key(name);
        }

        this._clearCustomKeybindings();
        this._keybindingSettings.disconnect(this._changedId);

        this._mediaPlayerKeys.disable();
    },

    _clearCustomKeybindings: function() {
        for (let i = 0; i < this._customKeybindings.length; i++)
            global.display.remove_keybinding('binding', this._customKeybindings[i]);

        this._customKeybindings = [];
    },

    _reloadCustomKeybindings: function() {
        this._clearCustomKeybindings();

        let paths = this._keybindingSettings.get_strv('custom-keybindings');
        for (let i = 0; i < paths.length; i++) {
            let setting = new Gio.Settings({ schema: CUSTOM_KEYBINDING_SETTINGS,
                                             path: paths[i] });
            let func = Util.wrapKeybinding(Lang.bind(this, this.doCustom, setting), true);

            global.display.add_keybinding('binding', setting,
                                          Meta.KeyBindingFlags.IS_SINGLE |
                                          Meta.KeyBindingFlags.HANDLE_WHEN_GRABBED, func);
            this._customKeybindings.push(setting);
        }
    },

    doCustom: function(display, screen, window, binding, settings) {
        let command = settings.get_string('command');
        Util.spawnCommandLine(command);
    },

    doTouchpadToggle: function(display, screen, window, binding) {
        let settings = new Gio.Settings({ schema: TOUCHPAD_SETTINGS });
        let enabled = settings.get_boolean('touchpad-enabled');

        this.doTouchpadSet(display, screen, window, binding, !enabled);
        settings.set_boolean(!enabled);

        return true;
    },

    doTouchpadSet: function(display, screen, window, binding, enabled) {
        showOSD(enabled ? 'input-touchpad-symbolic' : 'touchpad-disabled-symbolic');
        return true;
    },

    doMute: function(display, screen, window, binding, quiet) {
        let [icon, value] = this._volumeControl.volumeMenu.toggleMute(quiet);
        showOSD(icon, value);
        return true;
    },

    doVolumeAdjust: function(display, screen, window, binding, direction, quiet) {
        let [icon, value] = this._volumeControl.volumeMenu.scroll(direction, quiet);
        showOSD(icon, value);
        return true;
    },

    doLogout: function(display, screen, window, binding) {
        this._userMenu.logOut();
        return true;
    },

    doEject: function(display, screen, window, binding) {
        let volumeMonitor = Gio.VolumeMonitor.get();

        let drives = volumeMonitor.get_connected_drives();
        let score = 0, drive;
        for (let i = 0; i < drives.length; i++) {
            if (!drives[i].can_eject())
                continue;
            if (!drives[i].is_media_removable())
                continue;
            if (score < 1) {
                drive = drives[i];
                score = 1;
            }
            if (!drives[i].has_media())
                continue;
            if (score < 2) {
                drive = drives[i];
                score = 2;
                break;
            }
        }

        showOSD('media-eject-custom-symbolic');

        if (!drive)
            return true;

        let mountOp = new ShellMountOperation.ShellMountOperation(drive);
        drive.eject_with_operation(Gio.MountUnmountFlags.FORCE,
                                   mountOp.mountOp, null, null);

        return true;
    },

    doHome: function() {
        let homeFile = Gio.file_new_for_path (GLib.get_home_dir());
        let homeUri = homeFile.get_uri();
        Gio.app_info_launch_default_for_uri(homeUri, null);

        return true;
    },

    doLaunchMimeHandler: function(display, screen, window, binding, mimeType) {
        let gioApp = Gio.AppInfo.get_default_for_type(mimeType, false);
        if (gioApp != null) {
            let app = Shell.AppSystem.get_default().lookup_app(gioApp.get_id());
            app.open_new_window(-1);
        } else {
            log('Could not find default application for \'%s\' mime-type'.format(mimeType));
        }

        return true;
    },

    doLaunchApp: function(display, screen, window, binding, appId) {
        let app = Shell.AppSystem.get_default().lookup_app(appId);
        app.open_new_window(-1);

        return true;
    },

    doScreensaver: function() {
        // FIXME: handled in house, to the screenshield!
        return true;
    },

    doSpawn: function(display, screen, window, binding, argv) {
        Util.spawn(argv);
        return true;
    },

    doMediaKey: function(display, screen, window, binding, key) {
        this._mediaPlayerKeys.mediaKeyPressed(key);
    },

    _onXRandRFinished: function(connection, result) {
        connection.call_finish(result);
        this._XRandRCancellable = null;
    },

    doXRandRAction: function(display, screen, window, binding, action) {
        if (this._XRandRCancellable)
            this._XRandRCancellable.cancel();

        this._XRandRCancellable = new Gio.Cancellable();
        Gio.DBus.session.call('org.gnome.SettingsDaemon',
                              '/org/gnome/SettingsDaemon/XRANDR',
                              'org.gnome.SettingsDaemon.XRANDR_2',
                              action,
                              GLib.Variant.new('(x)', [global.get_current_time()]),
                              null, /* reply type */
                              Gio.DBusCallFlags.NONE,
                              -1,
                              this._XRandRCancellable,
                              Lang.bind(this, this._onXRandRFinished));
    },

    doA11yAction: function(display, screen, window, binding, key) {
        let settings = new Gio.Settings({ schema: A11Y_SETTINGS });
        let enabled = settings.get_boolean(key);
        settings.set_boolean(key, !enabled);
    },

    doTextSize: function(display, screen, window, binding, multiplier) {
        // Same values used in the Seeing tab of the Universal Access panel
        const FACTORS = [ 0.75, 1.0, 1.25, 1.5 ];

	// Figure out the current DPI scaling factor
        let settings = new Gio.Settings({ schema: INTERFACE_SETTINGS });
        let factor = settings.get_double('text-scaling-factor');
        factor += multiplier * 0.25;

        /* Try to find a matching value */
        let distance = 1e6;
        let best = 1.0;
        for (let i = 0; i < FACTORS.length; i++) {
            let d = Math.abs(factor - FACTORS[i]);
            if (d < distance) {
                best = factors[i];
                distance = d;
            }
        }

        if (best == 1.0)
            settings.reset('text-scaling-factor');
        else
            settings.set_double('text-scaling-factor', best);
    },

    doToggleContrast: function(display, screen, window, binding) {
        this._a11yControl.toggleHighContrast();
    },

    doMagnifierZoom: function(display, screen, window, binding, offset) {
        let settings = new Gio.Settings({ schema: MAGNIFIER_SETTINGS });

        let value = settings.get_value('mag-factor');
        value = Math.round(value + offset);
        settings.set_value('mag-factor', value);
    },

    doPowerAction: function(display, screen, window, binding, action) {
        let settings = new Gio.Settings({ schema: POWER_SETTINGS });
        switch (settings.get_string(action)) {
        case 'suspend':
            this._userMenu.suspend();
            break;
        case 'interactive':
        case 'shutdown':
            this._userMenu.shutdown();
            break;
        case 'hibernate':
            this._userMenu.hibernate();
            break;
        case 'blank':
        case 'default':
        default:
            break;
        }
    },

    _onBrightnessFinished: function(connection, result, kind) {
        let [percentage] = connection.call_finish(result).deep_unpack();

        let icon = kind == 'Keyboard' ? 'keyboard-brightness-symbolic' : 'display-brightness-symbolic';
        showOSD(icon, percentage / 100);
    },

    doBrightness: function(display, screen, window, binding, kind, action) {
        let iface = 'org.gnome.SettingsDaemon.Power.' + kind;
        let objectPath = '/org/gnome/SettingsDaemon/Power';

        Gio.DBus.session.call('org.gnome.SettingsDaemon',
                              objectPath, iface, action,
                              null, null, /* parameters, reply type */
                              Gio.DBusCallFlags.NONE, -1, null,
                              Lang.bind(this, this._onBrightnessFinished, kind));
    },

    doInputSource: function(display, screen, window, binding, offset) {
        let settings = new Gio.Settings({ schema: INPUT_SOURCE_SETTINGS });

        let current = settings.get_uint('current');
        let max = settings.get_strv('sources').length - 1;

        current += offset;
        if (current < 0)
            current = 0;
        else if (current > max)
            current = max;

        settings.set_uint('current', current);
    },
});

const Component = MediaKeysManager;
