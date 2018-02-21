// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

var BackgroundMenu = new Lang.Class({
    Name: 'BackgroundMenu',
    Extends: PopupMenu.PopupMenu,

    _init(layoutManager) {
        this.parent(layoutManager.dummyCursor, 0, St.Side.TOP);

        this.addSettingsAction(_("Change Background…"), 'gnome-background-panel.desktop');
        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.addSettingsAction(_("Display Settings"), 'gnome-display-panel.desktop');
        this.addSettingsAction(_("Settings"), 'gnome-control-center.desktop');

        this.actor.add_style_class_name('background-menu');

        layoutManager.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }
});

function addBackgroundMenu(actor, layoutManager) {
    actor.reactive = true;
    actor._backgroundMenu = new BackgroundMenu(layoutManager);
    actor._backgroundManager = new PopupMenu.PopupMenuManager({ actor: actor });
    actor._backgroundManager.addMenu(actor._backgroundMenu);

    function openMenu(x, y) {
        Main.layoutManager.setDummyCursorGeometry(x, y, 0, 0);
        actor._backgroundMenu.open(BoxPointer.PopupAnimation.NONE);
    }

    let clickAction = new Clutter.ClickAction();
    clickAction.connect('long-press', (action, actor, state) => {
        if (state == Clutter.LongPressState.QUERY)
            return ((action.get_button() == 0 ||
                     action.get_button() == 1) &&
                    !actor._backgroundMenu.isOpen);
        if (state == Clutter.LongPressState.ACTIVATE) {
            let [x, y] = action.get_coords();
            openMenu(x, y);
            actor._backgroundManager.ignoreRelease();
        }
        return true;
    });
    clickAction.connect('clicked', action => {
        if (action.get_button() == 3) {
            let [x, y] = action.get_coords();
            openMenu(x, y);
        }
    });
    actor.add_action(clickAction);

    let grabOpBeginId = global.display.connect('grab-op-begin', () => {
        clickAction.release();
    });

    actor.connect('destroy', () => {
        actor._backgroundMenu.destroy();
        actor._backgroundMenu = null;
        actor._backgroundManager = null;
        global.display.disconnect(grabOpBeginId);
    });
}
