// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported XdndHandler */

const { Clutter } = imports.gi;
const Signals = imports.misc.signals;

const DND = imports.ui.dnd;
const Main = imports.ui.main;

var XdndHandler = class extends Signals.EventEmitter {
    constructor() {
        super();

        // Used to display a clone of the cursor window when the
        // window group is hidden (like it happens in the overview)
        this._cursorWindowClone = null;

        // Used as a drag actor in case we don't have a cursor window clone
        this._dummy = new Clutter.Actor({ width: 1, height: 1, opacity: 0 });
        Main.uiGroup.add_actor(this._dummy);
        this._dummy.hide();

        var dnd = global.backend.get_dnd();
        dnd.connect('dnd-enter', this._onEnter.bind(this));
        dnd.connect('dnd-position-change', this._onPositionChanged.bind(this));
        dnd.connect('dnd-leave', this._onLeave.bind(this));
    }

    // Called when the user cancels the drag (i.e release the button)
    _onLeave() {
        global.window_group.disconnectObject(this);
        if (this._cursorWindowClone) {
            this._cursorWindowClone.destroy();
            this._cursorWindowClone = null;
        }

        this.emit('drag-end');
    }

    _onEnter() {
        global.window_group.connectObject('notify::visible',
            this._onWindowGroupVisibilityChanged.bind(this), this);

        this.emit('drag-begin', global.get_current_time());
    }

    _onWindowGroupVisibilityChanged() {
        if (!global.window_group.visible) {
            if (this._cursorWindowClone)
                return;

            let windows = global.get_window_actors();
            let cursorWindow = windows[windows.length - 1];

            // FIXME: more reliable way?
            if (!cursorWindow.get_meta_window().is_override_redirect())
                return;

            const constraintPosition = new Clutter.BindConstraint({
                coordinate: Clutter.BindCoordinate.POSITION,
                source: cursorWindow,
            });

            this._cursorWindowClone = new Clutter.Clone({ source: cursorWindow });
            Main.uiGroup.add_actor(this._cursorWindowClone);

            // Make sure that the clone has the same position as the source
            this._cursorWindowClone.add_constraint(constraintPosition);
        } else {
            if (!this._cursorWindowClone)
                return;

            this._cursorWindowClone.destroy();
            this._cursorWindowClone = null;
        }
    }

    _onPositionChanged(obj, x, y) {
        let pickedActor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);

        // Make sure that the cursor window is on top
        if (this._cursorWindowClone)
            Main.uiGroup.set_child_above_sibling(this._cursorWindowClone, null);

        let dragEvent = {
            x,
            y,
            dragActor: this._cursorWindowClone ?? this._dummy,
            source: this,
            targetActor: pickedActor,
        };

        for (let i = 0; i < DND.dragMonitors.length; i++) {
            let motionFunc = DND.dragMonitors[i].dragMotion;
            if (motionFunc) {
                let result = motionFunc(dragEvent);
                if (result != DND.DragMotionResult.CONTINUE)
                    return;
            }
        }

        while (pickedActor) {
            if (pickedActor._delegate && pickedActor._delegate.handleDragOver) {
                let [r_, targX, targY] = pickedActor.transform_stage_point(x, y);
                let result = pickedActor._delegate.handleDragOver(this,
                                                                  dragEvent.dragActor,
                                                                  targX,
                                                                  targY,
                                                                  global.get_current_time());
                if (result != DND.DragMotionResult.CONTINUE)
                    return;
            }
            pickedActor = pickedActor.get_parent();
        }
    }
};
