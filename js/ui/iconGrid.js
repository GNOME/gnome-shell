// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BaseIcon, IconGrid, PaginatedIconGrid */

const { Clutter, GLib, GObject, Graphene, Meta, St } = imports.gi;

const Params = imports.misc.params;
const Main = imports.ui.main;

var ICON_SIZE = 96;
var MIN_ICON_SIZE = 16;

var ANIMATION_TIME_IN = 350;
var ANIMATION_TIME_OUT = 1 / 2 * ANIMATION_TIME_IN;
var ANIMATION_MAX_DELAY_FOR_ITEM = 2 / 3 * ANIMATION_TIME_IN;
var ANIMATION_BASE_DELAY_FOR_ITEM = 1 / 4 * ANIMATION_MAX_DELAY_FOR_ITEM;
var ANIMATION_MAX_DELAY_OUT_FOR_ITEM = 2 / 3 * ANIMATION_TIME_OUT;
var ANIMATION_FADE_IN_TIME_FOR_ITEM = 1 / 4 * ANIMATION_TIME_IN;

var ANIMATION_BOUNCE_ICON_SCALE = 1.1;

var AnimationDirection = {
    IN: 0,
    OUT: 1,
};

var IconSize = {
    HUGE: 128,
    LARGE: 96,
    MEDIUM: 64,
    SMALL: 32,
    TINY: 16,
};

var APPICON_ANIMATION_OUT_SCALE = 3;
var APPICON_ANIMATION_OUT_TIME = 250;

const ICON_POSITION_DELAY = 25;

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
            this.label = new St.Label({ text: label });
            this.label.clutter_text.set({
                x_align: Clutter.ActorAlign.CENTER,
                y_align: Clutter.ActorAlign.CENTER,
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

function animateIconPosition(icon, box, nChangedIcons) {
    if (!icon.has_allocation() || icon.allocation.equal(box) || icon.opacity === 0) {
        icon.allocate(box);
        return false;
    }

    icon.save_easing_state();
    icon.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
    icon.set_easing_delay(nChangedIcons * ICON_POSITION_DELAY);

    icon.allocate(box);

    icon.restore_easing_state();

    return true;
}

function swap(value, length) {
    return length - value - 1;
}

var IconGridLayout = GObject.registerClass({
    Properties: {
        'allow-incomplete-pages': GObject.ParamSpec.boolean('allow-incomplete-pages',
            'Allow incomplete pages', 'Allow incomplete pages',
            GObject.ParamFlags.READWRITE,
            true),
        'column-spacing': GObject.ParamSpec.int('column-spacing',
            'Column spacing', 'Column spacing',
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'columns-per-page': GObject.ParamSpec.int('columns-per-page',
            'Columns per page', 'Columns per page',
            GObject.ParamFlags.READWRITE,
            1, GLib.MAXINT32, 1),
        'icon-size': GObject.ParamSpec.int('icon-size',
            'Icon size', 'Icon size',
            GObject.ParamFlags.READABLE,
            0, GLib.MAXINT32, 0),
        'last-row-align': GObject.ParamSpec.enum('last-row-align',
            'Last row align', 'Last row align',
            GObject.ParamFlags.READWRITE,
            Clutter.ActorAlign.$gtype,
            Clutter.ActorAlign.FILL),
        'orientation': GObject.ParamSpec.enum('orientation',
            'Orientation', 'Orientation',
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation.$gtype,
            Clutter.Orientation.VERTICAL),
        'page-halign': GObject.ParamSpec.enum('page-halign',
            'Horizontal page align',
            'Horizontal page align',
            GObject.ParamFlags.READWRITE,
            Clutter.ActorAlign.$gtype,
            Clutter.ActorAlign.FILL),
        'page-valign': GObject.ParamSpec.enum('page-valign',
            'Vertical page align',
            'Vertical page align',
            GObject.ParamFlags.READWRITE,
            Clutter.ActorAlign.$gtype,
            Clutter.ActorAlign.FILL),
        'row-spacing': GObject.ParamSpec.int('row-spacing',
            'Row spacing', 'Row spacing',
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'rows-per-page': GObject.ParamSpec.int('rows-per-page',
            'Rows per page', 'Rows per page',
            GObject.ParamFlags.READWRITE,
            1, GLib.MAXINT32, 1),
    },
    Signals: {
        'pages-changed': {},
    },
}, class OmniGridLayout extends Clutter.LayoutManager {
    _init(params = {}) {
        params = Params.parse(params, {
            allow_incomplete_pages: true,
            column_spacing: 0,
            columns_per_page: 6,
            last_row_align: Clutter.ActorAlign.FILL,
            orientation: Clutter.Orientation.VERTICAL,
            page_halign: Clutter.ActorAlign.FILL,
            page_valign: Clutter.ActorAlign.FILL,
            row_spacing: 0,
            rows_per_page: 4,
        });

        this._allowIncompletePages = params.allow_incomplete_pages;
        this._columnsPerPage = params.columns_per_page;
        this._rowsPerPage = params.rows_per_page;
        this._orientation = params.orientation;
        this._columnSpacing = params.column_spacing;
        this._pageHAlign = params.page_halign;
        this._pageVAlign = params.page_valign;
        this._lastRowAlign = params.last_row_align;
        this._rowSpacing = params.row_spacing;

        super._init(params);

        this._iconSize = IconSize.LARGE;
        this._pageSizeChanged = false;
        this._pageHeight = 0;
        this._pageWidth = 0;
        this._nPages = -1;

        // [
        //     {
        //         children: [ itemData, itemData, itemData, ... ],
        //     },
        //     {
        //         children: [ itemData, itemData, itemData, ... ],
        //     },
        //     {
        //         children: [ itemData, itemData, itemData, ... ],
        //     },
        // ]
        this._pages = [];

        // {
        //     item: {
        //         actor: Clutter.Actor,
        //         pageIndex: <index>,
        //     },
        //     item: {
        //         actor: Clutter.Actor,
        //         pageIndex: <index>,
        //     },
        // }
        this._items = new Map();

        this._containerDestroyedId = 0;
        this._updateIconSizesLaterId = 0;
    }

    _findBestIconSize() {
        let nColumns = this._columnsPerPage;
        let nRows = this._rowsPerPage;
        let columnSpacingPerPage = this._columnSpacing * (nColumns - 1);
        let rowSpacingPerPage = this._rowSpacing * (nRows - 1);
        let firstItem = this._actor.get_first_child();
        let bestSize;

        for (let size of Object.values(IconSize)) {
            let usedWidth, usedHeight;

            if (firstItem) {
                firstItem.icon.setIconSize(size);
                const [firstItemWidth, firstItemHeight] =
                    firstItem.get_preferred_size();

                const itemSize = Math.max(firstItemWidth, firstItemHeight);

                usedWidth = itemSize * nColumns;
                usedHeight = itemSize * nRows;
            } else {
                usedWidth = size * nColumns;
                usedHeight = size * nRows;
            }

            const emptyHSpace =
                this._pageWidth - usedWidth - columnSpacingPerPage;
            const emptyVSpace =
                this._pageHeight - usedHeight -  rowSpacingPerPage;

            if (emptyHSpace >= 0 && emptyVSpace > 0) {
                bestSize = size;
                break;
            }
        }

        return bestSize;
    }

    _getChildrenMaxSize() {
        let minWidth = 0;
        let minHeight = 0;

        for (let child of this._actor) {
            if (!child.visible)
                continue;

            const [childMinHeight] = child.get_preferred_height(-1);
            const [childMinWidth] = child.get_preferred_width(-1);

            minWidth = Math.max(minWidth, childMinWidth);
            minHeight = Math.max(minHeight, childMinHeight);
        }

        return Math.max(minWidth, minHeight);
    }

    _getVisibleChildrenForPage(pageIndex) {
        return this._pages[pageIndex].children.filter(actor => actor.visible);
    }

    _updatePages() {
        for (let i in this._pages)
            this._maybeOverflowPage(i);
    }

    _removePage(pageIndex) {
        // Make sure to not leave any icon left here
        this._pages[pageIndex].children.forEach(item => {
            this._items.delete(item);
        });

        this._pages.splice(pageIndex, 1);
        this.emit('pages-changed');
    }

    _maybeReducePage(pageIndex) {
        if (pageIndex >= this._pages.length - 1)
            return;

        const visiblePageItems = this._getVisibleChildrenForPage(pageIndex);
        const itemsPerPage = this._columnsPerPage * this._rowsPerPage;

        // No reduce needed
        if (visiblePageItems.length === itemsPerPage)
            return;

        const visibleNextPageItems = this._getVisibleChildrenForPage(pageIndex + 1);
        const nMissingItems = Math.min(itemsPerPage - visiblePageItems.length, visibleNextPageItems.length);

        // Append to the current page the first items of the next page
        for (let i = 0; i < nMissingItems; i++) {
            const reducedItem = visibleNextPageItems[0];

            this._removeItemData(reducedItem);
            this._addItemToPage(reducedItem, pageIndex, -1);
        }

        // We may have made the next page incomplete, reduce it too
        this._maybeReducePage(pageIndex + 1);
    }

    _removeItemData(item) {
        const itemData = this._items.get(item);
        const pageIndex = itemData.pageIndex;
        const page = this._pages[pageIndex];
        const itemIndex = page.children.indexOf(item);

        item.disconnect(itemData.destroyId);
        item.disconnect(itemData.visibleId);

        page.children.splice(itemIndex, 1);

        // Delete the page if this is the last icon in it
        const visibleItems = this._getVisibleChildrenForPage(pageIndex);
        if (visibleItems.length === 0)
            this._removePage(itemData.pageIndex);

        this._items.delete(item);

        if (!this._allowIncompletePages)
            this._maybeReducePage(pageIndex);
    }

    _maybeOverflowPage(pageIndex) {
        const visiblePageItems = this._getVisibleChildrenForPage(pageIndex);
        const itemsPerPage = this._columnsPerPage * this._rowsPerPage;

        // No overflow needed
        if (visiblePageItems.length <= itemsPerPage)
            return;

        const nExtraItems = visiblePageItems.length - itemsPerPage;
        for (let i = 0; i < nExtraItems; i++) {
            const overflowIndex = visiblePageItems.length - i - 1;
            const overflowItem = visiblePageItems[overflowIndex];

            this._removeItemData(overflowItem);
            this._addItemToPage(overflowItem, pageIndex + 1, 0);
        }
    }

    _addItemToPage(item, pageIndex, index) {
        // Ensure we have at least one page
        if (this._pages.length === 0)
            this._appendPage();

        // Append a new page if necessary
        if (pageIndex === this._pages.length)
            this._appendPage();

        if (pageIndex === -1)
            pageIndex = this._pages.length - 1;

        if (index === -1)
            index = this._pages[pageIndex].children.length;

        this._items.set(item, {
            actor: item,
            pageIndex,
            destroyId: item.connect('destroy', () => this._removeItemData(item)),
            visibleId: item.connect('notify::visible', () => {
                const itemData = this._items.get(item);

                if (item.visible)
                    this._maybeOverflowPage(itemData.pageIndex);
                else if (!this._allowIncompletePages)
                    this._maybeReducePage(itemData.pageIndex);
            }),
        });

        item.icon.setIconSize(this._iconSize);

        this._pages[pageIndex].children.splice(index, 0, item);
        this._maybeOverflowPage(pageIndex);
    }

    _appendPage() {
        this._pages.push({ children: [] });
        this.emit('pages-changed');
    }

    _calculateSpacing(childSize) {
        const nColumns = this._columnsPerPage;
        const nRows = this._rowsPerPage;
        const usedWidth = childSize * nColumns;
        const usedHeight = childSize * nRows;
        const columnSpacingPerPage = this._columnSpacing * (nColumns - 1);
        const rowSpacingPerPage = this._rowSpacing * (nRows - 1);

        let emptyHSpace = this._pageWidth - usedWidth - columnSpacingPerPage;
        let emptyVSpace = this._pageHeight - usedHeight -  rowSpacingPerPage;
        let leftEmptySpace;
        let topEmptySpace;
        let hSpacing;
        let vSpacing;

        switch (this._pageHAlign) {
        case Clutter.ActorAlign.START:
            leftEmptySpace = 0;
            hSpacing = this._columnSpacing;
            break;
        case Clutter.ActorAlign.CENTER:
            leftEmptySpace = Math.floor(emptyHSpace / 2);
            hSpacing = this._columnSpacing;
            break;
        case Clutter.ActorAlign.END:
            leftEmptySpace = emptyHSpace;
            hSpacing = this._columnSpacing;
            break;
        case Clutter.ActorAlign.FILL:
            leftEmptySpace = 0;
            hSpacing = Math.max(emptyHSpace / (nColumns - 1), this._columnSpacing);
            break;
        }

        switch (this._pageVAlign) {
        case Clutter.ActorAlign.START:
            topEmptySpace = 0;
            vSpacing = this._rowSpacing;
            break;
        case Clutter.ActorAlign.CENTER:
            topEmptySpace = Math.floor(emptyVSpace / 2);
            vSpacing = this._rowSpacing;
            break;
        case Clutter.ActorAlign.END:
            topEmptySpace = emptyVSpace;
            vSpacing = this._rowSpacing;
            break;
        case Clutter.ActorAlign.FILL:
            topEmptySpace = 0;
            vSpacing = Math.max(emptyVSpace / (nRows - 1), this._rowSpacing);
            break;
        }

        return [leftEmptySpace, topEmptySpace, hSpacing, vSpacing];
    }

    _getLastRowAlign(items, itemIndex, childSize, spacing) {
        const nRows = Math.ceil(items.length / this._columnsPerPage);

        let rowAlign = 0;
        let row = Math.floor(itemIndex / this._columnsPerPage);

        // Only apply to the last row
        if (row < nRows - 1)
            return 0;

        let firstRowIndex = row * this._columnsPerPage;
        let lastRowIndex = Math.min((row + 1) * this._columnsPerPage - 1,
            items.length - 1);
        let itemsInThisRow = lastRowIndex - firstRowIndex + 1;
        let rowWidth =
            this._columnsPerPage * childSize + (this._columnsPerPage - 1) * spacing;
        let usedWidth =
            itemsInThisRow * childSize + (itemsInThisRow - 1) * spacing;
        let availableWidth = rowWidth - usedWidth;

        const isRtl =
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;

        switch (this._lastRowAlign) {
        case Clutter.ActorAlign.START:
            rowAlign = 0;
            break;
        case Clutter.ActorAlign.CENTER:
            rowAlign = availableWidth / 2;
            break;
        case Clutter.ActorAlign.END:
            rowAlign = availableWidth;
            break;
        case Clutter.ActorAlign.FILL:
            rowAlign = 0;
            break;
        }

        return isRtl ? rowAlign * -1 : rowAlign;
    }

    _onDestroy() {
        if (this._updateIconSizesLaterId >= 0) {
            Meta.later_remove(this._updateIconSizesLaterId);
            this._updateIconSizesLaterId = 0;
        }
    }

    vfunc_set_container(container) {
        if (this._actor) {
            this._actor.disconnect(this._containerDestroyedId);
            this._actor = null;
        }

        this._actor = container;

        if (this._actor)
            this._containerDestroyedId = this._actor.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_get_preferred_width(_actor, _forHeight) {
        let minWidth = -1;
        let natWidth = -1;

        switch (this._orientation) {
        case Clutter.Orientation.VERTICAL:
            natWidth = this._pageWidth;
            minWidth = IconSize.TINY;
            break;

        case Clutter.Orientation.HORIZONTAL:
            natWidth = this._pageWidth * this._pages.length;
            minWidth = natWidth;
            break;
        }

        return [minWidth, natWidth];
    }

    vfunc_get_preferred_height(_actor, _forWidth) {
        let minHeight = -1;
        let natHeight = -1;

        switch (this._orientation) {
        case Clutter.Orientation.VERTICAL:
            natHeight = this._pageHeight * this._pages.length;
            minHeight = natHeight;
            break;

        case Clutter.Orientation.HORIZONTAL:
            natHeight = this._pageHeight;
            minHeight = IconSize.TINY;
            break;
        }

        return [minHeight, natHeight];
    }

    vfunc_allocate() {
        this._updatePages();

        const isRtl =
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
        const childSize = this._getChildrenMaxSize();

        const [leftEmptySpace, topEmptySpace, hSpacing, vSpacing] =
            this._calculateSpacing(childSize);

        const childBox = new Clutter.ActorBox();
        childBox.set_size(childSize, childSize);

        let nChangedIcons = 0;

        this._pages.forEach((page, pageIndex) => {
            const visibleItems =
                page.children.filter(actor => actor.visible);

            if (isRtl && this._orientation === Clutter.Orientation.HORIZONTAL)
                pageIndex = swap(pageIndex, this._pages.length);

            visibleItems.forEach((item, itemIndex) => {
                const row = Math.floor(itemIndex / this._columnsPerPage);
                let column = itemIndex % this._columnsPerPage;

                if (isRtl)
                    column = swap(column, this._columnsPerPage);

                let lastRowAlign = this._getLastRowAlign(visibleItems,
                    itemIndex, childSize, hSpacing);

                // Icon position
                let x = leftEmptySpace + lastRowAlign + column * (childSize + hSpacing);
                let y = topEmptySpace + row * (childSize + vSpacing);

                // Page start
                switch (this._orientation) {
                case Clutter.Orientation.HORIZONTAL:
                    x += pageIndex * this._pageWidth;
                    break;
                case Clutter.Orientation.VERTICAL:
                    y += pageIndex * this._pageHeight;
                    break;
                }

                childBox.set_origin(x, y);

                // Only ease icons when the page size didn't change
                if (this._pageSizeChanged)
                    item.allocate(childBox);
                else if (animateIconPosition(item, childBox, nChangedIcons))
                    nChangedIcons++;
            });
        });

        this._pageSizeChanged = false;
    }

    /**
     * addItem:
     * @param {Clutter.Actor} item: item to append to the grid
     * @param {int} index: position in the page
     * @param {int} page: page number
     *
     * Adds @item to the grid. @item must not be part of the grid.
     *
     * If @index exceeds the number of items per page, @item will
     * be added to the next page.
     *
     * @page must be a number between 0 and the number of pages.
     * Adding to the page after next will create a new page.
     */
    addItem(item, index = -1, page = -1) {
        if (this._items.has(item))
            throw new Error(`Item ${item} already added to IconGridLayout`);

        if (page > this._pages.length)
            throw new Error(`Cannot add ${item} to page ${page}`);

        this._addItemToPage(item, page, index);
    }

    /**
     * appendItem:
     * @param {Clutter.Actor} item: item to append to the grid
     *
     * Appends @item to the grid. @item must not be part of the grid.
     */
    appendItem(item) {
        this.addItem(item);
    }

    /**
     * removeItem:
     * @param {Clutter.Actor} item: item to remove from the grid
     *
     * Removes @item to the grid. @item must be part of the grid.
     */
    removeItem(item) {
        if (!this._items.has(item))
            throw new Error(`Item ${item} is not part of the IconGridLayout`);

        this._removeItemData(item);
    }

    /**
     * getChildrenAtPage:
     * @param {int} pageIndex: page index
     *
     * Retrieves the children at page @pageIndex. Children may be invisible.
     *
     * @returns {Array} an array of {Clutter.Actor}s
     */
    getChildrenAtPage(pageIndex) {
        if (pageIndex >= this._pages.length)
            throw new Error(`IconGridLayout does not have page ${pageIndex}`);

        return [...this._pages[pageIndex].children];
    }

    /**
     * getItemPosition:
     * @param {BaseIcon} item: the item
     *
     * Retrieves the position of @item is its page, or -1 if @item is not
     * part of the grid.
     *
     * @returns {[int, int]} the page and position of @item
     */
    getItemPosition(item) {
        if (!this._items.has(item))
            return [-1, -1];

        const itemData = this._items.get(item);
        const visibleItems = this._getVisibleChildrenForPage(itemData.pageIndex);

        return [itemData.pageIndex, visibleItems.indexOf(item)];
    }

    /**
     * getItemAt:
     * @param {int} page: the page
     * @param {int} position: the position in page
     *
     * Retrieves the item at @page and @position.
     *
     * @returns {BaseItem} the item at @page and @position, or null
     */
    getItemAt(page, position) {
        if (page < 0 || page >= this._pages.length)
            return null;

        const visibleItems = this._getVisibleChildrenForPage(page);

        if (position < 0 || position >= visibleItems.length)
            return null;

        return visibleItems[position];
    }

    /**
     * getItemPage:
     * @param {BaseIcon} item: the item
     *
     * Retrieves the page @item is in, or -1 if @item is not part of the grid.
     *
     * @returns {int} the page where @item is in
     */
    getItemPage(item) {
        if (!this._items.has(item))
            return -1;

        const itemData = this._items.get(item);
        return itemData.pageIndex;
    }

    // eslint-disable-next-line camelcase
    get allow_incomplete_pages() {
        return this._allowIncompletePages;
    }

    // eslint-disable-next-line camelcase
    set allow_incomplete_pages(v) {
        if (this._allowIncompletePages === v)
            return;

        this._allowIncompletePages = v;
        this.notify('allow-incomplete-pages');
    }

    // eslint-disable-next-line camelcase
    get column_spacing() {
        return this._columnSpacing;
    }

    // eslint-disable-next-line camelcase
    set column_spacing(v) {
        if (this._columnSpacing === v)
            return;

        this._columnSpacing = v;
        this.notify('column-spacing');
    }

    // eslint-disable-next-line camelcase
    get columns_per_page() {
        return this._columnsPerPage;
    }

    // eslint-disable-next-line camelcase
    set columns_per_page(v) {
        if (this._columnsPerPage === v)
            return;

        this._columnsPerPage = v;
        this.notify('columns-per-page');
    }

    // eslint-disable-next-line camelcase
    get icon_size() {
        return this._iconSize;
    }

    // eslint-disable-next-line camelcase
    get last_row_align() {
        return this._lastRowAlign;
    }

    // eslint-disable-next-line camelcase
    set last_row_align(v) {
        if (this._lastRowAlign === v)
            return;

        this._lastRowAlign = v;
        this.notify('last-row-align');
    }

    get nPages() {
        return this._pages.length;
    }

    // eslint-disable-next-line camelcase
    get page_halign() {
        return this._pageHAlign;
    }

    // eslint-disable-next-line camelcase
    set page_halign(v) {
        if (this._pageHAlign === v)
            return;

        this._pageHAlign = v;
        this.notify('page-halign');
    }

    // eslint-disable-next-line camelcase
    get page_valign() {
        return this._pageVAlign;
    }

    // eslint-disable-next-line camelcase
    set page_valign(v) {
        if (this._pageVAlign === v)
            return;

        this._pageVAlign = v;
        this.notify('page-valign');
    }

    // eslint-disable-next-line camelcase
    get row_spacing() {
        return this._rowSpacing;
    }

    // eslint-disable-next-line camelcase
    set row_spacing(v) {
        if (this._rowSpacing === v)
            return;

        this._rowSpacing = v;
        this.notify('row-spacing');
    }

    // eslint-disable-next-line camelcase
    get rows_per_page() {
        return this._rowsPerPage;
    }

    // eslint-disable-next-line camelcase
    set rows_per_page(v) {
        if (this._rowsPerPage === v)
            return;

        this._rowsPerPage = v;
        this.notify('rows-per-page');
    }

    get orientation() {
        return this._orientation;
    }

    set orientation(v) {
        if (this._orientation === v)
            return;

        switch (v) {
        case Clutter.Orientation.VERTICAL:
            this.request_mode = Clutter.RequestMode.HEIGHT_FOR_WIDTH;
            break;
        case Clutter.Orientation.HORIZONTAL:
            this.request_mode = Clutter.RequestMode.WIDTH_FOR_HEIGHT;
            break;
        }

        this._orientation = v;
        this.notify('orientation');
    }

    get pageHeight() {
        return this._pageHeight;
    }

    get pageWidth() {
        return this._pageWidth;
    }

    adaptToSize(pageWidth, pageHeight) {
        if (this._pageWidth === pageWidth && this._pageHeight === pageHeight)
            return;

        this._pageWidth = pageWidth;
        this._pageHeight = pageHeight;
        this._pageSizeChanged = true;

        if (this._updateIconSizesLaterId === 0) {
            this._updateIconSizesLaterId =
                Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                    const iconSize = this._findBestIconSize();

                    if (this._iconSize !== iconSize) {
                        this._iconSize = iconSize;

                        this._actor.get_children().forEach(child => {
                            child.icon.setIconSize(iconSize);
                        });

                        this.notify('icon-size');
                    }

                    this._updateIconSizesLaterId = 0;
                    return GLib.SOURCE_REMOVE;
                });
        }
    }
});

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

        this._nPages = 0;
        this.currentPage = 0;
        this._rowsPerPage = 0;
        this._spaceBetweenPages = 0;
        this._childrenPerPage = 0;

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

    _availableHeightPerPageForItems() {
        return this.usedHeightForNRows(this._rowsPerPage) - (this.topPadding + this.bottomPadding);
    }

    vfunc_get_preferred_height() {
        let height = (this._availableHeightPerPageForItems() + this.bottomPadding + this.topPadding) * this._nPages + this._spaceBetweenPages * this._nPages;
        return [height, height];
    }

    vfunc_allocate(box) {
        if (this._childrenPerPage == 0)
            log('computePages() must be called before allocate(); pagination will not work.');

        this.set_allocation(box);

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

            if (animateIconPosition(children[i], childBox, nChangedIcons))
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
        const children = this._getVisibleChildren().filter(child => child.opacity > 0);
        let firstIndex = this._childrenPerPage * this.currentPage;
        let lastIndex = firstIndex + this._childrenPerPage;

        return children.slice(firstIndex, lastIndex);
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

    animateSpring(animationDirection, sourceActor) {
        this._resetAnimationActors();

        let actors = this._getChildrenToAnimate();
        if (actors.length == 0) {
            this._animationDone();
            return;
        }

        let [sourceX, sourceY] = sourceActor.get_transformed_position();
        let [sourceWidth, sourceHeight] = sourceActor.get_size();
        // Get the center
        let [sourceCenterX, sourceCenterY] = [sourceX + sourceWidth / 2, sourceY + sourceHeight / 2];
        // Design decision, 1/2 of the source actor size.
        let [sourceScaledWidth, sourceScaledHeight] = [sourceWidth / 2, sourceHeight / 2];

        actors.forEach(actor => {
            let [actorX, actorY] = actor._transformedPosition = actor.get_transformed_position();
            let [x, y] = [actorX - sourceX, actorY - sourceY];
            actor._distance = Math.sqrt(x * x + y * y);
        });
        let maxDist = actors.reduce((prev, cur) => {
            return Math.max(prev, cur._distance);
        }, 0);
        let minDist = actors.reduce((prev, cur) => {
            return Math.min(prev, cur._distance);
        }, Infinity);
        let normalization = maxDist - minDist;

        actors.forEach(actor => {
            let clone = new Clutter.Clone({ source: actor });
            this._clonesAnimating.push(clone);
            Main.uiGroup.add_actor(clone);
        });

        /*
         * ^
         * | These need to be separate loops because Main.uiGroup.add_actor
         * | is excessively slow if done inside the below loop and we want the
         * | below loop to complete within one frame interval (#2065, !1002).
         * v
         */

        this._clonesAnimating.forEach(actorClone => {
            let actor = actorClone.source;
            actor.opacity = 0;
            actor.reactive = false;

            let [width, height] = this._getAllocatedChildSizeAndSpacing(actor);
            actorClone.set_size(width, height);
            let scaleX = sourceScaledWidth / width;
            let scaleY = sourceScaledHeight / height;
            let [adjustedSourcePositionX, adjustedSourcePositionY] = [sourceCenterX - sourceScaledWidth / 2, sourceCenterY - sourceScaledHeight / 2];

            let movementParams, fadeParams;
            if (animationDirection == AnimationDirection.IN) {
                let isLastItem = actor._distance == minDist;

                actorClone.opacity = 0;
                actorClone.set_scale(scaleX, scaleY);
                actorClone.set_translation(
                    adjustedSourcePositionX, adjustedSourcePositionY, 0);

                let delay = (1 - (actor._distance - minDist) / normalization) * ANIMATION_MAX_DELAY_FOR_ITEM;
                let [finalX, finalY]  = actor._transformedPosition;
                movementParams = {
                    translation_x: finalX,
                    translation_y: finalY,
                    scale_x: 1,
                    scale_y: 1,
                    duration: ANIMATION_TIME_IN,
                    mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                    delay,
                };

                if (isLastItem)
                    movementParams.onComplete = this._animationDone.bind(this);

                fadeParams = {
                    opacity: 255,
                    duration: ANIMATION_FADE_IN_TIME_FOR_ITEM,
                    mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                    delay,
                };
            } else {
                let isLastItem = actor._distance == maxDist;

                let [startX, startY]  = actor._transformedPosition;
                actorClone.set_translation(startX, startY, 0);

                let delay = (actor._distance - minDist) / normalization * ANIMATION_MAX_DELAY_OUT_FOR_ITEM;
                movementParams = {
                    translation_x: adjustedSourcePositionX,
                    translation_y: adjustedSourcePositionY,
                    scale_x: scaleX,
                    scale_y: scaleY,
                    duration: ANIMATION_TIME_OUT,
                    mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                    delay,
                };

                if (isLastItem)
                    movementParams.onComplete = this._animationDone.bind(this);

                fadeParams = {
                    opacity: 0,
                    duration: ANIMATION_FADE_IN_TIME_FOR_ITEM,
                    mode: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
                    delay: ANIMATION_TIME_OUT + delay - ANIMATION_FADE_IN_TIME_FOR_ITEM,
                };
            }

            actorClone.ease(movementParams);
            actorClone.ease(fadeParams);
        });
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

    addItem(item, index) {
        if (!(item.icon instanceof BaseIcon))
            throw new Error('Only items with a BaseIcon icon property can be added to IconGrid');

        this._items.push(item);
        if (index !== undefined)
            this.insert_child_at_index(item, index);
        else
            this.add_actor(item);
    }

    removeItem(item) {
        this.remove_child(item);
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

    _updateSpacingForSize(availWidth, availHeight) {
        let maxEmptyVArea = availHeight - this._minRows * this._getVItemSize();
        let maxEmptyHArea = availWidth - this._minColumns * this._getHItemSize();
        let maxHSpacing, maxVSpacing;

        if (this._padWithSpacing) {
            // minRows + 1 because we want to put spacing before the first row, so it is like we have one more row
            // to divide the empty space
            maxVSpacing = Math.floor(maxEmptyVArea / (this._minRows + 1));
            maxHSpacing = Math.floor(maxEmptyHArea / (this._minColumns + 1));
        } else {
            if (this._minRows <=  1)
                maxVSpacing = maxEmptyVArea;
            else
                maxVSpacing = Math.floor(maxEmptyVArea / (this._minRows - 1));

            if (this._minColumns <=  1)
                maxHSpacing = maxEmptyHArea;
            else
                maxHSpacing = Math.floor(maxEmptyHArea / (this._minColumns - 1));
        }

        let maxSpacing = Math.min(maxHSpacing, maxVSpacing);
        // Limit spacing to the item size
        maxSpacing = Math.min(maxSpacing, Math.min(this._getVItemSize(), this._getHItemSize()));
        // The minimum spacing, regardless of whether it satisfies the row/columng minima,
        // is the spacing we get from CSS.
        let spacing = Math.max(this._spacing, maxSpacing);
        this.setSpacing(spacing);
        if (this._padWithSpacing)
            this.topPadding = this.rightPadding = this.bottomPadding = this.leftPadding = spacing;
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
        this._rowsPerPage = this.rowsForHeight(availHeightPerPage);
        this._nPages = Math.ceil(nRows / this._rowsPerPage);
        this._spaceBetweenPages = availHeightPerPage - (this.topPadding + this.bottomPadding) - this._availableHeightPerPageForItems();
        this._childrenPerPage = nColumns * this._rowsPerPage;
    }

    /*
     * This function must to be called before iconGrid allocation,
     * to know how much spacing can the grid has
     */
    adaptToSize(availWidth, availHeight) {
        this._fixedHItemSize = this._hItemSize;
        this._fixedVItemSize = this._vItemSize;
        this._updateSpacingForSize(availWidth, availHeight);

        if (this.columnsForWidth(availWidth) < this._minColumns || this.rowsForHeight(availHeight) < this._minRows) {
            let neededWidth = this.usedWidthForNColumns(this._minColumns) - availWidth;
            let neededHeight = this.usedHeightForNRows(this._minRows) - availHeight;

            let neededSpacePerItem = neededWidth > neededHeight
                ? Math.ceil(neededWidth / this._minColumns)
                : Math.ceil(neededHeight / this._minRows);
            this._fixedHItemSize = Math.max(this._hItemSize - neededSpacePerItem, MIN_ICON_SIZE);
            this._fixedVItemSize = Math.max(this._vItemSize - neededSpacePerItem, MIN_ICON_SIZE);

            this._updateSpacingForSize(availWidth, availHeight);
        }
        if (!this._updateIconSizesLaterId) {
            this._updateIconSizesLaterId = Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                                                          this._updateIconSizes.bind(this));
        }
        this._computePages(availWidth, availHeight);
    }

    // Note that this is ICON_SIZE as used by BaseIcon, not elsewhere in IconGrid; it's a bit messed up
    _updateIconSizes() {
        this._updateIconSizesLaterId = 0;
        let scale = Math.min(this._fixedHItemSize, this._fixedVItemSize) / Math.max(this._hItemSize, this._vItemSize);
        let newIconSize = Math.floor(ICON_SIZE * scale);
        for (let i in this._items)
            this._items[i].icon.setIconSize(newIconSize);

        return GLib.SOURCE_REMOVE;
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
