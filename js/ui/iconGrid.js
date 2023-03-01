// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BaseIcon, IconGrid, IconGridLayout */

const { Clutter, GLib, GObject, Meta, Shell, St } = imports.gi;

const Params = imports.misc.params;
const Main = imports.ui.main;

var ICON_SIZE = 96;

var PAGE_SWITCH_TIME = 300;

var IconSize = {
    LARGE: 96,
    MEDIUM: 64,
    MEDIUM_SMALL: 48,
    SMALL: 32,
    SMALLER: 24,
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
class BaseIcon extends Shell.SquareBin {
    _init(label, params) {
        params = Params.parse(params, {
            createIcon: null,
            setSizeManually: false,
            showLabel: true,
        });

        let styleClass = 'overview-icon';
        if (params.showLabel)
            styleClass += ' overview-icon-with-label';

        super._init({ style_class: styleClass });

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
        cache.connectObject(
            'icon-theme-changed', this._onIconThemeChanged.bind(this), this);
    }

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon(_size) {
        throw new GObject.NotImplementedError(`createIcon in ${this.constructor.name}`);
    }

    setIconSize(size) {
        if (!this._setSizeManually)
            throw new Error('setSizeManually has to be set to use setIconsize');

        if (size === this.iconSize)
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

        if (this.iconSize === size && this._iconBin.child)
            return;

        this._createIconTexture(size);
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
    const monitor = Main.layoutManager.findMonitorForActor(actor);
    if (!monitor)
        return;

    const actorClone = new Clutter.Clone({
        source: actor,
        reactive: false,
    });
    let [width, height] = actor.get_transformed_size();

    actorClone.set_size(width, height);
    actorClone.set_position(x, y);
    actorClone.opacity = 255;
    actorClone.set_pivot_point(0.5, 0.5);

    Main.uiGroup.add_actor(actorClone);

    // Avoid monitor edges to not zoom outside the current monitor
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
            1, GLib.MAXINT32, 6),
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
        'page-padding': GObject.ParamSpec.boxed('page-padding',
            'Page padding', 'Page padding',
            GObject.ParamFlags.READWRITE,
            Clutter.Margin.$gtype),
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
            1, GLib.MAXINT32, 4),
    },
    Signals: {
        'pages-changed': {},
    },
}, class IconGridLayout extends Clutter.LayoutManager {
    _init(params = {}) {
        this._orientation = params.orientation ?? Clutter.Orientation.VERTICAL;

        super._init(params);

        if (!this.pagePadding)
            this.pagePadding = new Clutter.Margin();

        this._iconSize = this.fixedIconSize !== -1
            ? this.fixedIconSize
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

        this._childrenMaxSize = -1;
    }

    _findBestIconSize() {
        const nColumns = this.columnsPerPage;
        const nRows = this.rowsPerPage;
        const columnSpacingPerPage = this.columnSpacing * (nColumns - 1);
        const rowSpacingPerPage = this.rowSpacing * (nRows - 1);
        const [firstItem] = this._container;

        if (this.fixedIconSize !== -1)
            return this.fixedIconSize;

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
                this._pageWidth - usedWidth - columnSpacingPerPage -
                this.pagePadding.left - this.pagePadding.right;
            const emptyVSpace =
                this._pageHeight - usedHeight -  rowSpacingPerPage -
                this.pagePadding.top - this.pagePadding.bottom;

            if (emptyHSpace >= 0 && emptyVSpace > 0)
                return size;
        }

        return IconSize.TINY;
    }

    _getChildrenMaxSize() {
        if (this._childrenMaxSize === -1) {
            let minWidth = 0;
            let minHeight = 0;

            const nPages = this._pages.length;
            for (let pageIndex = 0; pageIndex < nPages; pageIndex++) {
                const page = this._pages[pageIndex];
                const nVisibleItems = page.visibleChildren.length;
                for (let itemIndex = 0; itemIndex < nVisibleItems; itemIndex++) {
                    const item = page.visibleChildren[itemIndex];

                    const childMinHeight = item.get_preferred_height(-1)[0];
                    const childMinWidth = item.get_preferred_width(-1)[0];

                    minWidth = Math.max(minWidth, childMinWidth);
                    minHeight = Math.max(minHeight, childMinHeight);
                }
            }

            this._childrenMaxSize = Math.max(minWidth, minHeight);
        }

        return this._childrenMaxSize;
    }

    _updateVisibleChildrenForPage(pageIndex) {
        this._pages[pageIndex].visibleChildren =
            this._pages[pageIndex].children.filter(actor => actor.visible);
    }

    _updatePages() {
        for (let i = 0; i < this._pages.length; i++)
            this._relocateSurplusItems(i);
    }

    _unlinkItem(item) {
        const itemData = this._items.get(item);

        item.disconnect(itemData.destroyId);
        item.disconnect(itemData.visibleId);
        item.disconnect(itemData.queueRelayoutId);

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

        const visiblePageItems = this._pages[pageIndex].visibleChildren;
        const itemsPerPage = this.columnsPerPage * this.rowsPerPage;

        // No reduce needed
        if (visiblePageItems.length === itemsPerPage)
            return;

        const visibleNextPageItems = this._pages[pageIndex + 1].visibleChildren;
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

        this._updateVisibleChildrenForPage(pageIndex);

        // Delete the page if this is the last icon in it
        const visibleItems = this._pages[pageIndex].visibleChildren;
        if (visibleItems.length === 0)
            this._removePage(pageIndex);

        if (!this.allowIncompletePages)
            this._fillItemVacancies(pageIndex);
    }

    _relocateSurplusItems(pageIndex) {
        const visiblePageItems = this._pages[pageIndex].visibleChildren;
        const itemsPerPage = this.columnsPerPage * this.rowsPerPage;

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

                this._updateVisibleChildrenForPage(itemData.pageIndex);

                if (item.visible)
                    this._relocateSurplusItems(itemData.pageIndex);
                else if (!this.allowIncompletePages)
                    this._fillItemVacancies(itemData.pageIndex);
            }),
            queueRelayoutId: item.connect('queue-relayout', () => {
                this._childrenMaxSize = -1;
            }),
        });

        item.icon.setIconSize(this._iconSize);

        this._pages[pageIndex].children.splice(index, 0, item);
        this._updateVisibleChildrenForPage(pageIndex);
        this._relocateSurplusItems(pageIndex);
    }

    _calculateSpacing(childSize) {
        const nColumns = this.columnsPerPage;
        const nRows = this.rowsPerPage;
        const usedWidth = childSize * nColumns;
        const usedHeight = childSize * nRows;
        const columnSpacingPerPage = this.columnSpacing * (nColumns - 1);
        const rowSpacingPerPage = this.rowSpacing * (nRows - 1);

        const emptyHSpace =
            this._pageWidth - usedWidth - columnSpacingPerPage -
            this.pagePadding.left - this.pagePadding.right;
        const emptyVSpace =
            this._pageHeight - usedHeight -  rowSpacingPerPage -
            this.pagePadding.top - this.pagePadding.bottom;
        let leftEmptySpace = this.pagePadding.left;
        let topEmptySpace = this.pagePadding.top;
        let hSpacing;
        let vSpacing;

        switch (this.pageHalign) {
        case Clutter.ActorAlign.START:
            hSpacing = this.columnSpacing;
            break;
        case Clutter.ActorAlign.CENTER:
            leftEmptySpace += Math.floor(emptyHSpace / 2);
            hSpacing = this.columnSpacing;
            break;
        case Clutter.ActorAlign.END:
            leftEmptySpace += emptyHSpace;
            hSpacing = this.columnSpacing;
            break;
        case Clutter.ActorAlign.FILL:
            hSpacing = this.columnSpacing + emptyHSpace / (nColumns - 1);

            // Maybe constraint horizontal spacing
            if (this.maxColumnSpacing !== -1 && hSpacing > this.maxColumnSpacing) {
                const extraHSpacing =
                    (this.maxColumnSpacing - this.columnSpacing) * (nColumns - 1);

                hSpacing = this.maxColumnSpacing;
                leftEmptySpace +=
                    Math.max((emptyHSpace - extraHSpacing) / 2, 0);
            }
            break;
        }

        switch (this.pageValign) {
        case Clutter.ActorAlign.START:
            vSpacing = this.rowSpacing;
            break;
        case Clutter.ActorAlign.CENTER:
            topEmptySpace += Math.floor(emptyVSpace / 2);
            vSpacing = this.rowSpacing;
            break;
        case Clutter.ActorAlign.END:
            topEmptySpace += emptyVSpace;
            vSpacing = this.rowSpacing;
            break;
        case Clutter.ActorAlign.FILL:
            vSpacing = this.rowSpacing + emptyVSpace / (nRows - 1);

            // Maybe constraint vertical spacing
            if (this.maxRowSpacing !== -1 && vSpacing > this.maxRowSpacing) {
                const extraVSpacing =
                    (this.maxRowSpacing - this.rowSpacing) * (nRows - 1);

                vSpacing = this.maxRowSpacing;
                topEmptySpace +=
                    Math.max((emptyVSpace - extraVSpacing) / 2, 0);
            }

            break;
        }

        return [leftEmptySpace, topEmptySpace, hSpacing, vSpacing];
    }

    _getRowPadding(align, items, itemIndex, childSize, spacing) {
        if (align === Clutter.ActorAlign.START ||
            align === Clutter.ActorAlign.FILL)
            return 0;

        const nRows = Math.ceil(items.length / this.columnsPerPage);

        let rowAlign = 0;
        const row = Math.floor(itemIndex / this.columnsPerPage);

        // Only apply to the last row
        if (row < nRows - 1)
            return 0;

        const rowStart = row * this.columnsPerPage;
        const rowEnd = Math.min((row + 1) * this.columnsPerPage - 1, items.length - 1);
        const itemsInThisRow = rowEnd - rowStart + 1;
        const nEmpty = this.columnsPerPage - itemsInThisRow;
        const availableWidth = nEmpty * (spacing + childSize);

        const isRtl =
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;

        switch (align) {
        case Clutter.ActorAlign.CENTER:
            rowAlign = availableWidth / 2;
            break;
        case Clutter.ActorAlign.END:
            rowAlign = availableWidth;
            break;
        // START and FILL align are handled at the beginning of the function
        }

        return isRtl ? rowAlign * -1 : rowAlign;
    }

    _onDestroy() {
        if (this._updateIconSizesLaterId >= 0) {
            const laters = global.compositor.get_laters();
            laters.remove(this._updateIconSizesLaterId);
            this._updateIconSizesLaterId = 0;
        }
    }

    vfunc_set_container(container) {
        this._container?.disconnectObject(this);

        this._container = container;

        if (this._container)
            this._container.connectObject('destroy', this._onDestroy.bind(this), this);
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

        const isRtl =
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL;
        const childSize = this._getChildrenMaxSize();

        const [leftEmptySpace, topEmptySpace, hSpacing, vSpacing] =
            this._calculateSpacing(childSize);

        const childBox = new Clutter.ActorBox();

        let nChangedIcons = 0;
        const columnsPerPage = this.columnsPerPage;
        const orientation = this._orientation;
        const pageWidth = this._pageWidth;
        const pageHeight = this._pageHeight;
        const pageSizeChanged = this._pageSizeChanged;
        const lastRowAlign = this.lastRowAlign;
        const shouldEaseItems = this._shouldEaseItems;

        this._pages.forEach((page, pageIndex) => {
            if (isRtl && orientation === Clutter.Orientation.HORIZONTAL)
                pageIndex = swap(pageIndex, this._pages.length);

            page.visibleChildren.forEach((item, itemIndex) => {
                const row = Math.floor(itemIndex / columnsPerPage);
                let column = itemIndex % columnsPerPage;

                if (isRtl)
                    column = swap(column, columnsPerPage);

                const rowPadding = this._getRowPadding(lastRowAlign,
                    page.visibleChildren, itemIndex, childSize, hSpacing);

                // Icon position
                let x = leftEmptySpace + rowPadding + column * (childSize + hSpacing);
                let y = topEmptySpace + row * (childSize + vSpacing);

                // Page start
                switch (orientation) {
                case Clutter.Orientation.HORIZONTAL:
                    x += pageIndex * pageWidth;
                    break;
                case Clutter.Orientation.VERTICAL:
                    y += pageIndex * pageHeight;
                    break;
                }

                childBox.set_origin(Math.floor(x), Math.floor(y));

                const [,, naturalWidth, naturalHeight] = item.get_preferred_size();
                childBox.set_size(
                    Math.max(childSize, naturalWidth),
                    Math.max(childSize, naturalHeight));

                if (!shouldEaseItems || pageSizeChanged)
                    item.allocate(childBox);
                else if (animateIconPosition(item, childBox, nChangedIcons))
                    nChangedIcons++;
            });
        });

        this._pageSizeChanged = false;
        this._shouldEaseItems = false;
    }

    _findBestPageToAppend(startPage) {
        const itemsPerPage = this.columnsPerPage * this.rowsPerPage;

        for (let i = startPage; i < this._pages.length; i++) {
            const visibleItems = this._pages[i].visibleChildren;

            if (visibleItems.length < itemsPerPage)
                return i;
        }

        return this._pages.length;
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

        if (page !== -1 && index === -1)
            page = this._findBestPageToAppend(page);

        this._shouldEaseItems = true;

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

        this._shouldEaseItems = true;

        this._removeItemData(item);

        if (newPage !== -1 && newPosition === -1)
            newPage = this._findBestPageToAppend(newPage);

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

        this._shouldEaseItems = true;

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
        const visibleItems = this._pages[itemData.pageIndex].visibleChildren;

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

        const visibleItems = this._pages[page].visibleChildren;

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
            const laters = global.compositor.get_laters();
            this._updateIconSizesLaterId =
                laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
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
            return [0, 0, DragLocation.INVALID];

        if (isRtl && this._orientation === Clutter.Orientation.HORIZONTAL)
            page = swap(page, this._pages.length);

        // Get page-relative coordinates
        let adjX = x;
        let adjY = y;
        if (this._orientation === Clutter.Orientation.HORIZONTAL)
            adjX %= this._pageWidth;
        else
            adjY %= this._pageHeight;

        const gridWidth =
            childSize * this.columnsPerPage +
            hSpacing * (this.columnsPerPage - 1);
        const gridHeight =
            childSize * this.rowsPerPage +
            vSpacing * (this.rowsPerPage - 1);

        const inTopEmptySpace = adjY < topEmptySpace;
        const inLeftEmptySpace = adjX < leftEmptySpace;
        const inRightEmptySpace = adjX > leftEmptySpace + gridWidth;
        const inBottomEmptySpace = adjY > topEmptySpace + gridHeight;

        if (inTopEmptySpace || inBottomEmptySpace)
            return [0, 0, DragLocation.INVALID];

        const halfHSpacing = hSpacing / 2;
        const halfVSpacing = vSpacing / 2;
        const visibleItems = this._pages[page].visibleChildren;

        for (let i = 0; i < visibleItems.length; i++) {
            const item = visibleItems[i];
            const childBox = item.allocation;

            const firstInRow = i % this.columnsPerPage === 0;
            const lastInRow = i % this.columnsPerPage === this.columnsPerPage - 1;

            // Check icon boundaries
            if ((inLeftEmptySpace && firstInRow) ||
                (inRightEmptySpace && lastInRow)) {
                if (y < childBox.y1 - halfVSpacing ||
                    y > childBox.y2 + halfVSpacing)
                    continue;
            } else {
                // eslint-disable-next-line no-lonely-if
                if (x < childBox.x1 - halfHSpacing ||
                    x > childBox.x2 + halfHSpacing ||
                    y < childBox.y1 - halfVSpacing ||
                    y > childBox.y2 + halfVSpacing)
                    continue;
            }

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

            return [page, i, dragLocation];
        }

        return [page, -1, DragLocation.EMPTY_SPACE];
    }

    get iconSize() {
        return this._iconSize;
    }

    get nPages() {
        return this._pages.length;
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
    },
}, class IconGrid extends St.Viewport {
    _init(layoutParams = {}) {
        layoutParams = Params.parse(layoutParams, {
            allow_incomplete_pages: false,
            orientation: Clutter.Orientation.HORIZONTAL,
            columns_per_page: 6,
            rows_per_page: 4,
            page_halign: Clutter.ActorAlign.FILL,
            page_padding: new Clutter.Margin(),
            page_valign: Clutter.ActorAlign.FILL,
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

        this.connect('actor-added', this._childAdded.bind(this));
        this.connect('actor-removed', this._childRemoved.bind(this));
        this.connect('destroy', () => layoutManager.disconnect(pagesChangedId));
    }

    _childAdded(grid, child) {
        child._iconGridKeyFocusInId = child.connect('key-focus-in', () => {
            this._ensureItemIsVisible(child);
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
        const { pagePadding } = this.layout_manager;
        width -= pagePadding.left + pagePadding.right;
        height -= pagePadding.top + pagePadding.bottom;

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
    }

    vfunc_allocate(box) {
        const [width, height] = box.get_size();
        this._findBestModeForSize(width, height);
        this.layout_manager.adaptToSize(width, height);
        super.vfunc_allocate(box);
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

        const padding = new Clutter.Margin();
        ['top', 'right', 'bottom', 'left'].forEach(side => {
            padding[side] = node.get_length(`page-padding-${side}`);
        });
        this.layout_manager.page_padding = padding;
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

        global.compositor.get_laters().add(
            Meta.LaterType.BEFORE_REDRAW, () => {
                adjustment.ease(newValue, {
                    mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
                    duration: animate ? PAGE_SWITCH_TIME : 0,
                });
                return GLib.SOURCE_REMOVE;
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
