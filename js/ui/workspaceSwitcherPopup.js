/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

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
        this.actor = new Clutter.Group({ reactive: true,
                                         x: 0,
                                         y: 0,
                                         width: global.screen_width,
                                         height: global.screen_height });
        global.stage.add_actor(this.actor);

        this._scaleWidth = global.screen_width / global.screen_height;

        this._container = new St.BoxLayout({ style_class: "workspace-switcher-container" });
        this._list = new St.BoxLayout({ style_class: "workspace-switcher" });

        this._container.add(this._list);

        this.actor.add_actor(this._container);

        this._redraw();

        this._position();

        this.actor.show();
        this._timeoutId = Mainloop.timeout_add(DISPLAY_TIMEOUT, Lang.bind(this, this._onTimeout));
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

           this._list.add(indicator);
           indicator.set_width(Math.round(indicator.get_height() * this._scaleWidth));

           if (i < global.screen.n_workspaces - 1) {
               let spacer = new St.Bin({ style_class: 'ws-switcher-spacer' });
               this._list.add(spacer);
           }

        }
    },

    _position: function() {
        let focus = global.get_focus_monitor();
        this._container.x = focus.x + Math.floor((focus.width - this._container.width) / 2);
        this._container.y = focus.y + Math.floor((focus.height - this._container.height) / 2);
    },

    _show : function() {
        Tweener.addTween(this._container, { opacity: 255,
                                            time: ANIMATION_TIME,
                                            transition: "easeOutQuad"
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
                                            transition: "easeOutQuad",
                                            onComplete: function() { this.actor.hide() },
                                            onCompleteScope: this
                                           });
    }
};
