/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Main = imports.ui.main;

const Tweener = imports.ui.tweener;

const ANIMATION_TIME = 0.075;
const DISPLAY_TIMEOUT = 600;

const LEFT = -1;
const RIGHT = 1;

function WorkspaceSwitcherPopup() {
    this._init();
}

WorkspaceSwitcherPopup.prototype = {
    _init : function() {
        this.actor = new St.Group({ reactive: true,
                                         x: 0,
                                         y: 0,
                                         width: global.screen_width,
                                         height: global.screen_height,
                                         style_class: 'workspace-switcher-group' });
        Main.uiGroup.add_actor(this.actor);

        this._container = new St.BoxLayout({ style_class: 'workspace-switcher-container' });
        this._list = new Shell.GenericContainer({ style_class: 'workspace-switcher' });
        this._itemSpacing = 0;
        this._list.connect('style-changed', Lang.bind(this, function() {
                                                        let [found, spacing] = this._list.get_theme_node().get_length('spacing', false);
                                                        this._itemSpacing = (found) ? spacing : 0;
                                                     }));

        this._list.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._list.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._list.connect('allocate', Lang.bind(this, this._allocate));
        this._container.add(this._list);

        this.actor.add_actor(this._container);

        this._redraw();

        this._position();

        this.actor.show();
        this._timeoutId = Mainloop.timeout_add(DISPLAY_TIMEOUT, Lang.bind(this, this._onTimeout));
    },

    _getPreferredWidth : function (actor, forHeight, alloc) {
        let children = this._list.get_children();
        let primary = global.get_primary_monitor();

        let availwidth = primary.width;
        availwidth -= this.actor.get_theme_node().get_horizontal_padding();
        availwidth -= this._container.get_theme_node().get_horizontal_padding();
        availwidth -= this._list.get_theme_node().get_horizontal_padding();

        let width = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinWidth, childNaturalWidth] = children[i].get_preferred_width(-1);
            let [childMinHeight, childNaturalHeight] = children[i].get_preferred_height(childNaturalWidth);
            width += childNaturalHeight * primary.width / primary.height;
        }

        let spacing = this._itemSpacing * (global.screen.n_workspaces - 1);
        width += spacing;
        width = Math.min(width, availwidth);

        this._childWidth = (width - spacing) / global.screen.n_workspaces;

        alloc.min_size = width;
        alloc.natural_size = width;
    },

    _getPreferredHeight : function (actor, forWidth, alloc) {
        let primary = global.get_primary_monitor();
        this._childHeight = Math.round(this._childWidth * primary.height / primary.width);

        alloc.min_size = this._childHeight;
        alloc.natural_size = this._childHeight;
    },

    _allocate : function (actor, box, flags) {
        let children = this._list.get_children();
        let childBox = new Clutter.ActorBox();

        let x = box.x1;
        let prevChildBoxX2 = box.x1 - this._itemSpacing;
        for (let i = 0; i < children.length; i++) {
            childBox.x1 = prevChildBoxX2 + this._itemSpacing;
            childBox.x2 = Math.round(x + this._childWidth);
            childBox.y1 = box.y1;
            childBox.y2 = box.y1 + this._childHeight;
            x += this._childWidth + this._itemSpacing;
            prevChildBoxX2 = childBox.x2;
            children[i].allocate(childBox, flags);
        }
    },

    _redraw : function(direction, activeWorkspaceIndex) {
        this._list.destroy_children();

        for (let i = 0; i < global.screen.n_workspaces; i++) {
            let indicator = null;

           if (i == activeWorkspaceIndex && direction == LEFT)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-left' });
           else if(i == activeWorkspaceIndex && direction == RIGHT)
               indicator = new St.Bin({ style_class: 'ws-switcher-active-right' });
           else
               indicator = new St.Bin({ style_class: 'ws-switcher-box' });

           this._list.add_actor(indicator);

        }
    },

    _position: function() {
        let primary = global.get_primary_monitor();
        this._container.x = primary.x + Math.floor((primary.width - this._container.width) / 2);
        this._container.y = primary.y + Math.floor((primary.height - this._container.height) / 2);
    },

    _show : function() {
        Tweener.addTween(this._container, { opacity: 255,
                                            time: ANIMATION_TIME,
                                            transition: 'easeOutQuad'
                                           });
        this._position();
        this.actor.show();
    },

    display : function(direction, activeWorkspaceIndex) {
        this._redraw(direction, activeWorkspaceIndex);
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
                                            onComplete: function() { this.actor.hide(); },
                                            onCompleteScope: this
                                           });
    }
};
