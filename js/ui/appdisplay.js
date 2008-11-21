/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Signals = imports.signals;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Pango = imports.gi.Pango;
const Gtk = imports.gi.Gtk;

const Tidy = imports.gi.Tidy;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const MENU_NAME_COLOR = new Clutter.Color();
MENU_NAME_COLOR.from_pixel(0xffffffff);
const MENU_COMMENT_COLOR = new Clutter.Color();
MENU_COMMENT_COLOR.from_pixel(0xffffffbb);
const MENU_BACKGROUND_COLOR = new Clutter.Color();
MENU_BACKGROUND_COLOR.from_pixel(0x000000ff);

const APPDISPLAY_HEIGHT = 50;
const APPDISPLAY_PADDING = 4;

function AppDisplayItem(node, width) {
    this._init(node, width);
}

AppDisplayItem.prototype = {
    _init: function(appinfo, width) {
	let me = this;
        this._appinfo = appinfo;

	let name = appinfo.get_name();

	let icontheme = Gtk.icon_theme_get_default();

	this._group = new Clutter.Group({reactive: true,
					 width: width,
					 height: APPDISPLAY_HEIGHT});
	this._bg = new Clutter.Rectangle({ color: MENU_BACKGROUND_COLOR,
	 				   reactive: true });
	this._group.add_actor(this._bg);
	this._bg.connect('button-press-event', function(group, e) {
	    me.emit('launch');
	    return true;
	});

        this._icon = new Clutter.Texture({ width: 48, height: 48 });
	let gicon = appinfo.get_icon();
	let path = null;
        if (gicon != null) {
            let iconinfo = icontheme.lookup_by_gicon(gicon, 48, Gtk.IconLookupFlags.NO_SVG);
	    if (iconinfo)
	    	path = iconinfo.get_filename();
        }

	if (path)
	    this._icon.set_from_file(path);
	this._group.add_actor(this._icon);

	let comment = appinfo.get_description();
	let text_width = width - me._icon.width + 4;
	this._name = new Clutter.Label({ color: MENU_NAME_COLOR,
                                     font_name: "Sans 14px",
                                     width: text_width,
                                     ellipsize: Pango.EllipsizeMode.END,
		     		     text: name});
	this._group.add_actor(this._name);
	this._comment = new Clutter.Label({ color: MENU_COMMENT_COLOR,
                                             font_name: "Sans 12px",
                                             width: text_width,
                                             ellipsize: Pango.EllipsizeMode.END,
		     		             text: comment})
	this._group.add_actor(this._comment);

	this._group.connect("notify::allocation", function (grp, prop) {
            let x = me._group.x;
            let y = me._group.y;
	    let width = me._group.width;
	    let height = me._group.height;
	    me._bg.set_position(x, y);
            me._icon.set_position(x, y);
	    let text_x = x + me._icon.width + 4;
	    me._name.set_position(text_x, y);
            me._comment.set_position(text_x, y + me._name.get_height() + 4);
        });

        this.actor = this._group;
    }
}
Signals.addSignalMethods(AppDisplayItem.prototype);

function AppDisplay(x, y, width, height) {
    this._init(x, y, width, height);
}

AppDisplay.prototype = {
    _init : function(x, y, width, height) {
	let me = this;
	let global = Shell.global_get();
        this._x = x;
	this._y = y;
	this._width = width;
	this._height = height;
	this._appmonitor = new Shell.AppMonitor();
	this._appsStale = true;
	this._appmonitor.connect('changed', function(mon) {
            me._appsStale = true;
	});
        this._grid = new Tidy.Grid({x: x, y: y, width: width, height: height,
                                    column_major: true,
		     	 	    column_gap: APPDISPLAY_PADDING });
	global.stage.add_actor(this._grid);
        this._appset = {}; // Map<appid, appinfo>
	this._displayed = {} // Map<appid, AppDisplay>
	this._max_items = this._height / (APPDISPLAY_HEIGHT + APPDISPLAY_PADDING);
    },

    _refresh: function() {
        let me = this;

        if (!this._appsStale)
	    return;
        for (id in this._displayed)
            this._displayed[id].destroy();
	this._appset = {};
	this._displayed = {};
        let apps = Gio.app_info_get_all();
	let i = 0;
	for (i = 0; i < apps.length; i++) {
	    let appinfo = apps[i];
	    let appid = appinfo.get_id();
	    this._appset[appid] = appinfo;
        }
	for (i = 0; i < apps.length && i < this._max_items; i++) {
	    let appinfo = apps[i];
	    let appid = appinfo.get_id();
	    this._filterAdd(appid);
        }
        this._appsStale = false;
    },

    _filterAdd: function(appid) {
	let me = this;

        let appinfo = this._appset[appid];
	let name = appinfo.get_name();
	let index = 0; for (i in this._displayed) { index += 1; }

        let appdisplay = new AppDisplayItem(appinfo, this._width);
	appdisplay.connect('launch', function() {
	    appinfo.launch([], null);
	    me.emit('activated');
        });
	let group = appdisplay.actor;
        this._grid.add_actor(group);
	this._displayed[appid] = appdisplay;
    },

    _filterRemove: function(appid) {
        let item = this._displayed[appid];
	let group = item.actor;
	group.destroy();
	delete this._displayed[appid];
    },

    _appinfoMatches: function(appinfo, search) {
        if (search == null || search == '')
            return true;
        let name = appinfo.get_name().toLowerCase();
        let description = appinfo.get_description();
        if (description) description = description.toLowerCase();
	if (name.indexOf(search) >= 0)
	    return true;
	if (description && description.indexOf(search) >= 0)
	    return true;
	return false;
    },

    _doSearchFilter: function() {
        let c = 0;
        for (appid in this._displayed) {
            let app = this._appset[appid];
            if (!this._appinfoMatches(app, this._search))
                this._filterRemove(appid);
            else
                c += 1;
        }
        for (appid in this._appset) {
	    if (c >= this._max_items)
	        break;
            if (appid in this._displayed)
                continue;
            let app = this._appset[appid];
            if (this._appinfoMatches(app, this._search)) {
                this._filterAdd(appid);
                c += 1;
            }
        }
    },

    setSearch: function(text) {
        this._search = text.toLowerCase();
        this._doSearchFilter();
    },

    show: function() {
        this._refresh();
        this._grid.show();
    },

    hide: function() {
    	this._grid.hide();
    }
}
Signals.addSignalMethods(AppDisplay.prototype);

