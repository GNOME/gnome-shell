/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;

const Main = imports.ui.main;
const BoxPointer = imports.ui.boxpointer;
const Tweener = imports.ui.tweener;

const POPUP_ANIMATION_TIME = 0.1;

function PopupBaseMenuItem(reactive) {
    this._init(reactive);
}

PopupBaseMenuItem.prototype = {
    _init: function (reactive) {
        this.actor = new St.Bin({ style_class: 'popup-menu-item',
                                  reactive: reactive,
                                  track_hover: reactive,
                                  x_fill: true,
                                  y_fill: true,
                                  x_align: St.Align.START });
        this.actor._delegate = this;
        this.active = false;

        if (reactive) {
            this.actor.connect('button-release-event', Lang.bind(this, function (actor, event) {
                this.emit('activate', event);
            }));
            this.actor.connect('notify::hover', Lang.bind(this, this._hoverChanged));
        }
    },

    _hoverChanged: function (actor) {
        this.setActive(actor.hover);
    },

    activate: function (event) {
        this.emit('activate', event);
    },

    setActive: function (active) {
        let activeChanged = active != this.active;

        if (activeChanged) {
            this.active = active;
            if (active)
                this.actor.add_style_pseudo_class('active');
            else
                this.actor.remove_style_pseudo_class('active');
            this.emit('active-changed', active);
        }
    }
};
Signals.addSignalMethods(PopupBaseMenuItem.prototype);

function PopupMenuItem(text) {
    this._init(text);
}

PopupMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function (text) {
        PopupBaseMenuItem.prototype._init.call(this, true);

        this.label = new St.Label({ text: text });
        this.actor.set_child(this.label);
    }
};

function PopupSeparatorMenuItem() {
    this._init();
}

PopupSeparatorMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function () {
        PopupBaseMenuItem.prototype._init.call(this, false);

        this._drawingArea = new St.DrawingArea({ style_class: 'popup-separator-menu-item' });
        this.actor.set_child(this._drawingArea);
        this._drawingArea.connect('repaint', Lang.bind(this, this._onRepaint));
    },

    _onRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();
        let found, margin, gradientHeight;
        [found, margin] = themeNode.get_length('-margin-horizontal', false);
        [found, gradientHeight] = themeNode.get_length('-gradient-height', false);
        let startColor = new Clutter.Color();
        themeNode.get_color('-gradient-start', false, startColor);
        let endColor = new Clutter.Color();
        themeNode.get_color('-gradient-end', false, endColor);

        let gradientWidth = (width - margin * 2);
        let gradientOffset = (height - gradientHeight) / 2;
        let pattern = new Cairo.LinearGradient(margin, gradientOffset, width - margin, gradientOffset + gradientHeight);
        pattern.addColorStopRGBA(0, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
        pattern.addColorStopRGBA(0.5, endColor.red / 255, endColor.green / 255, endColor.blue / 255, endColor.alpha / 255);
        pattern.addColorStopRGBA(1, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
        cr.setSource(pattern);
        cr.rectangle(margin, gradientOffset, gradientWidth, gradientHeight);
        cr.fill();
    }
};

function PopupImageMenuItem(text, iconName, alwaysShowImage) {
    this._init(text, iconName, alwaysShowImage);
}

// We need to instantiate a GtkImageMenuItem so it
// hooks up its properties on the GtkSettings
var _gtkImageMenuItemCreated = false;

PopupImageMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function (text, iconName, alwaysShowImage) {
        PopupBaseMenuItem.prototype._init.call(this, true);

        if (!_gtkImageMenuItemCreated) {
            let menuItem = new Gtk.ImageMenuItem();
            menuItem.destroy();
            _gtkImageMenuItemCreated = true;
        }

        this._alwaysShowImage = alwaysShowImage;
        this._iconName = iconName;
        this._size = 16;

        let box = new St.BoxLayout({ style_class: 'popup-image-menu-item' });
        this.actor.set_child(box);
        this._imageBin = new St.Bin({ width: this._size, height: this._size });
        box.add(this._imageBin, { y_fill: false });
        box.add(new St.Label({ text: text }), { expand: true });

        if (!alwaysShowImage) {
            let settings = Gtk.Settings.get_default();
            settings.connect('notify::gtk-menu-images', Lang.bind(this, this._onMenuImagesChanged));
        }
        this._onMenuImagesChanged();
    },

    _onMenuImagesChanged: function() {
        let show;
        if (this._alwaysShowImage) {
            show = true;
        } else {
            let settings = Gtk.Settings.get_default();
            show = settings.gtk_menu_images;
        }
        if (!show) {
            this._imageBin.hide();
        } else {
            let img = St.TextureCache.get_default().load_icon_name(this._iconName, this._size);
            this._imageBin.set_child(img);
            this._imageBin.show();
        }
    }
};

function mod(a, b) {
    return (a + b) % b;
}

function findNextInCycle(items, current, direction) {
    let cur;

    if (items.length == 0)
        return current;
    else if (items.length == 1)
        return items[0];

    if (current)
        cur = items.indexOf(current);
    else if (direction == 1)
        cur = items.length - 1;
    else
        cur = 0;

    return items[mod(cur + direction, items.length)];
}

function PopupMenu(sourceActor, alignment, arrowSide, gap) {
    this._init(sourceActor, alignment, arrowSide, gap);
}

PopupMenu.prototype = {
    _init: function(sourceActor, alignment, arrowSide, gap) {
        this.sourceActor = sourceActor;
        this._alignment = alignment;
        this._arrowSide = arrowSide;
        this._gap = gap;

        this._boxPointer = new BoxPointer.BoxPointer(arrowSide,
                                                     { x_fill: true,
                                                       y_fill: true,
                                                       x_align: St.Align.START });
        this.actor = this._boxPointer.actor;
        this.actor.style_class = 'popup-menu-boxpointer';
        this._box = new St.BoxLayout({ style_class: 'popup-menu-content',
                                       vertical: true });
        this._boxPointer.bin.set_child(this._box);
        this.actor.add_style_class_name('popup-menu');

        this.isOpen = false;
        this._activeMenuItem = null;
    },

    addAction: function(title, callback) {
        var menuItem = new PopupMenuItem(title);
        this.addMenuItem(menuItem);
        menuItem.connect('activate', Lang.bind(this, function (menuItem, event) {
            callback(event);
        }));
    },

    addMenuItem: function(menuItem) {
        this._box.add(menuItem.actor);
        menuItem._activeChangeId = menuItem.connect('active-changed', Lang.bind(this, function (menuItem, active) {
            if (active && this._activeMenuItem != menuItem) {
                if (this._activeMenuItem)
                    this._activeMenuItem.setActive(false);
                this._activeMenuItem = menuItem;
                this.emit('active-changed', menuItem);
            } else if (!active && this._activeMenuItem == menuItem) {
                this._activeMenuItem = null;
                this.emit('active-changed', null);
            }
        }));
        menuItem._activateId = menuItem.connect('activate', Lang.bind(this, function (menuItem, event) {
            this.emit('activate', menuItem);
            this.close();
        }));
    },

    addActor: function(actor) {
        this._box.add(actor);
    },

    getMenuItems: function() {
        return this._box.get_children().map(function (actor) { return actor._delegate; });
    },

    removeAll: function() {
        let children = this.getMenuItems();
        for (let i = 0; i < children.length; i++) {
            let item = children[i];
            if (item._activeChangeId != 0)
                item.disconnect(item._activeChangeId);
            if (item._activateId != 0)
                item.disconnect(item._activateId);
            item.actor.destroy();
        }
    },

    setArrowOrigin: function(origin) {
        this._boxPointer.setArrowOrigin(origin);
    },

    open: function() {
        if (this.isOpen)
            return;

        this.emit('opening');

        let primary = global.get_primary_monitor();

        // We need to show it now to force an allocation,
        // so that we can query the correct size.
        this.actor.show();

        // Position correctly relative to the sourceActor
        let [sourceX, sourceY] = this.sourceActor.get_transformed_position();
        let [sourceWidth, sourceHeight] = this.sourceActor.get_transformed_size();

        let [minWidth, minHeight, natWidth, natHeight] = this.actor.get_preferred_size();

        let menuX, menuY;
        let menuWidth = natWidth, menuHeight = natHeight;

        // Position the non-pointing axis
        switch (this._arrowSide) {
        case St.Side.TOP:
            menuY = sourceY + sourceHeight + this._gap;
            break;
        case St.Side.BOTTOM:
            menuY = sourceY - menuHeight - this._gap;
            break;
        case St.Side.LEFT:
            menuX = sourceX + sourceWidth + this._gap;
            break;
        case St.Side.RIGHT:
            menuX = sourceX - menuWidth - this._gap;
            break;
        }

        // Now align and position the pointing axis, making sure
        // it fits on screen
        switch (this._arrowSide) {
        case St.Side.TOP:
        case St.Side.BOTTOM:
            switch (this._alignment) {
            case St.Align.START:
                menuX = sourceX;
                break;
            case St.Align.MIDDLE:
                menuX = sourceX - Math.floor((menuWidth - sourceWidth) / 2);
                break;
            case St.Align.END:
                menuX = sourceX - (menuWidth - sourceWidth);
                break;
            }

            menuX = Math.min(menuX, primary.x + primary.width - menuWidth);
            menuX = Math.max(menuX, primary.x);

            this._boxPointer.setArrowOrigin((sourceX - menuX) + Math.floor(sourceWidth / 2));
            break;

        case St.Side.LEFT:
        case St.Side.RIGHT:
            switch (this._alignment) {
            case St.Align.START:
                menuY = sourceY;
                break;
            case St.Align.MIDDLE:
                menuY = sourceY - Math.floor((menuHeight - sourceHeight) / 2);
                break;
            case St.Align.END:
                menuY = sourceY - (menuHeight - sourceHeight);
                break;
            }

            menuY = Math.min(menuY, primary.y + primary.height - menuHeight);
            menuY = Math.max(menuY, primary.y);

            this._boxPointer.setArrowOrigin((sourceY - menuY) + Math.floor(sourceHeight / 2));
            break;
        }

        // Actually set the position
        this.actor.x = Math.floor(menuX);
        this.actor.y = Math.floor(menuY);

        // Now show it
        this.actor.opacity = 0;
        this.actor.reactive = true;
        Tweener.addTween(this.actor, { opacity: 255,
                                       transition: "easeOutQuad",
                                       time: POPUP_ANIMATION_TIME });
        this.isOpen = true;
        this.emit('open-state-changed', true);
    },

    close: function() {
        if (!this.isOpen)
            return;

        this.actor.reactive = false;
        Tweener.addTween(this.actor, { opacity: 0,
                                       transition: "easeOutQuad",
                                       time: POPUP_ANIMATION_TIME,
                                       onComplete: Lang.bind(this, function () { this.actor.hide(); })});
        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);
        this.isOpen = false;
        this.emit('open-state-changed', false);
    },


    toggle: function() {
        if (this.isOpen)
            this.close();
        else
            this.open();
    },

    handleKeyPress: function(event) {
        if (event.get_key_symbol() == Clutter.space ||
            event.get_key_symbol() == Clutter.Return) {
            if (this._activeMenuItem)
                this._activeMenuItem.activate(event);
            return true;
        } else if (event.get_key_symbol() == Clutter.Down
                   || event.get_key_symbol() == Clutter.Up) {
            let items = this._box.get_children().filter(function (child) { return child.visible && child.reactive; });
            let current = this._activeMenuItem ? this._activeMenuItem.actor : null;
            let direction = event.get_key_symbol() == Clutter.Down ? 1 : -1;

            let next = findNextInCycle(items, current, direction);
            if (next) {
                next._delegate.setActive(true);
                return true;
            }
        }

        return false;
    }
};
Signals.addSignalMethods(PopupMenu.prototype);

/* Basic implementation of a menu manager.
 * Call addMenu to add menus
 */
function PopupMenuManager(owner) {
    this._init(owner);
}

PopupMenuManager.prototype = {
    _init: function(owner) {
        this._owner = owner;
        this.grabbed = false;

        this._eventCaptureId = 0;
        this._enterEventId = 0;
        this._leaveEventId = 0;
        this._activeMenu = null;
        this._menus = [];
        this._delayedMenus = [];
    },

    addMenu: function(menu, noGrab) {
        this._menus.push(menu);
        menu.connect('open-state-changed', Lang.bind(this, this._onMenuOpenState));
        menu.connect('activate', Lang.bind(this, this._onMenuActivated));

        let source = menu.sourceActor;
        if (source) {
            source.connect('enter-event', Lang.bind(this, this._onMenuSourceEnter, menu));
            if (!noGrab)
                source.connect('button-press-event', Lang.bind(this, this._onMenuSourcePress, menu));
        }
    },

    grab: function() {
        Main.pushModal(this._owner.actor);

        this._eventCaptureId = global.stage.connect('captured-event', Lang.bind(this, this._onEventCapture));
        // captured-event doesn't see enter/leave events
        this._enterEventId = global.stage.connect('enter-event', Lang.bind(this, this._onEventCapture));
        this._leaveEventId = global.stage.connect('leave-event', Lang.bind(this, this._onEventCapture));

        this.grabbed = true;
    },

    ungrab: function() {
        global.stage.disconnect(this._eventCaptureId);
        this._eventCaptureId = 0;
        global.stage.disconnect(this._enterEventId);
        this._enterEventId = 0;
        global.stage.disconnect(this._leaveEventId);
        this._leaveEventId = 0;

        Main.popModal(this._owner.actor);

        this.grabbed = false;
    },

    _onMenuOpenState: function(menu, open) {
        if (!open && menu == this._activeMenu)
            this._activeMenu = null;
        else if (open)
            this._activeMenu = menu;
    },

    _onMenuSourceEnter: function(actor, event, menu) {
        if (!this.grabbed || menu == this._activeMenu)
            return false;

        if (this._activeMenu != null)
            this._activeMenu.close();
        menu.open();
        return false;
    },

    _onMenuSourcePress: function(actor, event, menu) {
        if (this.grabbed)
            return false;
        this.grab();
        return false;
    },

    _onMenuActivated: function(menu, item) {
        if (this.grabbed)
            this.ungrab();
    },

    _eventIsOnActiveMenu: function(event) {
        let src = event.get_source();
        return this._activeMenu != null
                && (this._activeMenu.actor.contains(src) ||
                    (this._activeMenu.sourceActor && this._activeMenu.sourceActor.contains(src)));
    },

    _eventIsOnAnyMenuSource: function(event) {
        let src = event.get_source();
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i];
            if (menu.sourceActor && menu.sourceActor.contains(src))
                return true;
        }
        return false;
    },

    _onEventCapture: function(actor, event) {
        if (!this.grabbed)
            return false;

        if (this._owner.menuEventFilter &&
            this._owner.menuEventFilter(event))
            return true;

        let activeMenuContains = this._eventIsOnActiveMenu(event);
        let eventType = event.type();
        if (eventType == Clutter.EventType.BUTTON_RELEASE) {
            if (activeMenuContains) {
                return false;
            } else {
                this._closeMenu();
                return true;
            }
        } else if ((eventType == Clutter.EventType.BUTTON_PRESS && !activeMenuContains)
                   || (eventType == Clutter.EventType.KEY_PRESS && event.get_key_symbol() == Clutter.Escape)) {
            this._closeMenu();
            return true;
        } else if (eventType == Clutter.EventType.KEY_PRESS
                   && this._activeMenu != null
                   && this._activeMenu.handleKeyPress(event)) {
                return true;
        } else if (eventType == Clutter.EventType.KEY_PRESS
                   && this._activeMenu != null
                   && (event.get_key_symbol() == Clutter.Left
                       || event.get_key_symbol() == Clutter.Right)) {
            let direction = event.get_key_symbol() == Clutter.Right ? 1 : -1;
            let next = findNextInCycle(this._menus, this._activeMenu, direction);
            if (next != this._activeMenu) {
                this._activeMenu.close();
                next.open();
            }
            return true;
        } else if (activeMenuContains || this._eventIsOnAnyMenuSource(event)) {
            return false;
        }

        return true;
    },

    _closeMenu: function() {
        if (this._activeMenu != null)
            this._activeMenu.close();
        this.ungrab();
    }
};
