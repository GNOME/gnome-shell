/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Meta = imports.gi.Meta;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const AppDisplay = imports.ui.appDisplay;
const Dash = imports.ui.dash;
const DocDisplay = imports.ui.docDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Panel = imports.ui.panel;
const PlaceDisplay = imports.ui.placeDisplay;
const Tweener = imports.ui.tweener;
const ViewSelector = imports.ui.viewSelector;
const WorkspacesView = imports.ui.workspacesView;

// Time for initial animation going into Overview mode
const ANIMATION_TIME = 0.25;

// We split the screen vertically between the dash and the view selector.
const DASH_SPLIT_FRACTION = 0.1;

const SHELL_INFO_HIDE_TIMEOUT = 10;

function Source() {
    this._init();
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init: function() {
        MessageTray.Source.prototype._init.call(this,
                                                "System Information");
        this._setSummaryIcon(this.createNotificationIcon());
    },

    createNotificationIcon: function() {
        return new St.Icon({ icon_name: 'dialog-information',
                             icon_type: St.IconType.SYMBOLIC,
                             icon_size: this.ICON_SIZE });
    },

    _notificationClicked: function() {
        this.destroy();
    }
}

function ShellInfo() {
    this._init();
}

ShellInfo.prototype = {
    _init: function() {
        this._source = null;
        this._timeoutId = 0;
        this._undoCallback = null;
    },

    _onUndoClicked: function() {
        Mainloop.source_remove(this._timeoutId);
        this._timeoutId = 0;

        if (this._undoCallback)
            this._undoCallback();
        this._undoCallback = null;

        if (this._source)
            this._source.destroy();
    },

    _onTimeout: function() {
        this._timeoutId = 0;
        if (this._source)
            this._source.destroy();
        return false;
    },

    setMessage: function(text, undoCallback, undoLabel) {
        if (this._timeoutId)
            Mainloop.source_remove(this._timeoutId);

        this._timeoutId = Mainloop.timeout_add_seconds(SHELL_INFO_HIDE_TIMEOUT,
                                                       Lang.bind(this, this._onTimeout));

        if (this._source == null) {
            this._source = new Source();
            this._source.connect('destroy', Lang.bind(this,
                function() {
                    this._source = null;
                }));
            Main.messageTray.add(this._source);
        }

        let notification = this._source.notification;
        if (notification == null)
            notification = new MessageTray.Notification(this._source, text, null);
        else
            notification.update(text, null, { clear: true });

        this._undoCallback = undoCallback;
        if (undoCallback) {
            notification.addButton('system-undo',
                                   undoLabel ? undoLabel : _("Undo"));
            notification.connect('action-invoked',
                                 Lang.bind(this, this._onUndoClicked));
        }

        this._source.notify(notification);
    }
};

function Overview() {
    this._init();
}

Overview.prototype = {
    _init : function() {
        // The actual global.background_actor is inside global.window_group,
        // which is hidden when displaying the overview, so we display a clone.
        this._background = new Clutter.Clone({ source: global.background_actor });
        this._background.hide();
        global.overlay_group.add_actor(this._background);

        this._desktopFade = new St.Bin();
        global.overlay_group.add_actor(this._desktopFade);

        this._spacing = 0;

        this._group = new St.Group({ name: 'overview' });
        this._group._delegate = this;
        this._group.connect('style-changed',
            Lang.bind(this, function() {
                let node = this._group.get_theme_node();
                let spacing = node.get_length('spacing');
                if (spacing != this._spacing) {
                    this._spacing = spacing;
                    this.relayout();
                }
            }));

        this.shellInfo = new ShellInfo();

        this._workspacesDisplay = null;

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        // During transitions, we raise this to the top to avoid having the overview
        // area be reactive; it causes too many issues such as double clicks on
        // Dash elements, or mouseover handlers in the workspaces.
        this._coverPane = new Clutter.Rectangle({ opacity: 0,
                                                  reactive: true });
        this._group.add_actor(this._coverPane);
        this._coverPane.connect('event', Lang.bind(this, function (actor, event) { return true; }));


        this._group.hide();
        global.overlay_group.add_actor(this._group);

        this.viewSelector = new ViewSelector.ViewSelector();
        this._group.add_actor(this.viewSelector.actor);

        this._workspacesDisplay = new WorkspacesView.WorkspacesDisplay();
        this.viewSelector.addViewTab(_("Windows"), this._workspacesDisplay.actor);

        let appView = new AppDisplay.AllAppDisplay();
        this.viewSelector.addViewTab(_("Applications"), appView.actor);

        // Default search providers
        this.viewSelector.addSearchProvider(new AppDisplay.AppSearchProvider());
        this.viewSelector.addSearchProvider(new AppDisplay.PrefsSearchProvider());
        this.viewSelector.addSearchProvider(new PlaceDisplay.PlaceSearchProvider());
        this.viewSelector.addSearchProvider(new DocDisplay.DocSearchProvider());

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash.Dash();
        this._group.add_actor(this._dash.actor);
        this._dash.actor.add_constraint(this.viewSelector.constrainY);
        this._dash.actor.add_constraint(this.viewSelector.constrainHeight);

        this._coverPane.lower_bottom();

        this.workspaces = null;
    },

    _getDesktopClone: function() {
        let windows = global.get_window_actors().filter(function(w) {
            return w.meta_window.get_window_type() == Meta.WindowType.DESKTOP;
        });
        if (windows.length == 0)
            return null;

        let clone = new Clutter.Clone({ source: windows[0].get_texture() });
        clone.source.connect('destroy', Lang.bind(this, function() {
            clone.destroy();
        }));
        return clone;
    },

    relayout: function () {
        let primary = global.get_primary_monitor();
        let rtl = (St.Widget.get_default_direction () == St.TextDirection.RTL);

        let contentY = Panel.PANEL_HEIGHT;
        let contentHeight = primary.height - contentY - Main.messageTray.actor.height;

        this._group.set_position(primary.x, primary.y);
        this._group.set_size(primary.width, primary.height);

        this._coverPane.set_position(0, contentY);
        this._coverPane.set_size(primary.width, contentHeight);

        let viewWidth = (1.0 - DASH_SPLIT_FRACTION) * primary.width - this._spacing;
        let viewHeight = contentHeight - 2 * this._spacing;
        let viewY = contentY + this._spacing;
        let viewX = rtl ? 0
                        : Math.floor(DASH_SPLIT_FRACTION * primary.width) + this._spacing;

        // Set the dash's x position - y is handled by a constraint
        let dashX;
        if (rtl) {
            this._dash.actor.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
            dashX = primary.width;
        } else {
            dashX = 0;
        }
        this._dash.actor.set_x(dashX);

        this.viewSelector.actor.set_position(viewX, viewY);
        this.viewSelector.actor.set_size(viewWidth, viewHeight);
    },

    //// Public methods ////

    beginItemDrag: function(source) {
        this.emit('item-drag-begin');
    },

    endItemDrag: function(source) {
        this.emit('item-drag-end');
    },

    beginWindowDrag: function(source) {
        this.emit('window-drag-begin');
    },

    endWindowDrag: function(source) {
        this.emit('window-drag-end');
    },

    // Returns the scale the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInScale : function() {
        return 1 / this.workspaces.getScale();
    },

    // Returns the position the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInPosition : function() {
        let [posX, posY] = this.workspaces.getActiveWorkspacePosition();
        let scale = this.getZoomedInScale();

        return [- posX * scale, - posY * scale];
    },

    // Returns the current scale of the Overview.
    getScale : function() {
        return this.workspaces.actor.scaleX;
    },

    // Returns the current position of the Overview.
    getPosition : function() {
        return [this.workspaces.actor.x, this.workspaces.actor.y];
    },

    show : function() {
        if (this.visible)
            return;
        if (!Main.pushModal(this.viewSelector.actor))
            return;

        this.visible = true;
        this.animationInProgress = true;

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the Overview is displayed greatly
        // increases performance of the Overview especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the Overview rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();
        this._background.show();

        this.viewSelector.show();
        this._workspacesDisplay.show();
        this._dash.show();

        this.workspaces = this._workspacesDisplay.workspacesView;
        global.overlay_group.add_actor(this.workspaces.actor);

        if (!this._desktopFade.child)
            this._desktopFade.child = this._getDesktopClone();

        if (!this.workspaces.getActiveWorkspace().hasMaximizedWindows()) {
            this._desktopFade.opacity = 255;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 0,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad'
                             });
        }

        // Create a zoom out effect. First scale the workspaces view up and
        // position it so that the active workspace fills up the whole screen,
        // then transform it to its normal dimensions and position.
        // The opposite transition is used in hide().
        this.workspaces.actor.scaleX = this.workspaces.actor.scaleY = this.getZoomedInScale();
        [this.workspaces.actor.x, this.workspaces.actor.y] = this.getZoomedInPosition();
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.workspaces.actor,
                         { x: primary.x - this._group.x,
                           y: primary.y - this._group.y,
                           scaleX: 1,
                           scaleY: 1,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._showDone,
                           onCompleteScope: this
                          });

        // Make the other elements fade in.
        this._group.opacity = 0;
        Tweener.addTween(this._group,
                         { opacity: 255,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('showing');
    },

    hide: function() {
        if (!this.visible || this._hideInProgress)
            return;

        this.animationInProgress = true;
        this._hideInProgress = true;

        if (!this.workspaces.getActiveWorkspace().hasMaximizedWindows()) {
            this._desktopFade.opacity = 0;
            this._desktopFade.show();
            Tweener.addTween(this._desktopFade,
                             { opacity: 255,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad' });
        }

        this.workspaces.hide();

        // Create a zoom in effect by transforming the workspaces view so that
        // the active workspace fills up the whole screen. The opposite
        // transition is used in show().
        let scale = this.getZoomedInScale();
        let [posX, posY] = this.getZoomedInPosition();
        Tweener.addTween(this.workspaces.actor,
                         { x: posX,
                           y: posY,
                           scaleX: scale,
                           scaleY: scale,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                          });

        // Make other elements fade out.
        Tweener.addTween(this._group,
                         { opacity: 0,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('hiding');
    },

    toggle: function() {
        if (this.visible)
            this.hide();
        else
            this.show();
    },

    /**
     * getWorkspacesForWindow:
     * @metaWindow: A #MetaWindow
     *
     * Returns the Workspaces object associated with the given window.
     * This method is not be accessible if the overview is not open
     * and will return %null.
     */
    getWorkspacesForWindow: function(metaWindow) {
        return this.workspaces;
    },

    //// Private methods ////

    _showDone: function() {
        if (this._hideInProgress)
            return;

        this.animationInProgress = false;
        this._desktopFade.hide();
        this._coverPane.lower_bottom();

        this.emit('shown');
    },

    _hideDone: function() {
        global.window_group.show();

        this.workspaces.destroy();
        this.workspaces = null;

        this._workspacesDisplay.hide();
        this.viewSelector.hide();
        this._dash.hide();

        this._desktopFade.hide();
        this._background.hide();
        this._group.hide();

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        this._coverPane.lower_bottom();

        Main.popModal(this.viewSelector.actor);
        this.emit('hidden');
    }
};
Signals.addSignalMethods(Overview.prototype);
