/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Lang = imports.lang;
const Params = imports.misc.params;

const ICON_SIZE = 48;


function BaseIcon(label, createIcon) {
    this._init(label, createIcon);
}

BaseIcon.prototype = {
    _init : function(label, params) {
        params = Params.parse(params, { createIcon: null,
                                        setSizeManually: false });
        this.actor = new St.Bin({ style_class: 'overview-icon',
                                  x_fill: true,
                                  y_fill: true });
        this.actor._delegate = this;
        this.actor.connect('style-changed',
                           Lang.bind(this, this._onStyleChanged));

        this._spacing = 0;

        let box = new Shell.GenericContainer();
        box.connect('allocate', Lang.bind(this, this._allocate));
        box.connect('get-preferred-width',
                    Lang.bind(this, this._getPreferredWidth));
        box.connect('get-preferred-height',
                    Lang.bind(this, this._getPreferredHeight));
        this.actor.set_child(box);

        this.iconSize = ICON_SIZE;
        this._iconBin = new St.Bin();

        box.add_actor(this._iconBin);

        this._name = new St.Label({ text: label });
        box.add_actor(this._name);

        if (params.createIcon)
            this.createIcon = params.createIcon;
        this._setSizeManually = params.setSizeManually;

        this.icon = this.createIcon(this.iconSize);
        this._iconBin.set_child(this.icon);
    },

    _allocate: function(actor, box, flags) {
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [labelMinHeight, labelNatHeight] = this._name.get_preferred_height(-1);
        let [iconMinHeight, iconNatHeight] = this._iconBin.get_preferred_height(-1);
        let preferredHeight = labelNatHeight + this._spacing + iconNatHeight;
        let labelHeight = availHeight >= preferredHeight ? labelNatHeight
                                                         : labelMinHeight;
        let iconSize = availHeight - this._spacing - labelHeight;
        let iconPadding = (availWidth - iconSize) / 2;

        let childBox = new Clutter.ActorBox();

        childBox.x1 = iconPadding;
        childBox.y1 = 0;
        childBox.x2 = availWidth - iconPadding;
        childBox.y2 = iconSize;
        this._iconBin.allocate(childBox, flags);

        childBox.x1 = 0;
        childBox.x2 = availWidth;
        childBox.y1 = iconSize + this._spacing;
        childBox.y2 = childBox.y1 + labelHeight;
        this._name.allocate(childBox, flags);
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        this._getPreferredHeight(actor, -1, alloc);
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [iconMinHeight, iconNatHeight] = this._iconBin.get_preferred_height(forWidth);
        let [labelMinHeight, labelNatHeight] = this._name.get_preferred_height(forWidth);
        alloc.min_size = iconMinHeight + this._spacing + labelMinHeight;
        alloc.natural_size = iconNatHeight + this._spacing + labelNatHeight;
    },

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon: function(size) {
        throw new Error('no implementation of createIcon in ' + this);
    },

    setIconSize: function(size) {
        if (!this._setSizeManually)
            throw new Error('setSizeManually has to be set to use setIconsize');

        this._setIconSize(size);
    },

    _setIconSize: function(size) {
        if (size == this.iconSize)
            return;

        this.icon.destroy();
        this.iconSize = size;
        this.icon = this.createIcon(this.iconSize);
        this._iconBin.child = this.icon;
    },

    _onStyleChanged: function() {
        let success, len;
        let node = this.actor.get_theme_node();

        [success, len] = node.get_length('spacing', false);
        if (success)
            this._spacing = spacing;

        if (this._setSizeManually)
            return;

        [success, len] = node.get_length('icon-size', false);
        if (success)
            this._setIconSize(len);
    }
};

function IconGrid(params) {
    this._init(params);
}

IconGrid.prototype = {
    _init: function(params) {
        params = Params.parse(params, { rowLimit: null, columnLimit: null });
        this._rowLimit = params.rowLimit;
        this._colLimit = params.columnLimit;

        this.actor = new St.BoxLayout({ style_class: 'icon-grid',
                                        vertical: true });
        // Pulled from CSS, but hardcode some defaults here
        this._spacing = 0;
        this._item_size = ICON_SIZE;
        this._grid = new Shell.GenericContainer();
        this.actor.add(this._grid, { expand: true, y_align: St.Align.START });
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));

        this._grid.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._grid.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._grid.connect('allocate', Lang.bind(this, this._allocate));
    },

    _getPreferredWidth: function (grid, forHeight, alloc) {
        let children = this._grid.get_children();
        let nColumns = this._colLimit ? Math.min(this._colLimit,
                                                 children.length)
                                      : children.length;
        let totalSpacing = Math.max(0, nColumns - 1) * this._spacing;
        // Kind of a lie, but not really an issue right now.  If
        // we wanted to support some sort of hidden/overflow that would
        // need higher level design
        alloc.min_size = this._item_size;
        alloc.natural_size = nColumns * this._item_size + totalSpacing;
    },

    _getPreferredHeight: function (grid, forWidth, alloc) {
        let children = this._grid.get_children();
        let [nColumns, usedWidth] = this._computeLayout(forWidth);
        let nRows;
        if (nColumns > 0)
            nRows = Math.ceil(children.length / nColumns);
        else
            nRows = 0;
        if (this._rowLimit)
            nRows = Math.min(nRows, this._rowLimit);
        let totalSpacing = Math.max(0, nRows - 1) * this._spacing;
        let height = nRows * this._item_size + totalSpacing;
        alloc.min_size = height;
        alloc.natural_size = height;
    },

    _allocate: function (grid, box, flags) {
        let children = this._grid.get_children();
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let [nColumns, usedWidth] = this._computeLayout(availWidth);

        let overallPaddingX = Math.floor((availWidth - usedWidth) / 2);

        let x = box.x1 + overallPaddingX;
        let y = box.y1;
        let columnIndex = 0;
        let rowIndex = 0;
        for (let i = 0; i < children.length; i++) {
            let [childMinWidth, childMinHeight, childNaturalWidth, childNaturalHeight]
                = children[i].get_preferred_size();

            /* Center the item in its allocation horizontally */
            let width = Math.min(this._item_size, childNaturalWidth);
            let childXSpacing = Math.max(0, width - childNaturalWidth) / 2;
            let height = Math.min(this._item_size, childNaturalHeight);
            let childYSpacing = Math.max(0, height - childNaturalHeight) / 2;

            let childBox = new Clutter.ActorBox();
            if (St.Widget.get_default_direction() == St.TextDirection.RTL) {
                let _x = box.x2 - (x + width);
                childBox.x1 = Math.floor(_x - childXSpacing);
            } else {
                childBox.x1 = Math.floor(x + childXSpacing);
            }
            childBox.y1 = Math.floor(y + childYSpacing);
            childBox.x2 = childBox.x1 + width;
            childBox.y2 = childBox.y1 + height;

            if (this._rowLimit && rowIndex >= this._rowLimit) {
                this._grid.set_skip_paint(children[i], true);
            } else {
                children[i].allocate(childBox, flags);
                this._grid.set_skip_paint(children[i], false);
            }

            columnIndex++;
            if (columnIndex == nColumns) {
                columnIndex = 0;
                rowIndex++;
            }

            if (columnIndex == 0) {
                y += this._item_size + this._spacing;
                x = box.x1 + overallPaddingX;
            } else {
                x += this._item_size + this._spacing;
            }
        }
    },

    _computeLayout: function (forWidth) {
        let children = this._grid.get_children();
        let nColumns = 0;
        let usedWidth = 0;
        while ((this._colLimit == null || nColumns < this._colLimit) &&
               (usedWidth + this._item_size <= forWidth)) {
            usedWidth += this._item_size + this._spacing;
            nColumns += 1;
        }

        if (nColumns > 0)
            usedWidth -= this._spacing;

        return [nColumns, usedWidth];
    },

    _onStyleChanged: function() {
        let themeNode = this.actor.get_theme_node();
        let [success, len] = themeNode.get_length('spacing', false);
        if (success)
            this._spacing = len;
        [success, len] = themeNode.get_length('-shell-grid-item-size', false);
        if (success)
            this._item_size = len;
        this._grid.queue_relayout();
    },

    removeAll: function () {
        this._grid.get_children().forEach(Lang.bind(this, function (child) {
            child.destroy();
        }));
    },

    addItem: function(actor) {
        this._grid.add_actor(actor);
    },

    getItemAtIndex: function(index) {
        return this._grid.get_children()[index];
    },

    visibleItemsCount: function() {
        return this._grid.get_children().length - this._grid.get_n_skip_paint();
    }
};
