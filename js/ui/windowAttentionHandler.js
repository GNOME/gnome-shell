// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

var WindowAttentionHandler = new Lang.Class({
    Name: 'WindowAttentionHandler',

    _init () {
        this._tracker = Shell.WindowTracker.get_default();
        this._windowDemandsAttentionId = global.display.connect('window-demands-attention',
                                                                Lang.bind(this, this._onWindowDemandsAttention));
    },

    _getTitleAndBanner(app, window) {
        let title = app.get_name();
        let banner = _("“%s” is ready").format(window.get_title());
        return [title, banner]
    },

    _onWindowDemandsAttention (display, window) {
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
        let source = new Source(app, window);
        Main.messageTray.add(source);

        let [title, banner] = this._getTitleAndBanner(app, window);

        let notification = new MessageTray.Notification(source, title, banner);
        notification.connect('activated', () => {
            source.open();
        });
        notification.setForFeedback(true);

        source.notify(notification);

        source.signalIDs.push(window.connect('notify::title', () => {
            let [title, banner] = this._getTitleAndBanner(app, window);
            notification.update(title, banner);
        }));
    }
});

var Source = new Lang.Class({
    Name: 'WindowAttentionSource',
    Extends: MessageTray.Source,

    _init(app, window) {
        this._window = window;
        this._app = app;

        this.parent(app.get_name());

        this.signalIDs = [];
        this.signalIDs.push(this._window.connect('notify::demands-attention',
                                                 () => { this.destroy(); }));
        this.signalIDs.push(this._window.connect('focus',
                                                 () => { this.destroy(); }));
        this.signalIDs.push(this._window.connect('unmanaged',
                                                 () => { this.destroy(); }));

        this.connect('destroy', Lang.bind(this, this._onDestroy));
    },

    _onDestroy () {
        for(let i = 0; i < this.signalIDs.length; i++) {
           this._window.disconnect(this.signalIDs[i]);
        }
        this.signalIDs = [];
    },

    _createPolicy() {
        if (this._app && this._app.get_app_info()) {
            let id = this._app.get_id().replace(/\.desktop$/,'');
            return new MessageTray.NotificationApplicationPolicy(id);
        } else {
            return new MessageTray.NotificationGenericPolicy();
        }
    },

    createIcon (size) {
        return this._app.create_icon_texture(size);
    },

    open() {
        Main.activateWindow(this._window);
        this.destroy();
    }
});
