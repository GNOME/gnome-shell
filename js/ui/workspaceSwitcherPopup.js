// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta  = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const ANIMATION_TIME = 0.1;
const DISPLAY_TIMEOUT = 600;

const WorkspaceSwitcherPopup = new Lang.Class({
    Name: 'WorkspaceSwitcherPopup',

    _init : function() {
        this.actor = new St.Widget({ reactive: true,
                                     x: 0,
                                     y: 0,
                                     width: global.screen_width,
                                     height: global.screen_height,
                                     style_class: 'workspace-switcher-group' });
        Main.layoutManager.osdGroup.add_child(this.actor);

        this._container = new St.BoxLayout({ style_class: 'workspace-switcher-container' });
        this._list = new Shell.GenericContainer({ style_class: 'workspace-switcher' });
        this._itemSpacing = 0;
        this._childHeight = 0;
        this._childWidth = 0;
        this._list.connect('style-changed', Lang.bind(this, function() {
                                                        this._itemSpacing = this._list.get_theme_node().get_length('spacing');
                                                     }));

        this._list.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._list.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._list.connect('allocate', Lang.bind(this, this._allocate));
        this._container.add(this._list);

        this.actor.add_actor(this._container);

        this._redisplay();

        this.actor.hide();

        this._globalSignals = [];
        this._globalSignals.push(global.screen.connect('workspace-added', Lang.bind(this, this._redisplay)));
        this._globalSignals.push(global.screen.connect('workspace-removed', Lang.bind(this, this._redisplay)));

        this._timeoutId = Mainloop.timeout_add(DISPLAY_TIMEOUT, Lang.bind(this, this._onTimeout));
    },

    _getPreferredHeight : function (actor, forWidth, alloc) {
        let children = this._list.get_children();
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);

        let availHeight = workArea.height;
        availHeight -= this.actor.get_theme_node().get_vertical_padding();
        availHeight -= this._container.get_theme_node().get_vertical_padding();
        availHeight -= this._list.get_theme_node().get_vertical_padding();

        let height = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinHeight, childNaturalHeight] = children[i].get_preferred_height(-1);
            let [childMinWidth, childNaturalWidth] = children[i].get_preferred_width(childNaturalHeight);
            height += childNaturalHeight * workArea.width / workArea.height;
        }

        let spacing = this._itemSpacing * (global.screen.n_workspaces - 1);
        height += spacing;
        height = Math.min(height, availHeight);

        this._childHeight = (height - spacing) / global.screen.n_workspaces;

        alloc.min_size = height;
        alloc.natural_size = height;
    },

    _getPreferredWidth : function (actor, forHeight, alloc) {
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        this._childWidth = Math.round(this._childHeight * workArea.width / workArea.height);

        alloc.min_size = this._childWidth;
        alloc.natural_size = this._childWidth;
    },

    _allocate : function (actor, box, flags) {
        let children = this._list.get_children();
        let childBox = new Clutter.ActorBox();

        let y = box.y1;
        let prevChildBoxY2 = box.y1 - this._itemSpacing;
        for (let i = 0; i < children.length; i++) {
            childBox.x1 = box.x1;
            childBox.x2 = box.x1 + this._childWidth;
            childBox.y1 = prevChildBoxY2 + this._itemSpacing;
            childBox.y2 = Math.round(y + this._childHeight);
            y += this._childHeight + this._itemSpacing;
            prevChildBoxY2 = childBox.y2;
            children[i].allocate(childBox, flags);
        }
    },

    _redisplay: function() {
        this._list.destroy_all_children();

        for (let i = 0; i < global.screen.n_workspaces; i++) {
            let indicator = null;

           if (i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.UP)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-up' });
           else if(i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.DOWN)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-down' });
           else
               indicator = new St.Bin({ style_class: 'ws-switcher-box' });

           this._list.add_actor(indicator);

        }

        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        let [containerMinHeight, containerNatHeight] = this._container.get_preferred_height(global.screen_width);
        let [containerMinWidth, containerNatWidth] = this._container.get_preferred_width(containerNatHeight);
        this._container.x = workArea.x + Math.floor((workArea.width - containerNatWidth) / 2);
        this._container.y = workArea.y + Math.floor((workArea.height - containerNatHeight) / 2);
    },

    _show : function() {
        Tweener.addTween(this._container, { opacity: 255,
                                            time: ANIMATION_TIME,
                                            transition: 'easeOutQuad'
                                           });
        this.actor.show();
    },

    display : function(direction, activeWorkspaceIndex) {
        this._direction = direction;
        this._activeWorkspaceIndex = activeWorkspaceIndex;

        this._redisplay();
        if (this._timeoutId != 0)
            Mainloop.source_remove(this._timeoutId);
        this._timeoutId = Mainloop.timeout_add(DISPLAY_TIMEOUT, Lang.bind(this, this._onTimeout));
        this._show();
    },

    _onTimeout : function() {
        Mainloop.source_remove(this._timeoutId);
        this._timeoutId = 0;
        Tweener.addTween(this._container, { opacity: 0.0,
                                            time: ANIMATION_TIME,
                                            transition: 'easeOutQuad',
                                            onComplete: function() { this.destroy(); },
                                            onCompleteScope: this
                                           });
        return GLib.SOURCE_REMOVE;
    },

    destroy: function() {
        if (this._timeoutId)
            Mainloop.source_remove(this._timeoutId);
        this._timeoutId = 0;

        for (let i = 0; i < this._globalSignals.length; i++)
            global.screen.disconnect(this._globalSignals[i]);

        this.actor.destroy();

        this.emit('destroy');
    }
});
Signals.addSignalMethods(WorkspaceSwitcherPopup.prototype);
