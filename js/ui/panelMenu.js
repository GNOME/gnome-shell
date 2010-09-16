/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

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
                                  x_fill: true,
                                  y_fill: false,
                                  track_hover: true });
        this.actor._delegate = this;
        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.menu = new PopupMenu.PopupMenu(this.actor, menuAlignment, St.Side.TOP, /* FIXME */ 0);
        this.menu.connect('open-state-changed', Lang.bind(this, this._onOpenStateChanged));
        Main.chrome.addActor(this.menu.actor, { visibleInOverview: true,
                                                affectsStruts: false });
        this.menu.actor.hide();
    },

    _onButtonPress: function(actor, event) {
        this.menu.toggle();
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
        this._iconActor = null;
        this.setIcon(iconName);
        this.setTooltip(tooltipText);
    },

    setIcon: function(iconName) {
        this._iconName = iconName;
        if (this._iconActor)
            this._iconActor.destroy();
        this._iconActor = St.TextureCache.get_default().load_icon_name(this._iconName, St.IconType.SYMBOLIC, 24);
        this.actor.set_child(this._iconActor);
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