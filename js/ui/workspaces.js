/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Tweener = imports.tweener.tweener;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;

const Main = imports.ui.main;
const Overlay = imports.ui.overlay;
const Panel = imports.ui.panel;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Big = imports.gi.Big;

// Windows are slightly translucent in the overlay mode
const WINDOW_OPACITY = 0.9 * 255;

const WINDOWCLONE_BG_COLOR = new Clutter.Color();
WINDOWCLONE_BG_COLOR.from_pixel(0x000000f0);
const WINDOWCLONE_TITLE_COLOR = new Clutter.Color();
WINDOWCLONE_TITLE_COLOR.from_pixel(0xffffffff);

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

// Spacing between workspaces. At the moment, the same spacing is used
// in both zoomed-in and zoomed-out views; this is slightly
// metaphor-breaking, but the alternatives are also weird.
const GRID_SPACING = 15;

function Workspaces() {
    this._init();
}

Workspaces.prototype = {
    _init : function() {
        let me = this;
        let global = Shell.Global.get();

        this.actor = new Clutter.Group();

        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height;

        this._width = screenWidth * Overlay.WORKSPACE_GRID_SCALE;
        this._height = screenHeight * Overlay.WORKSPACE_GRID_SCALE;
        this._x = screenWidth - this._width - Overlay.WORKSPACE_GRID_PADDING;
        this._y = Panel.PANEL_HEIGHT + (screenHeight - this._height - Panel.PANEL_HEIGHT) / 2;

        this._workspaces = [];
        
        this._clones = [];

        let windows = global.get_windows();
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace;

        // Create a group for each workspace (which lets us raise all
        // of its clone windows together when the workspace is
        // activated), figure out their initial grid positions, and
        // add the desktop windows
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._workspaces[w] = new Clutter.Group();
            if (w == activeWorkspaceIndex)
                activeWorkspace = this._workspaces[w];
            this.actor.add_actor(this._workspaces[w]);
        }
        activeWorkspace.raise_top();
        this._positionWorkspaces(global, activeWorkspace);
        this._createDesktopActors(windows);

        // Create a backdrop rectangle, so that you don't see the
        // other parts of the overlay (eg, sidebar) through the gaps
        // between the workspaces when they're zooming in/out
        this._backdrop = new Clutter.Rectangle({ color: Overlay.OVERLAY_BACKGROUND_COLOR,
                                                 x: this._backdropX,
                                                 y: this._backdropY,
                                                 width: this._backdropWidth,
                                                 height: this._backdropHeight
                                               });
        this.actor.add_actor(this._backdrop);
        this._backdrop.lower_bottom();
        Tweener.addTween(this._backdrop,
                         { x: this._x,
                           y: this._y,
                           width: this._width,
                           height: this._height,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });


        // Position/scale the desktop windows and their children. This
        // would be easier if we instead just positioned and scaled
        // the entire workspace group, but if we do that then the
        // windows of the active workspace will trace out a curved
        // path as they move into place, which looks odd. Positioning
        // everything independently lets us move them in a straight
        // line.
        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            let desktop = workspace.get_nth_child(0);
            desktop.set_position(workspace.zoomedOutX, workspace.zoomedOutY);

            Tweener.addTween(desktop,
                             { x: workspace.gridX,
                               y: workspace.gridY,
                               scale_x: workspace.gridScale,
                               scale_y: workspace.gridScale,
                               time: Overlay.ANIMATION_TIME,
                               transition: "easeOutQuad"
                             });

            // Now handle the rest of the windows in this workspace
            let wswindows = windows.filter(function (win) { return win.get_workspace() == w; });

            for (let i = 0, windowIndex = 0; i < wswindows.length; i++) {
                let win = wswindows[i];
                if (win.get_window_type() == Meta.WindowType.DESKTOP ||
                    win.is_override_redirect())
                    continue;

                let clone = this._createWindowClone(wswindows[i], workspace,
                                        wswindows.length - windowIndex - 1,
                                        wswindows.length);
                this._clones.push(clone);
                windowIndex++;
            }
        }

        // Track changes to the number of workspaces
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  function() {
                                      me._workspacesChanged();
                                  });
    },

    hide : function() {
        let global = Shell.Global.get();
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        this._positionWorkspaces(global, activeWorkspace);
        activeWorkspace.raise_top();
        
        this._clones.forEach(function (v, i, a) { if (v.cloneTitle) v.cloneTitle.destroy(); });
        this._clones = [];

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            let windows = workspace.get_children();

            for (let i = 0; i < windows.length; i++) {
                Tweener.addTween(windows[i],
                                 { x: workspace.zoomedOutX + windows[i].origX,
                                   y: workspace.zoomedOutY + windows[i].origY,
                                   scale_x: 1.0,
                                   scale_y: 1.0,
                                   time: Overlay.ANIMATION_TIME,
                                   opacity: 255,
                                   transition: "easeOutQuad"
                                 });
            }
        }

        Tweener.addTween(this._backdrop,
                         { x: this._backdropX,
                           y: this._backdropY,
                           width: this._backdropWidth,
                           height: this._backdropHeight,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    destroy : function() {
        let global = Shell.Global.get();

        for (let w = 0; w < this._workspaces.length; w++) {
            this._workspaces[w].destroy();
        }
        this._workspaces = [];

        this._backdrop.destroy();
        this._backdrop = null;

        global.screen.disconnect(this._nWorkspacesNotifyId);
    },

    // Assign grid positions to workspaces. We can't just do a simple
    // row-major or column-major numbering, because we don't want the
    // existing workspaces to get rearranged when we add a row or
    // column. So we alternate between adding to rows and adding to
    // columns. (So, eg, when going from a 2x2 grid of 4 workspaces to
    // a 3x2 grid of 5 workspaces, the 4 existing workspaces stay
    // where they are, and the 5th one is added to the end of the
    // first row.)
    //
    // FIXME: need to make the metacity internal layout agree with this!
    _positionWorkspaces : function(global, activeWorkspace) {
        if (!activeWorkspace) {
            let activeWorkspaceIndex = global.screen.get_active_workspace_index();
            activeWorkspace = this._workspaces[activeWorkspaceIndex];
        }

        let gridWidth = Math.ceil(Math.sqrt(this._workspaces.length));
        let gridHeight = Math.ceil(this._workspaces.length / gridWidth);

        let wsWidth = (this._width - (gridWidth - 1) * GRID_SPACING) / gridWidth;
        let wsHeight = (this._height - (gridHeight - 1) * GRID_SPACING) / gridHeight;
        let scale = wsWidth / global.screen_width;

        let span = 1, n = 0, row = 0, col = 0, horiz = true;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = row;
            workspace.gridCol = col;

            workspace.gridX = this._x + workspace.gridCol * (wsWidth + GRID_SPACING);
            workspace.gridY = this._y + workspace.gridRow * (wsHeight + GRID_SPACING);
            workspace.gridScale = scale;

            if (horiz) {
                col++;
                if (col == span) {
                    row = 0;
                    horiz = false;
                }
            } else {
                row++;
                if (row == span) {
                    col = 0;
                    horiz = true;
                    span++;
                }
            }
        }

        // Now figure out their zoomed-out coordinates
        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.zoomedOutX = (workspace.gridCol - activeWorkspace.gridCol) * (global.screen_width + GRID_SPACING);
            workspace.zoomedOutY = (workspace.gridRow - activeWorkspace.gridRow) * (global.screen_height + GRID_SPACING);
        }

        // And the backdrop
        this._backdropX = this._workspaces[0].zoomedOutX;
        this._backdropY = this._workspaces[0].zoomedOutY;
        this._backdropWidth = gridWidth * (global.screen_width + GRID_SPACING) - GRID_SPACING;
        this._backdropHeight = gridHeight * (global.screen_height + GRID_SPACING) - GRID_SPACING;
    },

    _setupDesktop : function(desktop, workspaceNum) {
        let global = Shell.Global.get();
        let workspace = global.screen.get_workspace_by_index(workspaceNum);
        desktop.connect("button-press-event",
                        function(clone, event) {
                            workspace.activate(event.get_time());
                            Main.hide_overlay();
                        });
        this._workspaces[workspaceNum].add_actor(desktop);
        desktop.origX = desktop.origY = 0;
    },

    _createDesktopActors : function(windows) {
        if (!windows) {
            let global = Shell.Global.get();
            windows = global.get_windows();
        }

        // Find the desktop window or windows
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_window_type() != Meta.WindowType.DESKTOP)
                continue;

            if (windows[i].get_meta_window().is_on_all_workspaces()) {
                for (let w = 0; w < this._workspaces.length; w++) {
                    if (this._workspaces[w].get_n_children() == 0)
                        this._setupDesktop(this._cloneWindow(windows[i]), w);
                }
                break;
            } else {
                let desktopWorkspace = windows[i].get_workspace;
                if (this._workspaces[desktopWorkspace].get_n_children() == 0)
                    this._setupDesktop(this._cloneWindow(windows[i]), desktopWorkspace);
            }
        }

        // Create dummy desktops for workspaces that don't have
        // desktop windows, and hook up button events on all desktops
        for (let w = 0; w < this._workspaces.length; w++) {
            if (this._workspaces[w].get_n_children() == 0)
                this._setupDesktop(this._createDesktopRectangle(), w);
        }
    },

    _cloneWindow : function(window) {
        let w = new Clutter.CloneTexture({ parent_texture: window.get_texture(),
                                           reactive: true,
                                           x: window.x,
                                           y: window.y });
        w.origX = window.x;
        w.origY = window.y;
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
    
    _addCloneTitle : function (clone, window) {
         let transformed = clone.get_transformed_size();
         let icon = window.meta_window.mini_icon;
         let iconTexture = new Clutter.Texture({ width: 16, height: 16, keep_aspect_ratio: true});
         Shell.clutter_texture_set_from_pixbuf(iconTexture, icon);
         let box = new Big.Box({background_color : WINDOWCLONE_BG_COLOR,
                                y_align: Big.BoxAlignment.CENTER,
                                corner_radius: 5,
                                padding: 4,
                                spacing: 4,
                                orientation: Big.BoxOrientation.HORIZONTAL});
         box.append(iconTexture, Big.BoxPackFlags.NONE);
         let title = new Clutter.Label({color: WINDOWCLONE_TITLE_COLOR,
                                        font_name: "Sans 14",
                                        text: window.meta_window.title,
                                        ellipsize: Pango.EllipsizeMode.END});
         // Get current width (just the icon), with spacing, plus title
         let width = box.width + box.spacing + title.width;
         let maxWidth = transformed[0]; 
         if (width > transformed[0])
             width = transformed[0];
         box.width = width;
         box.append(title, Big.BoxPackFlags.EXPAND);
         box.set_position(clone.x, clone.y);
         let parent = clone.get_parent();
         clone.cloneTitle = box;
         parent.add_actor(box);
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

    _createWindowClone : function(w, workspace, windowIndex, numberOfWindows) {
        let me = this;
        let global = Shell.Global.get();

        // We show the window using "clones" of the texture .. separate
        // actors that mirror the original actors for the window. For
        // animation purposes, it may be better to actually move the
        // original actors about instead.

        let clone = this._cloneWindow(w);
        let [xCenter, yCenter, fraction] = this._computeWindowPosition(windowIndex, numberOfWindows);

        let desiredSize = global.screen_width * fraction;

        xCenter = workspace.gridX + workspace.gridScale * (xCenter * global.screen_width);
        yCenter = workspace.gridY + workspace.gridScale * (yCenter * global.screen_height);

        let size = clone.width;
        if (clone.height > size)
            size = clone.height;

        // Never scale up
        let scale = desiredSize / size;
        if (scale > 1)
            scale = 1;
        scale *= workspace.gridScale;

        workspace.add_actor(clone);

        clone.set_position(workspace.zoomedOutX + clone.origX,
                           workspace.zoomedOutY + clone.origY);
        Tweener.addTween(clone,
                         { x: xCenter - 0.5 * scale * w.width,
                           y: yCenter - 0.5 * scale * w.height,
                           scale_x: scale,
                           scale_y: scale,
                           time: Overlay.ANIMATION_TIME,
                           opacity: WINDOW_OPACITY,
                           transition: "easeOutQuad",
                           onComplete: function () {
                               me._addCloneTitle(clone, w);
                           }                               
                         });

        clone.connect("button-press-event",
                      function(clone, event) {
                          clone.raise_top();
                          me._activateWindow(w, event.get_time());
                      });
        return clone;
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
        Main.hide_overlay();
    },

    _workspacesChanged : function() {
        let global = Shell.Global.get();

        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        let oldScale = this._workspaces[0].gridScale;
        let oldGridWidth = Math.ceil(Math.sqrt(oldNumWorkspaces));
        let oldGridHeight = Math.ceil(oldNumWorkspaces / oldGridWidth);
        let lostWorkspaces = [];

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Create new workspace groups
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._workspaces[w] = new Clutter.Group();
                this.actor.add_actor(this._workspaces[w]);
            }
        } else {
            // Truncate the list of workspaces
            // FIXME: assumes that the workspaces are being removed from
            // the end of the list, not the start/middle
            lostWorkspaces = this._workspaces.splice(newNumWorkspaces);
        }

        // Figure out the new layout
        this._positionWorkspaces(global);
        let newScale = this._workspaces[0].gridScale;
        let newGridWidth = Math.ceil(Math.sqrt(newNumWorkspaces));
        let newGridHeight = Math.ceil(newNumWorkspaces / newGridWidth);

        if (newGridWidth != oldGridWidth || newGridHeight != oldGridHeight) {
            // We need to resize/move the existing workspaces/windows
            let existingWorkspaces = Math.min(oldNumWorkspaces, newNumWorkspaces);
            for (let w = 0; w < existingWorkspaces; w++) {
                let workspace = this._workspaces[w];
                let windows = workspace.get_children();
                let desktop = windows[0];

                for (let i = 0; i < windows.length; i++) {
                    let newX = workspace.gridX + (windows[i].x - desktop.x) * newScale / oldScale;
                    let newY = workspace.gridY + (windows[i].y - desktop.y) * newScale / oldScale;
                    let newWindowScale = windows[i].scale_x * newScale / oldScale;

                    Tweener.addTween(windows[i],
                                     { x: newX,
                                       y: newY,
                                       scale_x: newWindowScale,
                                       scale_y: newWindowScale,
                                       time: Overlay.ANIMATION_TIME,
                                       transition: "easeOutQuad"
                                     });
                }
            }
        }

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Slide new workspaces in from offscreen
            this._createDesktopActors();
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                let workspace = this._workspaces[w];
                let desktop = this._workspaces[w].get_nth_child(0);
                if (workspace.gridCol > workspace.gridRow) {
                    desktop.set_position(global.screen_width, workspace.gridY);
                    desktop.set_scale(oldScale, oldScale);
                } else {
                    desktop.set_position(workspace.gridX, global.screen_height);
                    desktop.set_scale(workspace.gridScale, workspace.gridScale);
                }
                Tweener.addTween(desktop,
                                 { x: workspace.gridX,
                                   y: workspace.gridY,
                                   scale_x: workspace.gridScale,
                                   scale_y: workspace.gridScale,
                                   time: Overlay.ANIMATION_TIME,
                                   transition: "easeOutQuad"
                                 });
            }
        } else {
            // Slide old workspaces out
            for (let w = 0; w < lostWorkspaces.length; w++) {
                let workspace = lostWorkspaces[w];
                let desktop = lostWorkspaces[w].get_nth_child(0);
                let destX = desktop.x, destY = desktop.y;
                if (workspace.gridCol > workspace.gridRow)
                    destX = global.screen_width;
                else
                    destY = global.screen_height;
                Tweener.addTween(desktop,
                                 { x: destX,
                                   y: destY,
                                   scale_x: newScale,
                                   scale_y: newScale,
                                   time: Overlay.ANIMATION_TIME,
                                   transition: "easeOutQuad",
                                   onComplete: function() { workspace.destroy(); }
                                 });
            }

            // FIXME: deal with windows on the lost workspaces
        }
    }
};
