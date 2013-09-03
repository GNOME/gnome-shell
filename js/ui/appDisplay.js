// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const GMenu = imports.gi.GMenu;
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
const IconGrid = imports.ui.iconGrid;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const OverviewControls = imports.ui.overviewControls;
const PopupMenu = imports.ui.popupMenu;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const Params = imports.misc.params;
const Util = imports.misc.util;

const MAX_APPLICATION_WORK_MILLIS = 75;
const MENU_POPUP_TIMEOUT = 600;
const MAX_COLUMNS = 6;
const MIN_COLUMNS = 4;
const MIN_ROWS = 4;

const INACTIVE_GRID_OPACITY = 77;
const INACTIVE_GRID_OPACITY_ANIMATION_TIME = 0.40;
const FOLDER_SUBICON_FRACTION = .4;

const MIN_FREQUENT_APPS_COUNT = 3;

const INDICATORS_ANIMATION_TIME = 0.5;
// 100% means indicators wait for be animated until the previous one
// is animated completely. 0% means all animators are animated
// at once without delay
const INDICATORS_ANIMATION_DELAY_PERCENTAGE = 50;

// Fraction of page height the finger or mouse must reach
// to change page
const PAGE_SWITCH_TRESHOLD = 0.2;
const PAGE_SWITCH_TIME = 0.3;

// Recursively load a GMenuTreeDirectory; we could put this in ShellAppSystem too
function _loadCategory(dir, view) {
    let iter = dir.iter();
    let appSystem = Shell.AppSystem.get_default();
    let nextType;
    while ((nextType = iter.next()) != GMenu.TreeItemType.INVALID) {
        if (nextType == GMenu.TreeItemType.ENTRY) {
            let entry = iter.get_entry();
            let app = appSystem.lookup_app_by_tree_entry(entry);
            if (!entry.get_app_info().get_nodisplay())
                view.addApp(app);
        } else if (nextType == GMenu.TreeItemType.DIRECTORY) {
            let itemDir = iter.get_directory();
            if (!itemDir.get_is_nodisplay())
                _loadCategory(itemDir, view);
        }
    }
};

const BaseAppView = new Lang.Class({
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

        // Standard hack for ClutterBinLayout
        this._grid.actor.x_expand = true;

        this._items = {};
        this._allItems = [];
    },

    removeAll: function() {
        this._grid.removeAll();
        this._items = {};
        this._allItems = [];
    },

    _getItemId: function(item) {
        throw new Error('Not implemented');
    },

    _createItemIcon: function(item) {
        throw new Error('Not implemented');
    },

    _compareItems: function(a, b) {
        throw new Error('Not implemented');
    },

    _addItem: function(item) {
        let id = this._getItemId(item);
        if (this._items[id] !== undefined)
            return null;

        let itemIcon = this._createItemIcon(item);
        this._allItems.push(item);
        this._items[id] = itemIcon;

        return itemIcon;
    },

    loadGrid: function() {
        this._allItems.sort(this._compareItems);

        for (let i = 0; i < this._allItems.length; i++) {
            let id = this._getItemId(this._allItems[i]);
            if (!id)
                continue;
            this._grid.addItem(this._items[id]);
        }

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
    }
});
Signals.addSignalMethods(BaseAppView.prototype);


const PageIndicators = new Lang.Class({
    Name:'PageIndicators',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'page-indicators',
                                        vertical: true,
                                        x_expand: true, y_expand: true,
                                        x_align: Clutter.ActorAlign.END,
                                        y_align: Clutter.ActorAlign.CENTER });
        this._nPages = 0;
        this._currentPage = undefined;

        this.actor.connect('notify::mapped',
                           Lang.bind(this, this._animateIndicators));
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

    _animateIndicators: function() {
        if (!this.actor.mapped)
            return;

        let children = this.actor.get_children();
        if (children.length == 0)
            return;

        let timePerChild = INDICATORS_ANIMATION_TIME / this._nPages;
        let delay = INDICATORS_ANIMATION_DELAY_PERCENTAGE / 100 * timePerChild;

        let [stageX, ] = children[0].get_transformed_position();
        let offset;
        let monitor = Main.layoutManager.primaryMonitor;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            offset = monitor.x - stageX - children[0].width;
        else
            offset = monitor.x + monitor.width - stageX;

        for (let i = 0; i < this._nPages; i++) {
            children[i].translation_x = offset;
            Tweener.addTween(children[i],
                             { translation_x: 0,
                               time: timePerChild,
                               delay: delay * i,
                               transition: 'easeOutQuad'
                             });
        }
    }
});
Signals.addSignalMethods(PageIndicators.prototype);

const AllView = new Lang.Class({
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
                                    Gtk.PolicyType.AUTOMATIC);
        // we are only using ScrollView for the fade effect, hide scrollbars
        this._scrollView.vscroll.hide();
        this._adjustment = this._scrollView.vscroll.adjustment;

        this._pageIndicators = new PageIndicators();
        this._pageIndicators.connect('page-activated', Lang.bind(this,
            function(indicators, pageIndex) {
                this.goToPage(pageIndex);
            }));
        this.actor.add_actor(this._pageIndicators.actor);

        this._folderIcons = [];

        this._stack = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        let box = new St.BoxLayout({ vertical: true });

        this._currentPage = 0;
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
                this._scrollView.get_effect('fade').enabled = false;
                this.emit('space-ready');
            }));
        this._grid.connect('space-closed', Lang.bind(this,
            function() {
                this._displayingPopup = false;
            }));
    },

    getCurrentPageY: function() {
        return this._grid.getPageY(this._currentPage);
    },

    goToPage: function(pageNumber) {
        if (this._currentPage == pageNumber && this._displayingPopup && this._currentPopup)
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
        if (this._currentPage != pageNumber) {
            let minVelocity = totalHeight / (PAGE_SWITCH_TIME * 1000);
            velocity = Math.max(minVelocity, velocity);
            time = (diffToPage / velocity) / 1000;
        } else {
            time = PAGE_SWITCH_TIME * diffToPage / totalHeight;
        }
        // When changing more than one page, make sure to not take
        // longer than PAGE_SWITCH_TIME
        time = Math.min(time, PAGE_SWITCH_TIME);

        if (pageNumber < this._grid.nPages() && pageNumber >= 0) {
            this._currentPage = pageNumber;
            Tweener.addTween(this._adjustment,
                             { value: this._grid.getPageY(this._currentPage),
                               time: time,
                               transition: 'easeOutQuad' });
            this._pageIndicators.setCurrentPage(pageNumber);
        }
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
        this._scrollView.get_effect('fade').enabled = true;
        this._grid.closeExtraSpace();
    },

    _onScroll: function(actor, event) {
         if(this._displayingPopup)
            return;
        let direction = event.get_scroll_direction();
        if (direction == Clutter.ScrollDirection.UP) {
            if (this._currentPage > 0)
                this.goToPage(this._currentPage - 1);
        } else {
            if (direction == Clutter.ScrollDirection.DOWN) {
                if (this._currentPage < (this._grid.nPages() - 1))
                    this.goToPage(this._currentPage + 1);
            }
        }
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
        let diffCurrentPage = this._diffToPage(this._currentPage);
        if (diffCurrentPage > this._scrollView.height * PAGE_SWITCH_TRESHOLD) {
            if (action.get_velocity(0)[2] > 0 && this._currentPage > 0)
                this.goToPage(this._currentPage - 1);
            else if (this._currentPage < this._grid.nPages() - 1)
                this.goToPage(this._currentPage + 1);
        } else {
            this.goToPage(this._currentPage);
        }
        this._panning = false;
    },

    _getItemId: function(item) {
        if (item instanceof Shell.App)
            return item.get_id();
        else if (item instanceof GMenu.TreeDirectory)
            return item.get_menu_id();
        else
            return null;
    },

    _createItemIcon: function(item) {
        if (item instanceof Shell.App)
            return new AppIcon(item);
        else if (item instanceof GMenu.TreeDirectory)
            return new FolderIcon(item, this);
        else
            return null;
    },

    _compareItems: function(itemA, itemB) {
        // bit of a hack: rely on both ShellApp and GMenuTreeDirectory
        // having a get_name() method
        let nameA = GLib.utf8_collate_key(itemA.get_name(), -1);
        let nameB = GLib.utf8_collate_key(itemB.get_name(), -1);
        return (nameA > nameB) ? 1 : (nameA < nameB ? -1 : 0);
    },

    removeAll: function() {
        this._folderIcons = [];
        this.parent();
    },

    addApp: function(app) {
        let appIcon = this._addItem(app);
        if (appIcon)
            appIcon.actor.connect('key-focus-in',
                                  Lang.bind(this, this._ensureIconVisible));
    },

    addFolder: function(dir) {
        let folderIcon = this._addItem(dir);
        this._folderIcons.push(folderIcon);
        if (folderIcon)
            folderIcon.actor.connect('key-focus-in',
                                     Lang.bind(this, this._ensureIconVisible));
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

    _ensureIconVisible: function(icon) {
        let itemPage = this._grid.getItemPage(icon);
        this.goToPage(itemPage);
    },

    _updateAdjustment: function(availHeight) {
        this._adjustment.page_size = availHeight;
        this._adjustment.upper = this._stack.height;
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

        this._updateAdjustment(availHeight);

        this._grid.adaptToSize(availWidth, availHeight);

        let fadeOffset = Math.min(this._grid.topPadding,
                                  this._grid.bottomPadding);
        this._scrollView.update_fade_effect(fadeOffset, 0);
        this._scrollView.get_effect('fade').fade_edges = true;

        if (this._availWidth != availWidth || this._availHeight != availHeight || oldNPages != this._grid.nPages()) {
            this._adjustment.value = 0;
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this,
                function() {
                    this._pageIndicators.setNPages(this._grid.nPages());
                    this._pageIndicators.setCurrentPage(0);
                }));
        }

        this._availWidth = availWidth;
        this._availHeight = availHeight;
        // Update folder views
        for (let i = 0; i < this._folderIcons.length; i++)
            this._folderIcons[i].adaptToSize(availWidth, availHeight);
    }
});
Signals.addSignalMethods(AllView.prototype);

const FrequentView = new Lang.Class({
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
    },

    hasUsefulData: function() {
        return this._usage.get_most_used("").length >= MIN_FREQUENT_APPS_COUNT;
    },

    loadApps: function() {
        let mostUsed = this._usage.get_most_used ("");
        let hasUsefulData = this.hasUsefulData();
        this._noFrequentAppsLabel.visible = !hasUsefulData;
        if(!hasUsefulData)
            return;

        for (let i = 0; i < mostUsed.length; i++) {
            if (!mostUsed[i].get_app_info().should_show())
                continue;
            let appIcon = new AppIcon(mostUsed[i]);
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

const Views = {
    FREQUENT: 0,
    ALL: 1
};

const ControlsBoxLayout = Lang.Class({
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

const ViewStackLayout = new Lang.Class({
    Name: 'ViewStackLayout',
    Extends: Clutter.BinLayout,

    vfunc_allocate: function (actor, box, flags) {
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        // Prepare children of all views for the upcoming allocation, calculate all
        // the needed values to adapt available size
        this.emit('allocated-size-changed', availWidth, availHeight);
        this.parent(actor, box, flags);
    }
});
Signals.addSignalMethods(ViewStackLayout.prototype);

const AppDisplay = new Lang.Class({
    Name: 'AppDisplay',

    _init: function() {
        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('installed-changed', Lang.bind(this, function() {
            Main.queueDeferredWork(this._allAppsWorkId);
        }));
        Main.overview.connect('showing', Lang.bind(this, function() {
            Main.queueDeferredWork(this._frequentAppsWorkId);
        }));
        global.settings.connect('changed::app-folder-categories', Lang.bind(this, function() {
            Main.queueDeferredWork(this._allAppsWorkId);
        }));
        this._privacySettings = new Gio.Settings({ schema: 'org.gnome.desktop.privacy' });
        this._privacySettings.connect('changed::remember-app-usage',
                                      Lang.bind(this, this._updateFrequentVisibility));

        this._views = [];

        let view, button;
        view = new FrequentView();
        button = new St.Button({ label: _("Frequent"),
                                 style_class: 'app-view-control',
                                 can_focus: true,
                                 x_expand: true });
        this._views[Views.FREQUENT] = { 'view': view, 'control': button };

        view = new AllView();
        button = new St.Button({ label: _("All"),
                                 style_class: 'app-view-control',
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
        this.actor.add_actor(this._viewStack, { expand: true });
        let layout = new ControlsBoxLayout({ homogeneous: true });
        this._controls = new St.Widget({ style_class: 'app-view-controls',
                                         layout_manager: layout });
        layout.hookup_style(this._controls);
        this.actor.add_actor(new St.Bin({ child: this._controls }));

        for (let i = 0; i < this._views.length; i++) {
            this._viewStack.add_actor(this._views[i].view.actor);
            this._controls.add_actor(this._views[i].control);

            let viewIndex = i;
            this._views[i].control.connect('clicked', Lang.bind(this,
                function(actor) {
                    this._showView(viewIndex);
                }));
        }
        let frequentUseful = this._views[Views.FREQUENT].view.hasUsefulData();
        this._showView(frequentUseful ? Views.FREQUENT : Views.ALL);
        this._updateFrequentVisibility();

        // We need a dummy actor to catch the keyboard focus if the
        // user Ctrl-Alt-Tabs here before the deferred work creates
        // our real contents
        this._focusDummy = new St.Bin({ can_focus: true });
        this._viewStack.add_actor(this._focusDummy);

        this._allAppsWorkId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplayAllApps));
        this._frequentAppsWorkId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplayFrequentApps));
    },

    _showView: function(activeIndex) {
        for (let i = 0; i < this._views.length; i++) {
            let actor = this._views[i].view.actor;
            let params = { time: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
                           opacity: (i == activeIndex) ? 255 : 0 };
            if (i == activeIndex)
                actor.visible = true;
            else
                params.onComplete = function() { actor.hide(); };
            Tweener.addTween(actor, params);

            if (i == activeIndex)
                this._views[i].control.add_style_pseudo_class('checked');
            else
                this._views[i].control.remove_style_pseudo_class('checked');
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

    _redisplay: function() {
        this._redisplayFrequentApps();
        this._redisplayAllApps();
    },

    _redisplayFrequentApps: function() {
        let view = this._views[Views.FREQUENT].view;

        view.removeAll();
        view.loadApps();
    },

    _redisplayAllApps: function() {
        let view = this._views[Views.ALL].view;

        view.removeAll();

        let tree = this._appSystem.get_tree();
        let root = tree.get_root_directory();

        let iter = root.iter();
        let nextType;
        let folderCategories = global.settings.get_strv('app-folder-categories');
        while ((nextType = iter.next()) != GMenu.TreeItemType.INVALID) {
            if (nextType == GMenu.TreeItemType.DIRECTORY) {
                let dir = iter.get_directory();
                if (dir.get_is_nodisplay())
                    continue;

                if (folderCategories.indexOf(dir.get_menu_id()) != -1)
                    view.addFolder(dir);
                else
                    _loadCategory(dir, view);
            }
        }
        view.loadGrid();

        if (this._focusDummy) {
            let focused = this._focusDummy.has_key_focus();
            this._focusDummy.destroy();
            this._focusDummy = null;
            if (focused)
                this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        }
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

const AppSearchProvider = new Lang.Class({
    Name: 'AppSearchProvider',

    _init: function() {
        this._appSys = Shell.AppSystem.get_default();
        this.id = 'applications';
    },

    getResultMetas: function(apps, callback) {
        let metas = [];
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            metas.push({ 'id': app,
                         'name': app.get_name(),
                         'createIcon': function(size) {
                             return app.create_icon_texture(size);
                         }
                       });
        }
        callback(metas);
    },

    getInitialResultSet: function(terms) {
        this.searchSystem.setResults(this, this._appSys.initial_search(terms));
    },

    getSubsearchResultSet: function(previousResults, terms) {
        this.searchSystem.setResults(this, this._appSys.subsearch(previousResults, terms));
    },

    activateResult: function(app) {
        let event = Clutter.get_current_event();
        let modifiers = event ? event.get_state() : 0;
        let openNewWindow = modifiers & Clutter.ModifierType.CONTROL_MASK;

        if (openNewWindow)
            app.open_new_window(-1);
        else
            app.activate();
    },

    dragActivateResult: function(id, params) {
        params = Params.parse(params, { workspace: -1,
                                        timestamp: 0 });

        let app = this._appSys.lookup_app(id);
        app.open_new_window(workspace);
    },

    createResultObject: function (resultMeta, terms) {
        let app = resultMeta['id'];
        return new AppIcon(app);
    }
});

const FolderView = new Lang.Class({
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

    _getItemId: function(item) {
        return item.get_id();
    },

    _createItemIcon: function(item) {
        return new AppIcon(item);
    },

    _compareItems: function(a, b) {
        return a.compare_by_name(b);
    },

    addApp: function(app) {
        this._addItem(app);
    },

    createFolderIcon: function(size) {
        let icon = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                   style_class: 'app-folder-icon',
                                   width: size, height: size });
        let subSize = Math.floor(FOLDER_SUBICON_FRACTION * size);

        let aligns = [ Clutter.ActorAlign.START, Clutter.ActorAlign.END ];
        for (let i = 0; i < Math.min(this._allItems.length, 4); i++) {
            let texture = this._allItems[i].create_icon_texture(subSize);
            let bin = new St.Bin({ child: texture,
                                   x_expand: true, y_expand: true });
            bin.set_x_align(aligns[i % 2]);
            bin.set_y_align(aligns[Math.floor(i / 2)]);
            icon.add_actor(bin);
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

        // Set extra padding to avoid popup or close button being cut off
        this._grid.topPadding = Math.max(this._grid.topPadding - this._offsetForEachSide, 0);
        this._grid.bottomPadding = Math.max(this._grid.topPadding - this._offsetForEachSide, 0);
        this._grid.leftPadding = Math.max(this._grid.topPadding - this._offsetForEachSide, 0);
        this._grid.rightPadding = Math.max(this._grid.topPadding - this._offsetForEachSide, 0);

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

const FolderIcon = new Lang.Class({
    Name: 'FolderIcon',

    _init: function(dir, parentView) {
        this._dir = dir;
        this._parentView = parentView;

        this.actor = new St.Button({ style_class: 'app-well-app app-folder',
                                     button_mask: St.ButtonMask.ONE,
                                     toggle_mode: true,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });
        this.actor._delegate = this;
        // whether we need to update arrow side, position etc.
        this._popupInvalidated = false;

        let label = this._dir.get_name();
        this.icon = new IconGrid.BaseIcon(label,
                                          { createIcon: Lang.bind(this, this._createIcon), setSizeManually: true });
        this.actor.set_child(this.icon.actor);
        this.actor.label_actor = this.icon.label;

        this.view = new FolderView();
        _loadCategory(dir, this.view);
        this.view.loadGrid();

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

        let offsetForEachSide = (this._popup.getOffset(St.Side.TOP) +
                                 this._popup.getOffset(St.Side.BOTTOM) -
                                 this._popup.getCloseButtonOverlap()) / 2;
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

const AppFolderPopup = new Lang.Class({
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

        this.closeButton = Util.makeCloseButton();
        this.closeButton.connect('clicked', Lang.bind(this, this.popdown));
        this.actor.add_actor(this.closeButton);

        this._boxPointer.actor.bind_property('opacity', this.closeButton, 'opacity',
                                             GObject.BindingFlags.SYNC_CREATE);

        global.focus_manager.add_group(this.actor);

        source.actor.connect('destroy', Lang.bind(this,
            function() {
                this.actor.destroy();
            }));
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPress));
    },

    _onKeyPress: function(actor, event) {
        if (!this._isOpen)
            return false;

        if (event.get_key_symbol() != Clutter.KEY_Escape)
            return false;

        this.popdown();
        return true;
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

        this.actor.show();

        this._boxPointer.setArrowActor(this._source.actor);
        this._boxPointer.show(BoxPointer.PopupAnimation.FADE |
                              BoxPointer.PopupAnimation.SLIDE);

        this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);

        this._isOpen = true;
        this.emit('open-state-changed', true);
    },

    popdown: function() {
        if (!this._isOpen)
            return;

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

const AppIcon = new Lang.Class({
    Name: 'AppIcon',

    _init : function(app, iconParams) {
        this.app = app;
        this.actor = new St.Button({ style_class: 'app-well-app',
                                     reactive: true,
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO,
                                     can_focus: true,
                                     x_fill: true,
                                     y_fill: true });
        this.actor._delegate = this;

        if (!iconParams)
            iconParams = {};

        iconParams['createIcon'] = Lang.bind(this, this._createIcon);
        iconParams['setSizeManually'] = true;
        this.icon = new IconGrid.BaseIcon(app.get_name(), iconParams);
        this.actor.set_child(this.icon.actor);

        this.actor.label_actor = this.icon.label;

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this.actor.connect('popup-menu', Lang.bind(this, this._onKeyboardPopupMenu));

        this._menu = null;
        this._menuManager = new PopupMenu.PopupMenuManager(this);

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

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._menuTimeoutId = 0;
        this._stateChangedId = this.app.connect('notify::state',
                                                Lang.bind(this,
                                                          this._onStateChanged));
        this._onStateChanged();
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

    _onStateChanged: function() {
        if (this.app.state != Shell.AppState.STOPPED)
            this.actor.add_style_class_name('running');
        else
            this.actor.remove_style_class_name('running');
    },

    _onButtonPress: function(actor, event) {
        let button = event.get_button();
        if (button == 1) {
            this._removeMenuTimeout();
            this._menuTimeoutId = Mainloop.timeout_add(MENU_POPUP_TIMEOUT,
                Lang.bind(this, function() {
                    this.popupMenu();
                }));
        } else if (button == 3) {
            this.popupMenu();
            return true;
        }
        return false;
    },

    _onClicked: function(actor, button) {
        this._removeMenuTimeout();

        if (button == 1) {
            this._onActivate(Clutter.get_current_event());
        } else if (button == 2) {
            // Last workspace is always empty
            let launchWorkspace = global.screen.get_workspace_by_index(global.screen.n_workspaces - 1);
            launchWorkspace.activate(global.get_current_time());
            this.emit('launching');
            this.app.open_new_window(-1);
            Main.overview.hide();
        }
        return false;
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
            Main.overview.connect('hiding', Lang.bind(this, function () { this._menu.close(); }));

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

    _onActivate: function (event) {
        this.emit('launching');
        let modifiers = event.get_state();

        if (modifiers & Clutter.ModifierType.CONTROL_MASK
            && this.app.state == Shell.AppState.RUNNING) {
            this.app.open_new_window(-1);
        } else {
            this.app.activate();
        }

        Main.overview.hide();
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

const AppIconMenu = new Lang.Class({
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

        this.connect('activate', Lang.bind(this, this._onActivate));

        this.actor.add_style_class_name('app-well-menu');

        // Chain our visibility and lifecycle to that of the source
        source.actor.connect('notify::mapped', Lang.bind(this, function () {
            if (!source.actor.mapped)
                this.close();
        }));
        source.actor.connect('destroy', Lang.bind(this, function () { this.actor.destroy(); }));

        Main.uiGroup.add_actor(this.actor);
    },

    _redisplay: function() {
        this.removeAll();

        let windows = this._source.app.get_windows();

        // Display the app windows menu items and the separator between windows
        // of the current desktop and other windows.
        let activeWorkspace = global.screen.get_active_workspace();
        let separatorShown = windows.length > 0 && windows[0].get_workspace() != activeWorkspace;

        for (let i = 0; i < windows.length; i++) {
            if (!separatorShown && windows[i].get_workspace() != activeWorkspace) {
                this._appendSeparator();
                separatorShown = true;
            }
            let item = this._appendMenuItem(windows[i].title);
            item._window = windows[i];
        }

        if (!this._source.app.is_window_backed()) {
            if (windows.length > 0)
                this._appendSeparator();

            let isFavorite = AppFavorites.getAppFavorites().isFavorite(this._source.app.get_id());

            this._newWindowMenuItem = this._appendMenuItem(_("New Window"));
            this._appendSeparator();

            this._toggleFavoriteMenuItem = this._appendMenuItem(isFavorite ? _("Remove from Favorites")
                                                                : _("Add to Favorites"));
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
    },

    _onActivate: function (actor, child) {
        if (child._window) {
            let metaWindow = child._window;
            this.emit('activate-window', metaWindow);
        } else if (child == this._newWindowMenuItem) {
            this._source.app.open_new_window(-1);
            this.emit('activate-window', null);
        } else if (child == this._toggleFavoriteMenuItem) {
            let favs = AppFavorites.getAppFavorites();
            let isFavorite = favs.isFavorite(this._source.app.get_id());
            if (isFavorite)
                favs.removeFavorite(this._source.app.get_id());
            else
                favs.addFavorite(this._source.app.get_id());
        }
        this.close();
    }
});
Signals.addSignalMethods(AppIconMenu.prototype);
