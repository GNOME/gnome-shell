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

// The total sidebar width is the widget width plus the widget
// padding, plus the sidebar padding
const SIDEBAR_COLLAPSED_WIDTH = Widget.COLLAPSED_WIDTH + 2 * WidgetBox.WIDGETBOX_PADDING + 2 * SIDEBAR_PADDING;
const SIDEBAR_EXPANDED_WIDTH = Widget.EXPANDED_WIDTH + 2 * WidgetBox.WIDGETBOX_PADDING + 2 * SIDEBAR_PADDING;

// The maximum height of the sidebar would be extending from just
// below the panel to just above the taskbar. Since the taskbar is
// just a temporary hack and it would be too hard to do this the right
// way, we just hardcode its size.
const HARDCODED_TASKBAR_HEIGHT = 24;
const MAXIMUM_SIDEBAR_HEIGHT = Shell.Global.get().screen_height - Panel.PANEL_HEIGHT - HARDCODED_TASKBAR_HEIGHT;

// FIXME, needs to be configurable, obviously
const default_widgets = [
    "imports.ui.widget.ClockWidget",
    "imports.ui.widget.AppsWidget",
    "imports.ui.widget.DocsWidget"
];

function Sidebar() {
    this._init();
}

Sidebar.prototype = {
    _init : function() {
        let global = Shell.Global.get();

        // The top-left corner of the sidebar is fixed at:
        // x = -WidgetBox.WIDGETBOX_PADDING, y = Panel.PANEL_HEIGHT.
        // (The negative X is so that we don't see the rounded
        // WidgetBox corners on the screen edge side.)
        this.actor = new Clutter.Group({ x: -WidgetBox.WIDGETBOX_PADDING,
                                         y: Panel.PANEL_HEIGHT,
                                         width: SIDEBAR_EXPANDED_WIDTH });
        Main.chrome.addActor(this.actor);

        // The actual widgets go into a Big.Box inside this.actor. The
        // box's width will vary during the expand/collapse animations,
        // but this.actor's width will remain constant until we adjust
        // it at the end of the animation, because we don't want the
        // wm strut to move and cause windows to move multiple times
        // during the animation.
        this.box = new Big.Box ({ padding_top: SIDEBAR_PADDING,
                                  padding_bottom: SIDEBAR_PADDING,
                                  padding_right: SIDEBAR_PADDING,
                                  padding_left: 0,
                                  spacing: SIDEBAR_SPACING });
        this.actor.add_actor(this.box);

        this._visible = this.expanded = true;

        this._widgets = [];
        this.addWidget(new ToggleWidget(this));
        for (let i = 0; i < default_widgets.length; i++)
            this.addWidget(default_widgets[i]);
    },

    addWidget: function(widget) {
        let widgetBox;
        try {
            widgetBox = new WidgetBox.WidgetBox(widget);
        } catch(e) {
            logError(e, "Failed to add widget '" + widget + "'");
            return;
        }

        this.box.append(widgetBox.actor, Big.BoxPackFlags.NONE);
        this._widgets.push(widgetBox);
    },

    show: function() {
        this._visible = true;
        this.actor.show();
    },

    hide: function() {
        this._visible = false;
        this.actor.hide();
    },

    expand: function() {
        this.expanded = true;
        for (let i = 0; i < this._widgets.length; i++)
            this._widgets[i].expand();

        // Updated the strut/stage area after the animation completes
        Tweener.addTween(this, { time: WidgetBox.ANIMATION_TIME,
                                 onComplete: function () {
                                     this.actor.width = SIDEBAR_EXPANDED_WIDTH;
                                 } });
    },

    collapse: function() {
        this.expanded = false;
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

function ToggleWidget(sidebar) {
    this._init(sidebar);
}

ToggleWidget.prototype = {
    __proto__ : Widget.Widget.prototype,

    _init : function(sidebar) {
        this._sidebar = sidebar;
        this.actor = new Clutter.Text({ font_name: "Sans Bold 16px",
                                        text: LEFT_DOUBLE_ARROW,
                                        reactive: true });
        this.actor.connect('button-release-event',
                           Lang.bind(this._sidebar, this._sidebar.collapse));
        this.collapsedActor = new Clutter.Text({ font_name: "Sans Bold 16px",
                                                 text: RIGHT_DOUBLE_ARROW,
                                                 reactive: true });
        this.collapsedActor.connect('button-release-event',
                                    Lang.bind(this._sidebar, this._sidebar.expand));
    }
};
