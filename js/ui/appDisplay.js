// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Mainloop = imports.mainloop;
const Atk = imports.gi.Atk;

const AppFavorites = imports.ui.appFavorites;
const BoxPointer = imports.ui.boxpointer;
const DND = imports.ui.dnd;
const GrabHelper = imports.ui.grabHelper;
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const OverviewControls = imports.ui.overviewControls;
const PopupMenu = imports.ui.popupMenu;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const Search = imports.ui.search;
const System = imports.ui.status.system;
const Params = imports.misc.params;
const Util = imports.misc.util;
const SystemActions = imports.misc.systemActions;

var MAX_APPLICATION_WORK_MILLIS = 75;
var MENU_POPUP_TIMEOUT = 600;
var MAX_COLUMNS = 6;
var MIN_COLUMNS = 4;
var MIN_ROWS = 4;

var INACTIVE_GRID_OPACITY = 77;
// This time needs to be less than IconGrid.EXTRA_SPACE_ANIMATION_TIME
// to not clash with other animations
var INACTIVE_GRID_OPACITY_ANIMATION_TIME = 0.24;
var FOLDER_SUBICON_FRACTION = .4;

var MIN_FREQUENT_APPS_COUNT = 3;

var INDICATORS_BASE_TIME = 0.25;
var INDICATORS_ANIMATION_DELAY = 0.125;
var INDICATORS_ANIMATION_MAX_TIME = 0.75;

var VIEWS_SWITCH_TIME = 0.4;
var VIEWS_SWITCH_ANIMATION_DELAY = 0.1;

// Follow iconGrid animations approach and divide by 2 to animate out to
// not annoy the user when the user wants to quit appDisplay.
// Also, make sure we don't exceed iconGrid animation total time or
// views switch time.
var INDICATORS_BASE_TIME_OUT = 0.125;
var INDICATORS_ANIMATION_DELAY_OUT = 0.0625;
var INDICATORS_ANIMATION_MAX_TIME_OUT =
    Math.min (VIEWS_SWITCH_TIME,
              IconGrid.ANIMATION_TIME_OUT + IconGrid.ANIMATION_MAX_DELAY_OUT_FOR_ITEM);

var PAGE_SWITCH_TIME = 0.3;

const SWITCHEROO_BUS_NAME = 'net.hadess.SwitcherooControl';
const SWITCHEROO_OBJECT_PATH = '/net/hadess/SwitcherooControl';

const SwitcherooProxyInterface = '<node> \
<interface name="net.hadess.SwitcherooControl"> \
  <property name="HasDualGpu" type="b" access="read"/> \
</interface> \
</node>';

const SwitcherooProxy = Gio.DBusProxy.makeProxyWrapper(SwitcherooProxyInterface);
let discreteGpuAvailable = false;

function _getCategories(info) {
    let categoriesStr = info.get_categories();
    if (!categoriesStr)
        return [];
    return categoriesStr.split(';');
}

function _listsIntersect(a, b) {
    for (let itemA of a)
        if (b.indexOf(itemA) >= 0)
            return true;
    return false;
}

function _getFolderName(folder) {
    let name = folder.get_string('name');

    if (folder.get_boolean('translate')) {
        let keyfile = new GLib.KeyFile();
        let path = 'desktop-directories/' + name;

        try {
            keyfile.load_from_data_dirs(path, GLib.KeyFileFlags.NONE);
            name = keyfile.get_locale_string('Desktop Entry', 'Name', null);
        } catch(e) {
            return name;
        }
    }

    return name;
}

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

var BaseAppView = new Lang.Class({
    Name: 'BaseAppView',
    Abstract: true,

    _init: function(params, gridParams) {
        gridParams = Params.parse(gridParams, { xAlign: St.Align.MIDDLE,
                                                columnLimit: MAX_COLUMNS,
                                                minRows: MIN_ROWS,
                                                minColumns: MIN_COLUMNS,
                                                fillParent: false,
                                                padWithSpacing: true });
        params = Params.parse(params, { usePagination: false });

        if(params.usePagination)
            this._grid = new IconGrid.PaginatedIconGrid(gridParams);
        else
            this._grid = new IconGrid.IconGrid(gridParams);

        this._grid.connect('key-focus-in', Lang.bind(this, function(grid, actor) {
            this._keyFocusIn(actor);
        }));
        // Standard hack for ClutterBinLayout
        this._grid.actor.x_expand = true;

        this._items = {};
        this._allItems = [];
    },

    _keyFocusIn: function(actor) {
        // Nothing by default
    },

    removeAll: function() {
        this._grid.destroyAll();
        this._items = {};
        this._allItems = [];
    },

    _redisplay: function() {
        this.removeAll();
        this._loadApps();
    },

    getAllItems: function() {
        return this._allItems;
    },

    addItem: function(icon) {
        let id = icon.id;
        if (this._items[id] !== undefined)
            return;

        this._allItems.push(icon);
        this._items[id] = icon;
    },

    _compareItems: function(a, b) {
        return a.name.localeCompare(b.name);
    },

    loadGrid: function() {
        this._allItems.sort(this._compareItems);
        this._allItems.forEach(Lang.bind(this, function(item) {
            this._grid.addItem(item);
        }));
        this.emit('view-loaded');
    },

    _selectAppInternal: function(id) {
        if (this._items[id])
            this._items[id].actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        else
            log('No such application ' + id);
    },

    selectApp: function(id) {
        if (this._items[id] && this._items[id].actor.mapped) {
            this._selectAppInternal(id);
        } else if (this._items[id]) {
            // Need to wait until the view is mapped
            let signalId = this._items[id].actor.connect('notify::mapped', Lang.bind(this, function(actor) {
                if (actor.mapped) {
                    actor.disconnect(signalId);
                    this._selectAppInternal(id);
                }
            }));
        } else {
            // Need to wait until the view is built
            let signalId = this.connect('view-loaded', Lang.bind(this, function() {
                this.disconnect(signalId);
                this.selectApp(id);
            }));
        }
    },

    _doSpringAnimation: function(animationDirection) {
        this._grid.actor.opacity = 255;
        this._grid.animateSpring(animationDirection,
                                 Main.overview.getShowAppsButton());
    },

    animate: function(animationDirection, onComplete) {
        if (onComplete) {
            let animationDoneId = this._grid.connect('animation-done', Lang.bind(this,
                function () {
                    this._grid.disconnect(animationDoneId);
                    onComplete();
            }));
        }

        if (animationDirection == IconGrid.AnimationDirection.IN) {
            let id = this._grid.actor.connect('paint', () => {
                this._grid.actor.disconnect(id);
                this._doSpringAnimation(animationDirection);
            });
        } else {
            this._doSpringAnimation(animationDirection);
        }
    },

    animateSwitch: function(animationDirection) {
        Tweener.removeTweens(this.actor);
        Tweener.removeTweens(this._grid.actor);

        let params = { time: VIEWS_SWITCH_TIME,
                       transition: 'easeOutQuad' };
        if (animationDirection == IconGrid.AnimationDirection.IN) {
            this.actor.show();
            params.opacity = 255;
            params.delay = VIEWS_SWITCH_ANIMATION_DELAY;
        } else {
            params.opacity = 0;
            params.delay = 0;
            params.onComplete = Lang.bind(this, function() { this.actor.hide() });
        }

        Tweener.addTween(this._grid.actor, params);
    }
});
Signals.addSignalMethods(BaseAppView.prototype);

var PageIndicatorsActor = new Lang.Class({
    Name:'PageIndicatorsActor',
    Extends: St.BoxLayout,

    _init: function() {
        this.parent({ style_class: 'page-indicators',
                      vertical: true,
                      x_expand: true, y_expand: true,
                      x_align: Clutter.ActorAlign.END,
                      y_align: Clutter.ActorAlign.CENTER,
                      reactive: true,
                      clip_to_allocation: true });
    },

    vfunc_get_preferred_height: function(forWidth) {
        // We want to request the natural height of all our children as our
        // natural height, so we chain up to St.BoxLayout, but we only request 0
        // as minimum height, since it's not that important if some indicators
        // are not shown
        let [, natHeight] = this.parent(forWidth);
        return [0, natHeight];
    }
});

var PageIndicators = new Lang.Class({
    Name:'PageIndicators',

    _init: function() {
        this.actor = new PageIndicatorsActor();
        this._nPages = 0;
        this._currentPage = undefined;

        this.actor.connect('notify::mapped',
                           Lang.bind(this, function() {
                               this.animateIndicators(IconGrid.AnimationDirection.IN);
                           })
                          );
    },

    setNPages: function(nPages) {
        if (this._nPages == nPages)
            return;

        let diff = nPages - this._nPages;
        if (diff > 0) {
            for (let i = 0; i < diff; i++) {
                let pageIndex = this._nPages + i;
                let indicator = new St.Button({ style_class: 'page-indicator',
                                                button_mask: St.ButtonMask.ONE |
                                                             St.ButtonMask.TWO |
                                                             St.ButtonMask.THREE,
                                                toggle_mode: true,
                                                checked: pageIndex == this._currentPage });
                indicator.child = new St.Widget({ style_class: 'page-indicator-icon' });
                indicator.connect('clicked', Lang.bind(this,
                    function() {
                        this.emit('page-activated', pageIndex);
                    }));
                this.actor.add_actor(indicator);
            }
        } else {
            let children = this.actor.get_children().splice(diff);
            for (let i = 0; i < children.length; i++)
                children[i].destroy();
        }
        this._nPages = nPages;
        this.actor.visible = (this._nPages > 1);
    },

    setCurrentPage: function(currentPage) {
        this._currentPage = currentPage;

        let children = this.actor.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].set_checked(i == this._currentPage);
    },

    animateIndicators: function(animationDirection) {
        if (!this.actor.mapped)
            return;

        let children = this.actor.get_children();
        if (children.length == 0)
            return;

        for (let i = 0; i < this._nPages; i++)
            Tweener.removeTweens(children[i]);

        let offset;
        if (this.actor.get_text_direction() == Clutter.TextDirection.RTL)
            offset = -children[0].width;
        else
            offset = children[0].width;

        let isAnimationIn = animationDirection == IconGrid.AnimationDirection.IN;
        let delay = isAnimationIn ? INDICATORS_ANIMATION_DELAY :
                                    INDICATORS_ANIMATION_DELAY_OUT;
        let baseTime = isAnimationIn ? INDICATORS_BASE_TIME : INDICATORS_BASE_TIME_OUT;
        let totalAnimationTime = baseTime + delay * this._nPages;
        let maxTime = isAnimationIn ? INDICATORS_ANIMATION_MAX_TIME :
                                      INDICATORS_ANIMATION_MAX_TIME_OUT;
        if (totalAnimationTime > maxTime)
            delay -= (totalAnimationTime - maxTime) / this._nPages;

        for (let i = 0; i < this._nPages; i++) {
            children[i].translation_x = isAnimationIn ? offset : 0;
            Tweener.addTween(children[i],
                             { translation_x: isAnimationIn ? 0 : offset,
                               time: baseTime + delay * i,
                               transition: 'easeInOutQuad',
                               delay: isAnimationIn ? VIEWS_SWITCH_ANIMATION_DELAY : 0
                             });
        }
    }
});
Signals.addSignalMethods(PageIndicators.prototype);

var AllView = new Lang.Class({
    Name: 'AllView',
    Extends: BaseAppView,

    _init: function() {
        this.parent({ usePagination: true }, null);
        this._scrollView = new St.ScrollView({ style_class: 'all-apps',
                                               x_expand: true,
                                               y_expand: true,
                                               x_fill: true,
                                               y_fill: false,
                                               reactive: true,
                                               y_align: St.Align.START });
        this.actor = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                     x_expand:true, y_expand:true });
        this.actor.add_actor(this._scrollView);

        this._scrollView.set_policy(Gtk.PolicyType.NEVER,
                                    Gtk.PolicyType.EXTERNAL);
        this._adjustment = this._scrollView.vscroll.adjustment;

        this._pageIndicators = new PageIndicators();
        this._pageIndicators.connect('page-activated', Lang.bind(this,
            function(indicators, pageIndex) {
                this.goToPage(pageIndex);
            }));
        this._pageIndicators.actor.connect('scroll-event', Lang.bind(this, this._onScroll));
        this.actor.add_actor(this._pageIndicators.actor);

        this.folderIcons = [];

        this._stack = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        let box = new St.BoxLayout({ vertical: true });

        this._grid.currentPage = 0;
        this._stack.add_actor(this._grid.actor);
        this._eventBlocker = new St.Widget({ x_expand: true, y_expand: true });
        this._stack.add_actor(this._eventBlocker);

        box.add_actor(this._stack);
        this._scrollView.add_actor(box);

        this._scrollView.connect('scroll-event', Lang.bind(this, this._onScroll));

        let panAction = new Clutter.PanAction({ interpolate: false });
        panAction.connect('pan', Lang.bind(this, this._onPan));
        panAction.connect('gesture-cancel', Lang.bind(this, this._onPanEnd));
        panAction.connect('gesture-end', Lang.bind(this, this._onPanEnd));
        this._panAction = panAction;
        this._scrollView.add_action(panAction);
        this._panning = false;
        this._clickAction = new Clutter.ClickAction();
        this._clickAction.connect('clicked', Lang.bind(this, function() {
            if (!this._currentPopup)
                return;

            let [x, y] = this._clickAction.get_coords();
            let actor = global.stage.get_actor_at_pos(Clutter.PickMode.ALL, x, y);
            if (!this._currentPopup.actor.contains(actor))
                this._currentPopup.popdown();
        }));
        this._eventBlocker.add_action(this._clickAction);

        this._displayingPopup = false;

        this._availWidth = 0;
        this._availHeight = 0;

        Main.overview.connect('hidden', Lang.bind(this,
            function() {
                this.goToPage(0);
            }));
        this._grid.connect('space-opened', Lang.bind(this,
            function() {
                let fadeEffect = this._scrollView.get_effect('fade');
                if (fadeEffect)
                    fadeEffect.enabled = false;

                this.emit('space-ready');
            }));
        this._grid.connect('space-closed', Lang.bind(this,
            function() {
                this._displayingPopup = false;
            }));

        this.actor.connect('notify::mapped', Lang.bind(this,
            function() {
                if (this.actor.mapped) {
                    this._keyPressEventId =
                        global.stage.connect('key-press-event',
                                             Lang.bind(this, this._onKeyPressEvent));
                } else {
                    if (this._keyPressEventId)
                        global.stage.disconnect(this._keyPressEventId);
                    this._keyPressEventId = 0;
                }
            }));

        this._redisplayWorkId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));

        Shell.AppSystem.get_default().connect('installed-changed', Lang.bind(this, function() {
            Main.queueDeferredWork(this._redisplayWorkId);
        }));
        this._folderSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders' });
        this._folderSettings.connect('changed::folder-children', Lang.bind(this, function() {
            Main.queueDeferredWork(this._redisplayWorkId);
        }));
    },

    removeAll: function() {
        this.folderIcons = [];
        this.parent();
    },

    _itemNameChanged: function(item) {
        // If an item's name changed, we can pluck it out of where it's
        // supposed to be and reinsert it where it's sorted.
        let oldIdx = this._allItems.indexOf(item);
        this._allItems.splice(oldIdx, 1);
        let newIdx = Util.insertSorted(this._allItems, item, this._compareItems);

        this._grid.removeItem(item);
        this._grid.addItem(item, newIdx);
    },

    _refilterApps: function() {
        this._allItems.forEach(function(icon) {
            if (icon instanceof AppIcon)
                icon.actor.visible = true;
        });

        this.folderIcons.forEach(Lang.bind(this, function(folder) {
            let folderApps = folder.getAppIds();
            folderApps.forEach(Lang.bind(this, function(appId) {
                let appIcon = this._items[appId];
                appIcon.actor.visible = false;
            }));
        }));
    },

    _loadApps: function() {
        let apps = Gio.AppInfo.get_all().filter(function(appInfo) {
            try {
                let id = appInfo.get_id(); // catch invalid file encodings
            } catch(e) {
                return false;
            }
            return appInfo.should_show();
        }).map(function(app) {
            return app.get_id();
        });

        let appSys = Shell.AppSystem.get_default();

        let folders = this._folderSettings.get_strv('folder-children');
        folders.forEach(Lang.bind(this, function(id) {
            let path = this._folderSettings.path + 'folders/' + id + '/';
            let icon = new FolderIcon(id, path, this);
            icon.connect('name-changed', Lang.bind(this, this._itemNameChanged));
            icon.connect('apps-changed', Lang.bind(this, this._refilterApps));
            this.addItem(icon);
            this.folderIcons.push(icon);
        }));

        // Allow dragging of the icon only if the Dash would accept a drop to
        // change favorite-apps. There are no other possible drop targets from
        // the app picker, so there's no other need for a drag to start,
        // at least on single-monitor setups.
        // This also disables drag-to-launch on multi-monitor setups,
        // but we hope that is not used much.
        let favoritesWritable = global.settings.is_writable('favorite-apps');

        apps.forEach(Lang.bind(this, function(appId) {
            let app = appSys.lookup_app(appId);

            let icon = new AppIcon(app,
                                   { isDraggable: favoritesWritable });
            this.addItem(icon);
        }));

        this.loadGrid();
        this._refilterApps();
    },

    // Overriden from BaseAppView
    animate: function (animationDirection, onComplete) {
        this._scrollView.reactive = false;
        let completionFunc = Lang.bind(this, function() {
            this._scrollView.reactive = true;
            if (onComplete)
                onComplete();
        });

        if (animationDirection == IconGrid.AnimationDirection.OUT &&
            this._displayingPopup && this._currentPopup) {
            this._currentPopup.popdown();
            let spaceClosedId = this._grid.connect('space-closed', Lang.bind(this,
                function() {
                    this._grid.disconnect(spaceClosedId);
                    // Given that we can't call this.parent() inside the
                    // signal handler, call again animate which will
                    // call the parent given that popup is already
                    // closed.
                    this.animate(animationDirection, completionFunc);
                }));
        } else {
            this.parent(animationDirection, completionFunc);
            if (animationDirection == IconGrid.AnimationDirection.OUT)
                this._pageIndicators.animateIndicators(animationDirection);
        }
    },

    animateSwitch: function(animationDirection) {
        this.parent(animationDirection);

        if (this._currentPopup && this._displayingPopup &&
            animationDirection == IconGrid.AnimationDirection.OUT)
            Tweener.addTween(this._currentPopup.actor,
                             { time: VIEWS_SWITCH_TIME,
                               transition: 'easeOutQuad',
                               opacity: 0,
                               onComplete: function() {
                                  this.opacity = 255;
                               } });

        if (animationDirection == IconGrid.AnimationDirection.OUT)
            this._pageIndicators.animateIndicators(animationDirection);
    },

    getCurrentPageY: function() {
        return this._grid.getPageY(this._grid.currentPage);
    },

    goToPage: function(pageNumber) {
        pageNumber = clamp(pageNumber, 0, this._grid.nPages() - 1);

        if (this._grid.currentPage == pageNumber && this._displayingPopup && this._currentPopup)
            return;
        if (this._displayingPopup && this._currentPopup)
            this._currentPopup.popdown();

        let velocity;
        if (!this._panning)
            velocity = 0;
        else
            velocity = Math.abs(this._panAction.get_velocity(0)[2]);
        // Tween the change between pages.
        // If velocity is not specified (i.e. scrolling with mouse wheel),
        // use the same speed regardless of original position
        // if velocity is specified, it's in pixels per milliseconds
        let diffToPage = this._diffToPage(pageNumber);
        let childBox = this._scrollView.get_allocation_box();
        let totalHeight = childBox.y2 - childBox.y1;
        let time;
        // Only take the velocity into account on page changes, otherwise
        // return smoothly to the current page using the default velocity
        if (this._grid.currentPage != pageNumber) {
            let minVelocity = totalHeight / (PAGE_SWITCH_TIME * 1000);
            velocity = Math.max(minVelocity, velocity);
            time = (diffToPage / velocity) / 1000;
        } else {
            time = PAGE_SWITCH_TIME * diffToPage / totalHeight;
        }
        // When changing more than one page, make sure to not take
        // longer than PAGE_SWITCH_TIME
        time = Math.min(time, PAGE_SWITCH_TIME);

        this._grid.currentPage = pageNumber;
        Tweener.addTween(this._adjustment,
                         { value: this._grid.getPageY(this._grid.currentPage),
                           time: time,
                           transition: 'easeOutQuad' });
        this._pageIndicators.setCurrentPage(pageNumber);
    },

    _diffToPage: function (pageNumber) {
        let currentScrollPosition = this._adjustment.value;
        return Math.abs(currentScrollPosition - this._grid.getPageY(pageNumber));
    },

    openSpaceForPopup: function(item, side, nRows) {
        this._updateIconOpacities(true);
        this._displayingPopup = true;
        this._grid.openExtraSpace(item, side, nRows);
    },

    _closeSpaceForPopup: function() {
        this._updateIconOpacities(false);

        let fadeEffect = this._scrollView.get_effect('fade');
        if (fadeEffect)
            fadeEffect.enabled = true;

        this._grid.closeExtraSpace();
    },

    _onScroll: function(actor, event) {
        if (this._displayingPopup || !this._scrollView.reactive)
            return Clutter.EVENT_STOP;

        let direction = event.get_scroll_direction();
        if (direction == Clutter.ScrollDirection.UP)
            this.goToPage(this._grid.currentPage - 1);
        else if (direction == Clutter.ScrollDirection.DOWN)
            this.goToPage(this._grid.currentPage + 1);

        return Clutter.EVENT_STOP;
    },

    _onPan: function(action) {
        if (this._displayingPopup)
            return false;
        this._panning = true;
        this._clickAction.release();
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._adjustment;
        adjustment.value -= (dy / this._scrollView.height) * adjustment.page_size;
        return false;
    },

    _onPanEnd: function(action) {
         if (this._displayingPopup)
            return;

        let pageHeight = this._grid.getPageHeight();

        // Calculate the scroll value we'd be at, which is our current
        // scroll plus any velocity the user had when they released
        // their finger.

        let velocity = -action.get_velocity(0)[2];
        let endPanValue = this._adjustment.value + velocity;

        let closestPage = Math.round(endPanValue / pageHeight);
        this.goToPage(closestPage);

        this._panning = false;
    },

    _onKeyPressEvent: function(actor, event) {
        if (this._displayingPopup)
            return Clutter.EVENT_STOP;

        if (event.get_key_symbol() == Clutter.Page_Up) {
            this.goToPage(this._grid.currentPage - 1);
            return Clutter.EVENT_STOP;
        } else if (event.get_key_symbol() == Clutter.Page_Down) {
            this.goToPage(this._grid.currentPage + 1);
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    addFolderPopup: function(popup) {
        this._stack.add_actor(popup.actor);
        popup.connect('open-state-changed', Lang.bind(this,
            function(popup, isOpen) {
                this._eventBlocker.reactive = isOpen;
                this._currentPopup = isOpen ? popup : null;
                this._updateIconOpacities(isOpen);
                if(!isOpen)
                    this._closeSpaceForPopup();
            }));
    },

    _keyFocusIn: function(icon) {
        let itemPage = this._grid.getItemPage(icon);
        this.goToPage(itemPage);
    },

    _updateIconOpacities: function(folderOpen) {
        for (let id in this._items) {
            let params, opacity;
            if (folderOpen && !this._items[id].actor.checked)
                opacity =  INACTIVE_GRID_OPACITY;
            else
                opacity = 255;
            params = { opacity: opacity,
                       time: INACTIVE_GRID_OPACITY_ANIMATION_TIME,
                       transition: 'easeOutQuad' };
            Tweener.addTween(this._items[id].actor, params);
        }
    },

    // Called before allocation to calculate dynamic spacing
    adaptToSize: function(width, height) {
        let box = new Clutter.ActorBox();
        box.x1 = 0;
        box.x2 = width;
        box.y1 = 0;
        box.y2 = height;
        box = this.actor.get_theme_node().get_content_box(box);
        box = this._scrollView.get_theme_node().get_content_box(box);
        box = this._grid.actor.get_theme_node().get_content_box(box);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        let oldNPages = this._grid.nPages();

        this._grid.adaptToSize(availWidth, availHeight);

        let fadeOffset = Math.min(this._grid.topPadding,
                                  this._grid.bottomPadding);
        this._scrollView.update_fade_effect(fadeOffset, 0);
        if (fadeOffset > 0)
            this._scrollView.get_effect('fade').fade_edges = true;

        if (this._availWidth != availWidth || this._availHeight != availHeight || oldNPages != this._grid.nPages()) {
            this._adjustment.value = 0;
            this._grid.currentPage = 0;
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this,
                function() {
                    this._pageIndicators.setNPages(this._grid.nPages());
                    this._pageIndicators.setCurrentPage(0);
                }));
        }

        this._availWidth = availWidth;
        this._availHeight = availHeight;
        // Update folder views
        for (let i = 0; i < this.folderIcons.length; i++)
            this.folderIcons[i].adaptToSize(availWidth, availHeight);
    }
});
Signals.addSignalMethods(AllView.prototype);

var FrequentView = new Lang.Class({
    Name: 'FrequentView',
    Extends: BaseAppView,

    _init: function() {
        this.parent(null, { fillParent: true });

        this.actor = new St.Widget({ style_class: 'frequent-apps',
                                     layout_manager: new Clutter.BinLayout(),
                                     x_expand: true, y_expand: true });

        this._noFrequentAppsLabel = new St.Label({ text: _("Frequently used applications will appear here"),
                                                   style_class: 'no-frequent-applications-label',
                                                   x_align: Clutter.ActorAlign.CENTER,
                                                   x_expand: true,
                                                   y_align: Clutter.ActorAlign.CENTER,
                                                   y_expand: true });

        this._grid.actor.y_expand = true;

        this.actor.add_actor(this._grid.actor);
        this.actor.add_actor(this._noFrequentAppsLabel);
        this._noFrequentAppsLabel.hide();

        this._usage = Shell.AppUsage.get_default();

        this.actor.connect('notify::mapped', Lang.bind(this, function() {
            if (this.actor.mapped)
                this._redisplay();
        }));
    },

    hasUsefulData: function() {
        return this._usage.get_most_used("").length >= MIN_FREQUENT_APPS_COUNT;
    },

    _loadApps: function() {
        let mostUsed = this._usage.get_most_used ("");
        let hasUsefulData = this.hasUsefulData();
        this._noFrequentAppsLabel.visible = !hasUsefulData;
        if(!hasUsefulData)
            return;

        // Allow dragging of the icon only if the Dash would accept a drop to
        // change favorite-apps. There are no other possible drop targets from
        // the app picker, so there's no other need for a drag to start,
        // at least on single-monitor setups.
        // This also disables drag-to-launch on multi-monitor setups,
        // but we hope that is not used much.
        let favoritesWritable = global.settings.is_writable('favorite-apps');

        for (let i = 0; i < mostUsed.length; i++) {
            if (!mostUsed[i].get_app_info().should_show())
                continue;
            let appIcon = new AppIcon(mostUsed[i],
                                      { isDraggable: favoritesWritable });
            this._grid.addItem(appIcon, -1);
        }
    },

    // Called before allocation to calculate dynamic spacing
    adaptToSize: function(width, height) {
        let box = new Clutter.ActorBox();
        box.x1 = box.y1 = 0;
        box.x2 = width;
        box.y2 = height;
        box = this.actor.get_theme_node().get_content_box(box);
        box = this._grid.actor.get_theme_node().get_content_box(box);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        this._grid.adaptToSize(availWidth, availHeight);
    }
});

var Views = {
    FREQUENT: 0,
    ALL: 1
};

var ControlsBoxLayout = Lang.Class({
    Name: 'ControlsBoxLayout',
    Extends: Clutter.BoxLayout,

    /**
     * Override the BoxLayout behavior to use the maximum preferred width of all
     * buttons for each child
     */
    vfunc_get_preferred_width: function(container, forHeight) {
        let maxMinWidth = 0;
        let maxNaturalWidth = 0;
        for (let child = container.get_first_child();
             child;
             child = child.get_next_sibling()) {
             let [minWidth, natWidth] = child.get_preferred_width(forHeight);
             maxMinWidth = Math.max(maxMinWidth, minWidth);
             maxNaturalWidth = Math.max(maxNaturalWidth, natWidth);
        }
        let childrenCount = container.get_n_children();
        let totalSpacing = this.spacing * (childrenCount - 1);
        return [maxMinWidth * childrenCount + totalSpacing,
                maxNaturalWidth * childrenCount + totalSpacing];
    }
});

var ViewStackLayout = new Lang.Class({
    Name: 'ViewStackLayout',
    Extends: Clutter.BinLayout,
    Signals: { 'allocated-size-changed': { param_types: [GObject.TYPE_INT,
                                                         GObject.TYPE_INT] } },

    vfunc_allocate: function (actor, box, flags) {
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        // Prepare children of all views for the upcoming allocation, calculate all
        // the needed values to adapt available size
        this.emit('allocated-size-changed', availWidth, availHeight);
        this.parent(actor, box, flags);
    }
});

var AppDisplay = new Lang.Class({
    Name: 'AppDisplay',

    _init: function() {
        this._privacySettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.privacy' });
        this._privacySettings.connect('changed::remember-app-usage',
                                      Lang.bind(this, this._updateFrequentVisibility));

        this._views = [];

        let view, button;
        view = new FrequentView();
        button = new St.Button({ label: _("Frequent"),
                                 style_class: 'app-view-control button',
                                 can_focus: true,
                                 x_expand: true });
        this._views[Views.FREQUENT] = { 'view': view, 'control': button };

        view = new AllView();
        button = new St.Button({ label: _("All"),
                                 style_class: 'app-view-control button',
                                 can_focus: true,
                                 x_expand: true });
        this._views[Views.ALL] = { 'view': view, 'control': button };

        this.actor = new St.BoxLayout ({ style_class: 'app-display',
                                         x_expand: true, y_expand: true,
                                         vertical: true });
        this._viewStackLayout = new ViewStackLayout();
        this._viewStack = new St.Widget({ x_expand: true, y_expand: true,
                                          layout_manager: this._viewStackLayout });
        this._viewStackLayout.connect('allocated-size-changed', Lang.bind(this, this._onAllocatedSizeChanged));
        this.actor.add_actor(this._viewStack);
        let layout = new ControlsBoxLayout({ homogeneous: true });
        this._controls = new St.Widget({ style_class: 'app-view-controls',
                                         layout_manager: layout });
        this._controls.connect('notify::mapped', Lang.bind(this,
            function() {
                // controls are faded either with their parent or
                // explicitly in animate(); we can't know how they'll be
                // shown next, so make sure to restore their opacity
                // when they are hidden
                if (this._controls.mapped)
                  return;

                Tweener.removeTweens(this._controls);
                this._controls.opacity = 255;
            }));

        layout.hookup_style(this._controls);
        this.actor.add_actor(new St.Bin({ child: this._controls }));

        for (let i = 0; i < this._views.length; i++) {
            this._viewStack.add_actor(this._views[i].view.actor);
            this._controls.add_actor(this._views[i].control);

            let viewIndex = i;
            this._views[i].control.connect('clicked', Lang.bind(this,
                function(actor) {
                    this._showView(viewIndex);
                    global.settings.set_uint('app-picker-view', viewIndex);
                }));
        }
        let initialView = Math.min(global.settings.get_uint('app-picker-view'),
                                   this._views.length - 1);
        let frequentUseful = this._views[Views.FREQUENT].view.hasUsefulData();
        if (initialView == Views.FREQUENT && !frequentUseful)
            initialView = Views.ALL;
        this._showView(initialView);
        this._updateFrequentVisibility();

        Gio.DBus.system.watch_name(SWITCHEROO_BUS_NAME,
                                   Gio.BusNameWatcherFlags.NONE,
                                   Lang.bind(this, this._switcherooProxyAppeared),
                                   Lang.bind(this, function() {
                                       this._switcherooProxy = null;
                                       this._updateDiscreteGpuAvailable();
                                   }));
    },

    _updateDiscreteGpuAvailable: function() {
        if (!this._switcherooProxy)
            discreteGpuAvailable = false;
        else
            discreteGpuAvailable = this._switcherooProxy.HasDualGpu;
    },

    _switcherooProxyAppeared: function() {
        this._switcherooProxy = new SwitcherooProxy(Gio.DBus.system, SWITCHEROO_BUS_NAME, SWITCHEROO_OBJECT_PATH,
            Lang.bind(this, function(proxy, error) {
                if (error) {
                    log(error.message);
                    return;
                }
                this._updateDiscreteGpuAvailable();
            }));
    },

    animate: function(animationDirection, onComplete) {
        let currentView = this._views.filter(v => v.control.has_style_pseudo_class('checked')).pop().view;

        // Animate controls opacity using iconGrid animation time, since
        // it will be the time the AllView or FrequentView takes to show
        // it entirely.
        let finalOpacity;
        if (animationDirection == IconGrid.AnimationDirection.IN) {
            this._controls.opacity = 0;
            finalOpacity = 255;
        } else {
            finalOpacity = 0
        }

        Tweener.addTween(this._controls,
                         { time: IconGrid.ANIMATION_TIME_IN,
                           transition: 'easeInOutQuad',
                           opacity: finalOpacity,
                          });

        currentView.animate(animationDirection, onComplete);
    },

    _showView: function(activeIndex) {
        for (let i = 0; i < this._views.length; i++) {
            if (i == activeIndex)
                this._views[i].control.add_style_pseudo_class('checked');
            else
                this._views[i].control.remove_style_pseudo_class('checked');

            let animationDirection = i == activeIndex ? IconGrid.AnimationDirection.IN :
                                                        IconGrid.AnimationDirection.OUT;
            this._views[i].view.animateSwitch(animationDirection);
        }
    },

    _updateFrequentVisibility: function() {
        let enabled = this._privacySettings.get_boolean('remember-app-usage');
        this._views[Views.FREQUENT].control.visible = enabled;

        let visibleViews = this._views.filter(function(v) {
            return v.control.visible;
        });
        this._controls.visible = visibleViews.length > 1;

        if (!enabled && this._views[Views.FREQUENT].view.actor.visible)
            this._showView(Views.ALL);
    },

    selectApp: function(id) {
        this._showView(Views.ALL);
        this._views[Views.ALL].view.selectApp(id);
    },

    _onAllocatedSizeChanged: function(actor, width, height) {
        let box = new Clutter.ActorBox();
        box.x1 = box.y1 =0;
        box.x2 = width;
        box.y2 = height;
        box = this._viewStack.get_theme_node().get_content_box(box);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        for (let i = 0; i < this._views.length; i++)
            this._views[i].view.adaptToSize(availWidth, availHeight);
    }
})

var AppSearchProvider = new Lang.Class({
    Name: 'AppSearchProvider',

    _init: function() {
        this._appSys = Shell.AppSystem.get_default();
        this.id = 'applications';
        this.isRemoteProvider = false;
        this.canLaunchSearch = false;

        this._systemActions = new SystemActions.getDefault();
    },

    getResultMetas: function(apps, callback) {
        let metas = [];
        for (let id of apps) {
            if (id.endsWith('.desktop')) {
                let app = this._appSys.lookup_app(id);

                metas.push({ 'id': app.get_id(),
                             'name': app.get_name(),
                             'createIcon': function(size) {
                                 return app.create_icon_texture(size);
                           }
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
    },

    filterResults: function(results, maxNumber) {
        return results.slice(0, maxNumber);
    },

    getInitialResultSet: function(terms, callback, cancellable) {
        let query = terms.join(' ');
        let groups = Shell.AppSystem.search(query);
        let usage = Shell.AppUsage.get_default();
        let results = [];
        groups.forEach(function(group) {
            group = group.filter(function(appID) {
                let app = Gio.DesktopAppInfo.new(appID);
                return app && app.should_show();
            });
            results = results.concat(group.sort(function(a, b) {
                return usage.compare('', a, b);
            }));
        });

        results = results.concat(this._systemActions.getMatchingActions(terms));

        callback(results);
    },

    getSubsearchResultSet: function(previousResults, terms, callback, cancellable) {
        this.getInitialResultSet(terms, callback, cancellable);
    },

    createResultObject: function (resultMeta) {
        if (resultMeta.id.endsWith('.desktop'))
            return new AppIcon(this._appSys.lookup_app(resultMeta['id']));
        else
            return new SystemActionIcon(this, resultMeta);
    }
});

var FolderView = new Lang.Class({
    Name: 'FolderView',
    Extends: BaseAppView,

    _init: function() {
        this.parent(null, null);
        // If it not expand, the parent doesn't take into account its preferred_width when allocating
        // the second time it allocates, so we apply the "Standard hack for ClutterBinLayout"
        this._grid.actor.x_expand = true;

        this.actor = new St.ScrollView({ overlay_scrollbars: true });
        this.actor.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        let scrollableContainer = new St.BoxLayout({ vertical: true, reactive: true });
        scrollableContainer.add_actor(this._grid.actor);
        this.actor.add_actor(scrollableContainer);

        let action = new Clutter.PanAction({ interpolate: true });
        action.connect('pan', Lang.bind(this, this._onPan));
        this.actor.add_action(action);
    },

    _keyFocusIn: function(actor) {
        Util.ensureActorVisibleInScrollView(this.actor, actor);
    },

    // Overriden from BaseAppView
    animate: function(animationDirection) {
        this._grid.animatePulse(animationDirection);
    },

    createFolderIcon: function(size) {
        let layout = new Clutter.GridLayout();
        let icon = new St.Widget({ layout_manager: layout,
                                   style_class: 'app-folder-icon' });
        layout.hookup_style(icon);
        let subSize = Math.floor(FOLDER_SUBICON_FRACTION * size);

        let numItems = this._allItems.length;
        let rtl = icon.get_text_direction() == Clutter.TextDirection.RTL;
        for (let i = 0; i < 4; i++) {
            let bin = new St.Bin({ width: subSize, height: subSize });
            if (i < numItems)
                bin.child = this._allItems[i].app.create_icon_texture(subSize);
            layout.attach(bin, rtl ? (i + 1) % 2 : i % 2, Math.floor(i / 2), 1, 1);
        }

        return icon;
    },

    _onPan: function(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this.actor.vscroll.adjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    adaptToSize: function(width, height) {
        this._parentAvailableWidth = width;
        this._parentAvailableHeight = height;

        this._grid.adaptToSize(width, height);

        // To avoid the fade effect being applied to the unscrolled grid,
        // the offset would need to be applied after adjusting the padding;
        // however the final padding is expected to be too small for the
        // effect to look good, so use the unadjusted padding
        let fadeOffset = Math.min(this._grid.topPadding,
                                  this._grid.bottomPadding);
        this.actor.update_fade_effect(fadeOffset, 0);

        // Set extra padding to avoid popup or close button being cut off
        this._grid.topPadding = Math.max(this._grid.topPadding - this._offsetForEachSide, 0);
        this._grid.bottomPadding = Math.max(this._grid.bottomPadding - this._offsetForEachSide, 0);
        this._grid.leftPadding = Math.max(this._grid.leftPadding - this._offsetForEachSide, 0);
        this._grid.rightPadding = Math.max(this._grid.rightPadding - this._offsetForEachSide, 0);

        this.actor.set_width(this.usedWidth());
        this.actor.set_height(this.usedHeight());
    },

    _getPageAvailableSize: function() {
        let pageBox = new Clutter.ActorBox();
        pageBox.x1 = pageBox.y1 = 0;
        pageBox.x2 = this._parentAvailableWidth;
        pageBox.y2 = this._parentAvailableHeight;

        let contentBox = this.actor.get_theme_node().get_content_box(pageBox);
        // We only can show icons inside the collection view boxPointer
        // so we have to substract the required padding etc of the boxpointer
        return [(contentBox.x2 - contentBox.x1) - 2 * this._offsetForEachSide, (contentBox.y2 - contentBox.y1) - 2 * this._offsetForEachSide];
    },

    usedWidth: function() {
        let [availWidthPerPage, availHeightPerPage] = this._getPageAvailableSize();
        return this._grid.usedWidth(availWidthPerPage);
    },

    usedHeight: function() {
        return this._grid.usedHeightForNRows(this.nRowsDisplayedAtOnce());
    },

    nRowsDisplayedAtOnce: function() {
        let [availWidthPerPage, availHeightPerPage] = this._getPageAvailableSize();
        let maxRows = this._grid.rowsForHeight(availHeightPerPage) - 1;
        return Math.min(this._grid.nRows(availWidthPerPage), maxRows);
    },

    setPaddingOffsets: function(offset) {
        this._offsetForEachSide = offset;
    }
});

var FolderIcon = new Lang.Class({
    Name: 'FolderIcon',

    _init: function(id, path, parentView) {
        this.id = id;
        this.name = '';
        this._parentView = parentView;

        this._folder = new Gio.Settings({ schema_id: 'org.gnome.desktop.app-folders.folder',
                                          path: path });
        this.actor = new St.Button({ style_class: 'app-well-app app-folder',
                                     button_mask: St.ButtonMask.ONE,
                                     toggle_mode: true,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });
        this.actor._delegate = this;
        // whether we need to update arrow side, position etc.
        this._popupInvalidated = false;

        this.icon = new IconGrid.BaseIcon('', { createIcon: Lang.bind(this, this._createIcon), setSizeManually: true });
        this.actor.set_child(this.icon.actor);
        this.actor.label_actor = this.icon.label;

        this.view = new FolderView();

        this.actor.connect('clicked', Lang.bind(this,
            function() {
                this._ensurePopup();
                this.view.actor.vscroll.adjustment.value = 0;
                this._openSpaceForPopup();
            }));
        this.actor.connect('notify::mapped', Lang.bind(this,
            function() {
                if (!this.actor.mapped && this._popup)
                    this._popup.popdown();
            }));

        this._folder.connect('changed', Lang.bind(this, this._redisplay));
        this._redisplay();
    },

    getAppIds: function() {
        return this.view.getAllItems().map(function(item) {
            return item.id;
        });
    },

    _updateName: function() {
        let name = _getFolderName(this._folder);
        if (this.name == name)
            return;

        this.name = name;
        this.icon.label.text = this.name;
        this.emit('name-changed');
    },

    _redisplay: function() {
        this._updateName();

        this.view.removeAll();

        let excludedApps = this._folder.get_strv('excluded-apps');
        let appSys = Shell.AppSystem.get_default();
        let addAppId = (function addAppId(appId) {
            if (excludedApps.indexOf(appId) >= 0)
                return;

            let app = appSys.lookup_app(appId);
            if (!app)
                return;

            if (!app.get_app_info().should_show())
                return;

            let icon = new AppIcon(app);
            this.view.addItem(icon);
        }).bind(this);

        let folderApps = this._folder.get_strv('apps');
        folderApps.forEach(addAppId);

        let folderCategories = this._folder.get_strv('categories');
        Gio.AppInfo.get_all().forEach(function(appInfo) {
            let appCategories = _getCategories(appInfo);
            if (!_listsIntersect(folderCategories, appCategories))
                return;

            try {
                addAppId(appInfo.get_id()); // catch invalid file encodings
            } catch(e) {
            }
        });

        this.actor.visible = this.view.getAllItems().length > 0;
        this.view.loadGrid();
        this.emit('apps-changed');
    },

    _createIcon: function(iconSize) {
        return this.view.createFolderIcon(iconSize, this);
    },

    _popupHeight: function() {
        let usedHeight = this.view.usedHeight() + this._popup.getOffset(St.Side.TOP) + this._popup.getOffset(St.Side.BOTTOM);
        return usedHeight;
    },

    _openSpaceForPopup: function() {
        let id = this._parentView.connect('space-ready', Lang.bind(this,
            function() {
                this._parentView.disconnect(id);
                this._popup.popup();
                this._updatePopupPosition();
            }));
        this._parentView.openSpaceForPopup(this, this._boxPointerArrowside, this.view.nRowsDisplayedAtOnce());
    },

    _calculateBoxPointerArrowSide: function() {
        let spaceTop = this.actor.y - this._parentView.getCurrentPageY();
        let spaceBottom = this._parentView.actor.height - (spaceTop + this.actor.height);

        return spaceTop > spaceBottom ? St.Side.BOTTOM : St.Side.TOP;
    },

    _updatePopupSize: function() {
        // StWidget delays style calculation until needed, make sure we use the correct values
        this.view._grid.actor.ensure_style();

        let offsetForEachSide = Math.ceil((this._popup.getOffset(St.Side.TOP) +
                                           this._popup.getOffset(St.Side.BOTTOM) -
                                           this._popup.getCloseButtonOverlap()) / 2);
        // Add extra padding to prevent boxpointer decorations and close button being cut off
        this.view.setPaddingOffsets(offsetForEachSide);
        this.view.adaptToSize(this._parentAvailableWidth, this._parentAvailableHeight);
    },

    _updatePopupPosition: function() {
        if (!this._popup)
            return;

        if (this._boxPointerArrowside == St.Side.BOTTOM)
            this._popup.actor.y = this.actor.allocation.y1 + this.actor.translation_y - this._popupHeight();
        else
            this._popup.actor.y = this.actor.allocation.y1 + this.actor.translation_y + this.actor.height;
    },

    _ensurePopup: function() {
        if (this._popup && !this._popupInvalidated)
            return;
        this._boxPointerArrowside = this._calculateBoxPointerArrowSide();
        if (!this._popup) {
            this._popup = new AppFolderPopup(this, this._boxPointerArrowside);
            this._parentView.addFolderPopup(this._popup);
            this._popup.connect('open-state-changed', Lang.bind(this,
                function(popup, isOpen) {
                    if (!isOpen)
                        this.actor.checked = false;
                }));
        } else {
            this._popup.updateArrowSide(this._boxPointerArrowside);
        }
        this._updatePopupSize();
        this._updatePopupPosition();
        this._popupInvalidated = false;
    },

    adaptToSize: function(width, height) {
        this._parentAvailableWidth = width;
        this._parentAvailableHeight = height;
        if(this._popup)
            this.view.adaptToSize(width, height);
        this._popupInvalidated = true;
    },
});
Signals.addSignalMethods(FolderIcon.prototype);

var AppFolderPopup = new Lang.Class({
    Name: 'AppFolderPopup',

    _init: function(source, side) {
        this._source = source;
        this._view = source.view;
        this._arrowSide = side;

        this._isOpen = false;
        this.parentOffset = 0;

        this.actor = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                     visible: false,
                                     // We don't want to expand really, but look
                                     // at the layout manager of our parent...
                                     //
                                     // DOUBLE HACK: if you set one, you automatically
                                     // get the effect for the other direction too, so
                                     // we need to set the y_align
                                     x_expand: true,
                                     y_expand: true,
                                     x_align: Clutter.ActorAlign.CENTER,
                                     y_align: Clutter.ActorAlign.START });
        this._boxPointer = new BoxPointer.BoxPointer(this._arrowSide,
                                                     { style_class: 'app-folder-popup-bin',
                                                       x_fill: true,
                                                       y_fill: true,
                                                       x_expand: true,
                                                       x_align: St.Align.START });

        this._boxPointer.actor.style_class = 'app-folder-popup';
        this.actor.add_actor(this._boxPointer.actor);
        this._boxPointer.bin.set_child(this._view.actor);

        this.closeButton = Util.makeCloseButton(this._boxPointer);
        this.closeButton.connect('clicked', Lang.bind(this, this.popdown));
        this.actor.add_actor(this.closeButton);

        this._boxPointer.actor.bind_property('opacity', this.closeButton, 'opacity',
                                             GObject.BindingFlags.SYNC_CREATE);

        global.focus_manager.add_group(this.actor);

        source.actor.connect('destroy', Lang.bind(this,
            function() {
                this.actor.destroy();
            }));
        this._grabHelper = new GrabHelper.GrabHelper(this.actor);
        this._grabHelper.addActor(Main.layoutManager.overviewGroup);
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPress));
    },

    _onKeyPress: function(actor, event) {
        if (global.stage.get_key_focus() != actor)
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
        switch (event.get_key_symbol()) {
            case Clutter.Down:
                direction = Gtk.DirectionType.TAB_FORWARD;
                break;
            case Clutter.Right:
                direction = isLtr ? Gtk.DirectionType.TAB_FORWARD :
                                    Gtk.DirectionType.TAB_BACKWARD;
                break;
            case Clutter.Up:
                direction = Gtk.DirectionType.TAB_BACKWARD;
                break;
            case Clutter.Left:
                direction = isLtr ? Gtk.DirectionType.TAB_BACKWARD :
                                    Gtk.DirectionType.TAB_FORWARD;
                break;
            default:
                return Clutter.EVENT_PROPAGATE;
        }
        return actor.navigate_focus(null, direction, false);
    },

    toggle: function() {
        if (this._isOpen)
            this.popdown();
        else
            this.popup();
    },

    popup: function() {
        if (this._isOpen)
            return;

        this._isOpen = this._grabHelper.grab({ actor: this.actor,
                                               onUngrab: Lang.bind(this, this.popdown) });

        if (!this._isOpen)
            return;

        this.actor.show();

        this._boxPointer.setArrowActor(this._source.actor);
        // We need to hide the icons of the view until the boxpointer animation
        // is completed so we can animate the icons after as we like without
        // showing them while boxpointer is animating.
        this._view.actor.opacity = 0;
        this._boxPointer.show(BoxPointer.PopupAnimation.FADE |
                              BoxPointer.PopupAnimation.SLIDE,
                              Lang.bind(this,
            function() {
                this._view.actor.opacity = 255;
                this._view.animate(IconGrid.AnimationDirection.IN);
            }));

        this.emit('open-state-changed', true);
    },

    popdown: function() {
        if (!this._isOpen)
            return;

        this._grabHelper.ungrab({ actor: this.actor });

        this._boxPointer.hide(BoxPointer.PopupAnimation.FADE |
                              BoxPointer.PopupAnimation.SLIDE);
        this._isOpen = false;
        this.emit('open-state-changed', false);
    },

    getCloseButtonOverlap: function() {
        return this.closeButton.get_theme_node().get_length('-shell-close-overlap-y');
    },

    getOffset: function (side) {
        let offset = this._boxPointer.getPadding(side);
        if (this._arrowSide == side)
            offset += this._boxPointer.getArrowHeight();
        return offset;
    },

    updateArrowSide: function (side) {
        this._arrowSide = side;
        this._boxPointer.updateArrowSide(side);
    }
});
Signals.addSignalMethods(AppFolderPopup.prototype);

var AppIcon = new Lang.Class({
    Name: 'AppIcon',

    _init : function(app, iconParams) {
        this.app = app;
        this.id = app.get_id();
        this.name = app.get_name();

        this.actor = new St.Button({ style_class: 'app-well-app',
                                     reactive: true,
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });

        this._dot = new St.Widget({ style_class: 'app-well-app-running-dot',
                                    layout_manager: new Clutter.BinLayout(),
                                    x_expand: true, y_expand: true,
                                    x_align: Clutter.ActorAlign.CENTER,
                                    y_align: Clutter.ActorAlign.END });

        this._iconContainer = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                              x_expand: true, y_expand: true });

        this.actor.set_child(this._iconContainer);
        this._iconContainer.add_child(this._dot);

        this.actor._delegate = this;

        if (!iconParams)
            iconParams = {};

        // Get the isDraggable property without passing it on to the BaseIcon:
        let appIconParams = Params.parse(iconParams, { isDraggable: true }, true);
        let isDraggable = appIconParams['isDraggable'];
        delete iconParams['isDraggable'];

        iconParams['createIcon'] = Lang.bind(this, this._createIcon);
        iconParams['setSizeManually'] = true;
        this.icon = new IconGrid.BaseIcon(app.get_name(), iconParams);
        this._iconContainer.add_child(this.icon.actor);

        this.actor.label_actor = this.icon.label;

        this.actor.connect('leave-event', Lang.bind(this, this._onLeaveEvent));
        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('touch-event', Lang.bind(this, this._onTouchEvent));
        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this.actor.connect('popup-menu', Lang.bind(this, this._onKeyboardPopupMenu));

        this._menu = null;
        this._menuManager = new PopupMenu.PopupMenuManager(this);

        if (isDraggable) {
            this._draggable = DND.makeDraggable(this.actor);
            this._draggable.connect('drag-begin', Lang.bind(this,
                function () {
                    this._removeMenuTimeout();
                    Main.overview.beginItemDrag(this);
                }));
            this._draggable.connect('drag-cancelled', Lang.bind(this,
                function () {
                    Main.overview.cancelledItemDrag(this);
                }));
            this._draggable.connect('drag-end', Lang.bind(this,
                function () {
                   Main.overview.endItemDrag(this);
                }));
        }

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._menuTimeoutId = 0;
        this._stateChangedId = this.app.connect('notify::state', Lang.bind(this,
            function () {
                this._updateRunningStyle();
            }));
        this._updateRunningStyle();
    },

    _onDestroy: function() {
        if (this._stateChangedId > 0)
            this.app.disconnect(this._stateChangedId);
        this._stateChangedId = 0;
        this._removeMenuTimeout();
    },

    _createIcon: function(iconSize) {
        return this.app.create_icon_texture(iconSize);
    },

    _removeMenuTimeout: function() {
        if (this._menuTimeoutId > 0) {
            Mainloop.source_remove(this._menuTimeoutId);
            this._menuTimeoutId = 0;
        }
    },

    _updateRunningStyle: function() {
        if (this.app.state != Shell.AppState.STOPPED)
            this._dot.show();
        else
            this._dot.hide();
    },

    _setPopupTimeout: function() {
        this._removeMenuTimeout();
        this._menuTimeoutId = Mainloop.timeout_add(MENU_POPUP_TIMEOUT,
            Lang.bind(this, function() {
                this._menuTimeoutId = 0;
                this.popupMenu();
                return GLib.SOURCE_REMOVE;
            }));
        GLib.Source.set_name_by_id(this._menuTimeoutId, '[gnome-shell] this.popupMenu');
    },

    _onLeaveEvent: function(actor, event) {
        this.actor.fake_release();
        this._removeMenuTimeout();
    },

    _onButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            this._setPopupTimeout();
        } else if (button == 3) {
            this.popupMenu();
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onTouchEvent: function (actor, event) {
        if (event.type() == Clutter.EventType.TOUCH_BEGIN)
            this._setPopupTimeout();

        return Clutter.EVENT_PROPAGATE;
    },

    _onClicked: function(actor, button) {
        this._removeMenuTimeout();
        this.activate(button);
    },

    _onKeyboardPopupMenu: function() {
        this.popupMenu();
        this._menu.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
    },

    getId: function() {
        return this.app.get_id();
    },

    popupMenu: function() {
        this._removeMenuTimeout();
        this.actor.fake_release();

        if (this._draggable)
            this._draggable.fakeRelease();

        if (!this._menu) {
            this._menu = new AppIconMenu(this);
            this._menu.connect('activate-window', Lang.bind(this, function (menu, window) {
                this.activateWindow(window);
            }));
            this._menu.connect('open-state-changed', Lang.bind(this, function (menu, isPoppedUp) {
                if (!isPoppedUp)
                    this._onMenuPoppedDown();
            }));
            let id = Main.overview.connect('hiding', Lang.bind(this, function () { this._menu.close(); }));
            this.actor.connect('destroy', function() {
                Main.overview.disconnect(id);
            });

            this._menuManager.addMenu(this._menu);
        }

        this.emit('menu-state-changed', true);

        this.actor.set_hover(true);
        this._menu.popup();
        this._menuManager.ignoreRelease();
        this.emit('sync-tooltip');

        return false;
    },

    activateWindow: function(metaWindow) {
        if (metaWindow) {
            Main.activateWindow(metaWindow);
        } else {
            Main.overview.hide();
        }
    },

    _onMenuPoppedDown: function() {
        this.actor.sync_hover();
        this.emit('menu-state-changed', false);
    },

    activate: function (button) {
        let event = Clutter.get_current_event();
        let modifiers = event ? event.get_state() : 0;
        let openNewWindow = this.app.can_open_new_window () &&
                            modifiers & Clutter.ModifierType.CONTROL_MASK &&
                            this.app.state == Shell.AppState.RUNNING ||
                            button && button == 2;

        if (this.app.state == Shell.AppState.STOPPED || openNewWindow)
            this.animateLaunch();

        if (openNewWindow)
            this.app.open_new_window(-1);
        else
            this.app.activate();

        Main.overview.hide();
    },

    animateLaunch: function() {
        this.icon.animateZoomOut();
    },

    shellWorkspaceLaunch : function(params) {
        params = Params.parse(params, { workspace: -1,
                                        timestamp: 0 });

        this.app.open_new_window(params.workspace);
    },

    getDragActor: function() {
        return this.app.create_icon_texture(Main.overview.dashIconSize);
    },

    // Returns the original actor that should align with the actor
    // we show as the item is being dragged.
    getDragActorSource: function() {
        return this.icon.icon;
    },

    shouldShowTooltip: function() {
        return this.actor.hover && (!this._menu || !this._menu.isOpen);
    },
});
Signals.addSignalMethods(AppIcon.prototype);

var AppIconMenu = new Lang.Class({
    Name: 'AppIconMenu',
    Extends: PopupMenu.PopupMenu,

    _init: function(source) {
        let side = St.Side.LEFT;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            side = St.Side.RIGHT;

        this.parent(source.actor, 0.5, side);

        // We want to keep the item hovered while the menu is up
        this.blockSourceEvents = true;

        this._source = source;

        this.actor.add_style_class_name('app-well-menu');

        // Chain our visibility and lifecycle to that of the source
        this._sourceMappedId = source.actor.connect('notify::mapped', () => {
            if (!source.actor.mapped)
                this.close();
        });
        source.actor.connect('destroy', () => {
            source.actor.disconnect(this._sourceMappedId);
            this.destroy();
        });

        Main.uiGroup.add_actor(this.actor);
    },

    _redisplay: function() {
        this.removeAll();

        let windows = this._source.app.get_windows().filter(function(w) {
            return !w.skip_taskbar;
        });

        // Display the app windows menu items and the separator between windows
        // of the current desktop and other windows.
        let activeWorkspace = global.screen.get_active_workspace();
        let separatorShown = windows.length > 0 && windows[0].get_workspace() != activeWorkspace;

        for (let i = 0; i < windows.length; i++) {
            let window = windows[i];
            if (!separatorShown && window.get_workspace() != activeWorkspace) {
                this._appendSeparator();
                separatorShown = true;
            }
            let item = this._appendMenuItem(window.title);
            item.connect('activate', Lang.bind(this, function() {
                this.emit('activate-window', window);
            }));
        }

        if (!this._source.app.is_window_backed()) {
            this._appendSeparator();

            let appInfo = this._source.app.get_app_info();
            let actions = appInfo.list_actions();
            if (this._source.app.can_open_new_window() &&
                actions.indexOf('new-window') == -1) {
                this._newWindowMenuItem = this._appendMenuItem(_("New Window"));
                this._newWindowMenuItem.connect('activate', Lang.bind(this, function() {
                    if (this._source.app.state == Shell.AppState.STOPPED)
                        this._source.animateLaunch();

                    this._source.app.open_new_window(-1);
                    this.emit('activate-window', null);
                }));
                this._appendSeparator();
            }

            if (discreteGpuAvailable &&
                this._source.app.state == Shell.AppState.STOPPED &&
                actions.indexOf('activate-discrete-gpu') == -1) {
                this._onDiscreteGpuMenuItem = this._appendMenuItem(_("Launch using Dedicated Graphics Card"));
                this._onDiscreteGpuMenuItem.connect('activate', Lang.bind(this, function() {
                    if (this._source.app.state == Shell.AppState.STOPPED)
                        this._source.animateLaunch();

                    this._source.app.launch(0, -1, true);
                    this.emit('activate-window', null);
                }));
            }

            for (let i = 0; i < actions.length; i++) {
                let action = actions[i];
                let item = this._appendMenuItem(appInfo.get_action_name(action));
                item.connect('activate', Lang.bind(this, function(emitter, event) {
                    this._source.app.launch_action(action, event.get_time(), -1);
                    this.emit('activate-window', null);
                }));
            }

            let canFavorite = global.settings.is_writable('favorite-apps');

            if (canFavorite) {
                this._appendSeparator();

                let isFavorite = AppFavorites.getAppFavorites().isFavorite(this._source.app.get_id());

                if (isFavorite) {
                    let item = this._appendMenuItem(_("Remove from Favorites"));
                    item.connect('activate', Lang.bind(this, function() {
                        let favs = AppFavorites.getAppFavorites();
                        favs.removeFavorite(this._source.app.get_id());
                    }));
                } else {
                    let item = this._appendMenuItem(_("Add to Favorites"));
                    item.connect('activate', Lang.bind(this, function() {
                        let favs = AppFavorites.getAppFavorites();
                        favs.addFavorite(this._source.app.get_id());
                    }));
                }
            }

            if (Shell.AppSystem.get_default().lookup_app('org.gnome.Software.desktop')) {
                this._appendSeparator();
                let item = this._appendMenuItem(_("Show Details"));
                item.connect('activate', Lang.bind(this, function() {
                    let id = this._source.app.get_id();
                    let args = GLib.Variant.new('(ss)', [id, '']);
                    Gio.DBus.get(Gio.BusType.SESSION, null,
                        function(o, res) {
                            let bus = Gio.DBus.get_finish(res);
                            bus.call('org.gnome.Software',
                                     '/org/gnome/Software',
                                     'org.gtk.Actions', 'Activate',
                                     GLib.Variant.new('(sava{sv})',
                                                      ['details', [args], null]),
                                     null, 0, -1, null, null);
                            Main.overview.hide();
                        });
                }));
            }
        }
    },

    _appendSeparator: function () {
        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this.addMenuItem(separator);
    },

    _appendMenuItem: function(labelText) {
        // FIXME: app-well-menu-item style
        let item = new PopupMenu.PopupMenuItem(labelText);
        this.addMenuItem(item);
        return item;
    },

    popup: function(activatingButton) {
        this._redisplay();
        this.open();
    }
});
Signals.addSignalMethods(AppIconMenu.prototype);

var SystemActionIcon = new Lang.Class({
    Name: 'SystemActionIcon',
    Extends: Search.GridSearchResult,

    activate: function() {
        SystemActions.getDefault().activateAction(this.metaInfo['id']);
        Main.overview.hide();
    }
});
