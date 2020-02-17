// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PadOsd, PadOsdService */

const { Atk, Clutter, GDesktopEnums, Gio,
        GLib, GObject, Gtk, Meta, Rsvg, St } = imports.gi;
const Signals = imports.signals;

const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Layout = imports.ui.layout;

const { loadInterfaceXML } = imports.misc.fileUtils;

const ACTIVE_COLOR = "#729fcf";

const LTR = 0;
const RTL = 1;

const CW = 0;
const CCW = 1;

const UP = 0;
const DOWN = 1;

var PadChooser = GObject.registerClass({
    Signals: { 'pad-selected': { param_types: [Clutter.InputDevice.$gtype] } },
}, class PadChooser extends St.Button {
    _init(device, groupDevices) {
        super._init({
            style_class: 'pad-chooser-button',
            toggle_mode: true,
        });
        this.currentDevice = device;
        this._padChooserMenu = null;

        let arrow = new St.Icon({
            style_class: 'popup-menu-arrow',
            icon_name: 'pan-down-symbolic',
            accessible_role: Atk.Role.ARROW,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.set_child(arrow);
        this._ensureMenu(groupDevices);

        this.connect('destroy', this._onDestroy.bind(this));
    }

    vfunc_clicked() {
        if (this.get_checked()) {
            if (this._padChooserMenu != null)
                this._padChooserMenu.open(true);
            else
                this.set_checked(false);
        } else {
            this._padChooserMenu.close(true);
        }
    }

    _ensureMenu(devices) {
        this._padChooserMenu =  new PopupMenu.PopupMenu(this, 0.5, St.Side.TOP);
        this._padChooserMenu.connect('menu-closed', () => {
            this.set_checked(false);
        });
        this._padChooserMenu.actor.hide();
        Main.uiGroup.add_actor(this._padChooserMenu.actor);

        for (let i = 0; i < devices.length; i++) {
            let device = devices[i];
            if (device == this.currentDevice)
                continue;

            this._padChooserMenu.addAction(device.get_device_name(), () => {
                this.emit('pad-selected', device);
            });
        }
    }

    _onDestroy() {
        this._padChooserMenu.destroy();
    }

    update(devices) {
        if (this._padChooserMenu)
            this._padChooserMenu.actor.destroy();
        this.set_checked(false);
        this._ensureMenu(devices);
    }
});

var KeybindingEntry = GObject.registerClass({
    Signals: { 'keybinding-edited': {} },
}, class KeybindingEntry extends St.Entry {
    _init() {
        super._init({ hint_text: _("New shortcut…"), style: 'width: 10em' });
    }

    vfunc_captured_event(event) {
        if (event.type() != Clutter.EventType.KEY_PRESS)
            return Clutter.EVENT_PROPAGATE;

        let str = Gtk.accelerator_name_with_keycode(null,
                                                    event.get_key_symbol(),
                                                    event.get_key_code(),
                                                    event.get_state());
        this.set_text(str);
        this.emit('keybinding-edited', str);
        return Clutter.EVENT_STOP;
    }
});

var ActionComboBox = GObject.registerClass({
    Signals: { 'action-selected': { param_types: [GObject.TYPE_INT] } },
}, class ActionComboBox extends St.Button {
    _init() {
        super._init({ style_class: 'button' });
        this.set_toggle_mode(true);

        let boxLayout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.HORIZONTAL,
                                                spacing: 6 });
        let box = new St.Widget({ layout_manager: boxLayout });
        this.set_child(box);

        this._label = new St.Label({ style_class: 'combo-box-label' });
        box.add_child(this._label);

        let arrow = new St.Icon({ style_class: 'popup-menu-arrow',
                                  icon_name: 'pan-down-symbolic',
                                  accessible_role: Atk.Role.ARROW,
                                  y_expand: true,
                                  y_align: Clutter.ActorAlign.CENTER });
        box.add_child(arrow);

        this._editMenu = new PopupMenu.PopupMenu(this, 0, St.Side.TOP);
        this._editMenu.connect('menu-closed', () => {
            this.set_checked(false);
        });
        this._editMenu.actor.hide();
        Main.uiGroup.add_actor(this._editMenu.actor);

        this._actionLabels = new Map();
        this._actionLabels.set(GDesktopEnums.PadButtonAction.NONE, _("Application defined"));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.HELP, _("Show on-screen help"));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.SWITCH_MONITOR, _("Switch monitor"));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.KEYBINDING, _("Assign keystroke"));

        this._buttonItems = [];

        for (let [action, label] of this._actionLabels.entries()) {
            let selectedAction = action;
            let item = this._editMenu.addAction(label, () => {
                this._onActionSelected(selectedAction);
            });

            /* These actions only apply to pad buttons */
            if (selectedAction == GDesktopEnums.PadButtonAction.HELP ||
                selectedAction == GDesktopEnums.PadButtonAction.SWITCH_MONITOR)
                this._buttonItems.push(item);
        }

        this.setAction(GDesktopEnums.PadButtonAction.NONE);
    }

    _onActionSelected(action) {
        this.setAction(action);
        this.popdown();
        this.emit('action-selected', action);
    }

    setAction(action) {
        this._label.set_text(this._actionLabels.get(action));
    }

    popup() {
        this._editMenu.open(true);
    }

    popdown() {
        this._editMenu.close(true);
    }

    vfunc_clicked() {
        if (this.get_checked())
            this.popup();
        else
            this.popdown();
    }

    setButtonActionsActive(active) {
        this._buttonItems.forEach(item => item.setSensitive(active));
    }
});

var ActionEditor = GObject.registerClass({
    Signals: { 'done': {} },
}, class ActionEditor extends St.Widget {
    _init() {
        let boxLayout = new Clutter.BoxLayout({ orientation: Clutter.Orientation.HORIZONTAL,
                                                spacing: 12 });

        super._init({ layout_manager: boxLayout });

        this._actionComboBox = new ActionComboBox();
        this._actionComboBox.connect('action-selected', this._onActionSelected.bind(this));
        this.add_actor(this._actionComboBox);

        this._keybindingEdit = new KeybindingEntry();
        this._keybindingEdit.connect('keybinding-edited', this._onKeybindingEdited.bind(this));
        this.add_actor(this._keybindingEdit);

        this._doneButton = new St.Button({ label: _("Done"),
                                           style_class: 'button',
                                           x_expand: false });
        this._doneButton.connect('clicked', this._onEditingDone.bind(this));
        this.add_actor(this._doneButton);
    }

    _updateKeybindingEntryState() {
        if (this._currentAction == GDesktopEnums.PadButtonAction.KEYBINDING) {
            this._keybindingEdit.set_text(this._currentKeybinding);
            this._keybindingEdit.show();
            this._keybindingEdit.grab_key_focus();
        } else {
            this._keybindingEdit.hide();
        }
    }

    setSettings(settings, action) {
        this._buttonSettings = settings;

        this._currentAction = this._buttonSettings.get_enum('action');
        this._currentKeybinding = this._buttonSettings.get_string('keybinding');
        this._actionComboBox.setAction(this._currentAction);
        this._updateKeybindingEntryState();

        let isButton = action == Meta.PadActionType.BUTTON;
        this._actionComboBox.setButtonActionsActive(isButton);
    }

    close() {
        this._actionComboBox.popdown();
        this.hide();
    }

    _onKeybindingEdited(entry, keybinding) {
        this._currentKeybinding = keybinding;
    }

    _onActionSelected(menu, action) {
        this._currentAction = action;
        this._updateKeybindingEntryState();
    }

    _storeSettings() {
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
    }

    _onEditingDone() {
        this._storeSettings();
        this.close();
        this.emit('done');
    }
});

var PadDiagram = GObject.registerClass({
    Properties: {
        'left-handed': GObject.ParamSpec.boolean('left-handed',
                                                 'left-handed', 'Left handed',
                                                 GObject.ParamFlags.READWRITE |
                                                 GObject.ParamFlags.CONSTRUCT_ONLY,
                                                 false),
        'image': GObject.ParamSpec.string('image', 'image', 'Image',
                                          GObject.ParamFlags.READWRITE |
                                          GObject.ParamFlags.CONSTRUCT_ONLY,
                                          null),
        'editor-actor': GObject.ParamSpec.object('editor-actor',
                                                 'editor-actor',
                                                 'Editor actor',
                                                 GObject.ParamFlags.READWRITE |
                                                 GObject.ParamFlags.CONSTRUCT_ONLY,
                                                 Clutter.Actor.$gtype),
    },
}, class PadDiagram extends St.DrawingArea {
    _init(params) {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/pad-osd.css');
        let [success_, css] = file.load_contents(null);
        if (css instanceof Uint8Array)
            css = imports.byteArray.toString(css);
        this._curEdited = null;
        this._prevEdited = null;
        this._css = css;
        this._labels = [];
        this._activeButtons = [];
        super._init(params);
    }

    // eslint-disable-next-line camelcase
    get left_handed() {
        return this._leftHanded;
    }

    // eslint-disable-next-line camelcase
    set left_handed(leftHanded) {
        this._leftHanded = leftHanded;
    }

    get image() {
        return this._imagePath;
    }

    set image(imagePath) {
        let originalHandle = Rsvg.Handle.new_from_file(imagePath);
        let dimensions = originalHandle.get_dimensions();
        this._imageWidth = dimensions.width;
        this._imageHeight = dimensions.height;

        this._imagePath = imagePath;
        this._handle = this._composeStyledDiagram();
    }

    // eslint-disable-next-line camelcase
    get editor_actor() {
        return this._editorActor;
    }

    // eslint-disable-next-line camelcase
    set editor_actor(actor) {
        actor.hide();
        this._editorActor = actor;
        this.add_actor(actor);
    }

    _wrappingSvgHeader() {
        return '<?xml version="1.0" encoding="UTF-8" standalone="no"?>' +
               '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" ' +
               'xmlns:xi="http://www.w3.org/2001/XInclude" ' +
               'width="%d" height="%d">'.format(this._imageWidth, this._imageHeight) +
               '<style type="text/css">';
    }

    _wrappingSvgFooter() {
        return '</style>' +
                '<xi:include href="' + this._imagePath + '" />' +
                '</svg>';
    }

    _cssString() {
        let css = this._css;

        for (let i = 0; i < this._activeButtons.length; i++) {
            let ch = String.fromCharCode('A'.charCodeAt() + this._activeButtons[i]);
            css += '.%s {'.format(ch);
            css += '    stroke: %s !important;'.format(ACTIVE_COLOR);
            css += '    fill: %s !important;'.format(ACTIVE_COLOR);
            css += '}';
        }

        return css;
    }

    _composeStyledDiagram() {
        let svgData = '';

        if (!GLib.file_test(this._imagePath, GLib.FileTest.EXISTS))
            return null;

        svgData += this._wrappingSvgHeader();
        svgData += this._cssString();
        svgData += this._wrappingSvgFooter();

        let istream = new Gio.MemoryInputStream();
        istream.add_bytes(new GLib.Bytes(svgData));

        return Rsvg.Handle.new_from_stream_sync(istream,
                                                Gio.File.new_for_path(this._imagePath),
                                                0, null);
    }

    _updateDiagramScale() {
        if (this._handle == null)
            return;

        [this._actorWidth, this._actorHeight] = this.get_size();
        let dimensions = this._handle.get_dimensions();
        let scaleX = this._actorWidth / dimensions.width;
        let scaleY = this._actorHeight / dimensions.height;
        this._scale = Math.min(scaleX, scaleY);
    }

    _allocateChild(child, x, y, direction) {
        let [, natHeight] = child.get_preferred_height(-1);
        let [, natWidth] = child.get_preferred_width(natHeight);
        let childBox = new Clutter.ActorBox();

        if (direction == LTR) {
            childBox.x1 = x;
            childBox.x2 = x + natWidth;
        } else {
            childBox.x1 = x - natWidth;
            childBox.x2 = x;
        }

        childBox.y1 = y - natHeight / 2;
        childBox.y2 = y + natHeight / 2;
        child.allocate(childBox, 0);
    }

    vfunc_allocate(box, flags) {
        super.vfunc_allocate(box, flags);
        this._updateDiagramScale();

        for (let i = 0; i < this._labels.length; i++) {
            let [label, action, idx, dir] = this._labels[i];
            let [found_, x, y, arrangement] = this.getLabelCoords(action, idx, dir);
            this._allocateChild(label, x, y, arrangement);
        }

        if (this._editorActor && this._curEdited) {
            let [label_, action, idx, dir] = this._curEdited;
            let [found_, x, y, arrangement] = this.getLabelCoords(action, idx, dir);
            this._allocateChild(this._editorActor, x, y, arrangement);
        }
    }

    vfunc_repaint() {
        if (this._handle == null)
            return;

        if (this._scale == null)
            this._updateDiagramScale();

        let [width, height] = this.get_surface_size();
        let dimensions = this._handle.get_dimensions();
        let cr = this.get_context();

        cr.save();
        cr.translate(width / 2, height / 2);
        cr.scale(this._scale, this._scale);
        if (this._leftHanded)
            cr.rotate(Math.PI);
        cr.translate(-dimensions.width / 2, -dimensions.height / 2);
        this._handle.render_cairo(cr);
        cr.restore();
        cr.$dispose();
    }

    _transformPoint(x, y) {
        if (this._handle == null || this._scale == null)
            return [x, y];

        // I miss Cairo.Matrix
        let dimensions = this._handle.get_dimensions();
        x = x * this._scale + this._actorWidth / 2 - dimensions.width / 2 * this._scale;
        y = y * this._scale + this._actorHeight / 2 - dimensions.height / 2 * this._scale;
        return [Math.round(x), Math.round(y)];
    }

    _getItemLabelCoords(labelName, leaderName) {
        if (this._handle == null)
            return [false];

        let leaderPos, leaderSize, pos;
        let found, direction;

        [found, pos] = this._handle.get_position_sub('#%s'.format(labelName));
        if (!found)
            return [false];

        [found, leaderPos] = this._handle.get_position_sub('#%s'.format(leaderName));
        [found, leaderSize] = this._handle.get_dimensions_sub('#%s'.format(leaderName));
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

        let [x, y] = this._transformPoint(pos.x, pos.y);

        return [true, x, y, direction];
    }

    getButtonLabelCoords(button) {
        let ch = String.fromCharCode('A'.charCodeAt() + button);
        let labelName = 'Label%s'.format(ch);
        let leaderName = 'Leader%s'.format(ch);

        return this._getItemLabelCoords(labelName, leaderName);
    }

    getRingLabelCoords(number, dir) {
        let numStr = number > 0 ? (number + 1).toString() : '';
        let dirStr = dir == CW ? 'CW' : 'CCW';
        let labelName = 'LabelRing%s%s'.format(numStr, dirStr);
        let leaderName = 'LeaderRing%s%s'.format(numStr, dirStr);

        return this._getItemLabelCoords(labelName, leaderName);
    }

    getStripLabelCoords(number, dir) {
        let numStr = number > 0 ? (number + 1).toString() : '';
        let dirStr = dir == UP ? 'Up' : 'Down';
        let labelName = 'LabelStrip%s%s'.format(numStr, dirStr);
        let leaderName = 'LeaderStrip%s%s'.format(numStr, dirStr);

        return this._getItemLabelCoords(labelName, leaderName);
    }

    getLabelCoords(action, idx, dir) {
        if (action == Meta.PadActionType.BUTTON)
            return this.getButtonLabelCoords(idx);
        else if (action == Meta.PadActionType.RING)
            return this.getRingLabelCoords(idx, dir);
        else if (action == Meta.PadActionType.STRIP)
            return this.getStripLabelCoords(idx, dir);

        return [false];
    }

    _invalidateSvg() {
        if (this._handle == null)
            return;
        this._handle = this._composeStyledDiagram();
        this.queue_repaint();
    }

    activateButton(button) {
        this._activeButtons.push(button);
        this._invalidateSvg();
    }

    deactivateButton(button) {
        for (let i = 0; i < this._activeButtons.length; i++) {
            if (this._activeButtons[i] == button)
                this._activeButtons.splice(i, 1);
        }
        this._invalidateSvg();
    }

    addLabel(label, type, idx, dir) {
        this._labels.push([label, type, idx, dir]);
        this.add_actor(label);
    }

    updateLabels(getText) {
        for (let i = 0; i < this._labels.length; i++) {
            let [label, action, idx, dir] = this._labels[i];
            let str = getText(action, idx, dir);
            label.set_text(str);
        }
    }

    _applyLabel(label, action, idx, dir, str) {
        if (str != null) {
            label.set_text(str);

            let [found_, x, y, arrangement] = this.getLabelCoords(action, idx, dir);
            this._allocateChild(label, x, y, arrangement);
        }
        label.show();
    }

    stopEdition(continues, str) {
        this._editorActor.hide();

        if (this._prevEdited) {
            let [label, action, idx, dir] = this._prevEdited;
            this._applyLabel(label, action, idx, dir, str);
            this._prevEdited = null;
        }

        if (this._curEdited) {
            let [label, action, idx, dir] = this._curEdited;
            this._applyLabel(label, action, idx, dir, str);
            if (continues)
                this._prevEdited = this._curEdited;
            this._curEdited = null;
        }
    }

    startEdition(action, idx, dir) {
        let editedLabel;

        if (this._curEdited)
            return;

        for (let i = 0; i < this._labels.length; i++) {
            let [label, itemAction, itemIdx, itemDir] = this._labels[i];
            if (action == itemAction && idx == itemIdx && dir == itemDir) {
                this._curEdited = this._labels[i];
                editedLabel = label;
                break;
            }
        }

        if (this._curEdited == null)
            return;
        let [found] = this.getLabelCoords(action, idx, dir);
        if (!found)
            return;
        this._editorActor.show();
        editedLabel.hide();
    }
});

var PadOsd = GObject.registerClass({
    Signals: {
        'pad-selected': { param_types: [Clutter.InputDevice.$gtype] },
        'closed': {},
    },
}, class PadOsd extends St.BoxLayout {
    _init(padDevice, settings, imagePath, editionMode, monitorIndex) {
        super._init({
            style_class: 'pad-osd-window',
            vertical: true,
            x_expand: true,
            y_expand: true,
            reactive: true,
        });

        this.padDevice = padDevice;
        this._groupPads = [padDevice];
        this._settings = settings;
        this._imagePath = imagePath;
        this._editionMode = editionMode;
        this._capturedEventId = global.stage.connect('captured-event', this._onCapturedEvent.bind(this));
        this._padChooser = null;

        let seat = Clutter.get_default_backend().get_default_seat();
        this._deviceAddedId = seat.connect('device-added', (_seat, device) => {
            if (device.get_device_type() == Clutter.InputDeviceType.PAD_DEVICE &&
                this.padDevice.is_grouped(device)) {
                this._groupPads.push(device);
                this._updatePadChooser();
            }
        });
        this._deviceRemovedId = seat.connect('device-removed', (_seat, device) => {
            // If the device is being removed, destroy the padOsd.
            if (device == this.padDevice) {
                this.destroy();
            } else if (this._groupPads.includes(device)) {
                // Or update the pad chooser if the device belongs to
                // the same group.
                this._groupPads.splice(this._groupPads.indexOf(device), 1);
                this._updatePadChooser();

            }
        });

        seat.list_devices().forEach(device => {
            if (device != this.padDevice &&
                device.get_device_type() == Clutter.InputDeviceType.PAD_DEVICE &&
                this.padDevice.is_grouped(device))
                this._groupPads.push(device);
        });

        this.connect('destroy', this._onDestroy.bind(this));
        Main.uiGroup.add_actor(this);

        this._monitorIndex = monitorIndex;
        let constraint = new Layout.MonitorConstraint({ index: monitorIndex });
        this.add_constraint(constraint);

        this._titleBox = new St.BoxLayout({ style_class: 'pad-osd-title-box',
                                            vertical: false,
                                            x_expand: false,
                                            x_align: Clutter.ActorAlign.CENTER });
        this.add_actor(this._titleBox);

        let labelBox = new St.BoxLayout({ style_class: 'pad-osd-title-menu-box',
                                          vertical: true });
        this._titleBox.add_actor(labelBox);

        this._titleLabel = new St.Label({ style: 'font-side: larger; font-weight: bold;',
                                          x_align: Clutter.ActorAlign.CENTER });
        this._titleLabel.clutter_text.set_text(padDevice.get_device_name());
        labelBox.add_actor(this._titleLabel);

        this._tipLabel = new St.Label({ x_align: Clutter.ActorAlign.CENTER });
        labelBox.add_actor(this._tipLabel);

        this._updatePadChooser();

        this._actionEditor = new ActionEditor();
        this._actionEditor.connect('done', this._endActionEdition.bind(this));

        this._padDiagram = new PadDiagram({ image: this._imagePath,
                                            left_handed: settings.get_boolean('left-handed'),
                                            editor_actor: this._actionEditor,
                                            x_expand: true,
                                            y_expand: true });
        this.add_actor(this._padDiagram);

        // FIXME: Fix num buttons.
        let i = 0;
        for (i = 0; i < 50; i++) {
            let [found] = this._padDiagram.getButtonLabelCoords(i);
            if (!found)
                break;
            this._createLabel(Meta.PadActionType.BUTTON, i);
        }

        for (i = 0; i < padDevice.get_n_rings(); i++) {
            let [found] = this._padDiagram.getRingLabelCoords(i, CW);
            if (!found)
                break;
            this._createLabel(Meta.PadActionType.RING, i, CW);
            this._createLabel(Meta.PadActionType.RING, i, CCW);
        }

        for (i = 0; i < padDevice.get_n_strips(); i++) {
            let [found] = this._padDiagram.getStripLabelCoords(i, UP);
            if (!found)
                break;
            this._createLabel(Meta.PadActionType.STRIP, i, UP);
            this._createLabel(Meta.PadActionType.STRIP, i, DOWN);
        }

        let buttonBox = new St.Widget({ layout_manager: new Clutter.BinLayout(),
                                        x_expand: true,
                                        x_align: Clutter.ActorAlign.CENTER,
                                        y_align: Clutter.ActorAlign.CENTER });
        this.add_actor(buttonBox);
        this._editButton = new St.Button({
            label: _('Edit…'),
            style_class: 'button',
            can_focus: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._editButton.connect('clicked', () => {
            this.setEditionMode(true);
        });
        buttonBox.add_actor(this._editButton);

        this._syncEditionMode();
        Main.pushModal(this);
    }

    _updatePadChooser() {
        if (this._groupPads.length > 1) {
            if (this._padChooser == null) {
                this._padChooser = new PadChooser(this.padDevice, this._groupPads);
                this._padChooser.connect('pad-selected', (chooser, pad) => {
                    this._requestForOtherPad(pad);
                });
                this._titleBox.add_child(this._padChooser);
            } else {
                this._padChooser.update(this._groupPads);
            }
        } else if (this._padChooser != null) {
            this._padChooser.destroy();
            this._padChooser = null;
        }
    }

    _requestForOtherPad(pad) {
        if (pad == this.padDevice || !this._groupPads.includes(pad))
            return;

        let editionMode = this._editionMode;
        this.destroy();
        global.display.request_pad_osd(pad, editionMode);
    }

    _getActionText(type, number) {
        let str = global.display.get_pad_action_label(this.padDevice, type, number);
        return str ? str : _("None");
    }

    _createLabel(type, number, dir) {
        let label = new St.Label({ text: this._getActionText(type, number) });
        this._padDiagram.addLabel(label, type, number, dir);
    }

    _updateActionLabels() {
        this._padDiagram.updateLabels(this._getActionText.bind(this));
    }

    _onCapturedEvent(actor, event) {
        let isModeSwitch =
            (event.type() == Clutter.EventType.PAD_BUTTON_PRESS ||
             event.type() == Clutter.EventType.PAD_BUTTON_RELEASE) &&
            this.padDevice.get_mode_switch_button_group(event.get_button()) >= 0;

        if (event.type() == Clutter.EventType.PAD_BUTTON_PRESS &&
            event.get_source_device() == this.padDevice) {
            this._padDiagram.activateButton(event.get_button());

            /* Buttons that switch between modes cannot be edited */
            if (this._editionMode && !isModeSwitch)
                this._startButtonActionEdition(event.get_button());
            return Clutter.EVENT_STOP;
        } else if (event.type() == Clutter.EventType.PAD_BUTTON_RELEASE &&
                   event.get_source_device() == this.padDevice) {
            this._padDiagram.deactivateButton(event.get_button());

            if (isModeSwitch) {
                this._endActionEdition();
                this._updateActionLabels();
            }
            return Clutter.EVENT_STOP;
        } else if (event.type() == Clutter.EventType.KEY_PRESS &&
                   (!this._editionMode || event.get_key_symbol() === Clutter.KEY_Escape)) {
            if (this._editedAction != null)
                this._endActionEdition();
            else
                this.destroy();
            return Clutter.EVENT_STOP;
        } else if (event.get_source_device() == this.padDevice &&
                   event.type() == Clutter.EventType.PAD_STRIP) {
            if (this._editionMode) {
                let [retval_, number, mode] = event.get_pad_event_details();
                this._startStripActionEdition(number, UP, mode);
            }
        } else if (event.get_source_device() == this.padDevice &&
                   event.type() == Clutter.EventType.PAD_RING) {
            if (this._editionMode) {
                let [retval_, number, mode] = event.get_pad_event_details();
                this._startRingActionEdition(number, CCW, mode);
            }
        }

        // If the event comes from another pad in the same group,
        // show the OSD for it.
        if (this._groupPads.includes(event.get_source_device())) {
            this._requestForOtherPad(event.get_source_device());
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _syncEditionMode() {
        this._editButton.set_reactive(!this._editionMode);
        this._editButton.save_easing_state();
        this._editButton.set_easing_duration(200);
        this._editButton.set_opacity(this._editionMode ? 128 : 255);
        this._editButton.restore_easing_state();

        let title;

        if (this._editionMode) {
            title = _("Press a button to configure");
            this._tipLabel.set_text(_("Press Esc to exit"));
        } else {
            title = this.padDevice.get_device_name();
            this._tipLabel.set_text(_("Press any key to exit"));
        }

        this._titleLabel.clutter_text.set_markup(
            '<span size="larger"><b>%s</b></span>'.format(title));
    }

    _isEditedAction(type, number, dir) {
        if (!this._editedAction)
            return false;

        return this._editedAction.type == type &&
                this._editedAction.number == number &&
                this._editedAction.dir == dir;
    }

    _followUpActionEdition(str) {
        let { type, dir, number, mode } = this._editedAction;
        let hasNextAction = type == Meta.PadActionType.RING && dir == CCW ||
                             type == Meta.PadActionType.STRIP && dir == UP;
        if (!hasNextAction)
            return false;

        this._padDiagram.stopEdition(true, str);
        this._editedAction = null;
        if (type == Meta.PadActionType.RING)
            this._startRingActionEdition(number, CW, mode);
        else
            this._startStripActionEdition(number, DOWN, mode);

        return true;
    }

    _endActionEdition() {
        this._actionEditor.close();

        if (this._editedAction != null) {
            let str = global.display.get_pad_action_label(this.padDevice,
                                                          this._editedAction.type,
                                                          this._editedAction.number);
            if (this._followUpActionEdition(str))
                return;

            this._padDiagram.stopEdition(false, str ? str : _("None"));
            this._editedAction = null;
        }

        this._editedActionSettings = null;
    }

    _startActionEdition(key, type, number, dir, mode) {
        if (this._isEditedAction(type, number, dir))
            return;

        this._endActionEdition();
        this._editedAction = { type, number, dir, mode };

        let settingsPath = this._settings.path + key + '/';
        this._editedActionSettings = Gio.Settings.new_with_path('org.gnome.desktop.peripherals.tablet.pad-button',
                                                                settingsPath);
        this._actionEditor.setSettings(this._editedActionSettings, type);
        this._padDiagram.startEdition(type, number, dir);
    }

    _startButtonActionEdition(button) {
        let ch = String.fromCharCode('A'.charCodeAt() + button);
        let key = 'button%s'.format(ch);
        this._startActionEdition(key, Meta.PadActionType.BUTTON, button);
    }

    _startRingActionEdition(ring, dir, mode) {
        let ch = String.fromCharCode('A'.charCodeAt() + ring);
        let key = 'ring%s-%s-mode-%d'.format(ch, dir == CCW ? 'ccw' : 'cw', mode);
        this._startActionEdition(key, Meta.PadActionType.RING, ring, dir, mode);
    }

    _startStripActionEdition(strip, dir, mode) {
        let ch = String.fromCharCode('A'.charCodeAt() + strip);
        let key = 'strip%s-%s-mode-%d'.format(ch, dir == UP ? 'up' : 'down', mode);
        this._startActionEdition(key, Meta.PadActionType.STRIP, strip, dir, mode);
    }

    setEditionMode(editionMode) {
        if (this._editionMode == editionMode)
            return;

        this._editionMode = editionMode;
        this._syncEditionMode();
    }

    _onDestroy() {
        Main.popModal(this);
        this._actionEditor.close();

        let seat = Clutter.get_default_backend().get_default_seat();
        if (this._deviceRemovedId != 0) {
            seat.disconnect(this._deviceRemovedId);
            this._deviceRemovedId = 0;
        }
        if (this._deviceAddedId != 0) {
            seat.disconnect(this._deviceAddedId);
            this._deviceAddedId = 0;
        }

        if (this._capturedEventId != 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        this.emit('closed');
    }
});

const PadOsdIface = loadInterfaceXML('org.gnome.Shell.Wacom.PadOsd');

var PadOsdService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(PadOsdIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Wacom');
        Gio.DBus.session.own_name('org.gnome.Shell.Wacom.PadOsd', Gio.BusNameOwnerFlags.REPLACE, null, null);
    }

    ShowAsync(params, invocation) {
        let [deviceNode, editionMode] = params;
        let seat = Clutter.get_default_backend().get_default_seat();
        let devices = seat.list_devices();
        let padDevice = null;

        devices.forEach(device => {
            if (deviceNode == device.get_device_node() &&
                device.get_device_type() == Clutter.InputDeviceType.PAD_DEVICE)
                padDevice = device;
        });

        if (padDevice == null) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }

        global.display.request_pad_osd(padDevice, editionMode);
        invocation.return_value(null);
    }
};
Signals.addSignalMethods(PadOsdService.prototype);
