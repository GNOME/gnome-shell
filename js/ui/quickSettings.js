/* exported QuickSettingsMenu */
const {Clutter, GLib, GObject, St} = imports.gi;

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const QuickSettingsLayoutMeta = GObject.registerClass({
    Properties: {
        'column-span': GObject.ParamSpec.int(
            'column-span', '', '',
            GObject.ParamFlags.READWRITE,
            1, GLib.MAXINT32, 1),
    },
}, class QuickSettingsLayoutMeta extends Clutter.LayoutMeta {});

const QuickSettingsLayout = GObject.registerClass({
    Properties: {
        'row-spacing': GObject.ParamSpec.int(
            'row-spacing', 'row-spacing', 'row-spacing',
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'column-spacing': GObject.ParamSpec.int(
            'column-spacing', 'column-spacing', 'column-spacing',
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'n-columns': GObject.ParamSpec.int(
            'n-columns', 'n-columns', 'n-columns',
            GObject.ParamFlags.READWRITE,
            1, GLib.MAXINT32, 1),
    },
}, class QuickSettingsLayout extends Clutter.LayoutManager {
    _containerStyleChanged() {
        const node = this._container.get_theme_node();

        let changed = false;
        let found, length;
        [found, length] = node.lookup_length('spacing-rows', false);
        changed ||= found;
        if (found)
            this.rowSpacing = length;

        [found, length] = node.lookup_length('spacing-columns', false);
        changed ||= found;
        if (found)
            this.columnSpacing = length;

        if (changed)
            this.layout_changed();
    }

    _getColSpan(container, child) {
        const {columnSpan} = this.get_child_meta(container, child);
        return Math.clamp(columnSpan, 1, this.nColumns);
    }

    _getMaxChildWidth(container) {
        let [minWidth, natWidth] = [0, 0];

        for (const child of container) {
            const [childMin, childNat] = child.get_preferred_width(-1);
            const colSpan = this._getColSpan(container, child);
            minWidth = Math.max(minWidth, childMin / colSpan);
            natWidth = Math.max(natWidth, childNat / colSpan);
        }

        return [minWidth, natWidth];
    }

    _getRows(container) {
        const rows = [];
        let lineIndex = 0;
        let curRow;

        /** private */
        function appendRow() {
            curRow = [];
            rows.push(curRow);
            lineIndex = 0;
        }

        for (const child of container) {
            if (!child.visible)
                continue;

            if (lineIndex === 0)
                appendRow();

            const colSpan = this._getColSpan(container, child);
            const fitsRow = lineIndex + colSpan <= this.nColumns;

            if (!fitsRow)
                appendRow();

            curRow.push(child);
            lineIndex = (lineIndex + colSpan) % this.nColumns;
        }

        return rows;
    }

    _getRowHeight(children) {
        let [minHeight, natHeight] = [0, 0];

        children.forEach(child => {
            const [childMin, childNat] = child.get_preferred_height(-1);
            minHeight = Math.max(minHeight, childMin);
            natHeight = Math.max(natHeight, childNat);
        });

        return [minHeight, natHeight];
    }

    vfunc_get_child_meta_type() {
        return QuickSettingsLayoutMeta.$gtype;
    }

    vfunc_set_container(container) {
        this._container?.disconnectObject(this);

        this._container = container;

        this._container?.connectObject('style-changed',
            () => this._containerStyleChanged(), this);
    }

    vfunc_get_preferred_width(container, _forHeight) {
        const [childMin, childNat] = this._getMaxChildWidth(container);
        const spacing = (this.nColumns - 1) * this.column_spacing;
        return [this.nColumns * childMin + spacing, this.nColumns * childNat + spacing];
    }

    vfunc_get_preferred_height(container, _forWidth) {
        const rows = this._getRows(container);

        let [minHeight, natHeight] = [0, 0];

        const spacing = (rows.length - 1) * this.row_spacing;
        minHeight += spacing;
        natHeight += spacing;

        rows.forEach(row => {
            const [rowMin, rowNat] = this._getRowHeight(row);
            minHeight += rowMin;
            natHeight += rowNat;
        });

        return [minHeight, natHeight];
    }

    vfunc_allocate(container, box) {
        const rows = this._getRows(container);

        const availWidth = box.get_width() - (this.nColumns - 1) * this.row_spacing;
        const childWidth = availWidth / this.nColumns;

        const isRtl = container.text_direction === Clutter.TextDirection.RTL;

        const childBox = new Clutter.ActorBox();
        let y = box.y1;
        rows.forEach(row => {
            const [, rowNat] = this._getRowHeight(row);

            let lineIndex = 0;
            row.forEach(child => {
                const colSpan = this._getColSpan(container, child);
                const width =
                    childWidth * colSpan + this.column_spacing * (colSpan - 1);
                let x = box.x1 + lineIndex * (childWidth + this.column_spacing);
                if (isRtl)
                    x = box.x2 - width - x;

                childBox.set_origin(x, y);
                childBox.set_size(width, rowNat);
                child.allocate(childBox);

                lineIndex = (lineIndex + colSpan) % this.nColumns;
            });

            y += rowNat + this.row_spacing;
        });
    }
});

var QuickSettingsMenu = class extends PopupMenu.PopupMenu {
    constructor(sourceActor, nColumns = 1) {
        super(sourceActor, 0, St.Side.TOP);

        Main.layoutManager.connectObject('system-modal-opened',
            () => this.close(), this);

        this.box.add_style_class_name('quick-settings');

        this._grid = new St.Widget({
            style_class: 'quick-settings-grid',
            layout_manager: new QuickSettingsLayout({
                nColumns,
            }),
        });
        this.box.add_child(this._grid);
    }

    addItem(item, colSpan = 1) {
        this._grid.add_child(item);
        this._grid.layout_manager.child_set_property(
            this._grid, item, 'column-span', colSpan);
    }
};
