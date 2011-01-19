/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Lang = imports.lang;
const PopupMenu = imports.ui.popupMenu;
const Main = imports.ui.main;

function Button(menuAlignment) {
    this._init(menuAlignment);
}

Button.prototype = {
    _init: function(menuAlignment) {
        this.actor = new St.Bin({ style_class: 'panel-button',
                                  reactive: true,
                                  can_focus: true,
                                  x_fill: true,
                                  y_fill: false,
                                  track_hover: true });
        this.actor._delegate = this;
        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPress));
        this.menu = new PopupMenu.PopupMenu(this.actor, menuAlignment, St.Side.TOP, /* FIXME */ 0);
        this.menu.connect('open-state-changed', Lang.bind(this, this._onOpenStateChanged));
        Main.chrome.addActor(this.menu.actor, { visibleInOverview: true,
                                                affectsStruts: false });
        this.menu.actor.hide();
    },

    _onButtonPress: function(actor, event) {
        this.menu.toggle();
    },

    _onKeyPress: function(actor, event) {
        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.menu.toggle();
            return true;
        } else if (symbol == Clutter.KEY_Down) {
            if (!this.menu.isOpen)
                this.menu.toggle();
            this.menu.activateFirst();
            return true;
        } else
            return false;
    },

    _onOpenStateChanged: function(menu, open) {
        if (open)
            this.actor.add_style_pseudo_class('pressed');
        else
            this.actor.remove_style_pseudo_class('pressed');
    }
};

/* SystemStatusButton:
 *
 * This class manages one System Status indicator (network, keyboard,
 * volume, bluetooth...), which is just a PanelMenuButton with an
 * icon and a tooltip
 */
function SystemStatusButton() {
    this._init.apply(this, arguments);
}

SystemStatusButton.prototype = {
    __proto__: Button.prototype,

    _init: function(iconName,tooltipText) {
        Button.prototype._init.call(this, St.Align.START);
        this._iconActor = new St.Icon({ icon_name: iconName,
                                        icon_type: St.IconType.SYMBOLIC,
                                        style_class: 'system-status-icon' });
        this.actor.set_child(this._iconActor);
        this.setTooltip(tooltipText);
    },

    setIcon: function(iconName) {
        this._iconActor.icon_name = iconName;
    },

    setGIcon: function(gicon) {
        this._iconActor.gicon = gicon;
    },

    setTooltip: function(text) {
        if (text != null) {
            this.tooltip = text;
            this.actor.has_tooltip = true;
            this.actor.tooltip_text = text;
        } else {
            this.actor.has_tooltip = false;
            this.tooltip = null;
        }
    }
};
