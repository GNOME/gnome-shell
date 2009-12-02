/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const Lang = imports.lang;

const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const Widget = imports.ui.widget;
const WidgetBox = imports.ui.widgetBox;

const SIDEBAR_SPACING = 4;
const SIDEBAR_PADDING = 4;

// The total sidebar width is the widget width plus the widget padding
// (counted twice for the widget box, and once again for the
// out-of-screen padding), plus the empty space between the border of
// the bar and of the windows
const SIDEBAR_COLLAPSED_WIDTH = Widget.COLLAPSED_WIDTH + 3 * WidgetBox.WIDGETBOX_PADDING + SIDEBAR_PADDING;
const SIDEBAR_EXPANDED_WIDTH = Widget.EXPANDED_WIDTH + 3 * WidgetBox.WIDGETBOX_PADDING + SIDEBAR_PADDING;

function Sidebar() {
    this._init();
}

Sidebar.prototype = {
    _init : function() {
        // The top-left corner of the sidebar is fixed at:
        // x = -WidgetBox.WIDGETBOX_PADDING, y = Panel.PANEL_HEIGHT.
        // (The negative X is so that we don't see the rounded
        // WidgetBox corners on the screen edge side.)
        this.actor = new Clutter.Group({ x: -WidgetBox.WIDGETBOX_PADDING,
                                         y: Panel.PANEL_HEIGHT,
                                         width: SIDEBAR_EXPANDED_WIDTH });

        // The actual widgets go into a Big.Box inside this.actor. The
        // box's width will vary during the expand/collapse animations,
        // but this.actor's width will remain constant until we adjust
        // it at the end of the animation, because we don't want the
        // wm strut to move and cause windows to move multiple times
        // during the animation.
        this.box = new Big.Box ({ padding_top: SIDEBAR_PADDING,
                                  padding_bottom: SIDEBAR_PADDING,
                                  padding_right: 0,
                                  padding_left: 0,
                                  spacing: SIDEBAR_SPACING });
        this.actor.add_actor(this.box);

        this._gconf = Shell.GConf.get_default();

        this._expanded = this._gconf.get_boolean ("sidebar/expanded");
        if (!this._expanded)
            this.actor.width = SIDEBAR_COLLAPSED_WIDTH;

        this._visible = this._gconf.get_boolean ("sidebar/visible");
        if (this._visible)
            Main.chrome.addActor(this.actor);

        this._widgets = [];
        this.addWidget(new ToggleWidget());

        let default_widgets = this._gconf.get_string_list("sidebar/widgets");
        for (let i = 0; i < default_widgets.length; i++)
            this.addWidget(default_widgets[i]);

        this._gconf.connect('changed::sidebar/expanded',
                            Lang.bind(this, this._expandedChanged));
        this._gconf.connect('changed::sidebar/visible',
                            Lang.bind(this, this._visibleChanged));

        this._adjustPosition();
    },

    addWidget: function(widget) {
        let widgetBox;
        try {
            widgetBox = new WidgetBox.WidgetBox(widget, this._expanded);
        } catch(e) {
            logError(e, "Failed to add widget '" + widget + "'");
            return;
        }

        this.box.append(widgetBox.actor, Big.BoxPackFlags.NONE);
        this._widgets.push(widgetBox);
        this._adjustPosition();
    },

    _adjustPosition: function() {
        let primary=global.get_primary_monitor();

        this.actor.y = Math.max(primary.y + Panel.PANEL_HEIGHT,primary.height/2 - this.actor.height/2);
        this.actor.x = primary.x;
    },

    _visibleChanged: function() {
        let visible = this._gconf.get_boolean("sidebar/visible");
        if (visible == this._visible)
            return;

        this._visible = visible;
        if (visible)
            Main.chrome.addActor(this.actor);
        else
            Main.chrome.removeActor(this.actor);
    },

    _expandedChanged: function() {
        let expanded = this._gconf.get_boolean("sidebar/expanded");
        if (expanded == this._expanded)
            return;

        this._expanded = expanded;
        if (expanded)
            this._expand();
        else
            this._collapse();
    },

    _expand: function() {
        this._expanded = true;
        for (let i = 0; i < this._widgets.length; i++)
            this._widgets[i].expand();

        // Updated the strut/stage area after the animation completes
        Tweener.addTween(this, { time: WidgetBox.ANIMATION_TIME,
                                 onComplete: function () {
                                     this.actor.width = SIDEBAR_EXPANDED_WIDTH;
                                 } });
    },

    _collapse: function() {
        this._expanded = false;
        for (let i = 0; i < this._widgets.length; i++)
            this._widgets[i].collapse();

        // Updated the strut/stage area after the animation completes
        Tweener.addTween(this, { time: WidgetBox.ANIMATION_TIME,
                                 onComplete: function () {
                                     this.actor.width = SIDEBAR_COLLAPSED_WIDTH;
                                 } });
    },

    destroy: function() {
        this.hide();

        for (let i = 0; i < this._widgets.length; i++)
            this._widgets[i].destroy();
        this.actor.destroy();
    }
};

const LEFT_DOUBLE_ARROW = "\u00AB";
const RIGHT_DOUBLE_ARROW = "\u00BB";

function ToggleWidget() {
    this._init();
}

ToggleWidget.prototype = {
    __proto__ : Widget.Widget.prototype,

    _init : function() {
        this._gconf = Shell.GConf.get_default();

        this.actor = new Clutter.Text({ font_name: "Sans Bold 16px",
                                        text: LEFT_DOUBLE_ARROW,
                                        reactive: true });
        this.actor.connect('button-release-event',
                           Lang.bind(this, this._collapse));
        this.collapsedActor = new Clutter.Text({ font_name: "Sans Bold 16px",
                                                 text: RIGHT_DOUBLE_ARROW,
                                                 reactive: true });
        this.collapsedActor.connect('button-release-event',
                                    Lang.bind(this, this._expand));
    },

    _collapse : function () {
        this._gconf.set_boolean ("sidebar/expanded", false);
    },

    _expand : function () {
        this._gconf.set_boolean ("sidebar/expanded", true);
    }
};
