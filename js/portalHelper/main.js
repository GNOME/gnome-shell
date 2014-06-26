const Format = imports.format;
const Gettext = imports.gettext;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Soup = imports.gi.Soup;
const WebKit = imports.gi.WebKit2;

const _ = Gettext.gettext;

const Config = imports.misc.config;

const PortalHelperResult = {
    CANCELLED: 0,
    COMPLETED: 1,
    RECHECK: 2
};

const INACTIVITY_TIMEOUT = 30000; //ms
const CONNECTIVITY_RECHECK_RATELIMIT_TIMEOUT = 30 * GLib.USEC_PER_SEC;

const HelperDBusInterface = '<node> \
<interface name="org.gnome.Shell.PortalHelper"> \
<method name="Authenticate"> \
    <arg type="o" direction="in" name="connection" /> \
    <arg type="s" direction="in" name="url" /> \
    <arg type="u" direction="in" name="timestamp" /> \
</method> \
<method name="Close"> \
    <arg type="o" direction="in" name="connection" /> \
</method> \
<method name="Refresh"> \
    <arg type="o" direction="in" name="connection" /> \
</method> \
<signal name="Done"> \
    <arg type="o" name="connection" /> \
    <arg type="u" name="result" /> \
</signal> \
</interface> \
</node>';

const PortalWindow = new Lang.Class({
    Name: 'PortalWindow',
    Extends: Gtk.ApplicationWindow,

    _init: function(application, url, timestamp, doneCallback) {
        this.parent({ application: application });

        if (url) {
            this._uri = new Soup.URI(uri);
        } else {
            url = 'http://www.gnome.org';
            this._uri = null;
            this._everSeenRedirect = false;
        }
        this._originalUrl = url;
        this._doneCallback = doneCallback;
        this._lastRecheck = 0;
        this._recheckAtExit = false;

        this._webView = new WebKit.WebView();
        this._webView.connect('decide-policy', Lang.bind(this, this._onDecidePolicy));
        this._webView.load_uri(url);
        this._webView.connect('notify::title', Lang.bind(this, this._syncTitle));
        this._syncTitle();

        this.add(this._webView);
        this._webView.show();
        this.maximize();
        this.present_with_time(timestamp);
    },

    _syncTitle: function() {
        let title = this._webView.title;

        if (title) {
            this.title = title;
        } else {
            // TRANSLATORS: this is the title of the wifi captive portal login
            // window, until we know the title of the actual login page
            this.title = _("Web Authentication Redirect");
        }
    },

    refresh: function() {
        this._everSeenRedirect = false;
        this._webView.load_uri(this._originalUrl);
    },

    vfunc_delete_event: function(event) {
        if (this._recheckAtExit)
            this._doneCallback(PortalHelperResult.RECHECK);
        else
            this._doneCallback(PortalHelperResult.CANCELLED);
        return false;
    },

    _onDecidePolicy: function(view, decision, type) {
        if (type == WebKit.PolicyDecisionType.NEW_WINDOW_ACTION) {
            decision.ignore();
            return true;
        }

        if (type != WebKit.PolicyDecisionType.NAVIGATION_ACTION)
            return false;

        let request = decision.get_request();
        let uri = new Soup.URI(request.get_uri());

        if (this._uri != null) {
            if (!uri.host_equal(uri, this._uri)) {
                // We *may* have finished here, but we don't know for
                // sure. Tell gnome-shell to run another connectivity check
                // (but ratelimit the checks, we don't want to spam
                // gnome.org for portals that have 10 or more internal
                // redirects - and unfortunately they exist)
                // If we hit the rate limit, we also queue a recheck
                // when the window is closed, just in case we miss the
                // final check and don't realize we're connected
                // This should not be a problem in the cancelled logic,
                // because if the user doesn't want to start the login,
                // we should not see any redirect at all, outside this._uri

                let now = GLib.get_monotonic_time();
                let shouldRecheck = (now - this._lastRecheck) >
                    CONNECTIVITY_RECHECK_RATELIMIT_TIMEOUT;

                if (shouldRecheck) {
                    this._lastRecheck = now;
                    this._recheckAtExit = false;
                    this._doneCallback(PortalHelperResult.RECHECK);
                } else {
                    this._recheckAtExit = true;
                }
            }

            // Update the URI, in case of chained redirects, so we still
            // think we're doing the login until gnome-shell kills us
            this._uri = uri;
        } else {
            if (uri.get_host() == 'www.gnome.org' && this._everSeenRedirect) {
                // Yay, we got to gnome!
                decision.ignore();
                this._doneCallback(PortalHelperResult.COMPLETED);
                return true;
            } else if (uri.get_host() != 'www.gnome.org') {
                this._everSeenRedirect = true;
            }
        }

        decision.use();
        return true;
    },
});

const WebPortalHelper = new Lang.Class({
    Name: 'WebPortalHelper',
    Extends: Gtk.Application,

    _init: function() {
        this.parent({ application_id: 'org.gnome.Shell.PortalHelper',
                      flags: Gio.ApplicationFlags.IS_SERVICE,
                      inactivity_timeout: 30000 });

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(HelperDBusInterface, this);
        this._queue = [];
    },

    vfunc_dbus_register: function(connection, path) {
        this._dbusImpl.export(connection, path);
        this.parent(connection, path);
        return true;
    },

    vfunc_dbus_unregister: function(connection, path) {
        this._dbusImpl.unexport_from_connection(connection);
        this.parent(connection, path);
    },

    vfunc_activate: function() {
        // If launched manually (for example for testing), force a dummy authentication
        // session with the default url
        this.Authenticate('/org/gnome/dummy', '', 0);
    },

    Authenticate: function(connection, url, timestamp) {
        this._queue.push({ connection: connection, url: url, timestamp: timestamp });

        this._processQueue();
    },

    Close: function(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection == connection) {
                if (obj.window)
                    obj.window.destroy();
                this._queue.splice(i, 1);
                break;
            }
        }

        this._processQueue();
    },

    Refresh: function(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection == connection) {
                if (obj.window)
                    obj.window.refresh();
                break;
            }
        }
    },

    _processQueue: function() {
        if (this._queue.length == 0)
            return;

        let top = this._queue[0];
        if (top.window != null)
            return;

        top.window = new PortalWindow(this, top.uri, top.timestamp, Lang.bind(this, function(result) {
            this._dbusImpl.emit_signal('Done', new GLib.Variant('(ou)', [top.connection, result]));
        }));
    },
});

function initEnvironment() {
    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    Gettext.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Gettext.textdomain(Config.GETTEXT_PACKAGE);

    let app = new WebPortalHelper();
    return app.run(argv);
}
