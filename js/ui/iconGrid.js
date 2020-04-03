// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BaseIcon, IconGrid, PaginatedIconGrid, ANIMATION_MAX_DELAY_OUT_FOR_ITEM */

const { Clutter, GObject, Graphene, Meta, Pango, St } = imports.gi;

const Params = imports.misc.params;
const Main = imports.ui.main;

var ICON_SIZE = 64;

var ANIMATION_TIME_IN = 350;
var ANIMATION_TIME_OUT = 1 / 2 * ANIMATION_TIME_IN;
var ANIMATION_MAX_DELAY_FOR_ITEM = 2 / 3 * ANIMATION_TIME_IN;
var ANIMATION_BASE_DELAY_FOR_ITEM = 1 / 4 * ANIMATION_MAX_DELAY_FOR_ITEM;
var ANIMATION_MAX_DELAY_OUT_FOR_ITEM = 2 / 3 * ANIMATION_TIME_OUT;

var ANIMATION_BOUNCE_ICON_SCALE = 1.1;

var AnimationDirection = {
    IN: 0,
    OUT: 1,
};

var APPICON_ANIMATION_OUT_SCALE = 3;
var APPICON_ANIMATION_OUT_TIME = 250;

const ICON_POSITION_DELAY = 25;

var LEFT_DIVIDER_LEEWAY = 30;
var RIGHT_DIVIDER_LEEWAY = 30;

const NUDGE_ANIMATION_TYPE = Clutter.AnimationMode.EASE_OUT_ELASTIC;
const NUDGE_DURATION = 800;

const NUDGE_RETURN_ANIMATION_TYPE = Clutter.AnimationMode.EASE_OUT_QUINT;
const NUDGE_RETURN_DURATION = 300;

const NUDGE_FACTOR = 0.33;

var DragLocation = {
    DEFAULT: 0,
    ON_ICON: 1,
    START_EDGE: 2,
    END_EDGE: 3,
    EMPTY_AREA: 4,
}

var BaseIcon = GObject.registerClass(
class BaseIcon extends St.Bin {
    _init(label, params) {
        params = Params.parse(params, { createIcon: null,
                                        setSizeManually: false,
                                        showLabel: true });

        let styleClass = 'overview-icon';
        if (params.showLabel)
            styleClass += ' overview-icon-with-label';

        super._init({ style_class: styleClass });

        this.connect('destroy', this._onDestroy.bind(this));

        this._box = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            y_expand: true,
        });
        this.set_child(this._box);

        this.iconSize = ICON_SIZE;
        this._iconBin = new St.Bin({ x_align: Clutter.ActorAlign.CENTER });

        this._box.add_actor(this._iconBin);

        if (params.showLabel) {
            this.label = new St.Label({
                text: label,
                style_class: 'overview-icon-label',
            });
            this.label.clutter_text.set({
                x_align: Clutter.ActorAlign.CENTER,
                y_align: Clutter.ActorAlign.CENTER,
                line_wrap: true,
                ellipsize: Pango.EllipsizeMode.END,
            });
            this._box.add_actor(this.label);
        } else {
            this.label = null;
        }

        if (params.createIcon)
            this.createIcon = params.createIcon;
        this._setSizeManually = params.setSizeManually;

        this.icon = null;

        let cache = St.TextureCache.get_default();
        this._iconThemeChangedId = cache.connect('icon-theme-changed', this._onIconThemeChanged.bind(this));
    }

    vfunc_get_preferred_width(_forHeight) {
        // Return the actual height to keep the squared aspect
        return this.get_preferred_height(-1);
    }

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon(_size) {
        throw new GObject.NotImplementedError(`createIcon in ${this.constructor.name}`);
    }

    setIconSize(size) {
        if (!this._setSizeManually)
            throw new Error('setSizeManually has to be set to use setIconsize');

        if (size == this.iconSize)
            return;

        this._createIconTexture(size);
    }

    _createIconTexture(size) {
        if (this.icon)
            this.icon.destroy();
        this.iconSize = size;
        this.icon = this.createIcon(this.iconSize);

        this._iconBin.child = this.icon;
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();
        let node = this.get_theme_node();

        let size;
        if (this._setSizeManually) {
            size = this.iconSize;
        } else {
            const { scaleFactor } =
                St.ThemeContext.get_for_stage(global.stage);

            let [found, len] = node.lookup_length('icon-size', false);
            size = found ? len / scaleFactor : ICON_SIZE;
        }

        if (this.iconSize == size && this._iconBin.child)
            return;

        this._createIconTexture(size);
    }

    _onDestroy() {
        if (this._iconThemeChangedId > 0) {
            let cache = St.TextureCache.get_default();
            cache.disconnect(this._iconThemeChangedId);
            this._iconThemeChangedId = 0;
        }
    }

    _onIconThemeChanged() {
        this._createIconTexture(this.iconSize);
    }

    animateZoomOut() {
        // Animate only the child instead of the entire actor, so the
        // styles like hover and running are not applied while
        // animating.
        zoomOutActor(this.child);
    }

    animateZoomOutAtPos(x, y) {
        zoomOutActorAtPos(this.child, x, y);
    }

    update() {
        this._createIconTexture(this.iconSize);
    }
});

function clamp(value, min, max) {
    return Math.max(Math.min(value, max), min);
}

function zoomOutActor(actor) {
    let [x, y] = actor.get_transformed_position();
    zoomOutActorAtPos(actor, x, y);
}

function zoomOutActorAtPos(actor, x, y) {
    let actorClone = new Clutter.Clone({ source: actor,
                                         reactive: false });
    let [width, height] = actor.get_transformed_size();

    actorClone.set_size(width, height);
    actorClone.set_position(x, y);
    actorClone.opacity = 255;
    actorClone.set_pivot_point(0.5, 0.5);

    Main.uiGroup.add_actor(actorClone);

    // Avoid monitor edges to not zoom outside the current monitor
    let monitor = Main.layoutManager.findMonitorForActor(actor);
    let scaledWidth = width * APPICON_ANIMATION_OUT_SCALE;
    let scaledHeight = height * APPICON_ANIMATION_OUT_SCALE;
    let scaledX = x - (scaledWidth - width) / 2;
    let scaledY = y - (scaledHeight - height) / 2;
    let containedX = clamp(scaledX, monitor.x, monitor.x + monitor.width - scaledWidth);
    let containedY = clamp(scaledY, monitor.y, monitor.y + monitor.height - scaledHeight);

    actorClone.ease({
        scale_x: APPICON_ANIMATION_OUT_SCALE,
        scale_y: APPICON_ANIMATION_OUT_SCALE,
        translation_x: containedX - scaledX,
        translation_y: containedY - scaledY,
        opacity: 0,
        duration: APPICON_ANIMATION_OUT_TIME,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => actorClone.destroy(),
    });
}

function animateIconPosition(icon, box, flags, nChangedIcons) {
    if (!icon.has_allocation() || icon.allocation.equal(box) || icon.opacity === 0) {
        icon.allocate(box, flags);
        return false;
    }

    icon.save_easing_state();
    icon.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
    icon.set_easing_delay(nChangedIcons * ICON_POSITION_DELAY);

    icon.allocate(box, flags);

    icon.restore_easing_state();

    return true;
}

var IconGrid = GObject.registerClass({
    Signals: { 'animation-done': {},
               'child-focused': { param_types: [Clutter.Actor.$gtype] } },
}, class IconGrid extends St.Widget {
    _init(params) {
        super._init({ style_class: 'icon-grid',
                      y_align: Clutter.ActorAlign.START });

        params = Params.parse(params, { rowLimit: null,
                                        columnLimit: null,
                                        minRows: 1,
                                        minColumns: 1,
                                        fillParent: false,
                                        xAlign: St.Align.MIDDLE,
                                        padWithSpacing: false });
        this._rowLimit = params.rowLimit;
        this._colLimit = params.columnLimit;
        this._minRows = params.minRows;
        this._minColumns = params.minColumns;
        this._xAlign = params.xAlign;
        this._fillParent = params.fillParent;
        this._padWithSpacing = params.padWithSpacing;

        this.topPadding = 0;
        this.bottomPadding = 0;
        this.rightPadding = 0;
        this.leftPadding = 0;

        this._updateIconSizesLaterId = 0;

        this._items = [];
        this._clonesAnimating = [];
        // Pulled from CSS, but hardcode some defaults here
        this._spacing = 0;
        this._hItemSize = this._vItemSize = ICON_SIZE;
        this._fixedHItemSize = this._fixedVItemSize = undefined;
        this.connect('style-changed', this._onStyleChanged.bind(this));

        this.connect('actor-added', this._childAdded.bind(this));
        this.connect('actor-removed', this._childRemoved.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_unmap() {
        // Cancel animations when hiding the overview, to avoid icons
        // swarming into the void ...
        this._resetAnimationActors();
        super.vfunc_unmap();
    }

    _onDestroy() {
        if (this._updateIconSizesLaterId) {
            Meta.later_remove(this._updateIconSizesLaterId);
            this._updateIconSizesLaterId = 0;
        }
    }

    _keyFocusIn(actor) {
        this.emit('child-focused', actor);
    }

    _childAdded(grid, child) {
        child._iconGridKeyFocusInId = child.connect('key-focus-in', this._keyFocusIn.bind(this));

        child._paintVisible = child.opacity > 0;
        child._opacityChangedId = child.connect('notify::opacity', () => {
            let paintVisible = child._paintVisible;
            child._paintVisible = child.opacity > 0;
            if (paintVisible !== child._paintVisible)
                this.queue_relayout();
        });
    }

    _childRemoved(grid, child) {
        child.disconnect(child._iconGridKeyFocusInId);
        delete child._iconGridKeyFocusInId;

        child.disconnect(child._opacityChangedId);
        delete child._opacityChangedId;
        delete child._paintVisible;
    }

    vfunc_get_preferred_width(_forHeight) {
        if (this._fillParent)
            // Ignore all size requests of children and request a size of 0;
            // later we'll allocate as many children as fit the parent
            return [0, 0];

        let nChildren = this.get_n_children();
        let nColumns = this._colLimit
            ? Math.min(this._colLimit, nChildren)
            : nChildren;
        let totalSpacing = Math.max(0, nColumns - 1) * this._getSpacing();
        // Kind of a lie, but not really an issue right now.  If
        // we wanted to support some sort of hidden/overflow that would
        // need higher level design
        let minSize = this._getHItemSize() + this.leftPadding + this.rightPadding;
        let natSize = nColumns * this._getHItemSize() + totalSpacing + this.leftPadding + this.rightPadding;

        return this.get_theme_node().adjust_preferred_width(minSize, natSize);
    }

    _getVisibleChildren() {
        return this.get_children().filter(actor => actor.visible);
    }

    vfunc_get_preferred_height(forWidth) {
        if (this._fillParent)
            // Ignore all size requests of children and request a size of 0;
            // later we'll allocate as many children as fit the parent
            return [0, 0];

        let themeNode = this.get_theme_node();
        let children = this._getVisibleChildren();
        let nColumns;

        forWidth = themeNode.adjust_for_width(forWidth);

        if (forWidth < 0)
            nColumns = children.length;
        else
            [nColumns] = this._computeLayout(forWidth);

        let nRows;
        if (nColumns > 0)
            nRows = Math.ceil(children.length / nColumns);
        else
            nRows = 0;
        if (this._rowLimit)
            nRows = Math.min(nRows, this._rowLimit);
        let totalSpacing = Math.max(0, nRows - 1) * this._getSpacing();
        let height = nRows * this._getVItemSize() + totalSpacing + this.topPadding + this.bottomPadding;

        return themeNode.adjust_preferred_height(height, height);
    }

    vfunc_allocate(box, flags) {
        this.set_allocation(box, flags);

        let themeNode = this.get_theme_node();
        box = themeNode.get_content_box(box);

        if (this._fillParent) {
            // Reset the passed in box to fill the parent
            let parentBox = this.get_parent().allocation;
            let gridBox = themeNode.get_content_box(parentBox);
            box = themeNode.get_content_box(gridBox);
        }

        let children = this._getVisibleChildren();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        let spacing = this._getSpacing();
        let [nColumns, usedWidth] = this._computeLayout(availWidth);

        let leftEmptySpace;
        switch (this._xAlign) {
        case St.Align.START:
            leftEmptySpace = 0;
            break;
        case St.Align.MIDDLE:
            leftEmptySpace = Math.floor((availWidth - usedWidth) / 2);
            break;
        case St.Align.END:
            leftEmptySpace = availWidth - usedWidth;
        }

        let animating = this._clonesAnimating.length > 0;
        let x = box.x1 + leftEmptySpace + this.leftPadding;
        let y = box.y1 + this.topPadding;
        let columnIndex = 0;
        let rowIndex = 0;
        let nChangedIcons = 0;
        for (let i = 0; i < children.length; i++) {
            let childBox = this._calculateChildBox(children[i], x, y, box);

            if (this._rowLimit && rowIndex >= this._rowLimit ||
                this._fillParent && childBox.y2 > availHeight - this.bottomPadding) {
                children[i].opacity = 0;
            } else {
                if (!animating)
                    children[i].opacity = 255;

                if (animateIconPosition(children[i], childBox, flags, nChangedIcons))
                    nChangedIcons++;
            }

            columnIndex++;
            if (columnIndex == nColumns) {
                columnIndex = 0;
                rowIndex++;
            }

            if (columnIndex == 0) {
                y += this._getVItemSize() + spacing;
                x = box.x1 + leftEmptySpace + this.leftPadding;
            } else {
                x += this._getHItemSize() + spacing;
            }
        }
    }

    vfunc_get_paint_volume(paintVolume) {
        // Setting the paint volume does not make sense when we don't have
        // any allocation
        if (!this.has_allocation())
            return false;

        let themeNode = this.get_theme_node();
        let allocationBox = this.get_allocation_box();
        let paintBox = themeNode.get_paint_box(allocationBox);

        let origin = new Graphene.Point3D();
        origin.x = paintBox.x1 - allocationBox.x1;
        origin.y = paintBox.y1 - allocationBox.y1;
        origin.z = 0.0;

        paintVolume.set_origin(origin);
        paintVolume.set_width(paintBox.x2 - paintBox.x1);
        paintVolume.set_height(paintBox.y2 - paintBox.y1);

        if (this.get_clip_to_allocation())
            return true;

        for (let child = this.get_first_child();
            child != null;
            child = child.get_next_sibling()) {

            if (!child.visible || !child.opacity)
                continue;

            let childVolume = child.get_transformed_paint_volume(this);
            if (!childVolume)
                return false;

            paintVolume.union(childVolume);
        }

        return true;
    }

    /*
     * Intended to be override by subclasses if they need a different
     * set of items to be animated.
     */
    _getChildrenToAnimate() {
        return this._getVisibleChildren().filter(child => child.opacity > 0);
    }

    _resetAnimationActors() {
        this._clonesAnimating.forEach(clone => {
            clone.source.reactive = true;
            clone.source.opacity = 255;
            clone.destroy();
        });
        this._clonesAnimating = [];
    }

    _animationDone() {
        this._resetAnimationActors();
        this.emit('animation-done');
    }

    animatePulse(animationDirection) {
        if (animationDirection != AnimationDirection.IN) {
            throw new GObject.NotImplementedError("Pulse animation only implements " +
                                                  "'in' animation direction");
        }

        this._resetAnimationActors();

        let actors = this._getChildrenToAnimate();
        if (actors.length == 0) {
            this._animationDone();
            return;
        }

        // For few items the animation can be slow, so use a smaller
        // delay when there are less than 4 items
        // (ANIMATION_BASE_DELAY_FOR_ITEM = 1/4 *
        // ANIMATION_MAX_DELAY_FOR_ITEM)
        let maxDelay = Math.min(ANIMATION_BASE_DELAY_FOR_ITEM * actors.length,
                                ANIMATION_MAX_DELAY_FOR_ITEM);

        for (let index = 0; index < actors.length; index++) {
            let actor = actors[index];
            actor.set_scale(0, 0);
            actor.set_pivot_point(0.5, 0.5);

            let delay = index / actors.length * maxDelay;
            let bounceUpTime = ANIMATION_TIME_IN / 4;
            let isLastItem = index == actors.length - 1;
            actor.ease({
                scale_x: ANIMATION_BOUNCE_ICON_SCALE,
                scale_y: ANIMATION_BOUNCE_ICON_SCALE,
                duration: bounceUpTime,
                mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                delay,
                onComplete: () => {
                    let duration = ANIMATION_TIME_IN - bounceUpTime;
                    actor.ease({
                        scale_x: 1,
                        scale_y: 1,
                        duration,
                        mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                        onComplete: () => {
                            if (isLastItem)
                                this._animationDone();
                            actor.reactive = true;
                        },
                    });
                },
            });
        }
    }

    animateSpring() {
        // We don't do the icon grid animations on Endless
        this._animationDone();
    }

    _getAllocatedChildSizeAndSpacing(child) {
        let [,, natWidth, natHeight] = child.get_preferred_size();
        let width = Math.min(this._getHItemSize(), natWidth);
        let xSpacing = Math.max(0, width - natWidth) / 2;
        let height = Math.min(this._getVItemSize(), natHeight);
        let ySpacing = Math.max(0, height - natHeight) / 2;
        return [width, height, xSpacing, ySpacing];
    }

    _calculateChildBox(child, x, y, box) {
        /* Center the item in its allocation horizontally */
        let [width, height, childXSpacing, childYSpacing] =
            this._getAllocatedChildSizeAndSpacing(child);

        let childBox = new Clutter.ActorBox();
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL) {
            let _x = box.x2 - (x + width);
            childBox.x1 = Math.floor(_x - childXSpacing);
        } else {
            childBox.x1 = Math.floor(x + childXSpacing);
        }
        childBox.y1 = Math.floor(y + childYSpacing);
        childBox.x2 = childBox.x1 + width;
        childBox.y2 = childBox.y1 + height;
        return childBox;
    }

    columnsForWidth(rowWidth) {
        return this._computeLayout(rowWidth)[0];
    }

    getRowLimit() {
        return this._rowLimit;
    }

    _computeLayout(forWidth) {
        this.ensure_style();

        let nColumns = 0;
        let usedWidth = this.leftPadding + this.rightPadding;
        let spacing = this._getSpacing();

        while ((this._colLimit == null || nColumns < this._colLimit) &&
               (usedWidth + this._getHItemSize() <= forWidth)) {
            usedWidth += this._getHItemSize() + spacing;
            nColumns += 1;
        }

        if (nColumns > 0)
            usedWidth -= spacing;

        return [nColumns, usedWidth];
    }

    _onStyleChanged() {
        let themeNode = this.get_theme_node();
        this._spacing = themeNode.get_length('spacing');
        this._hItemSize = themeNode.get_length('-shell-grid-horizontal-item-size') || ICON_SIZE;
        this._vItemSize = themeNode.get_length('-shell-grid-vertical-item-size') || ICON_SIZE;
        this.queue_relayout();
    }

    nRows(forWidth) {
        let children = this._getVisibleChildren();
        let nColumns = forWidth < 0 ? children.length : this._computeLayout(forWidth)[0];
        let nRows = nColumns > 0 ? Math.ceil(children.length / nColumns) : 0;
        if (this._rowLimit)
            nRows = Math.min(nRows, this._rowLimit);
        return nRows;
    }

    rowsForHeight(forHeight) {
        return Math.floor((forHeight - (this.topPadding + this.bottomPadding) + this._getSpacing()) / (this._getVItemSize() + this._getSpacing()));
    }

    usedHeightForNRows(nRows) {
        return (this._getVItemSize() + this._getSpacing()) * nRows - this._getSpacing() + this.topPadding + this.bottomPadding;
    }

    usedWidth(forWidth) {
        return this.usedWidthForNColumns(this.columnsForWidth(forWidth));
    }

    usedWidthForNColumns(columns) {
        let usedWidth = columns  * (this._getHItemSize() + this._getSpacing());
        usedWidth -= this._getSpacing();
        return usedWidth + this.leftPadding + this.rightPadding;
    }

    removeAll() {
        this._items = [];
        this.remove_all_children();
    }

    destroyAll() {
        this._items = [];
        this.destroy_all_children();
    }

    addItem(item, index) {
        if (!(item.icon instanceof BaseIcon))
            throw new Error('Only items with a BaseIcon icon property can be added to IconGrid');

        this._items.push(item);
        if (index !== undefined)
            this.insert_child_at_index(item, index);
        else
            this.add_actor(item);
    }

    moveItem(item, newPosition) {
        if (!this.contains(item)) {
            log('Cannot move item not contained by the IconGrid');
            return;
        }

        let children = this.get_children();
        let visibleChildren = children.filter(c => c.is_visible());
        let visibleChildAtPosition = visibleChildren[newPosition];
        let realPosition = children.indexOf(visibleChildAtPosition);

        this.set_child_at_index(item, realPosition);

        return realPosition;
    }

    removeItem(item) {
        this.remove_child(item);
    }

    getItemAtIndex(index) {
        return this.get_child_at_index(index);
    }

    visibleItemsCount() {
        return this.get_children().filter(c => c.is_visible()).length;
    }

    setSpacing(spacing) {
        this._fixedSpacing = spacing;
    }

    _getSpacing() {
        return this._fixedSpacing ? this._fixedSpacing : this._spacing;
    }

    _getHItemSize() {
        return this._fixedHItemSize ? this._fixedHItemSize : this._hItemSize;
    }

    _getVItemSize() {
        return this._fixedVItemSize ? this._fixedVItemSize : this._vItemSize;
    }

    /*
     * This function must to be called before iconGrid allocation,
     * to know how much spacing can the grid has
     */
    adaptToSize() {
        this._fixedHItemSize = this._hItemSize;
        this._fixedVItemSize = this._vItemSize;
    }

    // Drag n' Drop methods

    nudgeItemsAtIndex(index, dragLocation) {
        // No nudging when the cursor is in an empty area
        if (dragLocation == DragLocation.EMPTY_AREA || dragLocation == DragLocation.ON_ICON)
            return;

        let children = this._getVisibleChildren();
        let nudgeIndex = index;
        let rtl = (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL);

        if (dragLocation != DragLocation.START_EDGE) {
            let leftItem = children[nudgeIndex - 1];
            let offset = rtl ? Math.floor(this._hItemSize * NUDGE_FACTOR) : Math.floor(-this._hItemSize * NUDGE_FACTOR);
            this._animateNudge(leftItem, NUDGE_ANIMATION_TYPE, NUDGE_DURATION, offset);
        }

        // Nudge the icon to the right if we are the first item or not at the
        // end of row
        if (dragLocation != DragLocation.END_EDGE) {
            let rightItem = children[nudgeIndex];
            let offset = rtl ? Math.floor(-this._hItemSize * NUDGE_FACTOR) : Math.floor(this._hItemSize * NUDGE_FACTOR);
            this._animateNudge(rightItem, NUDGE_ANIMATION_TYPE, NUDGE_DURATION, offset);
        }
    }

    removeNudges() {
        let children = this._getVisibleChildren();
        for (let child of children) {
            this._animateNudge(child,
                               NUDGE_RETURN_ANIMATION_TYPE,
                               NUDGE_RETURN_DURATION,
                               0);
        }
    }

    _animateNudge(item, animationType, duration, offset) {
        if (!item)
            return;

        item.ease({
            translation_x: offset,
            duration: duration,
            mode: animationType,
        });
    }

    // This function is overriden by the PaginatedIconGrid subclass so we can
    // take into account the extra space when dragging from a folder
    _calculateDndRow(y) {
        let rowHeight = this._getVItemSize() + this._getSpacing();
        return Math.floor(y / rowHeight);
    }

    // Returns the drop point index or -1 if we can't drop there
    canDropAt(x, y) {
        // This is an complex calculation, but in essence, we divide the grid
        // as:
        //
        //  left empty space
        //      |   left padding                          right padding
        //      |     |        width without padding               |
        // +--------+---+---------------------------------------+-----+
        // |        |   |        |           |          |       |     |
        // |        |   |        |           |          |       |     |
        // |        |   |--------+-----------+----------+-------|     |
        // |        |   |        |           |          |       |     |
        // |        |   |        |           |          |       |     |
        // |        |   |--------+-----------+----------+-------|     |
        // |        |   |        |           |          |       |     |
        // |        |   |        |           |          |       |     |
        // |        |   |--------+-----------+----------+-------|     |
        // |        |   |        |           |          |       |     |
        // |        |   |        |           |          |       |     |
        // +--------+---+---------------------------------------+-----+
        //
        // The left empty space is immediately discarded, and ignored in all
        // calculations.
        //
        // The width (with paddings) is used to determine if we're dragging
        // over the left or right padding, and which column is being dragged
        // on.
        //
        // Finally, the width without padding is used to figure out where in
        // the icon (start edge, end edge, on it, etc) the cursor is.

        let [nColumns, usedWidth] = this._computeLayout(this.width);

        let leftEmptySpace;
        switch (this._xAlign) {
        case St.Align.START:
            leftEmptySpace = 0;
            break;
        case St.Align.MIDDLE:
            leftEmptySpace = Math.floor((this.width - usedWidth) / 2);
            break;
        case St.Align.END:
            leftEmptySpace = availWidth - usedWidth;
        }

        x -= leftEmptySpace;
        y -= this.topPadding;

        let row = this._calculateDndRow(y);

        // Correct sx to handle the left padding to correctly calculate
        // the column
        let rtl = (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL);
        let gridX = x - this.leftPadding;

        let widthWithoutPadding = usedWidth - this.leftPadding - this.rightPadding;
        let columnWidth = widthWithoutPadding / nColumns;

        let column;
        if (x < this.leftPadding)
            column = 0;
        else if (x > usedWidth - this.rightPadding)
            column = nColumns - 1;
        else
            column = Math.floor(gridX / columnWidth);

        let isFirstIcon = column == 0;
        let isLastIcon = column == nColumns - 1;

        // If we're outside of the grid, we are in an invalid drop location
        if (x < 0 || x > usedWidth)
            return [-1, DragLocation.DEFAULT];

        let children = this.get_children().filter(c => c.is_visible());
        let childIndex = Math.min((row * nColumns) + column, children.length);

        // If we're above the grid vertically, we are in an invalid
        // drop location
        if (childIndex < 0)
            return [-1, DragLocation.DEFAULT];

        // If we're past the last visible element in the grid,
        // we might be allowed to drop there.
        if (childIndex >= children.length)
            return [children.length - 1, DragLocation.EMPTY_AREA];

        let child = children[childIndex];
        let [childMinWidth, childMinHeight, childNaturalWidth, childNaturalHeight] = child.get_preferred_size();

        // This is the width of the cell that contains the icon
        // (excluding spacing between cells)
        let childIconWidth = Math.max(this._getHItemSize(), childNaturalWidth);

        // Calculate the original position of the child icon (prior to nudging)
        let childX;
        if (rtl)
            childX = widthWithoutPadding - (column * columnWidth) - childIconWidth;
        else
            childX = column * columnWidth;

        let iconLeftX = childX + LEFT_DIVIDER_LEEWAY;
        let iconRightX = childX + childIconWidth - RIGHT_DIVIDER_LEEWAY;

        let dropIndex;
        let dragLocation;

        x -= this.leftPadding;

        if (x < iconLeftX) {
            // We are to the left of the icon target
            if (isFirstIcon || x < 0) {
                // We are before the leftmost icon on the grid
                if (rtl) {
                    dropIndex = childIndex + 1;
                    dragLocation = DragLocation.END_EDGE;
                } else {
                    dropIndex = childIndex;
                    dragLocation = DragLocation.START_EDGE;
                }
            } else {
                // We are between the previous icon (next in RTL) and this one
                if (rtl)
                    dropIndex = childIndex + 1;
                else
                    dropIndex = childIndex;

                dragLocation = DragLocation.DEFAULT;
            }
        } else if (x >= iconRightX) {
            // We are to the right of the icon target
            if (childIndex >= children.length) {
                // We are beyond the last valid icon
                // (to the right of the app store / trash can, if present)
                dropIndex = -1;
                dragLocation = DragLocation.DEFAULT;
            } else if (isLastIcon || x >= widthWithoutPadding) {
                // We are beyond the rightmost icon on the grid
                if (rtl) {
                    dropIndex = childIndex;
                    dragLocation = DragLocation.START_EDGE;
                } else {
                    dropIndex = childIndex + 1;
                    dragLocation = DragLocation.END_EDGE;
                }
            } else {
                // We are between this icon and the next one (previous in RTL)
                if (rtl)
                    dropIndex = childIndex;
                else
                    dropIndex = childIndex + 1;

                dragLocation = DragLocation.DEFAULT;
            }
        } else {
            // We are over the icon target area
            dropIndex = childIndex;
            dragLocation = DragLocation.ON_ICON;
        }

        return [dropIndex, dragLocation];
    }
});

var PaginatedIconGrid = GObject.registerClass(
class PaginatedIconGrid extends IconGrid {
    _init(params) {
        super._init(params);
        this._nPages = 0;
        this.currentPage = 0;
        this._rowsPerPage = 0;
        this._spaceBetweenPages = 0;
        this._childrenPerPage = 0;
    }

    vfunc_get_preferred_height(_forWidth) {
        let height = (this._availableHeightPerPageForItems() + this.bottomPadding + this.topPadding) * this._nPages + this._spaceBetweenPages * this._nPages;
        return [height, height];
    }

    vfunc_allocate(box, flags) {
        if (this._childrenPerPage == 0)
            log('computePages() must be called before allocate(); pagination will not work.');

        this.set_allocation(box, flags);

        if (this._fillParent) {
            // Reset the passed in box to fill the parent
            let parentBox = this.get_parent().allocation;
            let gridBox = this.get_theme_node().get_content_box(parentBox);
            box = this.get_theme_node().get_content_box(gridBox);
        }
        let children = this._getVisibleChildren();
        let availWidth = box.x2 - box.x1;
        let spacing = this._getSpacing();
        let [nColumns, usedWidth] = this._computeLayout(availWidth);

        let leftEmptySpace;
        switch (this._xAlign) {
        case St.Align.START:
            leftEmptySpace = 0;
            break;
        case St.Align.MIDDLE:
            leftEmptySpace = Math.floor((availWidth - usedWidth) / 2);
            break;
        case St.Align.END:
            leftEmptySpace = availWidth - usedWidth;
        }

        let x = box.x1 + leftEmptySpace + this.leftPadding;
        let y = box.y1 + this.topPadding;
        let columnIndex = 0;

        let nChangedIcons = 0;
        for (let i = 0; i < children.length; i++) {
            let childBox = this._calculateChildBox(children[i], x, y, box);

            if (animateIconPosition(children[i], childBox, flags, nChangedIcons))
                nChangedIcons++;

            children[i].show();

            columnIndex++;
            if (columnIndex == nColumns)
                columnIndex = 0;

            if (columnIndex == 0) {
                y += this._getVItemSize() + spacing;
                if ((i + 1) % this._childrenPerPage == 0)
                    y +=  this._spaceBetweenPages - spacing + this.bottomPadding + this.topPadding;
                x = box.x1 + leftEmptySpace + this.leftPadding;
            } else {
                x += this._getHItemSize() + spacing;
            }
        }
    }

    // Overridden from IconGrid
    _calculateDndRow(y) {
        let row = super._calculateDndRow(y);

        // If there's no extra space, just return the current value and maintain
        // the same behavior when without a folder opened.
        if (!this._extraSpaceData)
            return row;

        let [ baseRow, nRowsUp, nRowsDown ] = this._extraSpaceData;
        let newRow = row + nRowsUp;

        if (row > baseRow)
            newRow -= nRowsDown;

        return newRow;
    }

    _getChildrenToAnimate() {
        let children = super._getChildrenToAnimate();
        let firstIndex = this._childrenPerPage * this.currentPage;
        let lastIndex = firstIndex + this._childrenPerPage;

        return children.slice(firstIndex, lastIndex);
    }

    _computePages(availWidthPerPage, availHeightPerPage) {
        let [nColumns, usedWidth_] = this._computeLayout(availWidthPerPage);
        let nRows;
        let children = this._getVisibleChildren();
        if (nColumns > 0)
            nRows = Math.ceil(children.length / nColumns);
        else
            nRows = 0;
        if (this._rowLimit)
            nRows = Math.min(nRows, this._rowLimit);

        // We want to contain the grid inside the parent box with padding
        this._rowsPerPage = Math.min(Math.max (this._minRows, nRows), this.rowsForHeight(availHeightPerPage));
        this._nPages = Math.ceil(nRows / this._rowsPerPage);

        if (this._nPages > 1)
            this._spaceBetweenPages = availHeightPerPage - (this.topPadding + this.bottomPadding) - this._availableHeightPerPageForItems();
        else
            this._spaceBetweenPages = this._getSpacing();

        this._childrenPerPage = nColumns * this._rowsPerPage;
    }

    adaptToSize(availWidth, availHeight) {
        super.adaptToSize(availWidth, availHeight);
        this._computePages(availWidth, availHeight);
    }

    _availableHeightPerPageForItems() {
        return this.usedHeightForNRows(this._rowsPerPage) - (this.topPadding + this.bottomPadding);
    }

    nPages() {
        return this._nPages;
    }

    getPageHeight() {
        return this._availableHeightPerPageForItems();
    }

    getPageY(pageNumber) {
        if (!this._nPages)
            return 0;

        let firstPageItem = pageNumber * this._childrenPerPage;
        let childBox = this._getVisibleChildren()[firstPageItem].get_allocation_box();
        return childBox.y1 - this.topPadding;
    }

    getItemPage(item) {
        let children = this._getVisibleChildren();
        let index = children.indexOf(item);
        if (index == -1)
            throw new Error('Item not found.');
        return Math.floor(index / this._childrenPerPage);
    }
});
