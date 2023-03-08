// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Button, SystemIndicator */

const { Atk, Clutter, GObject, St } = imports.gi;

const Main = imports.ui.main;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;

var ButtonBox = GObject.registerClass(
class ButtonBox extends St.Widget {
    _init(params) {
        params = Params.parse(params, {
            style_class: 'panel-button',
            x_expand: true,
            y_expand: true,
        }, true);

        super._init(params);

        this._delegate = this;

        this.container = new St.Bin({ child: this });

        this.connect('style-changed', this._onStyleChanged.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));

        this._minHPadding = this._natHPadding = 0.0;
    }

    _onStyleChanged(actor) {
        let themeNode = actor.get_theme_node();

        this._minHPadding = themeNode.get_length('-minimum-hpadding');
        this._natHPadding = themeNode.get_length('-natural-hpadding');
    }

    vfunc_get_preferred_width(_forHeight) {
        let child = this.get_first_child();
        let minimumSize, naturalSize;

        if (child)
            [minimumSize, naturalSize] = child.get_preferred_width(-1);
        else
            minimumSize = naturalSize = 0;

        minimumSize += 2 * this._minHPadding;
        naturalSize += 2 * this._natHPadding;

        return [minimumSize, naturalSize];
    }

    vfunc_get_preferred_height(_forWidth) {
        let child = this.get_first_child();

        if (child)
            return child.get_preferred_height(-1);

        return [0, 0];
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        let child = this.get_first_child();
        if (!child)
            return;

        let [, natWidth] = child.get_preferred_width(-1);

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let childBox = new Clutter.ActorBox();
        if (natWidth + 2 * this._natHPadding <= availWidth) {
            childBox.x1 = this._natHPadding;
            childBox.x2 = availWidth - this._natHPadding;
        } else {
            childBox.x1 = this._minHPadding;
            childBox.x2 = availWidth - this._minHPadding;
        }

        childBox.y1 = 0;
        childBox.y2 = availHeight;

        child.allocate(childBox);
    }

    _onDestroy() {
        this.container.child = null;
        this.container.destroy();
    }
});

var Button = GObject.registerClass({
    Signals: { 'menu-set': {} },
}, class PanelMenuButton extends ButtonBox {
    _init(menuAlignment, nameText, dontCreateMenu) {
        super._init({
            reactive: true,
            can_focus: true,
            track_hover: true,
            accessible_name: nameText ?? '',
            accessible_role: Atk.Role.MENU,
        });

        if (dontCreateMenu)
            this.menu = new PopupMenu.PopupDummyMenu(this);
        else
            this.setMenu(new PopupMenu.PopupMenu(this, menuAlignment, St.Side.TOP, 0));

        this.connect('key-press-event',
            (o, ev) => global.focus_manager.navigate_from_event(ev));
    }

    setSensitive(sensitive) {
        this.reactive = sensitive;
        this.can_focus = sensitive;
        this.track_hover = sensitive;
    }

    setMenu(menu) {
        if (this.menu)
            this.menu.destroy();

        this.menu = menu;
        if (this.menu) {
            this.menu.actor.add_style_class_name('panel-menu');
            this.menu.connect('open-state-changed', this._onOpenStateChanged.bind(this));
            this.menu.actor.connect('key-press-event', this._onMenuKeyPress.bind(this));

            Main.uiGroup.add_actor(this.menu.actor);
            this.menu.actor.hide();
        }
        this.emit('menu-set');
    }

    vfunc_event(event) {
        if (this.menu &&
            (event.type() == Clutter.EventType.TOUCH_BEGIN ||
             event.type() == Clutter.EventType.BUTTON_PRESS))
            this.menu.toggle();

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_hide() {
        super.vfunc_hide();

        if (this.menu)
            this.menu.close();
    }

    _onMenuKeyPress(actor, event) {
        if (global.focus_manager.navigate_from_event(event))
            return Clutter.EVENT_STOP;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_Left || symbol == Clutter.KEY_Right) {
            let group = global.focus_manager.get_group(this);
            if (group) {
                let direction = symbol == Clutter.KEY_Left ? St.DirectionType.LEFT : St.DirectionType.RIGHT;
                group.navigate_focus(this, direction, false);
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _onOpenStateChanged(menu, open) {
        if (open)
            this.add_style_pseudo_class('active');
        else
            this.remove_style_pseudo_class('active');

        // Setting the max-height won't do any good if the minimum height of the
        // menu is higher then the screen; it's useful if part of the menu is
        // scrollable so the minimum height is smaller than the natural height
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let verticalMargins = this.menu.actor.margin_top + this.menu.actor.margin_bottom;

        // The workarea and margin dimensions are in physical pixels, but CSS
        // measures are in logical pixels, so make sure to consider the scale
        // factor when computing max-height
        let maxHeight = Math.round((workArea.height - verticalMargins) / scaleFactor);
        this.menu.actor.style = `max-height: ${maxHeight}px;`;
    }

    _onDestroy() {
        if (this.menu)
            this.menu.destroy();
        super._onDestroy();
    }
});

/* SystemIndicator:
 *
 * This class manages one system indicator, which are the icons
 * that you see at the top right. A system indicator is composed
 * of an icon and a menu section, which will be composed into the
 * aggregate menu.
 */
var SystemIndicator = GObject.registerClass(
class SystemIndicator extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'panel-status-indicators-box',
            reactive: true,
            visible: false,
        });
        this.menu = new PopupMenu.PopupMenuSection();
    }

    get indicators() {
        let klass = this.constructor.name;
        let { stack } = new Error();
        log(`Usage of indicator.indicators is deprecated for ${klass}\n${stack}`);
        return this;
    }

    _syncIndicatorsVisible() {
        this.visible = this.get_children().some(a => a.visible);
    }

    _addIndicator() {
        let icon = new St.Icon({ style_class: 'system-status-icon' });
        this.add_actor(icon);
        icon.connect('notify::visible', this._syncIndicatorsVisible.bind(this));
        this._syncIndicatorsVisible();
        return icon;
    }
});
