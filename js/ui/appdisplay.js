/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Signals = imports.signals;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Pango = imports.gi.Pango;
const Gtk = imports.gi.Gtk;

const Tidy = imports.gi.Tidy;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

//TODO - move this into GConf once we're not a plugin anymore
//but have taken over metacity
//This list is taken from GNOME Online popular applications
//http://online.gnome.org/applications
//but with nautilus removed
const DEFAULT_APPLICATIONS = [
                              'mozilla-firefox.desktop',
                              'gnome-terminal.desktop',
                              'evolution.desktop',
                              'evince.desktop',
                              'gedit.desktop',
                              'mozilla-thunderbird.desktop',
                              'totem.desktop',
                              'gnome-file-roller.desktop',
                              'rhythmbox.desktop',
                              'epiphany.desktop',
                              'xchat.desktop',
                              'openoffice.org-1.9-writer.desktop',
                              'emacs.desktop',
                              'gnome-system-monitor.desktop',
                              'openoffice.org-1.9-calc.desktop',
                              'eclipse.desktop',
                              'openoffice.org-1.9-impress.desktop',
                              'vncviewer.desktop'
                              ];

const APPDISPLAY_NAME_COLOR = new Clutter.Color();
APPDISPLAY_NAME_COLOR.from_pixel(0xffffffff);
const APPDISPLAY_COMMENT_COLOR = new Clutter.Color();
APPDISPLAY_COMMENT_COLOR.from_pixel(0xffffffbb);
const APPDISPLAY_BACKGROUND_COLOR = new Clutter.Color();
APPDISPLAY_BACKGROUND_COLOR.from_pixel(0x000000ff);

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

	let icontheme = Gtk.IconTheme.get_default();

	this._group = new Clutter.Group({reactive: true,
		width: width,
		height: APPDISPLAY_HEIGHT});
	this._group.connect('button-press-event', function(group, e) {
		me.emit('activate');
		return true;
	});
	this._bg = new Clutter.Rectangle({ color: APPDISPLAY_BACKGROUND_COLOR,
		x: 0, y: 0 });
	this._group.add_actor(this._bg);

	this._icon = new Clutter.Texture({ width: 48, height: 48, x: 0, y: 0 });
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
	this._name = new Clutter.Label({ color: APPDISPLAY_NAME_COLOR,
		font_name: "Sans 14px",
		width: text_width,
		ellipsize: Pango.EllipsizeMode.END,
		text: name,
		x: this._icon.width + 4,
		y: 0});
	this._group.add_actor(this._name);
	this._comment = new Clutter.Label({ color: APPDISPLAY_COMMENT_COLOR,
		font_name: "Sans 12px",
		width: text_width,
		ellipsize: Pango.EllipsizeMode.END,
		text: comment,
		x: this._name.x,
		y: this._name.height + 4})
	this._group.add_actor(this._comment);
	this.actor = this._group;
},
launch: function() {
    this._appinfo.launch([], null);
},
appinfo: function () {
    return this._appinfo;
}
}
Signals.addSignalMethods(AppDisplayItem.prototype);

function AppDisplay(x, y, width, height) {
	this._init(x, y, width, height);
}

AppDisplay.prototype = {
_init : function(x, y, width, height) {
	let me = this;
	let global = Shell.Global.get();
	this._search = '';
	this._x = x;
	this._y = y;
	this._width = width;
	this._height = height;
	this._appmonitor = new Shell.AppMonitor();
	this._appsStale = true;
	this._appmonitor.connect('changed', function(mon) {
		me._appsStale = true;
	});
	this._grid = new Tidy.Grid({x: x, y: y, width: width, height: height});
	global.stage.add_actor(this._grid);
	this._appset = {}; // Map<appid, appinfo>
	this._displayed = {} // Map<appid, AppDisplay>
	this._max_items = this._height / (APPDISPLAY_HEIGHT + APPDISPLAY_PADDING);
},

_refreshCache: function() {
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
	this._appsStale = false;
},

_removeItem: function(appid) {
	let item = this._displayed[appid];
	let group = item.actor;
	group.destroy();
	delete this._displayed[appid];
	
},

_removeAll: function() {
	for (appid in this._displayed)
		this._removeItem(appid);
},

_setDefaultList: function() {
	this._removeAll();
	let added = 0;
	for (let i = 0; i < DEFAULT_APPLICATIONS.length && added < this._max_items; i++) {
		let appid = DEFAULT_APPLICATIONS[i];
		let appinfo = this._appset[appid];
		if (appinfo) {
			this._filterAdd(appid);
			added += 1;
		}
	}
},

_filterAdd: function(appid) {
	let me = this;

	let appinfo = this._appset[appid];
	let name = appinfo.get_name();
	let index = 0; for (i in this._displayed) { index += 1; };

	let appdisplay = new AppDisplayItem(appinfo, this._width);
	appdisplay.connect('activate', function() {
	    appdisplay.launch();
		me.emit('activated');
	});
	let group = appdisplay.actor;
	this._grid.add_actor(group);
	this._displayed[appid] = appdisplay;
},

_filterRemove: function(appid) {
	// In the future, do some sort of fade out or other effect here
	let item = this._displayed[appid];
	this._removeItem(item);
},

_appinfoMatches: function(appinfo, search) {
	if (search == null || search == '')
		return true;
	let name = appinfo.get_name().toLowerCase();
	if (name.indexOf(search) >= 0)
		return true;
	let description = appinfo.get_description();
	if (description) {
		description = description.toLowerCase();
		if (description.indexOf(search) >= 0)
			return true;
	}
	let exec = appinfo.get_executable().toLowerCase();
	if (exec.indexOf(search) >= 0)
		return true;
	return false;
},

_sortApps: function(appids) {
	let me = this;
	return appids.sort(function (a,b) {
		let appA = me._appset[a];
		let appB = me._appset[b];
		return appA.get_name().localeCompare(appB.get_name());
	});
},

_doSearchFilter: function() {
	this._removeAll();
	let matchedApps = [];
	for (appid in this._appset) {
		if (matchedApps.length >= this._max_items)
			break;
		if (appid in this._displayed)
			continue;
		let app = this._appset[appid];
		if (this._appinfoMatches(app, this._search))
			matchedApps.push(appid);
	}
	this._sortApps(matchedApps);
	for (let i = 0; i < matchedApps.length; i++) {
		this._filterAdd(matchedApps[i]);
	}
},

_redisplay: function() {
	this._refreshCache();
	if (!this._search)
		this._setDefaultList();
	else
		this._doSearchFilter();
},

setSearch: function(text) {
	this._search = text.toLowerCase();
	this._redisplay();
},

searchActivate: function() {
    let displayedActors = this._grid.get_children();
    if (displayedActors.length != 1)
        return;
    let selectedActor = displayedActors[0];
    let selectedMenuItem = null;
    for (appid in this._displayed) {
        let item = this._displayed[appid];
        if (item.actor == selectedActor) {
            selectedMenuItem = item;
            break;
        }
    }
    log("selected " + selectedMenuItem);    
    if (!selectedMenuItem)
        return;
    
    selectedMenuItem.launch();
    this.emit('activated');
},

show: function() {
	this._redisplay();
	this._grid.show();
},

hide: function() {
	this._grid.hide();
}
}
Signals.addSignalMethods(AppDisplay.prototype);

