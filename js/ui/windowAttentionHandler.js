/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Meta = imports.gi.Meta;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

function WindowAttentionHandler() {
    this._init();
}

WindowAttentionHandler.prototype = {
    _init : function() {
        let display = global.screen.get_display();
        display.connect('window-demands-attention', Lang.bind(this, this._onWindowDemandsAttention));
        let tracker = Shell.WindowTracker.get_default();
        this._startupIds = {};
        tracker.connect('startup-sequence-changed', Lang.bind(this, this._onStartupSequenceChanged));
    },

    _onStartupSequenceChanged : function(tracker) {
        let sequences = tracker.get_startup_sequences();
        this._startupIds = {};
        for(let i = 0; i < sequences.length; i++) {
            this._startupIds[sequences[i].get_id()] = true;
        }
    },

    _sourceId : function(appId) {
        return 'attention-' + appId;
    },

    _getTitle : function(app, window) {
        if (this._startupIds[window.get_startup_id()])
            return app.get_name();
        else
            return window.title;
    },

    _getBanner : function(app, window) {
        if (this._startupIds[window.get_startup_id()])
            return _("%s has finished starting").format(app.get_name());
        else
            return _("'%s' is ready").format(window.title);
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

        let tracker = Shell.WindowTracker.get_default();
        let app = tracker.get_window_app(window);

        let source = Main.messageTray.getSource(this._sourceId(app.get_id()));
        if (source == null) {
            source = new Source(this._sourceId(app.get_id()), app, window);
            Main.messageTray.add(source);
            source.connect('clicked', Lang.bind(this, function() { source.destroy(); }));
        }

        let notification = new MessageTray.Notification(source, this._getTitle(app, window), this._getBanner(app, window), true);
        source.notify(notification);

        window.connect('notify::title', Lang.bind(this, function(win) {
                                                            notification.update(this._getTitle(app, win), this._getBanner(app, win), false);
                                                        }));
        window.connect('notify::demands-attention', Lang.bind(this, function() { source.destroy() }));
        window.connect('focus', Lang.bind(this, function() { source.destroy() }));
        window.connect('unmanaged', Lang.bind(this, function() { source.destroy() }));

    }
}

function Source(sourceId, app, window) {
    this._init(sourceId, app, window);
}

Source.prototype = {
    __proto__ : MessageTray.Source.prototype,

    _init: function(sourceId, app, window) {
        MessageTray.Source.prototype._init.call(this, sourceId);
        this._window = window;
        this._app = app;
    },

    createIcon : function(size) {
        return this._app.create_icon_texture(size);
    },

    clicked : function() {
        Main.activateWindow(this._window);
        MessageTray.Source.prototype.clicked.call(this);
    }

}
