// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BaseIcon, IconGrid, IconGridLayout */

const { Clutter, GLib, GObject, Meta, St } = imports.gi;

const Params = imports.misc.params;
const Main = imports.ui.main;

var ICON_SIZE = 96;

var ANIMATION_TIME_IN = 350;
var ANIMATION_TIME_OUT = 1 / 2 * ANIMATION_TIME_IN;
var ANIMATION_MAX_DELAY_FOR_ITEM = 2 / 3 * ANIMATION_TIME_IN;
var ANIMATION_MAX_DELAY_OUT_FOR_ITEM = 2 / 3 * ANIMATION_TIME_OUT;
var ANIMATION_FADE_IN_TIME_FOR_ITEM = 1 / 4 * ANIMATION_TIME_IN;

var PAGE_SWITCH_TIME = 300;

var AnimationDirection = {
    IN: 0,
    OUT: 1,
};

var IconSize = {
    LARGE: 96,
    MEDIUM: 64,
    SMALL: 32,
    TINY: 16,
};

var APPICON_ANIMATION_OUT_SCALE = 3;
var APPICON_ANIMATION_OUT_TIME = 250;

const ICON_POSITION_DELAY = 10;

const defaultGridModes = [
    {
        rows: 8,
        columns: 3,
    },
    {
        rows: 6,
        columns: 4,
    },
    {
        rows: 4,
        columns: 6,
    },
    {
        rows: 3,
        columns: 8,
    },
];

var LEFT_DIVIDER_LEEWAY = 20;
var RIGHT_DIVIDER_LEEWAY = 20;

var DragLocation = {
    INVALID: 0,
    START_EDGE: 1,
    ON_ICON: 2,
    END_EDGE: 3,
    EMPTY_SPACE: 4,
};

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
    let containedX = Math.clamp(scaledX, monitor.x, monitor.x + monitor.width - scaledWidth);
    let containedY = Math.clamp(scaledY, monitor.y, monitor.y + monitor.height - scaledHeight);

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
        'fixed-icon-size': GObject.ParamSpec.int('fixed-icon-size',
            'Fixed icon size', 'Fixed icon size',
            GObject.ParamFlags.READWRITE,
            -1, GLib.MAXINT32, -1),
        'icon-size': GObject.ParamSpec.int('icon-size',
            'Icon size', 'Icon size',
            GObject.ParamFlags.READABLE,
            0, GLib.MAXINT32, 0),
        'last-row-align': GObject.ParamSpec.enum('last-row-align',
            'Last row align', 'Last row align',
            GObject.ParamFlags.READWRITE,
            Clutter.ActorAlign.$gtype,
            Clutter.ActorAlign.FILL),
        'max-column-spacing': GObject.ParamSpec.int('max-column-spacing',
            'Maximum column spacing', 'Maximum column spacing',
            GObject.ParamFlags.READWRITE,
            -1, GLib.MAXINT32, -1),
        'max-row-spacing': GObject.ParamSpec.int('max-row-spacing',
            'Maximum row spacing', 'Maximum row spacing',
            GObject.ParamFlags.READWRITE,
            -1, GLib.MAXINT32, -1),
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
}, class IconGridLayout extends Clutter.LayoutManager {
    _init(params = {}) {
        params = Params.parse(params, {
            allow_incomplete_pages: true,
            column_spacing: 0,
            columns_per_page: 6,
            fixed_icon_size: -1,
            last_row_align: Clutter.ActorAlign.FILL,
            max_column_spacing: -1,
            max_row_spacing: -1,
            orientation: Clutter.Orientation.VERTICAL,
            page_halign: Clutter.ActorAlign.FILL,
            page_valign: Clutter.ActorAlign.FILL,
            row_spacing: 0,
            rows_per_page: 4,
        });

        this._allowIncompletePages = params.allow_incomplete_pages;
        this._columnSpacing = params.column_spacing;
        this._columnsPerPage = params.columns_per_page;
        this._fixedIconSize = params.fixed_icon_size;
        this._lastRowAlign = params.last_row_align;
        this._maxColumnSpacing = params.max_column_spacing;
        this._maxRowSpacing = params.max_row_spacing;
        this._orientation = params.orientation;
        this._pageHAlign = params.page_halign;
        this._pageVAlign = params.page_valign;
        this._rowSpacing = params.row_spacing;
        this._rowsPerPage = params.rows_per_page;

        super._init(params);

        this._iconSize = this._fixedIconSize !== -1
            ? this._fixedIconSize
            : IconSize.LARGE;

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

        this._resolveOnIdleId = 0;
        this._iconSizeUpdateResolveCbs = [];
    }

    _findBestIconSize() {
        const nColumns = this._columnsPerPage;
        const nRows = this._rowsPerPage;
        const columnSpacingPerPage = this._columnSpacing * (nColumns - 1);
        const rowSpacingPerPage = this._rowSpacing * (nRows - 1);
        const [firstItem] = this._container;

        if (this._fixedIconSize !== -1)
            return this._fixedIconSize;

        const iconSizes = Object.values(IconSize).sort((a, b) => b - a);
        for (const size of iconSizes) {
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

            if (emptyHSpace >= 0 && emptyVSpace > 0)
                return size;
        }

        return IconSize.TINY;
    }

    _getChildrenMaxSize() {
        let minWidth = 0;
        let minHeight = 0;

        for (const child of this._container) {
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
        for (let i = 0; i < this._pages.length; i++)
            this._relocateSurplusItems(i);
    }

    _unlinkItem(item) {
        const itemData = this._items.get(item);

        item.disconnect(itemData.destroyId);
        item.disconnect(itemData.visibleId);

        this._items.delete(item);
    }

    _removePage(pageIndex) {
        // Make sure to not leave any icon left here
        this._pages[pageIndex].children.forEach(item => {
            this._unlinkItem(item);
        });

        // Adjust the page indexes of items after this page
        for (const itemData of this._items.values()) {
            if (itemData.pageIndex > pageIndex)
                itemData.pageIndex--;
        }

        this._pages.splice(pageIndex, 1);
        this.emit('pages-changed');
    }

    _fillItemVacancies(pageIndex) {
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
            const reducedItem = visibleNextPageItems[i];

            this._removeItemData(reducedItem);
            this._addItemToPage(reducedItem, pageIndex, -1);
        }
    }

    _removeItemData(item) {
        const itemData = this._items.get(item);
        const pageIndex = itemData.pageIndex;
        const page = this._pages[pageIndex];
        const itemIndex = page.children.indexOf(item);

        this._unlinkItem(item);

        page.children.splice(itemIndex, 1);

        // Delete the page if this is the last icon in it
        const visibleItems = this._getVisibleChildrenForPage(pageIndex);
        if (visibleItems.length === 0)
            this._removePage(pageIndex);

        if (!this._allowIncompletePages)
            this._fillItemVacancies(pageIndex);
    }

    _relocateSurplusItems(pageIndex) {
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

    _appendPage() {
        this._pages.push({ children: [] });
        this.emit('pages-changed');
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
                    this._relocateSurplusItems(itemData.pageIndex);
                else if (!this._allowIncompletePages)
                    this._fillItemVacancies(itemData.pageIndex);
            }),
        });

        item.icon.setIconSize(this._iconSize);

        this._pages[pageIndex].children.splice(index, 0, item);
        this._relocateSurplusItems(pageIndex);
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
            hSpacing = this._columnSpacing + emptyHSpace / (nColumns - 1);

            // Maybe constraint horizontal spacing
            if (this._maxColumnSpacing !== -1 && hSpacing > this._maxColumnSpacing) {
                const extraHSpacing =
                    (this._maxColumnSpacing - this._columnSpacing) * (nColumns - 1);

                hSpacing = this._maxColumnSpacing;
                leftEmptySpace =
                    Math.max((emptyHSpace - extraHSpacing) / 2, 0);
            }
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
            vSpacing = this._rowSpacing + emptyVSpace / (nRows - 1);

            // Maybe constraint vertical spacing
            if (this._maxRowSpacing !== -1 && vSpacing > this._maxRowSpacing) {
                const extraVSpacing =
                    (this._maxRowSpacing - this._rowSpacing) * (nRows - 1);

                vSpacing = this._maxRowSpacing;
                topEmptySpace =
                    Math.max((emptyVSpace - extraVSpacing) / 2, 0);
            }

            break;
        }

        return [leftEmptySpace, topEmptySpace, hSpacing, vSpacing];
    }

    _getRowPadding(items, itemIndex, childSize, spacing) {
        const nRows = Math.ceil(items.length / this._columnsPerPage);

        let rowAlign = 0;
        const row = Math.floor(itemIndex / this._columnsPerPage);

        // Only apply to the last row
        if (row < nRows - 1)
            return 0;

        const rowStart = row * this._columnsPerPage;
        const rowEnd = Math.min((row + 1) * this._columnsPerPage - 1, items.length - 1);
        const itemsInThisRow = rowEnd - rowStart + 1;
        const nEmpty = this._columnsPerPage - itemsInThisRow;
        const availableWidth = nEmpty * (spacing + childSize);

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

    _runPostAllocation() {
        if (this._iconSizeUpdateResolveCbs.length > 0 &&
            this._resolveOnIdleId === 0) {
            this._resolveOnIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this._iconSizeUpdateResolveCbs.forEach(cb => cb());
                this._iconSizeUpdateResolveCbs = [];
                this._resolveOnIdleId = 0;
                return GLib.SOURCE_REMOVE;
            });
        }
    }

    _onDestroy() {
        if (this._updateIconSizesLaterId >= 0) {
            Meta.later_remove(this._updateIconSizesLaterId);
            this._updateIconSizesLaterId = 0;
        }

        if (this._resolveOnIdleId > 0) {
            GLib.source_remove(this._resolveOnIdleId);
            delete this._resolveOnIdleId;
        }
    }

    vfunc_set_container(container) {
        if (this._container)
            this._container.disconnect(this._containerDestroyedId);

        this._container = container;

        if (this._container)
            this._containerDestroyedId = this._container.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_get_preferred_width(_container, _forHeight) {
        let minWidth = -1;
        let natWidth = -1;

        switch (this._orientation) {
        case Clutter.Orientation.VERTICAL:
            minWidth = IconSize.TINY;
            natWidth = this._pageWidth;
            break;

        case Clutter.Orientation.HORIZONTAL:
            minWidth = this._pageWidth * this._pages.length;
            natWidth = minWidth;
            break;
        }

        return [minWidth, natWidth];
    }

    vfunc_get_preferred_height(_container, _forWidth) {
        let minHeight = -1;
        let natHeight = -1;

        switch (this._orientation) {
        case Clutter.Orientation.VERTICAL:
            minHeight = this._pageHeight * this._pages.length;
            natHeight = minHeight;
            break;

        case Clutter.Orientation.HORIZONTAL:
            minHeight = IconSize.TINY;
            natHeight = this._pageHeight;
            break;
        }

        return [minHeight, natHeight];
    }

    vfunc_allocate() {
        if (this._pageWidth === 0 || this._pageHeight === 0)
            throw new Error('IconGridLayout.adaptToSize wasn\'t called before allocation');

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

                const rowPadding = this._getRowPadding(visibleItems, itemIndex,
                    childSize, hSpacing);

                // Icon position
                let x = leftEmptySpace + rowPadding + column * (childSize + hSpacing);
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

                childBox.set_origin(Math.floor(x), Math.floor(y));

                // Only ease icons when the page size didn't change
                if (this._pageSizeChanged)
                    item.allocate(childBox);
                else if (animateIconPosition(item, childBox, nChangedIcons))
                    nChangedIcons++;
            });
        });

        this._pageSizeChanged = false;

        this._runPostAllocation();
    }

    /**
     * addItem:
     * @param {Clutter.Actor} item: item to append to the grid
     * @param {int} page: page number
     * @param {int} index: position in the page
     *
     * Adds @item to the grid. @item must not be part of the grid.
     *
     * If @index exceeds the number of items per page, @item will
     * be added to the next page.
     *
     * @page must be a number between 0 and the number of pages.
     * Adding to the page after next will create a new page.
     */
    addItem(item, page = -1, index = -1) {
        if (this._items.has(item))
            throw new Error(`Item ${item} already added to IconGridLayout`);

        if (page > this._pages.length)
            throw new Error(`Cannot add ${item} to page ${page}`);

        if (!this._container)
            return;

        this._container.add_child(item);
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
     * moveItem:
     * @param {Clutter.Actor} item: item to move
     * @param {int} newPage: new page of the item
     * @param {int} newPosition: new page of the item
     *
     * Moves @item to the grid. @item must be part of the grid.
     */
    moveItem(item, newPage, newPosition) {
        if (!this._items.has(item))
            throw new Error(`Item ${item} is not part of the IconGridLayout`);

        this._removeItemData(item);
        this._addItemToPage(item, newPage, newPosition);
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

        if (!this._container)
            return;

        this._container.remove_child(item);
        this._removeItemData(item);
    }

    /**
     * getItemsAtPage:
     * @param {int} pageIndex: page index
     *
     * Retrieves the children at page @pageIndex. Children may be invisible.
     *
     * @returns {Array} an array of {Clutter.Actor}s
     */
    getItemsAtPage(pageIndex) {
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

    ensureIconSizeUpdated() {
        if (this._updateIconSizesLaterId === 0)
            return Promise.resolve();

        return new Promise(
            resolve => this._iconSizeUpdateResolveCbs.push(resolve));
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

                        for (const child of this._container)
                            child.icon.setIconSize(iconSize);

                        this.notify('icon-size');
                    }

                    this._updateIconSizesLaterId = 0;
                    return GLib.SOURCE_REMOVE;
                });
        }
    }

    /**
     * getDropTarget:
     * @param {int} x: position of the horizontal axis
     * @param {int} y: position of the vertical axis
     *
     * Retrieves the item located at (@x, @y), as well as the drag location.
     * Both @x and @y are relative to the grid.
     *
     * @returns {[Clutter.Actor, DragLocation]} the item and drag location
     * under (@x, @y)
     */
    getDropTarget(x, y) {
        const childSize = this._getChildrenMaxSize();
        const [leftEmptySpace, topEmptySpace, hSpacing, vSpacing] =
            this._calculateSpacing(childSize);

        const isRtl =
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;

        let page = this._orientation === Clutter.Orientation.VERTICAL
            ? Math.floor(y / this._pageHeight)
            : Math.floor(x / this._pageWidth);

        // Out of bounds
        if (page >= this._pages.length)
            return [null, DragLocation.INVALID];

        if (isRtl && this._orientation === Clutter.Orientation.HORIZONTAL)
            page = swap(page, this._pages.length);

        // Page-relative coordinates from now on
        x %= this._pageWidth;
        y %= this._pageHeight;

        if (x < leftEmptySpace || y < topEmptySpace)
            return [null, DragLocation.INVALID];

        const gridWidth =
            childSize * this._columnsPerPage +
            hSpacing * (this._columnsPerPage - 1);
        const gridHeight =
            childSize * this._rowsPerPage +
            vSpacing * (this._rowsPerPage - 1);

        if (x > leftEmptySpace + gridWidth || y > topEmptySpace + gridHeight)
            return [null, DragLocation.INVALID];

        const halfHSpacing = hSpacing / 2;
        const halfVSpacing = vSpacing / 2;
        const visibleItems = this._getVisibleChildrenForPage(page);

        for (const item of visibleItems) {
            const childBox = item.allocation.copy();

            // Page offset
            switch (this._orientation) {
            case Clutter.Orientation.HORIZONTAL:
                childBox.set_origin(childBox.x1 % this._pageWidth, childBox.y1);
                break;
            case Clutter.Orientation.VERTICAL:
                childBox.set_origin(childBox.x1, childBox.y1 % this._pageHeight);
                break;
            }

            // Outside the icon boundaries
            if (x < childBox.x1 - halfHSpacing ||
                x > childBox.x2 + halfHSpacing ||
                y < childBox.y1 - halfVSpacing ||
                y > childBox.y2 + halfVSpacing)
                continue;

            let dragLocation;

            if (x < childBox.x1 + LEFT_DIVIDER_LEEWAY)
                dragLocation = DragLocation.START_EDGE;
            else if (x > childBox.x2 - RIGHT_DIVIDER_LEEWAY)
                dragLocation = DragLocation.END_EDGE;
            else
                dragLocation = DragLocation.ON_ICON;

            if (isRtl) {
                if (dragLocation === DragLocation.START_EDGE)
                    dragLocation = DragLocation.END_EDGE;
                else if (dragLocation === DragLocation.END_EDGE)
                    dragLocation = DragLocation.START_EDGE;
            }

            return [item, dragLocation];
        }

        return [null, DragLocation.EMPTY_SPACE];
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
    get fixed_icon_size() {
        return this._fixedIconSize;
    }

    // eslint-disable-next-line camelcase
    set fixed_icon_size(v) {
        if (this._fixedIconSize === v)
            return;

        this._fixedIconSize = v;
        this.notify('fixed-icon-size');
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
    get max_column_spacing() {
        return this._maxColumnSpacing;
    }

    // eslint-disable-next-line camelcase
    set max_column_spacing(v) {
        if (this._maxColumnSpacing === v)
            return;

        this._maxColumnSpacing = v;
        this.notify('max-column-spacing');
    }

    // eslint-disable-next-line camelcase
    get max_row_spacing() {
        return this._maxRowSpacing;
    }

    // eslint-disable-next-line camelcase
    set max_row_spacing(v) {
        if (this._maxRowSpacing === v)
            return;

        this._maxRowSpacing = v;
        this.notify('max-row-spacing');
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
});

var IconGrid = GObject.registerClass({
    Signals: {
        'pages-changed': {},
        'animation-done': {},
    },
}, class IconGrid extends St.Viewport {
    _init(layoutParams = {}) {
        layoutParams = Params.parse(layoutParams, {
            allow_incomplete_pages: false,
            orientation: Clutter.Orientation.VERTICAL,
            columns_per_page: 6,
            rows_per_page: 4,
            page_halign: Clutter.ActorAlign.CENTER,
            page_valign: Clutter.ActorAlign.CENTER,
            last_row_align: Clutter.ActorAlign.START,
            column_spacing: 0,
            row_spacing: 0,
        });
        const layoutManager = new IconGridLayout(layoutParams);
        const pagesChangedId = layoutManager.connect('pages-changed',
            () => this.emit('pages-changed'));

        super._init({
            style_class: 'icon-grid',
            layoutManager,
            x_expand: true,
            y_expand: true,
        });

        this._gridModes = defaultGridModes;
        this._currentPage = 0;
        this._currentMode = -1;
        this._clonesAnimating = [];

        this.connect('actor-added', this._childAdded.bind(this));
        this.connect('actor-removed', this._childRemoved.bind(this));
        this.connect('destroy', () => layoutManager.disconnect(pagesChangedId));
    }

    _getChildrenToAnimate() {
        const layoutManager = this.layout_manager;
        const children = layoutManager.getItemsAtPage(this._currentPage);

        return children.filter(c => c.visible);
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

    _childAdded(grid, child) {
        child._iconGridKeyFocusInId = child.connect('key-focus-in', () => {
            this._ensureItemIsVisible(child);
        });

        child._paintVisible = child.opacity > 0;
        child._opacityChangedId = child.connect('notify::opacity', () => {
            let paintVisible = child._paintVisible;
            child._paintVisible = child.opacity > 0;
            if (paintVisible !== child._paintVisible)
                this.queue_relayout();
        });
    }

    _ensureItemIsVisible(item) {
        if (!this.contains(item))
            throw new Error(`${item} is not a child of IconGrid`);

        const itemPage = this.layout_manager.getItemPage(item);
        this.goToPage(itemPage);
    }

    _setGridMode(modeIndex) {
        if (this._currentMode === modeIndex)
            return;

        this._currentMode = modeIndex;

        if (modeIndex !== -1) {
            const newMode = this._gridModes[modeIndex];

            this.layout_manager.rows_per_page = newMode.rows;
            this.layout_manager.columns_per_page = newMode.columns;
        }
    }

    _findBestModeForSize(width, height) {
        const sizeRatio = width / height;
        let closestRatio = Infinity;
        let bestMode = -1;

        for (let modeIndex in this._gridModes) {
            const mode = this._gridModes[modeIndex];
            const modeRatio = mode.columns / mode.rows;

            if (Math.abs(sizeRatio - modeRatio) < Math.abs(sizeRatio - closestRatio)) {
                closestRatio = modeRatio;
                bestMode = modeIndex;
            }
        }

        this._setGridMode(bestMode);
    }

    _childRemoved(grid, child) {
        child.disconnect(child._iconGridKeyFocusInId);
        delete child._iconGridKeyFocusInId;

        child.disconnect(child._opacityChangedId);
        delete child._opacityChangedId;
        delete child._paintVisible;
    }

    vfunc_unmap() {
        // Cancel animations when hiding the overview, to avoid icons
        // swarming into the void ...
        this._resetAnimationActors();
        super.vfunc_unmap();
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();

        const node = this.get_theme_node();
        this.layout_manager.column_spacing = node.get_length('column-spacing');
        this.layout_manager.row_spacing = node.get_length('row-spacing');

        let [found, value] = node.lookup_length('max-column-spacing', false);
        this.layout_manager.max_column_spacing = found ? value : -1;

        [found, value] = node.lookup_length('max-row-spacing', false);
        this.layout_manager.max_row_spacing = found ? value : -1;
    }

    /**
     * addItem:
     * @param {Clutter.Actor} item: item to append to the grid
     * @param {int} page: page number
     * @param {int} index: position in the page
     *
     * Adds @item to the grid. @item must not be part of the grid.
     *
     * If @index exceeds the number of items per page, @item will
     * be added to the next page.
     *
     * @page must be a number between 0 and the number of pages.
     * Adding to the page after next will create a new page.
     */
    addItem(item, page = -1, index = -1) {
        if (!(item.icon instanceof BaseIcon))
            throw new Error('Only items with a BaseIcon icon property can be added to IconGrid');

        this.layout_manager.addItem(item, page, index);
    }

    /**
     * appendItem:
     * @param {Clutter.Actor} item: item to append to the grid
     *
     * Appends @item to the grid. @item must not be part of the grid.
     */
    appendItem(item) {
        this.layout_manager.appendItem(item);
    }

    /**
     * moveItem:
     * @param {Clutter.Actor} item: item to move
     * @param {int} newPage: new page of the item
     * @param {int} newPosition: new page of the item
     *
     * Moves @item to the grid. @item must be part of the grid.
     */
    moveItem(item, newPage, newPosition) {
        this.layout_manager.moveItem(item, newPage, newPosition);
        this.queue_relayout();
    }

    /**
     * removeItem:
     * @param {Clutter.Actor} item: item to remove from the grid
     *
     * Removes @item to the grid. @item must be part of the grid.
     */
    removeItem(item) {
        if (!this.contains(item))
            throw new Error(`Item ${item} is not part of the IconGrid`);

        this.layout_manager.removeItem(item);
    }

    /**
     * goToPage:
     * @param {int} pageIndex: page index
     * @param {boolean} animate: animate the page transition
     *
     * Moves the current page to @pageIndex. @pageIndex must be a valid page
     * number.
     */
    goToPage(pageIndex, animate = true) {
        if (pageIndex >= this.nPages)
            throw new Error(`IconGrid does not have page ${pageIndex}`);

        let newValue;
        let adjustment;
        switch (this.layout_manager.orientation) {
        case Clutter.Orientation.VERTICAL:
            adjustment = this.vadjustment;
            newValue = pageIndex * this.layout_manager.pageHeight;
            break;
        case Clutter.Orientation.HORIZONTAL:
            adjustment = this.hadjustment;
            newValue = pageIndex * this.layout_manager.pageWidth;
            break;
        }

        this._currentPage = pageIndex;

        if (!this.mapped)
            animate = false;

        adjustment.ease(newValue, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration: animate ? PAGE_SWITCH_TIME : 0,
        });
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
        return this.layout_manager.getItemPage(item);
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
        if (!this.contains(item))
            return [-1, -1];

        const layoutManager = this.layout_manager;
        return layoutManager.getItemPosition(item);
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
        const layoutManager = this.layout_manager;
        return layoutManager.getItemAt(page, position);
    }

    /**
     * getItemsAtPage:
     * @param {int} page: the page index
     *
     * Retrieves the children at page @page, including invisible children.
     *
     * @returns {Array} an array of {Clutter.Actor}s
     */
    getItemsAtPage(page) {
        if (page < 0 || page > this.nPages)
            throw new Error(`Page ${page} does not exist at IconGrid`);

        const layoutManager = this.layout_manager;
        return layoutManager.getItemsAtPage(page);
    }

    get currentPage() {
        return this._currentPage;
    }

    set currentPage(v) {
        this.goToPage(v);
    }

    get nPages() {
        return this.layout_manager.nPages;
    }

    adaptToSize(width, height) {
        this._findBestModeForSize(width, height);
        this.layout_manager.adaptToSize(width, height);
    }

    async animateSpring(animationDirection, sourceActor) {
        this._resetAnimationActors();

        let actors = this._getChildrenToAnimate();
        if (actors.length == 0) {
            this._animationDone();
            return;
        }

        await this.layout_manager.ensureIconSizeUpdated();

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
            const actor = actorClone.source;
            actor.opacity = 0;
            actor.reactive = false;

            let [width, height] = actor.get_size();
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

    setGridModes(modes) {
        this._gridModes = modes ? modes : defaultGridModes;
        this.queue_relayout();
    }

    getDropTarget(x, y) {
        const layoutManager = this.layout_manager;
        return layoutManager.getDropTarget(x, y, this._currentPage);
    }

    get itemsPerPage() {
        const layoutManager = this.layout_manager;
        return layoutManager.rows_per_page * layoutManager.columns_per_page;
    }
});
