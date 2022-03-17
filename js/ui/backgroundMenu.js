import Clutter from 'gi://Clutter';
import St from 'gi://St';

import * as BoxPointer from './boxpointer.js';
import * as PopupMenu from './popupMenu.js';

import * as Main from './main.js';

export class BackgroundMenu extends PopupMenu.PopupMenu {
    constructor(layoutManager) {
        super(layoutManager.dummyCursor, 0, St.Side.TOP);

        this.addSettingsAction(_('Change Background…'), 'gnome-background-panel.desktop');
        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.addSettingsAction(_('Display Settings'), 'gnome-display-panel.desktop');
        this.addSettingsAction(_('Settings'), 'org.gnome.Settings.desktop');

        this.actor.add_style_class_name('background-menu');

        layoutManager.uiGroup.add_child(this.actor);
        this.actor.hide();
    }
}

/**
 * @param {Meta.BackgroundActor} actor
 * @param {import('./layout.js').LayoutManager} layoutManager
 */
export function addBackgroundMenu(actor, layoutManager) {
    actor.reactive = true;
    actor._backgroundMenu = new BackgroundMenu(layoutManager);
    actor._backgroundManager = new PopupMenu.PopupMenuManager(actor);
    actor._backgroundManager.addMenu(actor._backgroundMenu);

    function openMenu(x, y) {
        Main.layoutManager.setDummyCursorGeometry(x, y, 0, 0);
        actor._backgroundMenu.open(BoxPointer.PopupAnimation.FULL);
    }

    const longPressGesture = new Clutter.LongPressGesture({
        required_button: Clutter.BUTTON_PRIMARY,
    });
    longPressGesture.connect('recognize', () => {
        if (actor._backgroundMenu.isOpen)
            return;

        const coords = longPressGesture.get_coords();
        const point = new Graphene.Point3D({x: coords.x, y: coords.y});
        const transformedPoint = actor.apply_transform_to_point(point);
        openMenu(transformedPoint.x, transformedPoint.y);
    });
    actor.add_action(longPressGesture);

    const clickAction = new Clutter.ClickAction();
    clickAction.connect('clicked', action => {
        if (action.get_button() === 3) {
            let [x, y] = action.get_coords();
            openMenu(x, y);
        }
    });
    actor.add_action(clickAction);

    actor.connect('destroy', () => {
        actor._backgroundMenu.destroy();
        actor._backgroundMenu = null;
        actor._backgroundManager = null;
    });
}
