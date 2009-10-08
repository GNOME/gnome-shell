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

const POPUP_ARROW_COLOR = new Clutter.Color();
POPUP_ARROW_COLOR.from_pixel(0xffffffff);
const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);
const POPUP_SEPARATOR_COLOR = new Clutter.Color();
POPUP_SEPARATOR_COLOR.from_pixel(0x80808066);

const POPUP_APPICON_SIZE = 96;
const POPUP_LIST_SPACING = 8;

const POPUP_POINTER_SELECTION_THRESHOLD = 3;

const THUMBNAIL_SIZE = 256;
const THUMBNAIL_POPUP_TIME = 1000; // milliseconds

const HOVER_TIME = 500; // milliseconds

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
        this._currentWindows = [];
        this._thumbnailTimeoutId = 0;

        global.stage.add_actor(this.actor);
    },

    show : function(backward) {
        let appMonitor = Shell.AppMonitor.get_default();
        let apps = appMonitor.get_running_apps ("");

        if (!apps.length)
            return false;

        if (!Main.pushModal(this.actor))
            return false;
        this._haveModal = true;

        this._keyPressEventId = global.stage.connect('key-press-event', Lang.bind(this, this._keyPressEvent));
        this._keyReleaseEventId = global.stage.connect('key-release-event', Lang.bind(this, this._keyReleaseEvent));

        this._motionEventId = this.actor.connect('motion-event', Lang.bind(this, this._mouseMoved));
        this._mouseActive = false;
        this._mouseMovement = 0;

        this._appSwitcher = new AppSwitcher(apps);
        this.actor.add_actor(this._appSwitcher.actor);
        this._appSwitcher.connect('item-activated', Lang.bind(this, this._appActivated));
        this._appSwitcher.connect('item-hovered', Lang.bind(this, this._appHovered));

        let primary = global.get_primary_monitor();
        this._appSwitcher.actor.x = primary.x + Math.floor((primary.width - this._appSwitcher.actor.width) / 2);
        this._appSwitcher.actor.y = primary.y + Math.floor((primary.height - this._appSwitcher.actor.height) / 2);

        this._appIcons = this._appSwitcher.icons;

        // _currentWindows give the index of the selected window for
        // each app; they all start at 0.
        this._currentWindows = this._appIcons.map(function (app) { return 0; });

        // Make the initial selection
        if (this._appIcons.length == 1) {
            if (!backward && this._appIcons[0].windows.length > 1) {
                // For compatibility with the multi-app case below
                this._select(0, 1);
            } else
                this._select(0);
        } else if (backward) {
            this._select(this._appIcons.length - 1);
        } else {
            if (this._appIcons[0].windows.length > 1) {
                let curAppNextWindow = this._appIcons[0].windows[1];
                let nextAppWindow = this._appIcons[1].windows[0];

                // If the next window of the current app is more-recently-used
                // than the first window of the next app, then select it.
                if (curAppNextWindow.get_workspace() == global.screen.get_active_workspace() &&
                    curAppNextWindow.get_user_time() > nextAppWindow.get_user_time())
                    this._select(0, 1);
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
        return mod(this._currentWindows[this._currentApp] + 1,
                   this._appIcons[this._currentApp].windows.length);
    },
    _previousWindow : function() {
        return mod(this._currentWindows[this._currentApp] - 1,
                   this._appIcons[this._currentApp].windows.length);
    },

    _keyPressEvent : function(actor, event) {
        let keysym = event.get_key_symbol();
        let shift = (Shell.get_event_state(event) & Clutter.ModifierType.SHIFT_MASK);

        // The WASD stuff is for debugging in Xephyr, where the arrow
        // keys aren't mapped correctly

        if (keysym == Clutter.Tab)
            this._select(shift ? this._previousApp() : this._nextApp());
        else if (keysym == Clutter.grave)
            this._select(this._currentApp, shift ? this._previousWindow() : this._nextWindow());
        else if (keysym == Clutter.Escape)
            this.destroy();
        else if (this._thumbnails) {
            if (keysym == Clutter.Left || keysym == Clutter.a)
                this._select(this._currentApp, this._previousWindow());
            else if (keysym == Clutter.Right || keysym == Clutter.d)
                this._select(this._currentApp, this._nextWindow());
            else if (keysym == Clutter.Up || keysym == Clutter.w)
                this._select(this._currentApp, null, true);
        } else {
            if (keysym == Clutter.Left || keysym == Clutter.a)
                this._select(this._previousApp());
            else if (keysym == Clutter.Right || keysym == Clutter.d)
                this._select(this._nextApp());
            else if (keysym == Clutter.Down || keysym == Clutter.s)
                this._select(this._currentApp, this._currentWindows[this._currentApp]);
        }

        return true;
    },

    _keyReleaseEvent : function(actor, event) {
        let keysym = event.get_key_symbol();

        if (keysym == Clutter.Alt_L || keysym == Clutter.Alt_R)
            this._finish();

        return true;
    },

    _appActivated : function(appSwitcher, n) {
        Main.activateWindow(this._appIcons[n].windows[this._currentWindows[n]]);
        this.destroy();
    },

    _appHovered : function(appSwitcher, n) {
        if (!this._mouseActive)
            return;

        this._select(n, this._currentWindows[n]);
    },

    _windowActivated : function(thumbnailList, n) {
        Main.activateWindow(this._appIcons[this._currentApp].windows[n]);
        this.destroy();
    },

    _windowHovered : function(thumbnailList, n) {
        if (!this._mouseActive)
            return;

        this._select(this._currentApp, n);
    },

    _mouseMoved : function(actor, event) {
        if (++this._mouseMovement < POPUP_POINTER_SELECTION_THRESHOLD)
            return;

        this.actor.disconnect(this._motionEventId);
        this._mouseActive = true;

        this._appSwitcher.checkHover();
        if (this._thumbnails)
            this._thumbnails.checkHover();
    },

    _finish : function() {
        let app = this._appIcons[this._currentApp];
        let window = app.windows[this._currentWindows[this._currentApp]];
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

        if (this._thumbnailTimeoutId != 0)
            Mainloop.source_remove(this._thumbnailTimeoutId);
    },

    _select : function(app, window, noTimeout) {
        if (app != this._currentApp || !window) {
            if (this._thumbnails) {
                this._thumbnails.actor.destroy();
                this._thumbnails = null;
            }
            this._appSwitcher.showArrow(-1);
        }

        if (this._thumbnailTimeoutId != 0) {
            Mainloop.source_remove(this._thumbnailTimeoutId);
            this._thumbnailTimeoutId = 0;
        }

        this._currentApp = app;
        if (window != null) {
            this._appSwitcher.highlight(-1);
            this._appSwitcher.showArrow(app);
        } else {
            this._appSwitcher.highlight(app);
            if (this._appIcons[this._currentApp].windows.length > 1)
                this._appSwitcher.showArrow(app);
        }

        if (window != null) {
            if (!this._thumbnails)
                this._createThumbnails();
            this._currentWindows[this._currentApp] = window;
            this._thumbnails.highlight(window);
        } else if (this._appIcons[this._currentApp].windows.length > 1 &&
                   !noTimeout) {
            this._thumbnailTimeoutId = Mainloop.timeout_add (
                THUMBNAIL_POPUP_TIME,
                Lang.bind(this, function () {
                              this._select(this._currentApp,
                                           this._currentWindows[this._currentApp]);
                              return false;
                          }));
        }
    },

    _createThumbnails : function() {
        this._thumbnails = new ThumbnailList (this._appIcons[this._currentApp].windows);
        this._thumbnails.connect('item-activated', Lang.bind(this, this._windowActivated));
        this._thumbnails.connect('item-hovered', Lang.bind(this, this._windowHovered));

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
    }
};

function SwitcherList(squareItems) {
    this._init(squareItems);
}

SwitcherList.prototype = {
    _init : function(squareItems) {
        this.actor = new St.Bin({ style_class: 'switcher-list' });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

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

        this._hoverTimeout = 0;
    },

    _onDestroy: function() {
        if (this._hoverTimeout != 0) {
            Mainloop.source_remove(this._hoverTimeout);
            this._hoverTimeout = 0;
        }
    },

    addItem : function(item) {
        let box = new St.Bin({ style_class: 'item-box' });
        let bbox;

        if (item instanceof Shell.ButtonBox)
            bbox = item;
        else {
            bbox = new Shell.ButtonBox({ reactive: true });
            bbox.append(item, Big.BoxPackFlags.NONE);
        }
        box.add_actor(bbox);
        this._list.add_actor(box);

        let n = this._items.length;
        bbox.connect('activate', Lang.bind(this, function () {
                                               this._itemActivated(n);
                                          }));
        bbox.connect('notify::hover', Lang.bind(this, function () {
                                                    this._hoverChanged(bbox, n);
                                               }));

        this._items.push(box);
    },

    addSeparator: function () {
        // FIXME: make this work with StWidgets and CSS
        let box = new Big.Box({ padding_top: 2, padding_bottom: 2 });
        box.append(new Clutter.Rectangle({ width: 1,
                                           color: POPUP_SEPARATOR_COLOR }),
                   Big.BoxPackFlags.EXPAND);
        this._separator = box;
        this._list.add_actor(box);
    },
    
    highlight: function(index) {
        if (this._highlighted != -1)
            this._items[this._highlighted].style_class = 'item-box';

        this._highlighted = index;

        if (this._highlighted != -1)
            this._items[this._highlighted].style_class = 'selected-item-box';
    },

    // Used after the mouse movement exceeds the threshold, to check
    // if it's already hovering over an icon
    checkHover: function() {
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].get_child().hover) {
                this._hoverChanged(this._items[i].get_child(), i);
                return;
            }
        }
    },

    _itemActivated: function(n) {
        this.emit('item-activated', n);
    },
    
    _hoverChanged: function(box, n) {
        if (this._hoverTimeout != 0) {
            Mainloop.source_remove(this._hoverTimeout);
            this._hoverTimeout = 0;
        }

        if (box.hover) {
            this._hoverTimeout = Mainloop.timeout_add(
                HOVER_TIME,
                Lang.bind (this, function () {
                               this._itemHovered(n);
                           }));
        }
    },

    _itemHovered: function(n) {
        this.emit('item-hovered', n);
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
            let appIcon = new AppIcon.AppIcon({ appInfo: apps[i],
                                                size: POPUP_APPICON_SIZE });
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

        this._shownArrow = -1;
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

    showArrow : function(n) {
        if (this._shownArrow != -1)
            this._arrows[this._shownArrow].hide();

        this._shownArrow = n;

        if (this._shownArrow != -1)
            this._arrows[this._shownArrow].show();
    },

    _addIcon : function(appIcon) {
        this.icons.push(appIcon);
        this.addItem(appIcon.actor);

        let arrow = new Shell.DrawingArea();
        arrow.connect('redraw', Lang.bind(this,
            function (area, texture) {
                Shell.draw_box_pointer(texture, Shell.PointerDirection.DOWN,
                                       TRANSPARENT_COLOR,
                                       POPUP_ARROW_COLOR);
            }));
        this._list.add_actor(arrow);
        this._arrows.push(arrow);
        arrow.hide();
    },

    _hasWindowsOnWorkspace: function(appIcon, workspace) {
        for (let i = 0; i < appIcon.windows.length; i++) {
            if (appIcon.windows[i].get_workspace() == workspace)
                return true;
        }
        return false;
    },

    _hasVisibleWindows : function(appIcon) {
        for (let i = 0; i < appIcon.windows.length; i++) {
            if (appIcon.windows[i].showing_on_its_workspace())
                return true;
        }
        return false;
    },

    _sortAppIcon : function(appIcon1, appIcon2) {
        let vis1 = this._hasVisibleWindows(appIcon1);
        let vis2 = this._hasVisibleWindows(appIcon2);

        if (vis1 && !vis2) {
            return -1;
        } else if (vis2 && !vis1) {
            return 1;
        } else {
            // The app's most-recently-used window is first
            // in its list
            return (appIcon2.windows[0].get_user_time() -
                    appIcon1.windows[0].get_user_time());
        }
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

            let clone = new Clutter.Clone ({ source: windowTexture,
                                             reactive: true,
                                             width: width * scale,
                                             height: height * scale });
            let box = new Big.Box({ padding: AppIcon.APPICON_BORDER_WIDTH + AppIcon.APPICON_PADDING });
            box.append(clone, Big.BoxPackFlags.NONE);
            this.addItem(box);
        }
    }
};
