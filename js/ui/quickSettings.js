/* exported QuickToggle, QuickSettingsMenu, SystemIndicator */
const {Atk, Clutter, Gio, GLib, GObject, Graphene, Pango, St} = imports.gi;

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const {POPUP_ANIMATION_TIME} = imports.ui.boxpointer;
const DIM_BRIGHTNESS = -0.4;

var QuickToggle = GObject.registerClass({
    Properties: {
        'icon-name': GObject.ParamSpec.override('icon-name', St.Button),
        'gicon': GObject.ParamSpec.object('gicon', '', '',
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'label': GObject.ParamSpec.string('label', '', '',
            GObject.ParamFlags.READWRITE,
            ''),
    },
}, class QuickToggle extends St.Button {
    _init(params) {
        super._init({
            style_class: 'quick-toggle button',
            accessible_role: Atk.Role.TOGGLE_BUTTON,
            can_focus: true,
            ...params,
        });

        this._box = new St.BoxLayout();
        this.set_child(this._box);

        const iconProps = {};
        if (this.gicon)
            iconProps['gicon'] = this.gicon;
        if (this.iconName)
            iconProps['icon-name'] = this.iconName;

        this._icon = new St.Icon({
            style_class: 'quick-toggle-icon',
            x_expand: false,
            ...iconProps,
        });
        this._box.add_child(this._icon);

        // bindings are in the "wrong" direction, so we
        // pick up StIcon's linking of the two properties
        this._icon.bind_property('icon-name',
            this, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE |
            GObject.BindingFlags.BIDIRECTIONAL);
        this._icon.bind_property('gicon',
            this, 'gicon',
            GObject.BindingFlags.SYNC_CREATE |
            GObject.BindingFlags.BIDIRECTIONAL);

        this._label = new St.Label({
            style_class: 'quick-toggle-label',
            y_align: Clutter.ActorAlign.CENTER,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
        });
        this.label_actor = this._label;
        this._box.add_child(this._label);

        this._label.clutter_text.ellipsize = Pango.EllipsizeMode.END;

        this.bind_property('label',
            this._label, 'text',
            GObject.BindingFlags.SYNC_CREATE);
    }
});

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
    _init(overlay, params) {
        super._init(params);

        this._overlay = overlay;
    }

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
            if (child === this._overlay)
                continue;

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

            if (child === this._overlay)
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

        let [minHeight, natHeight] = this._overlay
            ? this._overlay.get_preferred_height(-1)
            : [0, 0];

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

        const [, overlayHeight] = this._overlay
            ? this._overlay.get_preferred_height(-1)
            : [0, 0];

        const availWidth = box.get_width() - (this.nColumns - 1) * this.row_spacing;
        const childWidth = availWidth / this.nColumns;

        this._overlay?.allocate_available_size(0, 0, box.get_width(), box.get_height());

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

            if (row.some(c => c.menu?.actor.visible))
                y += overlayHeight;
        });
    }
});

var QuickSettingsMenu = class extends PopupMenu.PopupMenu {
    constructor(sourceActor, nColumns = 1) {
        super(sourceActor, 0, St.Side.TOP);

        this.actor = new St.Widget({reactive: true});
        this.actor.add_child(this._boxPointer);
        this.actor._delegate = this;

        this.connect('menu-closed', () => this.actor.hide());

        Main.layoutManager.connectObject('system-modal-opened',
            () => this.close(), this);

        this._dimEffect = new Clutter.BrightnessContrastEffect({
            enabled: false,
        });
        this._boxPointer.add_effect_with_name('dim', this._dimEffect);
        this.box.add_style_class_name('quick-settings');

        // Overlay layer for menus
        this._overlay = new Clutter.Actor({
            layout_manager: new Clutter.BinLayout(),
        });

        // "clone"
        const placeholder = new Clutter.Actor({
            constraints: new Clutter.BindConstraint({
                coordinate: Clutter.BindCoordinate.HEIGHT,
                source: this._overlay,
            }),
        });

        this._grid = new St.Widget({
            style_class: 'quick-settings-grid',
            layout_manager: new QuickSettingsLayout(placeholder, {
                nColumns,
            }),
        });
        this.box.add_child(this._grid);
        this._grid.add_child(placeholder);

        const yConstraint = new Clutter.BindConstraint({
            coordinate: Clutter.BindCoordinate.Y,
            source: this._boxPointer,
        });

        // Pick up additional spacing from any intermediate actors
        const updateOffset = () => {
            const offset = this._grid.apply_relative_transform_to_point(
                this._boxPointer, new Graphene.Point3D());
            yConstraint.offset = offset.y;
        };
        this._grid.connect('notify::y', updateOffset);
        this.box.connect('notify::y', updateOffset);
        this._boxPointer.bin.connect('notify::y', updateOffset);

        this._overlay.add_constraint(yConstraint);
        this._overlay.add_constraint(new Clutter.BindConstraint({
            coordinate: Clutter.BindCoordinate.X,
            source: this._boxPointer,
        }));
        this._overlay.add_constraint(new Clutter.BindConstraint({
            coordinate: Clutter.BindCoordinate.WIDTH,
            source: this._boxPointer,
        }));

        this.actor.add_child(this._overlay);
    }

    addItem(item, colSpan = 1) {
        this._grid.add_child(item);
        this._grid.layout_manager.child_set_property(
            this._grid, item, 'column-span', colSpan);

        if (item.menu) {
            this._overlay.add_child(item.menu.actor);

            item.menu.connect('open-state-changed', (m, isOpen) => {
                this._setDimmed(isOpen);
                this._activeMenu = isOpen ? item.menu : null;
            });
        }
    }

    open(animate) {
        this.actor.show();
        super.open(animate);
    }

    close(animate) {
        this._activeMenu?.close(animate);
        super.close(animate);
    }

    _setDimmed(dim) {
        const val = 127 * (1 + (dim ? 1 : 0) * DIM_BRIGHTNESS);
        const color = Clutter.Color.new(val, val, val, 255);

        this._boxPointer.ease_property('@effects.dim.brightness', color, {
            mode: Clutter.AnimationMode.LINEAR,
            duration: POPUP_ANIMATION_TIME,
            onStopped: () => (this._dimEffect.enabled = dim),
        });
        this._dimEffect.enabled = true;
    }
};

var SystemIndicator = GObject.registerClass(
class SystemIndicator extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'panel-status-indicators-box',
            reactive: true,
            visible: false,
        });

        this.quickSettingsItems = [];
    }

    _syncIndicatorsVisible() {
        this.visible = this.get_children().some(a => a.visible);
    }

    _addIndicator() {
        const icon = new St.Icon({style_class: 'system-status-icon'});
        this.add_actor(icon);
        icon.connect('notify::visible', () => this._syncIndicatorsVisible());
        this._syncIndicatorsVisible();
        return icon;
    }
});
