import Clutter from 'gi://Clutter';
import * as Signals from '../misc/signals.js';

import * as DND from './dnd.js';
import * as Main from './main.js';

export class XdndHandler extends Signals.EventEmitter {
    constructor() {
        super();

        // Used to display a clone of the cursor window when the
        // window group is hidden (like it happens in the overview)
        this._cursorWindowClone = null;

        // Used as a drag actor in case we don't have a cursor window clone
        this._dummy = new Clutter.Actor({width: 1, height: 1, opacity: 0});
        Main.uiGroup.add_child(this._dummy);
        this._dummy.hide();

        const dnd = global.backend.get_dnd();
        dnd.connect('dnd-enter', this._onEnter.bind(this));
        dnd.connect('dnd-position-change', this._onPositionChanged.bind(this));
        dnd.connect('dnd-leave', this._onLeave.bind(this));
    }

    // Called when the user cancels the drag (i.e release the button)
    _onLeave() {
        global.window_group.disconnectObject(this);
        Main.sessionMode.disconnectObject(this);

        if (this._cursorWindowClone) {
            this._cursorWindowClone.destroy();
            this._cursorWindowClone = null;
        }

        global.compositor.get_feedback_group().show();

        this.emit('drag-end');
    }

    _onEnter() {
        global.window_group.connectObject('notify::visible',
            () => this._syncCursorWindowClone(), this);
        Main.sessionMode.connectObject('updated', () => {
            this._syncCursorWindowClone();

            const feedbackGroup = global.compositor.get_feedback_group();
            feedbackGroup.visible = !Main.sessionMode.isLocked;
        }, this);

        this.emit('drag-begin', global.get_current_time());
    }

    _syncCursorWindowClone() {
        const needsCursorClone =
            !global.window_group.visible &&
            !Main.sessionMode.isLocked;

        if (needsCursorClone) {
            if (this._cursorWindowClone)
                return;

            const windows = global.get_window_actors();
            const cursorWindow = windows[windows.length - 1];

            // FIXME: more reliable way?
            if (!cursorWindow.get_meta_window().is_override_redirect())
                return;

            const constraintPosition = new Clutter.BindConstraint({
                coordinate: Clutter.BindCoordinate.POSITION,
                source: cursorWindow,
            });

            this._cursorWindowClone = new Clutter.Clone({source: cursorWindow});
            Main.uiGroup.add_child(this._cursorWindowClone);

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

        const dragEvent = {
            x,
            y,
            dragActor: this._cursorWindowClone ?? this._dummy,
            source: this,
            targetActor: pickedActor,
        };

        for (let i = 0; i < DND.dragMonitors.length; i++) {
            const motionFunc = DND.dragMonitors[i].dragMotion;
            if (motionFunc) {
                const result = motionFunc(dragEvent);
                if (result !== DND.DragMotionResult.CONTINUE)
                    return;
            }
        }

        while (pickedActor) {
            if (pickedActor._delegate && pickedActor._delegate.handleDragOver) {
                const [r_, targX, targY] = pickedActor.transform_stage_point(x, y);
                const result = pickedActor._delegate.handleDragOver(this,
                    dragEvent.dragActor,
                    targX,
                    targY,
                    global.get_current_time());
                if (result !== DND.DragMotionResult.CONTINUE)
                    return;
            }
            pickedActor = pickedActor.get_parent();
        }
    }
}
