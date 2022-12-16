// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PadOsd, PadOsdService */

const {
    Atk, Clutter, GDesktopEnums, Gio,
    GLib, GObject, Meta, Pango, Rsvg, St,
} = imports.gi;
const Signals = imports.misc.signals;

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

        this._menuManager = new PopupMenu.PopupMenuManager(this);
        this._menuManager.addMenu(this._padChooserMenu);

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
    Signals: { 'keybinding-edited': { param_types: [GObject.TYPE_STRING] } },
}, class KeybindingEntry extends St.Entry {
    _init() {
        super._init({ hint_text: _("New shortcut…"), style: 'width: 10em' });
    }

    vfunc_captured_event(event) {
        if (event.type() != Clutter.EventType.KEY_PRESS)
            return Clutter.EVENT_PROPAGATE;

        const str = Meta.accelerator_name(
            event.get_state(), event.get_key_symbol());

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

        const boxLayout = new Clutter.BoxLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            spacing: 6,
        });
        let box = new St.Widget({ layout_manager: boxLayout });
        this.set_child(box);

        this._label = new St.Label({ style_class: 'combo-box-label' });
        box.add_child(this._label);

        const arrow = new St.Icon({
            style_class: 'popup-menu-arrow',
            icon_name: 'pan-down-symbolic',
            accessible_role: Atk.Role.ARROW,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(arrow);

        this._editMenu = new PopupMenu.PopupMenu(this, 0, St.Side.TOP);
        this._editMenu.connect('menu-closed', () => {
            this.set_checked(false);
        });
        this._editMenu.actor.hide();
        Main.uiGroup.add_actor(this._editMenu.actor);

        this._editMenuManager = new PopupMenu.PopupMenuManager(this);
        this._editMenuManager.addMenu(this._editMenu);

        this._actionLabels = new Map();
        this._actionLabels.set(GDesktopEnums.PadButtonAction.NONE, _('App defined'));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.HELP, _('Show on-screen help'));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.SWITCH_MONITOR, _('Switch monitor'));
        this._actionLabels.set(GDesktopEnums.PadButtonAction.KEYBINDING, _('Assign keystroke'));

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
        const boxLayout = new Clutter.BoxLayout({
            orientation: Clutter.Orientation.HORIZONTAL,
            spacing: 12,
        });

        super._init({ layout_manager: boxLayout });

        this._actionComboBox = new ActionComboBox();
        this._actionComboBox.connect('action-selected', this._onActionSelected.bind(this));
        this.add_actor(this._actionComboBox);

        this._keybindingEdit = new KeybindingEntry();
        this._keybindingEdit.connect('keybinding-edited', this._onKeybindingEdited.bind(this));
        this.add_actor(this._keybindingEdit);

        this._doneButton = new St.Button({
            label: _('Done'),
            style_class: 'button',
            x_expand: false,
        });
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
        this._curEdited = null;
        this._prevEdited = null;
        this._css = new TextDecoder().decode(css);
        this._labels = [];
        this._activeButtons = [];
        super._init(params);
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
        this._initLabels();
    }

    get editorActor() {
        return this._editorActor;
    }

    set editorActor(actor) {
        actor.hide();
        this._editorActor = actor;
        this.add_actor(actor);
    }

    _initLabels() {
        let i = 0;
        for (i = 0; ; i++) {
            if (!this._addLabel(Meta.PadActionType.BUTTON, i))
                break;
        }

        for (i = 0; ; i++) {
            if (!this._addLabel(Meta.PadActionType.RING, i, CW) ||
                !this._addLabel(Meta.PadActionType.RING, i, CCW))
                break;
        }

        for (i = 0; ; i++) {
            if (!this._addLabel(Meta.PadActionType.STRIP, i, UP) ||
                !this._addLabel(Meta.PadActionType.STRIP, i, DOWN))
                break;
        }
    }

    _wrappingSvgHeader() {
        return '<?xml version="1.0" encoding="UTF-8" standalone="no"?>' +
               '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" ' +
               'xmlns:xi="http://www.w3.org/2001/XInclude" ' +
               `width="${this._imageWidth}" height="${this._imageHeight}"> ` +
               '<style type="text/css">';
    }

    _wrappingSvgFooter() {
        return '%s%s%s'.format(
            '</style>',
            '<xi:include href="%s" />'.format(this._imagePath),
            '</svg>');
    }

    _cssString() {
        let css = this._css;

        for (let i = 0; i < this._activeButtons.length; i++) {
            let ch = String.fromCharCode('A'.charCodeAt() + this._activeButtons[i]);
            css += `.${ch}.Leader { stroke: ${ACTIVE_COLOR} !important; }`;
            css += `.${ch}.Button { stroke: ${ACTIVE_COLOR} !important; fill: ${ACTIVE_COLOR} !important; }`;
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
            Gio.File.new_for_path(this._imagePath), 0, null);
    }

    _updateDiagramScale() {
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

        // I miss Cairo.Matrix
        let dimensions = this._handle.get_dimensions();
        x = x * this._scale + this._actorWidth / 2 - dimensions.width / 2 * this._scale;
        y = y * this._scale + this._actorHeight / 2 - dimensions.height / 2 * this._scale;

        if (direction == LTR) {
            childBox.x1 = x;
            childBox.x2 = x + natWidth;
        } else {
            childBox.x1 = x - natWidth;
            childBox.x2 = x;
        }

        childBox.y1 = y - natHeight / 2;
        childBox.y2 = y + natHeight / 2;
        child.allocate(childBox);
    }

    vfunc_allocate(box) {
        super.vfunc_allocate(box);
        if (this._handle === null)
            return;

        this._updateDiagramScale();

        for (let i = 0; i < this._labels.length; i++) {
            const { label, x, y, arrangement } = this._labels[i];
            this._allocateChild(label, x, y, arrangement);
        }

        if (this._editorActor && this._curEdited) {
            const { x, y, arrangement } = this._curEdited;
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
        if (this.leftHanded)
            cr.rotate(Math.PI);
        cr.translate(-dimensions.width / 2, -dimensions.height / 2);
        this._handle.render_cairo(cr);
        cr.restore();
        cr.$dispose();
    }

    _getItemLabelCoords(labelName, leaderName) {
        if (this._handle == null)
            return [false];

        const [labelFound, labelPos] = this._handle.get_position_sub(`#${labelName}`);
        const [, labelSize] = this._handle.get_dimensions_sub(`#${labelName}`);
        if (!labelFound)
            return [false];

        const [leaderFound, leaderPos] = this._handle.get_position_sub(`#${leaderName}`);
        const [, leaderSize] = this._handle.get_dimensions_sub(`#${leaderName}`);
        if (!leaderFound)
            return [false];

        let direction;
        if (labelPos.x > leaderPos.x + leaderSize.width)
            direction = LTR;
        else
            direction = RTL;

        let pos = {x: labelPos.x, y: labelPos.y + labelSize.height};
        if (this.leftHanded) {
            direction = 1 - direction;
            pos.x = this._imageWidth - pos.x;
            pos.y = this._imageHeight - pos.y;
        }

        return [true, pos.x, pos.y, direction];
    }

    _getButtonLabels(button) {
        let ch = String.fromCharCode('A'.charCodeAt() + button);
        const labelName = `Label${ch}`;
        const leaderName = `Leader${ch}`;
        return [labelName, leaderName];
    }

    _getRingLabels(number, dir) {
        let numStr = number > 0 ? (number + 1).toString() : '';
        let dirStr = dir == CW ? 'CW' : 'CCW';
        const labelName = `LabelRing${numStr}${dirStr}`;
        const leaderName = `LeaderRing${numStr}${dirStr}`;
        return [labelName, leaderName];
    }

    _getStripLabels(number, dir) {
        let numStr = number > 0 ? (number + 1).toString() : '';
        let dirStr = dir == UP ? 'Up' : 'Down';
        const labelName = `LabelStrip${numStr}${dirStr}`;
        const leaderName = `LeaderStrip${numStr}${dirStr}`;
        return [labelName, leaderName];
    }

    _getLabelCoords(action, idx, dir) {
        if (action == Meta.PadActionType.BUTTON)
            return this._getItemLabelCoords(...this._getButtonLabels(idx));
        else if (action == Meta.PadActionType.RING)
            return this._getItemLabelCoords(...this._getRingLabels(idx, dir));
        else if (action == Meta.PadActionType.STRIP)
            return this._getItemLabelCoords(...this._getStripLabels(idx, dir));

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

    _addLabel(action, idx, dir) {
        let [found, x, y, arrangement] = this._getLabelCoords(action, idx, dir);
        if (!found)
            return false;

        let label = new St.Label();
        this._labels.push({ label, action, idx, dir, x, y, arrangement });
        this.add_actor(label);
        return true;
    }

    updateLabels(getText) {
        for (let i = 0; i < this._labels.length; i++) {
            const { label, action, idx, dir } = this._labels[i];
            let str = getText(action, idx, dir);
            label.set_text(str);
        }

        this.queue_relayout();
    }

    _applyLabel(label, action, idx, dir, str) {
        if (str !== null)
            label.set_text(str);
        label.show();
    }

    stopEdition(continues, str) {
        this._editorActor.hide();

        if (this._prevEdited) {
            const { label, action, idx, dir } = this._prevEdited;
            this._applyLabel(label, action, idx, dir, str);
            this._prevEdited = null;
        }

        if (this._curEdited) {
            const { label, action, idx, dir } = this._curEdited;
            this._applyLabel(label, action, idx, dir, str);
            if (continues)
                this._prevEdited = this._curEdited;
            this._curEdited = null;
        }

        this.queue_relayout();
    }

    startEdition(action, idx, dir) {
        let editedLabel;

        if (this._curEdited)
            return;

        for (let i = 0; i < this._labels.length; i++) {
            if (action == this._labels[i].action &&
                idx == this._labels[i].idx && dir == this._labels[i].dir) {
                this._curEdited = this._labels[i];
                editedLabel = this._curEdited.label;
                break;
            }
        }

        if (this._curEdited == null)
            return;
        this._editorActor.show();
        editedLabel.hide();
        this.queue_relayout();
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
        this._padChooser = null;

        let seat = Clutter.get_default_backend().get_default_seat();
        seat.connectObject(
            'device-added', (_seat, device) => {
                if (device.get_device_type() === Clutter.InputDeviceType.PAD_DEVICE &&
                    this.padDevice.is_grouped(device)) {
                    this._groupPads.push(device);
                    this._updatePadChooser();
                }
            },
            'device-removed', (_seat, device) => {
                // If the device is being removed, destroy the padOsd.
                if (device === this.padDevice) {
                    this.destroy();
                } else if (this._groupPads.includes(device)) {
                    // Or update the pad chooser if the device belongs to
                    // the same group.
                    this._groupPads.splice(this._groupPads.indexOf(device), 1);
                    this._updatePadChooser();
                }
            }, this);

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

        this._titleBox = new St.BoxLayout({
            style_class: 'pad-osd-title-box',
            vertical: false,
            x_expand: false,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this.add_actor(this._titleBox);

        const labelBox = new St.BoxLayout({
            style_class: 'pad-osd-title-menu-box',
            vertical: true,
        });
        this._titleBox.add_actor(labelBox);

        this._titleLabel = new St.Label({
            style: 'font-side: larger; font-weight: bold;',
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._titleLabel.clutter_text.set_ellipsize(Pango.EllipsizeMode.NONE);
        this._titleLabel.clutter_text.set_text(padDevice.get_device_name());
        labelBox.add_actor(this._titleLabel);

        this._tipLabel = new St.Label({ x_align: Clutter.ActorAlign.CENTER });
        labelBox.add_actor(this._tipLabel);

        this._updatePadChooser();

        this._actionEditor = new ActionEditor();
        this._actionEditor.connect('done', this._endActionEdition.bind(this));

        this._padDiagram = new PadDiagram({
            image: this._imagePath,
            left_handed: settings.get_boolean('left-handed'),
            editor_actor: this._actionEditor,
            x_expand: true,
            y_expand: true,
        });
        this.add_actor(this._padDiagram);
        this._updateActionLabels();

        const buttonBox = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
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
        this._grab = Main.pushModal(this);
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
        return str ?? _('None');
    }

    _updateActionLabels() {
        this._padDiagram.updateLabels(this._getActionText.bind(this));
    }

    vfunc_captured_event(event) {
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

        this._titleLabel.set_text(title);
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

            this._padDiagram.stopEdition(false, str ?? _('None'));
            this._editedAction = null;
        }

        this._editedActionSettings = null;
    }

    _startActionEdition(key, type, number, dir, mode) {
        if (this._isEditedAction(type, number, dir))
            return;

        this._endActionEdition();
        this._editedAction = { type, number, dir, mode };

        const settingsPath = `${this._settings.path}${key}/`;
        this._editedActionSettings = Gio.Settings.new_with_path('org.gnome.desktop.peripherals.tablet.pad-button',
                                                                settingsPath);
        this._actionEditor.setSettings(this._editedActionSettings, type);
        this._padDiagram.startEdition(type, number, dir);
    }

    _startButtonActionEdition(button) {
        let ch = String.fromCharCode('A'.charCodeAt() + button);
        let key = `button${ch}`;
        this._startActionEdition(key, Meta.PadActionType.BUTTON, button);
    }

    _startRingActionEdition(ring, dir, mode) {
        let ch = String.fromCharCode('A'.charCodeAt() + ring);
        const key = `ring${ch}-${dir === CCW ? 'ccw' : 'cw'}-mode-${mode}`;
        this._startActionEdition(key, Meta.PadActionType.RING, ring, dir, mode);
    }

    _startStripActionEdition(strip, dir, mode) {
        let ch = String.fromCharCode('A'.charCodeAt() + strip);
        const key = `strip${ch}-${dir === UP ? 'up' : 'down'}-mode-${mode}`;
        this._startActionEdition(key, Meta.PadActionType.STRIP, strip, dir, mode);
    }

    setEditionMode(editionMode) {
        if (this._editionMode == editionMode)
            return;

        this._editionMode = editionMode;
        this._syncEditionMode();
    }

    _onDestroy() {
        Main.popModal(this._grab);
        this._grab = null;
        this._actionEditor.close();

        this.emit('closed');
    }
});

const PadOsdIface = loadInterfaceXML('org.gnome.Shell.Wacom.PadOsd');

var PadOsdService = class extends Signals.EventEmitter {
    constructor() {
        super();

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
