/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Tweener = imports.tweener.tweener;
const Clutter = imports.gi.Clutter;

const Main = imports.ui.main;
const Overlay = imports.ui.overlay;
const Panel = imports.ui.panel;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

// Windows are slightly translucent in the overlay mode
const WINDOW_OPACITY = 0.9 * 255;

// Define a layout scheme for small window counts. For larger
// counts we fall back to an algorithm. We need more schemes here
// unless we have a really good algorithm.

// Each triplet is [xCenter, yCenter, scale] where the scale
// is relative to the width of the workspace.
const POSITIONS = {
        1: [[0.5, 0.5, 0.8]],
        2: [[0.25, 0.5, 0.4], [0.75, 0.5, 0.4]],
        3: [[0.25, 0.25, 0.33],  [0.75, 0.25, 0.33],  [0.5, 0.75, 0.33]],
        4: [[0.25, 0.25, 0.33],   [0.75, 0.25, 0.33], [0.75, 0.75, 0.33], [0.25, 0.75, 0.33]],
        5: [[0.165, 0.25, 0.28], [0.495, 0.25, 0.28], [0.825, 0.25, 0.28], [0.25, 0.75, 0.4], [0.75, 0.75, 0.4]]
};

// spacing between workspaces
const GRID_SPACING = 15;

function Workspaces(x, y, width, height) {
    this._init(x, y, width, height);
}

Workspaces.prototype = {
    _init : function(x, y, width, height) {

        this._group = new Clutter.Group();

        this._x = x;
        this._y = y;
        this._width = width;
        this._height = height;
        this._workspaces = [];
    },

    show : function() {
        let global = Shell.Global.get();

        let windows = global.get_windows();
        let activeWorkspace = global.screen.get_active_workspace_index();

        // Create a group for each workspace (which lets us raise all of
        // its clone windows together when the workspace is activated)
        // and add the desktop windows
        this._workspaces = [];
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._workspaces[w] = new Clutter.Group();
            this._group.add_actor(this._workspaces[w]);
        }
        this._createDesktopActors(windows);

        // The workspaces will go into a grid that is either square,
        // or else 1 cell wider than it is tall.
        // FIXME: need to make the metacity internal layout agree with this!
        let gridWidth = Math.ceil(Math.sqrt(this._workspaces.length));
        let gridHeight = Math.ceil(this._workspaces.length / gridWidth);

        let wsWidth = (this._width - (gridWidth - 1) * GRID_SPACING) / gridWidth;
        let wsHeight = (this._height - (gridHeight - 1) * GRID_SPACING) / gridHeight;
        let scale = wsWidth / global.screen_width;

        // Position/scale the desktop windows and their children. This
        // would be easier if we instead just positioned and scaled
        // the entire workspace group, but if we do that then the
        // windows of the active workspace will trace out a curved
        // path as they move into place, which looks odd. Positioning
        // everything independently lets us move them in a straight
        // line.
        for (let w = 0, x = this._x, y = this._y; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            let desktop = workspace.get_nth_child(0);

            if (w == activeWorkspace) {
                // The currently-active workspace needs to
                // slide/shrink into place
                workspace.raise_top();
                Tweener.addTween(desktop,
                                 { x: x,
                                   y: y,
                                   scale_x: scale,
                                   scale_y: scale,
                                   time: Overlay.ANIMATION_TIME,
                                   transition: "easeOutQuad"
                                 });
            } else {
                // Other workspaces can start out in place; they'll be
                // revealed as the active workspace shrinks
                desktop.set_position(x, y);
                desktop.set_scale(scale, scale);
            }

            // Now handle the rest of the windows in this workspace
            let wswindows = windows.filter(function (win) { return win.get_workspace() == w; });

            // Do the windows in reverse order so that the active
            // actor ends up on top
            for (let i = 0, windowIndex = 0; i < wswindows.length; i++) {
                let win = wswindows[i];
                if (win.get_window_type() == Meta.WindowType.DESKTOP ||
                    win.is_override_redirect())
                    continue;

                this._createWindowClone(wswindows[i], this._workspaces[w],
                                        x, y, scale,
                                        wswindows.length - windowIndex - 1,
                                        wswindows.length,
                                        w == activeWorkspace);
                windowIndex++;
            }

            x += (wsWidth + GRID_SPACING);
            if (x >= this._x + this._width - GRID_SPACING) {
                x = this._x;
                y += wsHeight + GRID_SPACING;
            }
        }
    },

    hide : function() {
        let global = Shell.Global.get();
        let activeWorkspace = global.screen.get_active_workspace_index();

        this._workspaces[activeWorkspace].raise_top();
        let windows = this._workspaces[activeWorkspace].get_children();
        for (let i = 0; i < windows.length; i++) {
            Tweener.addTween(windows[i],
                             { x: windows[i].orig_x || 0,
                               y: windows[i].orig_y || 0,
                               scale_x: 1.0,
                               scale_y: 1.0,
                               time: Overlay.ANIMATION_TIME,
                               opacity: 255,
                               transition: "easeOutQuad"
                             });
        }
    },
    
    hideDone : function() {
        for (let w = 0; w < this._workspaces.length; w++) {
            this._workspaces[w].destroy();
        }
        this._workspaces = [];
    },

    _createDesktopActors : function(windows) {
        let me = this;
        let global = Shell.Global.get();

        // Find the desktop window or windows
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_window_type() != Meta.WindowType.DESKTOP)
                continue;

            if (windows[i].get_meta_window().is_on_all_workspaces()) {
                for (let w = 0; w < this._workspaces.length; w++)
                    this._workspaces[w].add_actor(this._cloneWindow(windows[i]));
                break;
            } else
                this._workspaces[windows[i].get_workspace()].add_actor(this._cloneWindow(windows[i]));
        }

        // Create dummy desktops for workspaces that don't have
        // desktop windows, and hook up button events on all desktops
        for (let w = 0; w < this._workspaces.length; w++) {
            if (this._workspaces[w].get_n_children() == 0)
                this._workspaces[w].add_actor(this._createDesktopRectangle());

            let workspace = global.screen.get_workspace_by_index(w);
            this._workspaces[w].get_nth_child(0).connect(
                "button-press-event",
                function(clone, event) {
                    workspace.activate(event.get_time());
                    me._deactivate();
                });
        }
    },

    _cloneWindow : function(window) {
        let w = new Clutter.CloneTexture({ parent_texture: window.get_texture(),
                                           reactive: true,
                                           x: window.x,
                                           y: window.y });
        w.orig_x = window.x;
        w.orig_y = window.y;
        return w;
    },

    _createDesktopRectangle : function() {
        let global = Shell.Global.get();

        // In the case when we have a desktop window from the file
        // manager, its height is full-screen, i.e. it includes the
        // height of the panel, so we should not subtract the height
        // of the panel from global.screen_height here either to have
        // them show up identically. We are also using (0,0)
        // coordinates in both cases which makes the background window
        // animate out from behind the panel.

        return new Clutter.Rectangle({ color: global.stage.color,
                                       reactive: true,
                                       x: 0,
                                       y: 0,
                                       width: global.screen_width,
                                       height: global.screen_height });
    },

    // windowIndex == 0 => top in stacking order
    _computeWindowPosition : function(windowIndex, numberOfWindows) {
        if (numberOfWindows in POSITIONS)
            return POSITIONS[numberOfWindows][windowIndex];

        // If we don't have a predefined scheme for this window count,
        // overlap the windows along the diagonal of the workspace
        // (improve this!)
        let fraction = Math.sqrt(1/numberOfWindows);

        // The top window goes at the lower right - this is different from the
        // fixed position schemes where the windows are in "reading order"
        // and the top window goes at the upper left.
        let pos = (numberOfWindows - windowIndex - 1) / (numberOfWindows - 1);
        let xCenter = (fraction / 2) + (1 - fraction) * pos;
        let yCenter = xCenter;

        return [xCenter, yCenter, fraction];
    },

    _createWindowClone : function(w, workspace, wsX, wsY, wsScale,
                                  windowIndex, numberOfWindows, animate) {
        let me = this;
        let global = Shell.Global.get();

        // We show the window using "clones" of the texture .. separate
        // actors that mirror the original actors for the window. For
        // animation purposes, it may be better to actually move the
        // original actors about instead.

        let clone = this._cloneWindow(w);
        let [xCenter, yCenter, fraction] = this._computeWindowPosition(windowIndex, numberOfWindows);

        let desiredSize = global.screen_width * fraction;

        xCenter = wsX + wsScale * (xCenter * global.screen_width);
        yCenter = wsY + wsScale * (yCenter * global.screen_height);

        let size = clone.width;
        if (clone.height > size)
            size = clone.height;

        // Never scale up
        let scale = desiredSize / size;
        if (scale > 1)
            scale = 1;
        scale *= wsScale;

        workspace.add_actor(clone);

        if (animate) {
            Tweener.addTween(clone,
                             { x: xCenter - 0.5 * scale * w.width,
                               y: yCenter - 0.5 * scale * w.height,
                               scale_x: scale,
                               scale_y: scale,
                               time: Overlay.ANIMATION_TIME,
                               opacity: WINDOW_OPACITY,
                               transition: "easeOutQuad"
                             });
        } else {
            clone.set_position(xCenter - 0.5 * scale * w.width,
                               yCenter - 0.5 * scale * w.height);
            clone.set_scale(scale, scale);
            clone.set_opacity(WINDOW_OPACITY);
        }

        clone.connect("button-press-event",
                      function(clone, event) {
                          clone.raise_top();
                          me._activateWindow(w, event.get_time());
                      });
    },

    _activateWindow : function(w, time) {
        let global = Shell.Global.get();
        let activeWorkspace = global.screen.get_active_workspace_index();
        let windowWorkspace = w.get_workspace();

        if (windowWorkspace != activeWorkspace) {
            let workspace = global.screen.get_workspace_by_index(windowWorkspace);
            workspace.activate_with_focus(w.get_meta_window(), time);
        } else
            w.get_meta_window().activate(time);
        this._deactivate();
    },

    _deactivate : function() {
        Main.hide_overlay();
    }
};

