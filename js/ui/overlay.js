/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Signals = imports.signals;
const Mainloop = imports.mainloop;
const Tweener = imports.tweener.tweener;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;

const Desktops = imports.ui.desktops;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Big = imports.gi.Big;
const AppDisplay = imports.ui.appdisplay;

const OVERLAY_BACKGROUND_COLOR = new Clutter.Color();
OVERLAY_BACKGROUND_COLOR.from_pixel(0x000000ff);

const SIDESHOW_PAD = 6;
const SIDESHOW_MIN_WIDTH = 250;
const SIDESHOW_SEARCH_BG_COLOR = new Clutter.Color();
SIDESHOW_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const SIDESHOW_TEXT_COLOR = new Clutter.Color();
SIDESHOW_TEXT_COLOR.from_pixel(0xffffffff);

// Time for initial animation going into overlay mode
const ANIMATION_TIME = 0.5;

// How much to scale the desktop down by in overlay mode
const DESKTOP_SCALE = 0.75;

function Sideshow(parent, width) {
    this._init(parent, width);
}

Sideshow.prototype = {
    _init : function(parent, width) {
        let me = this;

        let global = Shell.Global.get();
        this.actor = new Clutter.Group();
        parent.add_actor(this.actor);
        let icontheme = Gtk.IconTheme.get_default();
        let rect = new Big.Box({ background_color: SIDESHOW_SEARCH_BG_COLOR,
                                 corner_radius: 4,
                                 x: SIDESHOW_PAD,
                                 y: Panel.PANEL_HEIGHT + SIDESHOW_PAD,
                                 width: width,
                                 height: 24});
        this.actor.add_actor(rect);

        let searchIconTexture = new Clutter.Texture({ x: SIDESHOW_PAD + 2,
                                                      y: rect.y + 2 });
        let searchIconPath = icontheme.lookup_icon('gtk-find', 16, 0).get_filename();
        searchIconTexture.set_from_file(searchIconPath);
        this.actor.add_actor(searchIconTexture);

        this._searchEntry = new Clutter.Entry({
                                             font_name: "Sans 14px",
                                             x: searchIconTexture.x
                                                 + searchIconTexture.width + 4,
                                             y: searchIconTexture.y,
                                             width: rect.width - (searchIconTexture.x),
                                             height: searchIconTexture.height});
        this.actor.add_actor(this._searchEntry);
        global.stage.set_key_focus(this._searchEntry);
        this._searchQueued = false;
        this._searchActive = false;
        this._searchEntry.connect('notify::text', function (se, prop) {
            if (me._searchQueued)
                return;
            Mainloop.timeout_add(250, function() {
                let text = me._searchEntry.text;
                me._searchQueued = false;
                me._searchActive = text != '';
                me._appdisplay.setSearch(text);
                return false;
            });
        });
        this._searchEntry.connect('activate', function (se) {
            me._searchEntry.text = '';
            me._appdisplay.searchActivate();
            return true;
        });
        this._searchEntry.connect('key-press-event', function (se, e) {
            let code = e.get_code();
            if (code == 9) {
                me._searchEntry.text = '';
                return true;
            } else if (code == 111) {
                me._appdisplay.selectUp();
                return true;
            } else if (code == 116) {
                me._appdisplay.selectDown();
                return true;
            }
            return false;
        });

        let appsText = new Clutter.Label({ color: SIDESHOW_TEXT_COLOR,
                                           font_name: "Sans Bold 14px",
                                           text: "Applications",
                                           x: SIDESHOW_PAD,
                                           y: this._searchEntry.y + this._searchEntry.height + 10,
                                           height: 16});
        this.actor.add_actor(appsText);

        let menuY = appsText.y + appsText.height + 6;
        this._appdisplay = new AppDisplay.AppDisplay(width, global.screen_height - menuY);
        this._appdisplay.actor.x = SIDESHOW_PAD;
        this._appdisplay.actor.y = menuY;
        this.actor.add_actor(this._appdisplay.actor);

        /* Proxy the activated signal */
        this._appdisplay.connect('activated', function(appdisplay) {
          me.emit('activated');
        });
    },

    show: function() {
          this._appdisplay.show();
    },

    hide: function() {
    }
};
Signals.addSignalMethods(Sideshow.prototype);

function Overlay() {
    this._init();
}

Overlay.prototype = {
    _init : function() {
        let me = this;

        let global = Shell.Global.get();

        this._group = new Clutter.Group();
        this.visible = false;

        let background = new Clutter.Rectangle({ color: OVERLAY_BACKGROUND_COLOR,
                                                 reactive: true,
                                                 x: 0,
                                                 y: Panel.PANEL_HEIGHT,
                                                 width: global.screen_width,
                                                 height: global.screen_width - Panel.PANEL_HEIGHT });
        this._group.add_actor(background);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._recalculateSize();

        this._desktops = new Desktops.Desktops(this._desktopX, this._desktopY,
                                               this._desktopWidth, this._desktopHeight);
        this._group.add_actor(this._desktops._group);

        this._sideshow = new Sideshow(this._group, this._desktopX - 10);
        this._sideshow.connect('activated', function(sideshow) {
            // TODO - have some sort of animation/effect while
            // transitioning to the new app.  We definitely need
            // startup-notification integration at least.
            me._deactivate();
        });
    },

    _recalculateSize: function() {
        let global = Shell.Global.get();
        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height;

        // The desktop windows are shown on top of a scaled down version of the
        // desktop. This is positioned at the right side of the screen
        this._desktopWidth = screenWidth * DESKTOP_SCALE;
        this._desktopHeight = screenHeight * DESKTOP_SCALE;
        this._desktopX = screenWidth - this._desktopWidth - 10;
        this._desktopY = Panel.PANEL_HEIGHT + (screenHeight - this._desktopHeight - Panel.PANEL_HEIGHT) / 2;
    },

    show : function() {
        if (this.visible)
            return;

        this.visible = true;

        let global = Shell.Global.get();

        this._recalculateSize();

        this._sideshow.show();
        this._desktops.show();
        this._desktops._group.raise_top();

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the overlay is displayed greatly
        // increases performance of the overlay especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the overlay rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();
    },

    hide : function() {
        if (!this.visible)
            return;

        this._desktops.hide();
        this._sideshow.hide();

        // Dummy tween, just waiting for the workspace animation
        Tweener.addTween(this,
                         { time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });
    },
    
    _hideDone: function() {
        let global = Shell.Global.get();

        this.visible = false;
        global.window_group.show();
        this._group.lower_bottom();
        this._group.hide();
        this._desktops.hideDone();
    },

    _deactivate : function() {
        Main.hide_overlay();
    }
};
