// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'media-record-symbolic';
        this._indicator.add_style_class_name('screencast-indicator');
        this._sync();

        Main.screencastService.connect('updated', this._sync.bind(this));
    }

    _sync() {
        this._indicator.visible = Main.screencastService.isRecording;
    }
};
