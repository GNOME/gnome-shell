// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AppDisplay, AppSearchProvider */

const { Clutter, Gio, GLib, GObject, Graphene, Meta,
    Pango, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppFavorites = imports.ui.appFavorites;
const BoxPointer = imports.ui.boxpointer;
const DND = imports.ui.dnd;
const GrabHelper = imports.ui.grabHelper;
const IconGrid = imports.ui.iconGrid;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const PageIndicators = imports.ui.pageIndicators;
const ParentalControlsManager = imports.misc.parentalControlsManager;
const PopupMenu = imports.ui.popupMenu;
const Search = imports.ui.search;
const SwipeTracker = imports.ui.swipeTracker;
const Params = imports.misc.params;
const SystemActions = imports.misc.systemActions;

var MENU_POPUP_TIMEOUT = 600;
var POPDOWN_DIALOG_TIMEOUT = 500;

var FOLDER_SUBICON_FRACTION = .4;

var VIEWS_SWITCH_TIME = 400;
var VIEWS_SWITCH_ANIMATION_DELAY = 100;

var SCROLL_TIMEOUT_TIME = 150;

var APP_ICON_SCALE_IN_TIME = 500;
var APP_ICON_SCALE_IN_DELAY = 700;

var APP_ICON_TITLE_EXPAND_TIME = 200;
var APP_ICON_TITLE_COLLAPSE_TIME = 100;

const FOLDER_DIALOG_ANIMATION_TIME = 200;

const PAGE_PREVIEW_ANIMATION_TIME = 150;
const PAGE_PREVIEW_ANIMATION_START_OFFSET = 100;
const PAGE_PREVIEW_FADE_EFFECT_MAX_OFFSET = 300;
const PAGE_PREVIEW_MAX_ARROW_OFFSET = 80;
const PAGE_INDICATOR_FADE_TIME = 200;
const MAX_PAGE_PADDING = 200;

const OVERSHOOT_THRESHOLD = 20;
const OVERSHOOT_TIMEOUT = 1000;

const DELAYED_MOVE_TIMEOUT = 200;

const DIALOG_SHADE_NORMAL = Clutter.Color.from_pixel(0x000000cc);
const DIALOG_SHADE_HIGHLIGHT = Clutter.Color.from_pixel(0x00000055);

let discreteGpuAvailable = false;

var SidePages = {
    NONE: 0,
    PREVIOUS: 1 << 0,
    NEXT: 1 << 1,
    DND: 1 << 2,
};

function _getCategories(info) {
    let categoriesStr = info.get_categories();
    if (!categoriesStr)
        return [];
    return categoriesStr.split(';');
}

function _listsIntersect(a, b) {
    for (let itemA of a) {
        if (b.includes(itemA))
            return true;
    }
    return false;
}

function _getFolderName(folder) {
    let name = folder.get_string('name');

    if (folder.get_boolean('translate')) {
        let translated = Shell.util_get_translated_folder_name(name);
        if (translated !== null)
            return translated;
    }

    return name;
}

function _getViewFromIcon(icon) {
    for (let parent = icon.get_parent(); parent; parent = parent.get_parent()) {
        if (parent instanceof BaseAppView)
            return parent;
    }
    return null;
}

function _findBestFolderName(apps) {
    let appInfos = apps.map(app => app.get_app_info());

    let categoryCounter = {};
    let commonCategories = [];

    appInfos.reduce((categories, appInfo) => {
        for (let category of _getCategories(appInfo)) {
            if (!(category in categoryCounter))
                categoryCounter[category] = 0;

            categoryCounter[category] += 1;

            // If a category is present in all apps, its counter will
            // reach appInfos.length
            if (category.length > 0 &&
                categoryCounter[category] == appInfos.length)
                categories.push(category);
        }
        return categories;
    }, commonCategories);

    for (let category of commonCategories) {
        const directory = '%s.directory'.format(category);
        const translated = Shell.util_get_translated_folder_name(directory);
        if (translated !== null)
            return translated;
    }

    return null;
}

var BaseAppView = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
    Properties: {
        'gesture-modes': GObject.ParamSpec.flags(
            'gesture-modes', 'gesture-modes', 'gesture-modes',
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            Shell.ActionMode, Shell.ActionMode.OVERVIEW),
    },
    Signals: {
        'view-loaded': {},
    },
}, class BaseAppView extends St.Widget {
    _init(params = {}) {
        super._init(params);

        this._grid = this._createGrid();
        this._grid._delegate = this;
        // Standard hack for ClutterBinLayout
        this._grid.x_expand = true;
        this._grid.connect('pages-changed', () => {
            this.goToPage(this._grid.currentPage);
            this._pageIndicators.setNPages(this._grid.nPages);
            this._pageIndicators.setCurrentPosition(this._grid.currentPage);
        });

        // Scroll View
        this._scrollView = new St.ScrollView({
            style_class: 'apps-scroll-view',
            clip_to_allocation: true,
            x_expand: true,
            y_expand: true,
            reactive: true,
            enable_mouse_scrolling: false,
        });
        this._scrollView.set_policy(St.PolicyType.EXTERNAL, St.PolicyType.NEVER);
        this._scrollView._delegate = this;

        this._canScroll = true; // limiting scrolling speed
        this._scrollTimeoutId = 0;
        this._scrollView.connect('scroll-event', this._onScroll.bind(this));
        this._scrollView.connect('motion-event', this._onMotion.bind(this));
        this._scrollView.connect('enter-event', this._onMotion.bind(this));
        this._scrollView.connect('leave-event', this._onLeave.bind(this));
        this._scrollView.connect('button-press-event', this._onButtonPress.bind(this));

        this._scrollView.add_actor(this._grid);

        const scroll = this._scrollView.hscroll;
        this._adjustment = scroll.adjustment;
        this._adjustment.connect('notify::value', adj => {
            this._updateFade();
            const value = adj.value / adj.page_size;
            this._pageIndicators.setCurrentPosition(value);

            const distanceToPage = Math.abs(Math.round(value) - value);
            if (distanceToPage < 0.001) {
                this._hintContainer.opacity = 255;
                this._hintContainer.translationX = 0;
            } else {
                this._hintContainer.remove_transition('opacity');
                let opacity = Math.clamp(
                    255 * (1 - (distanceToPage * 2)),
                    0, 255);

                this._hintContainer.translationX = (Math.round(value) - value) * adj.page_size;
                this._hintContainer.opacity = opacity;
            }
        });

        // Page Indicators
        this._pageIndicators =
            new PageIndicators.PageIndicators(Clutter.Orientation.HORIZONTAL);

        this._pageIndicators.y_expand = false;
        this._pageIndicators.connect('page-activated',
            (indicators, pageIndex) => {
                this.goToPage(pageIndex);
            });
        this._pageIndicators.connect('scroll-event', (actor, event) => {
            this._scrollView.event(event, false);
        });

        // Navigation indicators
        this._nextPageIndicator = new St.Widget({
            style_class: 'page-navigation-hint next',
            opacity: 0,
            visible: false,
            reactive: false,
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.FILL,
        });

        this._prevPageIndicator = new St.Widget({
            style_class: 'page-navigation-hint previous',
            opacity: 0,
            visible: false,
            reactive: false,
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.FILL,
        });

        // Next/prev page arrows
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        this._nextPageArrow = new St.Icon({
            style_class: 'page-navigation-arrow',
            icon_name: rtl
                ? 'carousel-arrow-back-24-symbolic'
                : 'carousel-arrow-next-24-symbolic',
            opacity: 0,
            reactive: false,
            visible: false,
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
        });
        this._prevPageArrow = new St.Icon({
            style_class: 'page-navigation-arrow',
            icon_name: rtl
                ? 'carousel-arrow-next-24-symbolic'
                : 'carousel-arrow-back-24-symbolic',
            opacity: 0,
            reactive: false,
            visible: false,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
        });

        this._hintContainer = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        this._hintContainer.add_child(this._prevPageIndicator);
        this._hintContainer.add_child(this._nextPageIndicator);

        const scrollContainer = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            clip_to_allocation: true,
            y_expand: true,
        });
        scrollContainer.add_child(this._hintContainer);
        scrollContainer.add_child(this._scrollView);
        scrollContainer.add_child(this._nextPageArrow);
        scrollContainer.add_child(this._prevPageArrow);

        this._box = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            y_expand: true,
        });
        this._box.add_child(scrollContainer);
        this._box.add_child(this._pageIndicators);

        // Swipe
        this._swipeTracker = new SwipeTracker.SwipeTracker(this._scrollView,
            Clutter.Orientation.HORIZONTAL, this.gestureModes);
        this._swipeTracker.orientation = Clutter.Orientation.HORIZONTAL;
        this._swipeTracker.connect('begin', this._swipeBegin.bind(this));
        this._swipeTracker.connect('update', this._swipeUpdate.bind(this));
        this._swipeTracker.connect('end', this._swipeEnd.bind(this));

        this._availWidth = 0;
        this._availHeight = 0;
        this._orientation = Clutter.Orientation.HORIZONTAL;

        this._items = new Map();
        this._orderedItems = [];

        this._animateLaterId = 0;
        this._viewLoadedHandlerId = 0;
        this._viewIsReady = false;

        // Filter the apps through the user’s parental controls.
        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._appFilterChangedId =
            this._parentalControlsManager.connect('app-filter-changed', () => {
                this._redisplay();
            });

        // Don't duplicate favorites
        this._appFavorites = AppFavorites.getAppFavorites();
        this._appFavoritesChangedId =
            this._appFavorites.connect('changed', () => this._redisplay());

        // Drag n' Drop
        this._lastOvershoot = -1;
        this._lastOvershootTimeoutId = 0;
        this._delayedMoveData = null;

        this._dragBeginId = 0;
        this._dragEndId = 0;
        this._dragCancelledId = 0;

        this.connect('destroy', this._onDestroy.bind(this));

        this._previewedPages = new Map();
    }

    _onDestroy() {
        if (this._appFilterChangedId > 0) {
            this._parentalControlsManager.disconnect(this._appFilterChangedId);
            this._appFilterChangedId = 0;
        }

        if (this._appFavoritesChangedId > 0) {
            this._appFavorites.disconnect(this._appFavoritesChangedId);
            this._appFavoritesChangedId = 0;
        }

        if (this._swipeTracker) {
            this._swipeTracker.destroy();
            delete this._swipeTracker;
        }

        this._removeDelayedMove();
        this._disconnectDnD();
    }

    _updateFadeForNavigation() {
        const fadeMargin = new Clutter.Margin();
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const showingNextPage = this._pagesShown & SidePages.NEXT;
        const showingPrevPage = this._pagesShown & SidePages.PREVIOUS;

        if ((showingNextPage && !rtl) || (showingPrevPage && rtl)) {
            fadeMargin.right = Math.max(
                -PAGE_PREVIEW_FADE_EFFECT_MAX_OFFSET,
                -(this._availWidth - this._grid.layout_manager.pageWidth) / 2);
        }

        if ((showingPrevPage && !rtl) || (showingNextPage && rtl)) {
            fadeMargin.left = Math.max(
                -PAGE_PREVIEW_FADE_EFFECT_MAX_OFFSET,
                -(this._availWidth - this._grid.layout_manager.pageWidth) / 2);
        }

        this._scrollView.update_fade_effect(fadeMargin);
        const effect = this._scrollView.get_effect('fade');
        if (effect)
            effect.extend_fade_area = true;
    }

    _updateFade() {
        const { pagePadding } = this._grid.layout_manager;

        if (this._pagesShown)
            return;

        if (pagePadding.top === 0 &&
            pagePadding.right === 0 &&
            pagePadding.bottom === 0 &&
            pagePadding.left === 0)
            return;

        let hOffset = 0;
        let vOffset = 0;

        if ((this._adjustment.value % this._adjustment.page_size) !== 0.0) {
            const vertical = this._orientation === Clutter.Orientation.VERTICAL;

            hOffset = vertical ? 0 : Math.max(pagePadding.left, pagePadding.right);
            vOffset = vertical ? Math.max(pagePadding.top, pagePadding.bottom) : 0;

            if (hOffset === 0 && vOffset === 0)
                return;
        }

        this._scrollView.update_fade_effect(
            new Clutter.Margin({
                left: hOffset,
                right: hOffset,
                top: vOffset,
                bottom: vOffset,
            }));
    }

    _createGrid() {
        return new IconGrid.IconGrid({ allow_incomplete_pages: true });
    }

    _onScroll(actor, event) {
        if (this._swipeTracker.canHandleScrollEvent(event))
            return Clutter.EVENT_PROPAGATE;

        if (!this._canScroll)
            return Clutter.EVENT_STOP;

        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const vertical = this._orientation === Clutter.Orientation.VERTICAL;

        let nextPage = this._grid.currentPage;
        switch (event.get_scroll_direction()) {
        case Clutter.ScrollDirection.UP:
            nextPage -= 1;
            break;

        case Clutter.ScrollDirection.DOWN:
            nextPage += 1;
            break;

        case Clutter.ScrollDirection.LEFT:
            if (vertical)
                return Clutter.EVENT_STOP;
            nextPage += rtl ? 1 : -1;
            break;

        case Clutter.ScrollDirection.RIGHT:
            if (vertical)
                return Clutter.EVENT_STOP;
            nextPage += rtl ? -1 : 1;
            break;

        default:
            return Clutter.EVENT_STOP;
        }

        this.goToPage(nextPage);

        this._canScroll = false;
        this._scrollTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            SCROLL_TIMEOUT_TIME, () => {
                this._canScroll = true;
                this._scrollTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });

        return Clutter.EVENT_STOP;
    }

    _pageForCoords(x, y) {
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const { allocation } = this._grid;

        const [success, pointerX] = this._scrollView.transform_stage_point(x, y);
        if (!success)
            return SidePages.NONE;

        if (pointerX < allocation.x1)
            return rtl ? SidePages.NEXT : SidePages.PREVIOUS;
        else if (pointerX > allocation.x2)
            return rtl ? SidePages.PREVIOUS : SidePages.NEXT;

        return SidePages.NONE;
    }

    _onMotion(actor, event) {
        const page = this._pageForCoords(...event.get_coords());
        this._slideSidePages(page);

        return Clutter.EVENT_PROPAGATE;
    }

    _onButtonPress(actor, event) {
        const page = this._pageForCoords(...event.get_coords());
        if (page === SidePages.NEXT)
            this.goToPage(this._grid.currentPage + 1);
        else if (page === SidePages.PREVIOUS)
            this.goToPage(this._grid.currentPage - 1);
    }

    _onLeave() {
        this._slideSidePages(SidePages.NONE);
    }

    _swipeBegin(tracker, monitor) {
        if (monitor !== Main.layoutManager.primaryIndex)
            return;

        if (this._dragFocus) {
            this._dragFocus.cancelActions();
            this._dragFocus = null;
        }

        const adjustment = this._adjustment;
        adjustment.remove_transition('value');

        const progress = adjustment.value / adjustment.page_size;
        const points = Array.from({ length: this._grid.nPages }, (v, i) => i);
        const size = tracker.orientation === Clutter.Orientation.VERTICAL
            ? this._scrollView.height : this._scrollView.width;

        tracker.confirmSwipe(size, points, progress, Math.round(progress));
    }

    _swipeUpdate(tracker, progress) {
        const adjustment = this._adjustment;
        adjustment.value = progress * adjustment.page_size;
    }

    _swipeEnd(tracker, duration, endProgress) {
        const adjustment = this._adjustment;
        const value = endProgress * adjustment.page_size;

        this._syncPageHints(endProgress);

        adjustment.ease(value, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration,
            onComplete: () => this.goToPage(endProgress, false),
        });
    }

    _connectDnD() {
        this._dragBeginId =
            Main.overview.connect('item-drag-begin', this._onDragBegin.bind(this));
        this._dragEndId =
            Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));
        this._dragCancelledId =
            Main.overview.connect('item-drag-cancelled', this._onDragCancelled.bind(this));
    }

    _disconnectDnD() {
        if (this._dragBeginId > 0) {
            Main.overview.disconnect(this._dragBeginId);
            this._dragBeginId = 0;
        }

        if (this._dragEndId > 0) {
            Main.overview.disconnect(this._dragEndId);
            this._dragEndId = 0;
        }

        if (this._dragCancelledId > 0) {
            Main.overview.disconnect(this._dragCancelledId);
            this._dragCancelledId = 0;
        }

        if (this._dragMonitor) {
            DND.removeDragMonitor(this._dragMonitor);
            this._dragMonitor = null;
        }
    }

    _maybeMoveItem(dragEvent) {
        const [success, x, y] =
            this._grid.transform_stage_point(dragEvent.x, dragEvent.y);

        if (!success)
            return;

        const { source } = dragEvent;
        const [page, position, dragLocation] =
            this._getDropTarget(x, y, source);
        const item = position !== -1
            ? this._grid.getItemAt(page, position) : null;


        // Dragging over invalid parts of the grid cancels the timeout
        if (item === source ||
            dragLocation === IconGrid.DragLocation.INVALID ||
            dragLocation === IconGrid.DragLocation.ON_ICON) {
            this._removeDelayedMove();
            return;
        }

        if (!this._delayedMoveData ||
            this._delayedMoveData.page !== page ||
            this._delayedMoveData.position !== position) {
            // Update the item with a small delay
            this._removeDelayedMove();
            this._delayedMoveData = {
                page,
                position,
                source,
                destroyId: source.connect('destroy', () => this._removeDelayedMove()),
                timeoutId: GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                    DELAYED_MOVE_TIMEOUT, () => {
                        this._moveItem(source, page, position);
                        this._delayedMoveData.timeoutId = 0;
                        this._removeDelayedMove();
                        return GLib.SOURCE_REMOVE;
                    }),
            };
        }
    }

    _removeDelayedMove() {
        if (!this._delayedMoveData)
            return;

        const { source, destroyId, timeoutId  } = this._delayedMoveData;

        if (timeoutId > 0)
            GLib.source_remove(timeoutId);

        if (destroyId > 0)
            source.disconnect(destroyId);

        this._delayedMoveData = null;
    }

    _resetOvershoot() {
        if (this._lastOvershootTimeoutId)
            GLib.source_remove(this._lastOvershootTimeoutId);
        this._lastOvershootTimeoutId = 0;
        this._lastOvershoot = -1;
    }

    _handleDragOvershoot(dragEvent) {
        const [gridX, gridY] = this.get_transformed_position();
        const [gridWidth, gridHeight] = this.get_transformed_size();

        const vertical = this._orientation === Clutter.Orientation.VERTICAL;
        const gridStart = vertical ? gridY : gridX;
        const gridEnd = vertical
            ? gridY + gridHeight - OVERSHOOT_THRESHOLD
            : gridX + gridWidth - OVERSHOOT_THRESHOLD;

        // Already animating
        if (this._adjustment.get_transition('value') !== null)
            return;

        // Within the grid boundaries
        const dragPosition = vertical ? dragEvent.y : dragEvent.x;
        if (dragPosition > gridStart && dragPosition < gridEnd) {
            // Check whether we moved out the area of the last switch
            if (Math.abs(this._lastOvershoot - dragPosition) > OVERSHOOT_THRESHOLD)
                this._resetOvershoot();

            return;
        }

        // Still in the area of the previous page switch
        if (this._lastOvershoot >= 0)
            return;

        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        if (dragPosition <= gridStart)
            this.goToPage(this._grid.currentPage + (rtl ? 1 : -1));
        else if (dragPosition >= gridEnd)
            this.goToPage(this._grid.currentPage + (rtl ? -1 : 1));
        else
            return; // don't go beyond first/last page

        this._lastOvershoot = dragPosition;

        if (this._lastOvershootTimeoutId > 0)
            GLib.source_remove(this._lastOvershootTimeoutId);

        this._lastOvershootTimeoutId =
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, OVERSHOOT_TIMEOUT, () => {
                this._resetOvershoot();
                this._handleDragOvershoot(dragEvent);
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._lastOvershootTimeoutId,
            '[gnome-shell] this._lastOvershootTimeoutId');
    }

    _onDragBegin() {
        this._dragMonitor = {
            dragMotion: this._onDragMotion.bind(this),
        };
        DND.addDragMonitor(this._dragMonitor);
        this._slideSidePages(SidePages.PREVIOUS | SidePages.NEXT | SidePages.DND);
        this._dragFocus = null;
        this._swipeTracker.enabled = false;
    }

    _onDragMotion(dragEvent) {
        if (!(dragEvent.source instanceof AppViewItem))
            return DND.DragMotionResult.CONTINUE;

        const appIcon = dragEvent.source;

        this._dropPage = this._pageForCoords(dragEvent.x, dragEvent.y);
        if (this._dropPage &&
            this._dropPage === SidePages.PREVIOUS &&
            this._grid.currentPage === 0) {
            delete this._dropPage;
            return DND.DragMotionResult.NO_DROP;
        }

        // Handle the drag overshoot. When dragging to above the
        // icon grid, move to the page above; when dragging below,
        // move to the page below.
        if (appIcon instanceof AppViewItem)
            this._handleDragOvershoot(dragEvent);

        this._maybeMoveItem(dragEvent);

        return DND.DragMotionResult.CONTINUE;
    }

    _onDragEnd() {
        if (this._dragMonitor) {
            DND.removeDragMonitor(this._dragMonitor);
            this._dragMonitor = null;
        }

        this._resetOvershoot();
        this._slideSidePages(SidePages.NONE);
        delete this._dropPage;
        this._swipeTracker.enabled = true;
    }

    _onDragCancelled() {
        // At this point, the positions aren't stored yet, thus _redisplay()
        // will move all items to their original positions
        this._redisplay();
        this._slideSidePages(SidePages.NONE);
        this._swipeTracker.enabled = true;
    }

    _canAccept(source) {
        return source instanceof AppViewItem;
    }

    handleDragOver(source) {
        if (!this._canAccept(source))
            return DND.DragMotionResult.NO_DROP;

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source) {
        if (!this._canAccept(source))
            return false;

        if (this._dropPage) {
            const increment = this._dropPage === SidePages.NEXT ? 1 : -1;
            const { currentPage, nPages } = this._grid;
            const page = Math.min(currentPage + increment, nPages);
            const position = page < nPages ? -1 : 0;

            this._moveItem(source, page, position);
            this.goToPage(page);
        } else if (this._delayedMoveData) {
            // Dropped before the icon was moved
            const { page, position } = this._delayedMoveData;

            this._moveItem(source, page, position);
            this._removeDelayedMove();
        }

        return true;
    }

    _findBestPageToAppend(startPage = 1) {
        for (let i = startPage; i < this._grid.nPages; i++) {
            const pageItems =
                this._grid.getItemsAtPage(i).filter(c => c.visible);

            if (pageItems.length < this._grid.itemsPerPage)
                return i;
        }

        return -1;
    }

    _getLinearPosition(page, position) {
        let itemIndex = 0;

        if (this._grid.nPages > 0) {
            const realPage = page === -1 ? this._grid.nPages - 1 : page;

            itemIndex = position === -1
                ? this._grid.getItemsAtPage(realPage).filter(c => c.visible).length - 1
                : position;

            for (let i = 0; i < realPage; i++) {
                const pageItems = this._grid.getItemsAtPage(i).filter(c => c.visible);
                itemIndex += pageItems.length;
            }
        }

        return itemIndex;
    }

    _addItem(item, page, position) {
        // Append icons to the first page with empty slot, starting from
        // the second page
        if (this._grid.nPages > 1 && page === -1 && position === -1)
            page = this._findBestPageToAppend();

        const itemIndex = this._getLinearPosition(page, position);

        this._orderedItems.splice(itemIndex, 0, item);
        this._items.set(item.id, item);
        this._grid.addItem(item, page, position);
    }

    _removeItem(item) {
        const iconIndex = this._orderedItems.indexOf(item);

        this._orderedItems.splice(iconIndex, 1);
        this._items.delete(item.id);
        this._grid.removeItem(item);
    }

    _getItemPosition(item) {
        const { itemsPerPage } = this._grid;

        let iconIndex = this._orderedItems.indexOf(item);
        if (iconIndex === -1)
            iconIndex = this._orderedItems.length - 1;

        const page = Math.floor(iconIndex / itemsPerPage);
        const position = iconIndex % itemsPerPage;

        return [page, position];
    }

    _redisplay() {
        let oldApps = this._orderedItems.slice();
        let oldAppIds = oldApps.map(icon => icon.id);

        let newApps = this._loadApps().sort(this._compareItems.bind(this));
        let newAppIds = newApps.map(icon => icon.id);

        let addedApps = newApps.filter(icon => !oldAppIds.includes(icon.id));
        let removedApps = oldApps.filter(icon => !newAppIds.includes(icon.id));

        // Remove old app icons
        removedApps.forEach(icon => {
            this._removeItem(icon);
            icon.destroy();
        });

        // Add new app icons, or move existing ones
        newApps.forEach(icon => {
            const [page, position] = this._getItemPosition(icon);
            if (addedApps.includes(icon))
                this._addItem(icon, page, position);
            else if (page !== -1 && position !== -1)
                this._moveItem(icon, page, position);
        });

        this._viewIsReady = true;
        this.emit('view-loaded');
    }

    getAllItems() {
        return this._orderedItems;
    }

    _compareItems(a, b) {
        return a.name.localeCompare(b.name);
    }

    _selectAppInternal(id) {
        if (this._items.has(id))
            this._items.get(id).navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
        else
            log('No such application %s'.format(id));
    }

    selectApp(id) {
        if (this._items.has(id)) {
            let item = this._items.get(id);

            if (item.mapped) {
                this._selectAppInternal(id);
            } else {
                // Need to wait until the view is mapped
                let signalId = item.connect('notify::mapped', actor => {
                    if (actor.mapped) {
                        actor.disconnect(signalId);
                        this._selectAppInternal(id);
                    }
                });
            }
        } else {
            // Need to wait until the view is built
            let signalId = this.connect('view-loaded', () => {
                this.disconnect(signalId);
                this.selectApp(id);
            });
        }
    }

    _doSpringAnimation(animationDirection) {
        this._grid.opacity = 255;
        this._grid.animateSpring(
            animationDirection,
            Main.overview.dash.showAppsButton);
    }

    _clearAnimateLater() {
        if (this._animateLaterId) {
            Meta.later_remove(this._animateLaterId);
            this._animateLaterId = 0;
        }
        if (this._viewLoadedHandlerId) {
            this.disconnect(this._viewLoadedHandlerId);
            this._viewLoadedHandlerId = 0;
        }
    }

    animate(animationDirection, onComplete) {
        if (onComplete) {
            let animationDoneId = this._grid.connect('animation-done', () => {
                this._grid.disconnect(animationDoneId);
                onComplete();
            });
        }

        this._clearAnimateLater();
        this._grid.opacity = 255;

        if (animationDirection == IconGrid.AnimationDirection.IN) {
            const doSpringAnimationLater = laterType => {
                this._animateLaterId = Meta.later_add(laterType,
                    () => {
                        this._animateLaterId = 0;
                        this._doSpringAnimation(animationDirection);
                        return GLib.SOURCE_REMOVE;
                    });
            };

            if (this._viewIsReady) {
                this._grid.opacity = 0;
                doSpringAnimationLater(Meta.LaterType.IDLE);
            } else {
                this._viewLoadedHandlerId = this.connect('view-loaded',
                    () => {
                        this._clearAnimateLater();
                        this._grid.opacity = 255;
                        doSpringAnimationLater(Meta.LaterType.BEFORE_REDRAW);
                    });
            }
        } else {
            this._doSpringAnimation(animationDirection);
        }
    }

    _getDropTarget(x, y, source) {
        const { currentPage } = this._grid;

        let [item, dragLocation] = this._grid.getDropTarget(x, y);

        const [sourcePage, sourcePosition] = this._grid.getItemPosition(source);
        const targetPage = currentPage;
        let targetPosition = item
            ? this._grid.getItemPosition(item)[1] : -1;

        // In case we're hovering over the edge of an item but the
        // reflow will happen in the opposite direction (the drag
        // can't "naturally push the item away"), we instead set the
        // drop target to the adjacent item that can be pushed away
        // in the reflow-direction.
        //
        // We must avoid doing that if we're hovering over the first
        // or last column though, in that case there is no adjacent
        // icon we could push away.
        if (dragLocation === IconGrid.DragLocation.START_EDGE &&
            targetPosition > sourcePosition &&
            targetPage === sourcePage) {
            const nColumns = this._grid.layout_manager.columns_per_page;
            const targetColumn = targetPosition % nColumns;

            if (targetColumn > 0) {
                targetPosition -= 1;
                dragLocation = IconGrid.DragLocation.END_EDGE;
            }
        } else if (dragLocation === IconGrid.DragLocation.END_EDGE &&
            (targetPosition < sourcePosition ||
             targetPage !== sourcePage)) {
            const nColumns = this._grid.layout_manager.columns_per_page;
            const targetColumn = targetPosition % nColumns;

            if (targetColumn < nColumns - 1) {
                targetPosition += 1;
                dragLocation = IconGrid.DragLocation.START_EDGE;
            }
        }

        // Append to the page if dragging over empty area
        if (dragLocation === IconGrid.DragLocation.EMPTY_SPACE) {
            const pageItems =
                this._grid.getItemsAtPage(currentPage).filter(c => c.visible);

            targetPosition = pageItems.length;
        }

        return [targetPage, targetPosition, dragLocation];
    }

    _moveItem(item, newPage, newPosition) {
        const [page, position] = this._grid.getItemPosition(item);
        if (page === newPage && position === newPosition)
            return;

        // Update the _orderedItems array
        let index = this._orderedItems.indexOf(item);
        this._orderedItems.splice(index, 1);

        index = this._getLinearPosition(newPage, newPosition);
        this._orderedItems.splice(index, 0, item);

        this._grid.moveItem(item, newPage, newPosition);
    }

    vfunc_allocate(box) {
        const width = box.get_width();
        const height = box.get_height();

        this.adaptToSize(width, height);

        super.vfunc_allocate(box);
    }

    vfunc_map() {
        this._swipeTracker.enabled = true;
        this._connectDnD();
        super.vfunc_map();
    }

    vfunc_unmap() {
        this._swipeTracker.enabled = false;
        this._clearAnimateLater();
        this._disconnectDnD();
        super.vfunc_unmap();
    }

    animateSwitch(animationDirection) {
        this.remove_all_transitions();
        this._grid.remove_all_transitions();

        let params = {
            duration: VIEWS_SWITCH_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        };
        if (animationDirection == IconGrid.AnimationDirection.IN) {
            this.show();
            params.opacity = 255;
            params.delay = VIEWS_SWITCH_ANIMATION_DELAY;
        } else {
            params.opacity = 0;
            params.delay = 0;
            params.onComplete = () => this.hide();
        }

        this._grid.ease(params);
    }

    _syncPageHints(pageNumber, animate = true) {
        const showingNextPage = this._pagesShown & SidePages.NEXT;
        const showingPrevPage = this._pagesShown & SidePages.PREVIOUS;
        const dnd = this._pagesShown & SidePages.DND;
        const duration = animate ? PAGE_INDICATOR_FADE_TIME : 0;

        if (showingPrevPage) {
            const opacity = pageNumber === 0 ? 0 : 255;
            this._prevPageIndicator.visible = true;
            this._prevPageIndicator.ease({
                opacity,
                duration,
            });

            if (!dnd) {
                this._prevPageArrow.visible = true;
                this._prevPageArrow.ease({
                    opacity,
                    duration,
                });
            }
        }

        if (showingNextPage) {
            const opacity = pageNumber === this._grid.nPages - 1 ? 0 : 255;
            this._nextPageIndicator.visible = true;
            this._nextPageIndicator.ease({
                opacity,
                duration,
            });

            if (!dnd) {
                this._nextPageArrow.visible = true;
                this._nextPageArrow.ease({
                    opacity,
                    duration,
                });
            }
        }
    }

    goToPage(pageNumber, animate = true) {
        pageNumber = Math.clamp(pageNumber, 0, this._grid.nPages - 1);

        if (this._grid.currentPage === pageNumber)
            return;

        this._syncPageHints(pageNumber, animate);
        this._grid.goToPage(pageNumber, animate);
    }

    adaptToSize(width, height) {
        let box = new Clutter.ActorBox({
            x2: width,
            y2: height,
        });
        box = this.get_theme_node().get_content_box(box);
        box = this._scrollView.get_theme_node().get_content_box(box);
        box = this._grid.get_theme_node().get_content_box(box);

        const availWidth = box.get_width();
        const availHeight = box.get_height();

        const gridRatio = this._grid.layout_manager.columnsPerPage /
            this._grid.layout_manager.rowsPerPage;
        const spaceRatio = availWidth / availHeight;
        let pageWidth, pageHeight;

        if (spaceRatio > gridRatio * 1.1) {
            // Enough room for some preview
            pageHeight = availHeight;
            pageWidth = Math.ceil(availHeight * gridRatio);

            if (spaceRatio > gridRatio * 1.5) {
                // Ultra-wide layout, give some extra space for
                // the page area, but up to an extent.
                const extraPageSpace = Math.min(
                    Math.floor((availWidth - pageWidth) / 2), MAX_PAGE_PADDING);
                pageWidth += extraPageSpace;
                this._grid.layout_manager.pagePadding.left =
                    Math.floor(extraPageSpace / 2);
                this._grid.layout_manager.pagePadding.right =
                    Math.ceil(extraPageSpace / 2);
            }
        } else {
            // Not enough room, needs to shrink horizontally
            pageWidth = Math.ceil(availWidth * 0.8);
            pageHeight = availHeight;
            this._grid.layout_manager.pagePadding.left =
                Math.floor(availWidth * 0.02);
            this._grid.layout_manager.pagePadding.right =
                Math.ceil(availWidth * 0.02);
        }

        this._grid.adaptToSize(pageWidth, pageHeight);

        const leftPadding = Math.floor(
            (availWidth - this._grid.layout_manager.pageWidth) / 2);
        const rightPadding = Math.ceil(
            (availWidth - this._grid.layout_manager.pageWidth) / 2);
        const topPadding = Math.floor(
            (availHeight - this._grid.layout_manager.pageHeight) / 2);
        const bottomPadding = Math.ceil(
            (availHeight - this._grid.layout_manager.pageHeight) / 2);

        this._scrollView.content_padding = new Clutter.Margin({
            left: leftPadding,
            right: rightPadding,
            top: topPadding,
            bottom: bottomPadding,
        });

        this._availWidth = availWidth;
        this._availHeight = availHeight;

        this._pageIndicatorOffset = leftPadding;
        this._pageArrowOffset = Math.max(
            leftPadding - PAGE_PREVIEW_MAX_ARROW_OFFSET, 0);
    }

    _getIndicatorOffset(page, progress, baseOffset) {
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const translationX =
            (1 - progress) * PAGE_PREVIEW_ANIMATION_START_OFFSET;

        page = rtl ? -page : page;

        return (translationX - baseOffset) * page;
    }

    _getPagePreviewAdjustment(page) {
        const previewedPage = this._previewedPages.get(page);
        return previewedPage?.adjustment;
    }

    _syncClip() {
        const nextPageAdjustment = this._getPagePreviewAdjustment(1);
        const prevPageAdjustment = this._getPagePreviewAdjustment(-1);
        this._grid.clip_to_view =
            (!prevPageAdjustment || prevPageAdjustment.value === 0) &&
            (!nextPageAdjustment || nextPageAdjustment.value === 0);
    }

    _setupPagePreview(page, state) {
        if (this._previewedPages.has(page))
            return this._previewedPages.get(page).adjustment;

        const adjustment = new St.Adjustment({
            actor: this,
            lower: 0,
            upper: 1,
        });

        const indicator = page > 0
            ? this._nextPageIndicator : this._prevPageIndicator;

        const notifyId = adjustment.connect('notify::value', () => {
            const nextPage = this._grid.currentPage + page;
            const hasFollowingPage = nextPage >= 0 &&
                nextPage < this._grid.nPages;

            if (hasFollowingPage) {
                const items = this._grid.getItemsAtPage(nextPage);
                items.forEach(item => {
                    item.translation_x =
                        this._getIndicatorOffset(page, adjustment.value, 0);
                });

                if (!(state & SidePages.DND)) {
                    const pageArrow = page > 0
                        ? this._nextPageArrow
                        : this._prevPageArrow;
                    pageArrow.set({
                        visible: true,
                        opacity: adjustment.value * 255,
                        translation_x: this._getIndicatorOffset(
                            page, adjustment.value,
                            this._pageArrowOffset),
                    });
                }
            }
            if (hasFollowingPage ||
                (page > 0 &&
                 this._grid.layout_manager.allow_incomplete_pages &&
                 (state & SidePages.DND) !== 0)) {
                indicator.set({
                    visible: true,
                    opacity: adjustment.value * 255,
                    translation_x: this._getIndicatorOffset(
                        page, adjustment.value,
                        this._pageIndicatorOffset - indicator.width),
                });
            }
            this._syncClip();
        });

        this._previewedPages.set(page, {
            adjustment,
            notifyId,
        });

        return adjustment;
    }

    _teardownPagePreview(page) {
        const previewedPage = this._previewedPages.get(page);
        if (!previewedPage)
            return;

        previewedPage.adjustment.value = 1;
        previewedPage.adjustment.disconnect(previewedPage.notifyId);
        this._previewedPages.delete(page);
    }

    _slideSidePages(state) {
        if (this._pagesShown === state)
            return;
        this._pagesShown = state;
        const showingNextPage = state & SidePages.NEXT;
        const showingPrevPage = state & SidePages.PREVIOUS;
        const dnd = state & SidePages.DND;
        let adjustment;

        if (dnd) {
            this._nextPageIndicator.add_style_class_name('dnd');
            this._prevPageIndicator.add_style_class_name('dnd');
        } else {
            this._nextPageIndicator.remove_style_class_name('dnd');
            this._prevPageIndicator.remove_style_class_name('dnd');
        }

        adjustment = this._getPagePreviewAdjustment(1);
        if (showingNextPage) {
            adjustment = this._setupPagePreview(1, state);

            adjustment.ease(1, {
                duration: PAGE_PREVIEW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
            this._updateFadeForNavigation();
        } else if (adjustment) {
            adjustment.ease(0, {
                duration: PAGE_PREVIEW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._teardownPagePreview(1);
                    this._syncClip();
                    this._nextPageArrow.visible = false;
                    this._nextPageIndicator.visible = false;
                    this._updateFadeForNavigation();
                },
            });
        }

        adjustment = this._getPagePreviewAdjustment(-1);
        if (showingPrevPage) {
            adjustment = this._setupPagePreview(-1, state);

            adjustment.ease(1, {
                duration: PAGE_PREVIEW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
            this._updateFadeForNavigation();
        } else if (adjustment) {
            adjustment.ease(0, {
                duration: PAGE_PREVIEW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._teardownPagePreview(-1);
                    this._syncClip();
                    this._prevPageArrow.visible = false;
                    this._prevPageIndicator.visible = false;
                    this._updateFadeForNavigation();
                },
            });
        }
    }

    updateDragFocus(dragFocus) {
        this._dragFocus = dragFocus;
    }
});

var PageManager = GObject.registerClass({
    Signals: { 'layout-changed': {} },
}, class PageManager extends GObject.Object {
    _init() {
        super._init();

        this._updatingPages = false;
        this._loadPages();

        global.settings.connect('changed::app-picker-layout',
            this._loadPages.bind(this));
    }

    _loadPages() {
        const layout = global.settings.get_value('app-picker-layout');
        this._pages = layout.recursiveUnpack();
        if (!this._updatingPages)
            this.emit('layout-changed');
    }

    getAppPosition(appId) {
        let position = -1;
        let page = -1;

        for (let pageIndex = 0; pageIndex < this._pages.length; pageIndex++) {
            const pageData = this._pages[pageIndex];

            if (appId in pageData) {
                page = pageIndex;
                position = pageData[appId].position;
                break;
            }
        }

        return [page, position];
    }

    set pages(p) {
        const packedPages = [];

        // Pack the icon properties as a GVariant
        for (const page of p) {
            const pageData = {};
            for (const [appId, properties] of Object.entries(page))
                pageData[appId] = new GLib.Variant('a{sv}', properties);
            packedPages.push(pageData);
        }

        this._updatingPages = true;

        const variant = new GLib.Variant('aa{sv}', packedPages);
        global.settings.set_value('app-picker-layout', variant);

        this._updatingPages = false;
    }

    get pages() {
        return this._pages;
    }
});

var AppDisplay = GObject.registerClass(
class AppDisplay extends BaseAppView {
    _init() {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });

        this._pageManager = new PageManager();
        this._pageManager.connect('layout-changed', () => this._redisplay());

        this._stack = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        this.add_actor(this._stack);
        this._stack.add_child(this._box);

        this._folderIcons = [];

        this._currentDialog = null;
        this._displayingDialog = false;
        this._currentDialogDestroyId = 0;

        this._placeholder = null;

        Main.overview.connect('hidden', () => this.goToPage(0));

        this._redisplayWorkId = Main.initializeDeferredWork(this, this._redisplay.bind(this));

        Shell.AppSystem.get_default().connect('installed-changed', () => {
            this._viewIsReady = false;
            Main.queueDeferredWork(this._redisplayWorkId);
        });
        this._folderSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders' });
        this._folderSettings.connect('changed::folder-children', () => {
            this._viewIsReady = false;
            Main.queueDeferredWork(this._redisplayWorkId);
        });

        this._switcherooNotifyId = global.connect('notify::switcheroo-control',
            () => this._updateDiscreteGpuAvailable());
        this._updateDiscreteGpuAvailable();
    }

    _updateDiscreteGpuAvailable() {
        this._switcherooProxy = global.get_switcheroo_control();
        if (this._switcherooProxy) {
            let prop = this._switcherooProxy.get_cached_property('HasDualGpu');
            discreteGpuAvailable = prop?.unpack() ?? false;
        } else {
            discreteGpuAvailable = false;
        }
    }

    _onDestroy() {
        super._onDestroy();

        if (this._scrollTimeoutId !== 0) {
            GLib.source_remove(this._scrollTimeoutId);
            this._scrollTimeoutId = 0;
        }
    }

    vfunc_map() {
        this._keyPressEventId =
            global.stage.connect('key-press-event',
                this._onKeyPressEvent.bind(this));
        super.vfunc_map();
    }

    vfunc_unmap() {
        if (this._keyPressEventId) {
            global.stage.disconnect(this._keyPressEventId);
            this._keyPressEventId = 0;
        }
        super.vfunc_unmap();
    }

    _redisplay() {
        this._folderIcons.forEach(icon => {
            icon.view._redisplay();
        });

        super._redisplay();
    }

    adaptToSize(width, height) {
        const [, indicatorHeight] = this._pageIndicators.get_preferred_height(-1);
        height -= indicatorHeight;

        this._grid.findBestModeForSize(width, height);
        super.adaptToSize(width, height);
    }

    _savePages() {
        const pages = [];

        for (let i = 0; i < this._grid.nPages; i++) {
            const pageItems =
                this._grid.getItemsAtPage(i).filter(c => c.visible);
            const pageData = {};

            pageItems.forEach((item, index) => {
                pageData[item.id] = {
                    position: GLib.Variant.new_int32(index),
                };
            });
            pages.push(pageData);
        }

        this._pageManager.pages = pages;
    }

    _ensurePlaceholder(source) {
        if (this._placeholder)
            return;

        const appSys = Shell.AppSystem.get_default();
        const app = appSys.lookup_app(source.id);

        const isDraggable =
            global.settings.is_writable('favorite-apps') ||
            global.settings.is_writable('app-picker-layout');

        this._placeholder = new AppIcon(app, { isDraggable });
        this._placeholder.connect('notify::pressed', () => {
            if (this._placeholder.pressed)
                this.updateDragFocus(this._placeholder);
        });
        this._placeholder.scaleAndFade();
        this._redisplay();
    }

    _removePlaceholder() {
        if (this._placeholder) {
            this._placeholder.undoScaleAndFade();
            this._placeholder = null;
            this._redisplay();
        }
    }

    getAppInfos() {
        return this._appInfoList;
    }

    _getItemPosition(item) {
        if (item === this._placeholder) {
            let [page, position] = this._grid.getItemPosition(item);

            if (page === -1)
                page = this._findBestPageToAppend(this._grid.currentPage);

            return [page, position];
        }

        return this._pageManager.getAppPosition(item.id);
    }

    _compareItems(a, b) {
        const [aPage, aPosition] = this._getItemPosition(a);
        const [bPage, bPosition] = this._getItemPosition(b);

        if (aPage === -1 && bPage === -1)
            return a.name.localeCompare(b.name);
        else if (aPage === -1)
            return 1;
        else if (bPage === -1)
            return -1;

        if (aPage !== bPage)
            return aPage - bPage;

        return aPosition - bPosition;
    }

    _loadApps() {
        let appIcons = [];
        this._appInfoList = Shell.AppSystem.get_default().get_installed().filter(appInfo => {
            try {
                appInfo.get_id(); // catch invalid file encodings
            } catch (e) {
                return false;
            }
            return !this._appFavorites.isFavorite(appInfo.get_id()) &&
                this._parentalControlsManager.shouldShowApp(appInfo);
        });

        let apps = this._appInfoList.map(app => app.get_id());

        let appSys = Shell.AppSystem.get_default();

        const appsInsideFolders = new Set();
        this._folderIcons = [];

        let folders = this._folderSettings.get_strv('folder-children');
        folders.forEach(id => {
            let path = '%sfolders/%s/'.format(this._folderSettings.path, id);
            let icon = this._items.get(id);
            if (!icon) {
                icon = new FolderIcon(id, path, this);
                icon.connect('apps-changed', () => {
                    this._redisplay();
                    this._savePages();
                });
                icon.connect('notify::pressed', () => {
                    if (icon.pressed)
                        this.updateDragFocus(icon);
                });
            }

            // Don't try to display empty folders
            if (!icon.visible) {
                icon.destroy();
                return;
            }

            appIcons.push(icon);
            this._folderIcons.push(icon);

            icon.getAppIds().forEach(appId => appsInsideFolders.add(appId));
        });

        // Allow dragging of the icon only if the Dash would accept a drop to
        // change favorite-apps. There are no other possible drop targets from
        // the app picker, so there's no other need for a drag to start,
        // at least on single-monitor setups.
        // This also disables drag-to-launch on multi-monitor setups,
        // but we hope that is not used much.
        const isDraggable =
            global.settings.is_writable('favorite-apps') ||
            global.settings.is_writable('app-picker-layout');

        apps.forEach(appId => {
            if (appsInsideFolders.has(appId))
                return;

            let icon = this._items.get(appId);
            if (!icon) {
                let app = appSys.lookup_app(appId);

                icon = new AppIcon(app, { isDraggable });
                icon.connect('notify::pressed', () => {
                    if (icon.pressed)
                        this.updateDragFocus(icon);
                });
            }

            appIcons.push(icon);
        });

        // At last, if there's a placeholder available, add it
        if (this._placeholder)
            appIcons.push(this._placeholder);

        return appIcons;
    }

    // Overridden from BaseAppView
    animate(animationDirection, onComplete) {
        this._scrollView.reactive = false;
        this._swipeTracker.enabled = false;
        let completionFunc = () => {
            this._scrollView.reactive = true;
            this._swipeTracker.enabled = this.mapped;
            if (onComplete)
                onComplete();
        };

        if (animationDirection == IconGrid.AnimationDirection.OUT &&
            this._displayingDialog && this._currentDialog)
            this._currentDialog.popdown();
        else
            super.animate(animationDirection, completionFunc);
    }

    animateSwitch(animationDirection) {
        super.animateSwitch(animationDirection);

        if (this._currentDialog && this._displayingDialog &&
            animationDirection == IconGrid.AnimationDirection.OUT) {
            this._currentDialog.ease({
                opacity: 0,
                duration: VIEWS_SWITCH_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => (this.opacity = 255),
            });
        }
    }

    goToPage(pageNumber, animate = true) {
        pageNumber = Math.clamp(pageNumber, 0, this._grid.nPages - 1);

        if (this._grid.currentPage === pageNumber &&
            this._displayingDialog &&
            this._currentDialog)
            return;
        if (this._displayingDialog && this._currentDialog)
            this._currentDialog.popdown();

        super.goToPage(pageNumber, animate);
    }

    _onScroll(actor, event) {
        if (this._displayingDialog || !this._scrollView.reactive)
            return Clutter.EVENT_STOP;

        return super._onScroll(actor, event);
    }

    _onKeyPressEvent(actor, event) {
        if (this._displayingDialog)
            return Clutter.EVENT_STOP;

        if (event.get_key_symbol() === Clutter.KEY_Page_Up) {
            this.goToPage(this._grid.currentPage - 1);
            return Clutter.EVENT_STOP;
        } else if (event.get_key_symbol() === Clutter.KEY_Page_Down) {
            this.goToPage(this._grid.currentPage + 1);
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    addFolderDialog(dialog) {
        Main.layoutManager.overviewGroup.add_child(dialog);
        dialog.connect('open-state-changed', (o, isOpen) => {
            if (this._currentDialog) {
                this._currentDialog.disconnect(this._currentDialogDestroyId);
                this._currentDialogDestroyId = 0;
            }

            this._currentDialog = null;

            if (isOpen) {
                this._currentDialog = dialog;
                this._currentDialogDestroyId = dialog.connect('destroy', () => {
                    this._currentDialog = null;
                    this._currentDialogDestroyId = 0;
                });
            }
            this._displayingDialog = isOpen;
        });
    }

    _maybeMoveItem(dragEvent) {
        const clonedEvent = {
            ...dragEvent,
            source: this._placeholder ? this._placeholder : dragEvent.source,
        };

        super._maybeMoveItem(clonedEvent);
    }

    _onDragBegin(overview, source) {
        super._onDragBegin(overview, source);

        // When dragging from a folder dialog, the dragged app icon doesn't
        // exist in AppDisplay. We work around that by adding a placeholder
        // icon that is either destroyed on cancel, or becomes the effective
        // new icon when dropped.
        if (_getViewFromIcon(source) instanceof FolderView ||
            this._appFavorites.isFavorite(source.id))
            this._ensurePlaceholder(source);
    }

    _onDragMotion(dragEvent) {
        if (this._currentDialog)
            return DND.DragMotionResult.CONTINUE;

        return super._onDragMotion(dragEvent);
    }

    _onDragEnd() {
        super._onDragEnd();
        this._removePlaceholder();
        this._savePages();
    }

    _onDragCancelled(overview, source) {
        const view = _getViewFromIcon(source);

        if (view instanceof FolderView)
            return;

        super._onDragCancelled(overview, source);
    }

    acceptDrop(source) {
        if (!super.acceptDrop(source))
            return false;

        this._savePages();

        let view = _getViewFromIcon(source);
        if (view instanceof FolderView)
            view.removeApp(source.app);

        if (this._currentDialog)
            this._currentDialog.popdown();

        if (this._appFavorites.isFavorite(source.id))
            this._appFavorites.removeFavorite(source.id);

        return true;
    }

    createFolder(apps) {
        let newFolderId = GLib.uuid_string_random();

        let folders = this._folderSettings.get_strv('folder-children');
        folders.push(newFolderId);
        this._folderSettings.set_strv('folder-children', folders);

        // Create the new folder
        let newFolderPath = this._folderSettings.path.concat('folders/', newFolderId, '/');
        let newFolderSettings;
        try {
            newFolderSettings = new Gio.Settings({
                schema_id: 'org.gnome.desktop.app-folders.folder',
                path: newFolderPath,
            });
        } catch (e) {
            log('Error creating new folder');
            return false;
        }

        // The hovered AppIcon always passes its own id as the first
        // one, and this is where we want the folder to be created
        let [folderPage, folderPosition] =
            this._grid.getItemPosition(this._items.get(apps[0]));

        // Adjust the final position
        folderPosition -= apps.reduce((counter, appId) => {
            const [page, position] =
                this._grid.getItemPosition(this._items.get(appId));
            if (page === folderPage && position < folderPosition)
                counter++;
            return counter;
        }, 0);

        let appItems = apps.map(id => this._items.get(id).app);
        let folderName = _findBestFolderName(appItems);
        if (!folderName)
            folderName = _("Unnamed Folder");

        newFolderSettings.delay();
        newFolderSettings.set_string('name', folderName);
        newFolderSettings.set_strv('apps', apps);
        newFolderSettings.apply();

        this._redisplay();

        // Move the folder to where the icon target icon was
        const folderItem = this._items.get(newFolderId);
        this._moveItem(folderItem, folderPage, folderPosition);
        this._savePages();

        return true;
    }
});

var AppSearchProvider = class AppSearchProvider {
    constructor() {
        this._appSys = Shell.AppSystem.get_default();
        this.id = 'applications';
        this.isRemoteProvider = false;
        this.canLaunchSearch = false;

        this._systemActions = new SystemActions.getDefault();

        this._parentalControlsManager = ParentalControlsManager.getDefault();
    }

    getResultMetas(apps, callback) {
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);
        let metas = [];
        for (let id of apps) {
            if (id.endsWith('.desktop')) {
                let app = this._appSys.lookup_app(id);

                metas.push({
                    id: app.get_id(),
                    name: app.get_name(),
                    createIcon: size => app.create_icon_texture(size),
                });
            } else {
                let name = this._systemActions.getName(id);
                let iconName = this._systemActions.getIconName(id);

                const createIcon = size => new St.Icon({
                    icon_name: iconName,
                    width: size * scaleFactor,
                    height: size * scaleFactor,
                    style_class: 'system-action-icon',
                });

                metas.push({ id, name, createIcon });
            }
        }

        callback(metas);
    }

    filterResults(results, maxNumber) {
        return results.slice(0, maxNumber);
    }

    getInitialResultSet(terms, callback, _cancellable) {
        // Defer until the parental controls manager is initialised, so the
        // results can be filtered correctly.
        if (!this._parentalControlsManager.initialized) {
            let initializedId = this._parentalControlsManager.connect('app-filter-changed', () => {
                if (this._parentalControlsManager.initialized) {
                    this._parentalControlsManager.disconnect(initializedId);
                    this.getInitialResultSet(terms, callback, _cancellable);
                }
            });
            return;
        }

        let query = terms.join(' ');
        let groups = Shell.AppSystem.search(query);
        let usage = Shell.AppUsage.get_default();
        let results = [];

        groups.forEach(group => {
            group = group.filter(appID => {
                const app = this._appSys.lookup_app(appID);
                return app && this._parentalControlsManager.shouldShowApp(app.app_info);
            });
            results = results.concat(group.sort(
                (a, b) => usage.compare(a, b)));
        });

        results = results.concat(this._systemActions.getMatchingActions(terms));

        callback(results);
    }

    getSubsearchResultSet(previousResults, terms, callback, cancellable) {
        this.getInitialResultSet(terms, callback, cancellable);
    }

    createResultObject(resultMeta) {
        if (resultMeta.id.endsWith('.desktop')) {
            return new AppIcon(this._appSys.lookup_app(resultMeta['id']), {
                expandTitleOnHover: false,
            });
        } else {
            return new SystemActionIcon(this, resultMeta);
        }
    }
};

var AppViewItem = GObject.registerClass(
class AppViewItem extends St.Button {
    _init(params = {}, isDraggable = true, expandTitleOnHover = true) {
        super._init({
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
            reactive: true,
            button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO,
            can_focus: true,
            ...params,
        });

        this._delegate = this;

        if (isDraggable) {
            this._draggable = DND.makeDraggable(this, { timeoutThreshold: 200 });

            this._draggable.connect('drag-begin', this._onDragBegin.bind(this));
            this._draggable.connect('drag-cancelled', this._onDragCancelled.bind(this));
            this._draggable.connect('drag-end', this._onDragEnd.bind(this));
        }

        this._otherIconIsHovering = false;
        this._expandTitleOnHover = expandTitleOnHover;

        if (expandTitleOnHover)
            this.connect('notify::hover', this._onHover.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._dragMonitor) {
            DND.removeDragMonitor(this._dragMonitor);
            this._dragMonitor = null;
        }

        if (this._draggable) {
            if (this._dragging)
                Main.overview.endItemDrag(this);
            this._draggable = null;
        }
    }

    _updateMultiline() {
        if (!this._expandTitleOnHover || !this.icon.label)
            return;

        const { label } = this.icon;
        const { clutterText } = label;
        const layout = clutterText.get_layout();
        if (!layout.is_wrapped() && !layout.is_ellipsized())
            return;

        label.remove_transition('allocation');

        const id = label.connect('notify::allocation', () => {
            label.restore_easing_state();
            label.disconnect(id);
        });

        const expand = this.hover || this.has_key_focus();
        label.save_easing_state();
        label.set_easing_duration(expand
            ? APP_ICON_TITLE_EXPAND_TIME
            : APP_ICON_TITLE_COLLAPSE_TIME);
        clutterText.set({
            line_wrap: expand,
            line_wrap_mode: expand ? Pango.WrapMode.WORD_CHAR : Pango.WrapMode.NONE,
            ellipsize: expand ? Pango.EllipsizeMode.NONE : Pango.EllipsizeMode.END,
        });
    }

    _onHover() {
        this._updateMultiline();
    }

    _onDragBegin() {
        this._dragging = true;
        this.scaleAndFade();
        Main.overview.beginItemDrag(this);
    }

    _onDragCancelled() {
        this._dragging = false;
        Main.overview.cancelledItemDrag(this);
    }

    _onDragEnd() {
        this._dragging = false;
        this.undoScaleAndFade();
        Main.overview.endItemDrag(this);
    }

    scaleIn() {
        this.scale_x = 0;
        this.scale_y = 0;

        this.ease({
            scale_x: 1,
            scale_y: 1,
            duration: APP_ICON_SCALE_IN_TIME,
            delay: APP_ICON_SCALE_IN_DELAY,
            mode: Clutter.AnimationMode.EASE_OUT_QUINT,
        });
    }

    scaleAndFade() {
        this.reactive = false;
        this.ease({
            scale_x: 0.5,
            scale_y: 0.5,
            opacity: 0,
        });
    }

    undoScaleAndFade() {
        this.reactive = true;
        this.ease({
            scale_x: 1.0,
            scale_y: 1.0,
            opacity: 255,
        });
    }

    _canAccept(source) {
        return source !== this;
    }

    _setHoveringByDnd(hovering) {
        if (this._otherIconIsHovering === hovering)
            return;

        this._otherIconIsHovering = hovering;

        if (hovering) {
            this._dragMonitor = {
                dragMotion: this._onDragMotion.bind(this),
            };
            DND.addDragMonitor(this._dragMonitor);
        } else {
            DND.removeDragMonitor(this._dragMonitor);
        }
    }

    _onDragMotion(dragEvent) {
        if (!this.contains(dragEvent.targetActor))
            this._setHoveringByDnd(false);

        return DND.DragMotionResult.CONTINUE;
    }

    _withinLeeways(x) {
        return x < IconGrid.LEFT_DIVIDER_LEEWAY ||
            x > this.width - IconGrid.RIGHT_DIVIDER_LEEWAY;
    }

    vfunc_key_focus_in() {
        this._updateMultiline();
        super.vfunc_key_focus_in();
    }

    vfunc_key_focus_out() {
        this._updateMultiline();
        super.vfunc_key_focus_out();
    }

    handleDragOver(source, _actor, x) {
        if (source === this)
            return DND.DragMotionResult.NO_DROP;

        if (!this._canAccept(source))
            return DND.DragMotionResult.CONTINUE;

        if (this._withinLeeways(x)) {
            this._setHoveringByDnd(false);
            return DND.DragMotionResult.CONTINUE;
        }

        this._setHoveringByDnd(true);

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source, _actor, x) {
        this._setHoveringByDnd(false);

        if (!this._canAccept(source))
            return false;

        if (this._withinLeeways(x))
            return false;

        return true;
    }

    cancelActions() {
        if (this._draggable)
            this._draggable.fakeRelease();
        this.fake_release();
    }

    get id() {
        return this._id;
    }

    get name() {
        return this._name;
    }
});

var FolderGrid = GObject.registerClass(
class FolderGrid extends IconGrid.IconGrid {
    _init() {
        super._init({
            allow_incomplete_pages: false,
            columns_per_page: 3,
            rows_per_page: 3,
            page_halign: Clutter.ActorAlign.CENTER,
            page_valign: Clutter.ActorAlign.CENTER,
        });
    }

    adaptToSize(width, height) {
        this.layout_manager.adaptToSize(width, height);
    }
});

var FolderView = GObject.registerClass(
class FolderView extends BaseAppView {
    _init(folder, id, parentView) {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            gesture_modes: Shell.ActionMode.POPUP,
        });

        // If it not expand, the parent doesn't take into account its preferred_width when allocating
        // the second time it allocates, so we apply the "Standard hack for ClutterBinLayout"
        this._grid.x_expand = true;
        this._id = id;
        this._folder = folder;
        this._parentView = parentView;
        this._grid._delegate = this;

        this.add_child(this._box);

        let action = new Clutter.PanAction({ interpolate: true });
        action.connect('pan', this._onPan.bind(this));
        this._scrollView.add_action(action);

        this._deletingFolder = false;
        this._appIds = [];
        this._redisplay();
    }

    _createGrid() {
        return new FolderGrid();
    }

    _getFolderApps() {
        const appIds = [];
        const excludedApps = this._folder.get_strv('excluded-apps');
        const appSys = Shell.AppSystem.get_default();
        const addAppId = appId => {
            if (excludedApps.includes(appId))
                return;

            if (this._appFavorites.isFavorite(appId))
                return;

            const app = appSys.lookup_app(appId);
            if (!app)
                return;

            if (!this._parentalControlsManager.shouldShowApp(app.get_app_info()))
                return;

            if (appIds.indexOf(appId) !== -1)
                return;

            appIds.push(appId);
        };

        const folderApps = this._folder.get_strv('apps');
        folderApps.forEach(addAppId);

        const folderCategories = this._folder.get_strv('categories');
        const appInfos = this._parentView.getAppInfos();
        appInfos.forEach(appInfo => {
            let appCategories = _getCategories(appInfo);
            if (!_listsIntersect(folderCategories, appCategories))
                return;

            addAppId(appInfo.get_id());
        });

        return appIds;
    }

    _getItemPosition(item) {
        const appIndex = this._appIds.indexOf(item.id);

        if (appIndex === -1)
            return [-1, -1];

        const { itemsPerPage } = this._grid;
        return [Math.floor(appIndex / itemsPerPage), appIndex % itemsPerPage];
    }

    _compareItems(a, b) {
        const aPosition = this._appIds.indexOf(a.id);
        const bPosition = this._appIds.indexOf(b.id);

        if (aPosition === -1 && bPosition === -1)
            return a.name.localeCompare(b.name);
        else if (aPosition === -1)
            return 1;
        else if (bPosition === -1)
            return -1;

        return aPosition - bPosition;
    }

    // Overridden from BaseAppView
    animate(animationDirection) {
        this._grid.animatePulse(animationDirection);
    }

    createFolderIcon(size) {
        const layout = new Clutter.GridLayout({
            row_homogeneous: true,
            column_homogeneous: true,
        });
        let icon = new St.Widget({
            layout_manager: layout,
            x_align: Clutter.ActorAlign.CENTER,
            style: 'width: %dpx; height: %dpx;'.format(size, size),
        });

        let subSize = Math.floor(FOLDER_SUBICON_FRACTION * size);

        let numItems = this._orderedItems.length;
        let rtl = icon.get_text_direction() == Clutter.TextDirection.RTL;
        for (let i = 0; i < 4; i++) {
            const style = 'width: %dpx; height: %dpx;'.format(subSize, subSize);
            let bin = new St.Bin({ style });
            if (i < numItems)
                bin.child = this._orderedItems[i].app.create_icon_texture(subSize);
            layout.attach(bin, rtl ? (i + 1) % 2 : i % 2, Math.floor(i / 2), 1, 1);
        }

        return icon;
    }

    _onPan(action) {
        let [dist_, dx_, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollView.vscroll.adjustment;
        adjustment.value -= (dy / this._scrollView.height) * adjustment.page_size;
        return false;
    }

    adaptToSize(width, height) {
        const [, indicatorHeight] = this._pageIndicators.get_preferred_height(-1);
        height -= indicatorHeight;

        super.adaptToSize(width, height);
    }

    _loadApps() {
        let apps = [];
        let appSys = Shell.AppSystem.get_default();

        this._appIds.forEach(appId => {
            const app = appSys.lookup_app(appId);

            let icon = this._items.get(appId);
            if (!icon)
                icon = new AppIcon(app);

            apps.push(icon);
        });

        return apps;
    }

    _redisplay() {
        // Keep the app ids list cached
        this._appIds = this._getFolderApps();

        super._redisplay();
    }

    acceptDrop(source) {
        if (!super.acceptDrop(source))
            return false;

        const folderApps = this._orderedItems.map(item => item.id);
        this._folder.set_strv('apps', folderApps);

        return true;
    }

    addApp(app) {
        let folderApps = this._folder.get_strv('apps');
        folderApps.push(app.id);

        this._folder.set_strv('apps', folderApps);

        // Also remove from 'excluded-apps' if the app id is listed
        // there. This is only possible on categories-based folders.
        let excludedApps = this._folder.get_strv('excluded-apps');
        let index = excludedApps.indexOf(app.id);
        if (index >= 0) {
            excludedApps.splice(index, 1);
            this._folder.set_strv('excluded-apps', excludedApps);
        }
    }

    removeApp(app) {
        let folderApps = this._folder.get_strv('apps');
        let index = folderApps.indexOf(app.id);
        if (index >= 0)
            folderApps.splice(index, 1);

        // Remove the folder if this is the last app icon; otherwise,
        // just remove the icon
        if (folderApps.length == 0) {
            this._deletingFolder = true;

            // Resetting all keys deletes the relocatable schema
            let keys = this._folder.settings_schema.list_keys();
            for (const key of keys)
                this._folder.reset(key);

            let settings = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders' });
            let folders = settings.get_strv('folder-children');
            folders.splice(folders.indexOf(this._id), 1);
            settings.set_strv('folder-children', folders);

            this._deletingFolder = false;
        } else {
            // If this is a categories-based folder, also add it to
            // the list of excluded apps
            const categories = this._folder.get_strv('categories');
            if (categories.length > 0) {
                const excludedApps = this._folder.get_strv('excluded-apps');
                excludedApps.push(app.id);
                this._folder.set_strv('excluded-apps', excludedApps);
            }

            this._folder.set_strv('apps', folderApps);
        }
    }

    get deletingFolder() {
        return this._deletingFolder;
    }
});

var FolderIcon = GObject.registerClass({
    Signals: {
        'apps-changed': {},
    },
}, class FolderIcon extends AppViewItem {
    _init(id, path, parentView) {
        super._init({
            style_class: 'app-well-app app-folder',
            button_mask: St.ButtonMask.ONE,
            toggle_mode: true,
            can_focus: true,
        }, global.settings.is_writable('app-picker-layout'));
        this._id = id;
        this._name = '';
        this._parentView = parentView;

        this._folder = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders.folder',
                                          path });

        this.icon = new IconGrid.BaseIcon('', {
            createIcon: this._createIcon.bind(this),
            setSizeManually: true,
        });
        this.set_child(this.icon);
        this.label_actor = this.icon.label;

        this.view = new FolderView(this._folder, id, parentView);

        this._folderChangedId = this._folder.connect(
            'changed', this._sync.bind(this));
        this._sync();
    }

    _onDestroy() {
        super._onDestroy();

        if (this._dialog)
            this._dialog.destroy();
        else
            this.view.destroy();

        if (this._folderChangedId) {
            this._folder.disconnect(this._folderChangedId);
            delete this._folderChangedId;
        }
    }

    vfunc_clicked() {
        this.open();
    }

    vfunc_unmap() {
        if (this._dialog)
            this._dialog.popdown();

        super.vfunc_unmap();
    }

    open() {
        this._ensureFolderDialog();
        this.view._scrollView.vscroll.adjustment.value = 0;
        this._dialog.popup();
    }

    getAppIds() {
        return this.view.getAllItems().map(item => item.id);
    }

    _setHoveringByDnd(hovering) {
        if (this._otherIconIsHovering == hovering)
            return;

        super._setHoveringByDnd(hovering);

        if (hovering)
            this.add_style_pseudo_class('drop');
        else
            this.remove_style_pseudo_class('drop');
    }

    _onDragMotion(dragEvent) {
        if (!this._canAccept(dragEvent.source))
            this._setHoveringByDnd(false);

        return super._onDragMotion(dragEvent);
    }

    getDragActor() {
        const iconParams = {
            createIcon: this._createIcon.bind(this),
            showLabel: this.icon.label !== null,
            setSizeManually: false,
        };

        const icon = new IconGrid.BaseIcon(this.name, iconParams);
        icon.style_class = this.style_class;

        return icon;
    }

    getDragActorSource() {
        return this;
    }

    _canAccept(source) {
        if (!(source instanceof AppIcon))
            return false;

        let view = _getViewFromIcon(source);
        if (!view || !(view instanceof AppDisplay))
            return false;

        if (this._folder.get_strv('apps').includes(source.id))
            return false;

        return true;
    }

    acceptDrop(source) {
        const accepted = super.acceptDrop(source);

        if (!accepted)
            return false;

        this.view.addApp(source.app);

        return true;
    }

    _updateName() {
        let name = _getFolderName(this._folder);
        if (this.name == name)
            return;

        this._name = name;
        this.icon.label.text = this.name;
    }

    _sync() {
        if (this.view.deletingFolder)
            return;

        this.emit('apps-changed');
        this._updateName();
        this.visible = this.view.getAllItems().length > 0;
        this.icon.update();
    }

    _createIcon(iconSize) {
        return this.view.createFolderIcon(iconSize, this);
    }

    _ensureFolderDialog() {
        if (this._dialog)
            return;
        if (!this._dialog) {
            this._dialog = new AppFolderDialog(this, this._folder,
                this._parentView);
            this._parentView.addFolderDialog(this._dialog);
            this._dialog.connect('open-state-changed', (popup, isOpen) => {
                const duration = FOLDER_DIALOG_ANIMATION_TIME / 2;
                const mode = isOpen
                    ? Clutter.AnimationMode.EASE_OUT_QUAD
                    : Clutter.AnimationMode.EASE_IN_QUAD;

                this.ease({
                    opacity: isOpen ? 0 : 255,
                    duration,
                    mode,
                    delay: isOpen ? 0 : FOLDER_DIALOG_ANIMATION_TIME - duration,
                });

                if (!isOpen)
                    this.checked = false;
            });
        }
    }
});

var AppFolderDialog = GObject.registerClass({
    Signals: {
        'open-state-changed': { param_types: [GObject.TYPE_BOOLEAN] },
    },
}, class AppFolderDialog extends St.Bin {
    _init(source, folder, appDisplay) {
        super._init({
            visible: false,
            x_expand: true,
            y_expand: true,
            reactive: true,
        });

        this.add_constraint(new Layout.MonitorConstraint({ primary: true }));

        const clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', () => {
            const [x, y] = clickAction.get_coords();
            const actor =
                global.stage.get_actor_at_pos(Clutter.PickMode.ALL, x, y);

            if (actor === this)
                this.popdown();
        });
        this.add_action(clickAction);

        this._source = source;
        this._folder = folder;
        this._view = source.view;
        this._appDisplay = appDisplay;
        this._delegate = this;

        this._isOpen = false;

        this._viewBox = new St.BoxLayout({
            style_class: 'app-folder-dialog',
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.FILL,
            vertical: true,
        });

        this.child = new St.Bin({
            style_class: 'app-folder-dialog-container',
            child: this._viewBox,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._addFolderNameEntry();
        this._viewBox.add_child(this._view);

        global.focus_manager.add_group(this);

        this._grabHelper = new GrabHelper.GrabHelper(this, {
            actionMode: Shell.ActionMode.POPUP,
        });
        this._grabHelper.addActor(Main.layoutManager.overviewGroup);
        this.connect('destroy', this._onDestroy.bind(this));

        this._dragMonitor = null;
        this._sourceMappedId = 0;
        this._popdownTimeoutId = 0;
        this._needsZoomAndFade = false;

        this._popdownCallbacks = [];
    }

    _addFolderNameEntry() {
        this._entryBox = new St.BoxLayout({
            style_class: 'folder-name-container',
        });
        this._viewBox.add_child(this._entryBox);

        // Empty actor to center the title
        let ghostButton = new Clutter.Actor();
        this._entryBox.add_child(ghostButton);

        let stack = new Shell.Stack({
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._entryBox.add_child(stack);

        // Folder name label
        this._folderNameLabel = new St.Label({
            style_class: 'folder-name-label',
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        stack.add_child(this._folderNameLabel);

        // Folder name entry
        this._entry = new St.Entry({
            style_class: 'folder-name-entry',
            opacity: 0,
            reactive: false,
        });
        this._entry.clutter_text.set({
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });

        this._entry.clutter_text.connect('activate', () => {
            this._showFolderLabel();
        });

        stack.add_child(this._entry);

        // Edit button
        this._editButton = new St.Button({
            style_class: 'edit-folder-button',
            button_mask: St.ButtonMask.ONE,
            toggle_mode: true,
            reactive: true,
            can_focus: true,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
            child: new St.Icon({
                icon_name: 'document-edit-symbolic',
                icon_size: 16,
            }),
        });

        this._editButton.connect('notify::checked', () => {
            if (this._editButton.checked)
                this._showFolderEntry();
            else
                this._showFolderLabel();
        });

        this._entryBox.add_child(this._editButton);

        ghostButton.add_constraint(new Clutter.BindConstraint({
            source: this._editButton,
            coordinate: Clutter.BindCoordinate.SIZE,
        }));

        this._folder.connect('changed::name', () => this._syncFolderName());
        this._syncFolderName();
    }

    _syncFolderName() {
        let newName = _getFolderName(this._folder);

        this._folderNameLabel.text = newName;
        this._entry.text = newName;
    }

    _switchActor(from, to) {
        to.reactive = true;
        to.ease({
            opacity: 255,
            duration: 300,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        from.ease({
            opacity: 0,
            duration: 300,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                from.reactive = false;
            },
        });
    }

    _showFolderLabel() {
        if (this._editButton.checked)
            this._editButton.checked = false;

        this._maybeUpdateFolderName();
        this._switchActor(this._entry, this._folderNameLabel);
    }

    _showFolderEntry() {
        this._switchActor(this._folderNameLabel, this._entry);

        this._entry.clutter_text.set_selection(0, -1);
        this._entry.clutter_text.grab_key_focus();
    }

    _maybeUpdateFolderName() {
        let folderName = _getFolderName(this._folder);
        let newFolderName = this._entry.text.trim();

        if (newFolderName.length === 0 || newFolderName === folderName)
            return;

        this._folder.set_string('name', newFolderName);
        this._folder.set_boolean('translate', false);
    }

    _zoomAndFadeIn() {
        let [sourceX, sourceY] =
            this._source.get_transformed_position();
        let [dialogX, dialogY] =
            this.child.get_transformed_position();

        this.child.set({
            translation_x: sourceX - dialogX,
            translation_y: sourceY - dialogY,
            scale_x: this._source.width / this.child.width,
            scale_y: this._source.height / this.child.height,
            opacity: 0,
        });

        this.ease({
            background_color: DIALOG_SHADE_NORMAL,
            duration: FOLDER_DIALOG_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
        this.child.ease({
            translation_x: 0,
            translation_y: 0,
            scale_x: 1,
            scale_y: 1,
            opacity: 255,
            duration: FOLDER_DIALOG_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this._needsZoomAndFade = false;

        if (this._sourceMappedId === 0) {
            this._sourceMappedId = this._source.connect(
                'notify::mapped', this._zoomAndFadeOut.bind(this));
        }
    }

    _zoomAndFadeOut() {
        if (!this._isOpen)
            return;

        if (!this._source.mapped) {
            this.hide();
            return;
        }

        let [sourceX, sourceY] =
            this._source.get_transformed_position();
        let [dialogX, dialogY] =
            this.child.get_transformed_position();

        this.ease({
            background_color: Clutter.Color.from_pixel(0x00000000),
            duration: FOLDER_DIALOG_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this.child.ease({
            translation_x: sourceX - dialogX,
            translation_y: sourceY - dialogY,
            scale_x: this._source.width / this.child.width,
            scale_y: this._source.height / this.child.height,
            opacity: 0,
            duration: FOLDER_DIALOG_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this.child.set({
                    translation_x: 0,
                    translation_y: 0,
                    scale_x: 1,
                    scale_y: 1,
                    opacity: 255,
                });
                this.hide();

                this._popdownCallbacks.forEach(func => func());
                this._popdownCallbacks = [];
            },
        });

        this._needsZoomAndFade = false;
    }

    _removeDragMonitor() {
        if (!this._dragMonitor)
            return;

        DND.removeDragMonitor(this._dragMonitor);
        this._dragMonitor = null;
    }

    _removePopdownTimeout() {
        if (this._popdownTimeoutId === 0)
            return;

        GLib.source_remove(this._popdownTimeoutId);
        this._popdownTimeoutId = 0;
    }

    _onDestroy() {
        if (this._isOpen) {
            this._isOpen = false;
            this._grabHelper.ungrab({ actor: this });
            this._grabHelper = null;
        }

        if (this._sourceMappedId) {
            this._source.disconnect(this._sourceMappedId);
            this._sourceMappedId = 0;
        }

        this._removePopdownTimeout();
        this._removeDragMonitor();
    }

    vfunc_allocate(box) {
        super.vfunc_allocate(box);

        // We can only start zooming after receiving an allocation
        if (this._needsZoomAndFade)
            this._zoomAndFadeIn();
    }

    vfunc_key_press_event(keyEvent) {
        if (global.stage.get_key_focus() != this)
            return Clutter.EVENT_PROPAGATE;

        // Since we need to only grab focus on one item child when the user
        // actually press a key we don't use navigate_focus when opening
        // the popup.
        // Instead of that, grab the focus on the AppFolderPopup actor
        // and actually moves the focus to a child only when the user
        // actually press a key.
        // It should work with just grab_key_focus on the AppFolderPopup
        // actor, but since the arrow keys are not wrapping_around the focus
        // is not grabbed by a child when the widget that has the current focus
        // is the same that is requesting focus, so to make it works with arrow
        // keys we need to connect to the key-press-event and navigate_focus
        // when that happens using TAB_FORWARD or TAB_BACKWARD instead of arrow
        // keys

        // Use TAB_FORWARD for down key and right key
        // and TAB_BACKWARD for up key and left key on ltr
        // languages
        let direction;
        let isLtr = Clutter.get_default_text_direction() == Clutter.TextDirection.LTR;
        switch (keyEvent.keyval) {
        case Clutter.KEY_Down:
            direction = St.DirectionType.TAB_FORWARD;
            break;
        case Clutter.KEY_Right:
            direction = isLtr
                ? St.DirectionType.TAB_FORWARD
                : St.DirectionType.TAB_BACKWARD;
            break;
        case Clutter.KEY_Up:
            direction = St.DirectionType.TAB_BACKWARD;
            break;
        case Clutter.KEY_Left:
            direction = isLtr
                ? St.DirectionType.TAB_BACKWARD
                : St.DirectionType.TAB_FORWARD;
            break;
        default:
            return Clutter.EVENT_PROPAGATE;
        }
        return this.navigate_focus(null, direction, false);
    }

    _setLighterBackground(lighter) {
        const backgroundColor = lighter
            ? DIALOG_SHADE_HIGHLIGHT
            : DIALOG_SHADE_NORMAL;

        this.ease({
            backgroundColor,
            duration: FOLDER_DIALOG_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _withinDialog(x, y) {
        const childExtents = this.child.get_transformed_extents();
        return childExtents.contains_point(new Graphene.Point({ x, y }));
    }

    _setupDragMonitor() {
        if (this._dragMonitor)
            return;

        this._dragMonitor = {
            dragMotion: dragEvent => {
                const withinDialog =
                    this._withinDialog(dragEvent.x, dragEvent.y);

                this._setLighterBackground(!withinDialog);

                if (withinDialog) {
                    this._removePopdownTimeout();
                    this._removeDragMonitor();
                }
                return DND.DragMotionResult.CONTINUE;
            },
        };
        DND.addDragMonitor(this._dragMonitor);
    }

    _setupPopdownTimeout() {
        if (this._popdownTimeoutId > 0)
            return;

        this._popdownTimeoutId =
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, POPDOWN_DIALOG_TIMEOUT, () => {
                this._popdownTimeoutId = 0;
                this.popdown();
                return GLib.SOURCE_REMOVE;
            });
    }

    handleDragOver(source, actor, x, y) {
        if (this._withinDialog(x, y)) {
            this._setLighterBackground(false);
            this._removePopdownTimeout();
            this._removeDragMonitor();
        } else {
            this._setupPopdownTimeout();
            this._setupDragMonitor();
        }

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source) {
        const appId = source.id;

        this.popdown(() => {
            this._view.removeApp(source);
            this._appDisplay.selectApp(appId);
        });

        return true;
    }

    toggle() {
        if (this._isOpen)
            this.popdown();
        else
            this.popup();
    }

    popup() {
        if (this._isOpen)
            return;

        this._isOpen = this._grabHelper.grab({
            actor: this,
            onUngrab: () => this.popdown(),
        });

        if (!this._isOpen)
            return;

        this.get_parent().set_child_above_sibling(this, null);

        this._needsZoomAndFade = true;
        this.show();

        this.emit('open-state-changed', true);
    }

    popdown(callback) {
        // Either call the callback right away, or wait for the zoom out
        // animation to finish
        if (callback) {
            if (this.visible)
                this._popdownCallbacks.push(callback);
            else
                callback();
        }

        if (!this._isOpen)
            return;

        this._zoomAndFadeOut();
        this._showFolderLabel();

        this._isOpen = false;
        this._grabHelper.ungrab({ actor: this });
        this.emit('open-state-changed', false);
    }
});

var AppIcon = GObject.registerClass({
    Signals: {
        'menu-state-changed': { param_types: [GObject.TYPE_BOOLEAN] },
        'sync-tooltip': {},
    },
}, class AppIcon extends AppViewItem {
    _init(app, iconParams = {}) {
        // Get the isDraggable property without passing it on to the BaseIcon:
        const appIconParams = Params.parse(iconParams, { isDraggable: true }, true);
        const isDraggable = appIconParams['isDraggable'];
        delete iconParams['isDraggable'];
        const expandTitleOnHover = appIconParams['expandTitleOnHover'];
        delete iconParams['expandTitleOnHover'];

        super._init({ style_class: 'app-well-app' }, isDraggable, expandTitleOnHover);

        this.app = app;
        this._id = app.get_id();
        this._name = app.get_name();

        this._iconContainer = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                              x_expand: true, y_expand: true });

        this.set_child(this._iconContainer);

        this._folderPreviewId = 0;

        iconParams['createIcon'] = this._createIcon.bind(this);
        iconParams['setSizeManually'] = true;
        this.icon = new IconGrid.BaseIcon(app.get_name(), iconParams);
        this._iconContainer.add_child(this.icon);

        this._dot = new St.Widget({
            style_class: 'app-well-app-running-dot',
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.END,
        });
        this._iconContainer.add_child(this._dot);

        this.label_actor = this.icon.label;

        this.connect('popup-menu', this._onKeyboardPopupMenu.bind(this));

        this._menu = null;
        this._menuManager = new PopupMenu.PopupMenuManager(this);

        this._menuTimeoutId = 0;
        this._stateChangedId = this.app.connect('notify::state', () => {
            this._updateRunningStyle();
        });
        this._updateRunningStyle();
    }

    _onDestroy() {
        super._onDestroy();

        if (this._folderPreviewId > 0) {
            GLib.source_remove(this._folderPreviewId);
            this._folderPreviewId = 0;
        }
        if (this._stateChangedId > 0)
            this.app.disconnect(this._stateChangedId);

        this._stateChangedId = 0;
        this._removeMenuTimeout();
    }

    _onDragBegin() {
        if (this._menu)
            this._menu.close(true);
        this._removeMenuTimeout();
        super._onDragBegin();
    }

    _createIcon(iconSize) {
        return this.app.create_icon_texture(iconSize);
    }

    _removeMenuTimeout() {
        if (this._menuTimeoutId > 0) {
            GLib.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
    }

    _updateRunningStyle() {
        if (this.app.state != Shell.AppState.STOPPED)
            this._dot.show();
        else
            this._dot.hide();
    }

    _setPopupTimeout() {
        this._removeMenuTimeout();
        this._menuTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, MENU_POPUP_TIMEOUT, () => {
            this._menuTimeoutId = 0;
            this.popupMenu();
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(this._menuTimeoutId, '[gnome-shell] this.popupMenu');
    }

    vfunc_leave_event(crossingEvent) {
        const ret = super.vfunc_leave_event(crossingEvent);

        this.fake_release();
        this._removeMenuTimeout();
        return ret;
    }

    vfunc_button_press_event(buttonEvent) {
        const ret = super.vfunc_button_press_event(buttonEvent);
        if (buttonEvent.button == 1) {
            this._setPopupTimeout();
        } else if (buttonEvent.button == 3) {
            this.popupMenu();
            return Clutter.EVENT_STOP;
        }
        return ret;
    }

    vfunc_touch_event(touchEvent) {
        const ret = super.vfunc_touch_event(touchEvent);
        if (touchEvent.type == Clutter.EventType.TOUCH_BEGIN)
            this._setPopupTimeout();

        return ret;
    }

    vfunc_clicked(button) {
        this._removeMenuTimeout();
        this.activate(button);
    }

    _onKeyboardPopupMenu() {
        this.popupMenu();
        this._menu.actor.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
    }

    getId() {
        return this.app.get_id();
    }

    popupMenu(side = St.Side.LEFT) {
        this._removeMenuTimeout();
        this.fake_release();

        if (!this._menu) {
            this._menu = new AppIconMenu(this, side);
            this._menu.connect('activate-window', (menu, window) => {
                this.activateWindow(window);
            });
            this._menu.connect('open-state-changed', (menu, isPoppedUp) => {
                if (!isPoppedUp)
                    this._onMenuPoppedDown();
            });
            let id = Main.overview.connect('hiding', () => {
                this._menu.close();
            });
            this.connect('destroy', () => {
                Main.overview.disconnect(id);
            });

            this._menuManager.addMenu(this._menu);
        }

        this.emit('menu-state-changed', true);

        this.set_hover(true);
        this._menu.popup();
        this._menuManager.ignoreRelease();
        this.emit('sync-tooltip');

        return false;
    }

    activateWindow(metaWindow) {
        if (metaWindow)
            Main.activateWindow(metaWindow);
        else
            Main.overview.hide();
    }

    _onMenuPoppedDown() {
        this.sync_hover();
        this.emit('menu-state-changed', false);
    }

    activate(button) {
        let event = Clutter.get_current_event();
        let modifiers = event ? event.get_state() : 0;
        let isMiddleButton = button && button == Clutter.BUTTON_MIDDLE;
        let isCtrlPressed = (modifiers & Clutter.ModifierType.CONTROL_MASK) != 0;
        let openNewWindow = this.app.can_open_new_window() &&
                            this.app.state == Shell.AppState.RUNNING &&
                            (isCtrlPressed || isMiddleButton);

        if (this.app.state == Shell.AppState.STOPPED || openNewWindow)
            this.animateLaunch();

        if (openNewWindow)
            this.app.open_new_window(-1);
        else
            this.app.activate();

        Main.overview.hide();
    }

    animateLaunch() {
        this.icon.animateZoomOut();
    }

    animateLaunchAtPos(x, y) {
        this.icon.animateZoomOutAtPos(x, y);
    }

    shellWorkspaceLaunch(params) {
        let { stack } = new Error();
        log('shellWorkspaceLaunch is deprecated, use app.open_new_window() instead\n%s'.format(stack));

        params = Params.parse(params, { workspace: -1,
                                        timestamp: 0 });

        this.app.open_new_window(params.workspace);
    }

    getDragActor() {
        return this.app.create_icon_texture(Main.overview.dash.iconSize);
    }

    // Returns the original actor that should align with the actor
    // we show as the item is being dragged.
    getDragActorSource() {
        return this.icon.icon;
    }

    shouldShowTooltip() {
        return this.hover && (!this._menu || !this._menu.isOpen);
    }

    _showFolderPreview() {
        this.icon.label.opacity = 0;
        this.icon.icon.ease({
            scale_x: FOLDER_SUBICON_FRACTION,
            scale_y: FOLDER_SUBICON_FRACTION,
        });
    }

    _hideFolderPreview() {
        this.icon.label.opacity = 255;
        this.icon.icon.ease({
            scale_x: 1.0,
            scale_y: 1.0,
        });
    }

    _canAccept(source) {
        let view = _getViewFromIcon(source);

        return source != this &&
               (source instanceof this.constructor) &&
               (view instanceof AppDisplay);
    }

    _setHoveringByDnd(hovering) {
        if (this._otherIconIsHovering == hovering)
            return;

        super._setHoveringByDnd(hovering);

        if (hovering) {
            if (this._folderPreviewId > 0)
                return;

            this._folderPreviewId =
                GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, () => {
                    this.add_style_pseudo_class('drop');
                    this._showFolderPreview();
                    this._folderPreviewId = 0;
                    return GLib.SOURCE_REMOVE;
                });
        } else {
            if (this._folderPreviewId > 0) {
                GLib.source_remove(this._folderPreviewId);
                this._folderPreviewId = 0;
            }
            this._hideFolderPreview();
            this.remove_style_pseudo_class('drop');
        }
    }

    acceptDrop(source, actor, x) {
        const accepted = super.acceptDrop(source, actor, x);
        if (!accepted)
            return false;

        let view = _getViewFromIcon(this);
        let apps = [this.id, source.id];

        return view.createFolder(apps);
    }

    cancelActions() {
        if (this._menu)
            this._menu.close(true);
        this._removeMenuTimeout();
        super.cancelActions();
    }
});

var AppIconMenu = class AppIconMenu extends PopupMenu.PopupMenu {
    constructor(source, side) {
        if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL) {
            if (side === St.Side.LEFT)
                side = St.Side.RIGHT;
            else if (side === St.Side.RIGHT)
                side = St.Side.LEFT;
        }

        super(source, 0.5, side);

        // We want to keep the item hovered while the menu is up
        this.blockSourceEvents = true;

        this._source = source;

        this._parentalControlsManager = ParentalControlsManager.getDefault();

        this.actor.add_style_class_name('app-well-menu');

        // Chain our visibility and lifecycle to that of the source
        this._sourceMappedId = source.connect('notify::mapped', () => {
            if (!source.mapped)
                this.close();
        });
        source.connect('destroy', () => {
            source.disconnect(this._sourceMappedId);
            this.destroy();
        });

        Main.uiGroup.add_actor(this.actor);
    }

    _rebuildMenu() {
        this.removeAll();

        let windows = this._source.app.get_windows().filter(
            w => !w.skip_taskbar);

        if (windows.length > 0) {
            this.addMenuItem(
                /* Translators: This is the heading of a list of open windows */
                new PopupMenu.PopupSeparatorMenuItem(_('Open Windows')));
        }

        windows.forEach(window => {
            let title = window.title
                ? window.title : this._source.app.get_name();
            let item = this._appendMenuItem(title);
            item.connect('activate', () => {
                this.emit('activate-window', window);
            });
        });

        if (!this._source.app.is_window_backed()) {
            this._appendSeparator();

            let appInfo = this._source.app.get_app_info();
            let actions = appInfo.list_actions();
            if (this._source.app.can_open_new_window() &&
                !actions.includes('new-window')) {
                this._newWindowMenuItem = this._appendMenuItem(_("New Window"));
                this._newWindowMenuItem.connect('activate', () => {
                    this._source.animateLaunch();
                    this._source.app.open_new_window(-1);
                    this.emit('activate-window', null);
                });
                this._appendSeparator();
            }

            if (discreteGpuAvailable &&
                this._source.app.state == Shell.AppState.STOPPED) {
                const appPrefersNonDefaultGPU = appInfo.get_boolean('PrefersNonDefaultGPU');
                const gpuPref = appPrefersNonDefaultGPU
                    ? Shell.AppLaunchGpu.DEFAULT
                    : Shell.AppLaunchGpu.DISCRETE;
                this._onGpuMenuItem = this._appendMenuItem(appPrefersNonDefaultGPU
                    ? _('Launch using Integrated Graphics Card')
                    : _('Launch using Discrete Graphics Card'));
                this._onGpuMenuItem.connect('activate', () => {
                    this._source.animateLaunch();
                    this._source.app.launch(0, -1, gpuPref);
                    this.emit('activate-window', null);
                });
            }

            for (let i = 0; i < actions.length; i++) {
                let action = actions[i];
                let item = this._appendMenuItem(appInfo.get_action_name(action));
                item.connect('activate', (emitter, event) => {
                    if (action == 'new-window')
                        this._source.animateLaunch();

                    this._source.app.launch_action(action, event.get_time(), -1);
                    this.emit('activate-window', null);
                });
            }

            let canFavorite = global.settings.is_writable('favorite-apps') &&
                              this._parentalControlsManager.shouldShowApp(this._source.app.app_info);

            if (canFavorite) {
                this._appendSeparator();

                let isFavorite = AppFavorites.getAppFavorites().isFavorite(this._source.app.get_id());

                if (isFavorite) {
                    let item = this._appendMenuItem(_("Remove from Favorites"));
                    item.connect('activate', () => {
                        let favs = AppFavorites.getAppFavorites();
                        favs.removeFavorite(this._source.app.get_id());
                    });
                } else {
                    let item = this._appendMenuItem(_("Add to Favorites"));
                    item.connect('activate', () => {
                        let favs = AppFavorites.getAppFavorites();
                        favs.addFavorite(this._source.app.get_id());
                    });
                }
            }

            if (Shell.AppSystem.get_default().lookup_app('org.gnome.Software.desktop')) {
                this._appendSeparator();
                this.addAction(_('Show Details'), async () => {
                    let id = this._source.app.get_id();
                    let args = GLib.Variant.new('(ss)', [id, '']);
                    const bus = await Gio.DBus.get(Gio.BusType.SESSION, null);
                    bus.call(
                        'org.gnome.Software',
                        '/org/gnome/Software',
                        'org.gtk.Actions', 'Activate',
                        new GLib.Variant.new(
                            '(sava{sv})', ['details', [args], null]),
                        null, 0, -1, null);
                    Main.overview.hide();
                });
            }
        }
    }

    _appendSeparator() {
        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this.addMenuItem(separator);
    }

    _appendMenuItem(labelText) {
        // FIXME: app-well-menu-item style
        let item = new PopupMenu.PopupMenuItem(labelText);
        this.addMenuItem(item);
        return item;
    }

    popup(_activatingButton) {
        this._rebuildMenu();
        this.open(BoxPointer.PopupAnimation.FULL);
    }
};
Signals.addSignalMethods(AppIconMenu.prototype);

var SystemActionIcon = GObject.registerClass(
class SystemActionIcon extends Search.GridSearchResult {
    activate() {
        SystemActions.getDefault().activateAction(this.metaInfo['id']);
        Main.overview.hide();
    }
});
