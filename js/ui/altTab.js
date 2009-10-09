/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const AppIcon = imports.ui.appIcon;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const POPUP_ARROW_COLOR = new Clutter.Color();
POPUP_ARROW_COLOR.from_pixel(0xffffffff);
const POPUP_UNFOCUSED_ARROW_COLOR = new Clutter.Color();
POPUP_UNFOCUSED_ARROW_COLOR.from_pixel(0x808080ff);
const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

const POPUP_APPICON_SIZE = 96;
const POPUP_LIST_SPACING = 8;

const DISABLE_HOVER_TIMEOUT = 500; // milliseconds

const THUMBNAIL_SIZE = 256;
const THUMBNAIL_POPUP_TIME = 500; // milliseconds
const THUMBNAIL_FADE_TIME = 0.2; // seconds

function mod(a, b) {
    return (a + b) % b;
}

function AltTabPopup() {
    this._init();
}

AltTabPopup.prototype = {
    _init : function() {
        this.actor = new Clutter.Group({ reactive: true,
                                         x: 0,
                                         y: 0,
                                         width: global.screen_width,
                                         height: global.screen_height });

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._haveModal = false;

        this._currentApp = 0;
        this._currentWindow = 0;
        this._thumbnailTimeoutId = 0;
        this._motionTimeoutId = 0;

        // Initially disable hover so we ignore the enter-event if
        // the switcher appears underneath the current pointer location
        this._disableHover();

        global.stage.add_actor(this.actor);
    },

    show : function(backward) {
        let tracker = Shell.WindowTracker.get_default();
        let apps = tracker.get_running_apps ("");

        if (!apps.length)
            return false;

        if (!Main.pushModal(this.actor))
            return false;
        this._haveModal = true;

        this._keyPressEventId = global.stage.connect('key-press-event', Lang.bind(this, this._keyPressEvent));
        this._keyReleaseEventId = global.stage.connect('key-release-event', Lang.bind(this, this._keyReleaseEvent));

        this.actor.connect('button-press-event', Lang.bind(this, this._clickedOutside));
        this.actor.connect('scroll-event', Lang.bind(this, this._onScroll));

        this._appSwitcher = new AppSwitcher(apps);
        this.actor.add_actor(this._appSwitcher.actor);
        this._appSwitcher.connect('item-activated', Lang.bind(this, this._appActivated));
        this._appSwitcher.connect('item-entered', Lang.bind(this, this._appEntered));

        let primary = global.get_primary_monitor();
        this._appSwitcher.actor.x = primary.x + Math.floor((primary.width - this._appSwitcher.actor.width) / 2);
        this._appSwitcher.actor.y = primary.y + Math.floor((primary.height - this._appSwitcher.actor.height) / 2);

        this._appIcons = this._appSwitcher.icons;

        // Make the initial selection
        if (this._appIcons.length == 1) {
            if (!backward && this._appIcons[0].cachedWindows.length > 1) {
                // For compatibility with the multi-app case below
                this._select(0, 1, true);
            } else
                this._select(0);
        } else if (backward) {
            this._select(this._appIcons.length - 1);
        } else {
            let firstWindows = this._appIcons[0].cachedWindows;
            if (firstWindows.length > 1) {
                let curAppNextWindow = firstWindows[1];
                let nextAppWindow = this._appIcons[1].cachedWindows[0];

                // If the next window of the current app is more-recently-used
                // than the first window of the next app, then select it.
                if (curAppNextWindow.get_workspace() == global.screen.get_active_workspace() &&
                    curAppNextWindow.get_user_time() > nextAppWindow.get_user_time())
                    this._select(0, 1, true);
                else
                    this._select(1);
            } else
                this._select(1);
        }

        // There's a race condition; if the user released Alt before
        // we got the grab, then we won't be notified. (See
        // https://bugzilla.gnome.org/show_bug.cgi?id=596695 for
        // details.) So we check now. (Have to do this after updating
        // selection.)
        let mods = global.get_modifier_keys();
        if (!(mods & Gdk.ModifierType.MOD1_MASK)) {
            this._finish();
            return false;
        }

        return true;
    },

    _nextApp : function() {
        return mod(this._currentApp + 1, this._appIcons.length);
    },
    _previousApp : function() {
        return mod(this._currentApp - 1, this._appIcons.length);
    },

    _nextWindow : function() {
        return mod(this._currentWindow + 1,
                   this._appIcons[this._currentApp].cachedWindows.length);
    },
    _previousWindow : function() {
        return mod(this._currentWindow - 1,
                   this._appIcons[this._currentApp].cachedWindows.length);
    },

    _keyPressEvent : function(actor, event) {
        let keysym = event.get_key_symbol();
        let shift = (Shell.get_event_state(event) & Clutter.ModifierType.SHIFT_MASK);

        this._disableHover();

        // The WASD stuff is for debugging in Xephyr, where the arrow
        // keys aren't mapped correctly

        if (keysym == Clutter.grave)
            this._select(this._currentApp, shift ? this._previousWindow() : this._nextWindow());
        else if (keysym == Clutter.Escape)
            this.destroy();
        else if (this._thumbnailsFocused) {
            if (keysym == Clutter.Tab) {
                if (shift && this._currentWindow == 0)
                    this._select(this._previousApp());
                else if (!shift && this._currentWindow == this._appIcons[this._currentApp].cachedWindows.length - 1)
                    this._select(this._nextApp());
                else
                    this._select(this._currentApp, shift ? this._previousWindow() : this._nextWindow());
            } else if (keysym == Clutter.Left || keysym == Clutter.a)
                this._select(this._currentApp, this._previousWindow());
            else if (keysym == Clutter.Right || keysym == Clutter.d)
                this._select(this._currentApp, this._nextWindow());
            else if (keysym == Clutter.Up || keysym == Clutter.w)
                this._select(this._currentApp, null, true);
        } else {
            if (keysym == Clutter.Tab)
                this._select(shift ? this._previousApp() : this._nextApp());
            else if (keysym == Clutter.Left || keysym == Clutter.a)
                this._select(this._previousApp());
            else if (keysym == Clutter.Right || keysym == Clutter.d)
                this._select(this._nextApp());
            else if (keysym == Clutter.Down || keysym == Clutter.s)
                this._select(this._currentApp, this._currentWindow);
        }

        return true;
    },

    _keyReleaseEvent : function(actor, event) {
        let keysym = event.get_key_symbol();

        if (keysym == Clutter.Alt_L || keysym == Clutter.Alt_R)
            this._finish();

        return true;
    },

    _onScroll : function(actor, event) {
        let direction = event.get_scroll_direction();
        if (direction == Clutter.ScrollDirection.UP) {
            if (this._thumbnailsFocused) {
                if (this._currentWindow == 0)
                    this._select(this._previousApp());
                else
                    this._select(this._currentApp, this._previousWindow());
            } else {
                let nwindows = this._appIcons[this._currentApp].cachedWindows.length;
                if (nwindows > 1)
                    this._select(this._currentApp, nwindows - 1);
                else
                    this._select(this._previousApp());
            }
        } else if (direction == Clutter.ScrollDirection.DOWN) {
            if (this._thumbnailsFocused) {
                if (this._currentWindow == this._appIcons[this._currentApp].cachedWindows.length - 1)
                    this._select(this._nextApp());
                else
                    this._select(this._currentApp, this._nextWindow());
            } else {
                let nwindows = this._appIcons[this._currentApp].cachedWindows.length;
                if (nwindows > 1)
                    this._select(this._currentApp, 0);
                else
                    this._select(this._nextApp());
            }
        }
    },

    _clickedOutside : function(actor, event) {
        this.destroy();
    },

    _appActivated : function(appSwitcher, n) {
        // If the user clicks on the selected app, activate the
        // selected window; otherwise (eg, they click on an app while
        // !mouseActive) activate the first window of the clicked-on
        // app.
        let window = (n == this._currentApp) ? this._currentWindow : 0;
        Main.activateWindow(this._appIcons[n].cachedWindows[window]);
        this.destroy();
    },

    _appEntered : function(appSwitcher, n) {
        if (!this._mouseActive)
            return;

        this._select(n);
    },

    _windowActivated : function(thumbnailList, n) {
        Main.activateWindow(this._appIcons[this._currentApp].cachedWindows[n]);
        this.destroy();
    },

    _windowEntered : function(thumbnailList, n) {
        if (!this._mouseActive)
            return;

        this._select(this._currentApp, n);
    },

    _disableHover : function() {
        this._mouseActive = false;

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);

        this._motionTimeoutId = Mainloop.timeout_add(DISABLE_HOVER_TIMEOUT, Lang.bind(this, this._mouseTimedOut));
    },

    _mouseTimedOut : function() {
        this._motionTimeoutId = 0;
        this._mouseActive = true;
    },

    _finish : function() {
        let app = this._appIcons[this._currentApp];
        let window = app.cachedWindows[this._currentWindow];
        Main.activateWindow(window);
        this.destroy();
    },

    destroy : function() {
        this.actor.destroy();
    },

    _onDestroy : function() {
        if (this._haveModal)
            Main.popModal(this.actor);

        if (this._keyPressEventId)
            global.stage.disconnect(this._keyPressEventId);
        if (this._keyReleaseEventId)
            global.stage.disconnect(this._keyReleaseEventId);

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);
        if (this._thumbnailTimeoutId != 0)
            Mainloop.source_remove(this._thumbnailTimeoutId);
    },

    /**
     * _select:
     * @app: index of the app to select
     * @window: (optional) index of which of @app's windows to select
     * @forceAppFocus: optional flag, see below
     *
     * Selects the indicated @app, and optional @window, and sets
     * this._thumbnailsFocused appropriately to indicate whether the
     * arrow keys should act on the app list or the thumbnail list.
     *
     * If @app is specified and @window is unspecified or %null, then
     * the app is highlighted (ie, given a light background), and the
     * current thumbnail list, if any, is destroyed. If @app has
     * multiple windows, and @forceAppFocus is not %true, then a
     * timeout is started to open a thumbnail list.
     *
     * If @app and @window are specified (and @forceAppFocus is not),
     * then @app will be outlined, a thumbnail list will be created
     * and focused (if it hasn't been already), and the @window'th
     * window in it will be highlighted.
     *
     * If @app and @window are specified and @forceAppFocus is %true,
     * then @app will be highlighted, and @window outlined, and the
     * app list will have the keyboard focus.
     */
    _select : function(app, window, forceAppFocus) {
        if (app != this._currentApp || window == null) {
            if (this._thumbnails)
                this._destroyThumbnails();
        }

        if (this._thumbnailTimeoutId != 0) {
            Mainloop.source_remove(this._thumbnailTimeoutId);
            this._thumbnailTimeoutId = 0;
        }

        this._thumbnailsFocused = (window != null) && !forceAppFocus;

        this._currentApp = app;
        this._currentWindow = window ? window : 0;
        this._appSwitcher.highlight(app, this._thumbnailsFocused);

        if (window != null) {
            if (!this._thumbnails)
                this._createThumbnails();
            this._currentWindow = window;
            this._thumbnails.highlight(window, forceAppFocus);
        } else if (this._appIcons[this._currentApp].cachedWindows.length > 1 &&
                   !forceAppFocus) {
            this._thumbnailTimeoutId = Mainloop.timeout_add (
                THUMBNAIL_POPUP_TIME,
                Lang.bind(this, function () {
                              this._select(this._currentApp, 0, true);
                              return false;
                          }));
        }
    },

    _destroyThumbnails : function() {
        Tweener.addTween(this._thumbnails.actor,
                         { opacity: 0,
                           time: THUMBNAIL_FADE_TIME,
                           transition: "easeOutQuad",
                           onComplete: function() { this.destroy(); }
                         });
        this._thumbnails = null;
    },

    _createThumbnails : function() {
        this._thumbnails = new ThumbnailList (this._appIcons[this._currentApp].cachedWindows);
        this._thumbnails.connect('item-activated', Lang.bind(this, this._windowActivated));
        this._thumbnails.connect('item-entered', Lang.bind(this, this._windowEntered));

        this.actor.add_actor(this._thumbnails.actor);

        let thumbnailCenter;
        if (this._thumbnails.actor.width < this._appSwitcher.actor.width) {
            // Center the thumbnails under the corresponding AppIcon.
            // If this is being called when the switcher is first
            // being brought up, then nothing will have been assigned
            // an allocation yet, and the get_transformed_position()
            // call will return 0,0.
            // (http://bugzilla.openedhand.com/show_bug.cgi?id=1115).
            // Calling clutter_actor_get_allocation_box() would force
            // it to properly allocate itself, but we can't call that
            // because it has an out-caller-allocates arg. So we use
            // clutter_stage_get_actor_at_pos(), which will force a
            // reallocation as a side effect.
            global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, 0, 0);

            let icon = this._appIcons[this._currentApp].actor;
            let [stageX, stageY] = icon.get_transformed_position();
            thumbnailCenter = stageX + icon.width / 2;
        } else {
            // Center the thumbnails on the monitor
            let primary = global.get_primary_monitor();
            thumbnailCenter = primary.x + primary.width / 2;
        }

        this._thumbnails.actor.x = Math.floor(thumbnailCenter - this._thumbnails.actor.width / 2);
        this._thumbnails.actor.y = this._appSwitcher.actor.y + this._appSwitcher.actor.height + POPUP_LIST_SPACING;

        this._thumbnails.actor.opacity = 0;
        Tweener.addTween(this._thumbnails.actor,
                         { opacity: 255,
                           time: THUMBNAIL_FADE_TIME,
                           transition: "easeOutQuad"
                         });
    }
};

function SwitcherList(squareItems) {
    this._init(squareItems);
}

SwitcherList.prototype = {
    _init : function(squareItems) {
        this.actor = new St.Bin({ style_class: 'switcher-list' });

        // Here we use a GenericContainer so that we can force all the
        // children except the separator to have the same width.
        this._list = new Shell.GenericContainer();
        this._list.spacing = POPUP_LIST_SPACING;

        this._list.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._list.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._list.connect('allocate', Lang.bind(this, this._allocate));

        this.actor.add_actor(this._list);

        this._items = [];
        this._highlighted = -1;
        this._separator = null;
        this._squareItems = squareItems;
    },

    addItem : function(item) {
        // We want the St.Bin's padding to be clickable (since it will
        // be part of the highlighted background color), so we put the
        // bin inside the ButtonBox rather than vice versa.
        let bin = new St.Bin({ style_class: 'item-box' });
        let bbox = new Shell.ButtonBox({ reactive: true });

        bin.add_actor(item);
        bbox.append(bin, Big.BoxPackFlags.NONE);
        this._list.add_actor(bbox);

        let n = this._items.length;
        bbox.connect('activate', Lang.bind(this, function () {
                                               this._itemActivated(n);
                                          }));
        bbox.connect('enter-event', Lang.bind(this, function () {
                                                  this._itemEntered(n);
                                              }));

        bbox._bin = bin;
        this._items.push(bbox);
    },

    addSeparator: function () {
        let box = new St.Bin({ style_class: 'separator' })
        this._separator = box;
        this._list.add_actor(box);
    },
    
    highlight: function(index, justOutline) {
        if (this._highlighted != -1)
            this._items[this._highlighted]._bin.style_class = 'item-box';

        this._highlighted = index;

        if (this._highlighted != -1) {
            if (justOutline)
                this._items[this._highlighted]._bin.style_class = 'outlined-item-box';
            else
                this._items[this._highlighted]._bin.style_class = 'selected-item-box';
        }
    },

    _itemActivated: function(n) {
        this.emit('item-activated', n);
    },
    
    _itemEntered: function(n) {
        this.emit('item-entered', n);
    },

    _maxChildWidth: function (forHeight) {
        let maxChildMin = 0;
        let maxChildNat = 0;

        for (let i = 0; i < this._items.length; i++) {
            let [childMin, childNat] = this._items[i].get_preferred_width(forHeight);
            maxChildMin = Math.max(childMin, maxChildMin);
            maxChildNat = Math.max(childNat, maxChildNat);

            if (this._squareItems) {
                let [childMin, childNat] = this._items[i].get_preferred_height(-1);
                maxChildMin = Math.max(childMin, maxChildMin);
                maxChildNat = Math.max(childNat, maxChildNat);
            }
        }

        return [maxChildMin, maxChildNat];
    },

    _getPreferredWidth: function (actor, forHeight, alloc) {
        let [maxChildMin, maxChildNat] = this._maxChildWidth(forHeight);

        let separatorWidth = 0;
        if (this._separator) {
            let [sepMin, sepNat] = this._separator.get_preferred_width(forHeight);
            separatorWidth = sepNat + this._list.spacing;
        }

        let totalSpacing = this._list.spacing * (this._items.length - 1);
        alloc.min_size = this._items.length * maxChildMin + separatorWidth + totalSpacing;
        alloc.nat_size = this._items.length * maxChildNat + separatorWidth + totalSpacing;
    },

    _getPreferredHeight: function (actor, forWidth, alloc) {
        let maxChildMin = 0;
        let maxChildNat = 0;

        for (let i = 0; i < this._items.length; i++) {
            let [childMin, childNat] = this._items[i].get_preferred_height(-1);
            maxChildMin = Math.max(childMin, maxChildMin);
            maxChildNat = Math.max(childNat, maxChildNat);
        }

        if (this._squareItems) {
            let [childMin, childNat] = this._maxChildWidth(-1);
            maxChildMin = Math.max(childMin, maxChildMin);
            maxChildNat = Math.max(childNat, maxChildNat);
        }

        alloc.min_size = maxChildMin;
        alloc.nat_size = maxChildNat;
    },

    _allocate: function (actor, box, flags) {
        let childHeight = box.y2 - box.y1;

        let [maxChildMin, maxChildNat] = this._maxChildWidth(childHeight);
        let totalSpacing = this._list.spacing * (this._items.length - 1);

        let separatorWidth = 0;
        if (this._separator) {
            let [sepMin, sepNat] = this._separator.get_preferred_width(childHeight);
            separatorWidth = sepNat;
            totalSpacing += this._list.spacing;
        }

        let childWidth = Math.floor(Math.max(0, box.x2 - box.x1 - totalSpacing - separatorWidth) / this._items.length);

        let x = 0;
        let children = this._list.get_children();
        let childBox = new Clutter.ActorBox();
        for (let i = 0; i < children.length; i++) {
            if (this._items.indexOf(children[i]) != -1) {
                let [childMin, childNat] = children[i].get_preferred_height(childWidth);
                let vSpacing = (childHeight - childNat) / 2;
                childBox.x1 = x;
                childBox.y1 = vSpacing;
                childBox.x2 = x + childWidth;
                childBox.y2 = childBox.y1 + childNat;
                children[i].allocate(childBox, flags);

                x += this._list.spacing + childWidth;
            } else if (children[i] == this._separator) {
                // We want the separator to be more compact than the rest.
                childBox.x1 = x;
                childBox.y1 = 0;
                childBox.x2 = x + separatorWidth;
                childBox.y2 = childHeight;
                children[i].allocate(childBox, flags);
                x += this._list.spacing + separatorWidth;
            } else {
                // Something else, eg, AppSwitcher's arrows;
                // we don't allocate it.
            }
        }
    }
};

Signals.addSignalMethods(SwitcherList.prototype);

function AppSwitcher(apps) {
    this._init(apps);
}

AppSwitcher.prototype = {
    __proto__ : SwitcherList.prototype,

    _init : function(apps) {
        SwitcherList.prototype._init.call(this, true);

        // Construct the AppIcons, sort by time, add to the popup
        let activeWorkspace = global.screen.get_active_workspace();
        let workspaceIcons = [];
        let otherIcons = [];
        for (let i = 0; i < apps.length; i++) {
            let appIcon = new AppIcon.AppIcon({ app: apps[i],
                                                size: POPUP_APPICON_SIZE });
            // Cache the window list now; we don't handle dynamic changes here,
            // and we don't want to be continually retrieving it
            appIcon.cachedWindows = appIcon.app.get_windows();
            if (this._hasWindowsOnWorkspace(appIcon, activeWorkspace))
              workspaceIcons.push(appIcon);
            else
              otherIcons.push(appIcon);
        }

        workspaceIcons.sort(Lang.bind(this, this._sortAppIcon));
        otherIcons.sort(Lang.bind(this, this._sortAppIcon));

        this.icons = [];
        this._arrows = [];
        for (let i = 0; i < workspaceIcons.length; i++)
            this._addIcon(workspaceIcons[i]);
        if (workspaceIcons.length > 0 && otherIcons.length > 0)
            this.addSeparator();
        for (let i = 0; i < otherIcons.length; i++)
            this._addIcon(otherIcons[i]);

        this._curApp = -1;
    },

    _allocate: function (actor, box, flags) {
        // Allocate the main list items
        SwitcherList.prototype._allocate.call(this, actor, box, flags);

        let arrowHeight = Math.floor(this.actor.get_theme_node().get_padding(St.Side.BOTTOM) / 3);
        let arrowWidth = arrowHeight * 2;

        // Now allocate each arrow underneath its item
        let childBox = new Clutter.ActorBox();
        for (let i = 0; i < this._items.length; i++) {
            let itemBox = this._items[i].allocation;
            childBox.x1 = Math.floor(itemBox.x1 + (itemBox.x2 - itemBox.x1 - arrowWidth) / 2);
            childBox.x2 = childBox.x1 + arrowWidth;
            childBox.y1 = itemBox.y2 + arrowHeight;
            childBox.y2 = childBox.y1 + arrowHeight;
            this._arrows[i].allocate(childBox, flags);
        }
    },

    // We override SwitcherList's highlight() method to also deal with
    // the AppSwitcher->ThumbnailList arrows. Apps with only 1 window
    // will hide their arrows by default, but show them when their
    // thumbnails are visible (ie, when the app icon is supposed to be
    // in justOutline mode). Apps with multiple windows will normally
    // show a dim arrow, but show a bright arrow when they are
    // highlighted; their redraw handler will use the right color
    // based on this._curApp; we just need to do a queue_relayout() to
    // force it to redraw. (queue_redraw() doesn't work because
    // ShellDrawingArea only redraws on allocate.)
    highlight : function(n, justOutline) {
        if (this._curApp != -1) {
            if (this.icons[this._curApp].cachedWindows.length == 1)
                this._arrows[this._curApp].hide();
            else
                this._arrows[this._curApp].queue_relayout();
        }

        SwitcherList.prototype.highlight.call(this, n, justOutline);
        this._curApp = n;

        if (this._curApp != -1) {
            if (justOutline && this.icons[this._curApp].cachedWindows.length == 1)
                this._arrows[this._curApp].show();
            else
                this._arrows[this._curApp].queue_relayout();
        }
    },

    _addIcon : function(appIcon) {
        this.icons.push(appIcon);
        this.addItem(appIcon.actor);

        // SwitcherList creates its own Shell.ButtonBox; we want to
        // avoid intercepting the events it wants.
        appIcon.actor.reactive = false;

        let n = this._arrows.length;
        let arrow = new Shell.DrawingArea();
        arrow.connect('redraw', Lang.bind(this,
            function (area, texture) {
                Shell.draw_box_pointer(texture, Shell.PointerDirection.DOWN,
                                       TRANSPARENT_COLOR,
                                       this._curApp == n ? POPUP_ARROW_COLOR : POPUP_UNFOCUSED_ARROW_COLOR);
            }));
        this._list.add_actor(arrow);
        this._arrows.push(arrow);

        if (appIcon.cachedWindows.length == 1)
            arrow.hide();
    },

    _hasWindowsOnWorkspace: function(appIcon, workspace) {
        let windows = appIcon.cachedWindows;
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_workspace() == workspace)
                return true;
        }
        return false;
    },

    _sortAppIcon : function(appIcon1, appIcon2) {
        return appIcon1.app.compare(appIcon2.app);
    }
};

function ThumbnailList(windows) {
    this._init(windows);
}

ThumbnailList.prototype = {
    __proto__ : SwitcherList.prototype,

    _init : function(windows) {
        SwitcherList.prototype._init.call(this);

        for (let i = 0; i < windows.length; i++) {
            let mutterWindow = windows[i].get_compositor_private();
            let windowTexture = mutterWindow.get_texture ();
            let [width, height] = windowTexture.get_size();
            let scale = Math.min(1.0, THUMBNAIL_SIZE / width, THUMBNAIL_SIZE / height);

            let box = new St.BoxLayout({ style_class: "thumbnail-box",
                                         vertical: true });

            let clone = new Clutter.Clone ({ source: windowTexture,
                                             reactive: true,
                                             width: width * scale,
                                             height: height * scale });
            box.add_actor(clone);

            let name = new St.Label({ text: windows[i].get_title() });
            // St.Label doesn't support text-align so use a Bin
            let bin = new St.Bin({ x_align: St.Align.MIDDLE });
            bin.add_actor(name);
            box.add_actor(bin);

            this.addItem(box);
        }
    }
};
