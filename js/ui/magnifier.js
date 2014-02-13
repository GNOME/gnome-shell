// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Atspi = imports.gi.Atspi;
const Clutter = imports.gi.Clutter;
const GDesktopEnums = imports.gi.GDesktopEnums;
const Gio = imports.gi.Gio;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Signals = imports.signals;

const Background = imports.ui.background;
const FocusCaretTracker = imports.ui.focusCaretTracker;
const Main = imports.ui.main;
const MagnifierDBus = imports.ui.magnifierDBus;
const Params = imports.misc.params;
const PointerWatcher = imports.ui.pointerWatcher;

const MOUSE_POLL_FREQUENCY = 50;
const CROSSHAIRS_CLIP_SIZE = [100, 100];
const NO_CHANGE = 0.0;

// Settings
const APPLICATIONS_SCHEMA       = 'org.gnome.desktop.a11y.applications';
const SHOW_KEY                  = 'screen-magnifier-enabled';

const MAGNIFIER_SCHEMA          = 'org.gnome.desktop.a11y.magnifier';
const SCREEN_POSITION_KEY       = 'screen-position';
const MAG_FACTOR_KEY            = 'mag-factor';
const INVERT_LIGHTNESS_KEY      = 'invert-lightness';
const COLOR_SATURATION_KEY      = 'color-saturation';
const BRIGHT_RED_KEY            = 'brightness-red';
const BRIGHT_GREEN_KEY          = 'brightness-green';
const BRIGHT_BLUE_KEY           = 'brightness-blue';
const CONTRAST_RED_KEY          = 'contrast-red';
const CONTRAST_GREEN_KEY        = 'contrast-green';
const CONTRAST_BLUE_KEY         = 'contrast-blue';
const LENS_MODE_KEY             = 'lens-mode';
const CLAMP_MODE_KEY            = 'scroll-at-edges';
const MOUSE_TRACKING_KEY        = 'mouse-tracking';
const FOCUS_TRACKING_KEY        = 'focus-tracking';
const CARET_TRACKING_KEY        = 'caret-tracking';
const SHOW_CROSS_HAIRS_KEY      = 'show-cross-hairs';
const CROSS_HAIRS_THICKNESS_KEY = 'cross-hairs-thickness';
const CROSS_HAIRS_COLOR_KEY     = 'cross-hairs-color';
const CROSS_HAIRS_OPACITY_KEY   = 'cross-hairs-opacity';
const CROSS_HAIRS_LENGTH_KEY    = 'cross-hairs-length';
const CROSS_HAIRS_CLIP_KEY      = 'cross-hairs-clip';

let magDBusService = null;

const Magnifier = new Lang.Class({
    Name: 'Magnifier',

    _init: function() {
        // Magnifier is a manager of ZoomRegions.
        this._zoomRegions = [];

        // Create small clutter tree for the magnified mouse.
        let cursorTracker = Meta.CursorTracker.get_for_screen(global.screen);
        this._mouseSprite = new Clutter.Texture();
        Shell.util_cursor_tracker_to_clutter(cursorTracker, this._mouseSprite);
        this._cursorRoot = new Clutter.Actor();
        this._cursorRoot.add_actor(this._mouseSprite);

        // Create the first ZoomRegion and initialize it according to the
        // magnification settings.

        let mask;
        [this.xMouse, this.yMouse, mask] = global.get_pointer();

        let aZoomRegion = new ZoomRegion(this, this._cursorRoot);
        this._zoomRegions.push(aZoomRegion);
        let showAtLaunch = this._settingsInit(aZoomRegion);
        aZoomRegion.scrollContentsTo(this.xMouse, this.yMouse);

        cursorTracker.connect('cursor-changed', Lang.bind(this, this._updateMouseSprite));
        this._cursorTracker = cursorTracker;

        // Export to dbus.
        magDBusService = new MagnifierDBus.ShellMagnifier();
        this.setActive(showAtLaunch);
    },

    /**
     * showSystemCursor:
     * Show the system mouse pointer.
     */
    showSystemCursor: function() {
        this._cursorTracker.set_pointer_visible(true);
    },

    /**
     * hideSystemCursor:
     * Hide the system mouse pointer.
     */
    hideSystemCursor: function() {
        this._cursorTracker.set_pointer_visible(false);
    },

    /**
     * setActive:
     * Show/hide all the zoom regions.
     * @activate:   Boolean to activate or de-activate the magnifier.
     */
    setActive: function(activate) {
        let isActive = this.isActive();

        this._zoomRegions.forEach (function(zoomRegion, index, array) {
            zoomRegion.setActive(activate);
        });

        if (isActive != activate) {
            if (activate) {
                Meta.disable_unredirect_for_screen(global.screen);
                this.startTrackingMouse();
            } else {
                Meta.enable_unredirect_for_screen(global.screen);
                this.stopTrackingMouse();
            }
        }

        // Make sure system mouse pointer is shown when all zoom regions are
        // invisible.
        if (!activate)
            this._cursorTracker.set_pointer_visible(true);

        // Notify interested parties of this change
        this.emit('active-changed', activate);
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
        if (!this._pointerWatch)
            this._pointerWatch = PointerWatcher.getPointerWatcher().addWatch(MOUSE_POLL_FREQUENCY, Lang.bind(this, this.scrollToMousePos));
    },

    /**
     * stopTrackingMouse:
     * Turn off mouse tracking, if not already doing so.
     */
    stopTrackingMouse: function() {
        if (this._pointerWatch)
            this._pointerWatch.remove();

        this._pointerWatch = null;
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
     * @return      true.
     */
    scrollToMousePos: function() {
        let [xMouse, yMouse, mask] = global.get_pointer();

        if (xMouse != this.xMouse || yMouse != this.yMouse) {
            this.xMouse = xMouse;
            this.yMouse = yMouse;

            let sysMouseOverAny = false;
            this._zoomRegions.forEach(function(zoomRegion, index, array) {
                if (zoomRegion.scrollToMousePos())
                    sysMouseOverAny = true;
            });
            if (sysMouseOverAny)
                this.hideSystemCursor();
            else
                this.showSystemCursor();
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
        zoomRegion.setViewPort(viewPort);

        // We ignore the redundant width/height on the ROI
        let fixedROI = new Object(roi);
        fixedROI.width = viewPort.width / xMagFactor;
        fixedROI.height = viewPort.height / yMagFactor;
        zoomRegion.setROI(fixedROI);

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
        for (let i = 0; i < this._zoomRegions.length; i++)
            this._zoomRegions[i].setActive(false);

        this._zoomRegions.length = 0;
        this.stopTrackingMouse();
        this.showSystemCursor();
    },

    /**
     * addCrosshairs:
     * Add and show a cross hair centered on the magnified mouse.
     */
    addCrosshairs: function() {
        if (!this._crossHairs)
            this._crossHairs = new Crosshairs();

        let thickness = this._settings.get_int(CROSS_HAIRS_THICKNESS_KEY);
        let color = this._settings.get_string(CROSS_HAIRS_COLOR_KEY);
        let opacity = this._settings.get_double(CROSS_HAIRS_OPACITY_KEY);
        let length = this._settings.get_int(CROSS_HAIRS_LENGTH_KEY);
        let clip = this._settings.get_boolean(CROSS_HAIRS_CLIP_KEY);

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
                this.addCrosshairs();
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
     * @color:  The color as a string, e.g. '#ff0000ff' or 'red'.
     */
    setCrosshairsColor: function(color) {
        if (this._crossHairs) {
            let [res, clutterColor] = Clutter.Color.from_string(color);
            this._crossHairs.setColor(clutterColor);
        }
    },

    /**
     * getCrosshairsColor:
     * Get the color of the crosshairs.
     * @return: The color as a string, e.g. '#0000ffff' or 'blue'.
     */
    getCrosshairsColor: function() {
        if (this._crossHairs) {
            let clutterColor = this._crossHairs.getColor();
            return clutterColor.to_string();
        }
        else
            return '#00000000';
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
     * @opacity:    Value between 0.0 (transparent) and 1.0 (fully opaque).
     */
    setCrosshairsOpacity: function(opacity) {
        if (this._crossHairs)
            this._crossHairs.setOpacity(opacity * 255);
    },

    /**
     * getCrosshairsOpacity:
     * @return:     Value between 0.0 (transparent) and 1.0 (fully opaque).
     */
    getCrosshairsOpacity: function() {
        if (this._crossHairs)
            return this._crossHairs.getOpacity() / 255.0;
        else
            return 0.0;
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
                this._crossHairs.setClip(CROSSHAIRS_CLIP_SIZE);
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
        Shell.util_cursor_tracker_to_clutter(this._cursorTracker, this._mouseSprite);
        let [xHot, yHot] = this._cursorTracker.get_hot();
        this._mouseSprite.set_anchor_point(xHot, yHot);
    },

    _settingsInit: function(zoomRegion) {
        this._appSettings = new Gio.Settings({ schema: APPLICATIONS_SCHEMA });
        this._settings = new Gio.Settings({ schema: MAGNIFIER_SCHEMA });

        if (zoomRegion) {
            // Mag factor is accurate to two decimal places.
            let aPref = parseFloat(this._settings.get_double(MAG_FACTOR_KEY).toFixed(2));
            if (aPref != 0.0)
                zoomRegion.setMagFactor(aPref, aPref);

            aPref = this._settings.get_enum(SCREEN_POSITION_KEY);
            if (aPref)
                zoomRegion.setScreenPosition(aPref);

            zoomRegion.setLensMode(this._settings.get_boolean(LENS_MODE_KEY));
            zoomRegion.setClampScrollingAtEdges(!this._settings.get_boolean(CLAMP_MODE_KEY));

            aPref = this._settings.get_enum(MOUSE_TRACKING_KEY);
            if (aPref)
                zoomRegion.setMouseTrackingMode(aPref);

            aPref = this._settings.get_enum(FOCUS_TRACKING_KEY);
            if (aPref)
                zoomRegion.setFocusTrackingMode(aPref);

            aPref = this._settings.get_enum(CARET_TRACKING_KEY);
            if (aPref)
                zoomRegion.setCaretTrackingMode(aPref);

            aPref = this._settings.get_boolean(INVERT_LIGHTNESS_KEY);
            if (aPref)
                zoomRegion.setInvertLightness(aPref);

            aPref = this._settings.get_double(COLOR_SATURATION_KEY);
            if (aPref)
                zoomRegion.setColorSaturation(aPref);

            let bc = {};
            bc.r = this._settings.get_double(BRIGHT_RED_KEY);
            bc.g = this._settings.get_double(BRIGHT_GREEN_KEY);
            bc.b = this._settings.get_double(BRIGHT_BLUE_KEY);
            zoomRegion.setBrightness(bc);

            bc.r = this._settings.get_double(CONTRAST_RED_KEY);
            bc.g = this._settings.get_double(CONTRAST_GREEN_KEY);
            bc.b = this._settings.get_double(CONTRAST_BLUE_KEY);
            zoomRegion.setContrast(bc);
        }

        let showCrosshairs = this._settings.get_boolean(SHOW_CROSS_HAIRS_KEY);
        this.addCrosshairs();
        this.setCrosshairsVisible(showCrosshairs);

        this._appSettings.connect('changed::' + SHOW_KEY,
                                  Lang.bind(this, function() {
            this.setActive(this._appSettings.get_boolean(SHOW_KEY));
        }));

        this._settings.connect('changed::' + SCREEN_POSITION_KEY,
                               Lang.bind(this, this._updateScreenPosition));
        this._settings.connect('changed::' + MAG_FACTOR_KEY,
                               Lang.bind(this, this._updateMagFactor));
        this._settings.connect('changed::' + LENS_MODE_KEY,
                               Lang.bind(this, this._updateLensMode));
        this._settings.connect('changed::' + CLAMP_MODE_KEY,
                               Lang.bind(this, this._updateClampMode));
        this._settings.connect('changed::' + MOUSE_TRACKING_KEY,
                               Lang.bind(this, this._updateMouseTrackingMode));
        this._settings.connect('changed::' + FOCUS_TRACKING_KEY,
                               Lang.bind(this, this._updateFocusTrackingMode));
        this._settings.connect('changed::' + CARET_TRACKING_KEY,
                               Lang.bind(this, this._updateCaretTrackingMode));

        this._settings.connect('changed::' + INVERT_LIGHTNESS_KEY,
                               Lang.bind(this, this._updateInvertLightness));
        this._settings.connect('changed::' + COLOR_SATURATION_KEY,
                               Lang.bind(this, this._updateColorSaturation));

        this._settings.connect('changed::' + BRIGHT_RED_KEY,
                               Lang.bind(this, this._updateBrightness));
        this._settings.connect('changed::' + BRIGHT_GREEN_KEY,
                               Lang.bind(this, this._updateBrightness));
        this._settings.connect('changed::' + BRIGHT_BLUE_KEY,
                               Lang.bind(this, this._updateBrightness));

        this._settings.connect('changed::' + CONTRAST_RED_KEY,
                               Lang.bind(this, this._updateContrast));
        this._settings.connect('changed::' + CONTRAST_GREEN_KEY,
                               Lang.bind(this, this._updateContrast));
        this._settings.connect('changed::' + CONTRAST_BLUE_KEY,
                               Lang.bind(this, this._updateContrast));

        this._settings.connect('changed::' + SHOW_CROSS_HAIRS_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsVisible(this._settings.get_boolean(SHOW_CROSS_HAIRS_KEY));
        }));

        this._settings.connect('changed::' + CROSS_HAIRS_THICKNESS_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsThickness(this._settings.get_int(CROSS_HAIRS_THICKNESS_KEY));
        }));

        this._settings.connect('changed::' + CROSS_HAIRS_COLOR_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsColor(this._settings.get_string(CROSS_HAIRS_COLOR_KEY));
        }));

        this._settings.connect('changed::' + CROSS_HAIRS_OPACITY_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsOpacity(this._settings.get_double(CROSS_HAIRS_OPACITY_KEY));
        }));

        this._settings.connect('changed::' + CROSS_HAIRS_LENGTH_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsLength(this._settings.get_int(CROSS_HAIRS_LENGTH_KEY));
        }));

        this._settings.connect('changed::' + CROSS_HAIRS_CLIP_KEY,
                               Lang.bind(this, function() {
            this.setCrosshairsClip(this._settings.get_boolean(CROSS_HAIRS_CLIP_KEY));
        }));
        return this._appSettings.get_boolean(SHOW_KEY);
   },

    _updateScreenPosition: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let position = this._settings.get_enum(SCREEN_POSITION_KEY);
            this._zoomRegions[0].setScreenPosition(position);
            if (position != GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN)
                this._updateLensMode();
        }
    },

    _updateMagFactor: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            // Mag factor is accurate to two decimal places.
            let magFactor = parseFloat(this._settings.get_double(MAG_FACTOR_KEY).toFixed(2));
            this._zoomRegions[0].setMagFactor(magFactor, magFactor);
        }
    },

    _updateLensMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setLensMode(this._settings.get_boolean(LENS_MODE_KEY));
        }
    },

    _updateClampMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setClampScrollingAtEdges(
                !this._settings.get_boolean(CLAMP_MODE_KEY)
            );
        }
    },

    _updateMouseTrackingMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setMouseTrackingMode(
                this._settings.get_enum(MOUSE_TRACKING_KEY)
            );
        }
    },

    _updateFocusTrackingMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setFocusTrackingMode(
                this._settings.get_enum(FOCUS_TRACKING_KEY)
            );
        }
    },

    _updateCaretTrackingMode: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setCaretTrackingMode(
                this._settings.get_enum(CARET_TRACKING_KEY)
            );
        }
    },

    _updateInvertLightness: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setInvertLightness(
                this._settings.get_boolean(INVERT_LIGHTNESS_KEY)
            );
        }
    },

    _updateColorSaturation: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setColorSaturation(
                this._settings.get_double(COLOR_SATURATION_KEY)
            );
        }
    },

    _updateBrightness: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let brightness = {};
            brightness.r = this._settings.get_double(BRIGHT_RED_KEY);
            brightness.g = this._settings.get_double(BRIGHT_GREEN_KEY);
            brightness.b = this._settings.get_double(BRIGHT_BLUE_KEY);
            this._zoomRegions[0].setBrightness(brightness);
        }
    },

    _updateContrast: function() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let contrast = {};
            contrast.r = this._settings.get_double(CONTRAST_RED_KEY);
            contrast.g = this._settings.get_double(CONTRAST_GREEN_KEY);
            contrast.b = this._settings.get_double(CONTRAST_BLUE_KEY);
            this._zoomRegions[0].setContrast(contrast);
        }
    }
});
Signals.addSignalMethods(Magnifier.prototype);

const ZoomRegion = new Lang.Class({
    Name: 'ZoomRegion',

    _init: function(magnifier, mouseSourceActor) {
        this._magnifier = magnifier;
        this._focusCaretTracker = new FocusCaretTracker.FocusCaretTracker();

        this._mouseTrackingMode = GDesktopEnums.MagnifierMouseTrackingMode.NONE;
        this._focusTrackingMode = GDesktopEnums.MagnifierFocusTrackingMode.NONE;
        this._caretTrackingMode = GDesktopEnums.MagnifierCaretTrackingMode.NONE;
        this._clampScrollingAtEdges = false;
        this._lensMode = false;
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN;
        this._invertLightness = false;
        this._colorSaturation = 1.0;
        this._brightness = { r: NO_CHANGE, g: NO_CHANGE, b: NO_CHANGE };
        this._contrast = { r: NO_CHANGE, g: NO_CHANGE, b: NO_CHANGE };

        this._magView = null;
        this._background = null;
        this._uiGroupClone = null;
        this._mouseSourceActor = mouseSourceActor;
        this._mouseActor  = null;
        this._crossHairs = null;
        this._crossHairsActor = null;

        this._viewPortX = 0;
        this._viewPortY = 0;
        this._viewPortWidth = global.screen_width;
        this._viewPortHeight = global.screen_height;
        this._xCenter = this._viewPortWidth / 2;
        this._yCenter = this._viewPortHeight / 2;
        this._xMagFactor = 1;
        this._yMagFactor = 1;
        this._followingCursor = false;
        this._xFocus = 0;
        this._yFocus = 0;
        this._xCaret = 0;
        this._yCaret = 0;

        Main.layoutManager.connect('monitors-changed',
                                   Lang.bind(this, this._monitorsChanged));
        this._focusCaretTracker.connect('caret-moved',
                                    Lang.bind(this, this._updateCaret));
        this._focusCaretTracker.connect('focus-changed',
                                    Lang.bind(this, this._updateFocus));
    },

    _updateFocus: function(caller, event) {
        let component = event.source.get_component_iface();
        if (!component || event.detail1 != 1)
            return;
        let extents;
        try {
            extents = component.get_extents(Atspi.CoordType.SCREEN);
        } catch(e) {
            log('Failed to read extents of focused component: ' + e.message);
            return;
        }

        [this._xFocus, this._yFocus] = [extents.x + (extents.width / 2),
                                        extents.y + (extents.height / 2)];
        this._centerFromFocusPosition();
    },

    _updateCaret: function(caller, event) {
        let text = event.source.get_text_iface();
        if (!text)
            return;
        let extents;
        try {
            extents = text.get_character_extents(text.get_caret_offset(), 0);
        } catch(e) {
            log('Failed to read extents of text caret: ' + e.message);
            return;
        }

        [this._xCaret, this._yCaret] = [extents.x, extents.y];
        this._centerFromCaretPosition();
    },

    /**
     * setActive:
     * @activate:   Boolean to show/hide the ZoomRegion.
     */
    setActive: function(activate) {
        if (activate == this.isActive())
            return;

        if (activate) {
            this._createActors();
            if (this._isMouseOverRegion())
                this._magnifier.hideSystemCursor();
            this._updateMagViewGeometry();
            this._updateCloneGeometry();
            this._updateMousePosition();
        } else {
            this._destroyActors();
        }

        this._syncCaretTracking();
        this._syncFocusTracking();
    },

    /**
     * isActive:
     * @return  Whether this ZoomRegion is active (boolean).
     */
    isActive: function() {
        return this._magView != null;
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
        this._changeROI({ xMagFactor: xMagFactor,
                          yMagFactor: yMagFactor,
                          redoCursorTracking: this._followingCursor });
    },

    /**
     * getMagFactor:
     * @return  an array, [xMagFactor, yMagFactor], containing the horizontal
     *          and vertical magnification powers.  A value of 1.0 means no
     *          magnification.  A value of 2.0 means the contents are doubled
     *          in size, and so on.
     */
    getMagFactor: function() {
        return [this._xMagFactor, this._yMagFactor];
    },

    /**
     * setMouseTrackingMode
     * @mode:     One of the enum MouseTrackingMode values.
     */
    setMouseTrackingMode: function(mode) {
        if (mode >= GDesktopEnums.MagnifierMouseTrackingMode.NONE &&
            mode <= GDesktopEnums.MagnifierMouseTrackingMode.PUSH)
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
     * setFocusTrackingMode
     * @mode:     One of the enum FocusTrackingMode values.
     */
    setFocusTrackingMode: function(mode) {
        this._focusTrackingMode = mode;
        this._syncFocusTracking();
    },

    /**
     * setCaretTrackingMode
     * @mode:     One of the enum CaretTrackingMode values.
     */
    setCaretTrackingMode: function(mode) {
        this._caretTrackingMode = mode;
        this._syncCaretTracking();
    },

    _syncFocusTracking: function() {
        let enabled = this._focusTrackingMode != GDesktopEnums.MagnifierFocusTrackingMode.NONE &&
            this.isActive();

        if (enabled)
            this._focusCaretTracker.registerFocusListener();
        else
            this._focusCaretTracker.deregisterFocusListener();
    },

    _syncCaretTracking: function() {
        let enabled = this._caretTrackingMode != GDesktopEnums.MagnifierCaretTrackingMode.NONE &&
            this.isActive();

        if (enabled)
            this._focusCaretTracker.registerCaretListener();
        else
            this._focusCaretTracker.deregisterCaretListener();
    },

    /**
     * setViewPort
     * Sets the position and size of the ZoomRegion on screen.
     * @viewPort:   Object defining the position and size of the view port.
     *              It has members x, y, width, height.  The values are in
     *              stage coordinate space.
     */
    setViewPort: function(viewPort) {
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.NONE;
    },

    /**
     * setROI
     * Sets the "region of interest" that the ZoomRegion is magnifying.
     * @roi:    Object that defines the region of the screen to magnify.  It
     *          has members x, y, width, height.  The values are in
     *          screen (unmagnified) coordinate space.
     */
    setROI: function(roi) {
        if (roi.width <= 0 || roi.height <= 0)
            return;

        this._followingCursor = false;
        this._changeROI({ xMagFactor: this._viewPortWidth / roi.width,
                          yMagFactor: this._viewPortHeight / roi.height,
                          xCenter: roi.x + roi.width  / 2,
                          yCenter: roi.y + roi.height / 2 });
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
        let roiWidth = this._viewPortWidth / this._xMagFactor;
        let roiHeight = this._viewPortHeight / this._yMagFactor;

        return [this._xCenter - roiWidth / 2,
                this._yCenter - roiHeight / 2,
                roiWidth, roiHeight];
    },

    /**
     * setLensMode:
     * Turn lens mode on/off.  In full screen mode, lens mode does nothing since
     * a lens the size of the screen is pointless.
     * @lensMode:   A boolean to set the sense of lens mode.
     */
    setLensMode: function(lensMode) {
        this._lensMode = lensMode;
        if (!this._lensMode)
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
        if (clamp)
            this._changeROI();
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
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.TOP_HALF;
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
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.BOTTOM_HALF;
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
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.LEFT_HALF;
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
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.RIGHT_HALF;
    },

    /**
     * setFullScreenMode:
     * Set the ZoomRegion to full-screen mode.
     * Note:  disallows lens mode.
     */
    setFullScreenMode: function() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height;
        this.setViewPort(viewPort);

        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN;
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
            case GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN:
                this.setFullScreenMode();
                break;
            case GDesktopEnums.MagnifierScreenPosition.TOP_HALF:
                this.setTopHalf();
                break;
            case GDesktopEnums.MagnifierScreenPosition.BOTTOM_HALF:
                this.setBottomHalf();
                break;
            case GDesktopEnums.MagnifierScreenPosition.LEFT_HALF:
                this.setLeftHalf();
                break;
            case GDesktopEnums.MagnifierScreenPosition.RIGHT_HALF:
                this.setRightHalf();
                break;
        }
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
        this._followingCursor = true;
        if (this._mouseTrackingMode != GDesktopEnums.MagnifierMouseTrackingMode.NONE)
            this._changeROI({ redoCursorTracking: true });
        else
            this._updateMousePosition();

        // Determine whether the system mouse pointer is over this zoom region.
        return this._isMouseOverRegion();
    },

    /**
     * scrollContentsTo:
     * Shift the contents of the magnified view such it is centered on the given
     * coordinate.
     * @x:      The x-coord of the point to center on.
     * @y:      The y-coord of the point to center on.
     */
    scrollContentsTo: function(x, y) {
        this._followingCursor = false;
        this._changeROI({ xCenter: x,
                          yCenter: y });
    },

    /**
     * addCrosshairs:
     * Add crosshairs centered on the magnified mouse.
     * @crossHairs: Crosshairs instance
     */
    addCrosshairs: function(crossHairs) {
        this._crossHairs = crossHairs;

        // If the crossHairs is not already within a larger container, add it
        // to this zoom region.  Otherwise, add a clone.
        if (crossHairs && this.isActive()) {
            this._crossHairsActor = crossHairs.addToZoomRegion(this, this._mouseActor);
        }
    },

    /**
     * setInvertLightness:
     * Set whether to invert the lightness of the magnified view.
     * @flag    Boolean to either invert brightness (true), or not (false).
     */
    setInvertLightness: function(flag) {
        this._invertLightness = flag;
        if (this._magShaderEffects)
            this._magShaderEffects.setInvertLightness(this._invertLightness);
    },

    /**
     * getInvertLightness:
     * Retrieve whether the lightness is inverted.
     * @return    Boolean indicating inversion (true), or not (false).
     */
    getInvertLightness: function() {
        return this._invertLightness;
    },

    /**
     * setColorSaturation:
     * Set the color saturation of the magnified view.
     * @sauration  A value from 0.0 to 1.0 that defines the color
     *             saturation, with 0.0 defining no color (grayscale),
     *             and 1.0 defining full color.
     */
    setColorSaturation: function(saturation) {
        this._colorSaturation = saturation;
        if (this._magShaderEffects)
            this._magShaderEffects.setColorSaturation(this._colorSaturation);
    },

    /**
     * getColorSaturation:
     * Retrieve the color saturation of the magnified view.
     */
    getColorSaturation: function() {
        return this._colorSaturation;
    },

    /**
     * setBrightness:
     * Alter the brightness of the magnified view.
     * @brightness  Object containing the contrast for the red, green,
     *              and blue channels.  Values of 0.0 represent "standard"
     *              brightness (no change), whereas values less or greater than
     *              0.0 indicate decreased or incresaed brightness, respectively.
     */
    setBrightness: function(brightness) {
        this._brightness.r = brightness.r;
        this._brightness.g = brightness.g;
        this._brightness.b = brightness.b;
        if (this._magShaderEffects)
            this._magShaderEffects.setBrightness(this._brightness);
    },

    /**
     * setContrast:
     * Alter the contrast of the magnified view.
     * @contrast    Object containing the contrast for the red, green,
     *              and blue channels.  Values of 0.0 represent "standard"
     *              contrast (no change), whereas values less or greater than
     *              0.0 indicate decreased or incresaed contrast, respectively.
     */
    setContrast: function(contrast) {
        this._contrast.r = contrast.r;
        this._contrast.g = contrast.g;
        this._contrast.b = contrast.b;
        if (this._magShaderEffects)
            this._magShaderEffects.setContrast(this._contrast);
    },

    /**
     * getContrast:
     * Retreive the contrast of the magnified view.
     * @return  Object containing the contrast for the red, green,
     *          and blue channels.
     */
    getContrast: function() {
        let contrast = {};
        contrast.r = this._contrast.r;
        contrast.g = this._contrast.g;
        contrast.b = this._contrast.b;
        return contrast;
    },

    //// Private methods ////

    _createActors: function() {
        // The root actor for the zoom region
        this._magView = new St.Bin({ style_class: 'magnifier-zoom-region', x_fill: true, y_fill: true });
        global.stage.add_actor(this._magView);

        // hide the magnified region from CLUTTER_PICK_ALL
        Shell.util_set_hidden_from_pick (this._magView, true);

        // Add a group to clip the contents of the magnified view.
        let mainGroup = new Clutter.Actor({ clip_to_allocation: true });
        this._magView.set_child(mainGroup);

        // Add a background for when the magnified uiGroup is scrolled
        // out of view (don't want to see desktop showing through).
        this._background = new Clutter.Actor({ background_color: Main.DEFAULT_BACKGROUND_COLOR,
                                               layout_manager: new Clutter.BinLayout(),
                                               width: global.screen_width,
                                               height: global.screen_height });
        let noiseTexture = (new Background.SystemBackground()).actor;
        this._background.add_actor(noiseTexture);
        mainGroup.add_actor(this._background);

        // Clone the group that contains all of UI on the screen.  This is the
        // chrome, the windows, etc.
        this._uiGroupClone = new Clutter.Clone({ source: Main.uiGroup,
                                                 clip_to_allocation: true });
        mainGroup.add_actor(this._uiGroupClone);

        // Add either the given mouseSourceActor to the ZoomRegion, or a clone of
        // it.
        if (this._mouseSourceActor.get_parent() != null)
            this._mouseActor = new Clutter.Clone({ source: this._mouseSourceActor });
        else
            this._mouseActor = this._mouseSourceActor;
        mainGroup.add_actor(this._mouseActor);

        if (this._crossHairs)
            this._crossHairsActor = this._crossHairs.addToZoomRegion(this, this._mouseActor);
        else
            this._crossHairsActor = null;

        // Contrast and brightness effects.
        this._magShaderEffects = new MagShaderEffects(this._uiGroupClone);
        this._magShaderEffects.setColorSaturation(this._colorSaturation);
        this._magShaderEffects.setInvertLightness(this._invertLightness);
        this._magShaderEffects.setBrightness(this._brightness);
        this._magShaderEffects.setContrast(this._contrast);
    },

    _destroyActors: function() {
        if (this._mouseActor == this._mouseSourceActor)
            this._mouseActor.get_parent().remove_actor (this._mouseActor);
        if (this._crossHairs)
            this._crossHairs.removeFromParent(this._crossHairsActor);

        this._magShaderEffects.destroyEffects();
        this._magShaderEffects = null;
        this._magView.destroy();
        this._magView = null;
        this._background = null;
        this._uiGroupClone = null;
        this._mouseActor = null;
        this._crossHairsActor = null;
    },

    _setViewPort: function(viewPort, fromROIUpdate) {
        // Sets the position of the zoom region on the screen

        let width = Math.round(Math.min(viewPort.width, global.screen_width));
        let height = Math.round(Math.min(viewPort.height, global.screen_height));
        let x = Math.max(viewPort.x, 0);
        let y = Math.max(viewPort.y, 0);

        x = Math.round(Math.min(x, global.screen_width - width));
        y = Math.round(Math.min(y, global.screen_height - height));

        this._viewPortX = x;
        this._viewPortY = y;
        this._viewPortWidth = width;
        this._viewPortHeight = height;

        this._updateMagViewGeometry();

        if (!fromROIUpdate)
            this._changeROI({ redoCursorTracking: this._followingCursor }); // will update mouse

        if (this.isActive() && this._isMouseOverRegion())
            this._magnifier.hideSystemCursor();
    },

    _changeROI: function(params) {
        // Updates the area we are viewing; the magnification factors
        // and center can be set explicitly, or we can recompute
        // the position based on the mouse cursor position

        params = Params.parse(params, { xMagFactor: this._xMagFactor,
                                        yMagFactor: this._yMagFactor,
                                        xCenter: this._xCenter,
                                        yCenter: this._yCenter,
                                        redoCursorTracking: false });

        if (params.xMagFactor <= 0)
            params.xMagFactor = this._xMagFactor;
        if (params.yMagFactor <= 0)
            params.yMagFactor = this._yMagFactor;

        this._xMagFactor = params.xMagFactor;
        this._yMagFactor = params.yMagFactor;

        if (params.redoCursorTracking &&
            this._mouseTrackingMode != GDesktopEnums.MagnifierMouseTrackingMode.NONE) {
            // This depends on this.xMagFactor/yMagFactor already being updated
            [params.xCenter, params.yCenter] = this._centerFromMousePosition();
        }

        if (this._clampScrollingAtEdges) {
            let roiWidth = this._viewPortWidth / this._xMagFactor;
            let roiHeight = this._viewPortHeight / this._yMagFactor;

            params.xCenter = Math.min(params.xCenter, global.screen_width - roiWidth / 2);
            params.xCenter = Math.max(params.xCenter, roiWidth / 2);
            params.yCenter = Math.min(params.yCenter, global.screen_height - roiHeight / 2);
            params.yCenter = Math.max(params.yCenter, roiHeight / 2);
        }

        this._xCenter = params.xCenter;
        this._yCenter = params.yCenter;

        // If in lens mode, move the magnified view such that it is centered
        // over the actual mouse. However, in full screen mode, the "lens" is
        // the size of the screen -- pointless to move such a large lens around.
        if (this._lensMode && !this._isFullScreen())
            this._setViewPort({ x: this._xCenter - this._viewPortWidth / 2,
                                y: this._yCenter - this._viewPortHeight / 2,
                                width: this._viewPortWidth,
                                height: this._viewPortHeight }, true);

        this._updateCloneGeometry();
        this._updateMousePosition();
    },

    _isMouseOverRegion: function() {
        // Return whether the system mouse sprite is over this ZoomRegion.  If the
        // mouse's position is not given, then it is fetched.
        let mouseIsOver = false;
        if (this.isActive()) {
            let xMouse = this._magnifier.xMouse;
            let yMouse = this._magnifier.yMouse;

            mouseIsOver = (
                xMouse >= this._viewPortX && xMouse < (this._viewPortX + this._viewPortWidth) &&
                yMouse >= this._viewPortY && yMouse < (this._viewPortY + this._viewPortHeight)
            );
        }
        return mouseIsOver;
    },

    _isFullScreen: function() {
        // Does the magnified view occupy the whole screen? Note that this
        // doesn't necessarily imply
        // this._screenPosition = GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN;

        if (this._viewPortX != 0 || this._viewPortY != 0)
            return false;
        if (this._viewPortWidth != global.screen_width ||
            this._viewPortHeight != global.screen_height)
            return false;
        return true;
    },

    _centerFromMousePosition: function() {
        // Determines where the center should be given the current cursor
        // position and mouse tracking mode

        let xMouse = this._magnifier.xMouse;
        let yMouse = this._magnifier.yMouse;

        if (this._mouseTrackingMode == GDesktopEnums.MagnifierMouseTrackingMode.PROPORTIONAL) {
            return this._centerFromPointProportional(xMouse, yMouse);
        }
        else if (this._mouseTrackingMode == GDesktopEnums.MagnifierMouseTrackingMode.PUSH) {
            return this._centerFromPointPush(xMouse, yMouse);
        }
        else if (this._mouseTrackingMode == GDesktopEnums.MagnifierMouseTrackingMode.CENTERED) {
            return this._centerFromPointCentered(xMouse, yMouse);
        }

        return null; // Should never be hit
    },

    _centerFromCaretPosition: function() {
        let xCaret = this._xCaret;
        let yCaret = this._yCaret;

        if (this._caretTrackingMode == GDesktopEnums.MagnifierCaretTrackingMode.PROPORTIONAL)
            [xCaret, yCaret] = this._centerFromPointProportional(xCaret, yCaret);
        else if (this._caretTrackingMode == GDesktopEnums.MagnifierCaretTrackingMode.PUSH)
            [xCaret, yCaret] = this._centerFromPointPush(xCaret, yCaret);
        else if (this._caretTrackingMode == GDesktopEnums.MagnifierCaretTrackingMode.CENTERED)
            [xCaret, yCaret] = this._centerFromPointCentered(xCaret, yCaret);

        this.scrollContentsTo(xCaret, yCaret);
    },

    _centerFromFocusPosition: function() {
        let xFocus = this._xFocus;
        let yFocus = this._yFocus;

        if (this._focusTrackingMode == GDesktopEnums.MagnifierFocusTrackingMode.PROPORTIONAL)
            [xFocus, yFocus] = this._centerFromPointProportional(xFocus, yFocus);
        else if (this._focusTrackingMode == GDesktopEnums.MagnifierFocusTrackingMode.PUSH)
            [xFocus, yFocus] = this._centerFromPointPush(xFocus, yFocus);
        else if (this._focusTrackingMode == GDesktopEnums.MagnifierFocusTrackingMode.CENTERED)
            [xFocus, yFocus] = this._centerFromPointCentered(xFocus, yFocus);

        this.scrollContentsTo(xFocus, yFocus);
    },

    _centerFromPointPush: function(xPoint, yPoint) {
        let [xRoi, yRoi, widthRoi, heightRoi] = this.getROI();
        let [cursorWidth, cursorHeight] = this._mouseSourceActor.get_size();
        let xPos = xRoi + widthRoi / 2;
        let yPos = yRoi + heightRoi / 2;
        let xRoiRight = xRoi + widthRoi - cursorWidth;
        let yRoiBottom = yRoi + heightRoi - cursorHeight;

        if (xPoint < xRoi)
            xPos -= (xRoi - xPoint);
        else if (xPoint > xRoiRight)
            xPos += (xPoint - xRoiRight);

        if (yPoint < yRoi)
            yPos -= (yRoi - yPoint);
        else if (yPoint > yRoiBottom)
            yPos += (yPoint - yRoiBottom);

        return [xPos, yPos];
    },

    _centerFromPointProportional: function(xPoint, yPoint) {
        let [xRoi, yRoi, widthRoi, heightRoi] = this.getROI();
        let halfScreenWidth = global.screen_width / 2;
        let halfScreenHeight = global.screen_height / 2;
        // We want to pad with a constant distance after zooming, so divide
        // by the magnification factor.
        let unscaledPadding = Math.min(this._viewPortWidth, this._viewPortHeight) / 5;
        let xPadding = unscaledPadding / this._xMagFactor;
        let yPadding = unscaledPadding / this._yMagFactor;
        let xProportion = (xPoint - halfScreenWidth) / halfScreenWidth;   // -1 ... 1
        let yProportion = (yPoint - halfScreenHeight) / halfScreenHeight; // -1 ... 1
        let xPos = xPoint - xProportion * (widthRoi / 2 - xPadding);
        let yPos = yPoint - yProportion * (heightRoi /2 - yPadding);

        return [xPos, yPos];
    },

    _centerFromPointCentered: function(xPoint, yPoint) {
        return [xPoint, yPoint];
    },

    _screenToViewPort: function(screenX, screenY) {
        // Converts coordinates relative to the (unmagnified) screen to coordinates
        // relative to the origin of this._magView
        return [this._viewPortWidth / 2 + (screenX - this._xCenter) * this._xMagFactor,
                this._viewPortHeight / 2 + (screenY - this._yCenter) * this._yMagFactor];
    },

    _updateMagViewGeometry: function() {
        if (!this.isActive())
            return;

        if (this._isFullScreen())
            this._magView.add_style_class_name('full-screen');
        else
            this._magView.remove_style_class_name('full-screen');

        this._magView.set_size(this._viewPortWidth, this._viewPortHeight);
        this._magView.set_position(this._viewPortX, this._viewPortY);
    },

    _updateCloneGeometry: function() {
        if (!this.isActive())
            return;

        this._uiGroupClone.set_scale(this._xMagFactor, this._yMagFactor);
        this._mouseActor.set_scale(this._xMagFactor, this._yMagFactor);

        let [x, y] = this._screenToViewPort(0, 0);
        this._uiGroupClone.set_position(Math.round(x), Math.round(y));

        this._updateMousePosition();
    },

    _updateMousePosition: function() {
        if (!this.isActive())
            return;

        let [xMagMouse, yMagMouse] = this._screenToViewPort(this._magnifier.xMouse,
                                                            this._magnifier.yMouse);

        xMagMouse = Math.round(xMagMouse);
        yMagMouse = Math.round(yMagMouse);

        this._mouseActor.set_position(xMagMouse, yMagMouse);

        if (this._crossHairsActor) {
            let [groupWidth, groupHeight] = this._crossHairsActor.get_size();
            this._crossHairsActor.set_position(xMagMouse - groupWidth / 2,
                                               yMagMouse - groupHeight / 2);
        }
    },

    _monitorsChanged: function() {
        if (!this.isActive())
            return;

        this._background.set_size(global.screen_width, global.screen_height);

        if (this._screenPosition == GDesktopEnums.MagnifierScreenPosition.NONE)
            this._setViewPort({ x: this._viewPortX,
                                y: this._viewPortY,
                                width: this._viewPortWidth,
                                height: this._viewPortHeight });
        else
            this.setScreenPosition(this._screenPosition);
    }
});

const Crosshairs = new Lang.Class({
    Name: 'Crosshairs',

    _init: function() {

        // Set the group containing the crosshairs to three times the desktop
        // size in case the crosshairs need to appear to be infinite in
        // length (i.e., extend beyond the edges of the view they appear in).
        let groupWidth = global.screen_width * 3;
        let groupHeight = global.screen_height * 3;

        this._actor = new Clutter.Actor({
            clip_to_allocation: false,
            width: groupWidth,
            height: groupHeight
        });
        this._horizLeftHair = new Clutter.Actor();
        this._horizRightHair = new Clutter.Actor();
        this._vertTopHair = new Clutter.Actor();
        this._vertBottomHair = new Clutter.Actor();
        this._actor.add_actor(this._horizLeftHair);
        this._actor.add_actor(this._horizRightHair);
        this._actor.add_actor(this._vertTopHair);
        this._actor.add_actor(this._vertBottomHair);
        this._clipSize = [0, 0];
        this._clones = [];
        this.reCenter();

        Main.layoutManager.connect('monitors-changed',
                                   Lang.bind(this, this._monitorsChanged));
    },

    _monitorsChanged: function() {
        this._actor.set_size(global.screen_width * 3, global.screen_height * 3);
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
                crosshairsActor.visible = this._actor.visible;

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
     * @childActor: the actor returned from addToZoomRegion
     * Remove the crosshairs actor from its parent container, or destroy the
     * child actor if it was just a clone of the crosshairs actor.
     */
    removeFromParent: function(childActor) {
        if (childActor == this._actor)
            childActor.get_parent().remove_actor(childActor);
        else
            childActor.destroy();
    },

    /**
     * setColor:
     * Set the color of the crosshairs.
     * @clutterColor:   The color as a Clutter.Color.
     */
    setColor: function(clutterColor) {
        this._horizLeftHair.background_color = clutterColor;
        this._horizRightHair.background_color = clutterColor;
        this._vertTopHair.background_color = clutterColor;
        this._vertBottomHair.background_color = clutterColor;
    },

    /**
     * getColor:
     * Get the color of the crosshairs.
     * @color:  The color as a Clutter.Color.
     */
    getColor: function() {
        return this._horizLeftHair.get_color();
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
});

const MagShaderEffects = new Lang.Class({
    Name: 'MagShaderEffects',

    _init: function(uiGroupClone) {
        this._inverse = new Shell.InvertLightnessEffect();
        this._brightnessContrast = new Clutter.BrightnessContrastEffect();
        this._colorDesaturation = new Clutter.DesaturateEffect();
        this._inverse.set_enabled(false);
        this._brightnessContrast.set_enabled(false);

        this._magView = uiGroupClone;
        this._magView.add_effect(this._inverse);
        this._magView.add_effect(this._brightnessContrast);
        this._magView.add_effect(this._colorDesaturation);
    },

    /**
     * destroyEffects:
     * Remove contrast and brightness effects from the magnified view, and
     * lose the reference to the actor they were applied to.  Don't use this
     * object after calling this.
     */
    destroyEffects: function() {
        this._magView.clear_effects();
        this._colorDesaturation = null;
        this._brightnessContrast = null;
        this._inverse = null;
        this._magView = null;
    },

    /**
     * setInvertLightness:
     * Enable/disable invert lightness effect.
     * @invertFlag:     Enabled flag.
     */
    setInvertLightness: function(invertFlag) {
        this._inverse.set_enabled(invertFlag);
    },

    setColorSaturation: function(factor) {
        this._colorDesaturation.set_factor(1.0 - factor);
    },

    /**
     * setBrightness:
     * Set the brightness of the magnified view.
     * @brightness: Object containing the brightness for the red, green,
     *              and blue channels.  Values of 0.0 represent "standard"
     *              brightness (no change), whereas values less or greater than
     *              0.0 indicate decreased or incresaed brightness,
     *              respectively.
     */
    setBrightness: function(brightness) {
        let bRed = brightness.r;
        let bGreen = brightness.g;
        let bBlue = brightness.b;
        this._brightnessContrast.set_brightness_full(bRed, bGreen, bBlue);

        // Enable the effect if the brightness OR contrast change are such that
        // it modifies the brightness and/or contrast.
        let [cRed, cGreen, cBlue] = this._brightnessContrast.get_contrast();
        this._brightnessContrast.set_enabled(
            (bRed != NO_CHANGE || bGreen != NO_CHANGE || bBlue != NO_CHANGE ||
             cRed != NO_CHANGE || cGreen != NO_CHANGE || cBlue != NO_CHANGE)
        );
    },

    /**
     * Set the contrast of the magnified view.
     * @contrast:   Object containing the contrast for the red, green,
     *              and blue channels.  Values of 0.0 represent "standard"
     *              contrast (no change), whereas values less or greater than
     *              0.0 indicate decreased or incresaed contrast, respectively.
     */
    setContrast: function(contrast) {
        let cRed = contrast.r;
        let cGreen = contrast.g;
        let cBlue = contrast.b;

        this._brightnessContrast.set_contrast_full(cRed, cGreen, cBlue);

        // Enable the effect if the contrast OR brightness change are such that
        // it modifies the brightness and/or contrast.
        // should be able to use Clutter.color_equal(), but that complains of
        // a null first argument.
        let [bRed, bGreen, bBlue] = this._brightnessContrast.get_brightness();
        this._brightnessContrast.set_enabled(
             cRed != NO_CHANGE || cGreen != NO_CHANGE || cBlue != NO_CHANGE ||
             bRed != NO_CHANGE || bGreen != NO_CHANGE || bBlue != NO_CHANGE
        );
    },
});
