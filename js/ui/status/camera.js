import GObject from 'gi://GObject';
import Shell from 'gi://Shell';

import {SystemIndicator} from '../quickSettings.js';

export const Indicator = GObject.registerClass(
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
