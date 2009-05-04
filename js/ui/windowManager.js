/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const AltTab = imports.ui.altTab;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const WINDOW_ANIMATION_TIME = 0.25;
const SWITCH_ANIMATION_TIME = 0.5;

function WindowManager() {
    this._init();
}

WindowManager.prototype = {
    _init : function() {
        let me = this;

        this._global = Shell.Global.get();
        this._shellwm = this._global.window_manager;
        this._minimizing = [];
        this._maximizing = [];
        this._unmaximizing = [];
        this._mapping = [];
        this._destroying = [];

        this._switchData = null;
        this._shellwm.connect('switch-workspace',
            function(o, from, to, direction) {
                let actors = me._shellwm.get_switch_workspace_actors();
                me._switchWorkspace(actors, from, to, direction);
            });
        this._shellwm.connect('kill-switch-workspace',
            function(o) {
                me._switchWorkspaceDone();
            });
        this._shellwm.connect('minimize',
            function(o, actor) {
                me._minimizeWindow(actor);
            });
        this._shellwm.connect('kill-minimize',
            function(o, actor) {
                me._minimizeWindowDone(actor);
            });
        this._shellwm.connect('maximize',
            function(o, actor, tx, ty, tw, th) {
                me._maximizeWindow(actor, tx, ty, tw, th);
            });
        this._shellwm.connect('kill-maximize',
            function(o, actor) {
                me._maximizeWindowDone(actor);
            });
        this._shellwm.connect('unmaximize',
            function(o, actor, tx, ty, tw, th) {
                me._unmaximizeWindow(actor, tx, ty, tw, th);
            });
        this._shellwm.connect('kill-unmaximize',
            function(o, actor) {
                me._unmaximizeWindowDone(actor);
            });
        this._shellwm.connect('map',
            function(o, actor) {
                me._mapWindow(actor);
            });
        this._shellwm.connect('kill-map',
            function(o, actor) {
                me._mapWindowDone(actor);
            });
        this._shellwm.connect('destroy',
            function(o, actor) {
                me._destroyWindow(actor);
            });
        this._shellwm.connect('kill-destroy',
            function(o, actor) {
                me._destroyWindowDone(actor);
            });

        this._shellwm.connect('begin-alt-tab',
            function(o, handler) {
                me._beginAltTab(handler);
            });
    },

    _shouldAnimate : function(actor) {
        if (Main.overlayActive)
            return false;
        if (actor && (actor.get_window_type() != Meta.CompWindowType.NORMAL))
            return false;
        return true;
    },

    _removeEffect : function(list, actor) {
        let idx = list.indexOf(actor);
        if (idx != -1) {
            list.splice(idx, 1);
            return true;
        }
        return false;
    },

    _minimizeWindow : function(actor) {
        if (!this._shouldAnimate(actor)) {
            this._shellwm.completed_minimize(actor);
            return;
        }

        actor.set_scale(1.0, 1.0);
        actor.move_anchor_point_from_gravity(Clutter.Gravity.CENTER);

        /* scale window down to 0x0.
         * maybe TODO: get icon geometry passed through and move the window towards it?
         */
        this._minimizing.push(actor);
        Tweener.addTween(actor,
                         { scale_x: 0.0,
                           scale_y: 0.0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._minimizeWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [actor],
                           onOverwrite: this._minimizeWindowOverwritten,
                           onOverwriteScope: this,
                           onOverwriteParams: [actor]
                         });
    },

    _minimizeWindowDone : function(actor) {
        if (this._removeEffect(this._minimizing, actor)) {
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
            actor.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_WEST);

            this._shellwm.completed_minimize(actor);
        }
    },

    _minimizeWindowOverwritten : function(actor) {
        if (this._removeEffect(this._minimizing, actor)) {
            this._shellwm.completed_minimize(actor);
        }
    },

    _maximizeWindow : function(actor, target_x, target_y, target_width, target_height) {
        if (!this._shouldAnimate(actor)) {
            this._shellwm.completed_maximize(actor);
            return;
        }

        /* this doesn't work very well, as simply scaling up the existing
         * window contents doesn't produce anything like the same results as
         * actually maximizing the window.
         */
        let scale_x = target_width / actor.width;
        let scale_y = target_height / actor.height;
        let anchor_x = (actor.x - target_x) * actor.width/(target_width - actor.width);
        let anchor_y = (actor.y - target_y) * actor.height/(target_height - actor.height);
        
        actor.move_anchor_point(anchor_x, anchor_y);

        this._maximizing.push(actor);
        Tweener.addTween(actor,
                         { scale_x: scale_x,
                           scale_y: scale_y,
                           time: WINDOW_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._maximizeWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [actor],
                           onOverwrite: this._maximizeWindowOverwrite,
                           onOverwriteScope: this,
                           onOverwriteParams: [actor]
                         });
    },

    _maximizeWindowDone : function(actor) {
        if (this._removeEffect(this._maximizing, actor)) {
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
            actor.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_WEST);
            this._shellwm.completed_maximize(actor);
        }
    },

    _maximizeWindowOverwrite : function(actor) {
        if (this._removeEffect(this._maximizing, actor)) {
            this._shellwm.completed_maximize(actor);
        }
    },

    _unmaximizeWindow : function(actor, target_x, target_y, target_width, target_height) {
        this._shellwm.completed_unmaximize(actor);
    },

    _unmaximizeWindowDone : function(actor) {
    },

    _mapWindow : function(actor) {
        if (!this._shouldAnimate(actor)) {
            this._shellwm.completed_map(actor);
            return;
        }

        actor.move_anchor_point_from_gravity(Clutter.Gravity.CENTER);
        actor.set_scale(0.0, 0.0);
        actor.show();
        
        /* scale window up from 0x0 to normal size */
        this._mapping.push(actor);
        Tweener.addTween(actor,
                         { scale_x: 1.0,
                           scale_y: 1.0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._mapWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [actor],
                           onOverwrite: this._mapWindowOverwrite,
                           onOverwriteScope: this,
                           onOverwriteParams: [actor]
                         });
    },

    _mapWindowDone : function(actor) {
        if (this._removeEffect(this._mapping, actor)) {
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
            actor.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_WEST);
            this._shellwm.completed_map(actor);
        }
    },

    _mapWindowOverwrite : function(actor) {
        if (this._removeEffect(this._mapping, actor)) {
            this._shellwm.completed_map(actor);
        }
    },

    _destroyWindow : function(actor) {
        if (!this._shouldAnimate(actor)) {
            this._shellwm.completed_destroy(actor);
            return;
        }

        actor.move_anchor_point_from_gravity(Clutter.Gravity.CENTER);
        
        /* anachronistic 'tv-like' effect - squash on y axis, leave x alone */
        this._destroying.push(actor);
        Tweener.addTween(actor,
                         { scale_x: 1.0,
                           scale_y: 0.0,
                           time: WINDOW_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._destroyWindowDone,
                           onCompleteScope: this,
                           onCompleteParams: [actor],
                           onOverwrite: this._destroyWindowOverwrite,
                           onOverwriteScope: this,
                           onOverwriteParams: [actor]
                         });
    },
    
    _destroyWindowDone : function(actor) {
        if (this._removeEffect(this._destroying, actor)) {
            this._shellwm.completed_destroy(actor);
            Tweener.removeTweens(actor);
            actor.set_scale(1.0, 1.0);
        }
    },

    _destroyWindowOverwrite : function(actor) {
        if (this._removeEffect(this._destroying, actor)) {
            this._shellwm.completed_destroy(actor);
        }
    },

    _switchWorkspace : function(windows, from, to, direction) {
        if (!this._shouldAnimate()) {
            this._shellwm.completed_switch_workspace();
            return;
        }

        /* @direction is the direction that the "camera" moves, so the
         * screen contents have to move one screen's worth in the
         * opposite direction.
         */
        let xDest = 0, yDest = 0;

        if (direction == Meta.MotionDirection.UP ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.UP_RIGHT)
                yDest = this._global.screen_height;
        else if (direction == Meta.MotionDirection.DOWN ||
            direction == Meta.MotionDirection.DOWN_LEFT ||
            direction == Meta.MotionDirection.DOWN_RIGHT)
                yDest = -this._global.screen_height;

        if (direction == Meta.MotionDirection.LEFT ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.DOWN_LEFT)
                xDest = this._global.screen_width;
        else if (direction == Meta.MotionDirection.RIGHT ||
                 direction == Meta.MotionDirection.UP_RIGHT ||
                 direction == Meta.MotionDirection.DOWN_RIGHT)
                xDest = -this._global.screen_width;

        let switchData = {};
        this._switchData = switchData;
        switchData.inGroup = new Clutter.Group();
        switchData.outGroup = new Clutter.Group();
        switchData.windows = [];

        let wgroup = this._global.window_group;
        wgroup.add_actor(switchData.inGroup);
        wgroup.add_actor(switchData.outGroup);

        for (let i = 0; i < windows.length; i++) {
            let window = windows[i];
            if (window.get_workspace() == from) {
                switchData.windows.push({ window: window,
                                          parent: window.get_parent() });
                window.reparent(switchData.outGroup);
            } else if (window.get_workspace() == to) {
                switchData.windows.push({ window: window,
                                          parent: window.get_parent() });
                window.reparent(switchData.inGroup);
                window.show_all();
            }
        }

        switchData.inGroup.set_position(-xDest, -yDest);
        switchData.inGroup.raise_top();

        Tweener.addTween(switchData.outGroup,
                         { x: xDest,
                           y: yDest,
                           time: SWITCH_ANIMATION_TIME,
                           transition: "easeOutBack",
                           onComplete: this._switchWorkspaceDone,
                           onCompleteScope: this
                         });
        Tweener.addTween(switchData.inGroup,
                         { x: 0,
                           y: 0,
                           time: SWITCH_ANIMATION_TIME,
                           transition: "easeOutBack"
                         });
    },

    _switchWorkspaceDone : function() {
        let switchData = this._switchData;
        if (!switchData)
            return;
        this._switchData = null;

        for (let i = 0; i < switchData.windows.length; i++) {
                let w = switchData.windows[i];
                if (w.window.get_parent() == switchData.outGroup) {
                    w.window.reparent(w.parent);
                    w.window.hide();
                } else
                    w.window.reparent(w.parent);
        }
        Tweener.removeTweens(switchData.inGroup);
        Tweener.removeTweens(switchData.outGroup);
        switchData.inGroup.destroy();
        switchData.outGroup.destroy();

        this._shellwm.completed_switch_workspace();
    },

    _beginAltTab : function(handler) {
        let popup = new AltTab.AltTabPopup();

        handler.connect('window-added', function(handler, window) { popup.addWindow(window); });
        handler.connect('show', function(handler, initialSelection) { popup.show(initialSelection); });
        handler.connect('destroy', function() { popup.destroy(); });
        handler.connect('notify::selected', function() { popup.select(handler.selected); });
    }  
};
