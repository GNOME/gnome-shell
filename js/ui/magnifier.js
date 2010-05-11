/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Gtk = imports.gi.Gtk;
const Gdk = imports.gi.Gdk;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Main = imports.ui.main;
const MagnifierDBus = imports.ui.magnifierDBus;

const MouseTrackingMode = {
    NONE: 0,
    CENTERED: 1,
    PUSH: 2,
    PROPORTIONAL: 3
};

const ScreenPosition = {
    NONE: 0,
    FULL_SCREEN: 1,
    TOP_HALF: 2,
    BOTTOM_HALF: 3,
    LEFT_HALF: 4,
    RIGHT_HALF: 5
};

// Default settings
const DEFAULT_X_MAGFACTOR = 2;
const DEFAULT_Y_MAGFACTOR = 2;
const DEFAULT_MOUSE_POLL_FREQUENCY = 50;
const DEFAULT_LENS_MODE = false;
const DEFAULT_SCREEN_POSITION = ScreenPosition.BOTTOM_HALF;
const DEFAULT_MOUSE_TRACKING_MODE = MouseTrackingMode.CENTERED;
const DEFAULT_CLAMP_SCROLLING_AT_EDGES = true;

const DEFAULT_SHOW_CROSSHAIRS = false;
const DEFAULT_CROSSHAIRS_THICKNESS = 8;
const DEFAULT_CROSSHAIRS_OPACITY = 169;     // 66%
const DEFAULT_CROSSHAIRS_LENGTH = 4096;
const DEFAULT_CROSSHAIRS_CLIP = false;
const DEFAULT_CROSSHAIRS_CLIP_SIZE = [100, 100];
const DEFAULT_CROSSHAIRS_COLOR = new Clutter.Color();
DEFAULT_CROSSHAIRS_COLOR.from_string("Red");

// GConf settings
const A11Y_MAG_PREFS_DIR        = "/desktop/gnome/accessibility/magnifier";
const SHOW_KEY                  = A11Y_MAG_PREFS_DIR + "/show_magnifier";
const SCREEN_POSITION_KEY       = A11Y_MAG_PREFS_DIR + "/screen_position";
const MAG_FACTOR_KEY            = A11Y_MAG_PREFS_DIR + "/mag_factor";
const LENS_MODE_KEY             = A11Y_MAG_PREFS_DIR + "/lens_mode";
const CLAMP_MODE_KEY            = A11Y_MAG_PREFS_DIR + "/scroll_at_edges";
const MOUSE_TRACKING_KEY        = A11Y_MAG_PREFS_DIR + "/mouse_tracking";
const SHOW_CROSS_HAIRS_KEY      = A11Y_MAG_PREFS_DIR + "/show_cross_hairs";
const CROSS_HAIRS_THICKNESS_KEY = A11Y_MAG_PREFS_DIR + "/cross_hairs_thickness";
const CROSS_HAIRS_COLOR_KEY     = A11Y_MAG_PREFS_DIR + "/cross_hairs_color";
const CROSS_HAIRS_OPACITY_KEY   = A11Y_MAG_PREFS_DIR + "/cross_hairs_opacity";
const CROSS_HAIRS_LENGTH_KEY    = A11Y_MAG_PREFS_DIR + "/cross_hairs_length";
const CROSS_HAIRS_CLIP_KEY      = A11Y_MAG_PREFS_DIR + "/cross_hairs_clip";

let magDBusService = null;

function Magnifier() {
    this._init();
}

Magnifier.prototype = {
    _init: function() {
        // Magnifier is a manager of ZoomRegions.
        this._zoomRegions = [];

        // Create small clutter tree for the magnified mouse.
        let xfixesCursor = Shell.XFixesCursor.get_default();
        this._mouseSprite = new Clutter.Texture();
        xfixesCursor.update_texture_image(this._mouseSprite);
        this._cursorRoot = new Clutter.Group();
        this._cursorRoot.add_actor(this._mouseSprite);

        // Create the first ZoomRegion and initialize it according to the
        // magnification GConf settings.
        let [objUnder, xMouse, yMouse, mask] =
            Gdk.Screen.get_default().get_root_window().get_pointer();
        let aZoomRegion = new ZoomRegion(this, this._cursorRoot);
        this._zoomRegions.push(aZoomRegion);
        let showAtLaunch = this._gConfInit(aZoomRegion);
        aZoomRegion.scrollContentsTo(xMouse, yMouse);

        xfixesCursor.connect('cursor-change', Lang.bind(this, this._updateMouseSprite));
        this._xfixesCursor = xfixesCursor;

        // Export to dbus.
        magDBusService = new MagnifierDBus.ShellMagnifier();
        this.setActive(showAtLaunch);
    },

    /**
     * showSystemCursor:
     * Show the system mouse pointer.
     */
    showSystemCursor: function() {
        this._xfixesCursor.show();
    },

    /**
     * hideSystemCursor:
     * Hide the system mouse pointer.
     */
    hideSystemCursor: function() {
        this._xfixesCursor.hide();
    },

    /**
     * setActive:
     * Show/hide all the zoom regions.
     * @activate:   Boolean to activate or de-activate the magnifier.
     */
    setActive: function(activate) {
        this._zoomRegions.forEach (function(zoomRegion, index, array) {
            zoomRegion.setActive(activate);
        });

        if (activate)
            this.startTrackingMouse();
        else
            this.stopTrackingMouse();

        // Make sure system mouse pointer is shown when all zoom regions are
        // invisible.
        if (!activate)
            this._xfixesCursor.show();
    },

    /**
     * isActive:
     * @return  Whether the magnifier is active (boolean).
     */
    isActive: function() {
        // Sufficient to check one ZoomRegion since Magnifier's active
        // state applies to all of them.
        if (this._zoomRegions.length == 0)
            return false;
        else
            return this._zoomRegions[0].isActive();
    },

    /**
     * startTrackingMouse:
     * Turn on mouse tracking, if not already doing so.
     */
    startTrackingMouse: function() {
        // initialize previous mouse coord to undefined.
        let prevCoord = { x: NaN, y: NaN };
        if (!this._mouseTrackingId)
            this._mouseTrackingId = Mainloop.timeout_add(
                DEFAULT_MOUSE_POLL_FREQUENCY,
                Lang.bind(this, this.scrollToMousePos, prevCoord)
            );
    },

    /**
     * stopTrackingMouse:
     * Turn off mouse tracking, if not already doing so.
     */
    stopTrackingMouse: function() {
        if (this._mouseTrackingId)
            Mainloop.source_remove(this._mouseTrackingId);

        this._mouseTrackingId = null;
    },

    /**
     * isTrackingMouse:
     * Is the magnifier tracking the mouse currently?
     */
    isTrackingMouse: function() {
        return !!this._mouseTrackingId;
    },

    /**
     * scrollToMousePos:
     * Position all zoom regions' ROI relative to the current location of the
     * system pointer.
     * @prevCoord:  The previous mouse coordinates.  Used to stop scrolling if
     *              the new position is the same as the last one (optional).
     * @return      true.
     */
    scrollToMousePos: function(prevCoord) {
        let [objUnder, xMouse, yMouse, mask] =
            Gdk.Screen.get_default().get_root_window().get_pointer();

        if (!prevCoord || prevCoord.x != xMouse || prevCoord.y != yMouse) {
            let sysMouseOverAny = false;
            this._zoomRegions.forEach(function(zoomRegion, index, array) {
                if (zoomRegion.scrollToMousePos())
                    sysMouseOverAny = true;
            });
            if (sysMouseOverAny)
                this.hideSystemCursor();
            else
                this.showSystemCursor();

            if (prevCoord) {
                prevCoord.x = xMouse;
                prevCoord.y = yMouse;
            }
        }
        return true;
    },

    /**
     * createZoomRegion:
     * Create a ZoomRegion instance with the given properties.
     * @xMagFactor:     The power to set horizontal magnification of the
     *                  ZoomRegion.  A value of 1.0 means no magnification.  A
     *                  value of 2.0 doubles the size.
     * @yMagFactor:     The power to set the vertical magnification of the
     *                  ZoomRegion.
     * @roi             Object in the form { x, y, width, height } that
     *                  defines the region to magnify.  Given in unmagnified
     *                  coordinates.
     * @viewPort        Object in the form { x, y, width, height } that defines
     *                  the position of the ZoomRegion on screen.
     * @return          The newly created ZoomRegion.
     */
    createZoomRegion: function(xMagFactor, yMagFactor, roi, viewPort) {
        let zoomRegion = new ZoomRegion(this, this._cursorRoot);
        zoomRegion.setMagFactor(xMagFactor, yMagFactor);
        zoomRegion.setViewPort(viewPort);
        zoomRegion.setROI(roi);
        zoomRegion.addCrosshairs(this._crossHairs);
        return zoomRegion;
    },

    /**
     * addZoomRegion:
     * Append the given ZoomRegion to the list of currently defined ZoomRegions
     * for this Magnifier instance.
     * @zoomRegion:     The zoomRegion to add.
     */
    addZoomRegion: function(zoomRegion) {
        if(zoomRegion) {
            this._zoomRegions.push(zoomRegion);
            if (!this.isTrackingMouse())
                this.startTrackingMouse();
        }
    },

    /**
     * getZoomRegions:
     * Return a list of ZoomRegion's for this Magnifier.
     * @return:     The Magnifier's zoom region list (array).
     */
    getZoomRegions: function() {
        return this._zoomRegions;
    },

    /**
     * clearAllZoomRegions:
     * Remove all the zoom regions from this Magnfier's ZoomRegion list.
     */
    clearAllZoomRegions: function() {
        // First ZoomRegion is special since its magnified mouse and crosshairs
        // are the original -- all the others are Clutter.Clone's.  Deal with
        // all but first zoom region.
        for (let i = 1; i < this._zoomRegions.length; i++) {
            this._zoomRegions[i].setActive(false);
            this._zoomRegions[i].removeFromStage();
        }
        this._zoomRegions[0].setActive(false);

        // Detach the (original) magnified mouse and cross hair for later reuse
        // before removing ZoomRegion from the stage.
        this._cursorRoot.get_parent().remove_actor(this._cursorRoot);
        if (this._crossHairs)
            this._crossHairs.removeFromParent();

        this._zoomRegions[0].removeFromStage();
        this._zoomRegions.length = 0;
        this.stopTrackingMouse();
        this.showSystemCursor();
    },

    /**
     * addCrosshairs:
     * Add and show a cross hair centered on the magnified mouse.
     * @thickness:  The thickness of the vertical and horizontal lines of the
     *              crosshair.
     * @color:      The color of the crosshairs
     * @opacity:    The opacity.
     * @length:     The length of each hair.
     * @clip:       Whether the crosshairs intersection is clipped by the
     *              magnified mouse image.
     */
    addCrosshairs: function(thickness, color, opacity, length, clip) {
        if (!this._crossHairs)
            this._crossHairs = new Crosshairs();

        this.setCrosshairsThickness(thickness);
        this.setCrosshairsColor(color);
        this.setCrosshairsOpacity(opacity);
        this.setCrosshairsLength(length);
        this.setCrosshairsClip(clip);

        let theCrossHairs = this._crossHairs;
        this._zoomRegions.forEach (function(zoomRegion, index, array) {
            zoomRegion.addCrosshairs(theCrossHairs);
        });
    },

    /**
     * setCrosshairsVisible:
     * Show or hide the cross hair.
     * @visible    Flag that indicates show (true) or hide (false).
     */
    setCrosshairsVisible: function(visible) {
        if (visible) {
            if (!this._crossHairs)
                this.addCrosshairs(DEFAULT_CROSSHAIRS_THICKNESS, DEFAULT_CROSSHAIRS_COLOR, DEFAULT_CROSSHAIRS_OPACITY, DEFAULT_CROSSHAIRS_CLIP);
            this._crossHairs.show();
        }
        else {
            if (this._crossHairs)
                this._crossHairs.hide();
        }
    },

    /**
     * setCrosshairsColor:
     * Set the color of the crosshairs for all ZoomRegions.
     * @color:  The color as a string, e.g. "#ff0000ff" or "red".
     */
    setCrosshairsColor: function(color) {
        if (this._crossHairs) {
            let clutterColor = new Clutter.Color();
            clutterColor.from_string(color);
            this._crossHairs.setColor(clutterColor);
        }
    },

    /**
     * getCrosshairsColor:
     * Get the color of the crosshairs.
     * @return: The color as a string, e.g. "#0000ffff" for blue.
     */
    getCrosshairsColor: function() {
        if (this._crossHairs) {
            let clutterColor = this._crossHairs.getColor();
            return clutterColor.to_string();
        }
        else
            return "#00000000";
    },

    /**
     * setCrosshairsThickness:
     * Set the crosshairs thickness for all ZoomRegions.
     * @thickness:  The width of the vertical and horizontal lines of the
     *              crosshairs.
     */
    setCrosshairsThickness: function(thickness) {
        if (this._crossHairs)
            this._crossHairs.setThickness(thickness);
    },

    /**
     * getCrosshairsThickness:
     * Get the crosshairs thickness.
     * @return: The width of the vertical and horizontal lines of the
     *          crosshairs.
     */
    getCrosshairsThickness: function() {
        if (this._crossHairs)
            return this._crossHairs.getThickness();
        else
            return 0;
    },

    /**
     * setCrosshairsOpacity:
     * @opacity:    Value between 0 (transparent) and 255 (fully opaque).
     */
    setCrosshairsOpacity: function(opacity) {
        if (this._crossHairs)
            this._crossHairs.setOpacity(opacity);
    },

    /**
     * getCrosshairsOpacity:
     * @return:     Value between 0 (transparent) and 255 (fully opaque).
     */
    getCrosshairsOpacity: function() {
        if (this._crossHairs)
            return this._crossHairs.getOpacity();
        else
            return 0;
    },

    /**
     * setCrosshairsLength:
     * Set the crosshairs length for all ZoomRegions.
     * @length: The length of the vertical and horizontal lines making up the
     *          crosshairs.
     */
    setCrosshairsLength: function(length) {
        if (this._crossHairs)
            this._crossHairs.setLength(length);
    },

    /**
     * getCrosshairsLength:
     * Get the crosshairs length.
     * @return: The length of the vertical and horizontal lines making up the
     *          crosshairs.
     */
    getCrosshairsLength: function() {
        if (this._crossHairs)
            return this._crossHairs.getLength();
        else
            return 0;
    },

    /**
     * setCrosshairsClip:
     * Set whether the crosshairs are clipped at their intersection.
     * @clip:   Flag to indicate whether to clip the crosshairs.
     */
    setCrosshairsClip: function(clip) {
        if (clip) {
            if (this._crossHairs)
                this._crossHairs.setClip(DEFAULT_CROSSHAIRS_CLIP_SIZE);
        }
        else {
            // Setting no clipping on crosshairs means a zero sized clip
            // rectangle.
            if (this._crossHairs)
                this._crossHairs.setClip([0, 0]);
        }
    },

    /**
     * getCrosshairsClip:
     * Get whether the crosshairs are clipped by the mouse image.
     * @return:   Whether the crosshairs are clipped.
     */
     getCrosshairsClip: function() {
        if (this._crossHairs) {
            let [clipWidth, clipHeight] = this._crossHairs.getClip();
            return (clipWidth > 0 && clipHeight > 0);
        }
        else
            return false;
     },

    //// Private methods ////

    _updateMouseSprite: function() {
        this._xfixesCursor.update_texture_image(this._mouseSprite);
        let xHot = this._xfixesCursor.get_hot_x();
        let yHot = this._xfixesCursor.get_hot_y();
        this._mouseSprite.set_anchor_point(xHot, yHot);
    },

    _gConfInit: function(zoomRegion) {
        let gConf = Shell.GConf.get_default();
        if (zoomRegion) {
            // Mag factor is accurate to two decimal places.
            let aPref = parseFloat(gConf.get_float(MAG_FACTOR_KEY).toFixed(2));
            if (aPref != 0.0)
                zoomRegion.setMagFactor(aPref, aPref);

            aPref = gConf.get_int(SCREEN_POSITION_KEY);
            if (aPref)
                zoomRegion.setScreenPosition(aPref);

            zoomRegion.setLensMode(gConf.get_boolean(LENS_MODE_KEY));
            zoomRegion.setClampScrollingAtEdges(!gConf.get_boolean(CLAMP_MODE_KEY));

            aPref = gConf.get_int(MOUSE_TRACKING_KEY);
            if (aPref)
                zoomRegion.setMouseTrackingMode(aPref);
        }
        let showCrosshairs = gConf.get_boolean(SHOW_CROSS_HAIRS_KEY);
        let thickness = gConf.get_int(CROSS_HAIRS_THICKNESS_KEY);
        let color = gConf.get_string(CROSS_HAIRS_COLOR_KEY);
        let opacity = gConf.get_int(CROSS_HAIRS_OPACITY_KEY);
        let length = gConf.get_int(CROSS_HAIRS_LENGTH_KEY);
        let clip = gConf.get_boolean(CROSS_HAIRS_CLIP_KEY);
        this.addCrosshairs(thickness, color, opacity, length, clip);
        this.setCrosshairsVisible(showCrosshairs);

        gConf.watch_directory(A11Y_MAG_PREFS_DIR);
        gConf.connect('changed::' + SHOW_KEY, Lang.bind(this, this._updateShowHide));
        gConf.connect('changed::' + SCREEN_POSITION_KEY, Lang.bind(this, this._updateScreenPosition));
        gConf.connect('changed::' + MAG_FACTOR_KEY, Lang.bind(this, this._updateMagFactor));
        gConf.connect('changed::' + LENS_MODE_KEY, Lang.bind(this, this._updateLensMode));
        gConf.connect('changed::' + CLAMP_MODE_KEY, Lang.bind(this, this._updateClampMode));
        gConf.connect('changed::' + MOUSE_TRACKING_KEY, Lang.bind(this, this._updateMouseTrackingMode));
        gConf.connect('changed::' + SHOW_CROSS_HAIRS_KEY, Lang.bind(this, this._updateShowCrosshairs));
        gConf.connect('changed::' + CROSS_HAIRS_THICKNESS_KEY, Lang.bind(this, this._updateCrosshairsThickness));
        gConf.connect('changed::' + CROSS_HAIRS_COLOR_KEY, Lang.bind(this, this._updateCrosshairsColor));
        gConf.connect('changed::' + CROSS_HAIRS_OPACITY_KEY, Lang.bind(this, this._updateCrosshairsOpacity));
        gConf.connect('changed::' + CROSS_HAIRS_LENGTH_KEY, Lang.bind(this, this._updateCrosshairsLength));
        gConf.connect('changed::' + CROSS_HAIRS_CLIP_KEY, Lang.bind(this, this._updateCrosshairsClip));

        return gConf.get_boolean(SHOW_KEY);
   },

    _updateShowHide: function() {
        let gConf = Shell.GConf.get_default();
        this.setActive(gConf.get_boolean(SHOW_KEY));
    },

    _updateScreenPosition: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let gConf = Shell.GConf.get_default();
            let position = gConf.get_int(SCREEN_POSITION_KEY);
            this._zoomRegions[0].setScreenPosition(position);
            if (position != ScreenPosition.FULL_SCREEN)
                this._updateLensMode();
        }
    },

    _updateMagFactor: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let gConf = Shell.GConf.get_default();
            // Mag factor is accurate to two decimal places.
            let magFactor = parseFloat(gConf.get_float(MAG_FACTOR_KEY).toFixed(2));
            this._zoomRegions[0].setMagFactor(magFactor, magFactor);
        }
    },

    _updateLensMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let gConf = Shell.GConf.get_default();
            this._zoomRegions[0].setLensMode(gConf.get_boolean(LENS_MODE_KEY));
        }
    },

    _updateClampMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let gConf = Shell.GConf.get_default();
            this._zoomRegions[0].setClampScrollingAtEdges(
                !gConf.get_boolean(CLAMP_MODE_KEY)
            );
        }
    },

    _updateMouseTrackingMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let gConf = Shell.GConf.get_default();
            this._zoomRegions[0].setMouseTrackingMode(
                gConf.get_int(MOUSE_TRACKING_KEY)
            );
        }
    },

    _updateShowCrosshairs: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsVisible(gConf.get_boolean(SHOW_CROSS_HAIRS_KEY));
    },

    _updateCrosshairsThickness: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsThickness(gConf.get_int(CROSS_HAIRS_THICKNESS_KEY));
    },

    _updateCrosshairsColor: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsColor(gConf.get_string(CROSS_HAIRS_COLOR_KEY));
    },

    _updateCrosshairsOpacity: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsOpacity(gConf.get_int(CROSS_HAIRS_OPACITY_KEY));
    },

    _updateCrosshairsLength: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsLength(gConf.get_int(CROSS_HAIRS_LENGTH_KEY));
    },

    _updateCrosshairsClip: function() {
        let gConf = Shell.GConf.get_default();
        this.setCrosshairsClip(gConf.get_boolean(CROSS_HAIRS_CLIP_KEY));
    }
}

function ZoomRegion(magnifier, mouseRoot) {
    this._init(magnifier, mouseRoot);
}

ZoomRegion.prototype = {
    _init: function(magnifier, mouseRoot) {
        this._magnifier = magnifier;

        // The root actor for the zoom region
        this._magView = new St.Bin({ style_class: 'magnifier-zoom-region', x_fill: true, y_fill: true });
        global.stage.add_actor(this._magView);
        this._magView.hide();

        // Append a Clutter.Group to clip the contents of the magnified view.
        this._mainGroup = new Clutter.Group({ clip_to_allocation: true });
        this._magView.set_child(this._mainGroup);

        // Add a background for when the magnified uiGroup is scrolled
        // out of view (don't want to see desktop showing through).
        let background = new Clutter.Rectangle({ color: Main.DEFAULT_BACKGROUND_COLOR });
        this._mainGroup.add_actor(background);

        // Clone the group that contains all of UI on the screen.  This is the
        // chrome, the windows, etc.
        this._uiGroupClone = new Clutter.Clone({ source: Main.uiGroup });
        this._mainGroup.add_actor(this._uiGroupClone);
        Main.uiGroup.set_size(global.screen_width, global.screen_height);
        background.set_size(global.screen_width, global.screen_height);
        this._uiGroupClone.set_size(global.screen_width, global.screen_height);

        // Add either the given mouseRoot to the ZoomRegion, or a clone of
        // it.
        if (mouseRoot.get_parent() != null)
            this._mouseRoot = new Clutter.Clone({ source: mouseRoot });
        else
            this._mouseRoot = mouseRoot;
        this._mainGroup.add_actor(this._mouseRoot);
        this._crossHairs = null;

        this.setMagFactor(DEFAULT_X_MAGFACTOR, DEFAULT_Y_MAGFACTOR);
        this.setScreenPosition(DEFAULT_SCREEN_POSITION);
        this.setLensMode(DEFAULT_LENS_MODE);
        this.setClampScrollingAtEdges(DEFAULT_CLAMP_SCROLLING_AT_EDGES);
        this.setMouseTrackingMode(DEFAULT_MOUSE_TRACKING_MODE);
    },

    /**
     * setActive:
     * @activate:   Boolean to show/hide the ZoomRegion.
     */
    setActive: function(activate) {
        if (activate) {
            this._magView.show();
            if (this.isMouseOverRegion())
                this._magnifier.hideSystemCursor();
            this._updateMousePosition(false /* mouse didn't move */);
        }
        else
            this._magView.hide();
    },

    /**
     * isActive:
     * @return  Whether this ZoomRegion is active (boolean).
     */
    isActive: function() {
        return this._magView.visible;
    },

    /**
     * removeFromStage:
     * Remove the magnified view from the stage.
     */
    removeFromStage: function() {
        global.stage.remove_actor(this._magView);
        this._mouseRoot = null;
        this._uiGroupClone = null;
        this._magView = null;
    },

    /**
     * setMagFactor:
     * @xMagFactor:     The power to set the horizontal magnification factor to
     *                  of the magnified view.  A value of 1.0 means no
     *                  magnification.  A value of 2.0 doubles the size.
     * @yMagFactor:     The power to set the vertical magnification factor to
     *                  of the magnified view.
     */
    setMagFactor: function(xMagFactor, yMagFactor) {
        if (xMagFactor > 0 && yMagFactor > 0) {
            // Changing the mag factor moves the pixels along the axes of
            // magnification.  Set the view back to the point that was at the centre
            // of the region of interest.
            let [x, y, width, height] = this.getROI();
            let xCentre = x + width / 2;
            let yCentre = y + height / 2;
            this._uiGroupClone.set_scale(xMagFactor, yMagFactor);
            this._mouseRoot.set_scale(xMagFactor, yMagFactor);
            this._calcRightBottomStops();
            this._scrollToPosition(xCentre, yCentre);
            this._updateMousePosition(false /* mouse didn't move */);
        }
    },

    /**
     * getMagFactor:
     * @return  an array, [xMagFactor, yMagFactor], containing the horizontal
     *          and vertical magnification powers.  A value of 1.0 means no
     *          magnification.  A value of 2.0 means the contents are doubled
     *          in size, and so on.
     */
    getMagFactor: function() {
        return this._uiGroupClone.get_scale();
    },

    /**
     * setMouseTrackingMode
     * @mode:     One of the enum MouseTrackingMode values.
     */
    setMouseTrackingMode: function(mode) {
        if (mode >= MouseTrackingMode.NONE && mode <= MouseTrackingMode.PROPORTIONAL)
            this._mouseTrackingMode = mode;
    },

    /**
     * getMouseTrackingMode
     * @return:     One of the enum MouseTrackingMode values.
     */
    getMouseTrackingMode: function() {
        return this._mouseTrackingMode;
    },

    /**
     * setViewPort
     * Sets the position and size of the ZoomRegion on screen.
     * @viewPort:   Object defining the position and size of the view port.  It
     *              has the form { x, y, width, height }.  The values are in
     *              stage coordinate space.
     */
    setViewPort: function(viewPort) {
        let [xRoi, yRoi, wRoi, hRoi] = this.getROI();

        // Remove border if the view port is the entire screen.  Otherwise,
        // ensure that the border is there.
        if (viewPort.x == 0 && viewPort.y == 0 && viewPort.width == global.screen_width && viewPort.height == global.screen_height)
            this._magView.add_style_class_name('full-screen');
        else
            this._magView.remove_style_class_name('full-screen');

        this.setSize(viewPort.width, viewPort.height);
        this.setPosition(viewPort.x, viewPort.y);
        if (this._crossHairs)
            this._crossHairs.reCenter();

        this.scrollContentsTo(xRoi + wRoi / 2, yRoi + hRoi / 2);
        if (this.isMouseOverRegion())
            this._magnifier.hideSystemCursor();

        this._screenPosition = ScreenPosition.NONE;
    },

    /**
     * setROI
     * Sets the "region of interest" that the ZoomRegion is magnifying.
     * @roi:    Object that defines the region of the screen to magnify.  It
     *          has the form { x, y, width, height }.  The values are in
     *          screen (unmagnified) coordinate space.
     */
    setROI: function(roi) {
        let xRoiCenter = roi.x + roi.width  / 2;
        let yRoiCenter = roi.y + roi.height / 2;
        this.scrollContentsTo(xRoiCenter, yRoiCenter);
    },

    /**
     * setSize:
     * @width:    The width to set the magnified view to.
     * @height:   The height to set the magnified view to.
     */
    setSize: function(width, height) {
        this._magView.set_size(width, height);
        this._calcRightBottomStops();
    },

    /**
     * getSize:
     * @return  an array, [width, height], that specifies the size of the
     *          magnified view.
     */
    getSize: function() {
        return this._magView.get_size();
    },

    /**
     * setPosition:
     * Position the magnified view at the given coordinates.
     * @x:    The x-coord of the new position.
     * @y:    The y-coord of the new position.
     */
    setPosition: function(x, y) {
        let [width, height] = this._magView.get_size();
        if (this._clampScrollingAtEdges) {
            // Restrict positioning so view doesn't go beyond any edge of the
            // screen.
            if (x < 0)
                x = 0;
            if (x + width > global.screen_width)
                x = global.screen_width - width;
            if (y < 0)
                y = 0;
            if (y + height > global.screen_height)
                y = global.screen_height - height;
        }
        this._magView.set_position(x, y);
    },

    /**
     * getPosition:
     * @return  an array, [x, y], that gives the position of the
     *          magnified view on screen.
     */
    getPosition: function() {
        return this._magView.get_position();
    },

    /**
     * getCenter:
     * @return  an array, [x, y], that is half the width and height of the
     *          magnified view (the center of the magnified view).
     */
    getCenter: function() {
        let [width, height] = this._magView.get_size();
        return [width / 2, height / 2];
    },

    /**
     * isFullScreenMode:
     * Does the magnified view occupy the whole screen?
     */
    isFullScreenMode: function() {
        let [x, y] = this._magView.get_position();
        if (x != 0 || y != 0)
            return false;
        [width, height] = this._magView.get_size();
        if (width != global.screen_width || height != global.screen_height)
            return false;
        return true;
    },

    /**
     * getROI:
     * Retrieves the "region of interest" -- the rectangular bounds of that part
     * of the desktop that the magnified view is showing (x, y, width, height).
     * The bounds are given in non-magnified coordinates.
     * @return  an array, [x, y, width, height], representing the bounding
     *          rectangle of what is shown in the magnified view.
     */
    getROI: function() {
        let [xMagnified, yMagnified] = this._uiGroupClone.get_position();
        let [xMagFactor, yMagFactor] = this.getMagFactor();
        let [width, height] = this.getSize();
        let x = (0 - xMagnified) / xMagFactor;
        let y = (0 - yMagnified) / yMagFactor;
        return [x, y, width / xMagFactor, height / yMagFactor];
    },

    /**
     * setLensMode:
     * Turn lens mode on/off.  In full screen mode, lens mode is alway off since
     * a lens the size of the screen is pointless.
     * @lensMode:   A boolean to set the sense of lens mode.
     */
    setLensMode: function(lensMode) {
        let fullScreen = this.isFullScreenMode();
        this._lensMode = (lensMode && !fullScreen);
        if (!this._lensMode && !fullScreen)
            this.setScreenPosition (this._screenPosition);
    },

    /**
     * isLensMode:
     * Is lens mode on or off?
     * @return  The lens mode state as a boolean.
     */
    isLensMode: function() {
        return this._lensMode;
    },

    /**
     * setClampScrollingAtEdges:
     * Stop vs. allow scrolling of the magnified contents when it scroll beyond
     * the edges of the screen.
     * @clamp:   Boolean to turn on/off clamping.
     */
    setClampScrollingAtEdges: function(clamp) {
        this._clampScrollingAtEdges = clamp;
    },

    /**
     * setTopHalf:
     * Magnifier view occupies the top half of the screen.
     */
    setTopHalf: function() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height/2;
        this.setViewPort(viewPort);
        this._screenPosition = ScreenPosition.TOP_HALF;
    },

    /**
     * setBottomHalf:
     * Magnifier view occupies the bottom half of the screen.
     */
    setBottomHalf: function() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = global.screen_height/2;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height/2;
        this.setViewPort(viewPort);
        this._screenPosition = ScreenPosition.BOTTOM_HALF;
    },

    /**
     * setLeftHalf:
     * Magnifier view occupies the left half of the screen.
     */
    setLeftHalf: function() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width/2;
        viewPort.height = global.screen_height;
        this.setViewPort(viewPort);
        this._screenPosition = ScreenPosition.LEFT_HALF;
    },

    /**
     * setRightHalf:
     * Magnifier view occupies the right half of the screen.
     */
    setRightHalf: function() {
        let viewPort = {};
        viewPort.x = global.screen_width/2;
        viewPort.y = 0;
        viewPort.width = global.screen_width/2;
        viewPort.height = global.screen_height;
        this.setViewPort(viewPort);
        this._screenPosition = ScreenPosition.RIGHT_HALF;
    },

    /**
     * getScreenPosition:
     * Tell the outside world what the current mode is -- magnifiying the
     * top half, bottom half, etc.
     * @return:  the current mode.
     */
    getScreenPosition: function() {
        return this._screenPosition;
    },

    /**
     * scrollToMousePos:
     * Set the region of interest based on the position of the system pointer.
     * @return:     Whether the system mouse pointer is over the magnified view.
     */
    scrollToMousePos: function() {
        let [objUnder, xMouse, yMouse, mask] =
            Gdk.Screen.get_default().get_root_window().get_pointer();

        if (this._mouseTrackingMode == MouseTrackingMode.PROPORTIONAL) {
            this._setROIProportional(xMouse, yMouse);
        }
        else if (this._mouseTrackingMode == MouseTrackingMode.PUSH) {
            this._setROIPush(xMouse, yMouse);
        }
        else if (this._mouseTrackingMode == MouseTrackingMode.CENTERED) {
            this._setROICentered(xMouse, yMouse);
        }
        this._updateMousePosition(true);

        // Determine whether the system mouse pointer is over this zoom region.
        return this.isMouseOverRegion(xMouse, yMouse);
    },

    /**
     * setFullScreenMode:
     * Set the ZoomRegion to full-screen mode.
     * Note:  disallows lens mode.
     */
    setFullScreenMode: function() {
        if (!this.isFullScreenMode()) {
            let viewPort = {};
            viewPort.x = 0;
            viewPort.y = 0;
            viewPort.width = global.screen_width;
            viewPort.height = global.screen_height;
            this.setViewPort(viewPort);
            this.setLensMode(false);
            if (this.isActive())
                this._magnifier.hideSystemCursor();

            this._screenPosition = ScreenPosition.FULL_SCREEN;
        }
    },

    /**
     * setScreenPosition:
     * Positions the zoom region to one of the enumerated positions on the
     * screen.
     * @position:   one of Magnifier.FULL_SCREEN, Magnifier.TOP_HALF,
     *              Magnifier.BOTTOM_HALF,Magnifier.LEFT_HALF, or
     *              Magnifier.RIGHT_HALF.
     */
    setScreenPosition: function(inPosition) {
        switch (inPosition) {
            case ScreenPosition.FULL_SCREEN:
                this.setFullScreenMode();
                break;
            case ScreenPosition.TOP_HALF:
                this.setTopHalf();
                break;
            case ScreenPosition.BOTTOM_HALF:
                this.setBottomHalf();
                break;
            case ScreenPosition.LEFT_HALF:
                this.setLeftHalf();
                break;
            case ScreenPosition.RIGHT_HALF:
                this.setRightHalf();
                break;
        }
    },

    /**
     * scrollContentsTo:
     * Shift the contents of the magnified view such it is centered on the given
     * coordinate.  Also, update the position of the magnified mouse image after
     * the shift.
     * @x:      The x-coord of the point to center on.
     * @y:      The y-coord of the point to center on.
     */
    scrollContentsTo: function(x, y) {
        this._scrollToPosition(x, y);
        this._updateMousePosition(false /* mouse didn't move */);
    },

    /**
     * isMouseOverRegion:
     * Return whether the system mouse sprite is over this ZoomRegion.  If the
     * mouse's position is not given, then it is fetched.
     * @xMouse:     The system mouse's x-coord.  Optional.
     * @yMouse:     The system mouse's y-coord.  Optional.
     * @return:     Boolean:  true if the mouse is over the zoom region; false
     *              otherwise.
     */
    isMouseOverRegion: function(xMouse, yMouse) {
        let mouseIsOver = false;
        if (this.isActive()) {
            if (!xMouse || !yMouse) {
                let [objUnder, x, y, mask] =
                    Gdk.Screen.get_default().get_root_window().get_pointer();
                xMouse = x;
                yMouse = y;
            }
            let [x, y] = this.getPosition();
            let [width, height] = this.getSize();
            mouseIsOver = (
                xMouse >= x && xMouse < (x + width) &&
                yMouse >= y && yMouse < (y + height)
            );
        }
        return mouseIsOver;
    },

    /**
     * addCrosshairs:
     * Add crosshairs centered on the magnified mouse.
     * @crossHairs  Clutter.Group that contains the actors for the crosshairs.
     */
    addCrosshairs: function(crossHairs) {
        // If the crossHairs is not already within a larger container, add it
        // to this zoom region.  Otherwise, add a clone.
        if (crossHairs) {
            this._crosshairsActor = crossHairs.addToZoomRegion(this, this._mouseRoot);
            this._crossHairs = crossHairs;
        }
    },

    //// Private methods ////

    _scrollToPosition: function(x, y) {
        // Given the point (x, y) in non-magnified coordinates, scroll the
        // magnified contenst such that the point is at the centre of the
        // magnified view.
        let [xMagFactor, yMagFactor] = this.getMagFactor();
        let xMagnified = x * xMagFactor;
        let yMagnified = y * yMagFactor;

        let [xCenterMagView, yCenterMagView] = this.getCenter();
        let newX = xCenterMagView - xMagnified;
        let newY = yCenterMagView - yMagnified;

        if (this._clampScrollingAtEdges) {
            if (newX > 0)
                newX = 0;
            else if (newX < this._rightStop)
                newX = this._rightStop;
            if (newY > 0)
                newY = 0;
            else if (newY < this._bottomStop)
                newY = this._bottomStop;
            this._uiGroupClone.set_position(newX, newY);
        }
        else
            this._uiGroupClone.set_position(newX, newY);

        // If in lens mode, move the magnified view such that it is centered
        // over the actual mouse. However, in full screen mode, the "lens" is
        // the size of the screen -- pointless to move such a large lens around.
        if (this._lensMode && !this.isFullScreenMode())
            this.setPosition(x - xCenterMagView, y - yCenterMagView);
    },

    _calcRightBottomStops: function() {
        // Calculate the location of the top-left corner of _uiGroupClone
        // when its right and bottom edges are coincident with the right and
        // bottom edges of the _magView.
        let [contentWidth, contentHeight] = this._uiGroupClone.get_size();
        let [viewWidth, viewHeight] = this.getSize();
        let [xMagFactor, yMagFactor] = this.getMagFactor();
        let rightStop = viewWidth - (contentWidth * xMagFactor);
        let bottomStop = viewHeight - (contentHeight * yMagFactor);
        this._rightStop = parseInt(rightStop.toFixed(1));
        this._bottomStop = parseInt(bottomStop.toFixed(1));
    },

    _setROIPush: function(xMouse, yMouse) {
        let [xRoi, yRoi, widthRoi, heightRoi] = this.getROI();
        let [cursorWidth, cursorHeight] = this._mouseRoot.get_size();
        let xPos = xRoi + widthRoi / 2;
        let yPos = yRoi + heightRoi / 2;
        let xRoiRight = xRoi + widthRoi - cursorWidth;
        let yRoiBottom = yRoi + heightRoi - cursorHeight;

        if (xMouse < xRoi)
            xPos -= (xRoi - xMouse);
        else if (xMouse > xRoiRight)
            xPos += (xMouse - xRoiRight);

        if (yMouse < yRoi)
            yPos -= (yRoi - yMouse);
        else if (yMouse > yRoiBottom)
            yPos += (yMouse - yRoiBottom);

        this._scrollToPosition(xPos, yPos);
    },

    _setROIProportional: function(xMouse, yMouse) {
        let [xRoi, yRoi, widthRoi, heightRoi] = this.getROI();
        let halfScreenWidth = global.screen_width / 2;
        let halfScreenHeight = global.screen_height / 2;
        let xProportion = (halfScreenWidth - xMouse) / halfScreenWidth;
        let yProportion = (halfScreenHeight - yMouse) / halfScreenHeight;
        let xPos = xMouse + xProportion * widthRoi / 2;
        let yPos = yMouse + yProportion * heightRoi / 2;

        this._scrollToPosition(xPos, yPos);
    },

    _setROICentered: function(xMouse, yMouse) {
        this._scrollToPosition(xMouse, yMouse);
    },

    _updateMousePosition: function(mouseMoved) {
        let [x, y] = this._uiGroupClone.get_position();
        x = parseInt(x.toFixed(1));
        y = parseInt(y.toFixed(1));
        let [xCenterMagView, yCenterMagView] = this.getCenter();
        let [objUnder, xMouse, yMouse, mask] =
            Gdk.Screen.get_default().get_root_window().get_pointer();
        let [xMagFactor, yMagFactor] = this.getMagFactor();

        let xMagMouse = xMouse * xMagFactor + x;
        let yMagMouse = yMouse * yMagFactor + y;
        if (mouseMoved) {
            if (x == 0)
                xMagMouse = xMouse * xMagFactor;
            else if (x == this._rightStop)
                xMagMouse = (xMouse * xMagFactor) + this._rightStop;
            else if (this._mouseTrackingMode == MouseTrackingMode.CENTERED)
                xMagMouse = xCenterMagView;

            if (y == 0)
                yMagMouse = yMouse * yMagFactor;
            else if (y == this._bottomStop)
                yMagMouse = (yMouse * yMagFactor) + this._bottomStop;
            else if (this._mouseTrackingMode == MouseTrackingMode.CENTERED)
                yMagMouse = yCenterMagView;
        }
        this._mouseRoot.set_position(xMagMouse, yMagMouse);
        this._updateCrosshairsPosition(xMagMouse, yMagMouse);
    },

    _updateCrosshairsPosition: function(x, y) {
        if (this._crosshairsActor) {
            let [groupWidth, groupHeight] = this._crosshairsActor.get_size();
            this._crosshairsActor.set_position(x - groupWidth / 2, y - groupHeight / 2);
        }
    }
}

function Crosshairs() {
    this._init();
}

Crosshairs.prototype = {
    _init: function() {

        // Set the group containing the crosshairs to three times the desktop
        // size in case the crosshairs need to appear to be infinite in
        // length (i.e., extend beyond the edges of the view they appear in).
        let groupWidth = global.screen_width * 3;
        let groupHeight = global.screen_height * 3;
        this._actor = new Clutter.Group({
            clip_to_allocation: false,
            width: groupWidth,
            height: groupHeight
        });
        this._horizLeftHair = new Clutter.Rectangle({
            color: DEFAULT_CROSSHAIRS_COLOR,
            width: groupWidth / 2,
            height: DEFAULT_CROSSHAIRS_THICKNESS,
            opacity:  DEFAULT_CROSSHAIRS_OPACITY
        });
        this._horizRightHair = new Clutter.Rectangle({
            color: DEFAULT_CROSSHAIRS_COLOR,
            width: groupWidth / 2,
            height: DEFAULT_CROSSHAIRS_THICKNESS,
            opacity:  DEFAULT_CROSSHAIRS_OPACITY
        });
        this._vertTopHair = new Clutter.Rectangle({
            color: DEFAULT_CROSSHAIRS_COLOR,
            width: DEFAULT_CROSSHAIRS_THICKNESS,
            height: groupHeight / 2,
            opacity:  DEFAULT_CROSSHAIRS_OPACITY
        });
        this._vertBottomHair = new Clutter.Rectangle({
            color: DEFAULT_CROSSHAIRS_COLOR,
            width: DEFAULT_CROSSHAIRS_THICKNESS,
            height: groupHeight / 2,
            opacity:  DEFAULT_CROSSHAIRS_OPACITY
        });
        this._actor.add_actor(this._horizLeftHair);
        this._actor.add_actor(this._horizRightHair);
        this._actor.add_actor(this._vertTopHair);
        this._actor.add_actor(this._vertBottomHair);
        this._clipSize = [0, 0];
        this._clones = [];
        this.reCenter();
    },

   /**
    * addToZoomRegion
    * Either add the crosshairs actor to the given ZoomRegion, or, if it is
    * already part of some other ZoomRegion, create a clone of the crosshairs
    * actor, and add the clone instead.  Returns either the original or the
    * clone.
    * @zoomRegion:      The container to add the crosshairs group to.
    * @magnifiedMouse:  The mouse actor for the zoom region -- used to
    *                   position the crosshairs and properly layer them below
    *                   the mouse.
    * @return           The crosshairs actor, or its clone.
    */
    addToZoomRegion: function(zoomRegion, magnifiedMouse) {
        let crosshairsActor = null;
        if (zoomRegion && magnifiedMouse) {
            let container = magnifiedMouse.get_parent();
            if (container) {
                crosshairsActor = this._actor;
                if (this._actor.get_parent() != null) {
                    crosshairsActor = new Clutter.Clone({ source: this._actor });
                    this._clones.push(crosshairsActor);
                }
                if (this._actor.visible)
                    crosshairsActor.show();
                else
                    crosshairsActor.hide();

                container.add_actor(crosshairsActor);
                container.raise_child(magnifiedMouse, crosshairsActor);
                let [xMouse, yMouse] = magnifiedMouse.get_position();
                let [crosshairsWidth, crosshairsHeight] = crosshairsActor.get_size();
                crosshairsActor.set_position(xMouse - crosshairsWidth / 2 , yMouse - crosshairsHeight / 2);
            }
        }
        return crosshairsActor;
    },

    /**
     * removeFromParent:
     * Remove the crosshairs actor from its parent container.
     */
    removeFromParent: function() {
        this._actor.get_parent().remove_actor(this._actor);
    },

    /**
     * setColor:
     * Set the color of the crosshairs.
     * @clutterColor:   The color as a Clutter.Color.
     */
    setColor: function(clutterColor) {
        this._horizLeftHair.set_color(clutterColor);
        this._horizRightHair.set_color(clutterColor);
        this._vertTopHair.set_color(clutterColor);
        this._vertBottomHair.set_color(clutterColor);
    },

    /**
     * getColor:
     * Get the color of the crosshairs.
     * @color:  The color as a Clutter.Color.
     */
    getColor: function() {
        let clutterColor = new Clutter.Color();
        this._horizLeftHair.get_color(clutterColor);
        return clutterColor;
    },

    /**
     * setThickness:
     * Set the width of the vertical and horizontal lines of the crosshairs.
     * @thickness
     */
    setThickness: function(thickness) {
        this._horizLeftHair.set_height(thickness);
        this._horizRightHair.set_height(thickness);
        this._vertTopHair.set_width(thickness);
        this._vertBottomHair.set_width(thickness);
        this.reCenter();
    },

    /**
     * getThickness:
     * Get the width of the vertical and horizontal lines of the crosshairs.
     * @return:     The thickness of the crosshairs.
     */
    getThickness: function() {
        return this._horizLeftHair.get_height();
    },

    /**
     * setOpacity:
     * Set how opaque the crosshairs are.
     * @opacity:    Value between 0 (fully transparent) and 255 (full opaque).
     */
    setOpacity: function(opacity) {
        // set_opacity() throws an exception for values outside the range
        // [0, 255].
        if (opacity < 0)
            opacity = 0;
        else if (opacity > 255)
            opacity = 255;

        this._horizLeftHair.set_opacity(opacity);
        this._horizRightHair.set_opacity(opacity);
        this._vertTopHair.set_opacity(opacity);
        this._vertBottomHair.set_opacity(opacity);
    },

    /**
     * getOpacity:
     * Retriev how opaque the crosshairs are.
     * @return: A value between 0 (transparent) and 255 (opaque).
     */
    getOpacity: function() {
        return this._horizLeftHair.get_opacity();
    },

    /**
     * setLength:
     * Set the length of the vertical and horizontal lines in the crosshairs.
     * @length: The length of the crosshairs.
     */
    setLength: function(length) {
        this._horizLeftHair.set_width(length);
        this._horizRightHair.set_width(length);
        this._vertTopHair.set_height(length);
        this._vertBottomHair.set_height(length);
        this.reCenter();
    },

    /**
     * getLength:
     * Get the length of the vertical and horizontal lines in the crosshairs.
     * @return: The length of the crosshairs.
     */
    getLength: function() {
        return this._horizLeftHair.get_width();
    },

    /**
     * setClip:
     * Set the width and height of the rectangle that clips the crosshairs at
     * their intersection
     * @size:   Array of [width, height] defining the size of the clip
     *          rectangle.
     */
    setClip: function(size) {
        if (size) {
            // Take a chunk out of the crosshairs where it intersects the
            // mouse.
            this._clipSize = size;
            this.reCenter();
        }
        else {
            // Restore the missing chunk.
            this._clipSize = [0, 0];
            this.reCenter();
        }
     },

    /**
     * getClip:
     * Get the dimensions of the clip rectangle.
     * @return:   An array of the form [width, height].
     */
    getClip: function() {
        return this._clipSize;
    },

    /**
     * show:
     * Show the crosshairs.
     */
    show: function() {
        this._actor.show();
        // Clones don't share visibility.
        for (let i = 0; i < this._clones.length; i++)
            this._clones[i].show();
    },

    /**
     * hide:
     * Hide the crosshairs.
     */
    hide: function() {
        this._actor.hide();
        // Clones don't share visibility.
        for (let i = 0; i < this._clones.length; i++)
            this._clones[i].hide();
    },

    /**
     * reCenter:
     * Reposition the horizontal and vertical hairs such that they cross at
     * the center of crosshairs group.  If called with the dimensions of
     * the clip rectangle, these are used to update the size of the clip.
     * @clipSize:  Optional.  If present, an array of the form [width, height].
     */
    reCenter: function(clipSize) {
        let [groupWidth, groupHeight] = this._actor.get_size();
        let leftLength = this._horizLeftHair.get_width();
        let rightLength = this._horizRightHair.get_width();
        let topLength = this._vertTopHair.get_height();
        let bottomLength = this._vertBottomHair.get_height();
        let thickness = this._horizLeftHair.get_height();

        // Deal with clip rectangle.
        if (clipSize)
            this._clipSize = clipSize;
        let clipWidth = this._clipSize[0];
        let clipHeight = this._clipSize[1];

        // Note that clip, if present, is not centred on the cross hair
        // intersection, but biased towards the top left.
        let left = groupWidth / 2 - clipWidth * 0.25 - leftLength;
        let right = groupWidth / 2 + clipWidth * 0.75;
        let top = groupHeight / 2 - clipHeight * 0.25 - topLength - thickness / 2;
        let bottom = groupHeight / 2 + clipHeight * 0.75 + thickness / 2;
        this._horizLeftHair.set_position(left, (groupHeight - thickness) / 2);
        this._horizRightHair.set_position(right, (groupHeight - thickness) / 2);
        this._vertTopHair.set_position((groupWidth - thickness) / 2, top);
        this._vertBottomHair.set_position((groupWidth - thickness) / 2, bottom);
    }
 }
