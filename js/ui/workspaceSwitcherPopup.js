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
        this._orientation = global.screen.layout_rows == -1
            ? Clutter.Orientation.VERTICAL
            : Clutter.Orientation.HORIZONTAL;

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

        this._globalSignals = [];
        this._globalSignals.push(global.screen.connect('workspace-added', this._redisplay.bind(this)));
        this._globalSignals.push(global.screen.connect('workspace-removed', this._redisplay.bind(this)));
    },

    _getPreferredSizeForOrientation(forSize) {
        let children = this._list.get_children();
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        let themeNode = this.actor.get_theme_node();

        let availSize;
        if (this._orientation == Clutter.Orientation.HORIZONTAL) {
            availSize = workArea.width - themeNode.get_horizontal_padding();
            availSize -= this._container.get_theme_node().get_horizontal_padding();
            availSize -= this._list.get_theme_node().get_horizontal_padding();
        } else {
            availSize = workArea.height - themeNode.get_vertical_padding();
            availSize -= this._container.get_theme_node().get_vertical_padding();
            availSize -= this._list.get_theme_node().get_vertical_padding();
        }

        let size = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinHeight, childNaturalHeight] = children[i].get_preferred_height(-1);
            let height = childNaturalHeight * workArea.width / workArea.height;

            if (this._orientation == Clutter.Orientation.HORIZONTAL)
                size += height * workArea.width / workArea.height;
            else
                size += height;
        }

        let spacing = this._itemSpacing * (global.screen.n_workspaces - 1);
        size += spacing;
        size = Math.min(size, availSize);

        if (this._orientation == Clutter.Orientation.HORIZONTAL) {
            this._childWidth = (size - spacing) / global.screen.n_workspaces;
            return themeNode.adjust_preferred_width(size, size);
        } else {
            this._childHeight = (size - spacing) / global.screen.n_workspaces;
            return themeNode.adjust_preferred_height(size, size);
        }
    },

    _getSizeForOppositeOrientation() {
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);

        if (this._orientation == Clutter.Orientation.HORIZONTAL) {
            this._childHeight = Math.round(this._childWidth * workArea.height / workArea.width);
            return [this._childHeight, this._childHeight];
        } else {
            this._childWidth = Math.round(this._childHeight * workArea.width / workArea.height);
            return [this._childWidth, this._childWidth];
        }
    },

    _getPreferredHeight(actor, forWidth, alloc) {
        if (this._orientation == Clutter.Orientation.HORIZONTAL)
            [alloc.min_size, alloc.natural_size] = this._getSizeForOppositeOrientation();
        else
            [alloc.min_size, alloc.natural_size] = this._getPreferredSizeForOrientation(forWidth);
    },

    _getPreferredWidth(actor, forHeight, alloc) {
        if (this._orientation == Clutter.Orientation.HORIZONTAL)
            [alloc.min_size, alloc.natural_size] = this._getPreferredSizeForOrientation(forHeight);
        else
            [alloc.min_size, alloc.natural_size] = this._getSizeForOppositeOrientation();
    },

    _allocate(actor, box, flags) {
        let children = this._list.get_children();
        let childBox = new Clutter.ActorBox();

        let rtl = this.text_direction == Clutter.TextDirection.RTL;
        let x = rtl ? box.x2 - this._childWidth : box.x1;
        let y = box.y1;
        for (let i = 0; i < children.length; i++) {
            childBox.x1 = Math.round(x);
            childBox.x2 = Math.round(x + this._childWidth);
            childBox.y1 = Math.round(y);
            childBox.y2 = Math.round(y + this._childHeight);

            if (this._orientation == Clutter.Orientation.HORIZONTAL) {
                if (rtl)
                    x -= this._childWidth + this._itemSpacing;
                else
                    x += this._childWidth + this._itemSpacing;
            } else {
                y += this._childHeight + this._itemSpacing;
            }
            children[i].allocate(childBox, flags);
        }
    },

    _redisplay() {
        this._list.destroy_all_children();

        for (let i = 0; i < global.screen.n_workspaces; i++) {
            let indicator = null;

           if (i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.UP)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-up' });
           else if (i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.DOWN)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-down' });
           else if (i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.LEFT)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-left' });
           else if (i == this._activeWorkspaceIndex && this._direction == Meta.MotionDirection.RIGHT)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-right' });
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

        for (let i = 0; i < this._globalSignals.length; i++)
            global.screen.disconnect(this._globalSignals[i]);

        this.actor.destroy();

        this.emit('destroy');
    }
});
Signals.addSignalMethods(WorkspaceSwitcherPopup.prototype);
