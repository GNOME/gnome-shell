// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const GObject = imports.gi.GObject;
const Shell = imports.gi.Shell;

const {SystemIndicator} = imports.ui.quickSettings;

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    constructor() {
        super();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'camera-web-symbolic';
        this._indicator.add_style_class_name('privacy-indicator');

        this._cameraMonitor = new Shell.CameraMonitor();
        this._cameraMonitor.bind_property('cameras-in-use', this._indicator,
            'visible', GObject.BindingFlags.SYNC_CREATE);
    }
});
