// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Workspace */

const {Clutter, GLib, GObject, Graphene, Meta, Shell, St} = imports.gi;

const Background = imports.ui.background;
const DND = imports.ui.dnd;
const Main = imports.ui.main;
const OverviewControls = imports.ui.overviewControls;
const Params = imports.misc.params;
const Util = imports.misc.util;
const { WindowPreview } = imports.ui.windowPreview;

var WINDOW_PREVIEW_MAXIMUM_SCALE = 0.95;

var WINDOW_REPOSITIONING_DELAY = 750;

// When calculating a layout, we calculate the scale of windows and the percent
// of the available area the new layout uses. If the values for the new layout,
// when weighted with the values as below, are worse than the previous layout's,
// we stop looking for a new layout and use the previous layout.
// Otherwise, we keep looking for a new layout.
var LAYOUT_SCALE_WEIGHT = 1;
var LAYOUT_SPACE_WEIGHT = 0.1;

const BACKGROUND_CORNER_RADIUS_PIXELS = 30;

// Window Thumbnail Layout Algorithm
// =================================
//
// General overview
// ----------------
//
// The window thumbnail layout algorithm calculates some optimal layout
// by computing layouts with some number of rows, calculating how good
// each layout is, and stopping iterating when it finds one that is worse
// than the previous layout. A layout consists of which windows are in
// which rows, row sizes and other general state tracking that would make
// calculating window positions from this information fairly easy.
//
// After a layout is computed that's considered the best layout, we
// compute the layout scale to fit it in the area, and then compute
// slots (sizes and positions) for each thumbnail.
//
// Layout generation
// -----------------
//
// Layout generation is naive and simple: we simply add windows to a row
// until we've added too many windows to a row, and then make a new row,
// until we have our required N rows. The potential issue with this strategy
// is that we may have too many windows at the bottom in some pathological
// cases, which tends to make the thumbnails have the shape of a pile of
// sand with a peak, with one window at the top.
//
// Scaling factors
// ---------------
//
// Thumbnail position is mostly straightforward -- the main issue is
// computing an optimal scale for each window that fits the constraints,
// and doesn't make the thumbnail too small to see. There are two factors
// involved in thumbnail scale to make sure that these two goals are met:
// the window scale (calculated by _computeWindowScale) and the layout
// scale (calculated by computeSizeAndScale).
//
// The calculation logic becomes slightly more complicated because row
// and column spacing are not scaled, they're constant, so we can't
// simply generate a bunch of window positions and then scale it. In
// practice, it's not too bad -- we can simply try to fit the layout
// in the input area minus whatever spacing we have, and then add
// it back afterwards.
//
// The window scale is constant for the window's size regardless of the
// input area or the layout scale or rows or anything else, and right
// now just enlarges the window if it's too small. The fact that this
// factor is stable makes it easy to calculate, so there's no sense
// in not applying it in most calculations.
//
// The layout scale depends on the input area, the rows, etc, but is the
// same for the entire layout, rather than being per-window. After
// generating the rows of windows, we basically do some basic math to
// fit the full, unscaled layout to the input area, as described above.
//
// With these two factors combined, the final scale of each thumbnail is
// simply windowScale * layoutScale... almost.
//
// There's one additional constraint: the thumbnail scale must never be
// larger than WINDOW_PREVIEW_MAXIMUM_SCALE, which means that the inequality:
//
//   windowScale * layoutScale <= WINDOW_PREVIEW_MAXIMUM_SCALE
//
// must always be true. This is for each individual window -- while we
// could adjust layoutScale to make the largest thumbnail smaller than
// WINDOW_PREVIEW_MAXIMUM_SCALE, it would shrink windows which are already
// under the inequality. To solve this, we simply cheat: we simply keep
// each window's "cell" area to be the same, but we shrink the thumbnail
// and center it horizontally, and align it to the bottom vertically.

var LayoutStrategy = class {
    constructor(params) {
        params = Params.parse(params, {
            monitor: null,
            rowSpacing: 0,
            columnSpacing: 0,
        });

        if (!params.monitor)
            throw new Error(`No monitor param passed to ${this.constructor.name}`);

        this._monitor = params.monitor;
        this._rowSpacing = params.rowSpacing;
        this._columnSpacing = params.columnSpacing;
    }

    // Compute a strategy-specific overall layout given a list of WindowPreviews
    // @windows and the strategy-specific @layoutParams.
    //
    // Returns a strategy-specific layout object that is opaque to the user.
    computeLayout(_windows, _layoutParams) {
        throw new GObject.NotImplementedError(`computeLayout in ${this.constructor.name}`);
    }

    // Given @layout and @area, compute the overall scale of the layout and
    // space occupied by the layout.
    //
    // This method returns an array where the first element is the scale and
    // the second element is the space.
    //
    // This method must be called before calling computeWindowSlots(), as it
    // sets the fixed overall scale of the layout.
    computeScaleAndSpace(_layout, _area) {
        throw new GObject.NotImplementedError(`computeScaleAndSpace in ${this.constructor.name}`);
    }

    // Returns an array with final position and size information for each
    // window of the layout, given a bounding area that it will be inside of.
    computeWindowSlots(_layout, _area) {
        throw new GObject.NotImplementedError(`computeWindowSlots in ${this.constructor.name}`);
    }
};

var UnalignedLayoutStrategy = class extends LayoutStrategy {
    _newRow() {
        // Row properties:
        //
        // * x, y are the position of row, relative to area
        //
        // * width, height are the scaled versions of fullWidth, fullHeight
        //
        // * width also has the spacing in between windows. It's not in
        //   fullWidth, as the spacing is constant, whereas fullWidth is
        //   meant to be scaled
        //
        // * neither height/fullHeight have any sort of spacing or padding
        return {
            x: 0, y: 0,
            width: 0, height: 0,
            fullWidth: 0, fullHeight: 0,
            windows: [],
        };
    }

    // Computes and returns an individual scaling factor for @window,
    // to be applied in addition to the overall layout scale.
    _computeWindowScale(window) {
        // Since we align windows next to each other, the height of the
        // thumbnails is much more important to preserve than the width of
        // them, so two windows with equal height, but maybe differering
        // widths line up.
        let ratio = window.boundingBox.height / this._monitor.height;

        // The purpose of this manipulation here is to prevent windows
        // from getting too small. For something like a calculator window,
        // we need to bump up the size just a bit to make sure it looks
        // good. We'll use a multiplier of 1.5 for this.

        // Map from [0, 1] to [1.5, 1]
        return Util.lerp(1.5, 1, ratio);
    }

    _computeRowSizes(layout) {
        let { rows, scale } = layout;
        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            row.width = row.fullWidth * scale + (row.windows.length - 1) * this._columnSpacing;
            row.height = row.fullHeight * scale;
        }
    }

    _keepSameRow(row, window, width, idealRowWidth) {
        if (row.fullWidth + width <= idealRowWidth)
            return true;

        let oldRatio = row.fullWidth / idealRowWidth;
        let newRatio = (row.fullWidth + width) / idealRowWidth;

        if (Math.abs(1 - newRatio) < Math.abs(1 - oldRatio))
            return true;

        return false;
    }

    _sortRow(row) {
        // Sort windows horizontally to minimize travel distance.
        // This affects in what order the windows end up in a row.
        row.windows.sort((a, b) => a.windowCenter.x - b.windowCenter.x);
    }

    computeLayout(windows, layoutParams) {
        layoutParams = Params.parse(layoutParams, {
            numRows: 0,
        });

        if (layoutParams.numRows === 0)
            throw new Error(`${this.constructor.name}: No numRows given in layout params`);

        const numRows = layoutParams.numRows;

        let rows = [];
        let totalWidth = 0;
        for (let i = 0; i < windows.length; i++) {
            let window = windows[i];
            let s = this._computeWindowScale(window);
            totalWidth += window.boundingBox.width * s;
        }

        let idealRowWidth = totalWidth / numRows;

        // Sort windows vertically to minimize travel distance.
        // This affects what rows the windows get placed in.
        let sortedWindows = windows.slice();
        sortedWindows.sort((a, b) => a.windowCenter.y - b.windowCenter.y);

        let windowIdx = 0;
        for (let i = 0; i < numRows; i++) {
            let row = this._newRow();
            rows.push(row);

            for (; windowIdx < sortedWindows.length; windowIdx++) {
                let window = sortedWindows[windowIdx];
                let s = this._computeWindowScale(window);
                let width = window.boundingBox.width * s;
                let height = window.boundingBox.height * s;
                row.fullHeight = Math.max(row.fullHeight, height);

                // either new width is < idealWidth or new width is nearer from idealWidth then oldWidth
                if (this._keepSameRow(row, window, width, idealRowWidth) || (i === numRows - 1)) {
                    row.windows.push(window);
                    row.fullWidth += width;
                } else {
                    break;
                }
            }
        }

        let gridHeight = 0;
        let maxRow;
        for (let i = 0; i < numRows; i++) {
            let row = rows[i];
            this._sortRow(row);

            if (!maxRow || row.fullWidth > maxRow.fullWidth)
                maxRow = row;
            gridHeight += row.fullHeight;
        }

        return {
            numRows,
            rows,
            maxColumns: maxRow.windows.length,
            gridWidth: maxRow.fullWidth,
            gridHeight,
        };
    }

    computeScaleAndSpace(layout, area) {
        let hspacing = (layout.maxColumns - 1) * this._columnSpacing;
        let vspacing = (layout.numRows - 1) * this._rowSpacing;

        let spacedWidth = area.width - hspacing;
        let spacedHeight = area.height - vspacing;

        let horizontalScale = spacedWidth / layout.gridWidth;
        let verticalScale = spacedHeight / layout.gridHeight;

        // Thumbnails should be less than 70% of the original size
        let scale = Math.min(
            horizontalScale, verticalScale, WINDOW_PREVIEW_MAXIMUM_SCALE);

        let scaledLayoutWidth = layout.gridWidth * scale + hspacing;
        let scaledLayoutHeight = layout.gridHeight * scale + vspacing;
        let space = (scaledLayoutWidth * scaledLayoutHeight) / (area.width * area.height);

        layout.scale = scale;

        return [scale, space];
    }

    computeWindowSlots(layout, area) {
        this._computeRowSizes(layout);

        let { rows, scale } = layout;

        let slots = [];

        // Do this in three parts.
        let heightWithoutSpacing = 0;
        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];
            heightWithoutSpacing += row.height;
        }

        let verticalSpacing = (rows.length - 1) * this._rowSpacing;
        let additionalVerticalScale = Math.min(1, (area.height - verticalSpacing) / heightWithoutSpacing);

        // keep track how much smaller the grid becomes due to scaling
        // so it can be centered again
        let compensation = 0;
        let y = 0;

        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];

            // If this window layout row doesn't fit in the actual
            // geometry, then apply an additional scale to it.
            let horizontalSpacing = (row.windows.length - 1) * this._columnSpacing;
            let widthWithoutSpacing = row.width - horizontalSpacing;
            let additionalHorizontalScale = Math.min(1, (area.width - horizontalSpacing) / widthWithoutSpacing);

            if (additionalHorizontalScale < additionalVerticalScale) {
                row.additionalScale = additionalHorizontalScale;
                // Only consider the scaling in addition to the vertical scaling for centering.
                compensation += (additionalVerticalScale - additionalHorizontalScale) * row.height;
            } else {
                row.additionalScale = additionalVerticalScale;
                // No compensation when scaling vertically since centering based on a too large
                // height would undo what vertical scaling is trying to achieve.
            }

            row.x = area.x + (Math.max(area.width - (widthWithoutSpacing * row.additionalScale + horizontalSpacing), 0) / 2);
            row.y = area.y + (Math.max(area.height - (heightWithoutSpacing + verticalSpacing), 0) / 2) + y;
            y += row.height * row.additionalScale + this._rowSpacing;
        }

        compensation /= 2;

        for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            const rowY = row.y + compensation;
            const rowHeight = row.height * row.additionalScale;

            let x = row.x;
            for (let j = 0; j < row.windows.length; j++) {
                let window = row.windows[j];

                let s = scale * this._computeWindowScale(window) * row.additionalScale;
                let cellWidth = window.boundingBox.width * s;
                let cellHeight = window.boundingBox.height * s;

                s = Math.min(s, WINDOW_PREVIEW_MAXIMUM_SCALE);
                let cloneWidth = window.boundingBox.width * s;
                const cloneHeight = window.boundingBox.height * s;

                let cloneX = x + (cellWidth - cloneWidth) / 2;
                let cloneY;

                // If there's only one row, align windows vertically centered inside the row
                if (rows.length === 1)
                    cloneY = rowY + (rowHeight - cloneHeight) / 2;
                // If there are multiple rows, align windows to the bottom edge of the row
                else
                    cloneY = rowY + rowHeight - cellHeight;

                // Align with the pixel grid to prevent blurry windows at scale = 1
                cloneX = Math.floor(cloneX);
                cloneY = Math.floor(cloneY);

                slots.push([cloneX, cloneY, cloneWidth, cloneHeight, window]);
                x += cellWidth + this._columnSpacing;
            }
        }
        return slots;
    }
};

function animateAllocation(actor, box) {
    actor.save_easing_state();
    actor.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
    actor.set_easing_duration(200);

    actor.allocate(box);

    actor.restore_easing_state();

    return actor.get_transition('allocation');
}

var WorkspaceLayout = GObject.registerClass({
    Properties: {
        'spacing': GObject.ParamSpec.double(
            'spacing', 'Spacing', 'Spacing',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 20),
        'layout-frozen': GObject.ParamSpec.boolean(
            'layout-frozen', 'Layout frozen', 'Layout frozen',
            GObject.ParamFlags.READWRITE,
            false),
    },
}, class WorkspaceLayout extends Clutter.LayoutManager {
    _init(metaWorkspace, monitorIndex, overviewAdjustment) {
        super._init();

        this._spacing = 20;
        this._layoutFrozen = false;

        this._metaWorkspace = metaWorkspace;
        this._monitorIndex = monitorIndex;
        this._overviewAdjustment = overviewAdjustment;

        this._container = null;
        this._windows = new Map();
        this._sortedWindows = [];
        this._lastBox = null;
        this._windowSlots = [];
        this._layout = null;

        this._needsLayout = true;

        this._stateAdjustment = new St.Adjustment({
            value: 0,
            lower: 0,
            upper: 1,
        });

        this._stateAdjustment.connect('notify::value', () => {
            this._syncOpacities();
            this.syncOverlays();
            this.layout_changed();
        });

        this._workarea = null;
        this._workareasChangedId = 0;
    }

    _syncOpacity(actor, metaWindow) {
        if (!metaWindow.showing_on_its_workspace())
            actor.opacity = this._stateAdjustment.value * 255;
    }

    _syncOpacities() {
        this._windows.forEach(({ metaWindow }, actor) => {
            this._syncOpacity(actor, metaWindow);
        });
    }

    _isBetterScaleAndSpace(oldScale, oldSpace, scale, space) {
        let spacePower = (space - oldSpace) * LAYOUT_SPACE_WEIGHT;
        let scalePower = (scale - oldScale) * LAYOUT_SCALE_WEIGHT;

        if (scale > oldScale && space > oldSpace) {
            // Win win -- better scale and better space
            return true;
        } else if (scale > oldScale && space <= oldSpace) {
            // Keep new layout only if scale gain outweighs aspect space loss
            return scalePower > spacePower;
        } else if (scale <= oldScale && space > oldSpace) {
            // Keep new layout only if aspect space gain outweighs scale loss
            return spacePower > scalePower;
        } else {
            // Lose -- worse scale and space
            return false;
        }
    }

    _adjustSpacingAndPadding(rowSpacing, colSpacing, containerBox) {
        if (this._sortedWindows.length === 0)
            return [rowSpacing, colSpacing, containerBox];

        // All of the overlays have the same chrome sizes,
        // so just pick the first one.
        const window = this._sortedWindows[0];

        const [topOversize, bottomOversize] = window.chromeHeights();
        const [leftOversize, rightOversize] = window.chromeWidths();

        const oversize =
            Math.max(topOversize, bottomOversize, leftOversize, rightOversize);

        if (rowSpacing !== null)
            rowSpacing += oversize;
        if (colSpacing !== null)
            colSpacing += oversize;

        if (containerBox) {
            const monitor = Main.layoutManager.monitors[this._monitorIndex];

            const bottomPoint = new Graphene.Point3D({ y: containerBox.y2 });
            const transformedBottomPoint =
                this._container.apply_transform_to_point(bottomPoint);
            const bottomFreeSpace =
                (monitor.y + monitor.height) - transformedBottomPoint.y;

            const [, bottomOverlap] = window.overlapHeights();

            if ((bottomOverlap + oversize) > bottomFreeSpace)
                containerBox.y2 -= (bottomOverlap + oversize) - bottomFreeSpace;
        }

        return [rowSpacing, colSpacing, containerBox];
    }

    _createBestLayout(area) {
        const [rowSpacing, columnSpacing] =
            this._adjustSpacingAndPadding(this._spacing, this._spacing, null);

        // We look for the largest scale that allows us to fit the
        // largest row/tallest column on the workspace.
        this._layoutStrategy = new UnalignedLayoutStrategy({
            monitor: Main.layoutManager.monitors[this._monitorIndex],
            rowSpacing,
            columnSpacing,
        });

        let lastLayout = null;
        let lastNumColumns = -1;
        let lastScale = 0;
        let lastSpace = 0;

        for (let numRows = 1; ; numRows++) {
            const numColumns = Math.ceil(this._sortedWindows.length / numRows);

            // If adding a new row does not change column count just stop
            // (for instance: 9 windows, with 3 rows -> 3 columns, 4 rows ->
            // 3 columns as well => just use 3 rows then)
            if (numColumns === lastNumColumns)
                break;

            const layout = this._layoutStrategy.computeLayout(this._sortedWindows, {
                numRows,
            });

            const [scale, space] = this._layoutStrategy.computeScaleAndSpace(layout, area);

            if (lastLayout && !this._isBetterScaleAndSpace(lastScale, lastSpace, scale, space))
                break;

            lastLayout = layout;
            lastNumColumns = numColumns;
            lastScale = scale;
            lastSpace = space;
        }

        return lastLayout;
    }

    _getWindowSlots(containerBox) {
        [, , containerBox] =
            this._adjustSpacingAndPadding(null, null, containerBox);

        const availArea = {
            x: parseInt(containerBox.x1),
            y: parseInt(containerBox.y1),
            width: parseInt(containerBox.get_width()),
            height: parseInt(containerBox.get_height()),
        };

        return this._layoutStrategy.computeWindowSlots(this._layout, availArea);
    }

    _getAdjustedWorkarea(container) {
        const workarea = this._workarea.copy();

        if (container instanceof St.Widget) {
            const themeNode = container.get_theme_node();
            workarea.width -= themeNode.get_horizontal_padding();
            workarea.height -= themeNode.get_vertical_padding();
        }

        return workarea;
    }

    _syncWorkareaTracking() {
        if (this._container) {
            if (this._workAreaChangedId)
                return;
            this._workarea = Main.layoutManager.getWorkAreaForMonitor(this._monitorIndex);
            this._workareasChangedId =
                global.display.connect('workareas-changed', () => {
                    this._workarea = Main.layoutManager.getWorkAreaForMonitor(this._monitorIndex);
                    this.layout_changed();
                });
        } else if (this._workareasChangedId) {
            global.display.disconnect(this._workareasChangedId);
            this._workareasChangedId = 0;
        }
    }

    vfunc_set_container(container) {
        this._container = container;
        this._syncWorkareaTracking();
        this._stateAdjustment.actor = container;
    }

    vfunc_get_preferred_width(container, forHeight) {
        const workarea = this._getAdjustedWorkarea(container);
        if (forHeight === -1)
            return [0, workarea.width];

        const workAreaAspectRatio = workarea.width / workarea.height;
        const widthPreservingAspectRatio = forHeight * workAreaAspectRatio;

        return [0, widthPreservingAspectRatio];
    }

    vfunc_get_preferred_height(container, forWidth) {
        const workarea = this._getAdjustedWorkarea(container);
        if (forWidth === -1)
            return [0, workarea.height];

        const workAreaAspectRatio = workarea.width / workarea.height;
        const heightPreservingAspectRatio = forWidth / workAreaAspectRatio;

        return [0, heightPreservingAspectRatio];
    }

    vfunc_allocate(container, box) {
        const containerBox = container.allocation;
        const [containerWidth, containerHeight] = containerBox.get_size();
        const containerAllocationChanged =
            this._lastBox === null || !this._lastBox.equal(containerBox);

        // If the containers size changed, we can no longer keep around
        // the old windowSlots, so we must unfreeze the layout.
        //
        // However, if the overview animation is in progress, don't unfreeze
        // the layout. This is needed to prevent windows "snapping" to their
        // new positions during the overview closing animation when the
        // allocation subtly expands every frame.
        if (this._layoutFrozen && containerAllocationChanged && !Main.overview.animationInProgress) {
            this._layoutFrozen = false;
            this.notify('layout-frozen');
        }

        const { ControlsState } = OverviewControls;
        const { currentState } =
            this._overviewAdjustment.getStateTransitionParams();
        const inSessionTransition = currentState <= ControlsState.WINDOW_PICKER;

        const window = this._sortedWindows[0];

        if (inSessionTransition || !window) {
            container.remove_clip();
        } else {
            const [, bottomOversize] = window.chromeHeights();
            const [containerX, containerY] = containerBox.get_origin();

            const extraHeightProgress =
                currentState - OverviewControls.ControlsState.WINDOW_PICKER;

            const extraClipHeight = bottomOversize * (1 - extraHeightProgress);

            container.set_clip(containerX, containerY,
                containerWidth, containerHeight + extraClipHeight);
        }

        let layoutChanged = false;
        if (!this._layoutFrozen || !this._lastBox) {
            if (this._needsLayout) {
                this._layout = this._createBestLayout(this._workarea);
                this._needsLayout = false;
                layoutChanged = true;
            }

            if (layoutChanged || containerAllocationChanged) {
                this._windowSlotsBox = box.copy();
                this._windowSlots = this._getWindowSlots(this._windowSlotsBox);
            }
        }

        const slotsScale = box.get_width() / this._windowSlotsBox.get_width();
        const workareaX = this._workarea.x;
        const workareaY = this._workarea.y;
        const workareaWidth = this._workarea.width;
        const stateAdjustementValue = this._stateAdjustment.value;

        const allocationScale = containerWidth / workareaWidth;

        const childBox = new Clutter.ActorBox();

        const nSlots = this._windowSlots.length;
        for (let i = 0; i < nSlots; i++) {
            let [x, y, width, height, child] = this._windowSlots[i];
            if (!child.visible)
                continue;

            x *= slotsScale;
            y *= slotsScale;
            width *= slotsScale;
            height *= slotsScale;

            const windowInfo = this._windows.get(child);

            let workspaceBoxX, workspaceBoxY;
            let workspaceBoxWidth, workspaceBoxHeight;

            if (windowInfo.metaWindow.showing_on_its_workspace()) {
                workspaceBoxX = (child.boundingBox.x - workareaX) * allocationScale;
                workspaceBoxY = (child.boundingBox.y - workareaY) * allocationScale;
                workspaceBoxWidth = child.boundingBox.width * allocationScale;
                workspaceBoxHeight = child.boundingBox.height * allocationScale;
            } else {
                workspaceBoxX = workareaX * allocationScale;
                workspaceBoxY = workareaY * allocationScale;
                workspaceBoxWidth = 0;
                workspaceBoxHeight = 0;
            }

            // Don't allow the scaled floating size to drop below
            // the target layout size.
            // We only want to apply this when the scaled floating size is
            // actually larger than the target layout size, that is while
            // animating between the session and the window picker.
            if (inSessionTransition) {
                workspaceBoxWidth = Math.max(workspaceBoxWidth, width);
                workspaceBoxHeight = Math.max(workspaceBoxHeight, height);
            }

            x = Util.lerp(workspaceBoxX, x, stateAdjustementValue);
            y = Util.lerp(workspaceBoxY, y, stateAdjustementValue);
            width = Util.lerp(workspaceBoxWidth, width, stateAdjustementValue);
            height = Util.lerp(workspaceBoxHeight, height, stateAdjustementValue);

            childBox.set_origin(x, y);
            childBox.set_size(width, height);

            if (windowInfo.currentTransition) {
                windowInfo.currentTransition.get_interval().set_final(childBox);

                // The timeline of the transition might not have been updated
                // before this allocation cycle, so make sure the child
                // still updates needs_allocation to FALSE.
                // Unfortunately, this relies on the fast paths in
                // clutter_actor_allocate(), otherwise we'd start a new
                // transition on the child, replacing the current one.
                child.allocate(child.allocation);
                continue;
            }

            // We want layout changes (ie. larger changes to the layout like
            // reshuffling the window order) to be animated, but small changes
            // like changes to the container size to happen immediately (for
            // example if the container height is being animated, we want to
            // avoid animating the children allocations to make sure they
            // don't "lag behind" the other animation).
            if (layoutChanged && !Main.overview.animationInProgress) {
                const transition = animateAllocation(child, childBox);
                if (transition) {
                    windowInfo.currentTransition = transition;
                    windowInfo.currentTransition.connect('stopped', () => {
                        windowInfo.currentTransition = null;
                    });
                }
            } else {
                child.allocate(childBox);
            }
        }

        this._lastBox = containerBox.copy();
    }

    _syncOverlay(preview) {
        const active = this._metaWorkspace?.active ?? true;
        preview.overlayEnabled = active && this._stateAdjustment.value === 1;
    }

    /**
     * syncOverlays:
     *
     * Synchronizes the overlay state of all window previews.
     */
    syncOverlays() {
        [...this._windows.keys()].forEach(preview => this._syncOverlay(preview));
    }

    /**
     * addWindow:
     * @param {WindowPreview} window: the window to add
     * @param {Meta.Window} metaWindow: the MetaWindow of the window
     *
     * Adds @window to the workspace, it will be shown immediately if
     * the layout isn't frozen using the layout-frozen property.
     *
     * If @window is already part of the workspace, nothing will happen.
     */
    addWindow(window, metaWindow) {
        if (this._windows.has(window))
            return;

        this._windows.set(window, {
            metaWindow,
            sizeChangedId: metaWindow.connect('size-changed', () => {
                this._needsLayout = true;
                this.layout_changed();
            }),
            destroyId: window.connect('destroy', () =>
                this.removeWindow(window)),
            currentTransition: null,
        });

        this._sortedWindows.push(window);
        this._sortedWindows.sort((a, b) => {
            const winA = this._windows.get(a).metaWindow;
            const winB = this._windows.get(b).metaWindow;

            return winA.get_stable_sequence() - winB.get_stable_sequence();
        });

        this._syncOpacity(window, metaWindow);
        this._syncOverlay(window);
        this._container.add_child(window);

        this._needsLayout = true;
        this.layout_changed();
    }

    /**
     * removeWindow:
     * @param {WindowPreview} window: the window to remove
     *
     * Removes @window from the workspace if @window is a part of the
     * workspace. If the layout-frozen property is set to true, the
     * window will still be visible until the property is set to false.
     */
    removeWindow(window) {
        const windowInfo = this._windows.get(window);
        if (!windowInfo)
            return;

        windowInfo.metaWindow.disconnect(windowInfo.sizeChangedId);
        window.disconnect(windowInfo.destroyId);
        if (windowInfo.currentTransition)
            window.remove_transition('allocation');

        this._windows.delete(window);
        this._sortedWindows.splice(this._sortedWindows.indexOf(window), 1);

        // The layout might be frozen and we might not update the windowSlots
        // on the next allocation, so remove the slot now already
        const index = this._windowSlots.findIndex(s => s[4] === window);
        if (index !== -1)
            this._windowSlots.splice(index, 1);

        // The window might have been reparented by DND
        if (window.get_parent() === this._container)
            this._container.remove_child(window);

        this._needsLayout = true;
        this.layout_changed();
    }

    syncStacking(stackIndices) {
        const windows = [...this._windows.keys()];
        windows.sort((a, b) => {
            const seqA = this._windows.get(a).metaWindow.get_stable_sequence();
            const seqB = this._windows.get(b).metaWindow.get_stable_sequence();

            return stackIndices[seqA] - stackIndices[seqB];
        });

        let lastWindow = null;
        for (const window of windows) {
            window.setStackAbove(lastWindow);
            lastWindow = window;
        }

        this._needsLayout = true;
        this.layout_changed();
    }

    /**
     * getFocusChain:
     *
     * Gets the focus chain of the workspace. This function will return
     * an empty array if the floating window layout is used.
     *
     * @returns {Array} an array of {Clutter.Actor}s
     */
    getFocusChain() {
        if (this._stateAdjustment.value === 0)
            return [];

        // The fifth element in the slot array is the WindowPreview
        return this._windowSlots.map(s => s[4]);
    }

    /**
     * An StAdjustment for controlling and transitioning between
     * the alignment of windows using the layout strategy and the
     * floating window layout.
     *
     * A value of 0 of the adjustment completely uses the floating
     * window layout while a value of 1 completely aligns windows using
     * the layout strategy.
     *
     * @type {St.Adjustment}
     */
    get stateAdjustment() {
        return this._stateAdjustment;
    }

    get spacing() {
        return this._spacing;
    }

    set spacing(s) {
        if (this._spacing === s)
            return;

        this._spacing = s;

        this._needsLayout = true;
        this.notify('spacing');
        this.layout_changed();
    }

    get layoutFrozen() {
        return this._layoutFrozen;
    }

    set layoutFrozen(f) {
        if (this._layoutFrozen === f)
            return;

        this._layoutFrozen = f;

        this.notify('layout-frozen');
        if (!this._layoutFrozen)
            this.layout_changed();
    }
});

var WorkspaceBackground = GObject.registerClass(
class WorkspaceBackground extends Shell.WorkspaceBackground {
    _init(monitorIndex, stateAdjustment) {
        super._init({
            style_class: 'workspace-background',
            x_expand: true,
            y_expand: true,
            monitor_index: monitorIndex,
        });

        this._monitorIndex = monitorIndex;
        this._workarea = Main.layoutManager.getWorkAreaForMonitor(monitorIndex);

        this._stateAdjustment = stateAdjustment;
        this._stateAdjustment.connectObject('notify::value', () => {
            this._updateBorderRadius();
            this.queue_relayout();
        }, this);
        this._stateAdjustment.bind_property(
            'value', this, 'state-adjustment-value',
            GObject.BindingFlags.SYNC_CREATE
        );

        this._bin = new Clutter.Actor({
            layout_manager: new Clutter.BinLayout(),
            clip_to_allocation: true,
        });

        this._backgroundGroup = new Meta.BackgroundGroup({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        this._bin.add_child(this._backgroundGroup);
        this.add_child(this._bin);

        this._bgManager = new Background.BackgroundManager({
            container: this._backgroundGroup,
            monitorIndex: this._monitorIndex,
            controlPosition: false,
            useContentSize: false,
        });

        global.display.connectObject('workareas-changed', () => {
            this._workarea = Main.layoutManager.getWorkAreaForMonitor(monitorIndex);
            this._updateRoundedClipBounds();
            this.queue_relayout();
        }, this);
        this._updateRoundedClipBounds();

        this._updateBorderRadius();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _updateBorderRadius() {
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        const cornerRadius = scaleFactor * BACKGROUND_CORNER_RADIUS_PIXELS;

        const backgroundContent = this._bgManager.backgroundActor.content;
        backgroundContent.rounded_clip_radius =
            Util.lerp(0, cornerRadius, this._stateAdjustment.value);
    }

    _updateRoundedClipBounds() {
        const monitor = Main.layoutManager.monitors[this._monitorIndex];

        const rect = new Graphene.Rect();
        rect.origin.x = this._workarea.x - monitor.x;
        rect.origin.y = this._workarea.y - monitor.y;
        rect.size.width = this._workarea.width;
        rect.size.height = this._workarea.height;

        this._bgManager.backgroundActor.content.set_rounded_clip_bounds(rect);
    }

    _onDestroy() {
        if (this._bgManager) {
            this._bgManager.destroy();
            this._bgManager = null;
        }
    }
});

/**
 * @metaWorkspace: a #Meta.Workspace, or null
 */
var Workspace = GObject.registerClass(
class Workspace extends St.Widget {
    _init(metaWorkspace, monitorIndex, overviewAdjustment) {
        super._init({
            style_class: 'window-picker',
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
            layout_manager: new Clutter.BinLayout(),
        });

        const layoutManager = new WorkspaceLayout(metaWorkspace, monitorIndex,
            overviewAdjustment);

        // Background
        this._background =
            new WorkspaceBackground(monitorIndex, layoutManager.stateAdjustment);
        this.add_child(this._background);

        // Window previews
        this._container = new Clutter.Actor({
            reactive: true,
            x_expand: true,
            y_expand: true,
        });
        this._container.layout_manager = layoutManager;
        this.add_child(this._container);

        this.metaWorkspace = metaWorkspace;

        this._overviewAdjustment = overviewAdjustment;

        this.monitorIndex = monitorIndex;
        this._monitor = Main.layoutManager.monitors[this.monitorIndex];

        if (monitorIndex != Main.layoutManager.primaryIndex)
            this.add_style_class_name('external-monitor');

        const clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', action => {
            // Switch to the workspace when not the active one, leave the
            // overview otherwise.
            if (action.get_button() === 1 || action.get_button() === 0) {
                const leaveOverview = this._shouldLeaveOverview();

                this.metaWorkspace?.activate(global.get_current_time());
                if (leaveOverview)
                    Main.overview.hide();
            }
        });
        this.bind_property('mapped', clickAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);
        this._container.add_action(clickAction);

        this.connect('style-changed', this._onStyleChanged.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));

        this._skipTaskbarSignals = new Map();
        const windows = global.get_window_actors().map(a => a.meta_window)
            .filter(this._isMyWindow, this);

        // Create clones for windows that should be
        // visible in the Overview
        this._windows = [];
        for (let i = 0; i < windows.length; i++) {
            if (this._isOverviewWindow(windows[i]))
                this._addWindowClone(windows[i]);
        }

        // Track window changes, but let the window tracker process them first
        this.metaWorkspace?.connectObject(
            'window-added', this._windowAdded.bind(this), GObject.ConnectFlags.AFTER,
            'window-removed', this._windowRemoved.bind(this), GObject.ConnectFlags.AFTER,
            'notify::active', () => layoutManager.syncOverlays(), this);
        global.display.connectObject(
            'window-entered-monitor', this._windowEnteredMonitor.bind(this), GObject.ConnectFlags.AFTER,
            'window-left-monitor', this._windowLeftMonitor.bind(this), GObject.ConnectFlags.AFTER,
            this);
        this._layoutFrozenId = 0;

        // DND requires this to be set
        this._delegate = this;
    }

    _shouldLeaveOverview() {
        if (!this.metaWorkspace || this.metaWorkspace.active)
            return true;

        const overviewState = this._overviewAdjustment.value;
        return overviewState > OverviewControls.ControlsState.WINDOW_PICKER;
    }

    vfunc_get_focus_chain() {
        return this._container.layout_manager.getFocusChain();
    }

    _lookupIndex(metaWindow) {
        return this._windows.findIndex(w => w.metaWindow == metaWindow);
    }

    containsMetaWindow(metaWindow) {
        return this._lookupIndex(metaWindow) >= 0;
    }

    isEmpty() {
        return this._windows.length == 0;
    }

    syncStacking(stackIndices) {
        this._container.layout_manager.syncStacking(stackIndices);
    }

    _doRemoveWindow(metaWin) {
        let clone = this._removeWindowClone(metaWin);

        if (!clone)
            return;

        clone.destroy();

        // We need to reposition the windows; to avoid shuffling windows
        // around while the user is interacting with the workspace, we delay
        // the positioning until the pointer remains still for at least 750 ms
        // or is moved outside the workspace
        this._container.layout_manager.layout_frozen = true;

        if (this._layoutFrozenId > 0) {
            GLib.source_remove(this._layoutFrozenId);
            this._layoutFrozenId = 0;
        }

        let [oldX, oldY] = global.get_pointer();

        this._layoutFrozenId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            WINDOW_REPOSITIONING_DELAY,
            () => {
                const [newX, newY] = global.get_pointer();
                const pointerHasMoved = oldX !== newX || oldY !== newY;
                const actorUnderPointer = global.stage.get_actor_at_pos(
                    Clutter.PickMode.REACTIVE, newX, newY);

                if ((pointerHasMoved && this.contains(actorUnderPointer)) ||
                    this._windows.some(w => w.contains(actorUnderPointer))) {
                    oldX = newX;
                    oldY = newY;
                    return GLib.SOURCE_CONTINUE;
                }

                this._container.layout_manager.layout_frozen = false;
                this._layoutFrozenId = 0;
                return GLib.SOURCE_REMOVE;
            });

        GLib.Source.set_name_by_id(this._layoutFrozenId,
            '[gnome-shell] this._layoutFrozenId');
    }

    _doAddWindow(metaWin) {
        let win = metaWin.get_compositor_private();

        if (!win) {
            // Newly-created windows are added to a workspace before
            // the compositor finds out about them...
            let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                if (metaWin.get_compositor_private() &&
                    metaWin.get_workspace() == this.metaWorkspace)
                    this._doAddWindow(metaWin);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(id, '[gnome-shell] this._doAddWindow');
            return;
        }

        // We might have the window in our list already if it was on all workspaces and
        // now was moved to this workspace
        if (this._lookupIndex(metaWin) != -1)
            return;

        if (!this._isMyWindow(metaWin))
            return;

        this._skipTaskbarSignals.set(metaWin,
            metaWin.connect('notify::skip-taskbar', () => {
                if (metaWin.skip_taskbar)
                    this._doRemoveWindow(metaWin);
                else
                    this._doAddWindow(metaWin);
            }));

        if (!this._isOverviewWindow(metaWin)) {
            if (metaWin.get_transient_for() == null)
                return;

            // Let the top-most ancestor handle all transients
            let parent = metaWin.find_root_ancestor();
            let clone = this._windows.find(c => c.metaWindow == parent);

            // If no clone was found, the parent hasn't been created yet
            // and will take care of the dialog when added
            if (clone)
                clone.addDialog(metaWin);

            return;
        }

        const clone = this._addWindowClone(metaWin);

        clone.set_pivot_point(0.5, 0.5);
        clone.scale_x = 0;
        clone.scale_y = 0;
        clone.ease({
            scale_x: 1,
            scale_y: 1,
            duration: 250,
            onStopped: () => clone.set_pivot_point(0, 0),
        });

        if (this._layoutFrozenId > 0) {
            // If a window was closed before, unfreeze the layout to ensure
            // the new window is immediately shown
            this._container.layout_manager.layout_frozen = false;

            GLib.source_remove(this._layoutFrozenId);
            this._layoutFrozenId = 0;
        }
    }

    _windowAdded(metaWorkspace, metaWin) {
        if (!Main.overview.closing)
            this._doAddWindow(metaWin);
    }

    _windowRemoved(metaWorkspace, metaWin) {
        this._doRemoveWindow(metaWin);
    }

    _windowEnteredMonitor(metaDisplay, monitorIndex, metaWin) {
        if (monitorIndex === this.monitorIndex && !Main.overview.closing)
            this._doAddWindow(metaWin);
    }

    _windowLeftMonitor(metaDisplay, monitorIndex, metaWin) {
        if (monitorIndex == this.monitorIndex)
            this._doRemoveWindow(metaWin);
    }

    // check for maximized windows on the workspace
    hasMaximizedWindows() {
        for (let i = 0; i < this._windows.length; i++) {
            let metaWindow = this._windows[i].metaWindow;
            if (metaWindow.showing_on_its_workspace() &&
                metaWindow.maximized_horizontally &&
                metaWindow.maximized_vertically)
                return true;
        }
        return false;
    }

    _clearSkipTaskbarSignals() {
        for (const [metaWin, id] of this._skipTaskbarSignals)
            metaWin.disconnect(id);
        this._skipTaskbarSignals.clear();
    }

    prepareToLeaveOverview() {
        this._clearSkipTaskbarSignals();

        for (let i = 0; i < this._windows.length; i++)
            this._windows[i].remove_all_transitions();

        if (this._layoutFrozenId > 0) {
            GLib.source_remove(this._layoutFrozenId);
            this._layoutFrozenId = 0;
        }

        this._container.layout_manager.layout_frozen = true;
        Main.overview.connectObject(
            'hidden', this._doneLeavingOverview.bind(this), this);
    }

    _onDestroy() {
        this._clearSkipTaskbarSignals();

        if (this._layoutFrozenId > 0) {
            GLib.source_remove(this._layoutFrozenId);
            this._layoutFrozenId = 0;
        }

        this._windows = [];
    }

    _doneLeavingOverview() {
        this._container.layout_manager.layout_frozen = false;
    }

    _doneShowingOverview() {
        this._container.layout_manager.layout_frozen = false;
    }

    _isMyWindow(window) {
        const isOnWorkspace = this.metaWorkspace === null ||
            window.located_on_workspace(this.metaWorkspace);
        const isOnMonitor = window.get_monitor() === this.monitorIndex;

        return isOnWorkspace && isOnMonitor;
    }

    _isOverviewWindow(window) {
        return !window.skip_taskbar;
    }

    // Create a clone of a (non-desktop) window and add it to the window list
    _addWindowClone(metaWindow) {
        let clone = new WindowPreview(metaWindow, this, this._overviewAdjustment);

        clone.connect('selected',
                      this._onCloneSelected.bind(this));
        clone.connect('drag-begin', () => {
            Main.overview.beginWindowDrag(metaWindow);
        });
        clone.connect('drag-cancelled', () => {
            Main.overview.cancelledWindowDrag(metaWindow);
        });
        clone.connect('drag-end', () => {
            Main.overview.endWindowDrag(metaWindow);
        });
        clone.connect('show-chrome', () => {
            let focus = global.stage.key_focus;
            if (focus == null || this.contains(focus))
                clone.grab_key_focus();

            this._windows.forEach(c => {
                if (c !== clone)
                    c.hideOverlay(true);
            });
        });
        clone.connect('destroy', () => {
            this._doRemoveWindow(metaWindow);
        });

        this._container.layout_manager.addWindow(clone, metaWindow);

        if (this._windows.length == 0)
            clone.setStackAbove(null);
        else
            clone.setStackAbove(this._windows[this._windows.length - 1]);

        this._windows.push(clone);

        return clone;
    }

    _removeWindowClone(metaWin) {
        // find the position of the window in our list
        let index = this._lookupIndex(metaWin);

        if (index == -1)
            return null;

        this._container.layout_manager.removeWindow(this._windows[index]);

        return this._windows.splice(index, 1).pop();
    }

    _onStyleChanged() {
        const themeNode = this.get_theme_node();
        this._container.layout_manager.spacing = themeNode.get_length('spacing');
    }

    _onCloneSelected(clone, time) {
        const wsIndex = this.metaWorkspace?.index();

        if (this._shouldLeaveOverview())
            Main.activateWindow(clone.metaWindow, time, wsIndex);
        else
            this.metaWorkspace?.activate(time);
    }

    // Draggable target interface
    handleDragOver(source, _actor, _x, _y, _time) {
        if (source.metaWindow && !this._isMyWindow(source.metaWindow))
            return DND.DragMotionResult.MOVE_DROP;
        if (source.app && source.app.can_open_new_window())
            return DND.DragMotionResult.COPY_DROP;
        if (!source.app && source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;

        return DND.DragMotionResult.CONTINUE;
    }

    acceptDrop(source, actor, x, y, time) {
        let workspaceManager = global.workspace_manager;
        let workspaceIndex = this.metaWorkspace
            ? this.metaWorkspace.index()
            : workspaceManager.get_active_workspace_index();

        if (source.metaWindow) {
            const window = source.metaWindow;
            if (this._isMyWindow(window))
                return false;

            Main.moveWindowToMonitorAndWorkspace(window,
                this.monitorIndex, workspaceIndex);
            return true;
        } else if (source.app && source.app.can_open_new_window()) {
            if (source.animateLaunchAtPos)
                source.animateLaunchAtPos(actor.x, actor.y);

            source.app.open_new_window(workspaceIndex);
            return true;
        } else if (!source.app && source.shellWorkspaceLaunch) {
            // While unused in our own drag sources, shellWorkspaceLaunch allows
            // extensions to define custom actions for their drag sources.
            source.shellWorkspaceLaunch({
                workspace: workspaceIndex,
                timestamp: time,
            });
            return true;
        }

        return false;
    }

    get stateAdjustment() {
        return this._container.layout_manager.stateAdjustment;
    }
});
