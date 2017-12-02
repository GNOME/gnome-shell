// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;

var Indicator = new Lang.Class({
    Name: 'ScreencastIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'media-record-symbolic';
        this._indicator.add_style_class_name('screencast-indicator');
        this._sync();

        Main.screencastService.connect('updated', this._sync.bind(this));
    },

    _sync() {
        this._indicator.visible = Main.screencastService.isRecording;
    },
});
