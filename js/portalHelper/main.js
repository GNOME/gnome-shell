import Adw from 'gi://Adw?version=1';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Gtk from 'gi://Gtk?version=4.0';
import WebKit from 'gi://WebKit?version=6.0';

import * as Gettext from 'gettext';
import {programInvocationName, programArgs} from 'system';

const _ = Gettext.gettext;

import * as Config from '../misc/config.js';
import {loadInterfaceXML} from '../misc/fileUtils.js';

const PortalHelperResult = {
    CANCELLED: 0,
    COMPLETED: 1,
    RECHECK: 2,
};

const PortalHelperSecurityLevel = {
    NOT_YET_DETERMINED: 0,
    SECURE: 1,
    INSECURE: 2,
};

const HTTP_URI_FLAGS =
    GLib.UriFlags.HAS_PASSWORD |
    GLib.UriFlags.ENCODED_PATH |
    GLib.UriFlags.ENCODED_QUERY |
    GLib.UriFlags.ENCODED_FRAGMENT |
    GLib.UriFlags.SCHEME_NORMALIZE |
    GLib.UriFlags.PARSE_RELAXED;

const CONNECTIVITY_CHECK_HOST = 'nmcheck.gnome.org';
const CONNECTIVITY_CHECK_URI = `http://${CONNECTIVITY_CHECK_HOST}`;
const CONNECTIVITY_RECHECK_RATELIMIT_TIMEOUT = 5 * GLib.USEC_PER_SEC;

const HelperDBusInterface = loadInterfaceXML('org.gnome.Shell.PortalHelper');

const PortalSecurityButton = GObject.registerClass(
class PortalSecurityButton extends Gtk.MenuButton {
    _init() {
        const popover = new Gtk.Popover();

        super._init({
            popover,
            visible: false,
        });

        const vbox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            margin_top: 6,
            margin_bottom: 6,
            margin_start: 6,
            margin_end: 6,
            spacing: 6,
        });
        popover.set_child(vbox);

        const hbox = new Gtk.Box({
            orientation: Gtk.Orientation.HORIZONTAL,
            halign: Gtk.Align.CENTER,
        });
        vbox.append(hbox);

        this._secureIcon = new Gtk.Image();
        hbox.append(this._secureIcon);

        this._secureIcon.bind_property('icon-name',
            this, 'icon-name',
            GObject.BindingFlags.DEFAULT);

        this._titleLabel = new Gtk.Label();
        this._titleLabel.add_css_class('title');
        hbox.append(this._titleLabel);

        this._descriptionLabel = new Gtk.Label({
            wrap: true,
            max_width_chars: 32,
        });
        vbox.append(this._descriptionLabel);
    }

    setPopoverTitle(label) {
        this._titleLabel.set_text(label);
    }

    setSecurityIcon(securityLevel) {
        switch (securityLevel) {
        case PortalHelperSecurityLevel.NOT_YET_DETERMINED:
            this.hide();
            break;
        case PortalHelperSecurityLevel.SECURE:
            this.show();
            this._secureIcon.icon_name = 'channel-secure-symbolic';
            this._descriptionLabel.label = _('Your connection seems to be secure');
            break;
        case PortalHelperSecurityLevel.INSECURE:
            this.show();
            this._secureIcon.icon_name = 'channel-insecure-symbolic';
            this._descriptionLabel.label =
                _('Your connection to this hotspot login is not secure. Passwords or other information you enter on this page can be viewed by people nearby.');
            break;
        }
    }
});

const PortalWindow = GObject.registerClass(
class PortalWindow extends Gtk.ApplicationWindow {
    _init(application, url, timestamp, statusChangedCallback) {
        super._init({
            application,
            title: _('Hotspot Login'),
            default_width: 600,
            default_height: 450,
        });

        const headerbar = new Gtk.HeaderBar();
        this._secureMenu = new PortalSecurityButton();
        headerbar.pack_start(this._secureMenu);

        this.set_titlebar(headerbar);

        if (!url) {
            url = CONNECTIVITY_CHECK_URI;
            this._originalUrlWasGnome = true;
        } else {
            this._originalUrlWasGnome = false;
        }
        this._uri = GLib.Uri.parse(url, HTTP_URI_FLAGS);
        this._everSeenRedirect = false;
        this._originalUrl = url;
        this._statusChangedCallback = statusChangedCallback;
        this._lastRecheck = 0;
        this._recheckTimeoutId = 0;

        this._networkSession = WebKit.NetworkSession.new_ephemeral();
        this._networkSession.set_proxy_settings(WebKit.NetworkProxyMode.NO_PROXY, null);

        this._webContext = new WebKit.WebContext();
        this._webContext.set_cache_model(WebKit.CacheModel.DOCUMENT_VIEWER);

        this._webView = new WebKit.WebView({
            networkSession: this._networkSession,
            webContext: this._webContext,
        });
        this._webView.connect('decide-policy', this._onDecidePolicy.bind(this));
        this._webView.connect('load-changed', this._onLoadChanged.bind(this));
        this._webView.connect('insecure-content-detected', this._onInsecureContentDetected.bind(this));
        this._webView.connect('load-failed-with-tls-errors', this._onLoadFailedWithTlsErrors.bind(this));
        this._webView.load_uri(url);
        this._webView.connect('notify::uri', this._syncUri.bind(this));
        this._syncUri();

        this.set_child(this._webView);
        this.maximize();
        this.present_with_time(timestamp);

        this.application.set_accels_for_action('app.quit', ['<Primary>q', '<Primary>w']);
    }

    _syncUri() {
        const {uri} = this._webView;

        try {
            const [, , host] = GLib.Uri.split_network(uri, HTTP_URI_FLAGS);
            this._secureMenu.setPopoverTitle(host);
        } catch (e) {
            if (uri != null)
                console.error(`Failed to parse Uri ${uri}: ${e.message}`);
            this._secureMenu.setPopoverTitle('');
        }
    }

    refresh() {
        this._everSeenRedirect = false;
        this._webView.load_uri(this._originalUrl);
    }

    vfunc_close_request() {
        if (this._recheckTimeoutId) {
            this._statusChangedCallback(PortalHelperResult.RECHECK);
            GLib.source_remove(this._recheckTimeoutId);
            this._recheckTimeoutId = 0;
        }
        this._statusChangedCallback(PortalHelperResult.CANCELLED);
        return false;
    }

    _onLoadChanged(view, loadEvent) {
        if (loadEvent === WebKit.LoadEvent.STARTED) {
            this._secureMenu.setSecurityIcon(PortalHelperSecurityLevel.NOT_YET_DETERMINED);
        } else if (loadEvent === WebKit.LoadEvent.COMMITTED) {
            let tlsInfo = this._webView.get_tls_info();
            let ret = tlsInfo[0];
            let flags = tlsInfo[2];
            if (ret && flags === 0)
                this._secureMenu.setSecurityIcon(PortalHelperSecurityLevel.SECURE);
            else
                this._secureMenu.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
        }
    }

    _onInsecureContentDetected() {
        this._secureMenu.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
    }

    _onLoadFailedWithTlsErrors(view, failingURI, certificate, _errors) {
        this._secureMenu.setSecurityIcon(PortalHelperSecurityLevel.INSECURE);
        let uri = GLib.Uri.parse(failingURI, HTTP_URI_FLAGS);
        this._networkSession.allow_tls_certificate_for_host(certificate, uri.get_host());
        this._webView.load_uri(failingURI);
        return true;
    }

    _onDecidePolicy(view, decision, type) {
        if (type === WebKit.PolicyDecisionType.RESPONSE)
            return false;

        const navigationAction = decision.get_navigation_action();
        const request = navigationAction.get_request();

        if (type === WebKit.PolicyDecisionType.NEW_WINDOW_ACTION) {
            if (navigationAction.is_user_gesture()) {
                // Even though the portal asks for a new window,
                // perform the navigation in the current one. Some
                // portals open a window as their last login step and
                // ignoring that window causes them to not let the
                // user go through. We don't risk popups taking over
                // the page because we check that the navigation is
                // user initiated.
                this._webView.load_request(request);
            }

            decision.ignore();
            return true;
        }

        if (type !== WebKit.PolicyDecisionType.NAVIGATION_ACTION)
            return false;

        const uri = GLib.Uri.parse(request.get_uri(), HTTP_URI_FLAGS);

        if (uri.get_host() !== this._uri.get_host() && this._originalUrlWasGnome) {
            if (uri.get_host() === CONNECTIVITY_CHECK_HOST && this._everSeenRedirect) {
                // Yay, we got to gnome!
                decision.ignore();
                this._statusChangedCallback(PortalHelperResult.COMPLETED);
                return true;
            } else if (uri.get_host() !== CONNECTIVITY_CHECK_HOST) {
                this._everSeenRedirect = true;
            }
        }

        // We *may* have finished here, but we don't know for
        // sure. Tell gnome-shell to run another connectivity check
        // (but ratelimit the checks, we don't want to spam
        // nmcheck.gnome.org for portals that have 10 or more internal
        // redirects - and unfortunately they exist)

        if (this._recheckTimeoutId === 0) {
            const now = GLib.get_monotonic_time();
            const timeSinceLastRecheck = this._lastRecheck ? now - this._lastRecheck : 0;

            if (timeSinceLastRecheck > CONNECTIVITY_RECHECK_RATELIMIT_TIMEOUT || this._lastRecheck === 0) {
                this._lastRecheck = now;
                this._statusChangedCallback(PortalHelperResult.RECHECK);
            } else {
                const seconds =
                    (CONNECTIVITY_RECHECK_RATELIMIT_TIMEOUT - timeSinceLastRecheck) /
                    GLib.USEC_PER_SEC;
                this._recheckTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, seconds, () => {
                    this._statusChangedCallback(PortalHelperResult.RECHECK);
                    this._recheckTimeoutId = 0;
                    return GLib.SOURCE_REMOVE;
                });
            }
        }

        // Update the URI, in case of chained redirects, so we still
        // think we're doing the login until gnome-shell kills us
        this._uri = uri;

        decision.use();
        return true;
    }
});

const WebPortalHelper = GObject.registerClass(
class WebPortalHelper extends Adw.Application {
    _init() {
        super._init({
            application_id: 'org.gnome.Shell.PortalHelper',
            flags: Gio.ApplicationFlags.IS_SERVICE,
            inactivity_timeout: 30000,
        });

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(HelperDBusInterface, this);
        this._queue = [];

        let action = new Gio.SimpleAction({name: 'quit'});
        action.connect('activate', () => this.active_window.destroy());
        this.add_action(action);
    }

    vfunc_dbus_register(connection, path) {
        this._dbusImpl.export(connection, path);
        super.vfunc_dbus_register(connection, path);
        return true;
    }

    vfunc_dbus_unregister(connection, path) {
        this._dbusImpl.unexport_from_connection(connection);
        super.vfunc_dbus_unregister(connection, path);
    }

    vfunc_activate() {
        // If launched manually (for example for testing), force a dummy authentication
        // session with the default url
        this.Authenticate('/org/gnome/dummy', '', 0);
    }

    Authenticate(connection, url, timestamp) {
        this._queue.push({connection, url, timestamp});

        this._processQueue();
    }

    Close(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection === connection) {
                if (obj.window)
                    obj.window.destroy();
                this._queue.splice(i, 1);
                break;
            }
        }

        this._processQueue();
    }

    Refresh(connection) {
        for (let i = 0; i < this._queue.length; i++) {
            let obj = this._queue[i];

            if (obj.connection === connection) {
                if (obj.window)
                    obj.window.refresh();
                break;
            }
        }
    }

    _processQueue() {
        if (this._queue.length === 0)
            return;

        let top = this._queue[0];
        if (top.window != null)
            return;

        top.window = new PortalWindow(this, top.url, top.timestamp, result => {
            this._dbusImpl.emit_signal('StatusChanged', new GLib.Variant('(ou)', [top.connection, result]));
        });
    }
});

Gettext.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
Gettext.textdomain(Config.GETTEXT_PACKAGE);

const app = new WebPortalHelper();
await app.runAsync([programInvocationName, ...programArgs]);
