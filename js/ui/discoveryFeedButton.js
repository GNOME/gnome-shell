// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported maybeCreateInactiveButton, maybeCreateButton, determineAllocationWithinBox */

const { Clutter, GLib, GObject, St } = imports.gi;

const Main = imports.ui.main;

function maybeCreateInactiveButton() {
    if (_checkIfDiscoveryFeedEnabled()) {
        let discoveryFeed = new DiscoveryFeedButton();
        discoveryFeed.reactive = false;
        return discoveryFeed;
    }

    return null;
}

const DISCOVERY_FEED_PRIMARY_MONITOR_WIDTH_THRESHOLD = 1024;

function _primaryMonitorWidthPassesThreshold() {
    return Main.layoutManager.primaryMonitor.width >= DISCOVERY_FEED_PRIMARY_MONITOR_WIDTH_THRESHOLD;
}

function _checkIfDiscoveryFeedEnabled() {
    let supportedLanguages = global.settings.get_value('discovery-feed-languages').deep_unpack();
    let systemLanguages = GLib.get_language_names();

    let isEnabled = supportedLanguages.some(lang => systemLanguages.indexOf(lang) !== -1);

    return isEnabled;
}

function maybeCreateButton() {
    if (_checkIfDiscoveryFeedEnabled())
        return new DiscoveryFeedButton();

    return null;
}

/** DiscoveryFeedButton:
 *
 * This class handles the button to launch the discovery feed application
 */
var DiscoveryFeedButton = GObject.registerClass(
class DiscoveryFeedButton extends St.BoxLayout {
    _init() {
        super._init({
            vertical: true,
            visible: _primaryMonitorWidthPassesThreshold(),
        });

        this._bar = new St.Button({
            name: 'discovery-feed-bar',
            style_class: 'discovery-feed-bar',
        });
        this.add_child(this._bar);

        this._tile = new St.Button({
            name: 'discovery-feed-tile',
            style_class: 'discovery-feed-tile',
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._tile);

        this._bar.connect('clicked', () => {
            Main.discoveryFeed.show(global.get_current_time());
        });
        this._tile.connect('clicked', () => {
            Main.discoveryFeed.show(global.get_current_time());
        });

        this._bar.connect('notify::hover', this._onHoverChanged.bind(this));
        this._tile.connect('notify::hover', this._onHoverChanged.bind(this));

        Main.layoutManager.connect('monitors-changed', () => {
            this.visible = _primaryMonitorWidthPassesThreshold();
        });
    }

    _onHoverChanged(actor) {
        if (actor.get_hover()) {
            this._bar.add_style_pseudo_class('highlighted');
            this._tile.add_style_pseudo_class('highlighted');
        } else {
            this._bar.remove_style_pseudo_class('highlighted');
            this._tile.remove_style_pseudo_class('highlighted');
        }
    }

    changeVisbilityState(value) {
        // Helper function to ensure that visibility is set correctly,
        // consumers of this button should use this function as opposed
        // to mutating 'visible' directly, since it prevents the
        // button from appearing in cases where it should not.
        this.visible = value && _primaryMonitorWidthPassesThreshold();
    }
});

function determineAllocationWithinBox(discoveryFeedButton, box, availWidth) {
    // If we would not show the feed button because the monitor
    // is too small, just return box directly
    if (!_primaryMonitorWidthPassesThreshold())
        return box;

    let discoveryFeedButtonHeight = discoveryFeedButton.get_preferred_height(availWidth)[1];
    let discoveryFeedButtonBox = box.copy();
    let x1 = (availWidth - discoveryFeedButton.get_width()) * 0.5;
    discoveryFeedButtonBox.y1 = 0;
    discoveryFeedButtonBox.y2 = discoveryFeedButtonBox.y1 + discoveryFeedButtonHeight;
    discoveryFeedButtonBox.x1 = x1;
    discoveryFeedButtonBox.x2 = x1 + discoveryFeedButton.get_width();
    return discoveryFeedButtonBox;
}
