// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const WindowAttentionHandler = new Lang.Class({
    Name: 'WindowAttentionHandler',

    _init : function() {
        this._tracker = Shell.WindowTracker.get_default();
        global.display.connect('window-demands-attention', Lang.bind(this, this._onWindowDemandsAttention));
    },

    _getTitleAndBanner: function(app, window) {
        let title = app.get_name();
        let banner = _("'%s' is ready").format(window.get_title());
        return [title, banner]
    },

    _onWindowDemandsAttention : function(display, window) {
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
        notification.connect('clicked', function() {
            source.open();
        });
        notification.setForFeedback(true);

        source.notify(notification);

        source.signalIDs.push(window.connect('notify::title', Lang.bind(this, function() {
            let [title, banner] = this._getTitleAndBanner(app, window);
            notification.update(title, banner);
        })));
    }
});

const Source = new Lang.Class({
    Name: 'WindowAttentionSource',
    Extends: MessageTray.Source,

    _init: function(app, window) {
        this._window = window;
        this._app = app;

        this.parent(app.get_name());

        this.signalIDs = [];
        this.signalIDs.push(this._window.connect('notify::demands-attention', Lang.bind(this, function() { this.destroy(); })));
        this.signalIDs.push(this._window.connect('focus', Lang.bind(this, function() { this.destroy(); })));
        this.signalIDs.push(this._window.connect('unmanaged', Lang.bind(this, function() { this.destroy(); })));

        this.connect('destroy', Lang.bind(this, this._onDestroy));
    },

    _onDestroy : function() {
        for(let i = 0; i < this.signalIDs.length; i++) {
           this._window.disconnect(this.signalIDs[i]);
        }
        this.signalIDs = [];
    },

    createIcon : function(size) {
        return this._app.create_icon_texture(size);
    },

    open: function() {
        Main.activateWindow(this._window);
        this.destroy();
    }
});
