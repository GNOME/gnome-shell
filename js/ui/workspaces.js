/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Tweener = imports.tweener.tweener;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Signals = imports.signals;

const Main = imports.ui.main;
const Overlay = imports.ui.overlay;
const Panel = imports.ui.panel;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Big = imports.gi.Big;
const GdkPixbuf = imports.gi.GdkPixbuf;

// Windows are slightly translucent in the overlay mode
const WINDOW_OPACITY = 0.9 * 255;
const FOCUS_ANIMATION_TIME = 0.15;
const SNAP_BACK_ANIMATION_TIME = 0.25;

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

function WindowClone(realWindow) {
    this._init(realWindow);
}

WindowClone.prototype = {
    _init : function(realWindow) {
        this.actor = new Clutter.CloneTexture({ parent_texture: realWindow.get_texture(),
                                                reactive: true,
                                                x: realWindow.x,
                                                y: realWindow.y });
        this.realWindow = realWindow;
        this.origX = realWindow.x;
        this.origY = realWindow.y;

        this.actor.connect('button-press-event',
                           Lang.bind(this, this._onButtonPress));
        this.actor.connect('button-release-event',
                           Lang.bind(this, this._onButtonRelease));
        this.actor.connect('enter-event',
                           Lang.bind(this, this._onEnter));
        this.actor.connect('leave-event',
                           Lang.bind(this, this._onLeave));
        this.actor.connect('motion-event',
                           Lang.bind(this, this._onMotion));

        this._havePointer = false;
        this._inDrag = false;
        this._buttonDown = false;

        // We track the number of animations we are doing for the clone so we can
        // hide the floating title while animating. It seems like it should be
        // possible to use Tweener.getTweenCount(clone), but that annoyingly only
        // updates after onComplete is called.
        this._animationCount = 0;
    },

    destroy: function () {
        this.actor.destroy();
        if (this._title)
            this._title.destroy();
    },
    
    addTween: function (params) {
        this._animationCount++;
        this._updateTitle();

        if (params.onComplete) {
            let oldOnComplete = params.onComplete;
            let oldOnCompleteScope = params.onCompleteScope;
            let oldOnCompleteParams = params.onCompleteParams;
            let eventScope = oldOnCompleteScope ? oldOnCompleteScope : this.actor;

            params.onComplete = function () {
                oldOnComplete.apply(eventScope, oldOnCompleteParams);
                this._onAnimationComplete();
            };
        } else
            params.onComplete = this._onAnimationComplete;
        params.onCompleteScope = this;

        Tweener.addTween(this.actor, params);
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

        if (this._animationCount)
            return;

    	actor.raise(this.stackAbove);
        this._updateTitle();
    },

    _onButtonPress : function (actor, event) {
        if (this._animationCount)
            return;

        actor.raise_top();

        let [stageX, stageY] = event.get_coords();

        this._buttonDown = true;
        this._dragStartX = stageX;
        this._dragStartY = stageY;

        Clutter.grab_pointer(actor);

        this._updateTitle();
    },

    _onMotion : function (actor, event) {
        if (!this._buttonDown)
            return;

        let [stageX, stageY] = event.get_coords();

        // If we haven't begun a drag, see if the user has moved the mouse enough
        // to trigger a drag
        let dragThreshold = Gtk.Settings.get_default().gtk_dnd_drag_threshold;
        if (!this._inDrag &&
            (Math.abs(stageX - this._dragStartX) > dragThreshold ||
             Math.abs(stageY - this._dragStartY) > dragThreshold)) {
            this._inDrag = true;
            
            this._dragOrigParent = actor.get_parent();
            this._dragOrigX = actor.x;
            this._dragOrigY = actor.y;
            this._dragOrigScale = actor.scale_x;

            let [cloneStageX, cloneStageY] = actor.get_transformed_position();
            this._dragOffsetX = cloneStageX - this._dragStartX;
            this._dragOffsetY = cloneStageY - this._dragStartY;

            // Reparent the clone onto the stage, but keeping the same scale.
            // (the set_position call below will take care of position.)
            let [scaledWidth, scaledHeight] = actor.get_transformed_size();
            actor.reparent(actor.get_stage());
            actor.raise_top();
            actor.set_scale(scaledWidth / actor.width,
                            scaledHeight / actor.height);
        }

        // If we are dragging, update the position
        if (this._inDrag) {
            actor.set_position(stageX + this._dragOffsetX,
                               stageY + this._dragOffsetY);
        }
    },

    _onButtonRelease : function (actor, event) {
        Clutter.ungrab_pointer();

        let inDrag = this._inDrag;
        this._buttonDown = false;
        this._inDrag = false;

        if (inDrag) {
            let [stageX, stageY] = event.get_coords();

            let origWorkspace = this.realWindow.get_workspace();
            this.emit('dragged', stageX, stageY, event.get_time());
            if (this.realWindow.get_workspace() == origWorkspace) {
                // Didn't get moved elsewhere, restore position
                this.addTween({ x: this._dragStartX + this._dragOffsetX,
                                y: this._dragStartY + this._dragOffsetY,
                                time: SNAP_BACK_ANIMATION_TIME,
                                transition: "easeOutQuad",
                                onComplete: this._onSnapBackComplete,
                                onCompleteScope: this
                              });
                // Most likely, the clone is going to move away from the
                // pointer now. But that won't cause a leave-event, so
                // do this by hand. Of course, if the window only snaps
                // back a short distance, this might be wrong, but it's
                // better to have the label mysteriously missing than
                // mysteriously present
                this._havePointer = false;
            }
        } else
            this.emit('selected', event.get_time());
    },

    _onSnapBackComplete : function () {
        this.actor.reparent(this._dragOrigParent);
        this.actor.set_scale(this._dragOrigScale, this._dragOrigScale);
        this.actor.set_position(this._dragOrigX, this._dragOrigY);
    },

    _onAnimationComplete : function () {
        this._animationCount--;
        if (this._animationCount == 0) {
            this._updateTitle();
    	    this.actor.raise(this.stackAbove);
        }
    },

    _createTitle : function () {
        let window = this.realWindow;
        
        let box = new Big.Box({ background_color : WINDOWCLONE_BG_COLOR,
                                y_align: Big.BoxAlignment.CENTER,
                                corner_radius: 5,
                                padding: 4,
                                spacing: 4,
                                orientation: Big.BoxOrientation.HORIZONTAL });
        
        let icon = window.meta_window.mini_icon;
        let iconTexture = new Clutter.Texture({ x: this.actor.x,
                                                y: this.actor.y + this.actor.height - 16,
                                                width: 16,
                                                height: 16,
                                                keep_aspect_ratio: true });
        Shell.clutter_texture_set_from_pixbuf(iconTexture, icon);
        box.append(iconTexture, Big.BoxPackFlags.NONE);
        
        let title = new Clutter.Label({ color: WINDOWCLONE_TITLE_COLOR,
                                        font_name: "Sans 12",
                                        text: window.meta_window.title,
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
                          !this._buttonDown &&
                          this._animationCount == 0);

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
        this.actor = new Clutter.Group();
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

        let metaWorkspace = global.screen.get_workspace_by_index(workspaceNum);
        this._desktop.connect('selected',
                              function(clone, time) {
                                  metaWorkspace.activate(time);
                                  Main.hide_overlay();
                              });
        this.actor.add_actor(this._desktop.actor);

        // Create clones for remaining windows that should be
        // visible in the overlay
        this._windows = [this._desktop];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverlayWindow(windows[i])) {
                this._addWindowClone(windows[i]);
            }
        }

        this._removeButton = null;
        this._visible = false;

        this._frame = null;
    },

    // Checks if the workspace is empty (ie, contains only a desktop window)
    isEmpty : function() {
        return this._windows.length == 1;
    },

    // Change Workspace's removability.
    setRemovable : function(removable, buttonSize) {
        let global = Shell.Global.get();

        if (removable) {
            if (this._removeButton)
                return;

            this._removeButton = new Clutter.Texture({ width: buttonSize,
                                                       height: buttonSize,
                                                       reactive: true
                                                     });
            this._removeButton.set_from_file(global.imagedir + "remove-workspace.svg");
            this._removeButton.connect('button-press-event', Lang.bind(this, this._removeSelf));

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

            let tweenProperties = {
                x: xCenter - 0.5 * scale * clone.actor.width,
                y: yCenter - 0.5 * scale * clone.actor.height,
                scale_x: scale,
                scale_y: scale,
                time: Overlay.ANIMATION_TIME,
                opacity: WINDOW_OPACITY,
                transition: "easeOutQuad"
            };

            // workspace_relative assumes that the workspace is zooming in our out
            if (workspaceZooming)
                tweenProperties['workspace_relative'] = this;

            clone.addTween(tweenProperties);
        }
    },

    // Remove a window from the workspace - this is called to fix up the visual
    // display for changes to the window state that have already been made
    removeWindow : function(win) {
        // find the position of the window in our list
        let index = - 1;
        for (let i = 0; i < this._windows.length; i++) {
            if (this._windows[i].realWindow == win) {
                index = i;
                break;
            }
        }

        if (index == -1)
            return;

        let clone = this._windows[index];
        this._windows.splice(index, 1);
        clone.destroy();

        this._positionWindows();
    },

    // Add a window from the workspace - this is called to fix up the visual
    // display for changes to the window state that have already been made.
    // x/y/scale are used to give an initial position for the window (if the
    // window was dropped on the workspace, say) - the window will then be
    // animated to the final location.
    addWindow : function(win, x, y, scale) {
        let clone = this._addWindowClone(win);
        clone.actor.set_position (x, y);
        clone.actor.set_scale (scale, scale);
        
        this._positionWindows();
    },

    // Animate the full-screen to overlay transition.
    zoomToOverlay : function() {
        // Move the workspace into size/position
        this.actor.set_position(this.fullSizeX, this.fullSizeY);
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

        this._visible = true;
    },

    // Animates the return from overlay mode
    zoomFromOverlay : function() {
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
            clone.addTween({ x: clone.origX,
                             y: clone.origY,
                             scale_x: 1.0,
                             scale_y: 1.0,
                             workspace_relative: this,
                             time: Overlay.ANIMATION_TIME,
                             opacity: 255,
                             transition: "easeOutQuad"
                           });
        }

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
    },
    
    destroy : function() {
        this.actor.destroy();
        this.actor = null;
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
        let w = clone.realWindow;
        let windowWorkspace = w.get_workspace();

        if (windowWorkspace != activeWorkspace) {
            let workspace = global.screen.get_workspace_by_index(windowWorkspace);
            workspace.activate_with_focus(w.get_meta_window(), time);
        } else
            w.get_meta_window().activate(time);
        Main.hide_overlay();
    },

    _removeSelf : function(actor, event) {
        let global = Shell.Global.get();
        let screen = global.screen;
        let workspace = screen.get_workspace_by_index(this.workspaceNum);

        screen.remove_workspace(workspace, event.get_time());
    }
};

Signals.addSignalMethods(Workspace.prototype);

function Workspaces() {
    this._init();
}

Workspaces.prototype = {
    _init : function() {
        let global = Shell.Global.get();

        this.actor = new Clutter.Group();

        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height;

        this._width = screenWidth * Overlay.WORKSPACE_GRID_SCALE -
            2 * Overlay.WORKSPACE_GRID_PADDING;
        this._height = screenHeight * Overlay.WORKSPACE_GRID_SCALE;
        this._x = screenWidth - this._width - Overlay.WORKSPACE_GRID_PADDING;
        this._y = Panel.PANEL_HEIGHT + (screenHeight - this._height - Panel.PANEL_HEIGHT) / 2;

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

        // Create (+) and (-) buttons
        let bottomHeight = screenHeight - this._height - this._y;
        this._buttonSize = Math.floor(bottomHeight * 3/5);
        let plusX = this._x + this._width - this._buttonSize;
        let plusY = screenHeight - Math.floor(bottomHeight * 4/5);

        let plus = new Clutter.Texture({ x: plusX,
                                         y: plusY,
                                         width: this._buttonSize,
                                         height: this._buttonSize,
                                         reactive: true
                                       });
        plus.set_from_file(global.imagedir + "add-workspace.svg");
        plus.connect('button-press-event', this._appendNewWorkspace);
        this.actor.add_actor(plus);
        plus.lower_bottom();

        let lastWorkspace = this._workspaces[this._workspaces.length - 1];
        if (lastWorkspace.isEmpty())
            lastWorkspace.setRemovable(true, this._buttonSize);

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
        this._workspaces[oldNumWorkspaces - 1].setRemovable(false);

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
        if (newLastWorkspace.isEmpty())
            newLastWorkspace.setRemovable(true, this._buttonSize);

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
        workspace.connect('window-dragged',
                          Lang.bind(this, this._onWindowDragged));
        this.actor.add_actor(workspace.actor);
    },

    _appendNewWorkspace : function(actor, event) {
        let global = Shell.Global.get();

        global.screen.append_new_workspace(false, event.get_time());
    },

    _onWindowDragged : function(sourceWorkspace, clone, stageX, stageY, time) {
        let global = Shell.Global.get();

        // Positions in stage coordinates
        let [myX, myY] = this.actor.get_transformed_position();
        let [windowX, windowY] = clone.actor.get_transformed_position();

        let targetWorkspace = null;
        let targetX, targetY, targetScale;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            // Drag point relative to the new workspace
            let relX = stageX - myX - workspace.gridX;
            let relY = stageY - myY - workspace.gridY;

            if (relX > 0 && relY > 0 &&
                relX < global.screen_width * workspace.scale &&
                relY < global.screen_height * workspace.scale)
            {
                targetWorkspace = workspace;
                break;
            }
        }

        if (targetWorkspace == null || targetWorkspace == sourceWorkspace)
            return;

        // Window position and scale relative to the new workspace
        targetX = (windowX - myX - targetWorkspace.gridX) / targetWorkspace.scale;
        targetY = (windowY - myY - targetWorkspace.gridY) / targetWorkspace.scale;
        targetScale = clone.actor.scale_x / targetWorkspace.scale;

        let metaWindow = clone.realWindow.get_meta_window();
        metaWindow.change_workspace_by_index(targetWorkspace.workspaceNum,
                                             false, // don't create workspace
                                             time);
        sourceWorkspace.removeWindow(clone.realWindow);
        targetWorkspace.addWindow(clone.realWindow, targetX, targetY, targetScale);
    }
};

// Create a SpecialPropertyModifier to let us move windows in a
// straight line on the screen even though their containing workspace
// is also moving.
Tweener.registerSpecialPropertyModifier("workspace_relative", _workspace_relative_modifier, _workspace_relative_get);

function _workspace_relative_modifier(workspace) {
    let endX, endY;

    if (workspace.actor.x == workspace.fullSizeX) {
        endX = workspace.gridX;
        endY = workspace.gridY;
    } else {
        endX = workspace.fullSizeX;
        endY = workspace.fullSizeY;
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