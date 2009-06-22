/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

const THUMBNAIL_ICON_MARGIN = 2;

function DocInfo(recentInfo) {
    this._init(recentInfo);
}

DocInfo.prototype = {
    _init : function(recentInfo) {
        this._recentInfo = recentInfo;
        this.name = recentInfo.get_display_name();
        this.uri = recentInfo.get_uri();
        this.mimeType = recentInfo.get_mime_type();
    },

    getIcon : function(size) {
        let icon = new Clutter.Texture();
        let iconPixbuf;

        if (this.uri.match("^file://"))
            iconPixbuf = Shell.get_thumbnail(this.uri, this.mimeType);

        if (iconPixbuf) {
            // We calculate the width and height of the texture so as
            // to preserve the aspect ratio of the thumbnail. Because
            // the images generated based on thumbnails don't have an
            // internal padding like system icons do, we create a
            // slightly smaller texture and then create a group around
            // it for padding purposes

            let scalingFactor = (size - THUMBNAIL_ICON_MARGIN * 2) / Math.max(iconPixbuf.get_width(), iconPixbuf.get_height());
            icon.set_width(Math.ceil(iconPixbuf.get_width() * scalingFactor));
            icon.set_height(Math.ceil(iconPixbuf.get_height() * scalingFactor));
            Shell.clutter_texture_set_from_pixbuf(icon, iconPixbuf);

            let group = new Clutter.Group({ width: size,
                                            height: size });
            group.add_actor(icon);
            icon.set_position(THUMBNAIL_ICON_MARGIN, THUMBNAIL_ICON_MARGIN);
            return group;
        } else {
            Shell.clutter_texture_set_from_pixbuf(icon, this._recentInfo.get_icon(size));
            return icon;
        }
    },

    launch : function() {
        // While using Gio.app_info_launch_default_for_uri() would be
        // shorter in terms of lines of code, we are not doing so
        // because that would duplicate the work of retrieving the
        // mime type.

        let appInfo = Gio.app_info_get_default_for_type(this.mimeType, true);

        if (appInfo != null) {
            appInfo.launch_uris([this.uri], Main.createAppLaunchContext());
        } else {
            log("Failed to get default application info for mime type " + mimeType +
                ". Will try to use the last application that registered the document.");
            let appName = this._recentInfo.last_application();
            let [success, appExec, count, time] = this._recentInfo.get_application_info(appName);
            if (success) {
                log("Will open a document with the following command: " + appExec);
                // TODO: Change this once better support for creating
                // GAppInfo is added to GtkRecentInfo, as right now
                // this relies on the fact that the file uri is
                // already a part of appExec, so we don't supply any
                // files to appInfo.launch().

                // The 'command line' passed to
                // create_from_command_line is allowed to contain
                // '%<something>' macros that are expanded to file
                // name / icon name, etc, so we need to escape % as %%
                appExec = appExec.replace(/%/g, "%%");

                let appInfo = Gio.app_info_create_from_commandline(appExec, null, 0, null);

                // The point of passing an app launch context to
                // launch() is mostly to get startup notification and
                // associated benefits like the app appearing on the
                // right desktop; but it doesn't really work for now
                // because with the way we create the appInfo we
                // aren't reading the application's desktop file, and
                // thus don't find the StartupNotify=true in it. So,
                // despite passing the app launch context, no startup
                // notification occurs.
                appInfo.launch([], Main.createAppLaunchContext());
            } else {
                log("Failed to get application info for " + this.uri);
            }
        }
    },

    exists : function() {
        return this._recentInfo.exists();
    },

    lastVisited : function() {
        // We actually used get_modified() instead of get_visited()
        // here, as GtkRecentInfo doesn't updated get_visited()
        // correctly. See
        // http://bugzilla.gnome.org/show_bug.cgi?id=567094

        return this._recentInfo.get_modified();
    }
};
