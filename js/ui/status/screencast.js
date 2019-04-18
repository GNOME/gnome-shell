// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GObject = imports.gi.GObject;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;

var Indicator = GObject.registerClass({
    GTypeName: 'Screencast_Indicator'
}, class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'media-record-symbolic';
        this._indicator.add_style_class_name('screencast-indicator');
        this._sync();

        Main.screencastService.connect('updated', this._sync.bind(this));
    }

    _sync() {
        this._indicator.visible = Main.screencastService.isRecording;
    }
});
