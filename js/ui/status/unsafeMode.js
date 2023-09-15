import GObject from 'gi://GObject';

import {SystemIndicator} from '../quickSettings.js';

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'channel-insecure-symbolic';

        global.context.bind_property('unsafe-mode',
            this._indicator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
    }
});
