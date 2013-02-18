// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const BackgroundMenu = new Lang.Class({
    Name: 'BackgroundMenu',
    Extends: PopupMenu.PopupMenu,

    _init: function(source) {
        this.parent(source, 0, St.Side.TOP);

        this.addSettingsAction(_("Change Background…"), 'gnome-background-panel.desktop');
        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.addSettingsAction(_("System Settings"), 'gnome-control-center.desktop');

        this.actor.add_style_class_name('background-menu');

        Main.uiGroup.add_actor(this.actor);
        this.actor.hide();
    }
});

function addBackgroundMenu(actor) {
    let cursor = new St.Bin({ opacity: 0 });
    Main.uiGroup.add_actor(cursor);

    actor._backgroundMenu = new BackgroundMenu(cursor);
    actor._backgroundManager = new PopupMenu.PopupMenuManager({ actor: actor });
    actor._backgroundManager.addMenu(actor._backgroundMenu);

    function openMenu() {
        let [x, y] = global.get_pointer();
        cursor.set_position(x, y);
        actor._backgroundMenu.open(BoxPointer.PopupAnimation.FULL);
    }

    let clickAction = new Clutter.ClickAction();
    clickAction.connect('long-press', function(action, actor, state) {
        if (state == Clutter.LongPressState.QUERY)
            return action.get_button() == 1 && !actor._backgroundMenu.isOpen;
        if (state == Clutter.LongPressState.ACTIVATE)
            openMenu();
        return true;
    });
    clickAction.connect('clicked', function(action) {
        if (action.get_button() == 3)
            openMenu();
    });
    actor.add_action(clickAction);
}
