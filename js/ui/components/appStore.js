// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Component */

const { GObject } = imports.gi;

const Main = imports.ui.main;
const SideComponent = imports.ui.sideComponent;
const ViewSelector = imports.ui.viewSelector;

const { loadInterfaceXML } = imports.misc.fileUtils;

const APP_STORE_NAME = 'com.endlessm.AppStore';
const APP_STORE_PATH = '/com/endlessm/AppStore';

const AppStoreIface = loadInterfaceXML('com.endlessm.AppStore');

var AppStore = GObject.registerClass(
class AppStore extends SideComponent.SideComponent {
    _init() {
        super._init(AppStoreIface, APP_STORE_NAME, APP_STORE_PATH);
    }

    enable() {
        super.enable();
        Main.appStore = this;
    }

    disable() {
        super.disable();
        Main.appStore = null;
    }

    toggle(reset) {
        reset = !!reset;
        super.toggle(reset);
    }

    callShow(timestamp, reset) {
        this.proxy.showRemote(timestamp, reset);
    }

    callHide(timestamp) {
        this.proxy.hideRemote(timestamp);
    }

    showPage(timestamp, page) {
        if (!this._visible) {
            this._launchedFromDesktop =
                Main.overview.visible &&
                Main.overview.getActivePage() === ViewSelector.ViewPage.APPS;
        }

        this.proxy.showPageRemote(timestamp, page);
    }
});
var Component = AppStore;
