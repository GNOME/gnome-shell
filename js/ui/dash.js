/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const AppFavorites = imports.ui.appFavorites;
const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Workspace = imports.ui.workspace;


function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];
        this._maxHeight = -1;

        this._favorites = [];

        this._box = new St.BoxLayout({ name: 'dash',
                                       vertical: true,
                                       clip_to_allocation: true });
        this._box._delegate = this;

        this.actor = new St.Bin({ y_align: St.Align.START, child: this._box });
        this.actor.connect('notify::height', Lang.bind(this,
            function() {
                if (this._maxHeight != this.actor.height)
                    this._queueRedisplay();
                this._maxHeight = this.actor.height;
            }));

        this._workId = Main.initializeDeferredWork(this._box, Lang.bind(this, this._redisplay));

        this._tracker = Shell.WindowTracker.get_default();
        this._appSystem = Shell.AppSystem.get_default();

        this._appSystem.connect('installed-changed', Lang.bind(this, this._queueRedisplay));
        AppFavorites.getAppFavorites().connect('changed', Lang.bind(this, this._queueRedisplay));
        this._tracker.connect('app-state-changed', Lang.bind(this, this._queueRedisplay));
    },

    _appIdListToHash: function(apps) {
        let ids = {};
        for (let i = 0; i < apps.length; i++)
            ids[apps[i].get_id()] = apps[i];
        return ids;
    },

    _queueRedisplay: function () {
        Main.queueDeferredWork(this._workId);
    },

    _redisplay: function () {
        this._box.hide();
        this._box.remove_all();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        /* hardcode here pending some design about how exactly desktop contexts behave */
        let contextId = '';

        let running = this._tracker.get_running_apps(contextId);

        for (let id in favorites) {
            let app = favorites[id];
            let display = new AppDisplay.AppWellIcon(app);
            this._box.add(display.actor);
        }

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            if (app.get_id() in favorites)
                continue;
            let display = new AppDisplay.AppWellIcon(app);
            this._box.add(display.actor);
        }

        let children = this._box.get_children();
        if (children.length == 0) {
            this._box.add_style_pseudo_class('empty');
        } else {
            this._box.remove_style_pseudo_class('empty');

            if (this._maxHeight > -1) {
                let iconSizes = [ 48, 32, 24, 22, 16 ];

                for (let i = 0; i < iconSizes.length; i++) {
                    let minHeight, natHeight;

                    this._iconSize = iconSizes[i];
                    for (let j = 0; j < children.length; j++)
                        children[j]._delegate.icon.setIconSize(this._iconSize);

                    [minHeight, natHeight] = this.actor.get_preferred_height(-1);

                    if (natHeight <= this._maxHeight)
                        break;
                }
            }
        }
        this._box.show();
    },

    handleDragOver : function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplay.AppWellIcon)
            app = this._appSystem.get_app(source.getId());
        else if (source instanceof Workspace.WindowClone)
            app = this._tracker.get_window_app(source.metaWindow);

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient())
            return DND.DragMotionResult.NO_DROP;

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite)
            return DND.DragMotionResult.NO_DROP;

        return DND.DragMotionResult.COPY_DROP;
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplay.AppWellIcon) {
            app = this._appSystem.get_app(source.getId());
        } else if (source instanceof Workspace.WindowClone) {
            app = this._tracker.get_window_app(source.metaWindow);
        }

        // Don't allow favoriting of transient apps
        if (app == null || app.is_transient()) {
            return false;
        }

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite) {
            return false;
        } else {
            Mainloop.idle_add(Lang.bind(this, function () {
                AppFavorites.getAppFavorites().addFavorite(id);
                return false;
            }));
        }

        return true;
    }
};

Signals.addSignalMethods(Dash.prototype);
