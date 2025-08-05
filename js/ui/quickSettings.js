import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import Pango from 'gi://Pango';
import St from 'gi://St';

import * as Main from './main.js';
import * as PopupMenu from './popupMenu.js';
import {Slider} from './slider.js';

import {PopupAnimation} from './boxpointer.js';

const DIM_BRIGHTNESS = -0.4;
const POPUP_ANIMATION_TIME = 400;

export const QuickSettingsItem = GObject.registerClass({
    Properties: {
        'has-menu': GObject.ParamSpec.boolean(
            'has-menu', null, null,
            GObject.ParamFlags.READWRITE |
            GObject.ParamFlags.CONSTRUCT_ONLY,
            false),
    },
}, class QuickSettingsItem extends St.Button {
    _init(params) {
        super._init(params);

        if (this.hasMenu) {
            this.menu = new QuickToggleMenu(this);
            this.menu.actor.hide();

            this._menuManager = new PopupMenu.PopupMenuManager(this);
            this._menuManager.addMenu(this.menu);
        }
    }
});

export const QuickToggle = GObject.registerClass({
    Properties: {
        'title': GObject.ParamSpec.string('title', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'subtitle': GObject.ParamSpec.string('subtitle', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'icon-name': GObject.ParamSpec.override('icon-name', St.Button),
        'gicon': GObject.ParamSpec.object('gicon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
    },
}, class QuickToggle extends QuickSettingsItem {
    _init(params) {
        super._init({
            style_class: 'quick-toggle button',
            accessible_role: Atk.Role.TOGGLE_BUTTON,
            can_focus: true,
            ...params,
        });

        this._box = new St.BoxLayout({x_expand: true});
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

        this._title = new St.Label({
            style_class: 'quick-toggle-title',
            y_align: Clutter.ActorAlign.CENTER,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
        });
        this.label_actor = this._title;

        this._subtitle = new St.Label({
            style_class: 'quick-toggle-subtitle',
            y_align: Clutter.ActorAlign.CENTER,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
        });
        this.get_accessible().add_relationship(Atk.RelationType.DESCRIBED_BY, this._subtitle.get_accessible());

        const titleBox = new St.BoxLayout({
            y_align: Clutter.ActorAlign.CENTER,
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
            orientation: Clutter.Orientation.VERTICAL,
        });
        titleBox.add_child(this._title);
        titleBox.add_child(this._subtitle);
        this._box.add_child(titleBox);

        this._title.clutter_text.ellipsize = Pango.EllipsizeMode.END;

        this.bind_property('title',
            this._title, 'text',
            GObject.BindingFlags.SYNC_CREATE);

        this.bind_property('subtitle',
            this._subtitle, 'text',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property_full('subtitle',
            this._subtitle, 'visible',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source !== null],
            null);
    }

    get label() {
        console.warn('Trying to get label from QuickToggle. Use title instead.');
        return this.title;
    }

    set label(label) {
        console.warn('Trying to set label on QuickToggle. Use title instead.');
        this.title = label;
    }
});

export const QuickMenuToggle = GObject.registerClass({
    Properties: {
        'title': GObject.ParamSpec.string('title', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'subtitle': GObject.ParamSpec.string('subtitle', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'icon-name': GObject.ParamSpec.override('icon-name', St.Button),
        'gicon': GObject.ParamSpec.object('gicon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'menu-enabled': GObject.ParamSpec.boolean(
            'menu-enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'menu-button-accessible-name': GObject.ParamSpec.string(
            'menu-button-accessible-name', null, null,
            GObject.ParamFlags.READWRITE,
            _('Open menu')),
    },
}, class QuickMenuToggle extends QuickSettingsItem {
    _init(params) {
        super._init({
            ...params,
            hasMenu: true,
        });

        this.add_style_class_name('quick-toggle-has-menu');

        this._box = new St.BoxLayout({x_expand: true});
        this.set_child(this._box);

        const contents = new QuickToggle({
            x_expand: true,
        });
        this._box.add_child(contents);

        let separator = new St.Widget({style_class: 'quick-toggle-separator'});
        this._box.add_child(separator);

        this._menuButton = new St.Button({
            style_class: 'quick-toggle-menu-button icon-button',
            child: new St.Icon({icon_name: 'go-next-symbolic'}),
            can_focus: true,
            x_expand: false,
            y_expand: true,
        });
        this._box.add_child(this._menuButton);

        this._menuButton.bind_property('visible',
            separator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this.bind_property('toggle-mode',
            contents, 'toggle-mode',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('checked',
            contents, 'checked',
            GObject.BindingFlags.SYNC_CREATE |
            GObject.BindingFlags.BIDIRECTIONAL);
        this.bind_property('title',
            contents, 'title',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('subtitle',
            contents, 'subtitle',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('gicon',
            contents, 'gicon',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('icon-name',
            contents, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE);

        this.bind_property('menu-enabled',
            this._menuButton, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('menu-button-accessible-name',
            this._menuButton, 'accessible-name',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('reactive',
            this._menuButton, 'reactive',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('checked',
            this._menuButton, 'checked',
            GObject.BindingFlags.SYNC_CREATE);
        contents.connect('clicked', (o, button) => this.emit('clicked', button));
        this._menuButton.connect('clicked', () => this.menu.open());
        this._menuButton.connect('popup-menu', () => this.emit('popup-menu'));
        contents.connect('popup-menu', () => this.emit('popup-menu'));
        this.connect('popup-menu', () => {
            if (this.menuEnabled)
                this.menu.open();
        });
    }
});

export const QuickSlider = GObject.registerClass({
    Properties: {
        'icon-name': GObject.ParamSpec.override('icon-name', St.Button),
        'gicon': GObject.ParamSpec.object('gicon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'icon-reactive': GObject.ParamSpec.boolean(
            'icon-reactive', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'icon-label': GObject.ParamSpec.string(
            'icon-label', null, null,
            GObject.ParamFlags.READWRITE,
            ''),
        'menu-enabled': GObject.ParamSpec.boolean(
            'menu-enabled', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'menu-button-accessible-name': GObject.ParamSpec.string(
            'menu-button-accessible-name', null, null,
            GObject.ParamFlags.READWRITE,
            _('Open menu')),
    },
    Signals: {
        'icon-clicked': {},
    },
}, class QuickSlider extends QuickSettingsItem {
    _init(params) {
        super._init({
            style_class: 'quick-slider',
            ...params,
            can_focus: false,
            reactive: false,
            hasMenu: true,
        });

        const box = new St.BoxLayout({x_expand: true});
        this.set_child(box);

        const iconProps = {};
        if (this.gicon)
            iconProps['gicon'] = this.gicon;
        if (this.iconName)
            iconProps['icon-name'] = this.iconName;

        this._icon = new St.Icon({
            ...iconProps,
        });
        this._iconButton = new St.Button({
            child: this._icon,
            style_class: 'icon-button flat',
            can_focus: true,
            x_expand: false,
            y_expand: true,
        });
        this._iconButton.connect('clicked',
            () => this.emit('icon-clicked'));
        // Show as regular icon when non-interactive
        this._iconButton.connect('notify::reactive',
            () => this._iconButton.remove_style_pseudo_class('insensitive'));
        box.add_child(this._iconButton);

        this.bind_property('icon-reactive',
            this._iconButton, 'reactive',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('icon-reactive',
            this._iconButton, 'can-focus',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('icon-label',
            this._iconButton, 'accessible-name',
            GObject.BindingFlags.SYNC_CREATE);

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

        this.slider = new Slider(0);

        // for focus indication
        const sliderBin = new St.Bin({
            style_class: 'slider-bin',
            child: this.slider,
            reactive: true,
            can_focus: true,
            x_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(sliderBin);

        // Make the slider bin transparent for a11y
        const sliderAccessible = this.slider.get_accessible();
        sliderAccessible.set_parent(sliderBin.get_parent().get_accessible());
        sliderBin.set_accessible(sliderAccessible);
        sliderBin.connect('event', (bin, event) => this.slider.event(event, false));

        this._menuButton = new St.Button({
            child: new St.Icon({icon_name: 'go-next-symbolic'}),
            style_class: 'icon-button flat',
            can_focus: true,
            x_expand: false,
            y_expand: true,
        });
        box.add_child(this._menuButton);

        this.bind_property('menu-button-accessible-name',
            this._menuButton, 'accessible-name',
            GObject.BindingFlags.SYNC_CREATE);
        this.bind_property('menu-enabled',
            this._menuButton, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._menuButton.connect('clicked', () => this.menu.open());
        this.slider.connect('popup-menu', () => {
            if (this.menuEnabled)
                this.menu.open();
        });
    }
});

class QuickToggleMenu extends PopupMenu.PopupMenuBase {
    constructor(sourceActor) {
        super(sourceActor, 'quick-toggle-menu');

        const constraints = new Clutter.BindConstraint({
            coordinate: Clutter.BindCoordinate.Y,
            source: sourceActor,
        });
        sourceActor.bind_property('height',
            constraints, 'offset',
            GObject.BindingFlags.DEFAULT);

        this.actor = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            style_class: 'quick-toggle-menu-container',
            reactive: true,
            x_expand: true,
            y_expand: false,
            height: 0,
            constraints,
        });
        this.actor._delegate = this;
        this.actor.add_child(this.box);

        global.focus_manager.add_group(this.actor);

        const headerLayout = new Clutter.GridLayout();
        this._header = new St.Widget({
            style_class: 'header',
            layout_manager: headerLayout,
            visible: false,
        });
        headerLayout.hookup_style(this._header);
        this.box.add_child(this._header);

        this._headerIcon = new St.Icon({
            style_class: 'icon',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._headerTitle = new St.Label({
            style_class: 'title',
            y_align: Clutter.ActorAlign.CENTER,
            y_expand: true,
        });
        this._headerSubtitle = new St.Label({
            style_class: 'subtitle',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._headerSpacer = new Clutter.Actor({x_expand: true});

        const side = this.actor.text_direction === Clutter.TextDirection.RTL
            ? Clutter.GridPosition.LEFT
            : Clutter.GridPosition.RIGHT;

        headerLayout.attach(this._headerIcon, 0, 0, 1, 2);
        headerLayout.attach_next_to(this._headerTitle,
            this._headerIcon, side, 1, 1);
        headerLayout.attach_next_to(this._headerSpacer,
            this._headerTitle, side, 1, 1);
        headerLayout.attach_next_to(this._headerSubtitle,
            this._headerTitle, Clutter.GridPosition.BOTTOM, 1, 1);

        sourceActor.connect('notify::checked',
            () => this._syncChecked());
        this._syncChecked();
    }

    setHeader(icon, title, subtitle = '') {
        if (icon instanceof Gio.Icon)
            this._headerIcon.gicon = icon;
        else
            this._headerIcon.icon_name = icon;

        this._headerTitle.text = title;
        this._headerSubtitle.set({
            text: subtitle,
            visible: !!subtitle,
        });

        this._header.show();
    }

    addHeaderSuffix(actor) {
        const {layoutManager: headerLayout} = this._header;
        const side = this.actor.text_direction === Clutter.TextDirection.RTL
            ? Clutter.GridPosition.LEFT
            : Clutter.GridPosition.RIGHT;
        this._header.remove_child(this._headerSpacer);
        headerLayout.attach_next_to(actor, this._headerTitle, side, 1, 1);
        headerLayout.attach_next_to(this._headerSpacer, actor, side, 1, 1);
    }

    open(animate) {
        if (this.isOpen)
            return;

        this.actor.show();
        this.isOpen = true;

        const previousHeight = this.actor.height;
        this.actor.height = -1;
        const [targetHeight] = this.actor.get_preferred_height(-1);
        this.actor.height = previousHeight;
        const distance = Math.abs(targetHeight - previousHeight);

        const duration = animate !== PopupAnimation.NONE
            ? POPUP_ANIMATION_TIME / 2
            : 0;

        this.box.opacity = 0;
        this.actor.ease({
            duration: duration * (distance / targetHeight),
            height: targetHeight,
            onComplete: () => {
                this.box.ease({
                    duration,
                    opacity: 255,
                });
                this.actor.height = -1;
            },
        });
        this.emit('open-state-changed', true);
    }

    close(animate) {
        if (!this.isOpen)
            return;

        const {opacity} = this.box;
        const duration = animate !== PopupAnimation.NONE
            ? POPUP_ANIMATION_TIME / 2
            : 0;

        this.box.ease({
            duration: duration * (opacity / 255),
            opacity: 0,
            onComplete: () => {
                this.actor.ease({
                    duration,
                    height: 0,
                    onComplete: () => {
                        this.actor.hide();
                        this.emit('menu-closed');
                    },
                });
            },
        });

        this.isOpen = false;
        this.emit('open-state-changed', false);
    }

    _syncChecked() {
        if (this.sourceActor.checked)
            this._headerIcon.add_style_class_name('active');
        else
            this._headerIcon.remove_style_class_name('active');
    }

    // expected on toplevel menus
    _setOpenedSubMenu(submenu) {
        this._openedSubMenu?.close(true);
        this._openedSubMenu = submenu;
    }
}

const QuickSettingsLayoutMeta = GObject.registerClass({
    Properties: {
        'column-span': GObject.ParamSpec.int(
            'column-span', null, null,
            GObject.ParamFlags.READWRITE,
            1, GLib.MAXINT32, 1),
    },
}, class QuickSettingsLayoutMeta extends Clutter.LayoutMeta {});

const QuickSettingsLayout = GObject.registerClass({
    Properties: {
        'row-spacing': GObject.ParamSpec.int(
            'row-spacing', null, null,
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'column-spacing': GObject.ParamSpec.int(
            'column-spacing', null, null,
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT32, 0),
        'n-columns': GObject.ParamSpec.int(
            'n-columns', null, null,
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

        let [minHeight, natHeight] = this._overlay.get_preferred_height(-1);

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

        const [, overlayHeight] = this._overlay.get_preferred_height(-1);

        const availWidth = box.get_width() - (this.nColumns - 1) * this.column_spacing;
        const childWidth = Math.floor(availWidth / this.nColumns);

        this._overlay.allocate_available_size(0, 0, box.get_width(), box.get_height());

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

export const QuickSettingsMenu = class extends PopupMenu.PopupMenu {
    constructor(sourceActor, nColumns = 1) {
        super(sourceActor, 0, St.Side.TOP);

        this.actor = new St.Widget({reactive: true, width: 0, height: 0});
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
            const laters = global.compositor.get_laters();
            laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                const offset = this._grid.apply_relative_transform_to_point(
                    this._boxPointer, new Graphene.Point3D());
                yConstraint.offset = offset.y;
                return GLib.SOURCE_REMOVE;
            });
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
        this._completeAddItem(item, colSpan);
    }

    insertItemBefore(item, sibling, colSpan = 1) {
        this._grid.insert_child_below(item, sibling);
        this._completeAddItem(item, colSpan);
    }

    _completeAddItem(item, colSpan) {
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

    getFirstItem() {
        return this._grid.get_first_child();
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
        const color = new Cogl.Color({
            red: val,
            green: val,
            blue: val,
            alpha: 255,
        });

        this._boxPointer.ease_property('@effects.dim.brightness', color, {
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: POPUP_ANIMATION_TIME,
            onStopped: () => (this._dimEffect.enabled = dim),
        });
        this._dimEffect.enabled = true;
    }
};

export const SystemIndicator = GObject.registerClass(
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
        this.add_child(icon);
        icon.connect('notify::visible', () => this._syncIndicatorsVisible());
        this._syncIndicatorsVisible();
        return icon;
    }
});
