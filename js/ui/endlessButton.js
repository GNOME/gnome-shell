// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported EndlessButton */

const { Clutter, Gio, GObject, St } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ViewSelector = imports.ui.viewSelector;

var EndlessButton = GObject.registerClass(
class EndlessButton extends PanelMenu.SingleIconButton {
    _init() {
        super._init(_('Endless Button'));
        this.add_style_class_name('endless-button');
        this.connect('style-changed', () => {
            this.width = this.get_theme_node().get_length('width');
            this.height = this.get_theme_node().get_length('height');
        });
        this.connect('notify::hover', this._onHoverChanged.bind(this));
        this.connect('button-press-event', this._onButtonPressEvent.bind(this));
        this.connect('button-release-event', this._onButtonReleaseEvent.bind(this));

        let iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/endless-button-symbolic.svg');
        this.setIcon(new Gio.FileIcon({ file: iconFile }));

        this._setupTooltipText();
    }

    _setupTooltipText() {
        this._label = new St.Label({ style_class: 'app-icon-hover-label' });

        this._labelOffsetX = 0;
        this._labelOffsetY = 0;
        this._label.connect('style-changed', () => {
            this._labelOffsetX = this._label.get_theme_node().get_length('-label-offset-x');
            this._labelOffsetY = this._label.get_theme_node().get_length('-label-offset-y');
        });

        let pageChangedId = Main.overview.connect('page-changed', this._onOverviewPageChanged.bind(this));
        let showingId = Main.overview.connect('showing', this._onOverviewShowing.bind(this));
        let hidingId = Main.overview.connect('hiding', this._onOverviewHiding.bind(this));

        this.connect('destroy', () => {
            Main.overview.disconnect(pageChangedId);
            Main.overview.disconnect(showingId);
            Main.overview.disconnect(hidingId);
        });

        this._setHoverLabelText(true);
    }

    _updateHoverLabel(hiding) {
        let viewSelector = Main.overview.viewSelector;
        let showingDesktop = true;

        if (!hiding &&
            viewSelector &&
            viewSelector.getActivePage() === ViewSelector.ViewPage.APPS)
            showingDesktop = false;

        this._setHoverLabelText(showingDesktop);
    }

    _setHoverLabelText(desktop) {
        if (desktop)
            this._label.text = _('Show Desktop');
        else
            this._label.text = _('Show Apps');
    }

    _onOverviewPageChanged() {
        this._updateHoverLabel(false);
    }

    _onOverviewShowing() {
        this._updateHoverLabel(false);
    }

    _onOverviewHiding() {
        this._updateHoverLabel(true);
    }

    // overrides default implementation from PanelMenu.Button
    vfunc_event(event) {
        if (this.menu &&
            (event.type() === Clutter.EventType.TOUCH_BEGIN ||
             event.type() === Clutter.EventType.BUTTON_PRESS))
            Main.overview.toggleApps();

        return Clutter.EVENT_PROPAGATE;
    }

    _onHoverChanged() {
        if (!this._label)
            return;

        if (this.hover) {
            if (this._label.get_parent())
                return;

            Main.uiGroup.add_actor(this._label);
            Main.uiGroup.set_child_above_sibling(this._label, null);

            // Update the tooltip position
            let monitor = Main.layoutManager.findMonitorForActor(this._label);
            let iconMidpoint = this.get_transformed_position()[0] + this.width / 2;
            this._label.translation_x = Math.floor(iconMidpoint - this._label.width / 2) + this._labelOffsetX;
            this._label.translation_y = Math.floor(this.get_transformed_position()[1] - this._labelOffsetY);

            // Clip left edge to be the left edge of the screen
            this._label.translation_x = Math.max(this._label.translation_x, monitor.x + this._labelOffsetX);
        } else if (this._label.get_parent()) {
            // Remove the tooltip from uiGroup
            Main.uiGroup.remove_actor(this._label);
        }
    }

    _onButtonPressEvent() {
        // This is the CSS active state
        this.add_style_pseudo_class('clicked');
        return Clutter.EVENT_PROPAGATE;
    }

    _onButtonReleaseEvent() {
        this.remove_style_pseudo_class('clicked');
        return Clutter.EVENT_PROPAGATE;
    }
});
