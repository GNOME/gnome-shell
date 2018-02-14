// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { GObject } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;

const Main = imports.ui.main;
const SideComponent = imports.ui.sideComponent;

const DISCOVERY_FEED_NAME = 'com.endlessm.DiscoveryFeed';
const DISCOVERY_FEED_PATH = '/com/endlessm/DiscoveryFeed';

const DiscoveryFeedIface = loadInterfaceXML('com.endlessm.DiscoveryFeed');

var DiscoveryFeed = GObject.registerClass(
class DiscoveryFeed extends SideComponent.SideComponent {
    _init() {
        super._init(DiscoveryFeedIface, DISCOVERY_FEED_NAME, DISCOVERY_FEED_PATH);
    }

    enable() {
        super.enable();
        Main.discoveryFeed = this;
    }

    disable() {
        super.disable();
        Main.discoveryFeed = null;
    }

    notifyHideAnimationCompleted() {
        this.proxy.notifyHideAnimationCompletedRemote();
    }

    callShow(timestamp) {
        this.proxy.showRemote(timestamp);
    }

    callHide(timestamp) {
        this.proxy.hideRemote(timestamp);
    }
});
var Component = DiscoveryFeed;
