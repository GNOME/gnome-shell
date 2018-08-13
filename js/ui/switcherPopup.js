// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

var POPUP_DELAY_TIMEOUT = 150; // milliseconds

var POPUP_SCROLL_TIME = 0.10; // seconds
var POPUP_FADE_OUT_TIME = 0.1; // seconds

var DISABLE_HOVER_TIMEOUT = 500; // milliseconds
var NO_MODS_TIMEOUT = 1500; // milliseconds

function mod(a, b) {
    return (a + b) % b;
}

function primaryModifier(mask) {
    if (mask == 0)
        return 0;

    let primary = 1;
    while (mask > 1) {
        mask >>= 1;
        primary <<= 1;
    }
    return primary;
}

var SwitcherPopup = new Lang.Class({
    Name: 'SwitcherPopup',
    Extends: St.Widget,
    Abstract: true,

    _init(items) {
        this.parent({ style_class: 'switcher-popup',
                      reactive: true,
                      visible: false });

        this._switcherList = null;

        this._items = items || [];
        this._selectedIndex = 0;

        this.connect('destroy', this._onDestroy.bind(this));

        Main.uiGroup.add_actor(this);

        this._haveModal = false;
        this._modifierMask = 0;

        this._motionTimeoutId = 0;
        this._initialDelayTimeoutId = 0;
        this._noModsTimeoutId = 0;

        // Initially disable hover so we ignore the enter-event if
        // the switcher appears underneath the current pointer location
        this._disableHover();
    },

    vfunc_get_preferred_width(forHeight) {
        let primary = Main.layoutManager.primaryMonitor;
        return [primary.width, primary.width];
    },

    vfunc_get_preferred_height(forWidth) {
        let primary = Main.layoutManager.primaryMonitor;
        return [primary.height, primary.height];
    },

    vfunc_allocate(box, flags) {
        this.set_allocation(box, flags);

        let childBox = new Clutter.ActorBox();
        let primary = Main.layoutManager.primaryMonitor;

        let leftPadding = this.get_theme_node().get_padding(St.Side.LEFT);
        let rightPadding = this.get_theme_node().get_padding(St.Side.RIGHT);
        let hPadding = leftPadding + rightPadding;

        // Allocate the switcherList
        // We select a size based on an icon size that does not overflow the screen
        let [childMinHeight, childNaturalHeight] = this._switcherList.actor.get_preferred_height(primary.width - hPadding);
        let [childMinWidth, childNaturalWidth] = this._switcherList.actor.get_preferred_width(childNaturalHeight);
        childBox.x1 = Math.max(primary.x + leftPadding, primary.x + Math.floor((primary.width - childNaturalWidth) / 2));
        childBox.x2 = Math.min(primary.x + primary.width - rightPadding, childBox.x1 + childNaturalWidth);
        childBox.y1 = primary.y + Math.floor((primary.height - childNaturalHeight) / 2);
        childBox.y2 = childBox.y1 + childNaturalHeight;
        this._switcherList.actor.allocate(childBox, flags);
    },

    _initialSelection(backward, binding) {
        if (backward)
            this._select(this._items.length - 1);
        else if (this._items.length == 1)
            this._select(0);
        else
            this._select(1);
    },

    show(backward, binding, mask) {
        if (this._items.length == 0)
            return false;

        if (!Main.pushModal(this)) {
            // Probably someone else has a pointer grab, try again with keyboard only
            if (!Main.pushModal(this, { options: Meta.ModalOptions.POINTER_ALREADY_GRABBED }))
                return false;
        }
        this._haveModal = true;
        this._modifierMask = primaryModifier(mask);

        this.connect('key-press-event', this._keyPressEvent.bind(this));
        this.connect('key-release-event', this._keyReleaseEvent.bind(this));

        this.connect('button-press-event', this._clickedOutside.bind(this));
        this.connect('scroll-event', this._scrollEvent.bind(this));

        this.add_actor(this._switcherList.actor);
        this._switcherList.connect('item-activated', this._itemActivated.bind(this));
        this._switcherList.connect('item-entered', this._itemEntered.bind(this));
        this._switcherList.connect('item-removed', this._itemRemoved.bind(this));

        // Need to force an allocation so we can figure out whether we
        // need to scroll when selecting
        this.opacity = 0;
        this.visible = true;
        this.get_allocation_box();

        this._initialSelection(backward, binding);

        // There's a race condition; if the user released Alt before
        // we got the grab, then we won't be notified. (See
        // https://bugzilla.gnome.org/show_bug.cgi?id=596695 for
        // details.) So we check now. (Have to do this after updating
        // selection.)
        if (this._modifierMask) {
            let [x, y, mods] = global.get_pointer();
            if (!(mods & this._modifierMask)) {
                this._finish(global.get_current_time());
                return false;
            }
        } else {
            this._resetNoModsTimeout();
        }

        // We delay showing the popup so that fast Alt+Tab users aren't
        // disturbed by the popup briefly flashing.
        this._initialDelayTimeoutId = Mainloop.timeout_add(POPUP_DELAY_TIMEOUT,
                                                           () => {
                                                               Main.osdWindowManager.hideAll();
                                                               this.opacity = 255;
                                                               this._initialDelayTimeoutId = 0;
                                                               return GLib.SOURCE_REMOVE;
                                                           });
        GLib.Source.set_name_by_id(this._initialDelayTimeoutId, '[gnome-shell] Main.osdWindow.cancel');
        return true;
    },

    _next() {
        return mod(this._selectedIndex + 1, this._items.length);
    },

    _previous() {
        return mod(this._selectedIndex - 1, this._items.length);
    },

    _keyPressHandler(keysym, action) {
        throw new Error('Not implemented');
    },

    _keyPressEvent(actor, event) {
        let keysym = event.get_key_symbol();
        let action = global.display.get_keybinding_action(event.get_key_code(), event.get_state());

        this._disableHover();

        if (this._keyPressHandler(keysym, action) != Clutter.EVENT_PROPAGATE)
            return Clutter.EVENT_STOP;

        // Note: pressing one of the below keys will destroy the popup only if
        // that key is not used by the active popup's keyboard shortcut
        if (keysym == Clutter.Escape || keysym == Clutter.Tab)
            this.fadeAndDestroy();

        return Clutter.EVENT_STOP;
    },

    _keyReleaseEvent(actor, event) {
        if (this._modifierMask) {
            let [x, y, mods] = global.get_pointer();
            let state = mods & this._modifierMask;

            if (state == 0)
                this._finish(event.get_time());
        } else {
            this._resetNoModsTimeout();
        }

        return Clutter.EVENT_STOP;
    },

    _clickedOutside(actor, event) {
        this.fadeAndDestroy();
        return Clutter.EVENT_PROPAGATE;
    },

    _scrollHandler(direction) {
        if (direction == Clutter.ScrollDirection.UP)
            this._select(this._previous());
        else if (direction == Clutter.ScrollDirection.DOWN)
            this._select(this._next());
    },

    _scrollEvent(actor, event) {
        this._scrollHandler(event.get_scroll_direction());
        return Clutter.EVENT_PROPAGATE;
    },

    _itemActivatedHandler(n) {
        this._select(n);
    },

    _itemActivated(switcher, n) {
        this._itemActivatedHandler(n);
        this._finish(global.get_current_time());
    },

    _itemEnteredHandler(n) {
        this._select(n);
    },

    _itemEntered(switcher, n) {
        if (!this.mouseActive)
            return;
        this._itemEnteredHandler(n);
    },

    _itemRemovedHandler(n) {
        if (this._items.length > 0) {
            let newIndex = Math.min(n, this._items.length - 1);
            this._select(newIndex);
        } else {
            this.fadeAndDestroy();
        }
    },

    _itemRemoved(switcher, n) {
        this._itemRemovedHandler(n);
    },

    _disableHover() {
        this.mouseActive = false;

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);

        this._motionTimeoutId = Mainloop.timeout_add(DISABLE_HOVER_TIMEOUT, this._mouseTimedOut.bind(this));
        GLib.Source.set_name_by_id(this._motionTimeoutId, '[gnome-shell] this._mouseTimedOut');
    },

    _mouseTimedOut() {
        this._motionTimeoutId = 0;
        this.mouseActive = true;
        return GLib.SOURCE_REMOVE;
    },

    _resetNoModsTimeout() {
        if (this._noModsTimeoutId != 0)
            Mainloop.source_remove(this._noModsTimeoutId);

        this._noModsTimeoutId = Mainloop.timeout_add(NO_MODS_TIMEOUT,
                                                     () => {
                                                         this._finish(global.get_current_time());
                                                         this._noModsTimeoutId = 0;
                                                         return GLib.SOURCE_REMOVE;
                                                     });
    },

    _popModal() {
        if (this._haveModal) {
            Main.popModal(this);
            this._haveModal = false;
        }
    },

    fadeAndDestroy() {
        this._popModal();
        if (this.visible) {
            Tweener.addTween(this,
                             { opacity: 0,
                               time: POPUP_FADE_OUT_TIME,
                               transition: 'easeOutQuad',
                               onComplete: () => {
                                   this.destroy();
                               }
                             });
        } else {
            this.destroy();
        }
    },

    _finish(timestamp) {
        this.fadeAndDestroy();
    },

    _onDestroy() {
        this._popModal();

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);
        if (this._initialDelayTimeoutId != 0)
            Mainloop.source_remove(this._initialDelayTimeoutId);
        if (this._noModsTimeoutId != 0)
            Mainloop.source_remove(this._noModsTimeoutId);
    },

    _select(num) {
        this._selectedIndex = num;
        this._switcherList.highlight(num);
    }
});

var SwitcherList = new Lang.Class({
    Name: 'SwitcherList',

    _init(squareItems) {
        this.actor = new Shell.GenericContainer({ style_class: 'switcher-list' });
        this.actor.connect('get-preferred-width', this._getPreferredWidth.bind(this));
        this.actor.connect('get-preferred-height', this._getPreferredHeight.bind(this));
        this.actor.connect('allocate', this._allocateTop.bind(this));

        // Here we use a GenericContainer so that we can force all the
        // children to have the same width.
        this._list = new Shell.GenericContainer({ style_class: 'switcher-list-item-container' });
        this._list.spacing = 0;
        this._list.connect('style-changed', () => {
            this._list.spacing = this._list.get_theme_node().get_length('spacing');
        });

        this._list.connect('get-preferred-width', this._getPreferredWidth.bind(this));
        this._list.connect('get-preferred-height', this._getPreferredHeight.bind(this));
        this._list.connect('allocate', this._allocate.bind(this));

        this._scrollView = new St.ScrollView({ style_class: 'hfade',
                                               enable_mouse_scrolling: false });
        this._scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.NEVER);

        let scrollBox = new St.BoxLayout();
        scrollBox.add_actor(this._list);
        this._scrollView.add_actor(scrollBox);
        this.actor.add_actor(this._scrollView);

        // Those arrows indicate whether scrolling in one direction is possible
        this._leftArrow = new St.DrawingArea({ style_class: 'switcher-arrow',
                                               pseudo_class: 'highlighted' });
        this._leftArrow.connect('repaint', () => {
            drawArrow(this._leftArrow, St.Side.LEFT);
        });
        this._rightArrow = new St.DrawingArea({ style_class: 'switcher-arrow',
                                                pseudo_class: 'highlighted' });
        this._rightArrow.connect('repaint', () => {
            drawArrow(this._rightArrow, St.Side.RIGHT);
        });

        this.actor.add_actor(this._leftArrow);
        this.actor.add_actor(this._rightArrow);

        this._items = [];
        this._highlighted = -1;
        this._squareItems = squareItems;
        this._minSize = 0;
        this._scrollableRight = true;
        this._scrollableLeft = false;
    },

    _allocateTop(actor, box, flags) {
        let leftPadding = this.actor.get_theme_node().get_padding(St.Side.LEFT);
        let rightPadding = this.actor.get_theme_node().get_padding(St.Side.RIGHT);

        let childBox = new Clutter.ActorBox();
        let scrollable = this._minSize > box.x2 - box.x1;

        box.y1 -= this.actor.get_theme_node().get_padding(St.Side.TOP);
        box.y2 += this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
        this._scrollView.allocate(box, flags);

        let arrowWidth = Math.floor(leftPadding / 3);
        let arrowHeight = arrowWidth * 2;
        childBox.x1 = leftPadding / 2;
        childBox.y1 = this.actor.height / 2 - arrowWidth;
        childBox.x2 = childBox.x1 + arrowWidth;
        childBox.y2 = childBox.y1 + arrowHeight;
        this._leftArrow.allocate(childBox, flags);
        this._leftArrow.opacity = (this._scrollableLeft && scrollable) ? 255 : 0;

        arrowWidth = Math.floor(rightPadding / 3);
        arrowHeight = arrowWidth * 2;
        childBox.x1 = this.actor.width - arrowWidth - rightPadding / 2;
        childBox.y1 = this.actor.height / 2 - arrowWidth;
        childBox.x2 = childBox.x1 + arrowWidth;
        childBox.y2 = childBox.y1 + arrowHeight;
        this._rightArrow.allocate(childBox, flags);
        this._rightArrow.opacity = (this._scrollableRight && scrollable) ? 255 : 0;
    },

    addItem(item, label) {
        let bbox = new St.Button({ style_class: 'item-box',
                                   reactive: true });

        bbox.set_child(item);
        this._list.add_actor(bbox);

        let n = this._items.length;
        bbox.connect('clicked', () => { this._onItemClicked(n); });
        bbox.connect('motion-event', () => this._onItemEnter(n));

        bbox.label_actor = label;

        this._items.push(bbox);

        return bbox;
    },

    removeItem(index) {
        let item = this._items.splice(index, 1);
        item[0].destroy();
        this.emit('item-removed', index);
    },

    _onItemClicked(index) {
        this._itemActivated(index);
    },

    _onItemEnter(index) {
        // Avoid reentrancy
        if (index != this._currentItemEntered) {
            this._currentItemEntered = index;
            this._itemEntered(index);
        }
        return Clutter.EVENT_PROPAGATE;
    },

    highlight(index, justOutline) {
        if (this._items[this._highlighted]) {
            this._items[this._highlighted].remove_style_pseudo_class('outlined');
            this._items[this._highlighted].remove_style_pseudo_class('selected');
        }

        if (this._items[index]) {
            if (justOutline)
                this._items[index].add_style_pseudo_class('outlined');
            else
                this._items[index].add_style_pseudo_class('selected');
        }

        this._highlighted = index;

        let adjustment = this._scrollView.hscroll.adjustment;
        let [value, lower, upper, stepIncrement, pageIncrement, pageSize] = adjustment.get_values();
        let [absItemX, absItemY] = this._items[index].get_transformed_position();
        let [result, posX, posY] = this.actor.transform_stage_point(absItemX, 0);
        let [containerWidth, containerHeight] = this.actor.get_transformed_size();
        if (posX + this._items[index].get_width() > containerWidth)
            this._scrollToRight();
        else if (this._items[index].allocation.x1 - value < 0)
            this._scrollToLeft();

    },

    _scrollToLeft() {
        let adjustment = this._scrollView.hscroll.adjustment;
        let [value, lower, upper, stepIncrement, pageIncrement, pageSize] = adjustment.get_values();

        let item = this._items[this._highlighted];

        if (item.allocation.x1 < value)
            value = Math.min(0, item.allocation.x1);
        else if (item.allocation.x2 > value + pageSize)
            value = Math.max(upper, item.allocation.x2 - pageSize);

        this._scrollableRight = true;
        Tweener.addTween(adjustment,
                         { value: value,
                           time: POPUP_SCROLL_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                                if (this._highlighted == 0)
                                    this._scrollableLeft = false;
                                this.actor.queue_relayout();
                           }
                          });
    },

    _scrollToRight() {
        let adjustment = this._scrollView.hscroll.adjustment;
        let [value, lower, upper, stepIncrement, pageIncrement, pageSize] = adjustment.get_values();

        let item = this._items[this._highlighted];

        if (item.allocation.x1 < value)
            value = Math.max(0, item.allocation.x1);
        else if (item.allocation.x2 > value + pageSize)
            value = Math.min(upper, item.allocation.x2 - pageSize);

        this._scrollableLeft = true;
        Tweener.addTween(adjustment,
                         { value: value,
                           time: POPUP_SCROLL_TIME,
                           transition: 'easeOutQuad',
                           onComplete: () => {
                                if (this._highlighted == this._items.length - 1)
                                    this._scrollableRight = false;
                                this.actor.queue_relayout();
                            }
                          });
    },

    _itemActivated(n) {
        this.emit('item-activated', n);
    },

    _itemEntered(n) {
        this.emit('item-entered', n);
    },

    _maxChildWidth(forHeight) {
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

    _getPreferredWidth(actor, forHeight, alloc) {
        let [maxChildMin, maxChildNat] = this._maxChildWidth(forHeight);

        let totalSpacing = Math.max(this._list.spacing * (this._items.length - 1), 0);
        alloc.min_size = this._items.length * maxChildMin + totalSpacing;
        alloc.natural_size = alloc.min_size;
        this._minSize = alloc.min_size;
    },

    _getPreferredHeight(actor, forWidth, alloc) {
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
            maxChildNat = maxChildMin;
        }

        alloc.min_size = maxChildMin;
        alloc.natural_size = maxChildNat;
    },

    _allocate(actor, box, flags) {
        let childHeight = box.y2 - box.y1;

        let [maxChildMin, maxChildNat] = this._maxChildWidth(childHeight);
        let totalSpacing = Math.max(this._list.spacing * (this._items.length - 1), 0);

        let childWidth = Math.floor(Math.max(0, box.x2 - box.x1 - totalSpacing) / this._items.length);

        let x = 0;
        let children = this._list.get_children();
        let childBox = new Clutter.ActorBox();

        let primary = Main.layoutManager.primaryMonitor;
        let parentRightPadding = this.actor.get_parent().get_theme_node().get_padding(St.Side.RIGHT);

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
            } else {
                // Something else, eg, AppSwitcher's arrows;
                // we don't allocate it.
            }
        }
    }
});
Signals.addSignalMethods(SwitcherList.prototype);

function drawArrow(area, side) {
    let themeNode = area.get_theme_node();
    let borderColor = themeNode.get_border_color(side);
    let bodyColor = themeNode.get_foreground_color();

    let [width, height] = area.get_surface_size ();
    let cr = area.get_context();

    cr.setLineWidth(1.0);
    Clutter.cairo_set_source_color(cr, borderColor);

    switch (side) {
    case St.Side.TOP:
        cr.moveTo(0, height);
        cr.lineTo(Math.floor(width * 0.5), 0);
        cr.lineTo(width, height);
        break;

    case St.Side.BOTTOM:
        cr.moveTo(width, 0);
        cr.lineTo(Math.floor(width * 0.5), height);
        cr.lineTo(0, 0);
        break;

    case St.Side.LEFT:
        cr.moveTo(width, height);
        cr.lineTo(0, Math.floor(height * 0.5));
        cr.lineTo(width, 0);
        break;

    case St.Side.RIGHT:
        cr.moveTo(0, 0);
        cr.lineTo(width, Math.floor(height * 0.5));
        cr.lineTo(0, height);
        break;
    }

    cr.strokePreserve();

    Clutter.cairo_set_source_color(cr, bodyColor);
    cr.fill();
    cr.$dispose();
}

