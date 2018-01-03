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

var ANIMATION_TIME = 0.1;
var DISPLAY_TIMEOUT = 600;

var WorkspaceSwitcherPopup = new Lang.Class({
    Name: 'WorkspaceSwitcherPopup',

    _init() {
        this.actor = new St.Widget({ x: 0,
                                     y: 0,
                                     width: global.screen_width,
                                     height: global.screen_height,
                                     style_class: 'workspace-switcher-group' });
        Main.uiGroup.add_actor(this.actor);

        this._container = new St.BoxLayout({ style_class: 'workspace-switcher-container' });
        this._list = new Shell.GenericContainer({ style_class: 'workspace-switcher' });
        this._itemSpacing = 0;
        this._childHeight = 0;
        this._childWidth = 0;
        this._timeoutId = 0;
        this._list.connect('style-changed', () => {
           this._itemSpacing = this._list.get_theme_node().get_length('spacing');
        });

        this._list.connect('get-preferred-width', this._getPreferredWidth.bind(this));
        this._list.connect('get-preferred-height', this._getPreferredHeight.bind(this));
        this._list.connect('allocate', this._allocate.bind(this));
        this._container.add(this._list);

        this.actor.add_actor(this._container);

        this._redisplay();

        this.actor.hide();

        let workspaceManager = global.workspace_manager;
        this._workspaceManagerSignals = [];
        this._workspaceManagerSignals.push(workspaceManager.connect('workspace-added',
                                                                    this._redisplay.bind(this)));
        this._workspaceManagerSignals.push(workspaceManager.connect('workspace-removed',
                                                                    this._redisplay.bind(this)));
    },

    _getPreferredHeight(actor, forWidth, alloc) {
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

        let workspaceManager = global.workspace_manager;
        let spacing = this._itemSpacing * (workspaceManager.n_workspaces - 1);
        height += spacing;
        height = Math.min(height, availHeight);

        this._childHeight = (height - spacing) / workspaceManager.n_workspaces;

        alloc.min_size = height;
        alloc.natural_size = height;
    },

    _getPreferredWidth(actor, forHeight, alloc) {
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        this._childWidth = Math.round(this._childHeight * workArea.width / workArea.height);

        alloc.min_size = this._childWidth;
        alloc.natural_size = this._childWidth;
    },

    _allocate(actor, box, flags) {
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

    _redisplay() {
        let workspaceManager = global.workspace_manager;

        this._list.destroy_all_children();

        for (let i = 0; i < workspaceManager.n_workspaces; i++) {
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

    _show() {
        Tweener.addTween(this._container, { opacity: 255,
                                            time: ANIMATION_TIME,
                                            transition: 'easeOutQuad'
                                           });
        this.actor.show();
    },

    display(direction, activeWorkspaceIndex) {
        this._direction = direction;
        this._activeWorkspaceIndex = activeWorkspaceIndex;

        this._redisplay();
        if (this._timeoutId != 0)
            Mainloop.source_remove(this._timeoutId);
        this._timeoutId = Mainloop.timeout_add(DISPLAY_TIMEOUT, this._onTimeout.bind(this));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._onTimeout');
        this._show();
    },

    _onTimeout() {
        Mainloop.source_remove(this._timeoutId);
        this._timeoutId = 0;
        Tweener.addTween(this._container, { opacity: 0.0,
                                            time: ANIMATION_TIME,
                                            transition: 'easeOutQuad',
                                            onComplete() { this.destroy(); },
                                            onCompleteScope: this
                                           });
        return GLib.SOURCE_REMOVE;
    },

    destroy() {
        if (this._timeoutId)
            Mainloop.source_remove(this._timeoutId);
        this._timeoutId = 0;

        let workspaceManager = global.workspace_manager;
        for (let i = 0; i < this._workspaceManagerSignals.length; i++)
            workspaceManager.disconnect(this._workspaceManagerSignals[i]);

        this.actor.destroy();

        this.emit('destroy');
    }
});
Signals.addSignalMethods(WorkspaceSwitcherPopup.prototype);
