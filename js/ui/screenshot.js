// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenshotService, ScreenshotUI, showScreenshotUI, captureScreenshot */

const {Clutter, Cogl, Gio, GObject, GLib, Graphene, Meta, Shell, St} = imports.gi;

const GrabHelper = imports.ui.grabHelper;
const Layout = imports.ui.layout;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Workspace = imports.ui.workspace;

Gio._promisify(Shell.Screenshot.prototype, 'pick_color');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_window');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_area');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_stage_to_content');
Gio._promisify(Shell.Screenshot, 'composite_to_stream');

const { loadInterfaceXML } = imports.misc.fileUtils;
const { DBusSenderChecker } = imports.misc.util;

const ScreenshotIface = loadInterfaceXML('org.gnome.Shell.Screenshot');

const ScreencastIface = loadInterfaceXML('org.gnome.Shell.Screencast');
const ScreencastProxy = Gio.DBusProxy.makeProxyWrapper(ScreencastIface);

var IconLabelButton = GObject.registerClass(
class IconLabelButton extends St.Button {
    _init(iconName, label, params) {
        super._init(params);

        this._container = new St.BoxLayout({
            vertical: true,
            style_class: 'icon-label-button-container',
        });
        this.set_child(this._container);

        this._container.add_child(new St.Icon({ icon_name: iconName }));
        this._container.add_child(new St.Label({
            text: label,
            x_align: Clutter.ActorAlign.CENTER,
        }));
    }
});

var Tooltip = GObject.registerClass(
class Tooltip extends St.Label {
    _init(widget, params) {
        super._init(params);

        this._widget = widget;
        this._timeoutId = null;

        this._widget.connect('notify::hover', () => {
            if (this._widget.hover)
                this.open();
            else
                this.close();
        });
    }

    open() {
        if (this._timeoutId)
            return;

        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 300, () => {
            this.opacity = 0;
            this.show();

            const extents = this._widget.get_transformed_extents();

            const xOffset = Math.floor((extents.get_width() - this.width) / 2);
            const x =
                Math.clamp(extents.get_x() + xOffset, 0, global.stage.width - this.width);

            const node = this.get_theme_node();
            const yOffset = node.get_length('-y-offset');

            const y = extents.get_y() - this.height - yOffset;

            this.set_position(x, y);
            this.ease({
                opacity: 255,
                duration: 150,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });

            this._timeoutId = null;
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] tooltip.open');
    }

    close() {
        if (this._timeoutId) {
            GLib.source_remove(this._timeoutId);
            this._timeoutId = null;
            return;
        }

        if (!this.visible)
            return;

        this.remove_all_transitions();
        this.ease({
            opacity: 0,
            duration: 100,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this.hide(),
        });
    }
});

var UIAreaIndicator = GObject.registerClass(
class UIAreaIndicator extends St.Widget {
    _init(params) {
        super._init(params);

        this._topRect = new St.Widget({ style_class: 'screenshot-ui-area-indicator-shade' });
        this._topRect.add_constraint(new Clutter.BindConstraint({
            source: this,
            coordinate: Clutter.BindCoordinate.WIDTH,
        }));
        this._topRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.TOP,
            to_edge: Clutter.SnapEdge.TOP,
        }));
        this._topRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.LEFT,
            to_edge: Clutter.SnapEdge.LEFT,
        }));
        this.add_child(this._topRect);

        this._bottomRect = new St.Widget({ style_class: 'screenshot-ui-area-indicator-shade' });
        this._bottomRect.add_constraint(new Clutter.BindConstraint({
            source: this,
            coordinate: Clutter.BindCoordinate.WIDTH,
        }));
        this._bottomRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.BOTTOM,
            to_edge: Clutter.SnapEdge.BOTTOM,
        }));
        this._bottomRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.LEFT,
            to_edge: Clutter.SnapEdge.LEFT,
        }));
        this.add_child(this._bottomRect);

        this._leftRect = new St.Widget({ style_class: 'screenshot-ui-area-indicator-shade' });
        this._leftRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.LEFT,
            to_edge: Clutter.SnapEdge.LEFT,
        }));
        this._leftRect.add_constraint(new Clutter.SnapConstraint({
            source: this._topRect,
            from_edge: Clutter.SnapEdge.TOP,
            to_edge: Clutter.SnapEdge.BOTTOM,
        }));
        this._leftRect.add_constraint(new Clutter.SnapConstraint({
            source: this._bottomRect,
            from_edge: Clutter.SnapEdge.BOTTOM,
            to_edge: Clutter.SnapEdge.TOP,
        }));
        this.add_child(this._leftRect);

        this._rightRect = new St.Widget({ style_class: 'screenshot-ui-area-indicator-shade' });
        this._rightRect.add_constraint(new Clutter.SnapConstraint({
            source: this,
            from_edge: Clutter.SnapEdge.RIGHT,
            to_edge: Clutter.SnapEdge.RIGHT,
        }));
        this._rightRect.add_constraint(new Clutter.SnapConstraint({
            source: this._topRect,
            from_edge: Clutter.SnapEdge.TOP,
            to_edge: Clutter.SnapEdge.BOTTOM,
        }));
        this._rightRect.add_constraint(new Clutter.SnapConstraint({
            source: this._bottomRect,
            from_edge: Clutter.SnapEdge.BOTTOM,
            to_edge: Clutter.SnapEdge.TOP,
        }));
        this.add_child(this._rightRect);

        this._selectionRect = new St.Widget({ style_class: 'screenshot-ui-area-indicator-selection' });
        this.add_child(this._selectionRect);

        this._topRect.add_constraint(new Clutter.SnapConstraint({
            source: this._selectionRect,
            from_edge: Clutter.SnapEdge.BOTTOM,
            to_edge: Clutter.SnapEdge.TOP,
        }));

        this._bottomRect.add_constraint(new Clutter.SnapConstraint({
            source: this._selectionRect,
            from_edge: Clutter.SnapEdge.TOP,
            to_edge: Clutter.SnapEdge.BOTTOM,
        }));

        this._leftRect.add_constraint(new Clutter.SnapConstraint({
            source: this._selectionRect,
            from_edge: Clutter.SnapEdge.RIGHT,
            to_edge: Clutter.SnapEdge.LEFT,
        }));

        this._rightRect.add_constraint(new Clutter.SnapConstraint({
            source: this._selectionRect,
            from_edge: Clutter.SnapEdge.LEFT,
            to_edge: Clutter.SnapEdge.RIGHT,
        }));
    }

    setSelectionRect(x, y, width, height) {
        this._selectionRect.set_position(x, y);
        this._selectionRect.set_size(width, height);
    }
});

var UIAreaSelector = GObject.registerClass({
    Signals: { 'drag-started': {}, 'drag-ended': {} },
}, class UIAreaSelector extends St.Widget {
    _init(params) {
        super._init(params);

        // During a drag, this can be Clutter.BUTTON_PRIMARY,
        // Clutter.BUTTON_SECONDARY or the string "touch" to identify the source
        // of the drag operation.
        this._dragButton = 0;
        this._dragSequence = null;

        this._areaIndicator = new UIAreaIndicator();
        this._areaIndicator.add_constraint(new Clutter.BindConstraint({
            source: this,
            coordinate: Clutter.BindCoordinate.ALL,
        }));
        this.add_child(this._areaIndicator);

        this._topLeftHandle = new St.Widget({ style_class: 'screenshot-ui-area-selector-handle' });
        this.add_child(this._topLeftHandle);
        this._topRightHandle = new St.Widget({ style_class: 'screenshot-ui-area-selector-handle' });
        this.add_child(this._topRightHandle);
        this._bottomLeftHandle = new St.Widget({ style_class: 'screenshot-ui-area-selector-handle' });
        this.add_child(this._bottomLeftHandle);
        this._bottomRightHandle = new St.Widget({ style_class: 'screenshot-ui-area-selector-handle' });
        this.add_child(this._bottomRightHandle);

        // This will be updated before the first drawn frame.
        this._handleSize = 0;
        this._topLeftHandle.connect('style-changed', widget => {
            this._handleSize = widget.get_theme_node().get_width();
            this._updateSelectionRect();
        });

        this.connect('notify::mapped', () => {
            if (this.mapped) {
                const [x, y] = global.get_pointer();
                this._updateCursor(x, y);
            }
        });

        // Initialize area to out of bounds so reset() below resets it.
        this._startX = -1;
        this._startY = 0;
        this._lastX = 0;
        this._lastY = 0;

        this.reset();
    }

    reset() {
        this.stopDrag();
        global.display.set_cursor(Meta.Cursor.DEFAULT);

        // Preserve area selection if possible. If the area goes out of bounds,
        // the monitors might have changed, so reset the area.
        const [x, y, w, h] = this.getGeometry();
        if (x < 0 || y < 0 || x + w > this.width || y + h > this.height) {
            // Initialize area to out of bounds so if there's no monitor,
            // the area will be reset once a monitor does appear.
            this._startX = -1;
            this._startY = 0;
            this._lastX = 0;
            this._lastY = 0;

            // This can happen when running headless without any monitors.
            if (Main.layoutManager.primaryIndex !== -1) {
                const monitor =
                    Main.layoutManager.monitors[Main.layoutManager.primaryIndex];

                this._startX = monitor.x + Math.floor(monitor.width * 3 / 8);
                this._startY = monitor.y + Math.floor(monitor.height * 3 / 8);
                this._lastX = monitor.x + Math.floor(monitor.width * 5 / 8) - 1;
                this._lastY = monitor.y + Math.floor(monitor.height * 5 / 8) - 1;
            }

            this._updateSelectionRect();
        }
    }

    getGeometry() {
        const leftX = Math.min(this._startX, this._lastX);
        const topY = Math.min(this._startY, this._lastY);
        const rightX = Math.max(this._startX, this._lastX);
        const bottomY = Math.max(this._startY, this._lastY);

        return [leftX, topY, rightX - leftX + 1, bottomY - topY + 1];
    }

    _updateSelectionRect() {
        const [x, y, w, h] = this.getGeometry();
        this._areaIndicator.setSelectionRect(x, y, w, h);

        const offset = this._handleSize / 2;
        this._topLeftHandle.set_position(x - offset, y - offset);
        this._topRightHandle.set_position(x + w - 1 - offset, y - offset);
        this._bottomLeftHandle.set_position(x - offset, y + h - 1 - offset);
        this._bottomRightHandle.set_position(x + w - 1 - offset, y + h - 1 - offset);
    }

    _computeCursorType(cursorX, cursorY) {
        const [leftX, topY, width, height] = this.getGeometry();
        const [rightX, bottomY] = [leftX + width - 1, topY + height - 1];
        const [x, y] = [cursorX, cursorY];

        // Check if the cursor overlaps the handles first.
        const limit = (this._handleSize / 2) ** 2;
        if ((leftX - x) ** 2 + (topY - y) ** 2 <= limit)
            return Meta.Cursor.NW_RESIZE;
        else if ((rightX - x) ** 2 + (topY - y) ** 2 <= limit)
            return Meta.Cursor.NE_RESIZE;
        else if ((leftX - x) ** 2 + (bottomY - y) ** 2 <= limit)
            return Meta.Cursor.SW_RESIZE;
        else if ((rightX - x) ** 2 + (bottomY - y) ** 2 <= limit)
            return Meta.Cursor.SE_RESIZE;

        // Now check the rest of the rectangle.
        const threshold =
            10 * St.ThemeContext.get_for_stage(global.stage).scaleFactor;

        if (leftX - x >= 0 && leftX - x <= threshold) {
            if (topY - y >= 0 && topY - y <= threshold)
                return Meta.Cursor.NW_RESIZE;
            else if (y - bottomY >= 0 && y - bottomY <= threshold)
                return Meta.Cursor.SW_RESIZE;
            else if (topY - y < 0 && y - bottomY < 0)
                return Meta.Cursor.WEST_RESIZE;
        } else if (x - rightX >= 0 && x - rightX <= threshold) {
            if (topY - y >= 0 && topY - y <= threshold)
                return Meta.Cursor.NE_RESIZE;
            else if (y - bottomY >= 0 && y - bottomY <= threshold)
                return Meta.Cursor.SE_RESIZE;
            else if (topY - y < 0 && y - bottomY < 0)
                return Meta.Cursor.EAST_RESIZE;
        } else if (leftX - x < 0 && x - rightX < 0) {
            if (topY - y >= 0 && topY - y <= threshold)
                return Meta.Cursor.NORTH_RESIZE;
            else if (y - bottomY >= 0 && y - bottomY <= threshold)
                return Meta.Cursor.SOUTH_RESIZE;
            else if (topY - y < 0 && y - bottomY < 0)
                return Meta.Cursor.MOVE_OR_RESIZE_WINDOW;
        }

        return Meta.Cursor.CROSSHAIR;
    }

    stopDrag() {
        if (!this._dragButton)
            return;

        if (this._dragGrab) {
            this._dragGrab.dismiss();
            this._dragGrab = null;
        }

        this._dragButton = 0;
        this._dragSequence = null;

        if (this._dragCursor === Meta.Cursor.CROSSHAIR &&
            this._lastX === this._startX && this._lastY === this._startY) {
            // The user clicked without dragging. Make up a larger selection
            // to reduce confusion.
            const offset =
                20 * St.ThemeContext.get_for_stage(global.stage).scaleFactor;
            this._startX -= offset;
            this._startY -= offset;
            this._lastX += offset;
            this._lastY += offset;

            // Keep the coordinates inside the stage.
            if (this._startX < 0) {
                this._lastX -= this._startX;
                this._startX = 0;
            } else if (this._lastX >= this.width) {
                this._startX -= this._lastX - this.width + 1;
                this._lastX = this.width - 1;
            }

            if (this._startY < 0) {
                this._lastY -= this._startY;
                this._startY = 0;
            } else if (this._lastY >= this.height) {
                this._startY -= this._lastY - this.height + 1;
                this._lastY = this.height - 1;
            }

            this._updateSelectionRect();
        }

        this.emit('drag-ended');
    }

    _updateCursor(x, y) {
        const cursor = this._computeCursorType(x, y);
        global.display.set_cursor(cursor);
    }

    _onPress(event, button, sequence) {
        if (this._dragButton)
            return Clutter.EVENT_PROPAGATE;

        const cursor = this._computeCursorType(event.x, event.y);

        // Clicking outside of the selection, or using the right mouse button,
        // or with Ctrl results in dragging a new selection from scratch.
        if (cursor === Meta.Cursor.CROSSHAIR ||
            button === Clutter.BUTTON_SECONDARY ||
            (event.modifier_state & Clutter.ModifierType.CONTROL_MASK)) {
            this._dragButton = button;

            this._dragCursor = Meta.Cursor.CROSSHAIR;
            global.display.set_cursor(Meta.Cursor.CROSSHAIR);

            [this._startX, this._startY] = [event.x, event.y];
            this._lastX = this._startX = Math.floor(this._startX);
            this._lastY = this._startY = Math.floor(this._startY);

            this._updateSelectionRect();
        } else {
            // This is a move or resize operation.
            this._dragButton = button;

            this._dragCursor = cursor;
            this._dragStartX = event.x;
            this._dragStartY = event.y;

            const [leftX, topY, width, height] = this.getGeometry();
            const rightX = leftX + width - 1;
            const bottomY = topY + height - 1;

            // For moving, start X and Y are the top left corner, while
            // last X and Y are the bottom right corner.
            if (cursor === Meta.Cursor.MOVE_OR_RESIZE_WINDOW) {
                this._startX = leftX;
                this._startY = topY;
                this._lastX = rightX;
                this._lastY = bottomY;
            }

            // Start X and Y are set to the stationary sides, while last X
            // and Y are set to the moving sides.
            if (cursor === Meta.Cursor.NW_RESIZE ||
                cursor === Meta.Cursor.WEST_RESIZE ||
                cursor === Meta.Cursor.SW_RESIZE) {
                this._startX = rightX;
                this._lastX = leftX;
            }
            if (cursor === Meta.Cursor.NE_RESIZE ||
                cursor === Meta.Cursor.EAST_RESIZE ||
                cursor === Meta.Cursor.SE_RESIZE) {
                this._startX = leftX;
                this._lastX = rightX;
            }
            if (cursor === Meta.Cursor.NW_RESIZE ||
                cursor === Meta.Cursor.NORTH_RESIZE ||
                cursor === Meta.Cursor.NE_RESIZE) {
                this._startY = bottomY;
                this._lastY = topY;
            }
            if (cursor === Meta.Cursor.SW_RESIZE ||
                cursor === Meta.Cursor.SOUTH_RESIZE ||
                cursor === Meta.Cursor.SE_RESIZE) {
                this._startY = topY;
                this._lastY = bottomY;
            }
        }

        if (this._dragButton) {
            this._dragGrab = global.stage.grab(this);
            this._dragSequence = sequence;

            this.emit('drag-started');

            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _onRelease(event, button, sequence) {
        if (this._dragButton !== button ||
            this._dragSequence?.get_slot() !== sequence?.get_slot())
            return Clutter.EVENT_PROPAGATE;

        this.stopDrag();

        // We might have finished creating a new selection, so we need to
        // update the cursor.
        this._updateCursor(event.x, event.y);

        return Clutter.EVENT_STOP;
    }

    _onMotion(event, sequence) {
        if (!this._dragButton) {
            this._updateCursor(event.x, event.y);
            return Clutter.EVENT_PROPAGATE;
        }

        if (sequence?.get_slot() !== this._dragSequence?.get_slot())
            return Clutter.EVENT_PROPAGATE;

        if (this._dragCursor === Meta.Cursor.CROSSHAIR) {
            [this._lastX, this._lastY] = [event.x, event.y];
            this._lastX = Math.floor(this._lastX);
            this._lastY = Math.floor(this._lastY);
        } else {
            let dx = Math.round(event.x - this._dragStartX);
            let dy = Math.round(event.y - this._dragStartY);

            if (this._dragCursor === Meta.Cursor.MOVE_OR_RESIZE_WINDOW) {
                const [,, selectionWidth, selectionHeight] = this.getGeometry();

                let newStartX = this._startX + dx;
                let newStartY = this._startY + dy;
                let newLastX = this._lastX + dx;
                let newLastY = this._lastY + dy;

                let overshootX = 0;
                let overshootY = 0;

                // Keep the size intact if we bumped into the stage edge.
                if (newStartX < 0) {
                    overshootX = 0 - newStartX;
                    newStartX = 0;
                    newLastX = newStartX + (selectionWidth - 1);
                } else if (newLastX > this.width - 1) {
                    overshootX = (this.width - 1) - newLastX;
                    newLastX = this.width - 1;
                    newStartX = newLastX - (selectionWidth - 1);
                }

                if (newStartY < 0) {
                    overshootY = 0 - newStartY;
                    newStartY = 0;
                    newLastY = newStartY + (selectionHeight - 1);
                } else if (newLastY > this.height - 1) {
                    overshootY = (this.height - 1) - newLastY;
                    newLastY = this.height - 1;
                    newStartY = newLastY - (selectionHeight - 1);
                }

                // Add the overshoot to the delta to create a "rubberbanding"
                // behavior of the pointer when dragging.
                dx += overshootX;
                dy += overshootY;

                this._startX = newStartX;
                this._startY = newStartY;
                this._lastX = newLastX;
                this._lastY = newLastY;
            } else {
                if (this._dragCursor === Meta.Cursor.WEST_RESIZE ||
                    this._dragCursor === Meta.Cursor.EAST_RESIZE)
                    dy = 0;
                if (this._dragCursor === Meta.Cursor.NORTH_RESIZE ||
                    this._dragCursor === Meta.Cursor.SOUTH_RESIZE)
                    dx = 0;

                // Make sure last X and Y are clamped between 0 and size - 1,
                // while always preserving the cursor dragging position relative
                // to the selection rectangle.
                this._lastX += dx;
                if (this._lastX >= this.width) {
                    dx -= this._lastX - this.width + 1;
                    this._lastX = this.width - 1;
                } else if (this._lastX < 0) {
                    dx -= this._lastX;
                    this._lastX = 0;
                }

                this._lastY += dy;
                if (this._lastY >= this.height) {
                    dy -= this._lastY - this.height + 1;
                    this._lastY = this.height - 1;
                } else if (this._lastY < 0) {
                    dy -= this._lastY;
                    this._lastY = 0;
                }

                // If we drag the handle past a selection side, update which
                // handles are which.
                if (this._lastX > this._startX) {
                    if (this._dragCursor === Meta.Cursor.NW_RESIZE)
                        this._dragCursor = Meta.Cursor.NE_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.SW_RESIZE)
                        this._dragCursor = Meta.Cursor.SE_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.WEST_RESIZE)
                        this._dragCursor = Meta.Cursor.EAST_RESIZE;
                } else {
                    // eslint-disable-next-line no-lonely-if
                    if (this._dragCursor === Meta.Cursor.NE_RESIZE)
                        this._dragCursor = Meta.Cursor.NW_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.SE_RESIZE)
                        this._dragCursor = Meta.Cursor.SW_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.EAST_RESIZE)
                        this._dragCursor = Meta.Cursor.WEST_RESIZE;
                }

                if (this._lastY > this._startY) {
                    if (this._dragCursor === Meta.Cursor.NW_RESIZE)
                        this._dragCursor = Meta.Cursor.SW_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.NE_RESIZE)
                        this._dragCursor = Meta.Cursor.SE_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.NORTH_RESIZE)
                        this._dragCursor = Meta.Cursor.SOUTH_RESIZE;
                } else {
                    // eslint-disable-next-line no-lonely-if
                    if (this._dragCursor === Meta.Cursor.SW_RESIZE)
                        this._dragCursor = Meta.Cursor.NW_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.SE_RESIZE)
                        this._dragCursor = Meta.Cursor.NE_RESIZE;
                    else if (this._dragCursor === Meta.Cursor.SOUTH_RESIZE)
                        this._dragCursor = Meta.Cursor.NORTH_RESIZE;
                }

                global.display.set_cursor(this._dragCursor);
            }

            this._dragStartX += dx;
            this._dragStartY += dy;
        }

        this._updateSelectionRect();

        return Clutter.EVENT_STOP;
    }

    vfunc_button_press_event(event) {
        if (event.button === Clutter.BUTTON_PRIMARY ||
            event.button === Clutter.BUTTON_SECONDARY)
            return this._onPress(event, event.button, null);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event(event) {
        if (event.button === Clutter.BUTTON_PRIMARY ||
            event.button === Clutter.BUTTON_SECONDARY)
            return this._onRelease(event, event.button, null);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_motion_event(event) {
        return this._onMotion(event, null);
    }

    vfunc_touch_event(event) {
        if (event.type === Clutter.EventType.TOUCH_BEGIN)
            return this._onPress(event, 'touch', event.sequence);
        else if (event.type === Clutter.EventType.TOUCH_END)
            return this._onRelease(event, 'touch', event.sequence);
        else if (event.type === Clutter.EventType.TOUCH_UPDATE)
            return this._onMotion(event, event.sequence);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_leave_event(event) {
        // If we're dragging and go over the panel we still get a leave event
        // for some reason, even though we have a grab. We don't want to switch
        // the cursor when we're dragging.
        if (!this._dragButton)
            global.display.set_cursor(Meta.Cursor.DEFAULT);

        return super.vfunc_leave_event(event);
    }
});

var UIWindowSelectorLayout = GObject.registerClass(
class UIWindowSelectorLayout extends Workspace.WorkspaceLayout {
    _init(monitorIndex) {
        super._init(null, monitorIndex, null);
    }

    vfunc_set_container(container) {
        this._container = container;
        this._syncWorkareaTracking();
    }

    vfunc_allocate(container, box) {
        const containerBox = container.allocation;
        const containerAllocationChanged =
            this._lastBox === null || !this._lastBox.equal(containerBox);
        this._lastBox = containerBox.copy();

        let layoutChanged = false;
        if (this._layout === null) {
            this._layout = this._createBestLayout(this._workarea);
            layoutChanged = true;
        }

        if (layoutChanged || containerAllocationChanged)
            this._windowSlots = this._getWindowSlots(box.copy());

        const childBox = new Clutter.ActorBox();

        const nSlots = this._windowSlots.length;
        for (let i = 0; i < nSlots; i++) {
            let [x, y, width, height, child] = this._windowSlots[i];

            childBox.set_origin(x, y);
            childBox.set_size(width, height);

            child.allocate(childBox);
        }
    }

    addWindow(window) {
        if (this._sortedWindows.includes(window))
            return;

        this._sortedWindows.push(window);

        this._container.add_child(window);

        this._layout = null;
        this.layout_changed();
    }

    reset() {
        for (const window of this._sortedWindows)
            window.destroy();

        this._sortedWindows = [];
        this._windowSlots = [];
        this._layout = null;
    }

    get windows() {
        return this._sortedWindows;
    }
});

var UIWindowSelectorWindow = GObject.registerClass(
class UIWindowSelectorWindow extends St.Button {
    _init(actor, params) {
        super._init(params);

        const window = actor.metaWindow;
        this._boundingBox = window.get_frame_rect();
        this._bufferRect = window.get_buffer_rect();
        this._bufferScale = actor.get_resource_scale();
        this._actor = new Clutter.Actor({
            content: actor.paint_to_content(null),
        });
        this.add_child(this._actor);

        this._border = new St.Bin({ style_class: 'screenshot-ui-window-selector-window-border' });
        this._border.connect('style-changed', () => {
            this._borderSize =
                this._border.get_theme_node().get_border_width(St.Side.TOP);
        });
        this.add_child(this._border);

        this._border.child = new St.Icon({
            icon_name: 'object-select-symbolic',
            style_class: 'screenshot-ui-window-selector-check',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._cursor = null;
        this._cursorPoint = { x: 0, y: 0 };
        this._shouldShowCursor = window.has_pointer && window.has_pointer();

        this.connect('destroy', this._onDestroy.bind(this));
    }

    get boundingBox() {
        return this._boundingBox;
    }

    get windowCenter() {
        const boundingBox = this.boundingBox;
        return {
            x: boundingBox.x + boundingBox.width / 2,
            y: boundingBox.y + boundingBox.height / 2,
        };
    }

    chromeHeights() {
        return [0, 0];
    }

    chromeWidths() {
        return [0, 0];
    }

    overlapHeights() {
        return [0, 0];
    }

    get cursorPoint() {
        return {
            x: this._cursorPoint.x + this._boundingBox.x - this._bufferRect.x,
            y: this._cursorPoint.y + this._boundingBox.y - this._bufferRect.y,
        };
    }

    get bufferScale() {
        return this._bufferScale;
    }

    get windowContent() {
        return this._actor.content;
    }

    _onDestroy() {
        this.remove_child(this._actor);
        this._actor.destroy();
        this._actor = null;
        this.remove_child(this._border);
        this._border.destroy();
        this._border = null;

        if (this._cursor) {
            this.remove_child(this._cursor);
            this._cursor.destroy();
            this._cursor = null;
        }
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        // Border goes around the window.
        const borderBox = box.copy();
        borderBox.set_origin(0, 0);
        borderBox.x1 -= this._borderSize;
        borderBox.y1 -= this._borderSize;
        borderBox.x2 += this._borderSize;
        borderBox.y2 += this._borderSize;
        this._border.allocate(borderBox);

        // box should contain this._boundingBox worth of window. Compute
        // origin and size for the actor box to satisfy that.
        const xScale = box.get_width() / this._boundingBox.width;
        const yScale = box.get_height() / this._boundingBox.height;

        const [, windowW, windowH] = this._actor.content.get_preferred_size();

        const actorBox = new Clutter.ActorBox();
        actorBox.set_origin(
            (this._bufferRect.x - this._boundingBox.x) * xScale,
            (this._bufferRect.y - this._boundingBox.y) * yScale
        );
        actorBox.set_size(
            windowW * xScale / this._bufferScale,
            windowH * yScale / this._bufferScale
        );
        this._actor.allocate(actorBox);

        // Allocate the cursor if we have one.
        if (!this._cursor)
            return;

        let [, , w, h] = this._cursor.get_preferred_size();
        w *= this._cursorScale;
        h *= this._cursorScale;

        const cursorBox = new Clutter.ActorBox({
            x1: this._cursorPoint.x,
            y1: this._cursorPoint.y,
            x2: this._cursorPoint.x + w,
            y2: this._cursorPoint.y + h,
        });
        cursorBox.x1 *= xScale;
        cursorBox.x2 *= xScale;
        cursorBox.y1 *= yScale;
        cursorBox.y2 *= yScale;

        this._cursor.allocate(cursorBox);
    }

    addCursorTexture(content, point, scale) {
        if (!this._shouldShowCursor)
            return;

        // Add the cursor.
        this._cursor = new St.Widget({
            content,
            request_mode: Clutter.RequestMode.CONTENT_SIZE,
        });

        this._cursorPoint = {
            x: point.x - this._boundingBox.x,
            y: point.y - this._boundingBox.y,
        };
        this._cursorScale = scale;

        this.insert_child_below(this._cursor, this._border);
    }

    getCursorTexture() {
        return this._cursor?.content;
    }

    setCursorVisible(visible) {
        if (!this._cursor)
            return;

        this._cursor.visible = visible;
    }
});

var UIWindowSelector = GObject.registerClass(
class UIWindowSelector extends St.Widget {
    _init(monitorIndex, params) {
        super._init(params);
        super.layout_manager = new Clutter.BinLayout();

        this._monitorIndex = monitorIndex;

        this._layoutManager = new UIWindowSelectorLayout(monitorIndex);

        // Window screenshots
        this._container = new St.Widget({
            style_class: 'screenshot-ui-window-selector-window-container',
            x_expand: true,
            y_expand: true,
        });
        this._container.layout_manager = this._layoutManager;
        this.add_child(this._container);
    }

    capture() {
        for (const actor of global.get_window_actors()) {
            let window = actor.metaWindow;
            let workspaceManager = global.workspace_manager;
            let activeWorkspace = workspaceManager.get_active_workspace();
            if (window.is_override_redirect() ||
                !window.located_on_workspace(activeWorkspace) ||
                window.get_monitor() !== this._monitorIndex)
                continue;

            const widget = new UIWindowSelectorWindow(
                actor,
                {
                    style_class: 'screenshot-ui-window-selector-window',
                    reactive: true,
                    can_focus: true,
                    toggle_mode: true,
                }
            );

            widget.connect('key-focus-in', win => {
                Main.screenshotUI.grab_key_focus();
                win.checked = true;
            });

            if (window.has_focus()) {
                widget.checked = true;
                widget.toggle_mode = false;
            }

            this._layoutManager.addWindow(widget);
        }
    }

    reset() {
        this._layoutManager.reset();
    }

    windows() {
        return this._layoutManager.windows;
    }
});

const UIMode = {
    SCREENSHOT: 0,
    SCREENCAST: 1,
};

var ScreenshotUI = GObject.registerClass({
    Properties: {
        'screencast-in-progress': GObject.ParamSpec.boolean(
            'screencast-in-progress',
            'screencast-in-progress',
            'screencast-in-progress',
            GObject.ParamFlags.READABLE,
            false),
    },
}, class ScreenshotUI extends St.Widget {
    _init() {
        super._init({
            name: 'screenshot-ui',
            constraints: new Clutter.BindConstraint({
                source: global.stage,
                coordinate: Clutter.BindCoordinate.ALL,
            }),
            layout_manager: new Clutter.BinLayout(),
            opacity: 0,
            visible: false,
            reactive: true,
        });

        this._screencastInProgress = false;
        this._screencastSupported = false;

        this._screencastProxy = new ScreencastProxy(
            Gio.DBus.session,
            'org.gnome.Shell.Screencast',
            '/org/gnome/Shell/Screencast',
            (object, error) => {
                if (error !== null) {
                    log('Error connecting to the screencast service');
                    return;
                }

                this._screencastSupported = this._screencastProxy.ScreencastSupported;
                this._castButton.visible = this._screencastSupported;
            });

        this._screencastProxy.connectSignal('Error',
            () => this._screencastFailed());

        this._screencastProxy.connect('notify::g-name-owner', () => {
            if (this._screencastProxy.g_name_owner)
                return;

            if (!this._screencastInProgress)
                return;

            this._screencastFailed();
        });

        this._lockdownSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });

        // The full-screen screenshot has a separate container so that we can
        // show it without the screenshot UI fade-in for a nicer animation.
        this._stageScreenshotContainer = new St.Widget({ visible: false });
        this._stageScreenshotContainer.add_constraint(new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        }));
        Main.layoutManager.screenshotUIGroup.add_child(
            this._stageScreenshotContainer);

        this._screencastAreaIndicator = new UIAreaIndicator({
            style_class: 'screenshot-ui-screencast-area-indicator',
            visible: false,
        });
        this._screencastAreaIndicator.add_constraint(new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        }));
        this.bind_property(
            'screencast-in-progress',
            this._screencastAreaIndicator,
            'visible',
            GObject.BindingFlags.DEFAULT);
        // Add it directly to the stage so that it's above popup menus.
        global.stage.add_child(this._screencastAreaIndicator);
        Shell.util_set_hidden_from_pick(this._screencastAreaIndicator, true);

        Main.layoutManager.screenshotUIGroup.add_child(this);

        this._stageScreenshot = new St.Widget({ style_class: 'screenshot-ui-screen-screenshot' });
        this._stageScreenshot.add_constraint(new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        }));
        this._stageScreenshotContainer.add_child(this._stageScreenshot);

        this._cursor = new St.Widget();
        this._stageScreenshotContainer.add_child(this._cursor);

        this._openingCoroutineInProgress = false;
        this._grabHelper = new GrabHelper.GrabHelper(this, {
            actionMode: Shell.ActionMode.POPUP,
        });

        this._areaSelector = new UIAreaSelector({
            style_class: 'screenshot-ui-area-selector',
            x_expand: true,
            y_expand: true,
            reactive: true,
        });
        this.add_child(this._areaSelector);

        this._primaryMonitorBin = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._primaryMonitorBin.add_constraint(
            new Layout.MonitorConstraint({ 'primary': true }));
        this.add_child(this._primaryMonitorBin);

        this._panel = new St.BoxLayout({
            style_class: 'screenshot-ui-panel',
            y_align: Clutter.ActorAlign.END,
            y_expand: true,
            vertical: true,
            offscreen_redirect: Clutter.OffscreenRedirect.AUTOMATIC_FOR_OPACITY,
        });
        this._primaryMonitorBin.add_child(this._panel);

        this._closeButton = new St.Button({
            style_class: 'screenshot-ui-close-button',
            icon_name: 'preview-close-symbolic',
        });
        this._closeButton.add_constraint(new Clutter.BindConstraint({
            source: this._panel,
            coordinate: Clutter.BindCoordinate.POSITION,
        }));
        this._closeButton.add_constraint(new Clutter.AlignConstraint({
            source: this._panel,
            align_axis: Clutter.AlignAxis.Y_AXIS,
            pivot_point: new Graphene.Point({ x: -1, y: 0.5 }),
            factor: 0,
        }));
        this._closeButtonXAlignConstraint = new Clutter.AlignConstraint({
            source: this._panel,
            align_axis: Clutter.AlignAxis.X_AXIS,
            pivot_point: new Graphene.Point({ x: 0.5, y: -1 }),
        });
        this._closeButton.add_constraint(this._closeButtonXAlignConstraint);
        this._closeButton.connect('clicked', () => this.close());
        this._primaryMonitorBin.add_child(this._closeButton);

        this._areaSelector.connect('drag-started', () => {
            this._panel.ease({
                opacity: 100,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
            this._closeButton.ease({
                opacity: 100,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        });
        this._areaSelector.connect('drag-ended', () => {
            this._panel.ease({
                opacity: 255,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
            this._closeButton.ease({
                opacity: 255,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        });

        this._typeButtonContainer = new St.Widget({
            style_class: 'screenshot-ui-type-button-container',
            layout_manager: new Clutter.BoxLayout({
                spacing: 12,
                homogeneous: true,
            }),
        });
        this._panel.add_child(this._typeButtonContainer);

        this._selectionButton = new IconLabelButton('screenshot-ui-area-symbolic', _('Selection'), {
            style_class: 'screenshot-ui-type-button',
            checked: true,
            x_expand: true,
        });
        this._selectionButton.connect('notify::checked',
            this._onSelectionButtonToggled.bind(this));
        this._typeButtonContainer.add_child(this._selectionButton);

        this.add_child(new Tooltip(this._selectionButton, {
            text: _('Area Selection'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        }));

        this._screenButton = new IconLabelButton('screenshot-ui-display-symbolic', _('Screen'), {
            style_class: 'screenshot-ui-type-button',
            toggle_mode: true,
            x_expand: true,
        });
        this._screenButton.connect('notify::checked',
            this._onScreenButtonToggled.bind(this));
        this._typeButtonContainer.add_child(this._screenButton);

        this.add_child(new Tooltip(this._screenButton, {
            text: _('Screen Selection'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        }));

        this._windowButton = new IconLabelButton('screenshot-ui-window-symbolic', _('Window'), {
            style_class: 'screenshot-ui-type-button',
            toggle_mode: true,
            x_expand: true,
        });
        this._windowButton.connect('notify::checked',
            this._onWindowButtonToggled.bind(this));
        this._typeButtonContainer.add_child(this._windowButton);

        this.add_child(new Tooltip(this._windowButton, {
            text: _('Window Selection'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        }));

        this._bottomRowContainer = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._panel.add_child(this._bottomRowContainer);

        this._shotCastContainer = new St.BoxLayout({
            style_class: 'screenshot-ui-shot-cast-container',
            x_align: Clutter.ActorAlign.START,
            x_expand: true,
        });
        this._bottomRowContainer.add_child(this._shotCastContainer);

        this._shotButton = new St.Button({
            style_class: 'screenshot-ui-shot-cast-button',
            icon_name: 'camera-photo-symbolic',
            checked: true,
        });
        this._shotButton.connect('notify::checked',
            this._onShotButtonToggled.bind(this));
        this._shotCastContainer.add_child(this._shotButton);

        this._castButton = new St.Button({
            style_class: 'screenshot-ui-shot-cast-button',
            icon_name: 'camera-web-symbolic',
            toggle_mode: true,
            visible: false,
        });
        this._castButton.connect('notify::checked',
            this._onCastButtonToggled.bind(this));
        this._shotCastContainer.add_child(this._castButton);

        this._shotButton.bind_property('checked', this._castButton, 'checked',
            GObject.BindingFlags.BIDIRECTIONAL | GObject.BindingFlags.INVERT_BOOLEAN);

        this._shotCastTooltip = new Tooltip(this._shotCastContainer, {
            text: _('Screenshot / Screencast'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        });
        const shotCastCallback = () => {
            if (this._shotButton.hover || this._castButton.hover)
                this._shotCastTooltip.open();
            else
                this._shotCastTooltip.close();
        };
        this._shotButton.connect('notify::hover', shotCastCallback);
        this._castButton.connect('notify::hover', shotCastCallback);
        this.add_child(this._shotCastTooltip);

        this._captureButton = new St.Button({ style_class: 'screenshot-ui-capture-button' });
        this._captureButton.set_child(new St.Widget({
            style_class: 'screenshot-ui-capture-button-circle',
        }));
        this.add_child(new Tooltip(this._captureButton, {
            /* Translators: since this string refers to an action,
            it needs to be phrased as a verb. */
            text: _('Capture'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        }));
        this._captureButton.connect('clicked',
            this._onCaptureButtonClicked.bind(this));
        this._bottomRowContainer.add_child(this._captureButton);

        this._showPointerButtonContainer = new St.BoxLayout({
            x_align: Clutter.ActorAlign.END,
            x_expand: true,
        });
        this._bottomRowContainer.add_child(this._showPointerButtonContainer);

        this._showPointerButton = new St.Button({
            style_class: 'screenshot-ui-show-pointer-button',
            icon_name: 'screenshot-ui-show-pointer-symbolic',
            toggle_mode: true,
        });
        this._showPointerButtonContainer.add_child(this._showPointerButton);

        this.add_child(new Tooltip(this._showPointerButton, {
            text: _('Show Pointer'),
            style_class: 'screenshot-ui-tooltip',
            visible: false,
        }));

        this._showPointerButton.connect('notify::checked', () => {
            const state = this._showPointerButton.checked;
            this._cursor.visible = state;

            const windows =
                this._windowSelectors.flatMap(selector => selector.windows());
            for (const window of windows)
                window.setCursorVisible(state);
        });
        this._cursor.visible = false;

        this._monitorBins = [];
        this._windowSelectors = [];
        this._rebuildMonitorBins();

        Main.layoutManager.connect('monitors-changed', () => {
            // Nope, not dealing with monitor changes.
            this.close(true);
            this._rebuildMonitorBins();
        });

        const uiModes =
            Shell.ActionMode.ALL & ~Shell.ActionMode.LOGIN_SCREEN;
        const restrictedModes =
            uiModes &
            ~(Shell.ActionMode.LOCK_SCREEN | Shell.ActionMode.UNLOCK_SCREEN);

        Main.wm.addKeybinding(
            'show-screenshot-ui',
            new Gio.Settings({ schema_id: 'org.gnome.shell.keybindings' }),
            Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
            uiModes,
            showScreenshotUI
        );

        Main.wm.addKeybinding(
            'show-screen-recording-ui',
            new Gio.Settings({ schema_id: 'org.gnome.shell.keybindings' }),
            Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
            restrictedModes,
            showScreenRecordingUI
        );

        Main.wm.addKeybinding(
            'screenshot-window',
            new Gio.Settings({ schema_id: 'org.gnome.shell.keybindings' }),
            Meta.KeyBindingFlags.IGNORE_AUTOREPEAT | Meta.KeyBindingFlags.PER_WINDOW,
            restrictedModes,
            async (_display, window, _binding) => {
                try {
                    const actor = window.get_compositor_private();
                    const content = actor.paint_to_content(null);
                    const texture = content.get_texture();

                    await captureScreenshot(texture, null, 1, null);
                } catch (e) {
                    logError(e, 'Error capturing screenshot');
                }
            }
        );

        Main.wm.addKeybinding(
            'screenshot',
            new Gio.Settings({ schema_id: 'org.gnome.shell.keybindings' }),
            Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
            uiModes,
            async () => {
                try {
                    const shooter = new Shell.Screenshot();
                    const [content] = await shooter.screenshot_stage_to_content();
                    const texture = content.get_texture();

                    await captureScreenshot(texture, null, 1, null);
                } catch (e) {
                    logError(e, 'Error capturing screenshot');
                }
            }
        );

        Main.sessionMode.connect('updated',
            () => this._sessionUpdated());
        this._sessionUpdated();
    }

    _sessionUpdated() {
        this.close(true);
        this._castButton.reactive = Main.sessionMode.allowScreencast;
    }

    _refreshButtonLayout() {
        const buttonLayout = Meta.prefs_get_button_layout();

        this._closeButton.remove_style_class_name('left');
        this._closeButton.remove_style_class_name('right');

        if (buttonLayout.left_buttons.includes(Meta.ButtonFunction.CLOSE)) {
            this._closeButton.add_style_class_name('left');
            this._closeButtonXAlignConstraint.factor = 0;
        } else {
            this._closeButton.add_style_class_name('right');
            this._closeButtonXAlignConstraint.factor = 1;
        }
    }

    _rebuildMonitorBins() {
        for (const bin of this._monitorBins)
            bin.destroy();

        this._monitorBins = [];
        this._windowSelectors = [];
        this._screenSelectors = [];

        for (let i = 0; i < Main.layoutManager.monitors.length; i++) {
            const bin = new St.Widget({
                layout_manager: new Clutter.BinLayout(),
            });
            bin.add_constraint(new Layout.MonitorConstraint({ 'index': i }));
            this.insert_child_below(bin, this._primaryMonitorBin);
            this._monitorBins.push(bin);

            const windowSelector = new UIWindowSelector(i, {
                style_class: 'screenshot-ui-window-selector',
                x_expand: true,
                y_expand: true,
                visible: this._windowButton.checked,
            });
            if (i === Main.layoutManager.primaryIndex)
                windowSelector.add_style_pseudo_class('primary-monitor');

            bin.add_child(windowSelector);
            this._windowSelectors.push(windowSelector);

            const screenSelector = new St.Button({
                style_class: 'screenshot-ui-screen-selector',
                x_expand: true,
                y_expand: true,
                visible: this._screenButton.checked,
                reactive: true,
                can_focus: true,
                toggle_mode: true,
            });
            screenSelector.connect('key-focus-in', () => {
                this.grab_key_focus();
                screenSelector.checked = true;
            });
            bin.add_child(screenSelector);
            this._screenSelectors.push(screenSelector);

            screenSelector.connect('notify::checked', () => {
                if (!screenSelector.checked)
                    return;

                screenSelector.toggle_mode = false;

                for (const otherSelector of this._screenSelectors) {
                    if (screenSelector === otherSelector)
                        continue;

                    otherSelector.toggle_mode = true;
                    otherSelector.checked = false;
                }
            });
        }

        if (Main.layoutManager.primaryIndex !== -1)
            this._screenSelectors[Main.layoutManager.primaryIndex].checked = true;
    }

    async open(mode = UIMode.SCREENSHOT) {
        if (this._openingCoroutineInProgress)
            return;

        if (this._screencastInProgress)
            return;

        if (mode === UIMode.SCREENCAST && !this._screencastSupported)
            return;

        this._castButton.checked = mode === UIMode.SCREENCAST;

        if (!this.visible) {
            // Screenshot UI is opening from completely closed state
            // (rather than opening back from in process of closing).
            for (const selector of this._windowSelectors)
                selector.capture();

            const windows =
                this._windowSelectors.flatMap(selector => selector.windows());
            for (const window of windows) {
                window.connect('notify::checked', () => {
                    if (!window.checked)
                        return;

                    window.toggle_mode = false;

                    for (const otherWindow of windows) {
                        if (window === otherWindow)
                            continue;

                        otherWindow.toggle_mode = true;
                        otherWindow.checked = false;
                    }
                });
            }

            this._windowButton.reactive =
                Main.sessionMode.hasWindows &&
                windows.length > 0 &&
                !this._castButton.checked;
            if (!this._windowButton.reactive)
                this._selectionButton.checked = true;

            this._shooter = new Shell.Screenshot();

            this._openingCoroutineInProgress = true;
            try {
                const [content, scale, cursorContent, cursorPoint, cursorScale] =
                    await this._shooter.screenshot_stage_to_content();
                this._stageScreenshot.set_content(content);
                this._scale = scale;

                if (cursorContent !== null) {
                    this._cursor.set_content(cursorContent);
                    this._cursor.set_position(cursorPoint.x, cursorPoint.y);

                    let [, w, h] = cursorContent.get_preferred_size();
                    w *= cursorScale;
                    h *= cursorScale;
                    this._cursor.set_size(w, h);

                    this._cursorScale = cursorScale;

                    for (const window of windows) {
                        window.addCursorTexture(cursorContent, cursorPoint, cursorScale);
                        window.setCursorVisible(this._showPointerButton.checked);
                    }
                }

                this._stageScreenshotContainer.show();
            } catch (e) {
                log(`Error capturing screenshot: ${e.message}`);
            }
            this._openingCoroutineInProgress = false;
        }

        // Get rid of any popup menus.
        // We already have them captured on the screenshot anyway.
        //
        // This needs to happen before the grab below as closing menus will
        // pop their grabs.
        Main.layoutManager.emit('system-modal-opened');

        const { screenshotUIGroup } = Main.layoutManager;
        screenshotUIGroup.get_parent().set_child_above_sibling(
            screenshotUIGroup, null);

        const grabResult = this._grabHelper.grab({
            actor: this,
            onUngrab: () => this.close(),
        });
        if (!grabResult) {
            this.close(true);
            return;
        }

        this._refreshButtonLayout();

        this.remove_all_transitions();
        this.visible = true;
        this.ease({
            opacity: 255,
            duration: 200,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._stageScreenshotContainer.get_parent().remove_child(
                    this._stageScreenshotContainer);
                this.insert_child_at_index(this._stageScreenshotContainer, 0);
            },
        });
    }

    _finishClosing() {
        this.hide();

        this._shooter = null;

        // Switch back to screenshot mode.
        this._shotButton.checked = true;

        this._stageScreenshotContainer.get_parent().remove_child(
            this._stageScreenshotContainer);
        Main.layoutManager.screenshotUIGroup.insert_child_at_index(
            this._stageScreenshotContainer, 0);
        this._stageScreenshotContainer.hide();

        this._stageScreenshot.set_content(null);
        this._cursor.set_content(null);

        this._areaSelector.reset();
        for (const selector of this._windowSelectors)
            selector.reset();
    }

    close(instantly = false) {
        this._grabHelper.ungrab();

        if (instantly) {
            this._finishClosing();
            return;
        }

        this.remove_all_transitions();
        this.ease({
            opacity: 0,
            duration: 200,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: this._finishClosing.bind(this),
        });
    }

    _onSelectionButtonToggled() {
        if (this._selectionButton.checked) {
            this._selectionButton.toggle_mode = false;
            this._windowButton.checked = false;
            this._screenButton.checked = false;

            this._areaSelector.show();
            this._areaSelector.remove_all_transitions();
            this._areaSelector.reactive = true;
            this._areaSelector.ease({
                opacity: 255,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            this._selectionButton.toggle_mode = true;

            this._areaSelector.stopDrag();
            global.display.set_cursor(Meta.Cursor.DEFAULT);

            this._areaSelector.remove_all_transitions();
            this._areaSelector.reactive = false;
            this._areaSelector.ease({
                opacity: 0,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._areaSelector.hide(),
            });
        }
    }

    _onScreenButtonToggled() {
        if (this._screenButton.checked) {
            this._screenButton.toggle_mode = false;
            this._selectionButton.checked = false;
            this._windowButton.checked = false;

            for (const selector of this._screenSelectors) {
                selector.show();
                selector.remove_all_transitions();
                selector.ease({
                    opacity: 255,
                    duration: 200,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            }
        } else {
            this._screenButton.toggle_mode = true;

            for (const selector of this._screenSelectors) {
                selector.remove_all_transitions();
                selector.ease({
                    opacity: 0,
                    duration: 200,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => selector.hide(),
                });
            }
        }
    }

    _onWindowButtonToggled() {
        if (this._windowButton.checked) {
            this._windowButton.toggle_mode = false;
            this._selectionButton.checked = false;
            this._screenButton.checked = false;

            for (const selector of this._windowSelectors) {
                selector.show();
                selector.remove_all_transitions();
                selector.ease({
                    opacity: 255,
                    duration: 200,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            }
        } else {
            this._windowButton.toggle_mode = true;

            for (const selector of this._windowSelectors) {
                selector.remove_all_transitions();
                selector.ease({
                    opacity: 0,
                    duration: 200,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => selector.hide(),
                });
            }
        }
    }

    _onShotButtonToggled() {
        if (this._shotButton.checked) {
            this._shotButton.toggle_mode = false;

            this._stageScreenshotContainer.show();
            this._stageScreenshotContainer.remove_all_transitions();
            this._stageScreenshotContainer.ease({
                opacity: 255,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            this._shotButton.toggle_mode = true;
        }
    }

    _onCastButtonToggled() {
        if (this._castButton.checked) {
            this._castButton.toggle_mode = false;

            this._captureButton.add_style_pseudo_class('cast');

            this._stageScreenshotContainer.remove_all_transitions();
            this._stageScreenshotContainer.ease({
                opacity: 0,
                duration: 200,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._stageScreenshotContainer.hide(),
            });

            // Screen recording doesn't support window selection yet.
            if (this._windowButton.checked)
                this._selectionButton.checked = true;

            this._windowButton.reactive = false;
        } else {
            this._castButton.toggle_mode = true;

            this._captureButton.remove_style_pseudo_class('cast');

            const windows =
                this._windowSelectors.flatMap(selector => selector.windows());
            this._windowButton.reactive = windows.length > 0;
        }
    }

    _getSelectedGeometry(rescale) {
        let x, y, w, h;

        if (this._selectionButton.checked) {
            [x, y, w, h] = this._areaSelector.getGeometry();
        } else if (this._screenButton.checked) {
            const index =
                this._screenSelectors.findIndex(screen => screen.checked);
            const monitor = Main.layoutManager.monitors[index];

            x = monitor.x;
            y = monitor.y;
            w = monitor.width;
            h = monitor.height;
        }

        if (rescale) {
            x *= this._scale;
            y *= this._scale;
            w *= this._scale;
            h *= this._scale;
        }

        return [x, y, w, h];
    }

    _onCaptureButtonClicked() {
        if (this._shotButton.checked) {
            this._saveScreenshot();
            this.close();
        } else {
            // Screencast closes the UI on its own.
            this._startScreencast();
        }
    }

    _saveScreenshot() {
        if (this._selectionButton.checked || this._screenButton.checked) {
            const content = this._stageScreenshot.get_content();
            if (!content)
                return; // Failed to capture the screenshot for some reason.

            const texture = content.get_texture();
            const geometry = this._getSelectedGeometry(true);

            let cursorTexture = this._cursor.content?.get_texture();
            if (!this._cursor.visible)
                cursorTexture = null;

            captureScreenshot(
                texture, geometry, this._scale,
                {
                    texture: cursorTexture ?? null,
                    x: this._cursor.x * this._scale,
                    y: this._cursor.y * this._scale,
                    scale: this._cursorScale,
                }
            ).catch(e => logError(e, 'Error capturing screenshot'));
        } else if (this._windowButton.checked) {
            const window =
                this._windowSelectors.flatMap(selector => selector.windows())
                                     .find(win => win.checked);
            if (!window)
                return;

            const content = window.windowContent;
            if (!content)
                return;

            const texture = content.get_texture();

            let cursorTexture = window.getCursorTexture()?.get_texture();
            if (!this._cursor.visible)
                cursorTexture = null;

            captureScreenshot(
                texture,
                null,
                window.bufferScale,
                {
                    texture: cursorTexture ?? null,
                    x: window.cursorPoint.x * window.bufferScale,
                    y: window.cursorPoint.y * window.bufferScale,
                    scale: this._cursorScale,
                }
            ).catch(e => logError(e, 'Error capturing screenshot'));
        }
    }

    async _startScreencast() {
        if (this._windowButton.checked)
            return; // TODO

        const [x, y, w, h] = this._getSelectedGeometry(false);
        const drawCursor = this._cursor.visible;

        // Set up the screencast indicator rect.
        if (this._selectionButton.checked) {
            this._screencastAreaIndicator.setSelectionRect(
                ...this._areaSelector.getGeometry());
        } else if (this._screenButton.checked) {
            const index =
                this._screenSelectors.findIndex(screen => screen.checked);
            const monitor = Main.layoutManager.monitors[index];

            this._screencastAreaIndicator.setSelectionRect(
                monitor.x, monitor.y, monitor.width, monitor.height);
        }

        // Close instantly so the fade-out doesn't get recorded.
        this.close(true);

        // This is a bit awkward because creating a proxy synchronously hangs Shell.
        let method =
            this._screencastProxy.ScreencastAsync.bind(this._screencastProxy);
        if (w !== -1) {
            method = this._screencastProxy.ScreencastAreaAsync.bind(
                this._screencastProxy, x, y, w, h);
        }

        // Set this before calling the method as the screen recording indicator
        // will check it before the success callback fires.
        this._setScreencastInProgress(true);

        try {
            const [success, path] = await method(
                GLib.build_filenamev([
                    /* Translators: this is the folder where recorded
                       screencasts are stored. */
                    _('Screencasts'),
                    /* Translators: this is a filename used for screencast
                     * recording, where "%d" and "%t" date and time, e.g.
                     * "Screencast from 07-17-2013 10:00:46 PM.webm" */
                    /* xgettext:no-c-format */
                    _('Screencast from %d %t.webm'),
                ]),
                {'draw-cursor': new GLib.Variant('b', drawCursor)});
            if (!success)
                throw new Error();
            this._screencastPath = path;
        } catch (error) {
            this._setScreencastInProgress(false);
            const {message} = error;
            if (message)
                log(`Error starting screencast: ${message}`);
            else
                log('Error starting screencast');
        }
    }

    async stopScreencast() {
        if (!this._screencastInProgress)
            return;

        // Set this before calling the method as the screen recording indicator
        // will check it before the success callback fires.
        this._setScreencastInProgress(false);

        try {
            const [success] = await this._screencastProxy.StopScreencastAsync();
            if (!success)
                throw new Error();
        } catch (error) {
            const {message} = error;
            if (message)
                log(`Error stopping screencast: ${message}`);
            else
                log('Error stopping screencast');
            return;
        }

        // Translators: notification title.
        this._showNotification(_('Screencast recorded'));
    }

    _screencastFailed() {
        this._setScreencastInProgress(false);

        // Translators: notification title.
        this._showNotification(_('Error'));
    }

    _showNotification(title) {
        // Show a notification.
        const file = Gio.file_new_for_path(this._screencastPath);

        const source = new MessageTray.Source(
            // Translators: notification source name.
            _('Screenshot'),
            'screencast-recorded-symbolic'
        );
        const notification = new MessageTray.Notification(
            source,
            title,
            // Translators: notification body when a screencast was recorded.
            _('Click here to view the video.')
        );
        // Translators: button on the screencast notification.
        notification.addAction(_('Show in Files'), () => {
            const app =
                Gio.app_info_get_default_for_type('inode/directory', false);

            if (app === null) {
                // It may be null e.g. in a toolbox without nautilus.
                log('Error showing in files: no default app set for inode/directory');
                return;
            }

            app.launch([file], global.create_app_launch_context(0, -1));
        });
        notification.connect('activated', () => {
            try {
                Gio.app_info_launch_default_for_uri(
                    file.get_uri(), global.create_app_launch_context(0, -1));
            } catch (err) {
                logError(err, 'Error opening screencast');
            }
        });
        notification.setTransient(true);

        Main.messageTray.add(source);
        source.showNotification(notification);
    }

    get screencast_in_progress() {
        return this._screencastInProgress;
    }

    _setScreencastInProgress(inProgress) {
        if (this._screencastInProgress === inProgress)
            return;

        this._screencastInProgress = inProgress;
        this.notify('screencast-in-progress');
    }

    vfunc_key_press_event(event) {
        const symbol = event.keyval;
        if (symbol === Clutter.KEY_Return || symbol === Clutter.KEY_space ||
            ((event.modifier_state & Clutter.ModifierType.CONTROL_MASK) &&
             (symbol === Clutter.KEY_c || symbol === Clutter.KEY_C))) {
            this._onCaptureButtonClicked();
            return Clutter.EVENT_STOP;
        }

        if (symbol === Clutter.KEY_s || symbol === Clutter.KEY_S) {
            this._selectionButton.checked = true;
            return Clutter.EVENT_STOP;
        }

        if (symbol === Clutter.KEY_c || symbol === Clutter.KEY_C) {
            this._screenButton.checked = true;
            return Clutter.EVENT_STOP;
        }

        if (this._windowButton.reactive &&
            (symbol === Clutter.KEY_w || symbol === Clutter.KEY_W)) {
            this._windowButton.checked = true;
            return Clutter.EVENT_STOP;
        }

        if (symbol === Clutter.KEY_p || symbol === Clutter.KEY_P) {
            this._showPointerButton.checked = !this._showPointerButton.checked;
            return Clutter.EVENT_STOP;
        }

        if (symbol === Clutter.KEY_v || symbol === Clutter.KEY_V) {
            this._castButton.checked = !this._castButton.checked;
            return Clutter.EVENT_STOP;
        }

        if (symbol === Clutter.KEY_Left || symbol === Clutter.KEY_Right ||
            symbol === Clutter.KEY_Up || symbol === Clutter.KEY_Down) {
            let direction;
            if (symbol === Clutter.KEY_Left)
                direction = St.DirectionType.LEFT;
            else if (symbol === Clutter.KEY_Right)
                direction = St.DirectionType.RIGHT;
            else if (symbol === Clutter.KEY_Up)
                direction = St.DirectionType.UP;
            else if (symbol === Clutter.KEY_Down)
                direction = St.DirectionType.DOWN;

            if (this._windowButton.checked) {
                const window =
                    this._windowSelectors.flatMap(selector => selector.windows())
                        .find(win => win.checked) ?? null;
                this.navigate_focus(window, direction, false);
            } else if (this._screenButton.checked) {
                const screen =
                    this._screenSelectors.find(selector => selector.checked) ?? null;
                this.navigate_focus(screen, direction, false);
            }

            return Clutter.EVENT_STOP;
        }

        return super.vfunc_key_press_event(event);
    }
});

/**
 * Stores a PNG-encoded screenshot into the clipboard and a file, and shows a
 * notification.
 *
 * @param {GLib.Bytes} bytes - The PNG-encoded screenshot.
 * @param {GdkPixbuf.Pixbuf} pixbuf - The Pixbuf with the screenshot.
 */
function _storeScreenshot(bytes, pixbuf) {
    // Store to the clipboard first in case storing to file fails.
    const clipboard = St.Clipboard.get_default();
    clipboard.set_content(St.ClipboardType.CLIPBOARD, 'image/png', bytes);

    const time = GLib.DateTime.new_now_local();

    // This will be set in the first save to disk branch and then accessed
    // in the second save to disk branch, so we need to declare it outside.
    let file;

    // The function is declared here rather than inside the condition to
    // satisfy eslint.

    /**
     * Returns a filename suffix with an increasingly large index.
     *
     * @returns {Generator<string|*, void, *>} suffix string
     */
    function* suffixes() {
        yield '';

        for (let i = 1; ; i++)
            yield `-${i}`;
    }

    /**
     * Adds a record of a screenshot file in the recently used files list.
     *
     * @param {Gio.File} screenshotFile - The screenshot file.
     */
    function saveRecentFile(screenshotFile) {
        const recentFile =
            GLib.build_filenamev([GLib.get_user_data_dir(), 'recently-used.xbel']);
        const uri = screenshotFile.get_uri();
        const bookmarks = new GLib.BookmarkFile();
        try {
            bookmarks.load_from_file(recentFile);
        } catch (e) {
            if (!e.matches(GLib.BookmarkFileError, GLib.BookmarkFileError.FILE_NOT_FOUND)) {
                log(`Could not open recent file ${uri}: ${e.message}`);
                return;
            }
        }

        try {
            bookmarks.add_application(uri, GLib.get_prgname(), 'gio open %u');
            bookmarks.to_file(recentFile);
        } catch (e) {
            log(`Could not save recent file ${uri}: ${e.message}`);
        }
    }

    const lockdownSettings =
        new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });
    const disableSaveToDisk =
        lockdownSettings.get_boolean('disable-save-to-disk');

    if (!disableSaveToDisk) {
        const dir = Gio.File.new_for_path(GLib.build_filenamev([
            GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_PICTURES) || GLib.get_home_dir(),
            // Translators: name of the folder under ~/Pictures for screenshots.
            _('Screenshots'),
        ]));

        try {
            dir.make_directory_with_parents(null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                throw e;
        }

        const timestamp = time.format('%Y-%m-%d %H-%M-%S');
        // Translators: this is the name of the file that the screenshot is
        // saved to. The placeholder is a timestamp, e.g. "2017-05-21 12-24-03".
        const name = _('Screenshot from %s').format(timestamp);

        // If the target file already exists, try appending a suffix with an
        // increasing number to it.
        for (const suffix of suffixes()) {
            file = Gio.File.new_for_path(GLib.build_filenamev([
                dir.get_path(), `${name}${suffix}.png`,
            ]));

            try {
                const stream = file.create(Gio.FileCreateFlags.NONE, null);
                stream.write_bytes(bytes, null);
                break;
            } catch (e) {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                    throw e;
            }
        }

        // Add it to recent files.
        saveRecentFile(file);
    }

    // Create a St.ImageContent icon for the notification. We want
    // St.ImageContent specifically because it preserves the aspect ratio when
    // shown in a notification.
    const pixels = pixbuf.read_pixel_bytes();
    const content =
        St.ImageContent.new_with_preferred_size(pixbuf.width, pixbuf.height);
    content.set_bytes(
        pixels,
        Cogl.PixelFormat.RGBA_8888,
        pixbuf.width,
        pixbuf.height,
        pixbuf.rowstride
    );

    // Show a notification.
    const source = new MessageTray.Source(
        // Translators: notification source name.
        _('Screenshot'),
        'screenshot-recorded-symbolic'
    );
    const notification = new MessageTray.Notification(
        source,
        // Translators: notification title.
        _('Screenshot captured'),
        // Translators: notification body when a screenshot was captured.
        _('You can paste the image from the clipboard.'),
        { datetime: time, gicon: content }
    );

    if (!disableSaveToDisk) {
        // Translators: button on the screenshot notification.
        notification.addAction(_('Show in Files'), () => {
            const app =
                Gio.app_info_get_default_for_type('inode/directory', false);

            if (app === null) {
                // It may be null e.g. in a toolbox without nautilus.
                log('Error showing in files: no default app set for inode/directory');
                return;
            }

            app.launch([file], global.create_app_launch_context(0, -1));
        });
        notification.connect('activated', () => {
            try {
                Gio.app_info_launch_default_for_uri(
                    file.get_uri(), global.create_app_launch_context(0, -1));
            } catch (err) {
                logError(err, 'Error opening screenshot');
            }
        });
    }

    notification.setTransient(true);
    Main.messageTray.add(source);
    source.showNotification(notification);
}

/**
 * Captures a screenshot from a texture, given a region, scale and optional
 * cursor data.
 *
 * @param {Cogl.Texture} texture - The texture to take the screenshot from.
 * @param {number[4]} [geometry] - The region to use: x, y, width and height.
 * @param {number} scale - The texture scale.
 * @param {Object} [cursor] - Cursor data to include in the screenshot.
 * @param {Cogl.Texture} cursor.texture - The cursor texture.
 * @param {number} cursor.x - The cursor x coordinate.
 * @param {number} cursor.y - The cursor y coordinate.
 * @param {number} cursor.scale - The cursor texture scale.
 */
async function captureScreenshot(texture, geometry, scale, cursor) {
    const stream = Gio.MemoryOutputStream.new_resizable();
    const [x, y, w, h] = geometry ?? [0, 0, -1, -1];
    if (cursor === null)
        cursor = { texture: null, x: 0, y: 0, scale: 1 };

    global.display.get_sound_player().play_from_theme(
        'screen-capture', _('Screenshot taken'), null);

    const pixbuf = await Shell.Screenshot.composite_to_stream(
        texture,
        x, y, w, h,
        scale,
        cursor.texture, cursor.x, cursor.y, cursor.scale,
        stream
    );

    stream.close(null);
    _storeScreenshot(stream.steal_as_bytes(), pixbuf);
}

/**
 * Shows the screenshot UI.
 */
function showScreenshotUI() {
    Main.screenshotUI.open().catch(err => {
        logError(err, 'Error opening the screenshot UI');
    });
}

/**
 * Shows the screen recording UI.
 */
function showScreenRecordingUI() {
    Main.screenshotUI.open(UIMode.SCREENCAST).catch(err => {
        logError(err, 'Error opening the screenshot UI');
    });
}

var ScreenshotService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(ScreenshotIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Screenshot');

        this._screenShooter = new Map();
        this._senderChecker = new DBusSenderChecker([
            'org.gnome.SettingsDaemon.MediaKeys',
            'org.freedesktop.impl.portal.desktop.gtk',
            'org.freedesktop.impl.portal.desktop.gnome',
            'org.gnome.Screenshot',
        ]);

        this._lockdownSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.lockdown' });

        Gio.DBus.session.own_name('org.gnome.Shell.Screenshot', Gio.BusNameOwnerFlags.REPLACE, null, null);
    }

    async _createScreenshot(invocation, needsDisk = true, restrictCallers = true) {
        let lockedDown = false;
        if (needsDisk)
            lockedDown = this._lockdownSettings.get_boolean('disable-save-to-disk');

        let sender = invocation.get_sender();
        if (this._screenShooter.has(sender)) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.BUSY,
                'There is an ongoing operation for this sender');
            return null;
        } else if (lockedDown) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.PERMISSION_DENIED,
                'Saving to disk is disabled');
            return null;
        } else if (restrictCallers) {
            try {
                await this._senderChecker.checkInvocation(invocation);
            } catch (e) {
                invocation.return_gerror(e);
                return null;
            }
        }

        let shooter = new Shell.Screenshot();
        shooter._watchNameId =
                        Gio.bus_watch_name(Gio.BusType.SESSION, sender, 0, null,
                                           this._onNameVanished.bind(this));

        this._screenShooter.set(sender, shooter);

        return shooter;
    }

    _onNameVanished(connection, name) {
        this._removeShooterForSender(name);
    }

    _removeShooterForSender(sender) {
        let shooter = this._screenShooter.get(sender);
        if (!shooter)
            return;

        Gio.bus_unwatch_name(shooter._watchNameId);
        this._screenShooter.delete(sender);
    }

    _checkArea(x, y, width, height) {
        return x >= 0 && y >= 0 &&
               width > 0 && height > 0 &&
               x + width <= global.screen_width &&
               y + height <= global.screen_height;
    }

    *_resolveRelativeFilename(filename) {
        filename = filename.replace(/\.png$/, '');

        let path = [
            GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_PICTURES),
            GLib.get_home_dir(),
        ].find(p => p && GLib.file_test(p, GLib.FileTest.EXISTS));

        if (!path)
            return null;

        yield Gio.File.new_for_path(
            GLib.build_filenamev([path, `${filename}.png`]));

        for (let idx = 1; ; idx++) {
            yield Gio.File.new_for_path(
                GLib.build_filenamev([path, `${filename}-${idx}.png`]));
        }
    }

    _createStream(filename, invocation) {
        if (filename == '')
            return [Gio.MemoryOutputStream.new_resizable(), null];

        if (GLib.path_is_absolute(filename)) {
            try {
                let file = Gio.File.new_for_path(filename);
                let stream = file.replace(null, false, Gio.FileCreateFlags.NONE, null);
                return [stream, file];
            } catch (e) {
                invocation.return_gerror(e);
                this._removeShooterForSender(invocation.get_sender());
                return [null, null];
            }
        }

        let err;
        for (let file of this._resolveRelativeFilename(filename)) {
            try {
                let stream = file.create(Gio.FileCreateFlags.NONE, null);
                return [stream, file];
            } catch (e) {
                err = e;
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS))
                    break;
            }
        }

        invocation.return_gerror(err);
        this._removeShooterForSender(invocation.get_sender());
        return [null, null];
    }

    _flashAsync(shooter) {
        return new Promise((resolve, _reject) => {
            shooter.connect('screenshot_taken', (s, area) => {
                const flashspot = new Flashspot(area);
                flashspot.fire(resolve);

                global.display.get_sound_player().play_from_theme(
                    'screen-capture', _('Screenshot taken'), null);
            });
        });
    }

    _onScreenshotComplete(stream, file, invocation) {
        stream.close(null);

        let filenameUsed = '';
        if (file) {
            filenameUsed = file.get_path();
        } else {
            let bytes = stream.steal_as_bytes();
            let clipboard = St.Clipboard.get_default();
            clipboard.set_content(St.ClipboardType.CLIPBOARD, 'image/png', bytes);
        }

        let retval = GLib.Variant.new('(bs)', [true, filenameUsed]);
        invocation.return_value(retval);
    }

    _scaleArea(x, y, width, height) {
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        x *= scaleFactor;
        y *= scaleFactor;
        width *= scaleFactor;
        height *= scaleFactor;
        return [x, y, width, height];
    }

    _unscaleArea(x, y, width, height) {
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        x /= scaleFactor;
        y /= scaleFactor;
        width /= scaleFactor;
        height /= scaleFactor;
        return [x, y, width, height];
    }

    async ScreenshotAreaAsync(params, invocation) {
        let [x, y, width, height, flash, filename] = params;
        [x, y, width, height] = this._scaleArea(x, y, width, height);
        if (!this._checkArea(x, y, width, height)) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }
        let screenshot = await this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot_area(x, y, width, height, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async ScreenshotWindowAsync(params, invocation) {
        let [includeFrame, includeCursor, flash, filename] = params;
        let screenshot = await this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot_window(includeFrame, includeCursor, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async ScreenshotAsync(params, invocation) {
        let [includeCursor, flash, filename] = params;
        let screenshot = await this._createScreenshot(invocation);
        if (!screenshot)
            return;

        let [stream, file] = this._createStream(filename, invocation);
        if (!stream)
            return;

        try {
            await Promise.all([
                flash ? this._flashAsync(screenshot) : null,
                screenshot.screenshot(includeCursor, stream),
            ]);
            this._onScreenshotComplete(stream, file, invocation);
        } catch (e) {
            invocation.return_value(new GLib.Variant('(bs)', [false, '']));
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }

    async SelectAreaAsync(params, invocation) {
        try {
            await this._senderChecker.checkInvocation(invocation);
        } catch (e) {
            invocation.return_gerror(e);
            return;
        }

        let selectArea = new SelectArea();
        try {
            let areaRectangle = await selectArea.selectAsync();
            let retRectangle = this._unscaleArea(
                areaRectangle.x, areaRectangle.y,
                areaRectangle.width, areaRectangle.height);
            invocation.return_value(GLib.Variant.new('(iiii)', retRectangle));
        } catch (e) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                'Operation was cancelled');
        }
    }

    async FlashAreaAsync(params, invocation) {
        try {
            await this._senderChecker.checkInvocation(invocation);
        } catch (e) {
            invocation.return_gerror(e);
            return;
        }

        let [x, y, width, height] = params;
        [x, y, width, height] = this._scaleArea(x, y, width, height);
        if (!this._checkArea(x, y, width, height)) {
            invocation.return_error_literal(Gio.IOErrorEnum,
                                            Gio.IOErrorEnum.CANCELLED,
                                            "Invalid params");
            return;
        }
        let flashspot = new Flashspot({ x, y, width, height });
        flashspot.fire();
        invocation.return_value(null);
    }

    async PickColorAsync(params, invocation) {
        const screenshot = await this._createScreenshot(invocation, false, false);
        if (!screenshot)
            return;

        const pickPixel = new PickPixel(screenshot);
        try {
            const color = await pickPixel.pickAsync();
            const { red, green, blue } = color;
            const retval = GLib.Variant.new('(a{sv})', [{
                color: GLib.Variant.new('(ddd)', [
                    red / 255.0,
                    green / 255.0,
                    blue / 255.0,
                ]),
            }]);
            invocation.return_value(retval);
        } catch (e) {
            invocation.return_error_literal(
                Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED,
                'Operation was cancelled');
        } finally {
            this._removeShooterForSender(invocation.get_sender());
        }
    }
};

var SelectArea = GObject.registerClass(
class SelectArea extends St.Widget {
    _init() {
        this._startX = -1;
        this._startY = -1;
        this._lastX = 0;
        this._lastY = 0;
        this._result = null;

        super._init({
            visible: false,
            reactive: true,
            x: 0,
            y: 0,
        });
        Main.uiGroup.add_actor(this);

        this._grabHelper = new GrabHelper.GrabHelper(this);

        const constraint = new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        });
        this.add_constraint(constraint);

        this._rubberband = new St.Widget({
            style_class: 'select-area-rubberband',
            visible: false,
        });
        this.add_actor(this._rubberband);
    }

    async selectAsync() {
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        Main.uiGroup.set_child_above_sibling(this, null);
        this.show();

        try {
            await this._grabHelper.grabAsync({ actor: this });
        } finally {
            global.display.set_cursor(Meta.Cursor.DEFAULT);

            GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this.destroy();
                return GLib.SOURCE_REMOVE;
            });
        }

        return this._result;
    }

    _getGeometry() {
        return new Meta.Rectangle({
            x: Math.min(this._startX, this._lastX),
            y: Math.min(this._startY, this._lastY),
            width: Math.abs(this._startX - this._lastX) + 1,
            height: Math.abs(this._startY - this._lastY) + 1,
        });
    }

    vfunc_motion_event(motionEvent) {
        if (this._startX == -1 || this._startY == -1 || this._result)
            return Clutter.EVENT_PROPAGATE;

        [this._lastX, this._lastY] = [motionEvent.x, motionEvent.y];
        this._lastX = Math.floor(this._lastX);
        this._lastY = Math.floor(this._lastY);
        let geometry = this._getGeometry();

        this._rubberband.set_position(geometry.x, geometry.y);
        this._rubberband.set_size(geometry.width, geometry.height);
        this._rubberband.show();

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_press_event(buttonEvent) {
        if (this._result)
            return Clutter.EVENT_PROPAGATE;

        [this._startX, this._startY] = [buttonEvent.x, buttonEvent.y];
        this._startX = Math.floor(this._startX);
        this._startY = Math.floor(this._startY);
        this._rubberband.set_position(this._startX, this._startY);

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event() {
        if (this._startX === -1 || this._startY === -1 || this._result)
            return Clutter.EVENT_PROPAGATE;

        this._result = this._getGeometry();
        this.ease({
            opacity: 0,
            duration: 200,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._grabHelper.ungrab(),
        });
        return Clutter.EVENT_PROPAGATE;
    }
});

var RecolorEffect = GObject.registerClass({
    Properties: {
        color: GObject.ParamSpec.boxed(
            'color', 'color', 'replacement color',
            GObject.ParamFlags.WRITABLE,
            Clutter.Color.$gtype),
        chroma: GObject.ParamSpec.boxed(
            'chroma', 'chroma', 'color to replace',
            GObject.ParamFlags.WRITABLE,
            Clutter.Color.$gtype),
        threshold: GObject.ParamSpec.float(
            'threshold', 'threshold', 'threshold',
            GObject.ParamFlags.WRITABLE,
            0.0, 1.0, 0.0),
        smoothing: GObject.ParamSpec.float(
            'smoothing', 'smoothing', 'smoothing',
            GObject.ParamFlags.WRITABLE,
            0.0, 1.0, 0.0),
    },
}, class RecolorEffect extends Shell.GLSLEffect {
    _init(params) {
        this._color = new Clutter.Color();
        this._chroma = new Clutter.Color();
        this._threshold = 0;
        this._smoothing = 0;

        this._colorLocation = null;
        this._chromaLocation = null;
        this._thresholdLocation = null;
        this._smoothingLocation = null;

        super._init(params);

        this._colorLocation = this.get_uniform_location('recolor_color');
        this._chromaLocation = this.get_uniform_location('chroma_color');
        this._thresholdLocation = this.get_uniform_location('threshold');
        this._smoothingLocation = this.get_uniform_location('smoothing');

        this._updateColorUniform(this._colorLocation, this._color);
        this._updateColorUniform(this._chromaLocation, this._chroma);
        this._updateFloatUniform(this._thresholdLocation, this._threshold);
        this._updateFloatUniform(this._smoothingLocation, this._smoothing);
    }

    _updateColorUniform(location, color) {
        if (!location)
            return;

        this.set_uniform_float(location,
            3, [color.red / 255, color.green / 255, color.blue / 255]);
        this.queue_repaint();
    }

    _updateFloatUniform(location, value) {
        if (!location)
            return;

        this.set_uniform_float(location, 1, [value]);
        this.queue_repaint();
    }

    set color(c) {
        if (this._color.equal(c))
            return;

        this._color = c;
        this.notify('color');

        this._updateColorUniform(this._colorLocation, this._color);
    }

    set chroma(c) {
        if (this._chroma.equal(c))
            return;

        this._chroma = c;
        this.notify('chroma');

        this._updateColorUniform(this._chromaLocation, this._chroma);
    }

    set threshold(value) {
        if (this._threshold === value)
            return;

        this._threshold = value;
        this.notify('threshold');

        this._updateFloatUniform(this._thresholdLocation, this._threshold);
    }

    set smoothing(value) {
        if (this._smoothing === value)
            return;

        this._smoothing = value;
        this.notify('smoothing');

        this._updateFloatUniform(this._smoothingLocation, this._smoothing);
    }

    vfunc_build_pipeline() {
        // Conversion parameters from https://en.wikipedia.org/wiki/YCbCr
        const decl = `
            vec3 rgb2yCrCb(vec3 c) {                                \n
                float y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;  \n
                float cr = 0.7133 * (c.r - y);                      \n
                float cb = 0.5643 * (c.b - y);                      \n
                return vec3(y, cr, cb);                             \n
            }                                                       \n
                                                                    \n
            uniform vec3 chroma_color;                              \n
            uniform vec3 recolor_color;                             \n
            uniform float threshold;                                \n
            uniform float smoothing;                                \n`;
        const src = `
            vec3 mask = rgb2yCrCb(chroma_color.rgb);                \n
            vec3 yCrCb = rgb2yCrCb(cogl_color_out.rgb);             \n
            float blend =                                           \n
              smoothstep(threshold,                                 \n
                         threshold + smoothing,                     \n
                         distance(yCrCb.gb, mask.gb));              \n
            cogl_color_out.rgb =                                    \n
              mix(recolor_color, cogl_color_out.rgb, blend);        \n`;

        this.add_glsl_snippet(Shell.SnippetHook.FRAGMENT, decl, src, false);
    }
});

var PickPixel = GObject.registerClass(
class PickPixel extends St.Widget {
    _init(screenshot) {
        super._init({ visible: false, reactive: true });

        this._screenshot = screenshot;

        this._result = null;
        this._color = null;
        this._inPick = false;

        Main.uiGroup.add_actor(this);

        this._grabHelper = new GrabHelper.GrabHelper(this);

        const constraint = new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        });
        this.add_constraint(constraint);

        const action = new Clutter.ClickAction();
        action.connect('clicked', async () => {
            await this._pickColor(...action.get_coords());
            this._result = this._color;
            this._grabHelper.ungrab();
        });
        this.add_action(action);

        this._recolorEffect = new RecolorEffect({
            chroma: new Clutter.Color({
                red: 80,
                green: 219,
                blue: 181,
            }),
            threshold: 0.04,
            smoothing: 0.07,
        });
        this._previewCursor = new St.Icon({
            icon_name: 'color-pick',
            icon_size: Meta.prefs_get_cursor_size(),
            effect: this._recolorEffect,
            visible: false,
        });
        Main.uiGroup.add_actor(this._previewCursor);
    }

    async pickAsync() {
        global.display.set_cursor(Meta.Cursor.BLANK);
        Main.uiGroup.set_child_above_sibling(this, null);
        this.show();

        this._pickColor(...global.get_pointer());

        try {
            await this._grabHelper.grabAsync({ actor: this });
        } finally {
            global.display.set_cursor(Meta.Cursor.DEFAULT);
            this._previewCursor.destroy();

            GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this.destroy();
                return GLib.SOURCE_REMOVE;
            });
        }

        return this._result;
    }

    async _pickColor(x, y) {
        if (this._inPick)
            return;

        this._inPick = true;
        this._previewCursor.set_position(x, y);
        [this._color] = await this._screenshot.pick_color(x, y);
        this._inPick = false;

        if (!this._color)
            return;

        this._recolorEffect.color = this._color;
        this._previewCursor.show();
    }

    vfunc_motion_event(motionEvent) {
        const { x, y } = motionEvent;
        this._pickColor(x, y);
        return Clutter.EVENT_PROPAGATE;
    }
});

var FLASHSPOT_ANIMATION_OUT_TIME = 500; // milliseconds

var Flashspot = GObject.registerClass(
class Flashspot extends Lightbox.Lightbox {
    _init(area) {
        super._init(Main.uiGroup, {
            inhibitEvents: true,
            width: area.width,
            height: area.height,
        });
        this.style_class = 'flashspot';
        this.set_position(area.x, area.y);
    }

    fire(doneCallback) {
        this.set({ visible: true, opacity: 255 });
        this.ease({
            opacity: 0,
            duration: FLASHSPOT_ANIMATION_OUT_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                if (doneCallback)
                    doneCallback();
                this.destroy();
            },
        });
    }
});
