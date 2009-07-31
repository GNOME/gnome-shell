/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const DocDisplay = imports.ui.docDisplay;
const DocInfo = imports.misc.docInfo;

const COLLAPSED_WIDTH = 24;
const EXPANDED_WIDTH = 200;

const STATE_EXPANDED    = 0;
const STATE_COLLAPSING  = 1;
const STATE_COLLAPSED   = 2;
const STATE_EXPANDING   = 3;
const STATE_POPPING_OUT = 4;
const STATE_POPPED_OUT  = 5;
const STATE_POPPING_IN  = 6;

function Widget() {
}

Widget.prototype = {
    // _init():
    //
    // Your widget constructor. Your constructor function should look
    // like:
    //
    //     function MyWidgetType() {
    //         this._init.apply(this, arguments);
    //     }
    //
    // and your _init method should start by doing:
    //
    //     Widget.Widget.prototype._init.apply(this, arguments);
    //
    // The _init method must define a field named "actor" containing
    // the Clutter.Actor to show in expanded mode. This actor will be
    // clipped to Widget.EXPANDED_WIDTH. Most widgets will also define
    // a field named "title" containing the title string to show above
    // the widget in the sidebar.
    //
    // If you want to have a separate collapsed view, you can define a
    // field "collapsedActor" containing the Clutter.Actor to show in
    // that mode. (It may be the same actor.) This actor will be
    // clipped to Widget.COLLAPSED_WIDTH, and will normally end up
    // having the same height as the main actor.
    //
    // If you do not set a collapsedActor, then you must set a title,
    // since that is what will be displayed in collapsed mode, and
    // in this case (and only in this case), the widget will support
    // pop-out, meaning that if the user hovers over its title while
    // the sidebar is collapsed, the widget's expanded view will pop
    // out of the sidebar until either the cursor moves out of it,
    // or else the widget calls this.activated() on itself.
    _init: function (initialState) {
        this.state = initialState;
    },

    // destroy():
    //
    // Optional. Will be called when the widget is removed from the
    // sidebar. (Note that you don't need to destroy the actors,
    // since they will be destroyed for you.)

    // collapse():
    //
    // Optional. Called during the sidebar collapse process, at the
    // point when the expanded sidebar has slid offscreen, but the
    // collapsed sidebar has not yet slid onscreen.

    // expand():
    //
    // Optional. Called during the sidebar expand process, at the
    // point when the collapsed sidebar has slid offscreen, but the
    // expanded sidebar has not yet slid onscreen.

    // activated():
    //
    // Emits the "activated" signal for you, which will cause pop-out
    // to end.
    activated: function() {
        this.emit('activated');
    }

    // state:
    //
    // A field set on your widget by the sidebar. Will contain one of
    // the Widget.STATE_* values. (Eg, Widget.STATE_EXPANDED).
};

Signals.addSignalMethods(Widget.prototype);


function ClockWidget() {
    this._init.apply(this, arguments);
}

ClockWidget.prototype = {
    __proto__ : Widget.prototype,

    _init: function() {
        Widget.prototype._init.apply(this, arguments);

        this.actor = new Clutter.Text({ font_name: "Sans Bold 16px",
                                        text: "",
                                        // Give an explicit height to ensure
                                        // it's the same in both modes
                                        height: COLLAPSED_WIDTH });

        this.collapsedActor = new Clutter.CairoTexture({ width: COLLAPSED_WIDTH,
                                                         height: COLLAPSED_WIDTH,
                                                         surface_width: COLLAPSED_WIDTH,
                                                         surface_height: COLLAPSED_WIDTH });

        this._update();
    },

    destroy: function() {
        if (this.timer)
            Mainloop.source_remove(this.timer);
    },

    expand: function() {
        this._update();
    },

    collapse: function() {
        this._update();
    },

    _update: function() {
        let time = new Date();
        let msec_remaining = 60000 - (1000 * time.getSeconds() +
                                      time.getMilliseconds());
        if (msec_remaining < 500) {
            time.setMinutes(time.getMinutes() + 1);
            msec_remaining += 60000;
        }

        if (this.state == STATE_COLLAPSED || this.state == STATE_COLLAPSING)
            this._updateCairo(time);
        else
            this._updateText(time);

        if (this.timer)
            Mainloop.source_remove(this.timer);
        this.timer = Mainloop.timeout_add(msec_remaining, Lang.bind(this, this._update));
        return false;
    },

    _updateText: function(time) {
        this.actor.set_text(time.toLocaleFormat("%H:%M"));
    },

    _updateCairo: function(time) {
        let global = Shell.Global.get();
        Shell.draw_clock(this.collapsedActor,
                         time.getHours() % 12,
                         time.getMinutes());
    }
};


const ITEM_ICON_SIZE = 48;
const ITEM_PADDING = 1;
const ITEM_SPACING = 4;

const ITEM_BG_COLOR = new Clutter.Color();
ITEM_BG_COLOR.from_pixel(0x00000000);
const ITEM_NAME_COLOR = new Clutter.Color();
ITEM_NAME_COLOR.from_pixel(0x000000ff);

function LauncherWidget() {
    this._init.apply(this, arguments);
}

LauncherWidget.prototype = {
    __proto__ : Widget.prototype,

    addItem : function(info) {
        let item = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                 width: EXPANDED_WIDTH,
                                 height: ITEM_ICON_SIZE,
                                 padding: ITEM_PADDING,
                                 spacing: ITEM_SPACING,
                                 reactive: true });
        item._info = info;
        item.append(info.createIcon(ITEM_ICON_SIZE), Big.BoxPackFlags.NONE);
        item.append(new Clutter.Text({ color: ITEM_NAME_COLOR,
                                       font_name: "Sans 14px",
                                       ellipsize: Pango.EllipsizeMode.END,
                                       text: info.name }),
                    Big.BoxPackFlags.NONE);

        this.actor.append(item, Big.BoxPackFlags.NONE);
        item.connect('button-press-event', Lang.bind(this, this._buttonPress));
        item.connect('button-release-event', Lang.bind(this, this._buttonRelease));
        item.connect('leave-event', Lang.bind(this, this._leave));
        item.connect('enter-event', Lang.bind(this, this._enter));

        if (!this.collapsedActor)
            return;

        item = new Big.Box({ width: COLLAPSED_WIDTH,
                             height: COLLAPSED_WIDTH,
                             padding: ITEM_PADDING,
                             reactive: true });
        item._info = info;
        item.append(info.createIcon(COLLAPSED_WIDTH - 2 * ITEM_PADDING),
                    Big.BoxPackFlags.NONE);

        this.collapsedActor.append(item, Big.BoxPackFlags.NONE);
        item.connect('button-press-event', Lang.bind(this, this._buttonPress));
        item.connect('button-release-event', Lang.bind(this, this._buttonRelease));
        item.connect('leave-event', Lang.bind(this, this._leave));
        item.connect('enter-event', Lang.bind(this, this._enter));
    },

    clear : function() {
        let children, i;

        children = this.actor.get_children();
        for (i = 0; i < children.length; i++)
            children[i].destroy();

        if (this.collapsedActor) {
            children = this.collapsedActor.get_children();
            for (i = 0; i < children.length; i++)
                children[i].destroy();
        }
    },

    _buttonPress : function(item) {
        Clutter.grab_pointer(item);
        item._buttonDown = true;
        item._inItem = true;
        this._updateItemState(item);
        return true;
    },

    _leave : function(item, evt) {
        if (evt.get_source() == item && item._buttonDown) {
            item._inItem = false;
            this._updateItemState(item);
        }
        return false;
    },

    _enter : function(item, evt) {
        if (evt.get_source() == item && item._buttonDown) {
            item._inItem = true;
            this._updateItemState(item);
        }
        return false;
    },

    _buttonRelease : function(item) {
        Clutter.ungrab_pointer(item);
        item._buttonDown = false;
        this._updateItemState(item);

        if (item._inItem) {
            item._info.launch();
            this.activated();
        }
        return true;
    },

    _updateItemState : function(item) {
        if (item._buttonDown && item._inItem) {
            item.padding_top = item.padding_left = 2 * ITEM_PADDING;
            item.padding_bottom = item.padding_right = 0;
        } else
            item.padding = ITEM_PADDING;
    }
};

function AppsWidgetInfo(appInfo) {
    this._init(appInfo);
}

AppsWidgetInfo.prototype = {
    _init : function(appInfo) {
        this._info = appInfo;
        this.name = appInfo.get_name();
    },

    createIcon : function(size) {
        return this._info.create_icon_texture(size);
    },

    launch : function() {
        this._info.launch();
    }
}

function AppsWidget() {
    this._init.apply(this, arguments);
}

AppsWidget.prototype = {
    __proto__ : LauncherWidget.prototype,

    _init : function() {
        Widget.prototype._init.apply(this, arguments);

        this.title = "Applications";
        this.actor = new Big.Box({ spacing: 2 });
        this.collapsedActor = new Big.Box({ spacing: 2});

        let appSystem = Shell.AppSystem.get_default();
        let apps = appSystem.get_favorites();
        for (let i = 0; i < apps.length; i++) {
            let app = appSystem.lookup_app(apps[i]);
            if (!app)
                continue;
            this.addItem(new AppsWidgetInfo(app));
        }
    }
};

function RecentDocsWidget() {
    this._init.apply(this, arguments);
}

RecentDocsWidget.prototype = {
    __proto__ : LauncherWidget.prototype,

    _init : function() {
        Widget.prototype._init.apply(this, arguments);

        this.title = "Recent Documents";
        this.actor = new Big.Box({ spacing: 2 });

        this._recentManager = Gtk.RecentManager.get_default();
        this._recentManager.connect('changed', Lang.bind(this, this._recentChanged));
        this._recentChanged();
    },

    _recentChanged: function() {
        let i;

        this.clear();

        let items = [];
        let docs = this._recentManager.get_items();
        for (i = 0; i < docs.length; i++) {
            let docInfo = new DocInfo.DocInfo (docs[i]);

            if (docInfo.exists())
                items.push(docInfo);
        }

        items.sort(function (a,b) { return b.timestamp - a.timestamp; });
        for (i = 0; i < Math.min(items.length, 5); i++)
            this.addItem(items[i]);
    }
};
