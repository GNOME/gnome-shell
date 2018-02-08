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

const PortalHelperSecurityLevel = {
    NOT_YET_DETERMINED: 0,
    SECURE: 1,
    INSECURE: 2
};

const INACTIVITY_TIMEOUT = 30000; //ms
const CONNECTIVITY_CHECK_HOST = 'nmcheck.gnome.org';
const CONNECTIVITY_CHECK_URI = 'http://' + CONNECTIVITY_CHECK_HOST;
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

var PortalHeaderBar = new Lang.Class({
    Name: 'PortalHeaderBar',
    Extends: Gtk.HeaderBar,

    _init() {
        this.parent({ show_close_button: true });

        // See ephy-title-box.c in epiphany for the layout
        let vbox = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL,
                                 spacing: 0 });
        this.set_custom_title(vbox);

        /* TRANSLATORS: this is the title of the wifi captive portal login window */
        let titleLabel = new Gtk.Label({ label: _("Hotspot Login"),
                                         wrap: false,
                                         single_line_mode: true,
                                         ellipsize: Pango.EllipsizeMode.END });
        titleLabel.get_style_context().add_class('title');
        vbox.add(titleLabel);

        let hbox = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL,
                                 spacing: 4,
                                 halign: Gtk.Align.CENTER,
                                 valign: Gtk.Align.BASELINE });
        hbox.get_style_context().add_class('subtitle');
        vbox.add(hbox);

        this._lockImage = new Gtk.Image({ icon_size: Gtk.IconSize.MENU,
                                          valign: Gtk.Align.BASELINE });
        hbox.add(this._lockImage);

        this.subtitleLabel = new Gtk.Label({ wrap: false,
                                             single_line_mode: true,
                                             ellipsize: Pango.EllipsizeMode.END,
                                             valign: Gtk.Align.BASELINE,
                                             selectable: true});
        this.subtitleLabel.get_style_context().add_class('subtitle');
        hbox.add(this.subtitleLabel);

        vbox.show_all();
    },

    setSubtitle(label) {
        this.subtitleLabel.set_text(label);
    },

    setSecurityIcon(securityLevel) {
        switch (securityLevel) {
        case PortalHelperSecurityLevel.NOT_YET_DETERMINED:
            this._lockImage.hide();
            break;
        case PortalHelperSecurityLevel.SECURE:
            this._lockImage.show();
            this._lockImage.set_from_icon_name("channel-secure-symbolic", Gtk.IconSize.MENU);
            this._lockImage.set_tooltip_text(null);
            break;
        case PortalHelperSecurityLevel.INSECURE:
            this._lockImage.show();
            this._lockImage.set_from_icon_name("channel-insecure-symbolic", Gtk.IconSize.MENU);
            this._lockImage.set_tooltip_text(_('Your connection to this hotspot login is not secure. Passwords or other information you enter on this page can be viewed by people nearby.'));
            break;
        }
    },
});

var PortalWindow = new Lang.Class({
    Name: 'PortalWindow',
    Extends: Gtk.ApplicationWindow,

    _init(application, url, timestamp, doneCallback) {
        this.parent({ application: application });

        this.connect('delete-event', this.destroyWindow.bind(this));
        this._headerBar = new PortalHeaderBar();
        this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.NOT_YET_DETERMINED);
        this.set_titlebar(this._headerBar);
        this._headerBar.show();

        if (!url) {
            url = CONNECTIVITY_CHECK_URI;
            this._originalUrlWasGnome = true;
        } else {
            this._originalUrlWasGnome = false;
        }
        this._uri = new Soup.URI(url);
        this._everSeenRedirect = false;
        this._originalUrl = url;
        this._doneCallback = doneCallback;
        this._lastRecheck = 0;
        this._recheckAtExit = false;

        this._webContext = WebKit.WebContext.new_ephemeral();
        this._webContext.set_cache_model(WebKit.CacheModel.DOCUMENT_VIEWER);
        this._webContext.set_network_proxy_settings(WebKit.NetworkProxyMode.NO_PROXY, null);

        this._webView = WebKit.WebView.new_with_context(this._webContext);
        this._webView.connect('decide-policy', this._onDecidePolicy.bind(this));
        this._webView.connect('load-changed', this._onLoadChanged.bind(this));
        this._webView.connect('insecure-content-detected', this._onInsecureContentDetected.bind(this));
        this._webView.connect('load-failed-with-tls-errors', this._onLoadFailedWithTlsErrors.bind(this));
        this._webView.load_uri(url);
        this._webView.connect('notify::uri', this._syncUri.bind(this));
        this._syncUri();

        this.add(this._webView);
        this._webView.show();
        this.set_size_request(600, 450);
        this.maximize();
        this.present_with_time(timestamp);

        this.application.set_accels_for_action('app.quit', ['<Primary>q', '<Primary>w']);
    },

    destroyWindow() {
        this.destroy();
    },

    _syncUri() {
        let uri = this._webView.uri;
        if (uri)
            this._headerBar.setSubtitle(GLib.uri_unescape_string(uri, null));
        else
            this._headerBar.setSubtitle('');
    },

    refresh() {
        this._everSeenRedirect = false;
        this._webView.load_uri(this._originalUrl);
    },

    vfunc_delete_event(event) {
        if (this._recheckAtExit)
            this._doneCallback(PortalHelperResult.RECHECK);
        else
            this._doneCallback(PortalHelperResult.CANCELLED);
        return false;
    },

    _onLoadChanged(view, loadEvent) {
        if (loadEvent == WebKit.LoadEvent.STARTED) {
            this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.NOT_YET_DETERMINED);
        } else if (loadEvent == WebKit.LoadEvent.COMMITTED) {
            let tlsInfo = this._webView.get_tls_info();
            let ret = tlsInfo[0];
            let flags = tlsInfo[2];
            if (ret && flags == 0)
                this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.SECURE);
            else
                this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
        }
    },

    _onInsecureContentDetected () {
        this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
    },

    _onLoadFailedWithTlsErrors (view, failingURI, certificate, errors) {
        this._headerBar.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
        let uri = new Soup.URI(failingURI);
        this._webContext.allow_tls_certificate_for_host(certificate, uri.get_host());
        this._webView.load_uri(failingURI);
        return true;
    },

    _onDecidePolicy(view, decision, type) {
        if (type == WebKit.PolicyDecisionType.NEW_WINDOW_ACTION) {
            let navigationAction = decision.get_navigation_action();
            if (navigationAction.is_user_gesture()) {
                // Even though the portal asks for a new window,
                // perform the navigation in the current one. Some
                // portals open a window as their last login step and
                // ignoring that window causes them to not let the
                // user go through. We don't risk popups taking over
                // the page because we check that the navigation is
                // user initiated.
                this._webView.load_request(navigationAction.get_request());
            }

            decision.ignore();
            return true;
        }

        if (type != WebKit.PolicyDecisionType.NAVIGATION_ACTION)
            return false;

        let request = decision.get_request();
        let uri = new Soup.URI(request.get_uri());

        if (!uri.host_equal(this._uri) && this._originalUrlWasGnome) {
            if (uri.get_host() == CONNECTIVITY_CHECK_HOST && this._everSeenRedirect) {
                // Yay, we got to gnome!
                decision.ignore();
                this._doneCallback(PortalHelperResult.COMPLETED);
                return true;
            } else if (uri.get_host() != CONNECTIVITY_CHECK_HOST) {
                this._everSeenRedirect = true;
            }
        }

        // We *may* have finished here, but we don't know for
        // sure. Tell gnome-shell to run another connectivity check
        // (but ratelimit the checks, we don't want to spam
        // nmcheck.gnome.org for portals that have 10 or more internal
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

        // Update the URI, in case of chained redirects, so we still
        // think we're doing the login until gnome-shell kills us
        this._uri = uri;

        decision.use();
        return true;
    },
});

var WebPortalHelper = new Lang.Class({
    Name: 'WebPortalHelper',
    Extends: Gtk.Application,

    _init() {
        this.parent({ application_id: 'org.gnome.Shell.PortalHelper',
                      flags: Gio.ApplicationFlags.IS_SERVICE,
                      inactivity_timeout: 30000 });

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(HelperDBusInterface, this);
        this._queue = [];

        let action = new Gio.SimpleAction({ name: 'quit' });
        action.connect('activate', () => { this.active_window.destroyWindow(); });
        this.add_action(action);
    },

    vfunc_dbus_register(connection, path) {
        this._dbusImpl.export(connection, path);
        this.parent(connection, path);
        return true;
    },

    vfunc_dbus_unregister(connection, path) {
        this._dbusImpl.unexport_from_connection(connection);
        this.parent(connection, path);
    },

    vfunc_activate() {
        // If launched manually (for example for testing), force a dummy authentication
        // session with the default url
        this.Authenticate('/org/gnome/dummy', '', 0);
    },

    Authenticate(connection, url, timestamp) {
        this._queue.push({ connection: connection, url: url, timestamp: timestamp });

        this._processQueue();
    },

    Close(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection == connection) {
                if (obj.window)
                    obj.window.destroyWindow();
                this._queue.splice(i, 1);
                break;
            }
        }

        this._processQueue();
    },

    Refresh(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection == connection) {
                if (obj.window)
                    obj.window.refresh();
                break;
            }
        }
    },

    _processQueue() {
        if (this._queue.length == 0)
            return;

        let top = this._queue[0];
        if (top.window != null)
            return;

        top.window = new PortalWindow(this, top.url, top.timestamp, result => {
            this._dbusImpl.emit_signal('Done', new GLib.Variant('(ou)', [top.connection, result]));
        });
    },
});

function initEnvironment() {
    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    if (!WebKit.WebContext.new_ephemeral) {
        log('WebKitGTK 2.16 is required for the portal-helper, see https://bugzilla.gnome.org/show_bug.cgi?id=780453');
        return 1;
    }

    Gettext.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Gettext.textdomain(Config.GETTEXT_PACKAGE);

    let app = new WebPortalHelper();
    return app.run(argv);
}
