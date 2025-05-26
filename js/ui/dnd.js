import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Signals from '../misc/signals.js';

import * as Main from './main.js';
import * as Params from '../misc/params.js';

// Time to scale down to maxDragActorSize
const SCALE_ANIMATION_TIME = 250;
// Time to animate to original position on cancel
const SNAP_BACK_ANIMATION_TIME = 250;
// Time to animate to original position on success
const REVERT_ANIMATION_TIME = 750;

/** @enum {number} */
export const DragMotionResult = {
    NO_DROP:   0,
    COPY_DROP: 1,
    MOVE_DROP: 2,
    CONTINUE:  3,
};

const DRAG_CURSOR_MAP = {
    0: Meta.Cursor.NO_DROP,
    1: Meta.Cursor.COPY,
    2: Meta.Cursor.MOVE,
};

export const DragDropResult = {
    FAILURE:  0,
    SUCCESS:  1,
    CONTINUE: 2,
};
export const dragMonitors = [];

let eventHandlerActor = null;
let currentDraggable = null;

function _getEventHandlerActor() {
    if (!eventHandlerActor) {
        eventHandlerActor = new Clutter.Actor({width: 0, height: 0, reactive: true});
        Main.uiGroup.add_child(eventHandlerActor);
        // We connect to 'event' rather than 'captured-event' because the capturing phase doesn't happen
        // when you've grabbed the pointer.
        eventHandlerActor.connect('event', (actor, event) => {
            return currentDraggable._onEvent(actor, event);
        });
    }
    return eventHandlerActor;
}

function _getRealActorScale(actor) {
    let scale = 1.0;
    while (actor) {
        scale *= actor.scale_x;
        actor = actor.get_parent();
    }
    return scale;
}

/**
 * @typedef {object} DragMonitor
 * @property {Function} dragMotion
 */

/**
 * @param {DragMonitor} monitor
 */
export function addDragMonitor(monitor) {
    dragMonitors.push(monitor);
}

/**
 * @param {DragMonitor} monitor
 */
export function removeDragMonitor(monitor) {
    for (let i = 0; i < dragMonitors.length; i++) {
        if (dragMonitors[i] === monitor) {
            dragMonitors.splice(i, 1);
            return;
        }
    }
}

class _Draggable extends Signals.EventEmitter {
    constructor(actor, params) {
        super();
        params = Params.parse(params, {
            manualMode: false,
            timeoutThreshold: 0,
            restoreOnSuccess: false,
            dragActorMaxSize: undefined,
            dragActorOpacity: undefined,
        });

        this.actor = actor;

        this._dndGesture = new St.DndStartGesture({
            manual_mode: params.manualMode,
            timeout_threshold: params.timeoutThreshold,
        });
        this._dndGesture.connect('recognize', () => this._gestureRecognized());

        this.actor.add_action(this._dndGesture);

        this._restoreOnSuccess = params.restoreOnSuccess;
        this._dragActorMaxSize = params.dragActorMaxSize;
        this._dragActorOpacity = params.dragActorOpacity;
        this._dragTimeoutThreshold = params.timeoutThreshold;

        this._animationInProgress = false; // The drag is over and the item is in the process of animating to its original position (snapping back or reverting).
        this._dragCancellable = true;
    }

    _grabEvents(sprite) {
        const grab = Main.pushModal(_getEventHandlerActor());
        if ((grab.get_seat_state() & Clutter.GrabState.POINTER) === 0) {
            Main.popModal(grab);
            return;
        }

        this._grab = grab;
        this._sprite = sprite;
    }

    _ungrabEvents() {
        Main.popModal(this._grab);
        this._grab = null;
        this._sprite = null;
    }

    _gestureRecognized() {
        const pointBeginEvent = this._dndGesture.get_point_begin_event();
        [this._dragStartX, this._dragStartY] = pointBeginEvent.get_coords();

        const triggeringEvent = this._dndGesture.get_drag_triggering_event();
        const [stageX, stageY] = triggeringEvent.get_coords();
        const backend = this.actor.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, triggeringEvent);
        const time = triggeringEvent.get_time();

        currentDraggable = this;

        this._grabEvents(sprite);
        if (!this._grab)
            return;

        this.emit('drag-begin', time);
        global.display.set_cursor(Meta.Cursor.NO_DROP);

        this._dragX = stageX;
        this._dragY = stageY;

        let scaledWidth, scaledHeight;

        if (this.actor._delegate && this.actor._delegate.getDragActor) {
            this._dragActor = this.actor._delegate.getDragActor();
            Main.uiGroup.add_child(this._dragActor);
            Main.uiGroup.set_child_above_sibling(this._dragActor, null);
            Shell.util_set_hidden_from_pick(this._dragActor, true);

            // Drag actor does not always have to be the same as actor. For example drag actor
            // can be an image that's part of the actor. So to perform "snap back" correctly we need
            // to know what was the drag actor source.
            if (this.actor._delegate.getDragActorSource) {
                this._dragActorSource = this.actor._delegate.getDragActorSource();
                // If the user dragged from the source, then position
                // the dragActor over it. Otherwise, center it
                // around the pointer
                let [sourceX, sourceY] = this._dragActorSource.get_transformed_position();
                let x, y;
                if (this._dragStartX > sourceX && this._dragStartX <= sourceX + this._dragActor.width &&
                    this._dragStartY > sourceY && this._dragStartY <= sourceY + this._dragActor.height) {
                    x = sourceX;
                    y = sourceY;
                } else {
                    x = this._dragStartX - this._dragActor.width / 2;
                    y = this._dragStartY - this._dragActor.height / 2;
                }
                this._dragActor.set_position(x, y);

                this._dragActorSourceDestroyId = this._dragActorSource.connect('destroy', () => {
                    this._dragActorSource = null;
                });
            } else {
                this._dragActorSource = this.actor;
            }
            this._dragOrigParent = undefined;

            this._dragOffsetX = this._dragActor.x - this._dragStartX;
            this._dragOffsetY = this._dragActor.y - this._dragStartY;

            [scaledWidth, scaledHeight] = this._dragActor.get_transformed_size();
        } else {
            this._dragActor = this.actor;

            this._dragActorSource = undefined;
            this._dragOrigParent = this.actor.get_parent();

            this._dragActorHadFixedPos = this._dragActor.fixed_position_set;
            this._dragOrigX = this._dragActor.allocation.x1;
            this._dragOrigY = this._dragActor.allocation.y1;
            this._dragActorHadNatWidth = this._dragActor.natural_width_set;
            this._dragActorHadNatHeight = this._dragActor.natural_height_set;
            this._dragOrigWidth = this._dragActor.allocation.get_width();
            this._dragOrigHeight = this._dragActor.allocation.get_height();
            this._dragOrigScale = this._dragActor.scale_x;

            // Ensure actors with an allocation smaller than their natural size
            // retain their size
            this._dragActor.set_size(...this._dragActor.allocation.get_size());

            const transformedExtents = this._dragActor.get_transformed_extents();

            this._dragOffsetX = transformedExtents.origin.x - this._dragStartX;
            this._dragOffsetY = transformedExtents.origin.y - this._dragStartY;

            scaledWidth = transformedExtents.get_width();
            scaledHeight = transformedExtents.get_height();

            this._dragActor.scale_x = scaledWidth / this._dragOrigWidth;
            this._dragActor.scale_y = scaledHeight / this._dragOrigHeight;

            this._dragOrigParent.remove_child(this._dragActor);
            Main.uiGroup.add_child(this._dragActor);
            Main.uiGroup.set_child_above_sibling(this._dragActor, null);
            Shell.util_set_hidden_from_pick(this._dragActor, true);

            this._dragOrigParentDestroyId = this._dragOrigParent.connect('destroy', () => {
                this._dragOrigParent = null;
            });
        }

        this._dragActorDestroyId = this._dragActor.connect('destroy', () => {
            // Cancel ongoing animation (if any)
            this._finishAnimation();

            this._dragActor = null;

            if (this._dragCancellable)
                this._cancelDrag(global.get_current_time());
        });
        this._dragOrigOpacity = this._dragActor.opacity;
        if (this._dragActorOpacity !== undefined)
            this._dragActor.opacity = this._dragActorOpacity;

        this._snapBackX = this._dragStartX + this._dragOffsetX;
        this._snapBackY = this._dragStartY + this._dragOffsetY;
        this._snapBackScale = this._dragActor.scale_x;

        let origDragOffsetX = this._dragOffsetX;
        let origDragOffsetY = this._dragOffsetY;
        let [transX, transY] = this._dragActor.get_translation();
        this._dragOffsetX -= transX;
        this._dragOffsetY -= transY;

        this._dragActor.set_position(
            this._dragX + this._dragOffsetX,
            this._dragY + this._dragOffsetY);

        if (this._dragActorMaxSize !== undefined) {
            let currentSize = Math.max(scaledWidth, scaledHeight);
            if (currentSize > this._dragActorMaxSize) {
                let scale = this._dragActorMaxSize / currentSize;
                let origScale =  this._dragActor.scale_x;

                // The position of the actor changes as we scale
                // around the drag position, but we can't just tween
                // to the final position because that tween would
                // fight with updates as the user continues dragging
                // the mouse; instead we do the position computations in
                // a ::new-frame handler.
                this._dragActor.ease({
                    scale_x: scale * origScale,
                    scale_y: scale * origScale,
                    duration: SCALE_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => {
                        this._updateActorPosition(origScale,
                            origDragOffsetX, origDragOffsetY, transX, transY);
                    },
                });

                this._dragActor.get_transition('scale-x').connect('new-frame', () => {
                    this._updateActorPosition(origScale,
                        origDragOffsetX, origDragOffsetY, transX, transY);
                });
            }
        }
    }

    _updateActorPosition(origScale, origDragOffsetX, origDragOffsetY, transX, transY) {
        const currentScale = this._dragActor.scale_x / origScale;
        this._dragOffsetX = currentScale * origDragOffsetX - transX;
        this._dragOffsetY = currentScale * origDragOffsetY - transY;
        this._dragActor.set_position(
            this._dragX + this._dragOffsetX,
            this._dragY + this._dragOffsetY);
    }

    _pickTargetActor() {
        return this._dragActor.get_stage().get_actor_at_pos(
            Clutter.PickMode.ALL, this._dragX, this._dragY);
    }

    _updateDragHover() {
        this._updateHoverId = 0;
        let target = this._pickTargetActor();

        let dragEvent = {
            x: this._dragX,
            y: this._dragY,
            dragActor: this._dragActor,
            source: this.actor._delegate,
            targetActor: target,
        };

        let targetActorDestroyHandlerId;
        let handleTargetActorDestroyClosure;
        handleTargetActorDestroyClosure = () => {
            target = this._pickTargetActor();
            dragEvent.targetActor = target;
            targetActorDestroyHandlerId =
                target.connect('destroy', handleTargetActorDestroyClosure);
        };
        targetActorDestroyHandlerId =
            target.connect('destroy', handleTargetActorDestroyClosure);

        for (let i = 0; i < dragMonitors.length; i++) {
            let motionFunc = dragMonitors[i].dragMotion;
            if (motionFunc) {
                let result = motionFunc(dragEvent);
                if (result !== DragMotionResult.CONTINUE) {
                    global.display.set_cursor(DRAG_CURSOR_MAP[result]);
                    dragEvent.targetActor.disconnect(targetActorDestroyHandlerId);
                    return GLib.SOURCE_REMOVE;
                }
            }
        }
        dragEvent.targetActor.disconnect(targetActorDestroyHandlerId);

        while (target) {
            if (target._delegate && target._delegate.handleDragOver) {
                let [r_, targX, targY] = target.transform_stage_point(this._dragX, this._dragY);
                // We currently loop through all parents on drag-over even if one of the children has handled it.
                // We can check the return value of the function and break the loop if it's true if we don't want
                // to continue checking the parents.
                let result = target._delegate.handleDragOver(
                    this.actor._delegate,
                    this._dragActor,
                    targX,
                    targY,
                    0);
                if (result !== DragMotionResult.CONTINUE) {
                    global.display.set_cursor(DRAG_CURSOR_MAP[result]);
                    return GLib.SOURCE_REMOVE;
                }
            }
            target = target.get_parent();
        }
        global.display.set_cursor(Meta.Cursor.NO_DROP);
        return GLib.SOURCE_REMOVE;
    }

    _queueUpdateDragHover() {
        if (this._updateHoverId)
            return;

        this._updateHoverId = GLib.idle_add(GLib.PRIORITY_DEFAULT,
            this._updateDragHover.bind(this));
        GLib.Source.set_name_by_id(this._updateHoverId, '[gnome-shell] this._updateDragHover');
    }

    _updateDragPosition(event) {
        let [stageX, stageY] = event.get_coords();
        this._dragX = stageX;
        this._dragY = stageY;
        this._dragActor.set_position(
            stageX + this._dragOffsetX,
            stageY + this._dragOffsetY);

        this._queueUpdateDragHover();
        return true;
    }

    _dragActorDropped(event) {
        let [dropX, dropY] = event.get_coords();
        let target = this._dragActor.get_stage().get_actor_at_pos(
            Clutter.PickMode.ALL, dropX, dropY);

        // We call observers only once per motion with the innermost
        // target actor. If necessary, the observer can walk the
        // parent itself.
        let dropEvent = {
            dropActor: this._dragActor,
            targetActor: target,
            clutterEvent: event,
        };
        for (let i = 0; i < dragMonitors.length; i++) {
            let dropFunc = dragMonitors[i].dragDrop;
            if (dropFunc) {
                switch (dropFunc(dropEvent)) {
                case DragDropResult.FAILURE:
                case DragDropResult.SUCCESS:
                    return;

                case DragDropResult.CONTINUE:
                    continue;
                }
            }
        }

        // At this point it is too late to cancel a drag by destroying
        // the actor, the fate of which is decided by acceptDrop and its
        // side-effects
        this._dragCancellable = false;

        while (target) {
            if (target._delegate && target._delegate.acceptDrop) {
                let [r_, targX, targY] = target.transform_stage_point(dropX, dropY);
                let accepted = false;
                try {
                    accepted = target._delegate.acceptDrop(this.actor._delegate,
                        this._dragActor, targX, targY, event.get_time());
                } catch (e) {
                    // On error, skip this target
                    logError(e, 'Skipping drag target');
                }
                if (accepted) {
                    // If it accepted the drop without taking the actor,
                    // handle it ourselves.
                    if (this._dragActor && this._dragActor.get_parent() === Main.uiGroup) {
                        if (this._restoreOnSuccess) {
                            this._restoreDragActor(event.get_time());
                            return;
                        } else {
                            this._dragActor.destroy();
                        }
                    }

                    global.display.set_cursor(Meta.Cursor.DEFAULT);
                    this.emit('drag-end', event.get_time(), true);
                    this._dragComplete();
                    return;
                }
            }
            target = target.get_parent();
        }

        // If no target has been found, cancel the drag
        this._cancelDrag(event.get_time());
    }

    _getRestoreLocation() {
        let x, y, scale;

        if (this._dragActorSource && this._dragActorSource.visible) {
            // Snap the clone back to its source
            [x, y] = this._dragActorSource.get_transformed_position();
            let [sourceScaledWidth] = this._dragActorSource.get_transformed_size();
            scale = sourceScaledWidth ? sourceScaledWidth / this._dragActor.width : 0;
        } else if (this._dragOrigParent) {
            // Snap the actor back to its original position within
            // its parent, adjusting for the fact that the parent
            // may have been moved or scaled
            let [parentX, parentY] = this._dragOrigParent.get_transformed_position();
            let parentScale = _getRealActorScale(this._dragOrigParent);

            x = parentX + parentScale * this._dragOrigX;
            y = parentY + parentScale * this._dragOrigY;
            scale = this._dragOrigScale * parentScale;
        } else {
            // Snap back actor to its original stage position
            x = this._snapBackX;
            y = this._snapBackY;
            scale = this._snapBackScale;
        }

        return [x, y, scale];
    }

    _restoreDragActor(eventTime) {
        let [restoreX, restoreY, restoreScale] = this._getRestoreLocation();

        // fade the actor back in at its original location
        this._dragActor.set_position(restoreX, restoreY);
        this._dragActor.set_scale(restoreScale, restoreScale);
        this._dragActor.opacity = 0;

        this._animateDragEnd(eventTime, {
            duration: REVERT_ANIMATION_TIME,
        });
    }

    _animateDragEnd(eventTime, params) {
        this._animationInProgress = true;

        // start the animation
        this._dragActor.ease({
            ...params,
            opacity: this._dragOrigOpacity,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => {
                this._onAnimationComplete(eventTime);
            },
        });
    }

    _finishAnimation() {
        if (!this._animationInProgress)
            return;

        this._animationInProgress = false;
        this._dragComplete();

        global.display.set_cursor(Meta.Cursor.DEFAULT);
    }

    _onAnimationComplete(eventTime) {
        if (this._dragOrigParent) {
            Main.uiGroup.remove_child(this._dragActor);
            this._dragOrigParent.add_child(this._dragActor);
            this._dragActor.set_scale(this._dragOrigScale, this._dragOrigScale);
            if (this._dragActorHadFixedPos)
                this._dragActor.set_position(this._dragOrigX, this._dragOrigY);
            else
                this._dragActor.fixed_position_set = false;
            if (this._dragActorHadNatWidth)
                this._dragActor.set_width(-1);
            if (this._dragActorHadNatHeight)
                this._dragActor.set_height(-1);
        } else {
            this._dragActor?.destroy();
        }

        this.emit('drag-end', eventTime, false);
        this._finishAnimation();
    }

    _dragComplete() {
        if (this._dragActor)
            Shell.util_set_hidden_from_pick(this._dragActor, false);

        if (this._grab)
            this._ungrabEvents();

        if (this._updateHoverId) {
            GLib.source_remove(this._updateHoverId);
            this._updateHoverId = 0;
        }

        if (this._dragActor) {
            this._dragActor.disconnect(this._dragActorDestroyId);
            this._dragActor = null;
        }

        if (this._dragOrigParent) {
            this._dragOrigParent.disconnect(this._dragOrigParentDestroyId);
            this._dragOrigParent = null;
        }

        if (this._dragActorSource) {
            this._dragActorSource.disconnect(this._dragActorSourceDestroyId);
            this._dragActorSource = null;
        }
    }

    _onEvent(actor, event) {
        if (event.type() === Clutter.EventType.KEY_PRESS) {
            if (event.get_key_symbol() === Clutter.KEY_Escape)
                this._cancelDrag(event.get_time());

            return Clutter.EVENT_PROPAGATE;
        }

        const backend = actor.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);
        if (sprite !== this._sprite)
            return Clutter.EVENT_PROPAGATE;

        if (event.type() === Clutter.EventType.BUTTON_RELEASE ||
            event.type() === Clutter.EventType.TOUCH_END)
            this._dragActorDropped(event);

        if (event.type() === Clutter.EventType.MOTION ||
            event.type() === Clutter.EventType.TOUCH_UPDATE)
            this._updateDragPosition(event);

        return Clutter.EVENT_PROPAGATE;
    }

    _cancelDrag(eventTime) {
        this.emit('drag-cancelled', eventTime);

        if (!this._dragActor) {
            global.display.set_cursor(Meta.Cursor.DEFAULT);
            this._dragComplete();
            this.emit('drag-end', eventTime, false);
            if (!this._dragOrigParent && this._dragActor)
                this._dragActor.destroy();

            return;
        }

        let [snapBackX, snapBackY, snapBackScale] = this._getRestoreLocation();

        this._animateDragEnd(eventTime, {
            x: snapBackX,
            y: snapBackY,
            scale_x: snapBackScale,
            scale_y: snapBackScale,
            duration: SNAP_BACK_ANIMATION_TIME,
        });
    }

    get startGesture() {
        return this._dndGesture;
    }
}

/**
 * Create an object which controls drag and drop for the given actor.
 *
 * If %manualMode is %true in @params, do not automatically start
 * drag and drop on click
 *
 * If %dragActorMaxSize is present in @params, the drag actor will
 * be scaled down to be no larger than that size in pixels.
 *
 * If %dragActorOpacity is present in @params, the drag actor will
 * will be set to have that opacity during the drag.
 *
 * Note that when the drag actor is the source actor and the drop
 * succeeds, the actor scale and opacity aren't reset; if the drop
 * target wants to reuse the actor, it's up to the drop target to
 * reset these values.
 *
 * @param {Clutter.Actor} actor Source actor
 * @param {object} [params] Additional parameters
 * @returns {_Draggable} a new Draggable
 */
export function makeDraggable(actor, params) {
    return new _Draggable(actor, params);
}
