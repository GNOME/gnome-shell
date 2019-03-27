import Atspi from 'gi://Atspi';
import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import GDesktopEnums from 'gi://GDesktopEnums';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Signals from '../misc/signals.js';

import * as Background from './background.js';
import * as FocusCaretTracker from './focusCaretTracker.js';
import * as Main from './main.js';
import * as Params from '../misc/params.js';
import * as PointerWatcher from './pointerWatcher.js';

const CROSSHAIRS_CLIP_SIZE = [100, 100];
const NO_CHANGE = 0.0;

const POINTER_REST_TIME = 1000; // milliseconds

// Settings
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

const MouseSpriteContent = GObject.registerClass({
    Implements: [Clutter.Content],
}, class MouseSpriteContent extends GObject.Object {
    _init() {
        super._init();
        this._scale = 1.0;
        this._monitorScale = 1.0;
        this._texture = null;
    }

    vfunc_get_preferred_size() {
        if (!this._texture)
            return [false, 0, 0];

        let width = this._texture.get_width() / this._scale;
        let height = this._texture.get_height() / this._scale;

        return [true, width, height];
    }

    vfunc_paint_content(actor, node, _paintContext) {
        if (!this._texture)
            return;

        let [minFilter, magFilter] = actor.get_content_scaling_filters();
        let textureNode = new Clutter.TextureNode(this._texture,
            null, minFilter, magFilter);
        textureNode.set_name('MouseSpriteContent');
        node.add_child(textureNode);

        textureNode.add_rectangle(actor.get_content_box());
    }

    _textureScale() {
        if (!this._texture)
            return 1;

        /* This is a workaround to guess the sprite scale; while it works fine
         * in normal scenarios, it's not guaranteed to work in all the cases,
         * and so we should actually add an API to mutter that will allow us
         * to know the real sprite texture scaling in order to adapt it to the
         * wanted one. */
        let avgSize = (this._texture.get_width() + this._texture.get_height()) / 2;
        return Math.max(1, Math.floor(avgSize / Meta.prefs_get_cursor_size() + .1));
    }

    _recomputeScale() {
        let scale = this._textureScale() / this._monitorScale;

        if (this._scale !== scale) {
            this._scale = scale;
            return true;
        }
        return false;
    }

    get texture() {
        return this._texture;
    }

    set texture(coglTexture) {
        if (this._texture === coglTexture)
            return;

        let oldTexture = this._texture;
        this._texture = coglTexture;
        this.invalidate();

        if (!oldTexture || !coglTexture ||
            oldTexture.get_width() !== coglTexture.get_width() ||
            oldTexture.get_height() !== coglTexture.get_height()) {
            this._recomputeScale();
            this.invalidate_size();
        }
    }

    set monitorScale(monitorScale) {
        this._monitorScale = monitorScale;
        if (this._recomputeScale())
            this.invalidate_size();
    }
});

export class Magnifier extends Signals.EventEmitter {
    constructor() {
        super();

        // Magnifier is a manager of ZoomRegions.
        this._zoomRegions = [];

        // Create small clutter tree for the magnified mouse.
        this._cursorTracker = global.backend.get_cursor_tracker();

        this._mouseSprite = new Clutter.Actor({request_mode: Clutter.RequestMode.CONTENT_SIZE});
        this._mouseSprite.content = new MouseSpriteContent();

        this._cursorRoot = new Clutter.Actor();
        this._cursorRoot.add_child(this._mouseSprite);

        const backend = this._cursorRoot.get_context().get_backend();
        this._seat = backend.get_default_seat();
        // Create the first ZoomRegion and initialize it according to the
        // magnification settings.

        [this.xMouse, this.yMouse] = global.get_pointer();

        let aZoomRegion = new ZoomRegion(this, this._cursorRoot);
        this._zoomRegions.push(aZoomRegion);
        this._settingsInit(aZoomRegion);
        aZoomRegion.scrollContentsTo(this.xMouse, this.yMouse);

        this._updateContentScale();

        St.Settings.get().connect('notify::magnifier-active', () => {
            this.setActive(St.Settings.get().magnifier_active);
        });

        this.setActive(St.Settings.get().magnifier_active);
        this._cursorUnfocusInhibited = false;
    }

    _updateContentScale() {
        let monitor = Main.layoutManager.findMonitorForPoint(this.xMouse,
            this.yMouse);
        this._mouseSprite.content.monitorScale = monitor
            ? monitor.geometry_scale : 1;
    }

    /**
     * showSystemCursor:
     * Show the system mouse pointer.
     */
    showSystemCursor() {
        if (this._cursorUnfocusInhibited) {
            this._seat.uninhibit_unfocus();
            this._cursorUnfocusInhibited = false;
        }

        if (this._cursorVisibleInhibited) {
            this._cursorTracker.uninhibit_cursor_visibility();
            this._cursorVisibleInhibited = false;
        }
    }

    /**
     * hideSystemCursor:
     * Hide the system mouse pointer.
     */
    hideSystemCursor() {
        if (!this._cursorUnfocusInhibited) {
            this._seat.inhibit_unfocus();
            this._cursorUnfocusInhibited = true;
        }

        if (!this._cursorVisibleInhibited) {
            this._cursorTracker.inhibit_cursor_visibility();
            this._cursorVisibleInhibited = true;
        }
    }

    /**
     * setActive:
     * Show/hide all the zoom regions.
     *
     * @param {boolean} activate Boolean to activate or de-activate the magnifier.
     */
    setActive(activate) {
        let isActive = this.isActive();

        this._zoomRegions.forEach(zoomRegion => {
            zoomRegion.setActive(activate);
        });

        if (isActive === activate)
            return;

        if (activate) {
            this._updateMouseSprite();
            this._cursorTracker.connectObject(
                'cursor-changed', this._updateMouseSprite.bind(this), this);
            global.compositor.disable_unredirect();
            this.startTrackingMouse();
        } else {
            this._cursorTracker.disconnectObject(this);
            this._mouseSprite.content.texture = null;
            global.compositor.enable_unredirect();
            this.stopTrackingMouse();
        }

        if (this._crossHairs)
            this._crossHairs.setEnabled(activate);

        // Make sure system mouse pointer is shown when all zoom regions are
        // invisible.
        if (!activate)
            this.showSystemCursor();

        // Notify interested parties of this change
        this.emit('active-changed', activate);
    }

    /**
     * isActive:
     *
     * @returns {boolean} Whether the magnifier is active.
     */
    isActive() {
        // Sufficient to check one ZoomRegion since Magnifier's active
        // state applies to all of them.
        if (this._zoomRegions.length === 0)
            return false;
        else
            return this._zoomRegions[0].isActive();
    }

    /**
     * startTrackingMouse:
     * Turn on mouse tracking, if not already doing so.
     */
    startTrackingMouse() {
        if (!this._pointerWatch) {
            let interval = 1000 / 60;
            this._pointerWatch = PointerWatcher.getPointerWatcher().addWatch(interval, this.scrollToMousePos.bind(this));

            this.scrollToMousePos();
        }
    }

    /**
     * stopTrackingMouse:
     * Turn off mouse tracking, if not already doing so.
     */
    stopTrackingMouse() {
        if (this._pointerWatch)
            this._pointerWatch.remove();

        this._pointerWatch = null;
    }

    /**
     * isTrackingMouse:
     *
     * @returns {boolean} whether the magnifier is currently tracking the mouse
     */
    isTrackingMouse() {
        return !!this._pointerWatch;
    }

    /**
     * scrollToMousePos:
     * Position all zoom regions' ROI relative to the current location of the
     * system pointer.
     *
     * @param {[xMouse: number, yMouse: number] | []} args
     */
    scrollToMousePos(...args) {
        const [xMouse, yMouse] = args.length ? args : global.get_pointer();

        if (xMouse === this.xMouse && yMouse === this.yMouse)
            return;

        this.xMouse = xMouse;
        this.yMouse = yMouse;

        this._updateContentScale();

        let sysMouseOverAny = false;
        this._zoomRegions.forEach(zoomRegion => {
            if (zoomRegion.scrollToMousePos())
                sysMouseOverAny = true;
        });
        if (sysMouseOverAny)
            this.hideSystemCursor();
        else
            this.showSystemCursor();
    }

    /**
     * createZoomRegion:
     * Create a ZoomRegion instance with the given properties.
     *
     * @param {number} xMagFactor
     *     The power to set horizontal magnification of the ZoomRegion. A value
     *     of 1.0 means no magnification, a value of 2.0 doubles the size.
     * @param {number} yMagFactor
     *    The power to set the vertical magnification of the ZoomRegion.
     * @param {{x: number, y: number, width: number, height: number}} roi
     *    The reg Object that defines the region to magnify, given in
     *    unmagnified coordinates.
     * @param {{x: number, y: number, width: number, height: number}} viewPort
     *     Object that defines the position of the ZoomRegion on screen.
     * @returns {ZoomRegion} the newly created ZoomRegion.
     */
    createZoomRegion(xMagFactor, yMagFactor, roi, viewPort) {
        let zoomRegion = new ZoomRegion(this, this._cursorRoot);
        zoomRegion.setViewPort(viewPort);

        // We ignore the redundant width/height on the ROI
        let fixedROI = Object.create(roi);
        fixedROI.width = viewPort.width / xMagFactor;
        fixedROI.height = viewPort.height / yMagFactor;
        zoomRegion.setROI(fixedROI);

        zoomRegion.addCrosshairs(this._crossHairs);
        return zoomRegion;
    }

    /**
     * addZoomRegion:
     * Append the given ZoomRegion to the list of currently defined ZoomRegions
     * for this Magnifier instance.
     *
     * @param {ZoomRegion} zoomRegion The zoomRegion to add.
     */
    addZoomRegion(zoomRegion) {
        if (zoomRegion) {
            this._zoomRegions.push(zoomRegion);
            if (!this.isTrackingMouse())
                this.startTrackingMouse();
        }
    }

    /**
     * getZoomRegions:
     * Return a list of ZoomRegion's for this Magnifier.
     *
     * @returns {ZoomRegion[]} The Magnifier's zoom region list.
     */
    getZoomRegions() {
        return this._zoomRegions;
    }

    /**
     * clearAllZoomRegions:
     * Remove all the zoom regions from this Magnfier's ZoomRegion list.
     */
    clearAllZoomRegions() {
        for (let i = 0; i < this._zoomRegions.length; i++)
            this._zoomRegions[i].setActive(false);

        this._zoomRegions.length = 0;
        this.stopTrackingMouse();
        this.showSystemCursor();
    }

    /**
     * addCrosshairs:
     * Add and show a cross hair centered on the magnified mouse.
     */
    addCrosshairs() {
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
        this._zoomRegions.forEach(zoomRegion => {
            zoomRegion.addCrosshairs(theCrossHairs);
        });
    }

    /**
     * setCrosshairsVisible:
     *
     * Show or hide the cross hair
     *
     * @param {boolean} visible Flag that indicates show (true) or hide (false).
     */
    setCrosshairsVisible(visible) {
        if (visible) {
            if (!this._crossHairs)
                this.addCrosshairs();
            this._crossHairs.show();
        } else {
            // eslint-disable-next-line no-lonely-if
            if (this._crossHairs)
                this._crossHairs.hide();
        }
    }

    /**
     * setCrosshairsColor:
     *
     * Set the color of the crosshairs for all ZoomRegions.
     *
     * @param {string} color The color as a string, e.g. '#ff0000ff' or 'red'.
     */
    setCrosshairsColor(color) {
        if (this._crossHairs) {
            let [res_, clutterColor] = Cogl.Color.from_string(color);
            this._crossHairs.setColor(clutterColor);
        }
    }

    /**
     * setCrosshairsThickness:
     *
     * Set the crosshairs thickness for all ZoomRegions.
     *
     * @param {number} thickness The width of the vertical and
     *     horizontal lines of the crosshairs.
     */
    setCrosshairsThickness(thickness) {
        if (this._crossHairs)
            this._crossHairs.setThickness(thickness);
    }

    /**
     * Get the crosshairs thickness.
     *
     * @returns {number} The width of the vertical and horizontal
     *     lines of the crosshairs.
     */
    getCrosshairsThickness() {
        if (this._crossHairs)
            return this._crossHairs.getThickness();
        else
            return 0;
    }

    /**
     * @param {number} opacity Value between 0.0 (transparent)
     *     and 1.0 (fully opaque).
     */
    setCrosshairsOpacity(opacity) {
        if (this._crossHairs)
            this._crossHairs.setOpacity(opacity * 255);
    }

    /**
     * @returns {number} Value between 0.0 (transparent) and 1.0 (fully opaque).
     */
    getCrosshairsOpacity() {
        if (this._crossHairs)
            return this._crossHairs.getOpacity() / 255.0;
        else
            return 0.0;
    }

    /**
     * Set the crosshairs length for all ZoomRegions.
     *
     * @param {number} length The length of the vertical and horizontal
     *     lines making up the crosshairs.
     */
    setCrosshairsLength(length) {
        if (this._crossHairs) {
            let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
            this._crossHairs.setLength(length / scaleFactor);
        }
    }

    /**
     * getCrosshairsLength:
     * Get the crosshairs length.
     *
     * @returns {number} The length of the vertical and horizontal
     *     lines making up the crosshairs.
     */
    getCrosshairsLength() {
        if (this._crossHairs)
            return this._crossHairs.getLength();
        else
            return 0;
    }

    /**
     * setCrosshairsClip:
     *
     * Set whether the crosshairs are clipped at their intersection.
     *
     * @param {boolean} clip Flag to indicate whether to clip the crosshairs.
     */
    setCrosshairsClip(clip) {
        if (!this._crossHairs)
            return;

        // Setting no clipping on crosshairs means a zero sized clip rectangle.
        this._crossHairs.setClip(clip ? CROSSHAIRS_CLIP_SIZE : [0, 0]);
    }

    /**
     * getCrosshairsClip:
     * Get whether the crosshairs are clipped by the mouse image.
     *
     * @returns {boolean} Whether the crosshairs are clipped.
     */
    getCrosshairsClip() {
        if (this._crossHairs) {
            let [clipWidth, clipHeight] = this._crossHairs.getClip();
            return clipWidth > 0 && clipHeight > 0;
        } else {
            return false;
        }
    }

    // Private methods //

    _updateMouseSprite() {
        this._updateSpriteTexture();
        let [xHot, yHot] = this._cursorTracker.get_hot();
        this._mouseSprite.set({
            translation_x: -xHot,
            translation_y: -yHot,
        });
    }

    _updateSpriteTexture() {
        let sprite = this._cursorTracker.get_sprite();

        if (sprite) {
            this._mouseSprite.content.texture = sprite;
            this._mouseSprite.show();
        } else {
            this._mouseSprite.hide();
        }
    }

    _settingsInit(zoomRegion) {
        this._settings = new Gio.Settings({schema_id: MAGNIFIER_SCHEMA});

        this._settings.connect(`changed::${SCREEN_POSITION_KEY}`,
            this._updateScreenPosition.bind(this));
        this._settings.connect(`changed::${MAG_FACTOR_KEY}`,
            this._updateMagFactor.bind(this));
        this._settings.connect(`changed::${LENS_MODE_KEY}`,
            this._updateLensMode.bind(this));
        this._settings.connect(`changed::${CLAMP_MODE_KEY}`,
            this._updateClampMode.bind(this));
        this._settings.connect(`changed::${MOUSE_TRACKING_KEY}`,
            this._updateMouseTrackingMode.bind(this));
        this._settings.connect(`changed::${FOCUS_TRACKING_KEY}`,
            this._updateFocusTrackingMode.bind(this));
        this._settings.connect(`changed::${CARET_TRACKING_KEY}`,
            this._updateCaretTrackingMode.bind(this));

        this._settings.connect(`changed::${INVERT_LIGHTNESS_KEY}`,
            this._updateInvertLightness.bind(this));
        this._settings.connect(`changed::${COLOR_SATURATION_KEY}`,
            this._updateColorSaturation.bind(this));

        this._settings.connect(`changed::${BRIGHT_RED_KEY}`,
            this._updateBrightness.bind(this));
        this._settings.connect(`changed::${BRIGHT_GREEN_KEY}`,
            this._updateBrightness.bind(this));
        this._settings.connect(`changed::${BRIGHT_BLUE_KEY}`,
            this._updateBrightness.bind(this));

        this._settings.connect(`changed::${CONTRAST_RED_KEY}`,
            this._updateContrast.bind(this));
        this._settings.connect(`changed::${CONTRAST_GREEN_KEY}`,
            this._updateContrast.bind(this));
        this._settings.connect(`changed::${CONTRAST_BLUE_KEY}`,
            this._updateContrast.bind(this));

        this._settings.connect(`changed::${SHOW_CROSS_HAIRS_KEY}`, () => {
            this.setCrosshairsVisible(this._settings.get_boolean(SHOW_CROSS_HAIRS_KEY));
        });

        this._settings.connect(`changed::${CROSS_HAIRS_THICKNESS_KEY}`, () => {
            this.setCrosshairsThickness(this._settings.get_int(CROSS_HAIRS_THICKNESS_KEY));
        });

        this._settings.connect(`changed::${CROSS_HAIRS_COLOR_KEY}`, () => {
            this.setCrosshairsColor(this._settings.get_string(CROSS_HAIRS_COLOR_KEY));
        });

        this._settings.connect(`changed::${CROSS_HAIRS_OPACITY_KEY}`, () => {
            this.setCrosshairsOpacity(this._settings.get_double(CROSS_HAIRS_OPACITY_KEY));
        });

        this._settings.connect(`changed::${CROSS_HAIRS_LENGTH_KEY}`, () => {
            this.setCrosshairsLength(this._settings.get_int(CROSS_HAIRS_LENGTH_KEY));
        });

        this._settings.connect(`changed::${CROSS_HAIRS_CLIP_KEY}`, () => {
            this.setCrosshairsClip(this._settings.get_boolean(CROSS_HAIRS_CLIP_KEY));
        });

        if (zoomRegion) {
            // Mag factor is accurate to two decimal places.
            let aPref = parseFloat(this._settings.get_double(MAG_FACTOR_KEY).toFixed(2));
            if (aPref !== 0.0)
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
    }

    _updateScreenPosition() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let position = this._settings.get_enum(SCREEN_POSITION_KEY);
            this._zoomRegions[0].setScreenPosition(position);
            if (position !== GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN)
                this._updateLensMode();
        }
    }

    _updateMagFactor() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            // Mag factor is accurate to two decimal places.
            let magFactor = parseFloat(this._settings.get_double(MAG_FACTOR_KEY).toFixed(2));
            this._zoomRegions[0].setMagFactor(magFactor, magFactor);
        }
    }

    _updateLensMode() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length)
            this._zoomRegions[0].setLensMode(this._settings.get_boolean(LENS_MODE_KEY));
    }

    _updateClampMode() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setClampScrollingAtEdges(
                !this._settings.get_boolean(CLAMP_MODE_KEY));
        }
    }

    _updateMouseTrackingMode() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setMouseTrackingMode(
                this._settings.get_enum(MOUSE_TRACKING_KEY));
        }
    }

    _updateFocusTrackingMode() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setFocusTrackingMode(
                this._settings.get_enum(FOCUS_TRACKING_KEY));
        }
    }

    _updateCaretTrackingMode() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setCaretTrackingMode(
                this._settings.get_enum(CARET_TRACKING_KEY));
        }
    }

    _updateInvertLightness() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setInvertLightness(
                this._settings.get_boolean(INVERT_LIGHTNESS_KEY));
        }
    }

    _updateColorSaturation() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            this._zoomRegions[0].setColorSaturation(
                this._settings.get_double(COLOR_SATURATION_KEY));
        }
    }

    _updateBrightness() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let brightness = {};
            brightness.r = this._settings.get_double(BRIGHT_RED_KEY);
            brightness.g = this._settings.get_double(BRIGHT_GREEN_KEY);
            brightness.b = this._settings.get_double(BRIGHT_BLUE_KEY);
            this._zoomRegions[0].setBrightness(brightness);
        }
    }

    _updateContrast() {
        // Applies only to the first zoom region.
        if (this._zoomRegions.length) {
            let contrast = {};
            contrast.r = this._settings.get_double(CONTRAST_RED_KEY);
            contrast.g = this._settings.get_double(CONTRAST_GREEN_KEY);
            contrast.b = this._settings.get_double(CONTRAST_BLUE_KEY);
            this._zoomRegions[0].setContrast(contrast);
        }
    }
}

class ZoomRegion {
    constructor(magnifier, mouseSourceActor) {
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
        this._brightness = {r: NO_CHANGE, g: NO_CHANGE, b: NO_CHANGE};
        this._contrast = {r: NO_CHANGE, g: NO_CHANGE, b: NO_CHANGE};

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

        this._pointerIdleMonitor = global.backend.get_core_idle_monitor();
        this._scrollContentsTimerId = 0;
    }

    _connectSignals() {
        if (this._signalConnections)
            return;

        this._signalConnections = [];
        let id = Main.layoutManager.connect('monitors-changed',
            this._monitorsChanged.bind(this));
        this._signalConnections.push([Main.layoutManager, id]);

        id = this._focusCaretTracker.connect('caret-moved', this._updateCaret.bind(this));
        this._signalConnections.push([this._focusCaretTracker, id]);

        id = this._focusCaretTracker.connect('focus-changed', this._updateFocus.bind(this));
        this._signalConnections.push([this._focusCaretTracker, id]);
    }

    _disconnectSignals() {
        for (let [obj, id] of this._signalConnections)
            obj.disconnect(id);

        delete this._signalConnections;
    }

    _updateScreenPosition() {
        if (this._screenPosition === GDesktopEnums.MagnifierScreenPosition.NONE) {
            this._setViewPort({
                x: this._viewPortX,
                y: this._viewPortY,
                width: this._viewPortWidth,
                height: this._viewPortHeight,
            });
        } else {
            this.setScreenPosition(this._screenPosition);
        }
    }

    _convertExtentsToScreenSpace(accessible, extents) {
        const toplevelWindowTypes = new Set([
            Atspi.Role.FRAME,
            Atspi.Role.DIALOG,
            Atspi.Role.WINDOW,
        ]);

        try {
            let app = null;
            let parentWindow = null;
            let iter = accessible;
            while (iter) {
                if (iter.get_role() === Atspi.Role.APPLICATION) {
                    app = iter;
                    /* This is the last Accessible we are interested in */
                    break;
                } else if (toplevelWindowTypes.has(iter.get_role())) {
                    parentWindow = iter;
                }
                iter = iter.get_parent();
            }

            /* We don't want to translate our own events to the focus window.
             * They are also already scaled by clutter before being sent, so
             * we don't need to do that here either. */
            if (app && app.get_name() === 'gnome-shell')
                return extents;

            /* Only events from the focused widget of the focused window. Some
             * widgets seem to claim to have focus when the window does not so
             * check both. */
            const windowActive = parentWindow &&
                parentWindow.get_state_set().contains(Atspi.StateType.ACTIVE);
            const accessibleFocused =
                accessible.get_state_set().contains(Atspi.StateType.FOCUSED);
            if (!windowActive || !accessibleFocused)
                return null;
        } catch (e) {
            throw new Error(`Failed to validate parent window: ${e}`);
        }

        const {focusWindow} = global.display;
        if (!focusWindow)
            return null;

        const windowRect = focusWindow.get_client_content_rect();
        const scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        const screenSpaceExtents = new Atspi.Rect({
            x: windowRect.x + (scaleFactor * extents.x),
            y: windowRect.y + (scaleFactor * extents.y),
            width: scaleFactor * extents.width,
            height: scaleFactor * extents.height,
        });

        return screenSpaceExtents;
    }

    _updateFocus(caller, event) {
        let component = event.source.get_component_iface();
        if (!component || event.detail1 !== 1)
            return;
        let extents;
        try {
            extents = component.get_extents(Atspi.CoordType.WINDOW);
            extents = this._convertExtentsToScreenSpace(event.source, extents);
            if (!extents)
                return;
        } catch (e) {
            log(`Failed to read extents of focused component: ${e.message}`);
            return;
        }

        const [xFocus, yFocus] = [
            extents.x + (extents.width / 2),
            extents.y + (extents.height / 2),
        ];

        if (this._xFocus !== xFocus || this._yFocus !== yFocus) {
            [this._xFocus, this._yFocus] = [xFocus, yFocus];
            this._centerFromFocusPosition();
        }
    }

    _updateCaret(caller, event) {
        let text = event.source.get_text_iface();
        if (!text)
            return;
        let extents;
        try {
            extents = text.get_character_extents(text.get_caret_offset(),
                Atspi.CoordType.WINDOW);
            extents = this._convertExtentsToScreenSpace(text, extents);
            if (!extents)
                return;
        } catch (e) {
            log(`Failed to read extents of text caret: ${e.message}`);
            return;
        }

        const [xCaret, yCaret] = [extents.x, extents.y];

        // Ignore event(s) if the caret size is none (0x0). This happens a lot if
        // the cursor offset can't be translated into a location. This is a work
        // around.
        if (extents.width === 0 && extents.height === 0)
            return;

        if (this._xCaret !== xCaret || this._yCaret !== yCaret) {
            [this._xCaret, this._yCaret] = [xCaret, yCaret];
            this._centerFromCaretPosition();
        }
    }

    /**
     * setActive:
     *
     * @param {boolean} activate Boolean to show/hide the ZoomRegion.
     */
    setActive(activate) {
        if (activate === this.isActive())
            return;

        if (activate) {
            this._createActors();
            if (this._isMouseOverRegion())
                this._magnifier.hideSystemCursor();
            this._updateScreenPosition();
            this._updateMagViewGeometry();
            this._updateCloneGeometry();
            this._updateMousePosition();
            this._connectSignals();
        } else {
            Main.uiGroup.set_opacity(255);
            this._disconnectSignals();
            this._destroyActors();
        }

        this._syncCaretTracking();
        this._syncFocusTracking();
    }

    /**
     * isActive:
     *
     * @returns {boolean} Whether this ZoomRegion is active
     */
    isActive() {
        return this._magView != null;
    }

    /**
     * setMagFactor:
     *
     * @param {number} xMagFactor The power to set the horizontal
     *     magnification factor to of the magnified view. A value of 1.0
     *     means no magnification. A value of 2.0 doubles the size.
     * @param {number} yMagFactor The power to set the vertical
     *     magnification factor to of the magnified view.
     */
    setMagFactor(xMagFactor, yMagFactor) {
        this._changeROI({
            xMagFactor,
            yMagFactor,
            redoCursorTracking: this._followingCursor,
            animate: true,
        });
    }

    /**
     * getMagFactor:
     *
     * @returns {number[]} an array, [xMagFactor, yMagFactor], containing
     *     the horizontal and vertical magnification powers. A value of
     *     1.0 means no magnification. A value of 2.0 means the contents
     *     are doubled in size, and so on.
     */
    getMagFactor() {
        return [this._xMagFactor, this._yMagFactor];
    }

    /**
     * setMouseTrackingMode
     *
     * @param {GDesktopEnums.MagnifierMouseTrackingMode} mode the new mode
     */
    setMouseTrackingMode(mode) {
        if (mode >= GDesktopEnums.MagnifierMouseTrackingMode.NONE &&
            mode <= GDesktopEnums.MagnifierMouseTrackingMode.PUSH)
            this._mouseTrackingMode = mode;
    }

    /**
     * getMouseTrackingMode:
     *
     * @returns {GDesktopEnums.MagnifierMouseTrackingMode} the current mode
     */
    getMouseTrackingMode() {
        return this._mouseTrackingMode;
    }

    /**
     * setFocusTrackingMode
     *
     * @param {GDesktopEnums.MagnifierFocusTrackingMode} mode the new mode
     */
    setFocusTrackingMode(mode) {
        this._focusTrackingMode = mode;
        this._syncFocusTracking();
    }

    /**
     * setCaretTrackingMode
     *
     * @param {GDesktopEnums.MagnifierCaretTrackingMode} mode the new mode
     */
    setCaretTrackingMode(mode) {
        this._caretTrackingMode = mode;
        this._syncCaretTracking();
    }

    _syncFocusTracking() {
        let enabled = this._focusTrackingMode !== GDesktopEnums.MagnifierFocusTrackingMode.NONE &&
            this.isActive();

        if (enabled)
            this._focusCaretTracker.registerFocusListener();
        else
            this._focusCaretTracker.deregisterFocusListener();
    }

    _syncCaretTracking() {
        let enabled = this._caretTrackingMode !== GDesktopEnums.MagnifierCaretTrackingMode.NONE &&
            this.isActive();

        if (enabled)
            this._focusCaretTracker.registerCaretListener();
        else
            this._focusCaretTracker.deregisterCaretListener();
    }

    /**
     * setViewPort
     * Sets the position and size of the ZoomRegion on screen.
     *
     * @param {{x: number, y: number, width: number, height: number}} viewPort
     *     Object defining the position and size of the view port.
     *     The values are in stage coordinate space.
     */
    setViewPort(viewPort) {
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.NONE;
    }

    /**
     * setROI
     * Sets the "region of interest" that the ZoomRegion is magnifying.
     *
     * @param {{x: number, y: number, width: number, height: number}} roi
     *     Object that defines the region of the screen to magnify.
     *     The values are in screen (unmagnified) coordinate space.
     */
    setROI(roi) {
        if (roi.width <= 0 || roi.height <= 0)
            return;

        this._followingCursor = false;
        this._changeROI({
            xMagFactor: this._viewPortWidth / roi.width,
            yMagFactor: this._viewPortHeight / roi.height,
            xCenter: roi.x + roi.width  / 2,
            yCenter: roi.y + roi.height / 2,
        });
    }

    /**
     * getROI:
     * Retrieves the "region of interest" -- the rectangular bounds of that part
     * of the desktop that the magnified view is showing (x, y, width, height).
     * The bounds are given in non-magnified coordinates.
     *
     * @returns {number[]} an array, [x, y, width, height], representing
     *     the bounding rectangle of what is shown in the magnified view.
     */
    getROI() {
        let roiWidth = this._viewPortWidth / this._xMagFactor;
        let roiHeight = this._viewPortHeight / this._yMagFactor;

        return [
            this._xCenter - roiWidth / 2,
            this._yCenter - roiHeight / 2,
            roiWidth, roiHeight,
        ];
    }

    /**
     * setLensMode:
     *
     *  Turn lens mode on/off.  In full screen mode, lens mode does nothing since
     * a lens the size of the screen is pointless.
     *
     * @param {boolean} lensMode Whether lensMode should be active
     */
    setLensMode(lensMode) {
        this._lensMode = lensMode;
        if (!this._lensMode)
            this.setScreenPosition(this._screenPosition);
    }

    /**
     * isLensMode:
     * Is lens mode on or off?
     *
     * @returns {boolean} The lens mode state.
     */
    isLensMode() {
        return this._lensMode;
    }

    /**
     * setClampScrollingAtEdges:
     * Stop vs. allow scrolling of the magnified contents when it scroll beyond
     * the edges of the screen.
     *
     *  @param {boolean} clamp Boolean to turn on/off clamping.
     */
    setClampScrollingAtEdges(clamp) {
        this._clampScrollingAtEdges = clamp;
        if (clamp)
            this._changeROI();
    }

    /**
     * setTopHalf:
     * Magnifier view occupies the top half of the screen.
     */
    setTopHalf() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height / 2;
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.TOP_HALF;
    }

    /**
     * setBottomHalf:
     * Magnifier view occupies the bottom half of the screen.
     */
    setBottomHalf() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = global.screen_height / 2;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height / 2;
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.BOTTOM_HALF;
    }

    /**
     * setLeftHalf:
     * Magnifier view occupies the left half of the screen.
     */
    setLeftHalf() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width / 2;
        viewPort.height = global.screen_height;
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.LEFT_HALF;
    }

    /**
     * setRightHalf:
     * Magnifier view occupies the right half of the screen.
     */
    setRightHalf() {
        let viewPort = {};
        viewPort.x = global.screen_width / 2;
        viewPort.y = 0;
        viewPort.width = global.screen_width / 2;
        viewPort.height = global.screen_height;
        this._setViewPort(viewPort);
        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.RIGHT_HALF;
    }

    /**
     * setFullScreenMode:
     * Set the ZoomRegion to full-screen mode.
     * Note:  disallows lens mode.
     */
    setFullScreenMode() {
        let viewPort = {};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = global.screen_width;
        viewPort.height = global.screen_height;
        this.setViewPort(viewPort);

        this._screenPosition = GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN;
    }

    /**
     * setScreenPosition:
     * Positions the zoom region to one of the enumerated positions on the
     * screen.
     *
     *  @param {GDesktopEnums.MagnifierScreenPosition} inPosition the position
     */
    setScreenPosition(inPosition) {
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
    }

    /**
     * getScreenPosition:
     * Tell the outside world what the current mode is -- magnifiying the
     * top half, bottom half, etc.
     *
     *  @returns {GDesktopEnums.MagnifierScreenPosition}: the current position.
     */
    getScreenPosition() {
        return this._screenPosition;
    }

    _clearScrollContentsTimer() {
        if (this._scrollContentsTimerId !== 0) {
            GLib.source_remove(this._scrollContentsTimerId);
            this._scrollContentsTimerId = 0;
        }
    }

    /**
     * scrollToMousePos:
     * Set the region of interest based on the position of the system pointer.
     *
     *  @returns {boolean}: Whether the system mouse pointer is over the
     *     magnified view.
     */
    scrollToMousePos() {
        this._followingCursor = true;
        if (this._mouseTrackingMode !== GDesktopEnums.MagnifierMouseTrackingMode.NONE)
            this._changeROI({redoCursorTracking: true});
        else
            this._updateMousePosition();

        this._clearScrollContentsTimer();
        this._scrollContentsTimerId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, POINTER_REST_TIME, () => {
            this._followingCursor = false;
            if (this._xDelayed != null && this._yDelayed != null) {
                this._scrollContentsToDelayed(this._xDelayed, this._yDelayed);
                this._xDelayed = null;
                this._yDelayed = null;
            }

            this._scrollContentsTimerId = 0;

            return GLib.SOURCE_REMOVE;
        });

        // Determine whether the system mouse pointer is over this zoom region.
        return this._isMouseOverRegion();
    }

    _scrollContentsToDelayed(x, y) {
        if (this._followingCursor) {
            this._xDelayed = x;
            this._yDelayed = y;
        } else {
            this.scrollContentsTo(x, y);
        }
    }

    /**
     * scrollContentsTo:
     * Shift the contents of the magnified view such it is centered on the given
     * coordinate.
     *
     *  @param {number} x The x-coord of the point to center on.
     * @param {number} y The y-coord of the point to center on.
     */
    scrollContentsTo(x, y) {
        if (x < 0 || x > global.screen_width ||
            y < 0 || y > global.screen_height)
            return;

        this._clearScrollContentsTimer();

        this._followingCursor = false;
        this._changeROI({
            xCenter: x,
            yCenter: y,
            animate: true,
        });
    }

    /**
     * addCrosshairs:
     * Add crosshairs centered on the magnified mouse.
     *
     *  @param {Crosshairs} crossHairs Crosshairs instance
     */
    addCrosshairs(crossHairs) {
        this._crossHairs = crossHairs;

        // If the crossHairs is not already within a larger container, add it
        // to this zoom region.  Otherwise, add a clone.
        if (crossHairs && this.isActive())
            this._crossHairsActor = crossHairs.addToZoomRegion(this, this._mouseActor);
    }

    /**
     * setInvertLightness:
     * Set whether to invert the lightness of the magnified view.
     *
     *  @param {boolean} flag whether brightness should be inverted
     */
    setInvertLightness(flag) {
        this._invertLightness = flag;
        if (this._magShaderEffects)
            this._magShaderEffects.setInvertLightness(this._invertLightness);
    }

    /**
     * getInvertLightness:
     *
     * Retrieve whether the lightness is inverted.
     *
     * @returns {boolean} whether brightness should be inverted
     */
    getInvertLightness() {
        return this._invertLightness;
    }

    /**
     * setColorSaturation:
     *
     * Set the color saturation of the magnified view.
     *
     * @param {number} saturation A value from 0.0 to 1.0 that defines
     *     the color saturation, with 0.0 defining no color (grayscale),
     *     and 1.0 defining full color.
     */
    setColorSaturation(saturation) {
        this._colorSaturation = saturation;
        if (this._magShaderEffects)
            this._magShaderEffects.setColorSaturation(this._colorSaturation);
    }

    /**
     * getColorSaturation:
     * Retrieve the color saturation of the magnified view.
     *
     *  @returns {number} the color saturation
     */
    getColorSaturation() {
        return this._colorSaturation;
    }

    /**
     * setBrightness:
     * Alter the brightness of the magnified view.
     *
     *  @param {object} brightness Object containing the contrast for the
     *     red, green, and blue channels. Values of 0.0 represent "standard"
     *     brightness (no change), whereas values less or greater than
     *     0.0 indicate decreased or incresaed brightness, respectively.
     *
     *     {number} brightness.r - the red component
     *     {number} brightness.g - the green component
     *     {number} brightness.b - the blue component
     */
    setBrightness(brightness) {
        this._brightness.r = brightness.r;
        this._brightness.g = brightness.g;
        this._brightness.b = brightness.b;
        if (this._magShaderEffects)
            this._magShaderEffects.setBrightness(this._brightness);
    }

    /**
     * setContrast:
     * Alter the contrast of the magnified view.
     *
     *  @param {object} contrast Object containing the contrast for the
     *     red, green, and blue channels. Values of 0.0 represent "standard"
     *     contrast (no change), whereas values less or greater than
     *     0.0 indicate decreased or incresaed contrast, respectively.
     *
     *     {number} contrast.r - the red component
     *     {number} contrast.g - the green component
     *     {number} contrast.b - the blue component
     */
    setContrast(contrast) {
        this._contrast.r = contrast.r;
        this._contrast.g = contrast.g;
        this._contrast.b = contrast.b;
        if (this._magShaderEffects)
            this._magShaderEffects.setContrast(this._contrast);
    }

    /**
     * getContrast:
     * Retrieve the contrast of the magnified view.
     *
     *  @returns {{r: number, g: number, b: number}}: Object containing
     *     the contrast for the red, green, and blue channels.
     */
    getContrast() {
        let contrast = {};
        contrast.r = this._contrast.r;
        contrast.g = this._contrast.g;
        contrast.b = this._contrast.b;
        return contrast;
    }

    // Private methods //

    _createActors() {
        // Add a group to clip the contents of the magnified view.
        const mainGroup = new Clutter.Actor({clip_to_allocation: true});

        // The root actor for the zoom region
        this._magView = new St.Bin({
            style_class: 'magnifier-zoom-region',
            child: mainGroup,
        });
        global.stage.add_child(this._magView);

        // hide the magnified region from CLUTTER_PICK_ALL
        Shell.util_set_hidden_from_pick(this._magView, true);

        // Add a background for when the magnified uiGroup is scrolled
        // out of view (don't want to see desktop showing through).
        this._background = new Background.SystemBackground();
        mainGroup.add_child(this._background);

        // Clone the group that contains all of UI on the screen.  This is the
        // chrome, the windows, etc.
        this._uiGroupClone = new Clutter.Clone({
            source: Main.uiGroup,
            clip_to_allocation: true,
        });
        mainGroup.add_child(this._uiGroupClone);

        // Add either the given mouseSourceActor to the ZoomRegion, or a clone of
        // it.
        if (this._mouseSourceActor.get_parent() != null)
            this._mouseActor = new Clutter.Clone({source: this._mouseSourceActor});
        else
            this._mouseActor = this._mouseSourceActor;
        mainGroup.add_child(this._mouseActor);

        if (this._crossHairs)
            this._crossHairsActor = this._crossHairs.addToZoomRegion(this, this._mouseActor);
        else
            this._crossHairsActor = null;

        // Contrast and brightness effects.
        this._magShaderEffects = new MagShaderEffects(mainGroup);
        this._magShaderEffects.setColorSaturation(this._colorSaturation);
        this._magShaderEffects.setInvertLightness(this._invertLightness);
        this._magShaderEffects.setBrightness(this._brightness);
        this._magShaderEffects.setContrast(this._contrast);
    }

    _destroyActors() {
        if (this._mouseActor === this._mouseSourceActor)
            this._mouseActor.get_parent().remove_child(this._mouseActor);
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
    }

    _setViewPort(viewPort, fromROIUpdate) {
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
            this._changeROI({redoCursorTracking: this._followingCursor}); // will update mouse

        if (this.isActive() && this._isMouseOverRegion())
            this._magnifier.hideSystemCursor();

        const uiGroupIsOccluded = this.isActive() && this._isFullScreen();
        Main.uiGroup.set_opacity(uiGroupIsOccluded ? 0 : 255);
    }

    _changeROI(params) {
        // Updates the area we are viewing; the magnification factors
        // and center can be set explicitly, or we can recompute
        // the position based on the mouse cursor position

        params = Params.parse(params, {
            xMagFactor: this._xMagFactor,
            yMagFactor: this._yMagFactor,
            xCenter: this._xCenter,
            yCenter: this._yCenter,
            redoCursorTracking: false,
            animate: false,
        });

        if (params.xMagFactor <= 0)
            params.xMagFactor = this._xMagFactor;
        if (params.yMagFactor <= 0)
            params.yMagFactor = this._yMagFactor;

        this._xMagFactor = params.xMagFactor;
        this._yMagFactor = params.yMagFactor;

        if (params.redoCursorTracking &&
            this._mouseTrackingMode !== GDesktopEnums.MagnifierMouseTrackingMode.NONE) {
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
        if (this._lensMode && !this._isFullScreen()) {
            this._setViewPort({
                x: this._xCenter - this._viewPortWidth / 2,
                y: this._yCenter - this._viewPortHeight / 2,
                width: this._viewPortWidth,
                height: this._viewPortHeight,
            }, true);
        }

        this._updateCloneGeometry(params.animate);
    }

    _isMouseOverRegion() {
        // Return whether the system mouse sprite is over this ZoomRegion.  If the
        // mouse's position is not given, then it is fetched.
        let mouseIsOver = false;
        if (this.isActive()) {
            let xMouse = this._magnifier.xMouse;
            let yMouse = this._magnifier.yMouse;

            mouseIsOver =
                xMouse >= this._viewPortX && xMouse < (this._viewPortX + this._viewPortWidth) &&
                yMouse >= this._viewPortY && yMouse < (this._viewPortY + this._viewPortHeight);
        }
        return mouseIsOver;
    }

    _isFullScreen() {
        // Does the magnified view occupy the whole screen? Note that this
        // doesn't necessarily imply
        // this._screenPosition = GDesktopEnums.MagnifierScreenPosition.FULL_SCREEN;

        if (this._viewPortX !== 0 || this._viewPortY !== 0)
            return false;
        if (this._viewPortWidth !== global.screen_width ||
            this._viewPortHeight !== global.screen_height)
            return false;
        return true;
    }

    _centerFromMousePosition() {
        // Determines where the center should be given the current cursor
        // position and mouse tracking mode

        let xMouse = this._magnifier.xMouse;
        let yMouse = this._magnifier.yMouse;

        if (this._mouseTrackingMode === GDesktopEnums.MagnifierMouseTrackingMode.PROPORTIONAL)
            return this._centerFromPointProportional(xMouse, yMouse);
        else if (this._mouseTrackingMode === GDesktopEnums.MagnifierMouseTrackingMode.PUSH)
            return this._centerFromPointPush(xMouse, yMouse);
        else if (this._mouseTrackingMode === GDesktopEnums.MagnifierMouseTrackingMode.CENTERED)
            return this._centerFromPointCentered(xMouse, yMouse);

        return null; // Should never be hit
    }

    _centerFromCaretPosition() {
        let xCaret = this._xCaret;
        let yCaret = this._yCaret;

        if (this._caretTrackingMode === GDesktopEnums.MagnifierCaretTrackingMode.PROPORTIONAL)
            [xCaret, yCaret] = this._centerFromPointProportional(xCaret, yCaret);
        else if (this._caretTrackingMode === GDesktopEnums.MagnifierCaretTrackingMode.PUSH)
            [xCaret, yCaret] = this._centerFromPointPush(xCaret, yCaret);
        else if (this._caretTrackingMode === GDesktopEnums.MagnifierCaretTrackingMode.CENTERED)
            [xCaret, yCaret] = this._centerFromPointCentered(xCaret, yCaret);

        this._scrollContentsToDelayed(xCaret, yCaret);
    }

    _centerFromFocusPosition() {
        let xFocus = this._xFocus;
        let yFocus = this._yFocus;

        if (this._focusTrackingMode === GDesktopEnums.MagnifierFocusTrackingMode.PROPORTIONAL)
            [xFocus, yFocus] = this._centerFromPointProportional(xFocus, yFocus);
        else if (this._focusTrackingMode === GDesktopEnums.MagnifierFocusTrackingMode.PUSH)
            [xFocus, yFocus] = this._centerFromPointPush(xFocus, yFocus);
        else if (this._focusTrackingMode === GDesktopEnums.MagnifierFocusTrackingMode.CENTERED)
            [xFocus, yFocus] = this._centerFromPointCentered(xFocus, yFocus);

        this._scrollContentsToDelayed(xFocus, yFocus);
    }

    _centerFromPointPush(xPoint, yPoint) {
        let [xRoi, yRoi, widthRoi, heightRoi] = this.getROI();
        let [cursorWidth, cursorHeight] = this._mouseSourceActor.get_size();
        let xPos = xRoi + widthRoi / 2;
        let yPos = yRoi + heightRoi / 2;
        let xRoiRight = xRoi + widthRoi - cursorWidth;
        let yRoiBottom = yRoi + heightRoi - cursorHeight;

        if (xPoint < xRoi)
            xPos -= xRoi - xPoint;
        else if (xPoint > xRoiRight)
            xPos += xPoint - xRoiRight;

        if (yPoint < yRoi)
            yPos -= yRoi - yPoint;
        else if (yPoint > yRoiBottom)
            yPos += yPoint - yRoiBottom;

        return [xPos, yPos];
    }

    _centerFromPointProportional(xPoint, yPoint) {
        let [xRoi_, yRoi_, widthRoi, heightRoi] = this.getROI();
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
        let yPos = yPoint - yProportion * (heightRoi / 2 - yPadding);

        return [xPos, yPos];
    }

    _centerFromPointCentered(xPoint, yPoint) {
        return [xPoint, yPoint];
    }

    _screenToViewPort(screenX, screenY) {
        // Converts coordinates relative to the (unmagnified) screen to coordinates
        // relative to the origin of this._magView
        return [
            this._viewPortWidth / 2 + (screenX - this._xCenter) * this._xMagFactor,
            this._viewPortHeight / 2 + (screenY - this._yCenter) * this._yMagFactor,
        ];
    }

    _updateMagViewGeometry() {
        if (!this.isActive())
            return;

        if (this._isFullScreen())
            this._magView.add_style_class_name('full-screen');
        else
            this._magView.remove_style_class_name('full-screen');

        this._magView.set_size(this._viewPortWidth, this._viewPortHeight);
        this._magView.set_position(this._viewPortX, this._viewPortY);
    }

    _updateCloneGeometry(animate = false) {
        if (!this.isActive())
            return;

        let [x, y] = this._screenToViewPort(0, 0);
        this._uiGroupClone.ease({
            x: Math.round(x),
            y: Math.round(y),
            scale_x: this._xMagFactor,
            scale_y: this._yMagFactor,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: animate ? 100 : 0,
        });

        let [mouseX, mouseY] = this._getMousePosition();
        this._mouseActor.ease({
            x: mouseX,
            y: mouseY,
            scale_x: this._xMagFactor,
            scale_y: this._yMagFactor,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: animate ? 100 : 0,
        });

        if (this._crossHairsActor) {
            let [crossX, crossY] = this._getCrossHairsPosition();
            this._crossHairsActor.ease({
                x: crossX,
                y: crossY,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: animate ? 100 : 0,
            });
        }
    }

    _updateMousePosition() {
        let [xMagMouse, yMagMouse] = this._getMousePosition();
        this._mouseActor.set_position(xMagMouse, yMagMouse);

        if (this._crossHairsActor) {
            let [crossX, crossY] = this._getCrossHairsPosition();
            this._crossHairsActor.set_position(crossX, crossY);
        }
    }

    _getMousePosition() {
        let [xMagMouse, yMagMouse] = this._screenToViewPort(
            this._magnifier.xMouse, this._magnifier.yMouse);
        return [Math.round(xMagMouse), Math.round(yMagMouse)];
    }

    _getCrossHairsPosition() {
        let [xMagMouse, yMagMouse] = this._getMousePosition();
        let [groupWidth, groupHeight] = this._crossHairsActor.get_size();

        return [xMagMouse - groupWidth / 2, yMagMouse - groupHeight / 2];
    }

    _monitorsChanged() {
        this._background.set_size(global.screen_width, global.screen_height);
        this._updateScreenPosition();
    }
}

const Crosshairs = GObject.registerClass(
class Crosshairs extends Clutter.Actor {
    _init() {
        // Set the group containing the crosshairs to three times the desktop
        // size in case the crosshairs need to appear to be infinite in
        // length (i.e., extend beyond the edges of the view they appear in).
        let groupWidth = global.screen_width * 3;
        let groupHeight = global.screen_height * 3;

        super._init({
            clip_to_allocation: false,
            width: groupWidth,
            height: groupHeight,
        });
        this._horizLeftHair = new Clutter.Actor();
        this._horizRightHair = new Clutter.Actor();
        this._vertTopHair = new Clutter.Actor();
        this._vertBottomHair = new Clutter.Actor();
        this.add_child(this._horizLeftHair);
        this.add_child(this._horizRightHair);
        this.add_child(this._vertTopHair);
        this.add_child(this._vertBottomHair);
        this._clipSize = [0, 0];
        this._clones = [];
        this.reCenter();
        this._monitorsChangedId = 0;
    }

    _monitorsChanged() {
        this.set_size(global.screen_width * 3, global.screen_height * 3);
        this.reCenter();
    }

    setEnabled(enabled) {
        if (enabled && this._monitorsChangedId === 0) {
            this._monitorsChangedId = Main.layoutManager.connect(
                'monitors-changed', this._monitorsChanged.bind(this));
        } else if (!enabled && this._monitorsChangedId !== 0) {
            Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;
        }
    }

    /**
     * Either add the crosshairs actor to the given ZoomRegion, or, if it is
     * already part of some other ZoomRegion, create a clone of the crosshairs
     * actor, and add the clone instead.  Returns either the original or the
     * clone.
     *
     * @param {ZoomRegion} zoomRegion The container to add the crosshairs
     *     group to.
     * @param {Clutter.Actor} magnifiedMouse The mouse actor for the
     *     zoom region -- used to position the crosshairs and properly
     *     layer them below the mouse.
     * @returns {Clutter.Actor} The crosshairs actor, or its clone.
     */
    addToZoomRegion(zoomRegion, magnifiedMouse) {
        let crosshairsActor = null;
        if (zoomRegion && magnifiedMouse) {
            let container = magnifiedMouse.get_parent();
            if (container) {
                crosshairsActor = this;
                if (this.get_parent() != null) {
                    crosshairsActor = new Clutter.Clone({source: this});
                    this._clones.push(crosshairsActor);

                    // Clones don't share visibility.
                    this.bind_property('visible',
                        crosshairsActor, 'visible',
                        GObject.BindingFlags.SYNC_CREATE);
                }

                container.add_child(crosshairsActor);
                container.set_child_above_sibling(magnifiedMouse, crosshairsActor);
                let [xMouse, yMouse] = magnifiedMouse.get_position();
                let [crosshairsWidth, crosshairsHeight] = crosshairsActor.get_size();
                crosshairsActor.set_position(xMouse - crosshairsWidth / 2, yMouse - crosshairsHeight / 2);
            }
        }
        return crosshairsActor;
    }

    /**
     * removeFromParent:
     *
     * Remove the crosshairs actor from its parent container, or destroy the
     * child actor if it was just a clone of the crosshairs actor.
     *
     * @param {Clutter.Actor} childActor the actor returned from
     *     addToZoomRegion
     */
    removeFromParent(childActor) {
        if (childActor === this)
            childActor.get_parent().remove_child(childActor);
        else
            childActor.destroy();
    }

    /**
     * setColor:
     * Set the color of the crosshairs.
     *
     *  @param {Cogl.Color} color The color
     */
    setColor(color) {
        this._horizLeftHair.background_color = color;
        this._horizRightHair.background_color = color;
        this._vertTopHair.background_color = color;
        this._vertBottomHair.background_color = color;
    }

    /**
     * setThickness:
     *
     * Set the width of the vertical and horizontal lines of the crosshairs.
     *
     * @param {number} thickness the new thickness value
     */
    setThickness(thickness) {
        this._horizLeftHair.set_height(thickness);
        this._horizRightHair.set_height(thickness);
        this._vertTopHair.set_width(thickness);
        this._vertBottomHair.set_width(thickness);
        this.reCenter();
    }

    /**
     * getThickness:
     * Get the width of the vertical and horizontal lines of the crosshairs.
     *
     * @returns {number} The thickness of the crosshairs.
     */
    getThickness() {
        return this._horizLeftHair.get_height();
    }

    /**
     * setOpacity:
     * Set how opaque the crosshairs are.
     *
     * @param {number} opacity Value between 0 (fully transparent)
     *     and 255 (full opaque).
     */
    setOpacity(opacity) {
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
    }

    /**
     * setLength:
     * Set the length of the vertical and horizontal lines in the crosshairs.
     *
     * @param {number} length The length of the crosshairs.
     */
    setLength(length) {
        this._horizLeftHair.set_width(length);
        this._horizRightHair.set_width(length);
        this._vertTopHair.set_height(length);
        this._vertBottomHair.set_height(length);
        this.reCenter();
    }

    /**
     * getLength:
     * Get the length of the vertical and horizontal lines in the crosshairs.
     *
     * @returns {number} The length of the crosshairs.
     */
    getLength() {
        return this._horizLeftHair.get_width();
    }

    /**
     * setClip:
     * Set the width and height of the rectangle that clips the crosshairs at
     * their intersection
     *
     * @param {[number, number]} size Array of [width, height] defining the size
     *     of the clip rectangle.
     */
    setClip(size) {
        if (size) {
            // Take a chunk out of the crosshairs where it intersects the
            // mouse.
            this._clipSize = size;
            this.reCenter();
        } else {
            // Restore the missing chunk.
            this._clipSize = [0, 0];
            this.reCenter();
        }
    }

    /**
     * reCenter:
     * Reposition the horizontal and vertical hairs such that they cross at
     * the center of crosshairs group.  If called with the dimensions of
     * the clip rectangle, these are used to update the size of the clip.
     *
     * @param {[number, number]} [clipSize] If present, the clip's [width, height].
     */
    reCenter(clipSize) {
        let [groupWidth, groupHeight] = this.get_size();
        let leftLength = this._horizLeftHair.get_width();
        let topLength = this._vertTopHair.get_height();
        let thickness = this._horizLeftHair.get_height();

        // Deal with clip rectangle.
        if (clipSize)
            this._clipSize = clipSize;
        let clipWidth = this._clipSize[0];
        let clipHeight = this._clipSize[1];

        let left = groupWidth / 2 - clipWidth / 2 - leftLength - thickness / 2;
        let right = groupWidth / 2 + clipWidth / 2 + thickness / 2;
        let top = groupHeight / 2 - clipHeight / 2 - topLength - thickness / 2;
        let bottom = groupHeight / 2 + clipHeight / 2 + thickness / 2;
        this._horizLeftHair.set_position(left, (groupHeight - thickness) / 2);
        this._horizRightHair.set_position(right, (groupHeight - thickness) / 2);
        this._vertTopHair.set_position((groupWidth - thickness) / 2, top);
        this._vertBottomHair.set_position((groupWidth - thickness) / 2, bottom);
    }
});

class MagShaderEffects {
    constructor(uiGroupClone) {
        this._inverse = new Shell.InvertLightnessEffect();
        this._brightnessContrast = new Clutter.BrightnessContrastEffect();
        this._colorDesaturation = new Clutter.DesaturateEffect();
        this._inverse.set_enabled(false);
        this._brightnessContrast.set_enabled(false);
        this._colorDesaturation.set_enabled(false);

        this._magView = uiGroupClone;
        this._magView.add_effect(this._inverse);
        this._magView.add_effect(this._brightnessContrast);
        this._magView.add_effect(this._colorDesaturation);
    }

    /**
     * destroyEffects:
     * Remove contrast and brightness effects from the magnified view, and
     * lose the reference to the actor they were applied to.  Don't use this
     * object after calling this.
     */
    destroyEffects() {
        this._magView.clear_effects();
        this._colorDesaturation = null;
        this._brightnessContrast = null;
        this._inverse = null;
        this._magView = null;
    }

    /**
     * setInvertLightness:
     * Enable/disable invert lightness effect.
     *
     * @param {boolean} invertFlag Enabled flag.
     */
    setInvertLightness(invertFlag) {
        this._inverse.set_enabled(invertFlag);
    }

    setColorSaturation(factor) {
        this._colorDesaturation.set_factor(1.0 - factor);
        this._colorDesaturation.set_enabled(factor !== 1.0);
    }

    /**
     * setBrightness:
     * Set the brightness of the magnified view.
     *
     * @param {object} brightness Object containing the contrast for the
     *     red, green, and blue channels. Values of 0.0 represent "standard"
     *     brightness (no change), whereas values less or greater than
     *     0.0 indicate decreased or incresaed brightness, respectively.
     *
     *     {number} brightness.r - the red component
     *     {number} brightness.g - the green component
     *     {number} brightness.b - the blue component
     */
    setBrightness(brightness) {
        let bRed = brightness.r;
        let bGreen = brightness.g;
        let bBlue = brightness.b;
        this._brightnessContrast.set_brightness_full(bRed, bGreen, bBlue);

        // Enable the effect if the brightness OR contrast change are such that
        // it modifies the brightness and/or contrast.
        let [cRed, cGreen, cBlue] = this._brightnessContrast.get_contrast();
        this._brightnessContrast.set_enabled(
            bRed !== NO_CHANGE || bGreen !== NO_CHANGE || bBlue !== NO_CHANGE ||
            cRed !== NO_CHANGE || cGreen !== NO_CHANGE || cBlue !== NO_CHANGE);
    }

    /**
     * Set the contrast of the magnified view.
     *
     * @param {object} contrast Object containing the contrast for the
     *     red, green, and blue channels. Values of 0.0 represent "standard"
     *     contrast (no change), whereas values less or greater than
     *     0.0 indicate decreased or incresaed contrast, respectively.
     *
     *     {number} contrast.r - the red component
     *     {number} contrast.g - the green component
     *     {number} contrast.b - the blue component
     */
    setContrast(contrast) {
        let cRed = contrast.r;
        let cGreen = contrast.g;
        let cBlue = contrast.b;

        this._brightnessContrast.set_contrast_full(cRed, cGreen, cBlue);

        // Enable the effect if the contrast OR brightness change are such that
        // it modifies the brightness and/or contrast.
        // should be able to use Cogl.Color.equal(), but that complains of
        // a null first argument.
        let [bRed, bGreen, bBlue] = this._brightnessContrast.get_brightness();
        this._brightnessContrast.set_enabled(
            cRed !== NO_CHANGE || cGreen !== NO_CHANGE || cBlue !== NO_CHANGE ||
            bRed !== NO_CHANGE || bGreen !== NO_CHANGE || bBlue !== NO_CHANGE);
    }
}
