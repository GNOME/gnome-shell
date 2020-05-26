// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AppDisplay, AppSearchProvider */

const { Clutter, Gio, GLib, GObject, Graphene, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppFavorites = imports.ui.appFavorites;
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
const Util = imports.misc.util;
const SystemActions = imports.misc.systemActions;

var MENU_POPUP_TIMEOUT = 600;

var FOLDER_SUBICON_FRACTION = .4;

var VIEWS_SWITCH_TIME = 400;
var VIEWS_SWITCH_ANIMATION_DELAY = 100;

var SCROLL_TIMEOUT_TIME = 150;

var APP_ICON_SCALE_IN_TIME = 500;
var APP_ICON_SCALE_IN_DELAY = 700;

const FOLDER_DIALOG_ANIMATION_TIME = 200;

const OVERSHOOT_THRESHOLD = 20;
const OVERSHOOT_TIMEOUT = 1000;

const DELAYED_MOVE_TIMEOUT = 750;

let discreteGpuAvailable = false;

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
        'use-pagination': GObject.ParamSpec.boolean(
            'use-pagination', 'use-pagination', 'use-pagination',
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            false),
    },
    Signals: {
        'view-loaded': {},
    },
}, class BaseAppView extends St.Widget {
    _init(params = {}, orientation = Clutter.Orientation.VERTICAL) {
        super._init(params);

        this._grid = this._createGrid();
        // Standard hack for ClutterBinLayout
        this._grid.x_expand = true;

        const vertical = orientation === Clutter.Orientation.VERTICAL;

        // Scroll View
        this._scrollView = new St.ScrollView({
            clip_to_allocation: true,
            x_expand: true,
            y_expand: true,
            reactive: true,
        });
        this._scrollView.set_policy(
            vertical ? St.PolicyType.NEVER : St.PolicyType.EXTERNAL,
            vertical ? St.PolicyType.EXTERNAL : St.PolicyType.NEVER);

        this._canScroll = true; // limiting scrolling speed
        this._scrollTimeoutId = 0;
        this._scrollView.connect('scroll-event', this._onScroll.bind(this));

        this._scrollView.add_actor(this._grid);

        const scroll = vertical ?  this._scrollView.vscroll : this._scrollView.hscroll;
        this._adjustment = scroll.adjustment;
        this._adjustment.connect('notify::value', adj => {
            this._pageIndicators.setCurrentPosition(adj.value / adj.page_size);
        });

        // Page Indicators
        if (vertical)
            this._pageIndicators = new PageIndicators.AnimatedPageIndicators();
        else
            this._pageIndicators = new PageIndicators.PageIndicators(orientation);

        this._pageIndicators.y_expand = vertical;
        this._pageIndicators.connect('page-activated',
            (indicators, pageIndex) => {
                this.goToPage(pageIndex);
            });
        this._pageIndicators.connect('scroll-event', (actor, event) => {
            this._scrollView.event(event, false);
        });

        // Swipe
        this._swipeTracker = new SwipeTracker.SwipeTracker(this._scrollView,
            Shell.ActionMode.OVERVIEW | Shell.ActionMode.POPUP);
        this._swipeTracker.orientation = orientation;
        this._swipeTracker.connect('begin', this._swipeBegin.bind(this));
        this._swipeTracker.connect('update', this._swipeUpdate.bind(this));
        this._swipeTracker.connect('end', this._swipeEnd.bind(this));

        this._availWidth = 0;
        this._availHeight = 0;
        this._orientation = orientation;

        this._items = new Map();
        this._orderedItems = [];

        this._animateLaterId = 0;
        this._viewLoadedHandlerId = 0;
        this._viewIsReady = false;

        // Filter the apps through the user’s parental controls.
        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._parentalControlsManager.connect('app-filter-changed', () => {
            this._redisplay();
        });
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

    _swipeBegin(tracker, monitor) {
        if (monitor !== Main.layoutManager.primaryIndex)
            return;

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

        adjustment.ease(value, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration,
            onComplete: () => this.goToPage(endProgress, false),
        });
    }

    _addItem(item, page, position) {
        let itemIndex = position;

        for (let i = 0; i < page - 1; i++) {
            const pageItems = this._grid.getItemsAtPage(i).filter(c => c.visible);
            itemIndex += pageItems.length;
        }

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

        // Add new app icons
        const { itemsPerPage } = this._grid;
        addedApps.forEach(icon => {
            let iconIndex = newApps.indexOf(icon);

            this._orderedItems.splice(iconIndex, 0, icon);
            this._items.set(icon.id, icon);

            const page = Math.floor(iconIndex / itemsPerPage);
            const position = iconIndex % itemsPerPage;
            this._grid.addItem(icon, page, position);
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
        this._grid.opacity = 255;
    }

    animate(animationDirection, onComplete) {
        if (onComplete) {
            let animationDoneId = this._grid.connect('animation-done', () => {
                this._grid.disconnect(animationDoneId);
                onComplete();
            });
        }

        this._clearAnimateLater();

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
                        doSpringAnimationLater(Meta.LaterType.BEFORE_REDRAW);
                    });
            }
        } else {
            this._doSpringAnimation(animationDirection);
        }
    }

    vfunc_allocate(box) {
        const width = box.get_width();
        const height = box.get_height();

        this.adaptToSize(width, height);

        super.vfunc_allocate(box);
    }

    vfunc_map() {
        this._swipeTracker.enabled = true;
        super.vfunc_map();
    }

    vfunc_unmap() {
        this._swipeTracker.enabled = false;
        this._clearAnimateLater();
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

    goToPage(pageNumber, animate = true) {
        pageNumber = Math.clamp(pageNumber, 0, this._grid.nPages - 1);

        if (this._grid.currentPage === pageNumber)
            return;

        this._grid.goToPage(pageNumber, animate);
    }

    getDropTarget(x, y) {
        let [item, dragLocation] = this._grid.getDropTarget(x, y);

        // Append to the page if dragging over empty area
        if (dragLocation === IconGrid.DragLocation.EMPTY_SPACE) {
            const currentPage = this._grid.currentPage;
            const pageItems =
                this._grid.getItemsAtPage(currentPage).filter(c => c.visible);

            item = pageItems[pageItems.length - 1];
            dragLocation = IconGrid.DragLocation.END_EDGE;
        }

        return [item, dragLocation];
    }

    moveItem(item, newPage, newPosition) {
        if (this._grid.contains(item))
            this._removeItem(item);

        this._addItem(item, newPage, newPosition);
    }

    adaptToSize(width, height) {
        let box = new Clutter.ActorBox({
            x2: width,
            y2: height,
        });
        box = this._scrollView.get_theme_node().get_content_box(box);
        box = this._grid.get_theme_node().get_content_box(box);

        const availWidth = box.get_width();
        const availHeight = box.get_height();

        this._grid.adaptToSize(availWidth, availHeight);

        if (this._availWidth !== availWidth ||
            this._availHeight !== availHeight ||
            this._pageIndicators.nPages !== this._grid.nPages) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._adjustment.value = 0;
                this._grid.currentPage = 0;
                this._pageIndicators.setNPages(this._grid.nPages);
                this._pageIndicators.setCurrentPosition(0);
                return GLib.SOURCE_REMOVE;
            });
        }

        this._availWidth = availWidth;
        this._availHeight = availHeight;
    }
});

var PageManager = GObject.registerClass({
    Signals: { 'layout-changed': {} },
}, class PageManager extends GObject.Object {
    _init() {
        super._init();

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });
        this._settings.connect('changed::grid-layout', this._loadPages.bind(this));

        this._loadPages();
    }

    _loadPages() {
        this._pages = this._settings.get_value('grid-layout').recursiveUnpack();
        this.emit('layout-changed');
    }

    getAppPosition(appId) {
        let position = -1;
        let page = -1;

        for (let pageIndex = 0; pageIndex < this._pages.length; pageIndex++) {
            const pageData = this._pages[pageIndex];

            if (!(appId in pageData))
                continue;

            page = pageIndex;
            position = pageData[appId].position;
        }

        return [page, position];
    }

    set pages(p) {
        const packedPages = [];

        // Pack the icon properties as a GVariant
        for (let page of p) {
            const pageData = {};
            for (let [appId, properties] of Object.entries(page)) {
                const variant = new GLib.Variant('a{sv}', properties);
                pageData[appId] = new GLib.Variant('v', variant);
            }
            packedPages.push(pageData);
        }

        const variant = new GLib.Variant('aa{sv}', packedPages);
        this._settings.set_value('grid-layout', variant);
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

        this._grid._delegate = this;
        this._pageManager = new PageManager();

        this._scrollView.add_style_class_name('all-apps');

        this._stack = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
        });
        this.add_actor(this._stack);
        this._stack.add_actor(this._scrollView);

        this.add_actor(this._pageIndicators);

        this._folderIcons = [];

        this._currentDialog = null;
        this._displayingDialog = false;
        this._currentDialogDestroyId = 0;

        this._lastOvershootY = -1;
        this._lastOvershootTimeoutId = 0;
        this._delayedMoveId = 0;
        this._targetDropPosition = null;
        this._nudgedItem = null;

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

        Main.overview.connect('item-drag-begin', this._onDragBegin.bind(this));
        Main.overview.connect('item-drag-end', this._onDragEnd.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));

        this._switcherooNotifyId = global.connect('notify::switcheroo-control',
            () => this._updateDiscreteGpuAvailable());
        this._updateDiscreteGpuAvailable();
    }

    _updateDiscreteGpuAvailable() {
        this._switcherooProxy = global.get_switcheroo_control();
        if (this._switcherooProxy) {
            let prop = this._switcherooProxy.get_cached_property('HasDualGpu');
            discreteGpuAvailable = prop ? prop.unpack() : false;
        } else {
            discreteGpuAvailable = false;
        }
    }

    _onDestroy() {
        this._removeDelayedMove();

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

    _itemNameChanged(item) {
        // If an item's name changed, we can pluck it out of where it's
        // supposed to be and reinsert it where it's sorted.
        let oldIdx = this._orderedItems.indexOf(item);
        this._orderedItems.splice(oldIdx, 1);
        let newIdx = Util.insertSorted(this._orderedItems, item, this._compareItems.bind(this));

        this._grid.removeItem(item);

        const { itemsPerPage } = this._grid;
        const page = Math.floor(newIdx / itemsPerPage);
        const position = newIdx % itemsPerPage;
        this._grid.addItem(item, page, position);

        this.selectApp(item.id);
    }

    getAppInfos() {
        return this._appInfoList;
    }

    _loadApps() {
        let appIcons = [];
        this._appInfoList = Shell.AppSystem.get_default().get_installed().filter(appInfo => {
            try {
                appInfo.get_id(); // catch invalid file encodings
            } catch (e) {
                return false;
            }
            return this._parentalControlsManager.shouldShowApp(appInfo);
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
                icon.connect('name-changed', this._itemNameChanged.bind(this));
                icon.connect('apps-changed', this._redisplay.bind(this));
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
        let favoritesWritable = global.settings.is_writable('favorite-apps');

        apps.forEach(appId => {
            if (appsInsideFolders.has(appId))
                return;

            let icon = this._items.get(appId);
            if (!icon) {
                let app = appSys.lookup_app(appId);

                icon = new AppIcon(app, {
                    isDraggable: favoritesWritable,
                });
            }

            appIcons.push(icon);
        });

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
            this._displayingDialog && this._currentDialog) {
            this._currentDialog.popdown();
        } else {
            super.animate(animationDirection, completionFunc);
            if (animationDirection == IconGrid.AnimationDirection.OUT)
                this._pageIndicators.animateIndicators(animationDirection);
        }
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

        if (animationDirection == IconGrid.AnimationDirection.OUT)
            this._pageIndicators.animateIndicators(animationDirection);
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

    _removeDelayedMove() {
        if (this._delayedMoveId > 0) {
            GLib.source_remove(this._delayedMoveId);
            this._delayedMoveId = 0;
        }
        this._targetDropPosition = null;
    }

    _nudgeItem(item, dragLocation) {
        if (this._nudgedItem)
            this._removeNudge();

        const params = {
            duration: DELAYED_MOVE_TIMEOUT,
            mode: Clutter.AnimationMode.EASE_IN_OUT_CUBIC,
        };

        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const nudgeOffset = item.width / 4;
        if (dragLocation === IconGrid.DragLocation.START_EDGE)
            params.translation_x = nudgeOffset * (rtl ? -1 : 1);
        else if (dragLocation === IconGrid.DragLocation.END_EDGE)
            params.translation_x = -nudgeOffset * (rtl ? -1 : 1);

        item.ease(params);
        this._nudgedItem = item;
    }

    _removeNudge() {
        if (!this._nudgedItem)
            return;

        this._nudgedItem.ease({
            translation_x: 0,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
        this._nudgedItem = null;
    }

    _maybeMoveItem(dragEvent) {
        const [success, x, y] =
            this._grid.transform_stage_point(dragEvent.x, dragEvent.y);

        if (!success)
            return;

        const { source } = dragEvent;
        const [item, dragLocation] = this.getDropTarget(x, y);

        // Dragging over invalid parts of the grid cancels the timeout
        if (!item ||
            item === source ||
            dragLocation === IconGrid.DragLocation.ON_ICON) {
            this._removeDelayedMove();
            this._removeNudge();
            return;
        }

        const [page, position] = this._grid.getItemPosition(item);

        if (!this._targetDropPosition ||
            this._targetDropPosition.page !== page ||
            this._targetDropPosition.position !== position) {
            // Update the item with a small delay
            this._removeDelayedMove();
            this._targetDropPosition = { page, position };

            this._delayedMoveId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                DELAYED_MOVE_TIMEOUT, () => {
                    this.moveItem(source, page, position);
                    this._removeNudge();
                    this._targetDropPosition = null;
                    this._delayedMoveId = 0;
                    return GLib.SOURCE_REMOVE;
                });

            this._nudgeItem(item, dragLocation);
        }
    }

    _resetOvershoot() {
        if (this._lastOvershootTimeoutId)
            GLib.source_remove(this._lastOvershootTimeoutId);
        this._lastOvershootTimeoutId = 0;
        this._lastOvershootY = -1;
    }

    _handleDragOvershoot(dragEvent) {
        let [, gridY] = this.get_transformed_position();
        let [, gridHeight] = this.get_transformed_size();
        const gridBottom = gridY + gridHeight - OVERSHOOT_THRESHOLD;

        // Already animating
        if (this._adjustment.get_transition('value') !== null)
            return;

        // Within the grid boundaries
        if (dragEvent.y > gridY && dragEvent.y < gridBottom) {
            // Check whether we moved out the area of the last switch
            if (Math.abs(this._lastOvershootY - dragEvent.y) > OVERSHOOT_THRESHOLD)
                this._resetOvershoot();

            return;
        }

        // Still in the area of the previous page switch
        if (this._lastOvershootY >= 0)
            return;

        let currentY = this._adjustment.value;
        let maxY = this._adjustment.upper - this._adjustment.page_size;

        if (dragEvent.y <= gridY && currentY > 0)
            this.goToPage(this._grid.currentPage - 1);
        else if (dragEvent.y >= gridBottom && currentY < maxY)
            this.goToPage(this._grid.currentPage + 1);
        else
            return; // don't go beyond first/last page

        this._lastOvershootY = dragEvent.y;

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
    }

    _onDragMotion(dragEvent) {
        if (!(dragEvent.source instanceof AppIcon))
            return DND.DragMotionResult.CONTINUE;

        let appIcon = dragEvent.source;

        // Handle the drag overshoot. When dragging to above the
        // icon grid, move to the page above; when dragging below,
        // move to the page below.
        if (this._grid.contains(appIcon))
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
        this._removeNudge();
    }

    _canAccept(source) {
        return (source instanceof AppIcon) ||
            (source instanceof FolderIcon);
    }

    handleDragOver(source) {
        if (!this._canAccept(source))
            return DND.DragMotionResult.NO_DROP;

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source) {
        if (!this._canAccept(source))
            return false;

        let view = _getViewFromIcon(source);
        if (view instanceof FolderView)
            view.removeApp(source.app);

        // Dropped before the icon was moved
        if (this._targetDropPosition) {
            const { page, position } = this._targetDropPosition;

            this.moveItem(source, page, position);
            this._removeDelayedMove();
        }

        if (this._currentDialog)
            this._currentDialog.popdown();

        return true;
    }

    createFolder(apps) {
        let newFolderId = GLib.uuid_string_random();

        let folders = this._folderSettings.get_strv('folder-children');
        folders.push(newFolderId);
        this._folderSettings.set_strv('folder-children', folders);

        // Create the new folder
        let newFolderPath = this._folderSettings.path.concat('folders/', newFolderId, '/');
        let newFolderSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.app-folders.folder',
            path: newFolderPath,
        });
        if (!newFolderSettings) {
            log('Error creating new folder');
            return false;
        }

        let appItems = apps.map(id => this._items.get(id).app);
        let folderName = _findBestFolderName(appItems);
        if (!folderName)
            folderName = _("Unnamed Folder");

        newFolderSettings.delay();
        newFolderSettings.set_string('name', folderName);
        newFolderSettings.set_strv('apps', apps);
        newFolderSettings.apply();

        this.selectApp(newFolderId);

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

                let createIcon = size => new St.Icon({ icon_name: iconName,
                                                       width: size,
                                                       height: size,
                                                       style_class: 'system-action-icon' });

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
        if (resultMeta.id.endsWith('.desktop'))
            return new AppIcon(this._appSys.lookup_app(resultMeta['id']));
        else
            return new SystemActionIcon(this, resultMeta);
    }
};

var FolderGrid = GObject.registerClass(
class FolderGrid extends IconGrid.IconGrid {
    _init() {
        super._init({
            allow_incomplete_pages: false,
            orientation: Clutter.Orientation.HORIZONTAL,
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
        }, Clutter.Orientation.HORIZONTAL);

        // If it not expand, the parent doesn't take into account its preferred_width when allocating
        // the second time it allocates, so we apply the "Standard hack for ClutterBinLayout"
        this._grid.x_expand = true;
        this._id = id;
        this._folder = folder;
        this._parentView = parentView;
        this._grid._delegate = this;

        const box = new St.BoxLayout({
            vertical: true,
            reactive: true,
            x_expand: true,
            y_expand: true,
        });
        box.add_child(this._scrollView);
        box.add_child(this._pageIndicators);
        this.add_child(box);

        let action = new Clutter.PanAction({ interpolate: true });
        action.connect('pan', this._onPan.bind(this));
        this._scrollView.add_action(action);

        this._redisplay();
    }

    _createGrid() {
        return new FolderGrid();
    }

    // Overridden from BaseAppView
    animate(animationDirection) {
        this._grid.animatePulse(animationDirection);
    }

    createFolderIcon(size) {
        let layout = new Clutter.GridLayout();
        let icon = new St.Widget({
            layout_manager: layout,
            style_class: 'app-folder-icon',
            x_align: Clutter.ActorAlign.CENTER,
        });
        layout.hookup_style(icon);
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
        let excludedApps = this._folder.get_strv('excluded-apps');
        let appSys = Shell.AppSystem.get_default();
        let addAppId = appId => {
            if (excludedApps.includes(appId))
                return;

            let app = appSys.lookup_app(appId);
            if (!app)
                return;

            if (!this._parentalControlsManager.shouldShowApp(app.get_app_info()))
                return;

            if (apps.some(appIcon => appIcon.id == appId))
                return;

            let icon = this._items.get(appId);
            if (!icon)
                icon = new AppIcon(app);

            apps.push(icon);
        };

        let folderApps = this._folder.get_strv('apps');
        folderApps.forEach(addAppId);

        let folderCategories = this._folder.get_strv('categories');
        let appInfos = this._parentView.getAppInfos();
        appInfos.forEach(appInfo => {
            let appCategories = _getCategories(appInfo);
            if (!_listsIntersect(folderCategories, appCategories))
                return;

            addAppId(appInfo.get_id());
        });

        return apps;
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

        // If this is a categories-based folder, also add it to
        // the list of excluded apps
        let categories = this._folder.get_strv('categories');
        if (categories.length > 0) {
            let excludedApps = this._folder.get_strv('excluded-apps');
            excludedApps.push(app.id);
            this._folder.set_strv('excluded-apps', excludedApps);
        }

        // Remove the folder if this is the last app icon; otherwise,
        // just remove the icon
        if (folderApps.length == 0) {
            // Resetting all keys deletes the relocatable schema
            let keys = this._folder.settings_schema.list_keys();
            for (let key of keys)
                this._folder.reset(key);

            let settings = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders' });
            let folders = settings.get_strv('folder-children');
            folders.splice(folders.indexOf(this._id), 1);
            settings.set_strv('folder-children', folders);
        } else {
            this._folder.set_strv('apps', folderApps);
        }
    }
});

var FolderIcon = GObject.registerClass({
    Signals: {
        'apps-changed': {},
        'name-changed': {},
    },
}, class FolderIcon extends St.Button {
    _init(id, path, parentView) {
        super._init({
            style_class: 'app-well-app app-folder',
            button_mask: St.ButtonMask.ONE,
            toggle_mode: true,
            can_focus: true,
        });
        this.id = id;
        this.name = '';
        this._parentView = parentView;

        this._folder = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders.folder',
                                          path });
        this._delegate = this;

        this.icon = new IconGrid.BaseIcon('', {
            createIcon: this._createIcon.bind(this),
            setSizeManually: true,
        });
        this.set_child(this.icon);
        this.label_actor = this.icon.label;

        this.view = new FolderView(this._folder, id, parentView);

        this._iconIsHovering = false;

        this.connect('destroy', this._onDestroy.bind(this));

        this._folderChangedId = this._folder.connect(
            'changed', this._sync.bind(this));
        this._sync();
    }

    _onDestroy() {
        if (this._dragMonitor) {
            DND.removeDragMonitor(this._dragMonitor);
            this._dragMonitor = null;
        }

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
        if (this._iconIsHovering == hovering)
            return;

        this._iconIsHovering = hovering;

        if (hovering) {
            this._dragMonitor = {
                dragMotion: this._onDragMotion.bind(this),
            };
            DND.addDragMonitor(this._dragMonitor);
            this.add_style_pseudo_class('drop');
        } else {
            DND.removeDragMonitor(this._dragMonitor);
            this.remove_style_pseudo_class('drop');
        }
    }

    _onDragMotion(dragEvent) {
        if (!this.contains(dragEvent.targetActor) ||
            !this._canAccept(dragEvent.source))
            this._setHoveringByDnd(false);

        return DND.DragMotionResult.CONTINUE;
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

    handleDragOver(source) {
        if (!this._canAccept(source))
            return DND.DragMotionResult.NO_DROP;

        this._setHoveringByDnd(true);

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source) {
        this._setHoveringByDnd(false);

        if (!this._canAccept(source))
            return false;

        this.view.addApp(source.app);

        return true;
    }

    _updateName() {
        let name = _getFolderName(this._folder);
        if (this.name == name)
            return;

        this.name = name;
        this.icon.label.text = this.name;
        this.emit('name-changed');
    }

    _sync() {
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

        this.add_constraint(new Layout.MonitorConstraint({
            primary: true,
            work_area: true,
        }));

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

        this._sourceMappedId = 0;
        this._popdownTimeoutId = 0;
        this._needsZoomAndFade = false;
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
            background_color: Clutter.Color.from_pixel(0x000000cc),
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
            },
        });

        this._needsZoomAndFade = false;
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

        if (this._popdownTimeoutId > 0) {
            GLib.source_remove(this._popdownTimeoutId);
            this._popdownTimeoutId = 0;
        }
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

    _withinDialog(x, y) {
        const childAllocation =
            Shell.util_get_transformed_allocation(this.child);

        return x > childAllocation.x1 &&
            x < childAllocation.x2 &&
            y > childAllocation.y1 &&
            y < childAllocation.y2;
    }

    handleDragOver(source, actor, x, y) {
        if (this._withinDialog(x, y)) {
            if (this._popdownTimeoutId > 0) {
                GLib.source_remove(this._popdownTimeoutId);
                this._popdownTimeoutId = 0;
            }
        } else if (this._popdownTimeoutId === 0) {
            this._popdownTimeoutId =
                GLib.timeout_add(GLib.PRIORITY_DEFAULT, MENU_POPUP_TIMEOUT, () => {
                    this._popdownTimeoutId = 0;
                    this.popdown();
                    return GLib.SOURCE_REMOVE;
                });
        }

        return DND.DragMotionResult.NO_DROP;
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

        this._isOpen = this._grabHelper.grab({ actor: this,
                                               onUngrab: this.popdown.bind(this) });

        if (!this._isOpen)
            return;

        this.get_parent().set_child_above_sibling(this, null);

        this._needsZoomAndFade = true;
        this.show();

        this.emit('open-state-changed', true);
    }

    popdown() {
        if (!this._isOpen)
            return;

        this._zoomAndFadeOut();
        this._showFolderLabel();

        this._grabHelper.ungrab({ actor: this });
        this._isOpen = false;
        this.emit('open-state-changed', false);
    }
});

var AppIcon = GObject.registerClass({
    Signals: {
        'menu-state-changed': { param_types: [GObject.TYPE_BOOLEAN] },
        'sync-tooltip': {},
    },
}, class AppIcon extends St.Button {
    _init(app, iconParams = {}) {
        super._init({
            style_class: 'app-well-app',
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
            reactive: true,
            button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO,
            can_focus: true,
        });

        this.app = app;
        this.id = app.get_id();
        this.name = app.get_name();

        this._iconContainer = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                              x_expand: true, y_expand: true });

        this.set_child(this._iconContainer);

        this._delegate = this;

        this._folderPreviewId = 0;

        // Get the isDraggable property without passing it on to the BaseIcon:
        let appIconParams = Params.parse(iconParams, { isDraggable: true }, true);
        let isDraggable = appIconParams['isDraggable'];
        delete iconParams['isDraggable'];

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

        if (isDraggable) {
            this._draggable = DND.makeDraggable(this);
            this._draggable.connect('drag-begin', () => {
                this._dragging = true;
                this.scaleAndFade();
                this._removeMenuTimeout();
                Main.overview.beginItemDrag(this);
            });
            this._draggable.connect('drag-cancelled', () => {
                this._dragging = false;
                Main.overview.cancelledItemDrag(this);
            });
            this._draggable.connect('drag-end', () => {
                this._dragging = false;
                this.undoScaleAndFade();
                Main.overview.endItemDrag(this);
            });
        }

        this._otherIconIsHovering = false;

        this._menuTimeoutId = 0;
        this._stateChangedId = this.app.connect('notify::state', () => {
            this._updateRunningStyle();
        });
        this._updateRunningStyle();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._folderPreviewId > 0) {
            GLib.source_remove(this._folderPreviewId);
            this._folderPreviewId = 0;
        }
        if (this._stateChangedId > 0)
            this.app.disconnect(this._stateChangedId);

        if (this._dragMonitor) {
            DND.removeDragMonitor(this._dragMonitor);
            this._dragMonitor = null;
        }

        if (this._draggable) {
            if (this._dragging)
                Main.overview.endItemDrag(this);
            this._draggable = null;
        }
        this._stateChangedId = 0;
        this._removeMenuTimeout();
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

    popupMenu() {
        this._removeMenuTimeout();
        this.fake_release();

        if (this._draggable)
            this._draggable.fakeRelease();

        if (!this._menu) {
            this._menu = new AppIconMenu(this);
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

    scaleAndFade() {
        this.reactive = false;
        this.ease({
            scale_x: 0.75,
            scale_y: 0.75,
            opacity: 128,
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

        this._otherIconIsHovering = hovering;

        if (hovering) {
            this._dragMonitor = {
                dragMotion: this._onDragMotion.bind(this),
            };
            DND.addDragMonitor(this._dragMonitor);

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
            DND.removeDragMonitor(this._dragMonitor);

            if (this._folderPreviewId > 0) {
                GLib.source_remove(this._folderPreviewId);
                this._folderPreviewId = 0;
            }
            this._hideFolderPreview();
            this.remove_style_pseudo_class('drop');
        }
    }

    _onDragMotion(dragEvent) {
        if (!this.contains(dragEvent.targetActor))
            this._setHoveringByDnd(false);

        return DND.DragMotionResult.CONTINUE;
    }

    handleDragOver(source, _actor, x) {
        if (source == this)
            return DND.DragMotionResult.NO_DROP;

        if (!this._canAccept(source))
            return DND.DragMotionResult.CONTINUE;

        if (x < IconGrid.LEFT_DIVIDER_LEEWAY ||
            x + IconGrid.RIGHT_DIVIDER_LEEWAY > this.width)
            return DND.DragMotionResult.CONTINUE;

        this._setHoveringByDnd(true);

        return DND.DragMotionResult.MOVE_DROP;
    }

    acceptDrop(source) {
        this._setHoveringByDnd(false);

        if (!this._canAccept(source))
            return false;

        let view = _getViewFromIcon(this);
        let apps = [this.id, source.id];

        return view.createFolder(apps);
    }
});

var AppIconMenu = class AppIconMenu extends PopupMenu.PopupMenu {
    constructor(source) {
        let side = St.Side.LEFT;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            side = St.Side.RIGHT;

        super(source, 0.5, side);

        // We want to keep the item hovered while the menu is up
        this.blockSourceEvents = true;

        this._source = source;

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

            let canFavorite = global.settings.is_writable('favorite-apps');

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
                let item = this._appendMenuItem(_("Show Details"));
                item.connect('activate', async () => {
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
        this.open();
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
