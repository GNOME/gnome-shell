// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported OsdWindowManager */

const { Clutter, GLib, GObject, Meta, St } = imports.gi;

const BarLevel = imports.ui.barLevel;
const Layout = imports.ui.layout;
const Main = imports.ui.main;

var HIDE_TIMEOUT = 1500;
var FADE_TIME = 100;
var LEVEL_ANIMATION_TIME = 100;

var OsdWindowConstraint = GObject.registerClass(
class OsdWindowConstraint extends Clutter.Constraint {
    _init(props) {
        this._minSize = 0;
        super._init(props);
    }

    set minSize(v) {
        this._minSize = v;
        if (this.actor)
            this.actor.queue_relayout();
    }

    vfunc_update_allocation(actor, actorBox) {
        // Clutter will adjust the allocation for margins,
        // so add it to our minimum size
        let minSize = this._minSize + actor.margin_top + actor.margin_bottom;
        let [width, height] = actorBox.get_size();

        // Enforce a ratio of 1
        let size = Math.ceil(Math.max(minSize, height));
        actorBox.set_size(size, size);

        // Recenter
        let [x, y] = actorBox.get_origin();
        actorBox.set_origin(Math.ceil(x + width / 2 - size / 2),
                            Math.ceil(y + height / 2 - size / 2));
    }
});

var OsdWindow = GObject.registerClass(
class OsdWindow extends St.Widget {
    _init(monitorIndex) {
        super._init({
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._monitorIndex = monitorIndex;
        let constraint = new Layout.MonitorConstraint({ index: monitorIndex });
        this.add_constraint(constraint);

        this._boxConstraint = new OsdWindowConstraint();
        this._box = new St.BoxLayout({ style_class: 'osd-window',
                                       vertical: true });
        this._box.add_constraint(this._boxConstraint);
        this.add_actor(this._box);

        this._icon = new St.Icon({ y_expand: true });
        this._box.add_child(this._icon);

        this._label = new St.Label();
        this._box.add(this._label);

        this._level = new BarLevel.BarLevel({
            style_class: 'level',
            value: 0,
        });
        this._box.add(this._level);

        this._hideTimeoutId = 0;
        this._reset();

        this.connect('destroy', this._onDestroy.bind(this));

        this._monitorsChangedId =
            Main.layoutManager.connect('monitors-changed',
                                       this._relayout.bind(this));
        let themeContext = St.ThemeContext.get_for_stage(global.stage);
        this._scaleChangedId =
            themeContext.connect('notify::scale-factor',
                                 this._relayout.bind(this));
        this._relayout();
        Main.uiGroup.add_child(this);
    }

    _onDestroy() {
        if (this._monitorsChangedId)
            Main.layoutManager.disconnect(this._monitorsChangedId);
        this._monitorsChangedId = 0;

        let themeContext = St.ThemeContext.get_for_stage(global.stage);
        if (this._scaleChangedId)
            themeContext.disconnect(this._scaleChangedId);
        this._scaleChangedId = 0;
    }

    setIcon(icon) {
        this._icon.gicon = icon;
    }

    setLabel(label) {
        this._label.visible = label != undefined;
        if (label)
            this._label.text = label;
    }

    setLevel(value) {
        this._level.visible = value != undefined;
        if (value != undefined) {
            if (this.visible) {
                this._level.ease_property('value', value, {
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    duration: LEVEL_ANIMATION_TIME,
                });
            } else {
                this._level.value = value;
            }
        }
    }

    setMaxLevel(maxLevel = 1) {
        this._level.maximum_value = maxLevel;
    }

    show() {
        if (!this._icon.gicon)
            return;

        if (!this.visible) {
            Meta.disable_unredirect_for_display(global.display);
            super.show();
            this.opacity = 0;
            this.get_parent().set_child_above_sibling(this, null);

            this.ease({
                opacity: 255,
                duration: FADE_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        }

        if (this._hideTimeoutId)
            GLib.source_remove(this._hideTimeoutId);
        this._hideTimeoutId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT, HIDE_TIMEOUT, this._hide.bind(this));
        GLib.Source.set_name_by_id(this._hideTimeoutId, '[gnome-shell] this._hide');
    }

    cancel() {
        if (!this._hideTimeoutId)
            return;

        GLib.source_remove(this._hideTimeoutId);
        this._hide();
    }

    _hide() {
        this._hideTimeoutId = 0;
        this.ease({
            opacity: 0,
            duration: FADE_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._reset();
                Meta.enable_unredirect_for_display(global.display);
            },
        });
        return GLib.SOURCE_REMOVE;
    }

    _reset() {
        super.hide();
        this.setLabel(null);
        this.setMaxLevel(null);
        this.setLevel(null);
    }

    _relayout() {
        /* assume 110x110 on a 640x480 display and scale from there */
        let monitor = Main.layoutManager.monitors[this._monitorIndex];
        if (!monitor)
            return; // we are about to be removed

        let scalew = monitor.width / 640.0;
        let scaleh = monitor.height / 480.0;
        let scale = Math.min(scalew, scaleh);
        let popupSize = 110 * Math.max(1, scale);

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._icon.icon_size = popupSize / (2 * scaleFactor);
        this._box.translation_y = Math.round(monitor.height / 4);
        this._boxConstraint.minSize = popupSize;
    }
});

var OsdWindowManager = class {
    constructor() {
        this._osdWindows = [];
        Main.layoutManager.connect('monitors-changed',
                                   this._monitorsChanged.bind(this));
        this._monitorsChanged();
    }

    _monitorsChanged() {
        for (let i = 0; i < Main.layoutManager.monitors.length; i++) {
            if (this._osdWindows[i] == undefined)
                this._osdWindows[i] = new OsdWindow(i);
        }

        for (let i = Main.layoutManager.monitors.length; i < this._osdWindows.length; i++) {
            this._osdWindows[i].destroy();
            this._osdWindows[i] = null;
        }

        this._osdWindows.length = Main.layoutManager.monitors.length;
    }

    _showOsdWindow(monitorIndex, icon, label, level, maxLevel) {
        this._osdWindows[monitorIndex].setIcon(icon);
        this._osdWindows[monitorIndex].setLabel(label);
        this._osdWindows[monitorIndex].setMaxLevel(maxLevel);
        this._osdWindows[monitorIndex].setLevel(level);
        this._osdWindows[monitorIndex].show();
    }

    show(monitorIndex, icon, label, level, maxLevel) {
        if (monitorIndex != -1) {
            for (let i = 0; i < this._osdWindows.length; i++) {
                if (i == monitorIndex)
                    this._showOsdWindow(i, icon, label, level, maxLevel);
                else
                    this._osdWindows[i].cancel();
            }
        } else {
            for (let i = 0; i < this._osdWindows.length; i++)
                this._showOsdWindow(i, icon, label, level, maxLevel);
        }
    }

    hideAll() {
        for (let i = 0; i < this._osdWindows.length; i++)
            this._osdWindows[i].cancel();
    }
};
