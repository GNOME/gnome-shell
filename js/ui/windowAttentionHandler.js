// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WindowAttentionHandler */

const { GObject, Shell } = imports.gi;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

var WindowAttentionHandler = class {
    constructor() {
        this._tracker = Shell.WindowTracker.get_default();
        global.display.connectObject(
            'window-demands-attention', this._onWindowDemandsAttention.bind(this),
            'window-marked-urgent', this._onWindowDemandsAttention.bind(this),
            this);
    }

    _getTitleAndBanner(app, window) {
        let title = app.get_name();
        let banner = _("“%s” is ready").format(window.get_title());
        return [title, banner];
    }

    _onWindowDemandsAttention(display, window) {
        // We don't want to show the notification when the window is already focused,
        // because this is rather pointless.
        // Some apps (like GIMP) do things like setting the urgency hint on the
        // toolbar windows which would result into a notification even though GIMP itself is
        // focused.
        // We are just ignoring the hint on skip_taskbar windows for now.
        // (Which is the same behaviour as with metacity + panel)

        if (!window || window.has_focus() || window.is_skip_taskbar())
            return;

        let app = this._tracker.get_window_app(window);
        let source = new WindowAttentionSource(app, window);
        Main.messageTray.add(source);

        let [title, banner] = this._getTitleAndBanner(app, window);

        let notification = new MessageTray.Notification(source, title, banner);
        notification.connect('activated', () => {
            source.open();
        });
        notification.setForFeedback(true);

        source.showNotification(notification);

        window.connectObject('notify::title', () => {
            [title, banner] = this._getTitleAndBanner(app, window);
            notification.update(title, banner);
        }, source);
    }
};

var WindowAttentionSource = GObject.registerClass(
class WindowAttentionSource extends MessageTray.Source {
    _init(app, window) {
        this._window = window;
        this._app = app;

        super._init(app.get_name());

        this._window.connectObject(
            'notify::demands-attention', this._sync.bind(this),
            'notify::urgent', this._sync.bind(this),
            'focus', () => this.destroy(),
            'unmanaged', () => this.destroy(), this);
    }

    _sync() {
        if (this._window.demands_attention || this._window.urgent)
            return;
        this.destroy();
    }

    _createPolicy() {
        if (this._app && this._app.get_app_info()) {
            let id = this._app.get_id().replace(/\.desktop$/, '');
            return new MessageTray.NotificationApplicationPolicy(id);
        } else {
            return new MessageTray.NotificationGenericPolicy();
        }
    }

    createIcon(size) {
        return this._app.create_icon_texture(size);
    }

    destroy(params) {
        this._window.disconnectObject(this);

        super.destroy(params);
    }

    open() {
        Main.activateWindow(this._window);
    }
});
