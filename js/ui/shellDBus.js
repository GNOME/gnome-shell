// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;

const Config = imports.misc.config;
const ExtensionSystem = imports.ui.extensionSystem;
const Main = imports.ui.main;

const GnomeShellIface = <interface name="org.gnome.Shell">
<method name="Eval">
    <arg type="s" direction="in" name="script" />
    <arg type="b" direction="out" name="success" />
    <arg type="s" direction="out" name="result" />
</method>
<method name="ListExtensions">
    <arg type="a{sa{sv}}" direction="out" name="extensions" />
</method>
<method name="GetExtensionInfo">
    <arg type="s" direction="in" name="extension" />
    <arg type="a{sv}" direction="out" name="info" />
</method>
<method name="GetExtensionErrors">
    <arg type="s" direction="in" name="extension" />
    <arg type="as" direction="out" name="errors" />
</method>
<method name="ScreenshotArea">
    <arg type="i" direction="in" name="x"/>
    <arg type="i" direction="in" name="y"/>
    <arg type="i" direction="in" name="width"/>
    <arg type="i" direction="in" name="height"/>
    <arg type="s" direction="in" name="filename"/>
    <arg type="b" direction="out" name="success"/>
</method>
<method name="ScreenshotWindow">
    <arg type="b" direction="in" name="include_frame"/>
    <arg type="s" direction="in" name="filename"/>
    <arg type="b" direction="out" name="success"/>
</method>
<method name="Screenshot">
    <arg type="s" direction="in" name="filename"/>
    <arg type="b" direction="out" name="success"/>
</method>
<method name="EnableExtension">
    <arg type="s" direction="in" name="uuid"/>
</method>
<method name="DisableExtension">
    <arg type="s" direction="in" name="uuid"/>
</method>
<method name="InstallRemoteExtension">
    <arg type="s" direction="in" name="uuid"/>
    <arg type="s" direction="in" name="version"/>
</method>
<method name="UninstallExtension">
    <arg type="s" direction="in" name="uuid"/>
    <arg type="b" direction="out" name="success"/>
</method>
<property name="OverviewActive" type="b" access="readwrite" />
<property name="ApiVersion" type="i" access="read" />
<property name="ShellVersion" type="s" access="read" />
<signal name="ExtensionStatusChanged">
    <arg type="s" name="uuid"/>
    <arg type="i" name="state"/>
    <arg type="s" name="error"/>
</signal>
</interface>;

const GnomeShell = new Lang.Class({
    Name: 'GnomeShellDBus',

    _init: function() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GnomeShellIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell');
        ExtensionSystem.connect('extension-state-changed',
                                Lang.bind(this, this._extensionStateChanged));
    },

    /**
     * Eval:
     * @code: A string containing JavaScript code
     *
     * This function executes arbitrary code in the main
     * loop, and returns a boolean success and
     * JSON representation of the object as a string.
     *
     * If evaluation completes without throwing an exception,
     * then the return value will be [true, JSON.stringify(result)].
     * If evaluation fails, then the return value will be
     * [false, JSON.stringify(exception)];
     *
     */
    Eval: function(code) {
        if (!global.settings.get_boolean('development-tools'))
            return [false, null];

        let returnValue;
        let success;
        try {
            returnValue = JSON.stringify(eval(code));
            // A hack; DBus doesn't have null/undefined
            if (returnValue == undefined)
                returnValue = '';
            success = true;
        } catch (e) {
            returnValue = JSON.stringify(e);
            success = false;
        }
        return [success, returnValue];
    },

    /**
     * ScreenshotArea:
     * @x: The X coordinate of the area
     * @y: The Y coordinate of the area
     * @width: The width of the area
     * @height: The height of the area
     * @filename: The filename for the screenshot
     *
     * Takes a screenshot of the passed in area and saves it
     * in @filename as png image, it returns a boolean
     * indicating whether the operation was successful or not.
     *
     */
    ScreenshotAreaAsync : function (params, invocation) {
        let [x, y, width, height, filename, callback] = params;
        global.screenshot_area (x, y, width, height, filename,
            function (obj, result) {
                let retval = GLib.Variant.new('(b)', [result]);
                invocation.return_value(retval);
            });
    },

    /**
     * ScreenshotWindow:
     * @include_frame: Whether to include the frame or not
     * @filename: The filename for the screenshot
     *
     * Takes a screenshot of the focused window (optionally omitting the frame)
     * and saves it in @filename as png image, it returns a boolean
     * indicating whether the operation was successful or not.
     *
     */
    ScreenshotWindowAsync : function (params, invocation) {
        let [include_frame, filename] = params;
        global.screenshot_window (include_frame, filename,
            function (obj, result) {
                let retval = GLib.Variant.new('(b)', [result]);
                invocation.return_value(retval);
            });
    },

    /**
     * Screenshot:
     * @filename: The filename for the screenshot
     *
     * Takes a screenshot of the whole screen and saves it
     * in @filename as png image, it returns a boolean
     * indicating whether the operation was successful or not.
     *
     */
    ScreenshotAsync : function (params, invocation) {
        let [filename] = params;
        global.screenshot(filename,
            function (obj, result) {
                let retval = GLib.Variant.new('(b)', [result]);
                invocation.return_value(retval);
            });
    },

    ListExtensions: function() {
        let out = {};
        for (let uuid in ExtensionSystem.extensionMeta) {
            let dbusObj = this.GetExtensionInfo(uuid);
            out[uuid] = dbusObj;
        }
        return out;
    },

    GetExtensionInfo: function(uuid) {
        let meta = ExtensionSystem.extensionMeta[uuid] || {};
        let out = {};
        for (let key in meta) {
            let val = meta[key];
            let type;
            switch (typeof val) {
            case 'string':
                type = 's';
                break;
            case 'number':
                type = 'd';
                break;
            default:
                continue;
            }
            out[key] = GLib.Variant.new(type, val);
        }
        return out;
    },

    GetExtensionErrors: function(uuid) {
        return ExtensionSystem.errors[uuid] || [];
    },

    EnableExtension: function(uuid) {
        let enabledExtensions = global.settings.get_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY);
        if (enabledExtensions.indexOf(uuid) == -1)
            enabledExtensions.push(uuid);
        global.settings.set_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY, enabledExtensions);
    },

    DisableExtension: function(uuid) {
        let enabledExtensions = global.settings.get_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY);
        while (enabledExtensions.indexOf(uuid) != -1)
            enabledExtensions.splice(enabledExtensions.indexOf(uuid), 1);
        global.settings.set_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY, enabledExtensions);
    },

    InstallRemoteExtension: function(uuid, version_tag) {
        ExtensionSystem.installExtensionFromUUID(uuid, version_tag);
    },

    UninstallExtension: function(uuid) {
        return ExtensionSystem.uninstallExtensionFromUUID(uuid);
    },

    get OverviewActive() {
        return Main.overview.visible;
    },

    set OverviewActive(visible) {
        if (visible)
            Main.overview.show();
        else
            Main.overview.hide();
    },

    ApiVersion: ExtensionSystem.API_VERSION,

    ShellVersion: Config.PACKAGE_VERSION,

    _extensionStateChanged: function(_, newState) {
        this._dbusImpl.emit_signal('ExtensionStatusChanged',
                                   GLib.Variant.new('(sis)', [newState.uuid, newState.state, newState.error]));
    }
});
