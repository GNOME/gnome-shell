/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const DND = imports.ui.dnd;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Overlay = imports.ui.overlay;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;

// Windows are slightly translucent in the overlay mode
const WINDOW_OPACITY = 0.9 * 255;
const FOCUS_ANIMATION_TIME = 0.15;

const WINDOWCLONE_BG_COLOR = new Clutter.Color();
WINDOWCLONE_BG_COLOR.from_pixel(0x000000f0);
const WINDOWCLONE_TITLE_COLOR = new Clutter.Color();
WINDOWCLONE_TITLE_COLOR.from_pixel(0xffffffff);
const FRAME_COLOR = new Clutter.Color();
FRAME_COLOR.from_pixel(0xffffffff);

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
const FRAME_SIZE = GRID_SPACING / 3;

let buttonSize = false;

function WindowClone(realWindow) {
    this._init(realWindow);
}

WindowClone.prototype = {
    _init : function(realWindow) {
        this.actor = new Clutter.CloneTexture({ parent_texture: realWindow.get_texture(),
                                                reactive: true,
                                                x: realWindow.x,
                                                y: realWindow.y });
        this.actor._delegate = this;
        this.realWindow = realWindow;
        this.metaWindow = realWindow.meta_window;
        this.origX = realWindow.x;
        this.origY = realWindow.y;

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));

        this.actor.connect('enter-event',
                           Lang.bind(this, this._onEnter));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onLeave));
        this._havePointer = false;

        this._draggable = DND.makeDraggable(this.actor);
        this._draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));
        this._draggable.connect('drag-end', Lang.bind(this, this._onDragEnd));
        this._inDrag = false;
    },

    destroy: function () {
        this.actor.destroy();
        if (this._title)
            this._title.destroy();
    },

    _onEnter: function (actor, event) {
        // If the user drags faster than we can follow, he'll end up
        // leaving the window temporarily and then re-entering it
        if (this._inDrag)
            return;

        this._havePointer = true;

        actor.raise_top();
        this._updateTitle();
    },

    _onLeave: function (actor, event) {
        // If the user drags faster than we can follow, he'll end up
        // leaving the window temporarily and then re-entering it
        if (this._inDrag)
            return;

        this._havePointer = false;

        if (Tweener.isTweening(this.actor))
            return;

    	actor.raise(this.stackAbove);
        this._updateTitle();
    },

    _onButtonRelease : function (actor, event) {
        this.emit('selected', event.get_time());
    },

    _onDragBegin : function (draggable, time) {
        this._inDrag = true;
        this._updateTitle();
    },

    _onDragEnd : function (draggable, time) {
        this._inDrag = false;

        // Most likely, the clone is going to move away from the
        // pointer now. But that won't cause a leave-event, so
        // do this by hand. Of course, if the window only snaps
        // back a short distance, this might be wrong, but it's
        // better to have the label mysteriously missing than
        // mysteriously present
        this._havePointer = false;
    },

    // Called by Tweener
    onAnimationStart : function () {
        this._updateTitle();
    },

    // Called by Tweener
    onAnimationComplete : function () {
        this._updateTitle();
        this.actor.raise(this.stackAbove);
    },

    _createTitle : function () {
        let window = this.realWindow;
        
        let box = new Big.Box({ background_color : WINDOWCLONE_BG_COLOR,
                                y_align: Big.BoxAlignment.CENTER,
                                corner_radius: 5,
                                padding: 4,
                                spacing: 4,
                                orientation: Big.BoxOrientation.HORIZONTAL });
        
        let icon = this.metaWindow.mini_icon;
        let iconTexture = new Clutter.Texture({ x: this.actor.x,
                                                y: this.actor.y + this.actor.height - 16,
                                                width: 16,
                                                height: 16,
                                                keep_aspect_ratio: true });
        Shell.clutter_texture_set_from_pixbuf(iconTexture, icon);
        box.append(iconTexture, Big.BoxPackFlags.NONE);
        
        let title = new Clutter.Label({ color: WINDOWCLONE_TITLE_COLOR,
                                        font_name: "Sans 12",
                                        text: this.metaWindow.title,
                                        ellipsize: Pango.EllipsizeMode.END
                                      });
        box.append(title, Big.BoxPackFlags.EXPAND);
        // Get and cache the expected width (just the icon), with spacing, plus title
        box.fullWidth = box.width;
        box.hide(); // Hidden by default, show on mouseover
        this._title = box;        

        // Make the title a sibling of the window
        this.actor.get_parent().add_actor(box);
    },

    _adjustTitle : function () {
        let title = this._title;
        if (!title)
            return;    

        let [cloneScreenWidth, cloneScreenHeight] = this.actor.get_transformed_size();
        let [titleScreenWidth, titleScreenHeight] = title.get_transformed_size();

        // Titles are supposed to be "full-size", so adjust its
        // scale to counteract the scaling of its ancestor actors.
        title.set_scale(title.width / titleScreenWidth * title.scale_x,
                        title.height / titleScreenHeight * title.scale_y);

        title.width = Math.min(title.fullWidth, cloneScreenWidth);
        let xoff = ((cloneScreenWidth - title.width) / 2) * title.scale_x;
        title.set_position(this.actor.x + xoff, this.actor.y);
    },

    _showTitle : function () {
        if (!this._title)
            this._createTitle();

        this._adjustTitle();
        this._title.show();
        this._title.raise(this.actor);
    },

    _hideTitle : function () {
        if (!this._title)
            return;

        this._title.hide();
    },

    _updateTitle : function () {
        let shouldShow = (this._havePointer &&
                          !this._inDrag &&
                          !Tweener.isTweening(this.actor));

        if (shouldShow)
            this._showTitle();
        else
            this._hideTitle();
    }
};

Signals.addSignalMethods(WindowClone.prototype);


function DesktopClone(window) {
    this._init(window);
}

DesktopClone.prototype = {
    _init : function(window) {
        if (window) {
            this.actor = new Clutter.CloneTexture({ parent_texture: window.get_texture(),
                                                    reactive: true });
        } else {
            let global = Shell.Global.get();
            this.actor = new Clutter.Rectangle({ color: global.stage.color,
                                                 reactive: true,
                                                 width: global.screen_width,
                                                 height: global.screen_height });
        }

        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));
    },

    _onButtonRelease : function (actor, event) {
        this.emit('selected', event.get_time());
    }
};

Signals.addSignalMethods(DesktopClone.prototype);


function Workspace(workspaceNum) {
    this._init(workspaceNum);
}

Workspace.prototype = {
    _init : function(workspaceNum) {
        let me = this;
        let global = Shell.Global.get();

        this.workspaceNum = workspaceNum;
        this._metaWorkspace = global.screen.get_workspace_by_index(workspaceNum);

        this.actor = new Clutter.Group();
        this.actor._delegate = this;
        this.scale = 1.0;

        let windows = global.get_windows().filter(this._isMyWindow, this);

        // Find the desktop window
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_window_type() == Meta.WindowType.DESKTOP) {
                this._desktop = new DesktopClone(windows[i]);
                break;
            }
        }
        // If there wasn't one, fake it
        if (!this._desktop)
            this._desktop = new DesktopClone();

        this._desktop.connect('selected',
                              Lang.bind(this,
                                        function(clone, time) {
                                            this._metaWorkspace.activate(time);
                                            Main.hide_overlay();
                                        }));
        this.actor.add_actor(this._desktop.actor);

        // Create clones for remaining windows that should be
        // visible in the overlay
        this._windows = [this._desktop];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverlayWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        // Track window changes
        this._windowAddedId = this._metaWorkspace.connect('window-added',
                                                          Lang.bind(this, this._windowAdded));
        this._windowRemovedId = this._metaWorkspace.connect('window-removed',
                                                            Lang.bind(this, this._windowRemoved));

        this._removeButton = null;
        this._visible = false;

        this._frame = null;

        this.leavingOverlay = false;
    },

    updateRemovable : function() {
        let global = Shell.Global.get();
        let removable = (this._windows.length == 1 /* just desktop */ &&
                         this.workspaceNum == global.screen.n_workspaces - 1);

        if (removable) {
            if (this._removeButton)
                return;

            this._removeButton = new Clutter.Texture({ width: buttonSize,
                                                       height: buttonSize,
                                                       reactive: true
                                                     });
            this._removeButton.set_from_file(global.imagedir + "remove-workspace.svg");
            this._removeButton.connect('button-release-event', Lang.bind(this, this._removeSelf));

            this.actor.add_actor(this._removeButton);
            this._adjustRemoveButton();
            this._adjustRemoveButtonId = this.actor.connect('notify::scale-x', Lang.bind(this, this._adjustRemoveButton));

            if (this._visible) {
                this._removeButton.set_opacity(0);
                Tweener.addTween(this._removeButton,
                                 { opacity: 255,
                                   time: Overlay.ANIMATION_TIME,
                                   transition: "easeOutQuad"
                                 });
            }
        } else {
            if (!this._removeButton)
                return;

            if (this._visible) {
                Tweener.addTween(this._removeButton,
                                 { opacity: 0,
                                   time: Overlay.ANIMATION_TIME,
                                   transition: "easeOutQuad",
                                   onComplete: this._removeRemoveButton,
                                   onCompleteScope: this
                                 });
            } else
                this._removeRemoveButton();
        }
    },

    _adjustRemoveButton : function() {
        this._removeButton.set_scale(1.0 / this.actor.scale_x,
                                     1.0 / this.actor.scale_y);
        this._removeButton.set_position(
            (this.actor.width - this._removeButton.width / this.actor.scale_x) / 2,
            (this.actor.height - this._removeButton.height / this.actor.scale_y) / 2);
    },

    _removeRemoveButton : function() {
        this._removeButton.destroy();
        this._removeButton = null;
        this.actor.disconnect(this._adjustRemoveButtonId);
    },

    // Mark the workspace selected/not-selected
    setSelected : function(selected) {
        if (selected) {
            if (this._frame)
                return;

            // FIXME: do something cooler-looking using clutter-cairo
            this._frame = new Clutter.Rectangle({ color: FRAME_COLOR });
            this.actor.add_actor(this._frame);
            this._frame.set_position(this._desktop.actor.x - FRAME_SIZE / this.actor.scale_x,
                                     this._desktop.actor.y - FRAME_SIZE / this.actor.scale_y);
            this._frame.set_size(this._desktop.actor.width + 2 * FRAME_SIZE / this.actor.scale_x,
                                 this._desktop.actor.height + 2 * FRAME_SIZE / this.actor.scale_y);
            this._frame.lower_bottom();

            this._framePosHandler = this.actor.connect('notify::x', Lang.bind(this, this._updateFramePosition));
            this._frameSizeHandler = this.actor.connect('notify::scale-x', Lang.bind(this, this._updateFrameSize));
        } else {
            if (!this._frame)
                return;
            this.actor.disconnect(this._framePosHandler);
            this.actor.disconnect(this._frameSizeHandler);
            this._frame.destroy();
            this._frame = null;
        }
    },

    _updateFramePosition : function() {
        this._frame.set_position(this._desktop.actor.x - FRAME_SIZE / this.actor.scale_x,
                                 this._desktop.actor.y - FRAME_SIZE / this.actor.scale_y);
    },

    _updateFrameSize : function() {
        this._frame.set_size(this._desktop.actor.width + 2 * FRAME_SIZE / this.actor.scale_x,
                             this._desktop.actor.height + 2 * FRAME_SIZE / this.actor.scale_y);
    },

    // Reposition all windows in their zoomed-to-overlay position. if workspaceZooming
    // is true, then the workspace is moving at the same time and we need to take
    // that into account
    _positionWindows : function(workspaceZooming) {
        let global = Shell.Global.get();

        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];
            clone.stackAbove = this._windows[i - 1].actor;

            let [xCenter, yCenter, fraction] = this._computeWindowPosition(i);
            xCenter = xCenter * global.screen_width;
            yCenter = yCenter * global.screen_height;

            let size = Math.max(clone.actor.width, clone.actor.height);
            let desiredSize = global.screen_width * fraction;
            let scale = Math.min(desiredSize / size, 1.0);

            Tweener.addTween(clone.actor, 
                             { x: xCenter - 0.5 * scale * clone.actor.width,
                               y: yCenter - 0.5 * scale * clone.actor.height,
                               scale_x: scale,
                               scale_y: scale,
                               workspace_relative: workspaceZooming ? this : null,
                               time: Overlay.ANIMATION_TIME,
                               opacity: WINDOW_OPACITY,
                               transition: "easeOutQuad"
                             });
        }
    },

    _windowRemoved : function(metaWorkspace, metaWin) {
        let global = Shell.Global.get();
        let win = metaWin.get_compositor_private();

        // find the position of the window in our list
        let index = - 1, clone;
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].metaWindow == metaWin) {
                index = i;
                clone = this._windows[index];
                break;
            }
        }

        if (index == -1)
            return;

        this._windows.splice(index, 1);

        // If metaWin.get_compositor_private() returned non-NULL, that
        // means the window still exists (and is just being moved to
        // another workspace or something), so set its overlayHint
        // accordingly. (If it returned NULL, then the window is being
        // destroyed; we'd like to animate this, but it's too late at
        // this point.)
        if (win) {
            let [stageX, stageY] = clone.actor.get_transformed_position();
            let [stageWidth, stageHeight] = clone.actor.get_transformed_size();
            win._overlayHint = {
                x: stageX,
                y: stageY,
                scale: stageWidth / clone.actor.width
            };
        }
        clone.destroy();

        this._positionWindows();
        this.updateRemovable();
    },

    _windowAdded : function(metaWorkspace, metaWin) {
        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            Mainloop.idle_add(Lang.bind(this,
                                        function () {
                                            if (this.actor && metaWin.get_compositor_private())
                                                this._windowAdded(metaWorkspace, metaWin);
                                            return false;
                                        }));
            return;
        }
        
        if (!this._isOverlayWindow(win))
            return;        

        let clone = this._addWindowClone(win);

        if (win._overlayHint) {
            let x = (win._overlayHint.x - this.actor.x) / this.scale;
            let y = (win._overlayHint.y - this.actor.y) / this.scale;
            let scale = win._overlayHint.scale / this.scale;
            delete win._overlayHint;

            clone.actor.set_position (x, y);
            clone.actor.set_scale (scale, scale);
        }

        this._positionWindows();
        this.updateRemovable();
    },

    // Animate the full-screen to overlay transition.
    zoomToOverlay : function() {
        // Move the workspace into size/position
        this.actor.set_position(this.fullSizeX, this.fullSizeY);
        
        this.updateInOverlay();

        this._visible = true;
    },

    // Animates the display of a workspace and its windows to have the current dimensions and position.
    updateInOverlay : function() {
        Tweener.addTween(this.actor,
                         { x: this.gridX,
                           y: this.gridY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        // Likewise for each of the windows in the workspace.
        this._positionWindows(true);
    },

    // Animates the return from overlay mode
    zoomFromOverlay : function() {
        this.leavingOverlay = true; 
 
        Tweener.addTween(this.actor,
                         { x: this.fullSizeX,
                           y: this.fullSizeY,
                           scale_x: 1.0,
                           scale_y: 1.0,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        for (let i = 1; i < this._windows.length; i++) {
            let clone = this._windows[i];
            Tweener.addTween(clone.actor,
                             { x: clone.origX,
                               y: clone.origY,
                               scale_x: 1.0,
                               scale_y: 1.0,
                               workspace_relative: this,
                               time: Overlay.ANIMATION_TIME,
                               opacity: 255,
                               transition: "easeOutQuad"
                             });
        }

        this.leavingOverlay = false;
        this._visible = false;
        
    },

    // Animates grid shrinking/expanding when a row or column
    // of workspaces is added or removed
    resizeToGrid : function (oldScale) {
        Tweener.addTween(this.actor,
                         { x: this.gridX,
                           y: this.gridY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },
    
    // Animates the addition of a new (empty) workspace
    slideIn : function(oldScale) {
        let global = Shell.Global.get();

        if (this.gridCol > this.gridRow) {
            this.actor.set_position(global.screen_width, this.gridY);
            this.actor.set_scale(oldScale, oldScale);
        } else {
            this.actor.set_position(this.gridX, global.screen_height);
            this.actor.set_scale(this.scale, this.scale);
        }
        Tweener.addTween(this.actor,
                         { x: this.gridX,
                           y: this.gridY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        this._visible = true;
    },
    
    // Animates the removal of a workspace
    slideOut : function(onComplete) {
        let global = Shell.Global.get();
        let destX = this.actor.x, destY = this.actor.y;

        if (this.gridCol > this.gridRow)
            destX = global.screen_width;
        else
            destY = global.screen_height;
        Tweener.addTween(this.actor,
                         { x: destX,
                           y: destY,
                           scale_x: this.scale,
                           scale_y: this.scale,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: onComplete
                         });

        this._visible = false;

        // Don't let the user try to select this workspace as it's
        // making its exit.
        this._desktop.reactive = false;
    },
    
    destroy : function() {
        let global = Shell.Global.get();

        Tweener.removeTweens(this.actor);
        this.actor.destroy();
        this.actor = null;

        this._metaWorkspace.disconnect(this._windowAddedId);
        this._metaWorkspace.disconnect(this._windowRemovedId);
    },

    // Tests if @win belongs to this workspaces
    _isMyWindow : function (win) {
        return win.get_workspace() == this.workspaceNum ||
            (win.get_meta_window() && win.get_meta_window().is_on_all_workspaces());
    },

    // Tests if @win should be shown in the overlay
    _isOverlayWindow : function (win) {
        let wintype = win.get_window_type();
        if (wintype == Meta.WindowType.DESKTOP || 
            wintype == Meta.WindowType.DOCK)
            return false;
        return !win.is_override_redirect();
    },

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone : function(win) {
        let clone = new WindowClone(win);
        clone.connect('selected',
                      Lang.bind(this, this._onCloneSelected));
        clone.connect('dragged',
                      Lang.bind(this, this._onCloneDragged));

        this.actor.add_actor(clone.actor);
        this._windows.push(clone);

        return clone;
    },

    _computeWindowPosition : function(index) {
        // ignore this._windows[0], which is the desktop
        let windowIndex = index - 1;
        let numberOfWindows = this._windows.length - 1;

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

    _onCloneDragged : function (clone, stageX, stageY, time) {
        this.emit('window-dragged', clone, stageX, stageY, time);
    },

    _onCloneSelected : function (clone, time) {
        let global = Shell.Global.get();
        let activeWorkspace = global.screen.get_active_workspace_index();
        let windowWorkspace = clone.realWindow.get_workspace();

        if (windowWorkspace != activeWorkspace) {
            let workspace = global.screen.get_workspace_by_index(windowWorkspace);
            workspace.activate_with_focus(clone.metaWindow, time);
        } else
            clone.metaWindow.activate(time);
        Main.hide_overlay();
    },

    _removeSelf : function(actor, event) {
        let global = Shell.Global.get();
        let screen = global.screen;
        let workspace = screen.get_workspace_by_index(this.workspaceNum);

        screen.remove_workspace(workspace, event.get_time());
        return true;
    },

    // Draggable target interface
    acceptDrop : function(source, actor, x, y, time) {
        let global = Shell.Global.get();

        if (source instanceof WindowClone) {
            let win = source.realWindow;
            if (this._isMyWindow(win))
                return false;

            // Set a hint on the Mutter.Window so its initial position
            // in the new workspace will be correct
            win._overlayHint = {
                x: actor.x,
                y: actor.y,
                scale: actor.scale_x
            };

            let metaWindow = win.get_meta_window();
            metaWindow.change_workspace_by_index(this.workspaceNum,
                                                 false, // don't create workspace
                                                 time);
            return true;
        } else if (source instanceof GenericDisplay.GenericDisplayItem) {
            this._metaWorkspace.activate(time);
            source.activate();
            return true;
        }

        return false;
    }
};

Signals.addSignalMethods(Workspace.prototype);

function Workspaces(width, height, x, y, addButtonSize, addButtonX, addButtonY) {
    this._init(width, height, x, y, addButtonSize, addButtonX, addButtonY);
}

Workspaces.prototype = {
    _init : function(width, height, x, y, addButtonSize, addButtonX, addButtonY) {
        let global = Shell.Global.get();

        this.actor = new Clutter.Group();

        let screenHeight = global.screen_height;
          
        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;

        this._workspaces = [];
        
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace;

        // Create and position workspace objects
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._addWorkspaceActor(w);
            if (w == activeWorkspaceIndex) {
                activeWorkspace = this._workspaces[w];
                activeWorkspace.setSelected(true);
            }
        }
        activeWorkspace.actor.raise_top();
        this._positionWorkspaces(global, activeWorkspace);

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

        // Create (+) button
        buttonSize = addButtonSize;
        this.addButton = new Clutter.Texture({ x: addButtonX,
                                                y: addButtonY,
                                                width: buttonSize,
                                                height: buttonSize,
                                                reactive: true
                                              });
        this.addButton.set_from_file(global.imagedir + "add-workspace.svg");
        this.addButton.connect('button-release-event', this._appendNewWorkspace);
        this.actor.add_actor(this.addButton);
        this.addButton.lower_bottom();

        let lastWorkspace = this._workspaces[this._workspaces.length - 1];
        lastWorkspace.updateRemovable(true);

        // Position/scale the desktop windows and their children
        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomToOverlay();

        // Track changes to the number of workspaces
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));
    },

    // Updates position of the workspaces display based on the new coordinates.
    // Preserves the old value for the coordinate, if the passed value is null.
    updatePosition : function(x, y) {
        if (x != null)
            this._x = x;
        if (y != null)
            this._y = y;

        this._updateInOverlay();
    },

    hide : function() {
        let global = Shell.Global.get();
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        this._positionWorkspaces(global, activeWorkspace);
        activeWorkspace.actor.raise_top();

        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomFromOverlay();

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

        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].destroy();
        this._workspaces = [];

        this.actor.destroy();
        this.actor = null;
        this._backdrop = null;

        global.screen.disconnect(this._nWorkspacesNotifyId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
    },

    // Updates the workspaces display based on the current dimensions and position.
    _updateInOverlay : function() {
        let global = Shell.Global.get();  
  
        this._positionWorkspaces(global);
        Tweener.addTween(this._backdrop,
                         { x: this._x,
                           y: this._y,
                           width: this._width,
                           height: this._height,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        // Position/scale the desktop windows and their children
        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].updateInOverlay();
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
            workspace.scale = scale;

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

        // Now figure out their full-size coordinates
        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.fullSizeX = (workspace.gridCol - activeWorkspace.gridCol) * (global.screen_width + GRID_SPACING);
            workspace.fullSizeY = (workspace.gridRow - activeWorkspace.gridRow) * (global.screen_height + GRID_SPACING);
        }

        // And the backdrop
        this._backdropX = this._workspaces[0].fullSizeX;
        this._backdropY = this._workspaces[0].fullSizeY;
        this._backdropWidth = gridWidth * (global.screen_width + GRID_SPACING) - GRID_SPACING;
        this._backdropHeight = gridHeight * (global.screen_height + GRID_SPACING) - GRID_SPACING;
    },

    _workspacesChanged : function() {
        let global = Shell.Global.get();

        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        let oldScale = this._workspaces[0].scale;
        let oldGridWidth = Math.ceil(Math.sqrt(oldNumWorkspaces));
        let oldGridHeight = Math.ceil(oldNumWorkspaces / oldGridWidth);
        let lostWorkspaces = [];

        // The old last workspace is no longer removable.
        this._workspaces[oldNumWorkspaces - 1].updateRemovable();

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Create new workspace groups
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._addWorkspaceActor(w);
            }

        } else {
            // Truncate the list of workspaces
            // FIXME: assumes that the workspaces are being removed from
            // the end of the list, not the start/middle
            lostWorkspaces = this._workspaces.splice(newNumWorkspaces);
        }

        // The new last workspace may be removable
        let newLastWorkspace = this._workspaces[this._workspaces.length - 1];
        newLastWorkspace.updateRemovable();

        // Figure out the new layout
        this._positionWorkspaces(global);
        let newScale = this._workspaces[0].scale;
        let newGridWidth = Math.ceil(Math.sqrt(newNumWorkspaces));
        let newGridHeight = Math.ceil(newNumWorkspaces / newGridWidth);

        if (newGridWidth != oldGridWidth || newGridHeight != oldGridHeight) {
            // We need to resize/move the existing workspaces/windows
            let existingWorkspaces = Math.min(oldNumWorkspaces, newNumWorkspaces);
            for (let w = 0; w < existingWorkspaces; w++)
                this._workspaces[w].resizeToGrid(oldScale);
        }

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Slide new workspaces in from offscreen
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._workspaces[w].slideIn(oldScale);
        } else {
            // Slide old workspaces out
            for (let w = 0; w < lostWorkspaces.length; w++) {
                let workspace = lostWorkspaces[w];
                workspace.actor.raise(this._backdrop);
                workspace.slideOut(function () { workspace.destroy(); });
            }

            // FIXME: deal with windows on the lost workspaces
        }
    },

    _activeWorkspaceChanged : function(wm, from, to, direction) {
        this._workspaces[from].setSelected(false);
        this._workspaces[to].setSelected(true);
    },

    _addWorkspaceActor : function(workspaceNum) {
        let workspace  = new Workspace(workspaceNum);
        this._workspaces[workspaceNum] = workspace;
        this.actor.add_actor(workspace.actor);
    },

    _appendNewWorkspace : function(actor, event) {
        let global = Shell.Global.get();

        global.screen.append_new_workspace(false, event.get_time());
    }
};

// Create a SpecialPropertyModifier to let us move windows in a
// straight line on the screen even though their containing workspace
// is also moving.
Tweener.registerSpecialPropertyModifier("workspace_relative", _workspace_relative_modifier, _workspace_relative_get);

function _workspace_relative_modifier(workspace) {
    let endX, endY;

    if (!workspace)
        return [];

    if (workspace.leavingOverlay) {
        endX = workspace.fullSizeX;
        endY = workspace.fullSizeY;        
    } else {
        endX = workspace.gridX;
        endY = workspace.gridY;
    }

    return [ { name: "x",
               parameters: { begin: workspace.actor.x, end: endX,
                             cur: function() { return workspace.actor.x; } } },
             { name: "y",
               parameters: { begin: workspace.actor.y, end: endY,
                             cur: function() { return workspace.actor.y; } } }
           ];
}

function _workspace_relative_get(begin, end, time, params) {
    return (begin + params.begin) + time * (end + params.end - (begin + params.begin)) - params.cur();
}
