/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Signals = imports.signals;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const AppFavorites = imports.ui.appFavorites;
const DND = imports.ui.dnd;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Workspace = imports.ui.workspace;


function RemoveFavoriteIcon() {
    this._init();
}

RemoveFavoriteIcon.prototype = {
    _init: function() {
        this.actor = new St.Bin({ style_class: 'remove-favorite' });
        this._iconActor = null;
        this.icon = new IconGrid.BaseIcon(_("Remove"),
                                           { setSizeManually: true,
                                             createIcon: Lang.bind(this, this._createIcon) });
        this.actor.set_child(this.icon.actor);
        this.actor._delegate = this;
    },

    _createIcon: function(size) {
        this._iconActor = new St.Icon({ icon_name: 'user-trash',
                                        style_class: 'remove-favorite-icon',
                                        icon_size: size });
        return this._iconActor;
    },

    setHover: function(hovered) {
        this.actor.set_hover(hovered);
        if (this._iconActor)
            this._iconActor.set_hover(hovered);
    },

    // Rely on the dragged item being a favorite
    handleDragOver: function(source, actor, x, y, time) {
        return DND.DragMotionResult.MOVE_DROP;
    },

    acceptDrop: function(source, actor, x, y, time) {
        let app = null;
        if (source instanceof AppDisplay.AppWellIcon) {
            let appSystem = Shell.AppSystem.get_default();
            app = appSystem.get_app(source.getId());
        } else if (source instanceof Workspace.WindowClone) {
            let tracker = Shell.WindowTracker.get_default();
            app = tracker.get_window_app(source.metaWindow);
        }

        let id = app.get_id();

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this,
            function () {
                AppFavorites.getAppFavorites().removeFavorite(id);
                return false;
            }));

        return true;
    }
};


function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        this._menus = [];
        this._menuDisplays = [];
        this._maxHeight = -1;
        this._iconSize = 48;

        this._dragPlaceholder = null;
        this._dragPlaceholderPos = -1;
        this._favRemoveTarget = null;

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

    show: function() {
        this._itemDragBeginId = Main.overview.connect('item-drag-begin',
            Lang.bind(this, this._onDragBegin));
        this._itemDragEndId = Main.overview.connect('item-drag-end',
            Lang.bind(this, this._onDragEnd));
        this._windowDragBeginId = Main.overview.connect('window-drag-begin',
            Lang.bind(this, this._onDragBegin));
        this._windowDragEndId = Main.overview.connect('window-drag-end',
            Lang.bind(this, this._onDragEnd));
    },

    hide: function() {
        Main.overview.disconnect(this._itemDragBeginId);
        Main.overview.disconnect(this._itemDragEndId);
        Main.overview.disconnect(this._windowDragBeginId);
        Main.overview.disconnect(this._windowDragEndId);
    },

    _onDragBegin: function() {
        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };
        DND.addDragMonitor(this._dragMonitor);
    },

    _onDragEnd: function() {
        this._clearDragPlaceholder();
        if (this._favRemoveTarget) {
            this._favRemoveTarget.actor.destroy();
            this._favRemoveTarget = null;
        }
        DND.removeMonitor(this._dragMonitor);
    },

    _onDragMotion: function(dragEvent) {
        let app = null;
        if (dragEvent.source instanceof AppDisplay.AppWellIcon)
            app = this._appSystem.get_app(dragEvent.source.getId());
        else if (dragEvent.source instanceof Workspace.WindowClone)
            app = this._tracker.get_window_app(dragEvent.source.metaWindow);
        else
            return DND.DragMotionResult.CONTINUE;

        let id = app.get_id();

        let favorites = AppFavorites.getAppFavorites().getFavoriteMap();

        let srcIsFavorite = (id in favorites);

        if (srcIsFavorite && this._favRemoveTarget == null) {
                this._favRemoveTarget = new RemoveFavoriteIcon();
                this._favRemoveTarget.icon.setIconSize(this._iconSize);
                this._box.add(this._favRemoveTarget.actor);
        }

        let favRemoveHovered = false;
        if (this._favRemoveTarget)
            favRemoveHovered =
                this._favRemoveTarget.actor.contains(dragEvent.targetActor);

        if (!this._box.contains(dragEvent.targetActor) || favRemoveHovered)
            this._clearDragPlaceholder();

        if (this._favRemoveTarget)
            this._favRemoveTarget.setHover(favRemoveHovered);

        return DND.DragMotionResult.CONTINUE;
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

    _addApp: function(app) {
        let display = new AppDisplay.AppWellIcon(app,
                                                 { setSizeManually: true });
        display._draggable.connect('drag-begin',
                                   Lang.bind(this, function() {
                                       display.actor.opacity = 50;
                                   }));
        display._draggable.connect('drag-end',
                                   Lang.bind(this, function() {
                                       display.actor.opacity = 255;
                                   }));
        this._box.add(display.actor);
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
            this._addApp(app);
        }

        for (let i = 0; i < running.length; i++) {
            let app = running[i];
            if (app.get_id() in favorites)
                continue;
            this._addApp(app);
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

    _clearDragPlaceholder: function() {
        if (this._dragPlaceholder) {
            this._dragPlaceholder.destroy();
            this._dragPlaceholder = null;
            this._dragPlaceholderPos = -1;
        }
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

        let favorites = AppFavorites.getAppFavorites().getFavorites();
        let numFavorites = favorites.length;

        let favPos = favorites.indexOf(app);

        let numChildren = this._box.get_children().length;
        let boxHeight = this._box.height;

        // Keep the placeholder out of the index calculation; assuming that
        // the remove target has the same size as "normal" items, we don't
        // need to do the same adjustment there.
        if (this._dragPlaceholder) {
            boxHeight -= this._dragPlaceholder.height;
            numChildren--;
        }

        let pos = Math.round(y * numChildren / boxHeight);

        if (pos != this._dragPlaceholderPos && pos <= numFavorites) {
            this._dragPlaceholderPos = pos;
            if (this._dragPlaceholder)
                this._dragPlaceholder.destroy();

            // Don't allow positioning before or after self
            if (favPos != -1 && (pos == favPos || pos == favPos + 1))
                return DND.DragMotionResult.CONTINUE;

            this._dragPlaceholder = new St.Bin({ style_class: 'dash-placeholder' });
            this._box.insert_actor(this._dragPlaceholder, pos);
        }

        let srcIsFavorite = (favPos != -1);

        if (srcIsFavorite)
            return DND.DragMotionResult.MOVE_DROP;

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

        let favPos = 0;
        let children = this._box.get_children();
        for (let i = 0; i < this._dragPlaceholderPos; i++) {
            let childId = children[i]._delegate.app.get_id();
            if (childId == id)
                continue;
            if (childId in favorites)
                favPos++;
        }

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this,
            function () {
                let appFavorites = AppFavorites.getAppFavorites();
                if (srcIsFavorite)
                    appFavorites.moveFavoriteToPos(id, favPos);
                else
                    appFavorites.addFavoriteAtPos(id, favPos);
                return false;
            }));

        return true;
    }
};

Signals.addSignalMethods(Dash.prototype);
