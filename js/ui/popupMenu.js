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

const SLIDER_SCROLL_STEP = 0.05; /* Slider scrolling step in % */

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
        this.actor = new Shell.GenericContainer({ style_class: 'popup-menu-item',
                                                  reactive: params.reactive,
                                                  track_hover: params.reactive,
                                                  can_focus: params.reactive });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));
        this.actor._delegate = this;

        this._children = [];
        this._dot = null;
        this._columnWidths = null;
        this._spacing = 0;
        this.active = false;

        if (params.reactive && params.activate) {
            this.actor.connect('button-release-event', Lang.bind(this, this._onButtonReleaseEvent));
            this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        }
        if (params.reactive && params.hover)
            this.actor.connect('notify::hover', Lang.bind(this, this._onHoverChanged));
        if (params.reactive)
            this.actor.connect('key-focus-in', Lang.bind(this, this._onKeyFocusIn));
    },

    _onStyleChanged: function (actor) {
        this._spacing = actor.get_theme_node().get_length('spacing');
    },

    _onButtonReleaseEvent: function (actor, event) {
        this.emit('activate', event);
        return true;
    },

    _onKeyPressEvent: function (actor, event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.emit('activate', event);
            return true;
        }
        return false;
    },

    _onKeyFocusIn: function (actor) {
        this.setActive(true);
    },

    _onHoverChanged: function (actor) {
        this.setActive(actor.hover);
    },

    activate: function (event) {
        this.emit('activate', event);
    },

    setActive: function (active) {
        let activeChanged = active != this.active;

        if (activeChanged) {
            this.active = active;
            if (active) {
                this.actor.add_style_pseudo_class('active');
                this.actor.grab_key_focus();
            } else
                this.actor.remove_style_pseudo_class('active');
            this.emit('active-changed', active);
        }
    },

    destroy: function() {
        this.actor.destroy();
        this.emit('destroy');
    },

    // true if non descendant content includes @actor
    contains: function(actor) {
        return false;
    },

    // adds an actor to the menu item; @column defaults to the next
    // open column, @span defaults to 1. If @span is -1, the actor
    // will span the width of the menu item. Children are not
    // allowed to overlap each other.
    addActor: function(child, column, span) {
        if (column == null) {
            if (this._children.length) {
                let lastChild = this._children[this._children.length - 1];
                column = lastChild.column + lastChild.span;
            } else
                column = 0;
            span = 1;
        } else if (span == null)
            span = 1;

        this._children.push({ actor: child,
                              column: column,
                              span: span });
        this.actor.connect('destroy', Lang.bind(this, function () { this._removeChild(child); }));
        this.actor.add_actor(child);
    },

    _removeChild: function(child) {
        for (let i = 0; i < this._children.length; i++) {
            if (this._children[i].actor == child) {
                this._children.splice(i, 1);
                return;
            }
        }
    },

    removeActor: function(child) {
        this.actor.remove_actor(child);
        this._removeChild(child);
    },

    setShowDot: function(show) {
        if (show) {
            if (this._dot)
                return;

            this._dot = new St.DrawingArea({ style_class: 'popup-menu-item-dot' });
            this._dot.connect('repaint', Lang.bind(this, this._onRepaintDot));
            this.actor.add_actor(this._dot);
        } else {
            if (!this._dot)
                return;

            this._dot.destroy();
            this._dot = null;
        }
    },

    _onRepaintDot: function(area) {
        let cr = area.get_context();
        let [width, height] = area.get_surface_size();
        let color = new Clutter.Color();
        area.get_theme_node().get_foreground_color(color);

        cr.setSourceRGBA (
            color.red / 255,
            color.green / 255,
            color.blue / 255,
            color.alpha / 255);
        cr.arc(width / 2, height / 2, width / 3, 0, 2 * Math.PI);
        cr.fill();
    },

    getColumnWidths: function() {
        let widths = [];
        for (let i = 0, col = 0; i < this._children.length; i++) {
            let child = this._children[i];
            let [min, natural] = child.actor.get_preferred_width(-1);
            widths[col++] = natural;
            if (child.span > 1) {
                for (let j = 1; j < child.span; j++)
                    widths[col++] = 0;
            }
        }
        return widths;
    },

    setColumnWidths: function(widths) {
        this._columnWidths = widths;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let width = 0;
        if (this._columnWidths) {
            for (let i = 0; i < this._columnWidths.length; i++) {
                if (i > 0)
                    width += this._spacing;
                width += this._columnWidths[i];
            }
        } else {
            for (let i = 0; i < this._children.length; i++) {
                let child = this._children[i];
                if (i > 0)
                    width += this._spacing;
                let [min, natural] = child.actor.get_preferred_width(forHeight);
                width += natural;
            }
        }
        alloc.min_size = alloc.natural_size = width;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let height = 0;
        for (let i = 0; i < this._children.length; i++) {
            let child = this._children[i];
            let [min, natural] = child.actor.get_preferred_height(-1);
            if (natural > height)
                height = natural;
        }
        alloc.min_size = alloc.natural_size = height;
    },

    _allocate: function(actor, box, flags) {
        let height = box.y2 - box.y1;

        if (this._dot) {
            let dotBox = new Clutter.ActorBox();
            let dotWidth = Math.round(box.x1 / 2);

            dotBox.x1 = Math.round(box.x1 / 4);
            dotBox.x2 = dotBox.x1 + dotWidth;
            dotBox.y1 = Math.round(box.y1 + (height - dotWidth) / 2);
            dotBox.y2 = dotBox.y1 + dotWidth;
            this._dot.allocate(dotBox, flags);
        }

        let x = box.x1;
        for (let i = 0, col = 0; i < this._children.length; i++) {
            let child = this._children[i];
            let childBox = new Clutter.ActorBox();
            childBox.x1 = x;
            if (this._columnWidths) {
                if (child.span == -1)
                    childBox.x2 = box.x2;
                else {
                    childBox.x2 = x;
                    for (let j = 0; j < child.span; j++)
                        childBox.x2 += this._columnWidths[col++];
                }
            } else {
                let [min, natural] = child.actor.get_preferred_width(-1);
                childBox.x2 = x + natural;
            }
            let [min, natural] = child.actor.get_preferred_height(-1);
            childBox.y1 = Math.round(box.y1 + (height - natural) / 2);
            childBox.y2 = childBox.y1 + natural;
            child.actor.allocate(childBox, flags);

            x = childBox.x2 + this._spacing;
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
        PopupBaseMenuItem.prototype._init.call(this);

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
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
        this.addActor(this._drawingArea, 0, -1);
        this._drawingArea.connect('repaint', Lang.bind(this, this._onRepaint));
    },

    _onRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();
        let margin = themeNode.get_length('-margin-horizontal');
        let gradientHeight = themeNode.get_length('-gradient-height');
        let startColor = new Clutter.Color();
        themeNode.get_color('-gradient-start', startColor);
        let endColor = new Clutter.Color();
        themeNode.get_color('-gradient-end', endColor);

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

        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));

        if (isNaN(value))
            // Avoid spreading NaNs around
            throw TypeError('The slider value must be a number');
        this._value = Math.max(Math.min(value, 1), 0);

        this._slider = new St.DrawingArea({ style_class: 'popup-slider-menu-item', reactive: true });
        this.addActor(this._slider, 0, -1);
        this._slider.connect('repaint', Lang.bind(this, this._sliderRepaint));
        this._slider.connect('button-press-event', Lang.bind(this, this._startDragging));
        this._slider.connect('scroll-event', Lang.bind(this, this._onScrollEvent));

        this._releaseId = this._motionId = 0;
        this._dragging = false;
    },

    setValue: function(value) {
        if (isNaN(value))
            throw TypeError('The slider value must be a number');

        this._value = Math.max(Math.min(value, 1), 0);
        this._slider.queue_repaint();
    },

    _sliderRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();

        let handleRadius = themeNode.get_length('-slider-handle-radius');

        let sliderWidth = width - 2 * handleRadius;
        let sliderHeight = themeNode.get_length('-slider-height');

        let sliderBorderWidth = themeNode.get_length('-slider-border-width');

        let sliderBorderColor = new Clutter.Color();
        themeNode.get_color('-slider-border-color', sliderBorderColor);
        let sliderColor = new Clutter.Color();
        themeNode.get_color('-slider-background-color', sliderColor);

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
        let handleX = handleRadius + (width - 2 * handleRadius) * this._value;

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

            this.emit('drag-end');
        }
        return true;
    },

    _onScrollEvent: function (actor, event) {
        let direction = event.get_scroll_direction();

        if (direction == Clutter.ScrollDirection.DOWN) {
            this._value = Math.max(0, this._value - SLIDER_SCROLL_STEP);
        }
        else if (direction == Clutter.ScrollDirection.UP) {
            this._value = Math.min(1, this._value + SLIDER_SCROLL_STEP);
        }

        this._slider.queue_repaint();
        this.emit('value-changed', this._value);
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
        let handleRadius = this._slider.get_theme_node().get_length('-slider-handle-radius');

        let newvalue;
        if (relX < handleRadius)
            newvalue = 0;
        else if (relX > width - handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - handleRadius) / (width - 2 * handleRadius);
        this._value = newvalue;
        this._slider.queue_repaint();
        this.emit('value-changed', this._value);
    },

    get value() {
        return this._value;
    },

    _onKeyPressEvent: function (actor, event) {
        let key = event.get_key_symbol();
        if (key == Clutter.KEY_Right || key == Clutter.KEY_Left) {
            let delta = key == Clutter.KEY_Right ? 0.1 : -0.1;
            this._value = Math.max(0, Math.min(this._value + delta, 1));
            this._slider.queue_repaint();
            this.emit('value-changed', this._value);
            this.emit('drag-end');
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

        this.label = new St.Label({ text: text });
        this._switch = new Switch(active);

        this.addActor(this.label);
        this.addActor(this._switch.actor);

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


function PopupImageMenuItem(text, iconName) {
    this._init(text, iconName);
}

PopupImageMenuItem.prototype = {
    __proto__: PopupBaseMenuItem.prototype,

    _init: function (text, iconName) {
        PopupBaseMenuItem.prototype._init.call(this);

        this._size = 16;

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
        this._imageBin = new St.Bin({ width: this._size, height: this._size });
        this.addActor(this._imageBin);

        this.setIcon(iconName);
    },

    setIcon: function(name) {
        if (this._imageBin.child)
            this._imageBin.child.destroy();

        let img = St.TextureCache.get_default().load_icon_name(name, St.IconType.SYMBOLIC, this._size);
        this._imageBin.set_child(img);
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
        this._boxWrapper = new Shell.GenericContainer();
        this._boxWrapper.connect('get-preferred-width', Lang.bind(this, this._boxGetPreferredWidth));
        this._boxWrapper.connect('get-preferred-height', Lang.bind(this, this._boxGetPreferredHeight));
        this._boxWrapper.connect('allocate', Lang.bind(this, this._boxAllocate));
        this._boxPointer.bin.set_child(this._boxWrapper);

        this._box = new St.BoxLayout({ style_class: 'popup-menu-content',
                                       vertical: true });
        this._boxWrapper.add_actor(this._box);
        this.actor.add_style_class_name('popup-menu');

        global.focus_manager.add_group(this.actor);

        if (sourceActor._delegate instanceof PopupSubMenuMenuItem) {
            this._isSubMenu = true;
            this.actor.reactive = true;
            this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        }

        this.isOpen = false;
        this._activeMenuItem = null;
    },

    _boxGetPreferredWidth: function (actor, forHeight, alloc) {
        // Update the menuitem column widths
        let columnWidths = [];
        let items = this._box.get_children();
        for (let i = 0; i < items.length; i++) {
            if (items[i]._delegate instanceof PopupBaseMenuItem) {
                let itemColumnWidths = items[i]._delegate.getColumnWidths();
                for (let j = 0; j < itemColumnWidths.length; j++) {
                    if (j >= columnWidths.length || itemColumnWidths[j] > columnWidths[j])
                        columnWidths[j] = itemColumnWidths[j];
                }
            }
        }
        for (let i = 0; i < items.length; i++) {
            if (items[i]._delegate instanceof PopupBaseMenuItem)
                items[i]._delegate.setColumnWidths(columnWidths);
        }

        // Now they will request the right sizes
        [alloc.min_size, alloc.natural_size] = this._box.get_preferred_width(forHeight);
    },

    _boxGetPreferredHeight: function (actor, forWidth, alloc) {
        [alloc.min_size, alloc.natural_size] = this._box.get_preferred_height(forWidth);
    },

    _boxAllocate: function (actor, box, flags) {
        this._box.allocate(box, flags);
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

    open: function() {
        if (this.isOpen)
            return;

        let primary = global.get_primary_monitor();

        // We need to show it now to force an allocation,
        // so that we can query the correct size.
        this.actor.show();

        // Position correctly relative to the sourceActor
        let [sourceX, sourceY] = this.sourceActor.get_transformed_position();
        let [sourceWidth, sourceHeight] = this.sourceActor.get_transformed_size();

        let [minWidth, minHeight, natWidth, natHeight] = this.actor.get_preferred_size();

        let menuWidth = natWidth, menuHeight = natHeight;

        // Position the non-pointing axis
        if (this._isSubmenu) {
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

        this._boxPointer.setPosition(this.sourceActor, this._gap, this._alignment);

        // Now show it
        this.actor.reactive = true;
        this._boxPointer.animateAppear();
        this.isOpen = true;
        this.emit('open-state-changed', true);
    },

    close: function() {
        if (!this.isOpen)
            return;

        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);
        this.actor.reactive = false;
        this._boxPointer.animateDisappear();
        this.isOpen = false;
        this.emit('open-state-changed', false);
    },

    toggle: function() {
        if (this.isOpen)
            this.close();
        else
            this.open();
    },

    _onKeyPressEvent: function(actor, event) {
        // Move focus back to parent menu if the user types Left.
        // (This handler is only connected if the PopupMenu is a
        // submenu.)
        if (this.isOpen &&
            this._activeMenuItem &&
            event.get_key_symbol() == Clutter.KEY_Left) {
            this._activeMenuItem.setActive(false);
            return true;
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
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
        this.addActor(new St.Label({ text: '>' }));

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
                this.menu.open();
            else
                this.menu.close();
        }

        PopupBaseMenuItem.prototype.setActive.call(this, active);
    },

    _onKeyPressEvent: function(actor, event) {
        if (!this.menu)
            return false;
        if (event.get_key_symbol() == Clutter.KEY_Right) {
            this.menu.activateFirst();
            return true;
        }
        return false;
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
        this._keyPressEventId = 0;
        this._enterEventId = 0;
        this._leaveEventId = 0;
        this._activeMenu = null;
        this._menus = [];
        this._delayedMenus = [];
    },

    addMenu: function(menu, position) {
        let menudata = {
            menu:              menu,
            openStateChangeId: menu.connect('open-state-changed', Lang.bind(this, this._onMenuOpenState)),
            activateId:        menu.connect('activate', Lang.bind(this, this._onMenuActivated)),
            destroyId:         menu.connect('destroy', Lang.bind(this, this._onMenuDestroy)),
            enterId:           0,
            focusId:           0
        };

        let source = menu.sourceActor;
        if (source) {
            menudata.enterId = source.connect('enter-event', Lang.bind(this, function() { this._onMenuSourceEnter(menu); }));
            menudata.focusId = source.connect('key-focus-in', Lang.bind(this, function() { this._onMenuSourceEnter(menu); }));
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
        if (menudata.focusId)
            menu.sourceActor.disconnect(menudata.focusId);

        this._menus.splice(position, 1);
    },

    grab: function() {
        Main.pushModal(this._owner.actor);

        this._eventCaptureId = global.stage.connect('captured-event', Lang.bind(this, this._onEventCapture));
        this._keyPressEventId = global.stage.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        // captured-event doesn't see enter/leave events
        this._enterEventId = global.stage.connect('enter-event', Lang.bind(this, this._onEventCapture));
        this._leaveEventId = global.stage.connect('leave-event', Lang.bind(this, this._onEventCapture));

        this.grabbed = true;
    },

    ungrab: function() {
        global.stage.disconnect(this._eventCaptureId);
        this._eventCaptureId = 0;
        global.stage.disconnect(this._keyPressEventId);
        this._keyPressEventId = 0;
        global.stage.disconnect(this._enterEventId);
        this._enterEventId = 0;
        global.stage.disconnect(this._leaveEventId);
        this._leaveEventId = 0;

        this.grabbed = false;
        Main.popModal(this._owner.actor);
    },

    _onMenuOpenState: function(menu, open) {
        if (open) {
            this._activeMenu = menu;
            if (!this.grabbed)
                this.grab();
        } else if (menu == this._activeMenu) {
            this._activeMenu = null;
            if (this.grabbed)
                this.ungrab();
        }
    },

    _onMenuSourceEnter: function(menu) {
        if (!this.grabbed || menu == this._activeMenu)
            return false;

        if (this._activeMenu != null)
            this._activeMenu.close();
        menu.open();
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
        } else if (activeMenuContains || this._eventIsOnAnyMenuSource(event)) {
            return false;
        }

        return true;
    },

    _onKeyPressEvent: function(actor, event) {
        if (!this.grabbed || !this._activeMenu)
            return false;
        if (!this._eventIsOnActiveMenu(event))
            return false;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.Left || symbol == Clutter.Right) {
            let direction = symbol == Clutter.Right ? 1 : -1;
            let pos = this._findMenu(this._activeMenu);
            let next = this._menus[mod(pos + direction, this._menus.length)].menu;
            if (next != this._activeMenu) {
                let oldMenu = this._activeMenu;
                this._activeMenu = next;
                oldMenu.close();
                next.open();
                next.activateFirst();
            }
            return true;
        }

        return false;
    },

    _closeMenu: function() {
        if (this._activeMenu != null)
            this._activeMenu.close();
    }
};
