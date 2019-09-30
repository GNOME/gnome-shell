// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported addBackgroundMenu */

const { Clutter, Meta, Shell, St } = imports.gi;

const AppActivation = imports.ui.appActivation;
const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

var BackgroundMenu = class BackgroundMenu extends PopupMenu.PopupMenu {
    constructor(layoutManager) {
        super(layoutManager.dummyCursor, 0, St.Side.TOP);

        this.addSettingsAction(_("Change Background…"), 'gnome-background-panel.desktop');
        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this.addAction(_("Add App"), () => {
            let app = Shell.AppSystem.get_default().lookup_app('org.gnome.Software.desktop');
            let activationContext = new AppActivation.AppActivationContext(app);
            activationContext.activate(Clutter.get_current_event());
        });

        this.addAction(_("Add Website"), () => {
            Main.appStore.showPage(global.get_current_time(), 'web');
        });

        this.addAction(_("Add Folder"), () => {
            Main.appStore.showPage(global.get_current_time(), 'folders');
        });

        this.actor.add_style_class_name('background-menu');

        layoutManager.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }
};

function addBackgroundMenu(actor, layoutManager) {
    actor.reactive = true;
    actor._backgroundMenu = new BackgroundMenu(layoutManager);
    actor._backgroundManager = new PopupMenu.PopupMenuManager(actor);
    actor._backgroundManager.addMenu(actor._backgroundMenu);

    function openMenu(x, y) {
        Main.layoutManager.setDummyCursorGeometry(x, y, 0, 0);
        actor._backgroundMenu.open(BoxPointer.PopupAnimation.FULL);
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
