/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const POPUP_ANIMATION_TIME = 0.1;

function Switch() {
    this._init.apply(this, arguments);
}

Switch.prototype = {
    _init: function(state) {
        this.actor = new St.Bin({ style_class: 'toggle-switch' });
        // Translators: this MUST be either "toggle-switch-us"
        // (for toggle switches containing the English words
        // "ON" and "OFF") or "toggle-switch-intl" (for toggle
        // switches containing "◯" and "|"). Other values will
        // simply result in invisible toggle switches.
        this.actor.add_style_class_name(_("toggle-switch-us"));
        this.setToggleState(state);
    },

    setToggleState: function(state) {
        if (state)
            this.actor.add_style_pseudo_class('checked');
        else
            this.actor.remove_style_pseudo_class('checked');
        this.state = state;
    },

    toggle: function() {
        this.setToggleState(!this.state);
    }
};

function PopupBaseMenuItem(params) {
    this._init(params);
}

PopupBaseMenuItem.prototype = {
    _init: function (params) {
        params = Params.parse (params, { reactive: true,
                                         activate: true,
                                         hover: true });
        this.actor = new St.Bin({ style_class: 'popup-menu-item',
                                  reactive: params.reactive,
                                  track_hover: params.reactive,
                                  x_fill: true,
                                  y_fill: true,
                                  x_align: St.Align.START });
        this.actor._delegate = this;
        this.active = false;

        if (params.reactive && params.activate) {
            this.actor.connect('button-release-event', Lang.bind(this, function (actor, event) {
                this.emit('activate', event);
            }));
        }
        if (params.reactive && params.hover)
            this.actor.connect('notify::hover', Lang.bind(this, this._hoverChanged));
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
    },

    destroy: function() {
        this.actor.destroy();
        this.emit('destroy');
    },

    handleKeyPress: function(event) {
        return false;
    },

    // true if non descendant content includes @actor
    contains: function(actor) {
        return false;
    }
};
Signals.addSignalMethods(PopupBaseMenuItem.prototype);

function PopupMenuItem(text) {
    this._init(text);
}

PopupMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function (text) {
        PopupBaseMenuItem.prototype._init.call(this);

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
        PopupBaseMenuItem.prototype._init.call(this, { reactive: false });

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

function PopupSliderMenuItem() {
    this._init.apply(this, arguments);
}

PopupSliderMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function(value) {
        PopupBaseMenuItem.prototype._init.call(this, { activate: false });

        if (isNaN(value))
            // Avoid spreading NaNs around
            throw TypeError('The slider value must be a number');
        this._displayValue = this._value = Math.max(Math.min(value, 1), 0);

        this._slider = new St.DrawingArea({ style_class: 'popup-slider-menu-item', reactive: true });
        this.actor.set_child(this._slider);
        this._slider.connect('repaint', Lang.bind(this, this._sliderRepaint));
        this._slider.connect('button-press-event', Lang.bind(this, this._startDragging));

        this._releaseId = this._motionId = 0;
        this._dragging = false;
    },

    setValue: function(value) {
        if (isNaN(value))
            throw TypeError('The slider value must be a number');

        this._displayValue = this._value = Math.max(Math.min(value, 1), 0);
        this._slider.queue_repaint();
    },

    _sliderRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();

        let found, handleRadius;
        [found, handleRadius] = themeNode.get_length('-slider-handle-radius', false);

        let sliderWidth = width - 2 * handleRadius;
        let sliderHeight;
        [found, sliderHeight] = themeNode.get_length('-slider-height', false);

        let sliderBorderWidth;
        [found, sliderBorderWidth] = themeNode.get_length('-slider-border-width', false);

        let sliderBorderColor = new Clutter.Color();
        themeNode.get_color('-slider-border-color', false, sliderBorderColor);
        let sliderColor = new Clutter.Color();
        themeNode.get_color('-slider-background-color', false, sliderColor);

        cr.setSourceRGBA (
            sliderColor.red / 255,
            sliderColor.green / 255,
            sliderColor.blue / 255,
            sliderColor.alpha / 255);
        cr.rectangle(handleRadius, (height - sliderHeight) / 2, sliderWidth, sliderHeight);
        cr.fillPreserve();
        cr.setSourceRGBA (
            sliderBorderColor.red / 255,
            sliderBorderColor.green / 255,
            sliderBorderColor.blue / 255,
            sliderBorderColor.alpha / 255);
        cr.setLineWidth(sliderBorderWidth);
        cr.stroke();

        let handleY = height / 2;
        let handleX = handleRadius + (width - 2 * handleRadius) * this._displayValue;

        let color = new Clutter.Color();
        themeNode.get_foreground_color(color);
        cr.setSourceRGBA (
            color.red / 255,
            color.green / 255,
            color.blue / 255,
            color.alpha / 255);
        cr.arc(handleX, handleY, handleRadius, 0, 2 * Math.PI);
        cr.fill();
    },

    _startDragging: function(actor, event) {
        if (this._dragging) // don't allow two drags at the same time
            return;

        this._dragging = true;

        // FIXME: we should only grab the specific device that originated
        // the event, but for some weird reason events are still delivered
        // outside the slider if using clutter_grab_pointer_for_device
        Clutter.grab_pointer(this._slider);
        this._releaseId = this._slider.connect('button-release-event', Lang.bind(this, this._endDragging));
        this._motionId = this._slider.connect('motion-event', Lang.bind(this, this._motionEvent));
        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY);
    },

    _endDragging: function() {
        if (this._dragging) {
            this._slider.disconnect(this._releaseId);
            this._slider.disconnect(this._motionId);

            Clutter.ungrab_pointer();
            this._dragging = false;

            this._value = this._displayValue;
            this.emit('value-changed', this._value);
        }
        return true;
    },

    _motionEvent: function(actor, event) {
        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY)
        return true;
    },

    _moveHandle: function(absX, absY) {
        let relX, relY, sliderX, sliderY;
        [sliderX, sliderY] = this._slider.get_transformed_position();
        relX = absX - sliderX;
        relY = absY - sliderY;

        let width = this._slider.width;
        let found, handleRadius;
        [found, handleRadius] = this._slider.get_theme_node().get_length('-slider-handle-radius', false);

        let newvalue;
        if (relX < handleRadius)
            newvalue = 0;
        else if (relX > width - handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - handleRadius) / (width - 2 * handleRadius);
        this._displayValue = newvalue;
        this._slider.queue_repaint();
    },

    get value() {
        return this._value;
    },

    handleKeyPress: function(event) {
        let key = event.get_key_symbol();
        if (key == Clutter.Right || key == Clutter.Left) {
            let delta = key == Clutter.Right ? 0.1 : -0.1;
            this._value = this._displayValue = Math.max(0, Math.min(this._value + delta, 1));
            this._slider.queue_repaint();
            this.emit('value-changed', this._value);
            return true;
        }
        return false;
    }
}

function PopupSwitchMenuItem() {
    this._init.apply(this, arguments);
}

PopupSwitchMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function(text, active) {
        PopupBaseMenuItem.prototype._init.call(this);

        this.active = !!active;
        this.label = new St.Label({ text: text });
        this._switch = new Switch(this.active);

        this._box = new St.BoxLayout({ style_class: 'popup-switch-menu-item' });
        this._box.add(this.label, { expand: true, y_fill: false });
        this._box.add(this._switch.actor, { y_fill: false });
        this.actor.set_child(this._box);

        this.connect('activate', Lang.bind(this,function(from) {
            this.toggle();
        }));
    },

    toggle: function() {
        this._switch.toggle();
        this.emit('toggled', this._switch.state);
    },

    get state() {
        return this._switch.state;
    },

    setToggleState: function(state) {
        this._switch.setToggleState(state);
    }
}


function PopupImageMenuItem(text, iconName, alwaysShowImage) {
    this._init(text, iconName, alwaysShowImage);
}

// We need to instantiate a GtkImageMenuItem so it
// hooks up its properties on the GtkSettings
var _gtkImageMenuItemCreated = false;

PopupImageMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function (text, iconName, alwaysShowImage) {
        PopupBaseMenuItem.prototype._init.call(this);

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
        this.label = new St.Label({ text: text });
        box.add(this.label, { expand: true });

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
            let img = St.TextureCache.get_default().load_icon_name(this._iconName, St.IconType.SYMBOLIC, this._size);
            this._imageBin.set_child(img);
            this._imageBin.show();
        }
    },
    
    setIcon: function(name) {
        this._iconName = name;
        this._onMenuImagesChanged();
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

function PopupMenu() {
    this._init.apply(this, arguments);
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
        menuItem.connect('destroy', Lang.bind(this, function(emitter) {
            menuItem.disconnect(menuItem._activateId);
            menuItem.disconnect(menuItem._activeChangeId);
            if (menuItem == this._activeMenuItem)
                this._activeMenuItem = null;
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
            item.destroy();
        }
    },

    setArrowOrigin: function(origin) {
        this._boxPointer.setArrowOrigin(origin);
    },

    activateFirst: function() {
        let children = this._box.get_children();
        for (let i = 0; i < children.length; i++) {
            let actor = children[i];
            if (actor._delegate && actor.visible && actor.reactive) {
                actor._delegate.setActive(true);
                break;
            }
        }
    },

    open: function(submenu) {
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
        if (submenu) {
            if (this._arrowSide == St.Side.TOP || this._arrowSide == St.Side.BOTTOM) {
                // vertical submenu
                if (sourceY + sourceHeigth + menuHeight + this._gap < primary.y + primary.height)
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.TOP;
                else if (primary.y + menuHeight + this._gap < sourceY)
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.BOTTOM;
                else
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.TOP;
            } else {
                // horizontal submenu
                if (sourceX + sourceWidth + menuWidth + this._gap < primary.x + primary.width)
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.LEFT;
                else if (primary.x + menuWidth + this._gap < sourceX)
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.RIGHT;
                else
                    this._boxPointer._arrowSide = this._arrowSide = St.Side.LEFT;
            }
        }
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

        this.emit('closing');

        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);
        this.actor.reactive = false;
        Tweener.addTween(this.actor, { opacity: 0,
                                       transition: "easeOutQuad",
                                       time: POPUP_ANIMATION_TIME,
                                       onComplete: Lang.bind(this, function () { this.actor.hide(); })});

        this.isOpen = false;
        this.emit('open-state-changed', false);
    },


    toggle: function() {
        if (this.isOpen)
            this.close();
        else
            this.open();
    },

    handleKeyPress: function(event, submenu) {
        if (!this.isOpen || (submenu && !this._activeMenuItem))
            return false;
        if (this._activeMenuItem && this._activeMenuItem.handleKeyPress(event))
            return true;
        switch (event.get_key_symbol()) {
        case Clutter.space:
        case Clutter.Return:
            if (this._activeMenuItem)
                this._activeMenuItem.activate(event);
            return true;
        case Clutter.Down:
        case Clutter.Up:
            let items = this._box.get_children().filter(function (child) { return child.visible && child.reactive; });
            let current = this._activeMenuItem ? this._activeMenuItem.actor : null;
            let direction = event.get_key_symbol() == Clutter.Down ? 1 : -1;

            let next = findNextInCycle(items, current, direction);
            if (next) {
                next._delegate.setActive(true);
                return true;
            }
            break;
        case Clutter.Left:
            if (submenu) {
                this._activeMenuItem.setActive(false);
                return true;
            }
            break;
        }

        return false;
    },

    // return true if the actor is inside the menu or
    // any actor related to the active submenu
    contains: function(actor) {
        if (this.actor.contains(actor))
            return true;
        if (this._activeMenuItem)
            return this._activeMenuItem.contains(actor);
        return false;
    },

    destroy: function() {
        this.removeAll();
        this.actor.destroy();

        this.emit('destroy');
    }
};
Signals.addSignalMethods(PopupMenu.prototype);

function PopupSubMenuMenuItem() {
    this._init.apply(this, arguments);
}

PopupSubMenuMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function(text) {
        PopupBaseMenuItem.prototype._init.call(this, { activate: false, hover: false });
        this.actor.connect('enter-event', Lang.bind(this, this._mouseEnter));

        this.label = new St.Label({ text: text });
        this._container = new St.BoxLayout();
        this._container.add(this.label, { fill: true, expand: true });
        this._container.add(new St.Label({ text: '>' }));
        this.actor.set_child(this._container);

        this.menu = new PopupMenu(this.actor, St.Align.MIDDLE, St.Side.LEFT, 0, true);
        Main.chrome.addActor(this.menu.actor, { visibleInOverview: true,
                                                affectsStruts: false });
        this.menu.actor.hide();

        this._openStateChangedId = this.menu.connect('open-state-changed', Lang.bind(this, this._subMenuOpenStateChanged));
        this._activateId = this.menu.connect('activate', Lang.bind(this, this._subMenuActivate));
    },

    _subMenuOpenStateChanged: function(menu, open) {
        PopupBaseMenuItem.prototype.setActive.call(this, open);
    },

    _subMenuActivate: function(menu, menuItem) {
        this.emit('activate', null);
    },

    setMenu: function(newmenu) {
        if (this.menu) {
            this.menu.close();
            this.menu.disconnect(this._openStateChangedId);
            this.menu.disconnect(this._activateId);
        }
        if (newmenu) {
            this._openStateChangedId = newmenu.connect('open-state-changed', Lang.bind(this, this._subMenuOpenStateChanged));
            this._activateId = newmenu.connect('activate', Lang.bind(this, this._subMenuActivate));
        }
        this.menu = newmenu;
    },

    destroy: function() {
        if (this.menu)
            this.menu.destroy();
        PopupBaseMenuItem.prototype.destroy.call(this);
    },

    setActive: function(active) {
        if (this.menu) {
            if (active)
                this.menu.open(true);
            else
                this.menu.close();
        }

        PopupBaseMenuItem.prototype.setActive.call(this, active);
    },

    handleKeyPress: function(event) {
        if (!this.menu)
            return false;
        if (event.get_key_symbol() == Clutter.Right) {
            this.menu.activateFirst();
            return true;
        }
        return this.menu.handleKeyPress(event, true);
    },

    contains: function(actor) {
        return this.menu && this.menu.contains(actor);
    },

    _mouseEnter: function(event) {
        this.setActive(true);
    }
};


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

    addMenu: function(menu, noGrab, position) {
        let menudata = {
            menu:              menu,
            openStateChangeId: menu.connect('open-state-changed', Lang.bind(this, this._onMenuOpenState)),
            activateId:        menu.connect('activate', Lang.bind(this, this._onMenuActivated)),
            destroyId:         menu.connect('destroy', Lang.bind(this, this._onMenuDestroy)),
            enterId:           0,
            buttonPressId:     0
        };

        let source = menu.sourceActor;
        if (source) {
            menudata.enterId = source.connect('enter-event', Lang.bind(this, this._onMenuSourceEnter, menu));
            if (!noGrab)
                menudata.buttonPressId = source.connect('button-press-event', Lang.bind(this, this._onMenuSourcePress, menu));
        }

        if (position == undefined)
            this._menus.push(menudata);
        else
            this._menus.splice(position, 0, menudata);
    },

    removeMenu: function(menu) {
        if (menu == this._activeMenu)
            this._closeMenu();

        let position = this._findMenu(menu);
        if (position == -1) // not a menu we manage
            return;

        let menudata = this._menus[position];
        menu.disconnect(menudata.openStateChangeId);
        menu.disconnect(menudata.activateId);
        menu.disconnect(menudata.destroyId);

        if (menudata.enterId)
            menu.sourceActor.disconnect(menudata.enterId);
        if (menudata.buttonPressId)
            menu.sourceActor.disconnect(menudata.buttonPressId);

        this._menus.splice(position, 1);
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

    _onMenuDestroy: function(menu) {
        this.removeMenu(menu);
    },

    _eventIsOnActiveMenu: function(event) {
        let src = event.get_source();
        return this._activeMenu != null
                && (this._activeMenu.contains(src) ||
                    (this._activeMenu.sourceActor && this._activeMenu.sourceActor.contains(src)));
    },

    _eventIsOnAnyMenuSource: function(event) {
        let src = event.get_source();
        for (let i = 0; i < this._menus.length; i++) {
            let menu = this._menus[i].menu;
            if (menu.sourceActor && menu.sourceActor.contains(src))
                return true;
        }
        return false;
    },

    _findMenu: function(item) {
        for (let i = 0; i < this._menus.length; i++) {
            let menudata = this._menus[i];
            if (item == menudata.menu)
                return i;
        }
        return -1;
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
                   && this._activeMenu.handleKeyPress(event, false)) {
                return true;
        } else if (eventType == Clutter.EventType.KEY_PRESS
                   && this._activeMenu != null
                   && (event.get_key_symbol() == Clutter.Left
                       || event.get_key_symbol() == Clutter.Right)) {
            let direction = event.get_key_symbol() == Clutter.Right ? 1 : -1;
            let pos = this._findMenu(this._activeMenu);
            let next = this._menus[mod(pos + direction, this._menus.length)].menu;
            if (next != this._activeMenu) {
                this._activeMenu.close();
                next.open(false);
                next.activateFirst();
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
