// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const POPUP_DELAY_TIMEOUT = 150; // milliseconds

const POPUP_SCROLL_TIME = 0.10; // seconds
const POPUP_FADE_OUT_TIME = 0.1; // seconds

const DISABLE_HOVER_TIMEOUT = 500; // milliseconds

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

const SwitcherPopup = new Lang.Class({
    Name: 'SwitcherPopup',
    Abstract: true,

    _init: function(items) {
        this._switcherList = null;

        this._items = items || [];
        this._selectedIndex = 0;

        this.actor = new Shell.GenericContainer({ style_class: 'switcher-popup',
                                                  reactive: true,
                                                  visible: false });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        Main.uiGroup.add_actor(this.actor);

        this._haveModal = false;
        this._modifierMask = 0;

        this._motionTimeoutId = 0;
        this._initialDelayTimeoutId = 0;

        // Initially disable hover so we ignore the enter-event if
        // the switcher appears underneath the current pointer location
        this._disableHover();
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let primary = Main.layoutManager.primaryMonitor;

        alloc.min_size = primary.width;
        alloc.natural_size = primary.width;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let primary = Main.layoutManager.primaryMonitor;

        alloc.min_size = primary.height;
        alloc.natural_size = primary.height;
    },

    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        let primary = Main.layoutManager.primaryMonitor;

        let leftPadding = this.actor.get_theme_node().get_padding(St.Side.LEFT);
        let rightPadding = this.actor.get_theme_node().get_padding(St.Side.RIGHT);
        let bottomPadding = this.actor.get_theme_node().get_padding(St.Side.BOTTOM);
        let vPadding = this.actor.get_theme_node().get_vertical_padding();
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

    _createSwitcher: function() {
        throw new Error('Not implemented');
    },

    _initialSelection: function(backward, binding) {
        throw new Error('Not implemented');
    },

    show: function(backward, binding, mask) {
        if (!this._createSwitcher())
            return false;

        if (!Main.pushModal(this.actor)) {
            // Probably someone else has a pointer grab, try again with keyboard only
            if (!Main.pushModal(this.actor, { options: Meta.ModalOptions.POINTER_ALREADY_GRABBED })) {
                return false;
            }
        }
        this._haveModal = true;
        this._modifierMask = primaryModifier(mask);

        this.actor.connect('key-press-event', Lang.bind(this, this._keyPressEvent));
        this.actor.connect('key-release-event', Lang.bind(this, this._keyReleaseEvent));

        this.actor.connect('button-press-event', Lang.bind(this, this._clickedOutside));
        this.actor.connect('scroll-event', Lang.bind(this, this._scrollEvent));

        this.actor.add_actor(this._switcherList.actor);
        this._switcherList.connect('item-activated', Lang.bind(this, this._itemActivated));
        this._switcherList.connect('item-entered', Lang.bind(this, this._itemEntered));

        // Need to force an allocation so we can figure out whether we
        // need to scroll when selecting
        this.actor.opacity = 0;
        this.actor.show();
        this.actor.get_allocation_box();

        if (this._items.length > 1)
            this._selectedIndex = 1;
        else
            this._selectedIndex = 0;

        this._initialSelection(backward, binding);

        // There's a race condition; if the user released Alt before
        // we got the grab, then we won't be notified. (See
        // https://bugzilla.gnome.org/show_bug.cgi?id=596695 for
        // details.) So we check now. (Have to do this after updating
        // selection.)
        let [x, y, mods] = global.get_pointer();
        if (!(mods & this._modifierMask)) {
            this._finish(global.get_current_time());
            return false;
        }

        // We delay showing the popup so that fast Alt+Tab users aren't
        // disturbed by the popup briefly flashing.
        this._initialDelayTimeoutId = Mainloop.timeout_add(POPUP_DELAY_TIMEOUT,
                                                           Lang.bind(this, function () {
                                                               Main.osdWindow.cancel();
                                                               this.actor.opacity = 255;
                                                               this._initialDelayTimeoutId = 0;
                                                           }));
        return true;
    },

    _next: function() {
        return mod(this._selectedIndex + 1, this._items.length);
    },

    _previous: function() {
        return mod(this._selectedIndex - 1, this._items.length);
    },

    _keyPressHandler: function(keysym, backwards, action) {
        throw new Error('Not implemented');
    },

    _keyPressEvent: function(actor, event) {
        let keysym = event.get_key_symbol();
        let event_state = event.get_state();
        let backwards = event_state & Clutter.ModifierType.SHIFT_MASK;
        let action = global.display.get_keybinding_action(event.get_key_code(), event_state);

        this._disableHover();

        if (keysym == Clutter.Escape)
            this.destroy();
        else
            this._keyPressHandler(keysym, backwards, action);

        return true;
    },

    _keyReleaseEvent: function(actor, event) {
        let [x, y, mods] = global.get_pointer();
        let state = mods & this._modifierMask;

        if (state == 0)
            this._finish(event.get_time());

        return true;
    },

    _clickedOutside: function(actor, event) {
        this.destroy();
    },

    _scrollHandler: function(direction) {
        if (direction == Clutter.ScrollDirection.UP)
            this._select(this._previous());
        else if (direction == Clutter.ScrollDirection.DOWN)
            this._select(this._next());
    },

    _scrollEvent: function(actor, event) {
        this._scrollHandler(event.get_scroll_direction());
    },

    _itemActivatedHandler: function(n) {
        this._select(n);
    },

    _itemActivated: function(switcher, n) {
        this._itemActivatedHandler(n);
        this._finish(global.get_current_time());
    },

    _itemEnteredHandler: function(n) {
        this._select(n);
    },

    _itemEntered: function(switcher, n) {
        if (!this.mouseActive)
            return;
        this._itemEnteredHandler(n);
    },

    _disableHover: function() {
        this.mouseActive = false;

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);

        this._motionTimeoutId = Mainloop.timeout_add(DISABLE_HOVER_TIMEOUT, Lang.bind(this, this._mouseTimedOut));
    },

    _mouseTimedOut: function() {
        this._motionTimeoutId = 0;
        this.mouseActive = true;
    },

    _popModal: function() {
        if (this._haveModal) {
            Main.popModal(this.actor);
            this._haveModal = false;
        }
    },

    destroy: function() {
        this._popModal();
        if (this.actor.visible) {
            Tweener.addTween(this.actor,
                             { opacity: 0,
                               time: POPUP_FADE_OUT_TIME,
                               transition: 'easeOutQuad',
                               onComplete: Lang.bind(this,
                                   function() {
                                       this.actor.destroy();
                                   })
                             });
        } else
            this.actor.destroy();
    },

    _finish: function(timestamp) {
        this.destroy();
    },

    _onDestroy: function() {
        this._popModal();

        if (this._motionTimeoutId != 0)
            Mainloop.source_remove(this._motionTimeoutId);
        if (this._initialDelayTimeoutId != 0)
            Mainloop.source_remove(this._initialDelayTimeoutId);
    },

    _select: function(num) {
        this._selectedIndex = num;
        this._switcherList.highlight(num);
    }
});

const SwitcherList = new Lang.Class({
    Name: 'SwitcherList',

    _init : function(squareItems) {
        this.actor = new Shell.GenericContainer({ style_class: 'switcher-list' });
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocateTop));

        // Here we use a GenericContainer so that we can force all the
        // children to have the same width.
        this._list = new Shell.GenericContainer({ style_class: 'switcher-list-item-container' });
        this._list.spacing = 0;
        this._list.connect('style-changed', Lang.bind(this, function() {
                                                        this._list.spacing = this._list.get_theme_node().get_length('spacing');
                                                     }));

        this._list.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._list.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._list.connect('allocate', Lang.bind(this, this._allocate));

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
        this._leftArrow.connect('repaint', Lang.bind(this,
            function() { drawArrow(this._leftArrow, St.Side.LEFT); }));
        this._rightArrow = new St.DrawingArea({ style_class: 'switcher-arrow',
                                                pseudo_class: 'highlighted' });
        this._rightArrow.connect('repaint', Lang.bind(this,
            function() { drawArrow(this._rightArrow, St.Side.RIGHT); }));

        this.actor.add_actor(this._leftArrow);
        this.actor.add_actor(this._rightArrow);

        this._items = [];
        this._highlighted = -1;
        this._squareItems = squareItems;
        this._minSize = 0;
        this._scrollableRight = true;
        this._scrollableLeft = false;
    },

    _allocateTop: function(actor, box, flags) {
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

    addItem : function(item, label) {
        let bbox = new St.Button({ style_class: 'item-box',
                                   reactive: true });

        bbox.set_child(item);
        this._list.add_actor(bbox);

        let n = this._items.length;
        bbox.connect('clicked', Lang.bind(this, function() { this._onItemClicked(n); }));
        bbox.connect('enter-event', Lang.bind(this, function() { this._onItemEnter(n); }));

        bbox.label_actor = label;

        this._items.push(bbox);

        return bbox;
    },

    _onItemClicked: function (index) {
        this._itemActivated(index);
    },

    _onItemEnter: function (index) {
        this._itemEntered(index);
    },

    highlight: function(index, justOutline) {
        if (this._highlighted != -1) {
            this._items[this._highlighted].remove_style_pseudo_class('outlined');
            this._items[this._highlighted].remove_style_pseudo_class('selected');
        }

        this._highlighted = index;

        if (this._highlighted != -1) {
            if (justOutline)
                this._items[this._highlighted].add_style_pseudo_class('outlined');
            else
                this._items[this._highlighted].add_style_pseudo_class('selected');
        }

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

    _scrollToLeft : function() {
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
                           onComplete: Lang.bind(this, function () {
                                if (this._highlighted == 0) {
                                    this._scrollableLeft = false;
                                    this.actor.queue_relayout();
                                }
                           })
                          });
    },

    _scrollToRight : function() {
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
                           onComplete: Lang.bind(this, function () {
                                if (this._highlighted == this._items.length - 1) {
                                    this._scrollableRight = false;
                                    this.actor.queue_relayout();
                                }
                            })
                          });
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

        let totalSpacing = Math.max(this._list.spacing * (this._items.length - 1), 0);
        alloc.min_size = this._items.length * maxChildMin + totalSpacing;
        alloc.natural_size = alloc.min_size;
        this._minSize = alloc.min_size;
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
            maxChildNat = maxChildMin;
        }

        alloc.min_size = maxChildMin;
        alloc.natural_size = maxChildNat;
    },

    _allocate: function (actor, box, flags) {
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

