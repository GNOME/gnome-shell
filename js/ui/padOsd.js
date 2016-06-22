// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Rsvg = imports.gi.Rsvg;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Gio = imports.gi.Gio;
const GDesktopEnums = imports.gi.GDesktopEnums;
const Atk = imports.gi.Atk;
const Cairo = imports.cairo;
const Signals = imports.signals;

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Layout = imports.ui.layout;

const ACTIVE_COLOR = "#729fcf";

const LTR = 0;
const RTL = 1;

const CW = 0;
const CCW = 1;

const UP = 0;
const DOWN = 1;

const KeybindingEntry = new Lang.Class({
    Name: 'KeybindingEntry',

    _init: function () {
        this.actor = new St.Entry({ hint_text: _('New shortcut...'),
                                    width: 120 });
        this.actor.connect('captured-event', Lang.bind(this, this._onCapturedEvent));
        this.actor.connect('destroy', Lang.bind(this, this.destroy));
    },

    _onCapturedEvent: function (actor, event) {
        if (event.type() == Clutter.EventType.KEY_PRESS) {
            if (GLib.unichar_isprint(event.get_key_unicode())) {
                let str = Gtk.accelerator_name_with_keycode(null,
                                                            event.get_key_symbol(),
                                                            event.get_key_code(),
                                                            event.get_state());
                this.actor.set_text(str);
                this.emit('keybinding', str);
            }
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    destroy: function () {
        this.actor.destroy();
    }
});
Signals.addSignalMethods(KeybindingEntry.prototype);

const ActionComboBox = new Lang.Class({
    Name: 'ActionComboBox',

    _init: function () {
        this.actor = new St.Button({ style_class: 'button' });
        this.actor.connect('clicked', Lang.bind(this, this._onButtonClicked));
        this.actor.set_toggle_mode(true);

        let boxLayout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.HORIZONTAL,
                                                spacing: 6 });
        let box = new St.Widget({ layout_manager: boxLayout });
        this.actor.set_child(box);

        this._label = new St.Label({ width: 150 });
        box.add_child(this._label)

        let arrow = new St.Icon({ style_class: 'popup-menu-arrow',
                                  icon_name: 'pan-down-symbolic',
                                  accessible_role: Atk.Role.ARROW,
                                  y_expand: true,
                                  y_align: Clutter.ActorAlign.CENTER });
        box.add_child(arrow);

        /* Order matches GDesktopPadButtonAction enum */
        this._actions = [_('Application defined'),
                         _('Show on-screen help'),
                         _('Switch monitor'),
                         _('Assign keystroke')];

        this._editMenu = new PopupMenu.PopupMenu(this.actor, 0, St.Side.TOP);
        this._editMenu.connect('menu-closed', Lang.bind(this, function() { this.actor.set_checked(false); }));
        this._editMenu.actor.hide();
        Main.uiGroup.add_actor(this._editMenu.actor);

        for (let i = 0; i < this._actions.length; i++) {
            let str = this._actions[i];
            let action = i;
            this._editMenu.addAction(str, Lang.bind(this, function() { this._onActionSelected(action) }));
        }

        this.setAction(GDesktopEnums.PadButtonAction.NONE);
    },

    _onActionSelected: function (action) {
        this.setAction(action);
        this.popdown();
        this.emit('action', action);
    },

    setAction: function (action) {
        this._label.set_text(this._actions[action]);
    },

    popup: function () {
        this._editMenu.open(true);
    },

    popdown: function () {
        this._editMenu.close(true);
    },

    _onButtonClicked: function () {
        if (this.actor.get_checked())
            this.popup();
        else
            this.popdown();
    }
});
Signals.addSignalMethods(ActionComboBox.prototype);

const ActionEditor = new Lang.Class({
    Name: 'ActionEditor',

    _init: function () {
        let boxLayout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.HORIZONTAL,
                                                spacing: 12 });

        this.actor = new St.Widget({ layout_manager: boxLayout });

        this._actionComboBox = new ActionComboBox();
        this._actionComboBox.connect('action', Lang.bind(this, this._onActionSelected));
        this.actor.add_actor(this._actionComboBox.actor);

        this._keybindingEdit = new KeybindingEntry();
        this._keybindingEdit.connect('keybinding', Lang.bind(this, this._onKeybindingEdited));
        this._keybindingEdit.actor.hide();
        this.actor.add_actor(this._keybindingEdit.actor);

        this._doneButton = new St.Button ({ label: _('Done'),
                                            width: 100,
                                            style_class: 'button'});
        this._doneButton.connect('clicked', Lang.bind(this, this._onEditingDone));
        this.actor.add_actor(this._doneButton);
    },

    setSettings: function (settings) {
        this._buttonSettings = settings;

        this._currentAction = this._buttonSettings.get_enum('action');
        this._currentKeybinding = this._buttonSettings.get_string('keybinding');
        this._actionComboBox.setAction (this._currentAction);

        if (this._currentAction == GDesktopEnums.PadButtonAction.KEYBINDING) {
            this._keybindingEdit.actor.set_text(this._currentKeybinding);
            this._keybindingEdit.actor.show();
        } else {
            this._keybindingEdit.actor.hide();
        }
    },

    close: function() {
        this._actionComboBox.popdown();
        this.actor.hide();
    },

    _onKeybindingEdited: function (entry, keybinding) {
        this._currentKeybinding = keybinding;
    },

    _onActionSelected: function (menu, action) {
        this._currentAction = action;

        if (action == GDesktopEnums.PadButtonAction.KEYBINDING) {
            this._keybindingEdit.actor.show();
            this._keybindingEdit.actor.grab_key_focus();
        } else {
            this._keybindingEdit.actor.hide();
        }
    },

    _storeSettings: function () {
        if (!this._buttonSettings)
            return;

        let keybinding = null;

        if (this._currentAction == GDesktopEnums.PadButtonAction.KEYBINDING)
            keybinding = this._currentKeybinding;

        this._buttonSettings.set_enum('action', this._currentAction);

        if (keybinding)
            this._buttonSettings.set_string('keybinding', keybinding);
        else
            this._buttonSettings.reset('keybinding');
    },

    _onEditingDone: function () {
        this._storeSettings();
        this.close();
        this.emit ('done');
    }
});
Signals.addSignalMethods(ActionEditor.prototype);

const PadDiagram = new Lang.Class({
    Name: 'PadDiagram',
    Extends: St.DrawingArea,

    _init: function (imagePath, leftHanded) {
        this.parent();

        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/pad-osd.css');
        let [success, css, etag] = file.load_contents(null);
        this._css = css;

        let originalHandle = Rsvg.Handle.new_from_file(imagePath);
        let dimensions = originalHandle.get_dimensions();
        this._imageWidth = dimensions.width;
        this._imageHeight = dimensions.height;

        this._activeButtons = [];
        this._imagePath = imagePath;
        this._handle = this._composeStyledDiagram();
        this.connect('repaint', Lang.bind(this, this._repaint));
        this.connect('notify::size', Lang.bind(this, this._updateScale));
        this._leftHanded = leftHanded;
    },

    _wrappingSvgHeader: function () {
        return ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>' +
                '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" ' +
                'xmlns:xi="http://www.w3.org/2001/XInclude" ' +
                'width="' + this._imageWidth + '" height="' + this._imageHeight + '"> ' +
                '<style type="text/css">');
    },

    _wrappingSvgFooter: function () {
        return ('</style>' +
                '<xi:include href="' + this._imagePath + '" />' +
                '</svg>');
    },

    _cssString: function () {
        let css = this._css;

        for (let i = 0; i < this._activeButtons.length; i++) {
            let ch = String.fromCharCode('A'.charCodeAt() + this._activeButtons[i]);
            css += ('.' + ch + ' { ' +
	            '  stroke: ' + ACTIVE_COLOR + ' !important; ' +
                    '  fill: ' + ACTIVE_COLOR + ' !important; ' +
                    '} ');
        }

        return css;
    },

    _composeStyledDiagram: function () {
        let svgData = '';

        if (!GLib.file_test(this._imagePath, GLib.FileTest.EXISTS))
            return null;

        svgData += this._wrappingSvgHeader();
        svgData += this._cssString();
        svgData += this._wrappingSvgFooter();

        let handle = new Rsvg.Handle();
        handle.set_base_uri (GLib.path_get_dirname (this._imagePath));
        handle.write(svgData);
        handle.close();

        return handle;
    },

    _updateScale: function () {
        let [width, height] = this.get_size();
        let dimensions = this._handle.get_dimensions ();
        let scaleX = width / dimensions.width;
        let scaleY = height / dimensions.height;
        this._scale = Math.min(scaleX, scaleY);
    },

    _repaint: function (area) {
        if (this._handle == null)
            return;

        let [width, height] = area.get_surface_size();
        let dimensions = this._handle.get_dimensions ();
        let cr = this.get_context();

        if (this._scale == null)
            this._updateScale();

        cr.save();
        cr.translate (width/2, height/2);
        cr.scale (this._scale, this._scale);
        if (this._leftHanded)
            cr.rotate(Math.PI);
        cr.translate (-dimensions.width/2, -dimensions.height/2);
        this._handle.render_cairo(cr);
        cr.restore();
        cr.$dispose();
    },

    _transformPoint: function (x, y) {
        if (this._handle == null || this._scale == null)
            return [x, y];

        // I miss Cairo.Matrix
        let [width, height] = this.get_size();
        let dimensions = this._handle.get_dimensions ();
        x = x * this._scale + width / 2 - dimensions.width / 2 * this._scale;
        y = y * this._scale + height / 2 - dimensions.height / 2 * this._scale;;
        return [Math.round(x), Math.round(y)];
    },

    _getItemLabelCoords: function (labelName, leaderName) {
        if (this._handle == null)
            return [false];

        let leaderPos, leaderSize, pos;
        let found, direction;

        [found, pos] = this._handle.get_position_sub('#' + labelName);
        if (!found)
            return [false];

        [found, leaderPos] = this._handle.get_position_sub('#' + leaderName);
        [found, leaderSize] = this._handle.get_dimensions_sub('#' + leaderName);
        if (!found)
            return [false];

        if (pos.x > leaderPos.x + leaderSize.width)
            direction = LTR;
        else
            direction = RTL;

        if (this._leftHanded) {
            direction = 1 - direction;
            pos.x = this._imageWidth - pos.x;
            pos.y = this._imageHeight - pos.y;
        }

        let [x, y] = this._transformPoint(pos.x, pos.y)

        return [true, x, y, direction];
    },

    getButtonLabelCoords: function (button) {
        let ch = String.fromCharCode('A'.charCodeAt() + button);
        let labelName = 'Label' + ch;
        let leaderName = 'Leader' + ch;

        return this._getItemLabelCoords(labelName, leaderName);
    },

    getRingLabelCoords: function (number, dir) {
        let numStr = number > 0 ? number.toString() : '';
        let dirStr = dir == CW ? 'CW' : 'CCW';
        let labelName = 'LabelRing' + numStr + dirStr;
        let leaderName = 'LeaderRing' + numStr + dirStr;

        return this._getItemLabelCoords(labelName, leaderName);
    },

    getStripLabelCoords: function (number, dir) {
        let numStr = number > 0 ? (number + 1).toString() : '';
        let dirStr = dir == UP ? 'Up' : 'Down';
        let labelName = 'LabelStrip' + numStr + dirStr;
        let leaderName = 'LeaderStrip' + numStr + dirStr;

        return this._getItemLabelCoords(labelName, leaderName);
    },

    _invalidateSvg: function () {
        if (this._handle == null)
            return;
        this._handle = this._composeStyledDiagram();
        this.queue_repaint();
    },

    activateButton: function (button) {
        this._activeButtons.push(button);
        this._invalidateSvg ();
    },

    deactivateButton: function (button) {
        for (let i = 0; i < this._activeButtons.length; i++) {
            if (this._activeButtons[i] == button)
                this._activeButtons.splice(i, 1);
        }
        this._invalidateSvg ();
    }
});

const PadOsd = new Lang.Class({
    Name: 'PadOsd',

    _init: function (padDevice, settings, imagePath, editionMode, monitorIndex) {
        this.padDevice = padDevice;
        this._settings = settings;
        this._imagePath = imagePath;
        this._editionMode = editionMode;
        this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));

        this.actor = new Shell.GenericContainer({ style_class: 'pad-osd-window',
                                                   reactive: true,
                                                   x: 0,
                                                   y: 0,
                                                   width: global.screen_width,
                                                   height: global.screen_height });
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor.connect('destroy', Lang.bind(this, this.destroy));
        Main.uiGroup.add_actor(this.actor);

        this._monitorIndex = monitorIndex;
        let constraint = new Layout.MonitorConstraint({ index: monitorIndex });
        this.actor.add_constraint(constraint);

        this._padDiagram = new PadDiagram(this._imagePath, settings.get_boolean('left-handed'));
        this.actor.add_actor(this._padDiagram);

        this._buttonBox = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                          x_expand: true,
                                          x_align: Clutter.ActorAlign.CENTER,
                                          y_align: Clutter.ActorAlign.CENTER });
        this._editButton = new St.Button({ label: _('Edit...'),
                                           style_class: 'button',
                                           can_focus: true,
                                           x_expand: true });
        this._editButton.connect('clicked', Lang.bind(this, function () { this.setEditionMode(true) }));
        this._buttonBox.add_actor(this._editButton);
        this.actor.add_actor(this._buttonBox);

        let boxLayout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.VERTICAL });
        this._labelBox = new St.Widget({ layout_manager: boxLayout,
                                         x_expand: true,
                                         x_align: Clutter.ActorAlign.CENTER,
                                         y_align: Clutter.ActorAlign.CENTER });
        this._titleLabel = new St.Label();
        this._titleLabel.clutter_text.set_markup('<span size="larger"><b>' + padDevice.get_device_name() + '</b></span>');
        this._labelBox.add_actor(this._titleLabel);

        this._tipLabel = new St.Label();
        this._labelBox.add_actor(this._tipLabel);
        this.actor.add_actor(this._labelBox);

        this._actionEditor = new ActionEditor();
        this._actionEditor.connect ('done', Lang.bind(this, this._endButtonActionEdition));
        this.actor.add_actor(this._actionEditor.actor);

        this._labels = [];
        this._ringLabels = [];
        this._stripLabels = [];

        // FIXME: Fix num buttons.
        let i = 0;
        for (i = 0; i < 50; i++) {
            let [found, x, y, direction] = this._padDiagram.getButtonLabelCoords(i);
            if (!found)
                break;
            let label = this._createLabel(i, Meta.PadActionType.BUTTON);
            this._labels.push(label);
        }

        for (i = 0; i < padDevice.get_n_rings(); i++) {
            let [found, x, y, direction] = this._padDiagram.getRingLabelCoords(i, CW);
            let [found2, x2, y2, direction2] = this._padDiagram.getRingLabelCoords(i, CCW);
            if (!found || !found2)
                break;

            let label1 = this._createLabel(i, Meta.PadActionType.RING);
            let label2 = this._createLabel(i, Meta.PadActionType.RING);
            this._ringLabels.push([label1, label2]);
        }

        for (i = 0; i < padDevice.get_n_strips(); i++) {
            let [found, x, y, direction] = this._padDiagram.getStripLabelCoords(i, UP);
            let [found2, x2, y2, direction2] = this._padDiagram.getStripLabelCoords(i, DOWN);
            if (!found || !found2)
                break;

            let label1 = this._createLabel(i, Meta.PadActionType.STRIP);
            let label2 = this._createLabel(i, Meta.PadActionType.STRIP);
            this._stripLabels.push([label1, label2]);
        }

        this._syncEditionMode();
    },

    _createLabel: function (number, type) {
        let str = global.display.get_pad_action_label(this.padDevice, type, number);
        let label = new St.Label({ text: str ? str : _('None') });
        this.actor.add_actor(label);

        return label;
    },

    _allocateChild: function (child, x, y, direction, box) {
        let [prefHeight, natHeight] = child.get_preferred_height (-1);
        let [prefWidth, natWidth] = child.get_preferred_width (natHeight);
        let childBox = new Clutter.ActorBox();

        natWidth = Math.min(natWidth, 250);

        if (direction == LTR) {
            childBox.x1 = x + box.x1;
            childBox.x2 = x + box.x1 + natWidth;
        } else {
            childBox.x1 = x + box.x1 - natWidth;
            childBox.x2 = x + box.x1;
        }

        childBox.y1 = y + box.y1 - natHeight / 2;
        childBox.y2 = y + box.y1 + natHeight / 2;
        child.allocate(childBox, 0);
    },

    _allocate: function (actor, box, flags) {
        let [prefLabelHeight, natLabelHeight] = this._labelBox.get_preferred_height(box.x2 - box.x1);
        let buttonY = Math.max((box.y2 - box.y1) * 3 / 4 + box.y1, (box.y2 - box.y1) - 100);
        let childBox = new Clutter.ActorBox();
        let diagramBox = new Clutter.ActorBox();

        diagramBox.x1 = box.x1;
        diagramBox.x2 = box.x2;
        diagramBox.y1 = prefLabelHeight;
        diagramBox.y2 = buttonY;
        this._padDiagram.allocate(diagramBox, flags);

        childBox.x1 = box.x1;
        childBox.x2 = box.x2;
        childBox.y1 = buttonY;
        childBox.y2 = box.y2;
        this._buttonBox.allocate(childBox, flags);

        childBox.y1 = 0;
        childBox.y2 = prefLabelHeight;
        this._labelBox.allocate(childBox, flags);

        for (let i = 0; i < this._labels.length; i++) {
            let label = this._labels[i];
            let [found, x, y, direction] = this._padDiagram.getButtonLabelCoords(i);
            this._allocateChild(label, x, y, direction, diagramBox);
        }

        for (let i = 0; i < this._ringLabels.length; i++) {
            let [label1, label2] = this._ringLabels[i];

            let [found, x, y, direction] = this._padDiagram.getRingLabelCoords(i, CW);
            this._allocateChild(label1, x, y, direction, diagramBox);

            [found, x, y, direction] = this._padDiagram.getRingLabelCoords(i, CCW);
            this._allocateChild(label2, x, y, direction, diagramBox);
        }

        for (let i = 0; i < this._stripLabels.length; i++) {
            let [label1, label2] = this._stripLabels[i];

            let [found, x, y, direction] = this._padDiagram.getStripLabelCoords(i, UP);
            this._allocateChild(label1, x, y, direction, diagramBox);

            [found, x, y, direction] = this._padDiagram.getStripLabelCoords(i, DOWN);
            this._allocateChild(label2, x, y, direction, diagramBox);
        }

        if (this._editingButtonAction != null) {
            let [found, x, y, direction] = this._padDiagram.getButtonLabelCoords(this._editingButtonAction);
            this._allocateChild(this._actionEditor.actor, x, y, direction, diagramBox);
        }
    },

    _onCapturedEvent : function (actor, event) {
        if (event.type() == Clutter.EventType.PAD_BUTTON_PRESS &&
            event.get_source_device() == this.padDevice) {
            this._padDiagram.activateButton(event.get_button());

            if (this._editionMode)
                this._startButtonActionEdition(event.get_button());
            return Clutter.EVENT_STOP;
        } else if (event.type() == Clutter.EventType.PAD_BUTTON_RELEASE &&
                   event.get_source_device() == this.padDevice) {
            this._padDiagram.deactivateButton(event.get_button());
            return Clutter.EVENT_STOP;
        } else if (event.type() == Clutter.EventType.KEY_PRESS &&
                   (!this._editionMode || event.get_key_symbol() == Clutter.Escape)) {
            if (this._editingButtonAction != null)
                this._endButtonActionEdition();
            else
                this.destroy();
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    _syncEditionMode: function () {
        this._editButton.set_reactive(!this._editionMode);
        this._editButton.save_easing_state();
        this._editButton.set_easing_duration(200);
        this._editButton.set_opacity(this._editionMode ? 128 : 255);
        this._editButton.restore_easing_state();

        let title;

        if (this._editionMode) {
            title = _('Press a button to configure');
            this._tipLabel.set_text (_("Press Esc to exit"));
        } else {
            title = this.padDevice.get_device_name();
            this._tipLabel.set_text (_("Press any key to exit"));
        }

        this._titleLabel.clutter_text.set_markup('<span size="larger"><b>' + title + '</b></span>');
    },

    _endButtonActionEdition: function () {
        this._actionEditor.close();

        if (this._editingButtonAction != null) {
            // Update and show the label
            let str = global.display.get_pad_action_label(this.padDevice,
                                                          Meta.PadActionType.BUTTON,
                                                          this._editingButtonAction);
            this._labels[this._editingButtonAction].set_text(str ? str : _('None'));

            this._labels[this._editingButtonAction].show();
            this._editingButtonAction = null;
        }

        this._editedButtonSettings = null;
    },

    _startButtonActionEdition: function (button) {
        if (this._editingButtonAction == button)
            return;

        this._endButtonActionEdition();
        this._editingButtonAction = button;

        this._labels[this._editingButtonAction].hide();
        this._actionEditor.actor.show();
        this.actor.queue_relayout();

        let ch = String.fromCharCode('A'.charCodeAt() + button);
        let settingsPath = this._settings.path + "button" + ch + '/';
        this._editedButtonSettings = Gio.Settings.new_with_path('org.gnome.desktop.peripherals.tablet.pad-button',
                                                                settingsPath);
        this._actionEditor.setSettings (this._editedButtonSettings);
    },

    setEditionMode: function (editionMode) {
        if (this._editionMode == editionMode)
            return;

        this._editionMode = editionMode;
        this._syncEditionMode();
    },

    destroy: function () {
        this._actionEditor.close();

        if (this._capturedEventId != 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        if (this.actor) {
            let actor = this.actor;
            this.actor = null;
            actor.destroy();
            this.emit('closed');
        }
    }
});
Signals.addSignalMethods(PadOsd.prototype);

const PadOsdIface = '<node> \
<interface name="org.gnome.Shell.Wacom.PadOsd"> \
<method name="Show"> \
    <arg name="device_node" direction="in" type="o"/> \
    <arg name="edition_mode" direction="in" type="b"/> \
</method> \
</interface> \
</node>';

const PadOsdService = new Lang.Class({
    Name: 'PadOsdService',

    _init: function() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(PadOsdIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Wacom');
        Gio.DBus.session.own_name('org.gnome.Shell.Wacom.PadOsd', Gio.BusNameOwnerFlags.REPLACE, null, null);
    },

    ShowAsync: function(params, invocation) {
        let [deviceNode, editionMode] = params;
        let deviceManager = Clutter.DeviceManager.get_default();
        let devices = deviceManager.list_devices();
        let padDevice = null;

        devices.forEach(Lang.bind(this, function(device) {
            if (deviceNode == device.get_device_node())
                padDevice = device;
        }));

        if (padDevice == null ||
            padDevice.get_device_type() != Clutter.InputDeviceType.PAD_DEVICE) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }

        global.display.request_pad_osd(padDevice, editionMode);
        invocation.return_value(null);
    }
});
Signals.addSignalMethods(PadOsdService.prototype);
