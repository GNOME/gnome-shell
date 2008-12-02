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
// is relative to the width of the desktop.
const POSITIONS = {
        1: [[0.5, 0.5, 0.8]],
        2: [[0.25, 0.5, 0.4], [0.75, 0.5, 0.4]],
        3: [[0.25, 0.25, 0.33],  [0.75, 0.25, 0.33],  [0.5, 0.75, 0.33]],
        4: [[0.25, 0.25, 0.33],   [0.75, 0.25, 0.33], [0.75, 0.75, 0.33], [0.25, 0.75, 0.33]],
        5: [[0.165, 0.25, 0.28], [0.495, 0.25, 0.28], [0.825, 0.25, 0.28], [0.25, 0.75, 0.4], [0.75, 0.75, 0.4]]
};

function Desktops(x, y, width, height) {
    this._init(x, y, width, height);
}

Desktops.prototype = {
    _init : function(x, y, width, height) {
        this._windowClones = [];
        this._x = x;
        this._y = y;
        this._width = width;
        this._height = height;

        this._group = new Clutter.Group();
    },

    show : function() {
        let global = Shell.Global.get();

        let windows = global.get_windows();
        let desktopWindow = null;

        for (let i = 0; i < windows.length; i++)
            if (windows[i].get_window_type() == Meta.WindowType.DESKTOP)
                desktopWindow = windows[i];

        // If a file manager is displaying desktop icons, there will
        // be a desktop window. This window will have the size of the
        // whole desktop. When such window is not present (e.g. when
        // the preference for showing icons on the desktop is disabled
        // by the user or we are running inside a Xephyr window), we
        // should create a desktop rectangle to serve as the
        // background.
        if (desktopWindow)
            this._createDesktopClone(desktopWindow);
        else
            this._createDesktopRectangle();

        // Count the total number of windows so we know what layout scheme to use
        let numberOfWindows = 0;
        for (let i = 0; i < windows.length; i++) {
            let w = windows[i];
            if (w == desktopWindow || w.is_override_redirect())
                continue;

            numberOfWindows++;
        }

        // Now create actors for all the desktop windows. Do it in
        // reverse order so that the active actor ends up on top
        let windowIndex = 0;
        for (let i = windows.length - 1; i >= 0; i--) {
            let w = windows[i];
            if (w == desktopWindow || w.is_override_redirect())
                continue;
            this._createWindowClone(w, numberOfWindows - windowIndex - 1, numberOfWindows);

            windowIndex++;
        }
    },

    hide : function() {
        for (let i = 0; i < this._windowClones.length; i++) {
            this._windowClones[i].destroy();
        }

        this._windowClones = [];
    },

    _createDesktopClone : function(w) {
        let clone = new Clutter.CloneTexture({ parent_texture: w.get_texture(),
                                               reactive: true,
                                               x: 0,
                                               y: 0 });
        this._addDesktop(clone);
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

        let desktopRectangle = new Clutter.Rectangle({ color: global.stage.color,
                                                       reactive: true,
                                                       x: 0,
                                                       y: 0,
                                                       width: global.screen_width,
                                                       height: global.screen_height });
        this._addDesktop(desktopRectangle);
    },

    _addDesktop : function(desktop) {
        let me = this;

        this._windowClones.push(desktop);
        this._group.add_actor(desktop);

        // Since the right side only moves a little bit (the width of padding
        // we add) it looks less jittery to put the anchor there.
        desktop.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
        Tweener.addTween(desktop,
                         { x: this._x + this._width,
                           y: this._y,
                           scale_x: Overlay.DESKTOP_SCALE,
                           scale_y: Overlay.DESKTOP_SCALE,
                           time: Overlay.ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        desktop.connect("button-press-event",
                        function() {
                            me._deactivate();
                        });
    },

    //windowIndex == 0 => top in stacking order
    _computeWindowPosition : function(windowIndex, numberOfWindows) {
        if (numberOfWindows in POSITIONS)
            return POSITIONS[numberOfWindows][windowIndex];

        // If we don't have a predefined scheme for this window count,
        // overlap the windows along the diagonal of the desktop
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

    _createWindowClone : function(w, windowIndex, numberOfWindows) {
        let me = this;

        // We show the window using "clones" of the texture .. separate
        // actors that mirror the original actors for the window. For
        // animation purposes, it may be better to actually move the
        // original actors about instead.

        let clone = new Clutter.CloneTexture({ parent_texture: w.get_texture(),
                                               reactive: true,
                                               x: w.x,
                                               y: w.y });

        let [xCenter, yCenter, fraction] = this._computeWindowPosition(windowIndex, numberOfWindows);

        let desiredSize = this._width * fraction;

        xCenter = this._x + xCenter * this._width;
        yCenter = this._y + yCenter * this._height;

        let size = clone.width;
        if (clone.height > size)
            size = clone.height;

        // Never scale up
        let scale = desiredSize / size;
        if (scale > 1)
            scale = 1;

        this._group.add_actor(clone);
        this._windowClones.push(clone);

        Tweener.addTween(clone,
                         { x: xCenter - 0.5 * scale * w.width,
                           y: yCenter - 0.5 * scale * w.height,
                           scale_x: scale,
                           scale_y: scale,
                           time: Overlay.ANIMATION_TIME,
                           opacity: WINDOW_OPACITY,
                           transition: "easeOutQuad"
                         });

        clone.connect("button-press-event",
                      function(clone, event) {
                          me._activateWindow(w, event.get_time());
                      });
    },

    _activateWindow : function(w, time) {
        this._deactivate();
        w.get_meta_window().activate(time);
    },

    _deactivate : function() {
        Main.hide_overlay();
    }
};

