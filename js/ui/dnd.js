// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const St = imports.gi.St;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Tweener = imports.ui.tweener;
const Main = imports.ui.main;

const Params = imports.misc.params;

// Time to scale down to maxDragActorSize
var SCALE_ANIMATION_TIME = 0.25;
// Time to animate to original position on cancel
var SNAP_BACK_ANIMATION_TIME = 0.25;
// Time to animate to original position on success
var REVERT_ANIMATION_TIME = 0.75;

var DragMotionResult = {
    NO_DROP:   0,
    COPY_DROP: 1,
    MOVE_DROP: 2,
    CONTINUE:  3
};

var DragState = {
    INIT:      0,
    DRAGGING:  1,
    CANCELLED: 2,
};

var DRAG_CURSOR_MAP = {
    0: Meta.Cursor.DND_UNSUPPORTED_TARGET,
    1: Meta.Cursor.DND_COPY,
    2: Meta.Cursor.DND_MOVE
};

var DragDropResult = {
    FAILURE:  0,
    SUCCESS:  1,
    CONTINUE: 2
};
var dragMonitors = [];

let eventHandlerActor = null;
let currentDraggable = null;

function _getEventHandlerActor() {
    if (!eventHandlerActor) {
        eventHandlerActor = new Clutter.Actor({ width: 0, height: 0 });
        Main.uiGroup.add_actor(eventHandlerActor);
        // We connect to 'event' rather than 'captured-event' because the capturing phase doesn't happen
        // when you've grabbed the pointer.
        eventHandlerActor.connect('event', (actor, event) => {
            return currentDraggable._onEvent(actor, event);
        });
    }
    return eventHandlerActor;
}

function addDragMonitor(monitor) {
    dragMonitors.push(monitor);
}

function removeDragMonitor(monitor) {
    for (let i = 0; i < dragMonitors.length; i++)
        if (dragMonitors[i] == monitor) {
            dragMonitors.splice(i, 1);
            return;
        }
}

var _Draggable = new Lang.Class({
    Name: 'Draggable',

    _init(actor, params) {
        params = Params.parse(params, { manualMode: false,
                                        restoreOnSuccess: false,
                                        dragActorMaxSize: undefined,
                                        dragActorOpacity: undefined });

        this.actor = actor;
        this._dragState = DragState.INIT;

        if (!params.manualMode) {
            this.actor.connect('button-press-event',
                               this._onButtonPress.bind(this));
            this.actor.connect('touch-event',
                               this._onTouchEvent.bind(this));
        }

        this.actor.connect('destroy', () => {
            this._actorDestroyed = true;

            if (this._dragState == DragState.DRAGGING && this._dragCancellable)
                this._cancelDrag(global.get_current_time());
            this.disconnectAll();
        });
        this._onEventId = null;
        this._touchSequence = null;

        this._restoreOnSuccess = params.restoreOnSuccess;
        this._dragActorMaxSize = params.dragActorMaxSize;
        this._dragActorOpacity = params.dragActorOpacity;

        this._buttonDown = false; // The mouse button has been pressed and has not yet been released.
        this._animationInProgress = false; // The drag is over and the item is in the process of animating to its original position (snapping back or reverting).
        this._dragCancellable = true;

        this._eventsGrabbed = false;
    },

    _onButtonPress(actor, event) {
        if (event.get_button() != 1)
            return Clutter.EVENT_PROPAGATE;

        if (Tweener.getTweenCount(actor))
            return Clutter.EVENT_PROPAGATE;

        this._buttonDown = true;
        this._grabActor();

        let [stageX, stageY] = event.get_coords();
        this._dragStartX = stageX;
        this._dragStartY = stageY;

        return Clutter.EVENT_PROPAGATE;
    },

    _onTouchEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCH_BEGIN ||
            !global.display.is_pointer_emulating_sequence(event.get_event_sequence()))
            return Clutter.EVENT_PROPAGATE;

        if (Tweener.getTweenCount(actor))
            return Clutter.EVENT_PROPAGATE;

        this._touchSequence = event.get_event_sequence();

        this._buttonDown = true;
        this._grabActor();

        let [stageX, stageY] = event.get_coords();
        this._dragStartX = stageX;
        this._dragStartY = stageY;

        return Clutter.EVENT_PROPAGATE;
    },

    _grabDevice(actor) {
        let manager = Clutter.DeviceManager.get_default();
        let pointer = manager.get_core_device(Clutter.InputDeviceType.POINTER_DEVICE);

        if (pointer && this._touchSequence)
            pointer.sequence_grab(this._touchSequence, actor);
        else if (pointer)
            pointer.grab (actor);

        this._grabbedDevice = pointer;
    },

    _ungrabDevice() {
        if (this._touchSequence)
            this._grabbedDevice.sequence_ungrab (this._touchSequence);
        else
            this._grabbedDevice.ungrab();

        this._touchSequence = null;
        this._grabbedDevice = null;
    },

    _grabActor() {
        this._grabDevice(this.actor);
        this._onEventId = this.actor.connect('event',
                                             this._onEvent.bind(this));
    },

    _ungrabActor() {
        if (!this._onEventId)
            return;

        this._ungrabDevice();
        this.actor.disconnect(this._onEventId);
        this._onEventId = null;
    },

    _grabEvents() {
        if (!this._eventsGrabbed) {
            this._eventsGrabbed = Main.pushModal(_getEventHandlerActor());
            if (this._eventsGrabbed)
                this._grabDevice(_getEventHandlerActor());
        }
    },

    _ungrabEvents() {
        if (this._eventsGrabbed) {
            this._ungrabDevice();
            Main.popModal(_getEventHandlerActor());
            this._eventsGrabbed = false;
        }
    },

    _onEvent(actor, event) {
        // We intercept BUTTON_RELEASE event to know that the button was released in case we
        // didn't start the drag, to drop the draggable in case the drag was in progress, and
        // to complete the drag and ensure that whatever happens to be under the pointer does
        // not get triggered if the drag was cancelled with Esc.
        if (event.type() == Clutter.EventType.BUTTON_RELEASE ||
            (event.type() == Clutter.EventType.TOUCH_END &&
             global.display.is_pointer_emulating_sequence(event.get_event_sequence()))) {
            this._buttonDown = false;
            if (this._dragState == DragState.DRAGGING) {
                return this._dragActorDropped(event);
            } else if ((this._dragActor != null || this._dragState == DragState.CANCELLED) &&
                       !this._animationInProgress) {
                // Drag must have been cancelled with Esc.
                this._dragComplete();
                return Clutter.EVENT_STOP;
            } else {
                // Drag has never started.
                this._ungrabActor();
                return Clutter.EVENT_PROPAGATE;
            }
        // We intercept MOTION event to figure out if the drag has started and to draw
        // this._dragActor under the pointer when dragging is in progress
        } else if (event.type() == Clutter.EventType.MOTION ||
                   (event.type() == Clutter.EventType.TOUCH_UPDATE &&
                    global.display.is_pointer_emulating_sequence(event.get_event_sequence()))) {
            if (this._dragActor && this._dragState == DragState.DRAGGING) {
                return this._updateDragPosition(event);
            } else if (this._dragActor == null && this._dragState != DragState.CANCELLED) {
                return this._maybeStartDrag(event);
            }
        // We intercept KEY_PRESS event so that we can process Esc key press to cancel
        // dragging and ignore all other key presses.
        } else if (event.type() == Clutter.EventType.KEY_PRESS && this._dragState == DragState.DRAGGING) {
            let symbol = event.get_key_symbol();
            if (symbol == Clutter.Escape) {
                this._cancelDrag(event.get_time());
                return Clutter.EVENT_STOP;
            }
        }

        return Clutter.EVENT_PROPAGATE;
    },

    /**
     * fakeRelease:
     *
     * Fake a release event.
     * Must be called if you want to intercept release events on draggable
     * actors for other purposes (for example if you're using
     * PopupMenu.ignoreRelease())
     */
    fakeRelease() {
        this._buttonDown = false;
        this._ungrabActor();
    },

    /**
     * startDrag:
     * @stageX: X coordinate of event
     * @stageY: Y coordinate of event
     * @time: Event timestamp
     *
     * Directly initiate a drag and drop operation from the given actor.
     * This function is useful to call if you've specified manualMode
     * for the draggable.
     */
    startDrag(stageX, stageY, time, sequence) {
        currentDraggable = this;
        this._dragState = DragState.DRAGGING;

        // Special-case St.Button: the pointer grab messes with the internal
        // state, so force a reset to a reasonable state here
        if (this.actor instanceof St.Button) {
            this.actor.fake_release();
            this.actor.hover = false;
        }

        this.emit('drag-begin', time);
        if (this._onEventId)
            this._ungrabActor();

        this._touchSequence = sequence;
        this._grabEvents();
        global.screen.set_cursor(Meta.Cursor.DND_IN_DRAG);

        this._dragX = this._dragStartX = stageX;
        this._dragY = this._dragStartY = stageY;

        if (this.actor._delegate && this.actor._delegate.getDragActor) {
            this._dragActor = this.actor._delegate.getDragActor();
            Main.uiGroup.add_child(this._dragActor);
            this._dragActor.raise_top();
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
                if (stageX > sourceX && stageX <= sourceX + this._dragActor.width &&
                    stageY > sourceY && stageY <= sourceY + this._dragActor.height) {
                    x = sourceX;
                    y = sourceY;
                } else {
                    x = stageX - this._dragActor.width / 2;
                    y = stageY - this._dragActor.height / 2;
                }
                this._dragActor.set_position(x, y);
            } else {
                this._dragActorSource = this.actor;
            }
            this._dragOrigParent = undefined;

            this._dragOffsetX = this._dragActor.x - this._dragStartX;
            this._dragOffsetY = this._dragActor.y - this._dragStartY;
        } else {
            this._dragActor = this.actor;

            this._dragActorSource = undefined;
            this._dragOrigParent = this.actor.get_parent();
            this._dragOrigX = this._dragActor.x;
            this._dragOrigY = this._dragActor.y;
            this._dragOrigScale = this._dragActor.scale_x;

            // Set the actor's scale such that it will keep the same
            // transformed size when it's reparented to the uiGroup
            let [scaledWidth, scaledHeight] = this.actor.get_transformed_size();
            this._dragActor.set_scale(scaledWidth / this.actor.width,
                                      scaledHeight / this.actor.height);

            let [actorStageX, actorStageY] = this.actor.get_transformed_position();
            this._dragOffsetX = actorStageX - this._dragStartX;
            this._dragOffsetY = actorStageY - this._dragStartY;

            this._dragOrigParent.remove_actor(this._dragActor);
            Main.uiGroup.add_child(this._dragActor);
            this._dragActor.raise_top();
            Shell.util_set_hidden_from_pick(this._dragActor, true);
        }

        this._dragActorDestroyId = this._dragActor.connect('destroy', () => {
            // Cancel ongoing animation (if any)
            this._finishAnimation();

            this._dragActor = null;
            this._dragState = DragState.CANCELLED;
        });
        this._dragOrigOpacity = this._dragActor.opacity;
        if (this._dragActorOpacity != undefined)
            this._dragActor.opacity = this._dragActorOpacity;

        this._snapBackX = this._dragStartX + this._dragOffsetX;
        this._snapBackY = this._dragStartY + this._dragOffsetY;
        this._snapBackScale = this._dragActor.scale_x;

        if (this._dragActorMaxSize != undefined) {
            let [scaledWidth, scaledHeight] = this._dragActor.get_transformed_size();
            let currentSize = Math.max(scaledWidth, scaledHeight);
            if (currentSize > this._dragActorMaxSize) {
                let scale = this._dragActorMaxSize / currentSize;
                let origScale =  this._dragActor.scale_x;
                let origDragOffsetX = this._dragOffsetX;
                let origDragOffsetY = this._dragOffsetY;

                // The position of the actor changes as we scale
                // around the drag position, but we can't just tween
                // to the final position because that tween would
                // fight with updates as the user continues dragging
                // the mouse; instead we do the position computations in
                // an onUpdate() function.
                Tweener.addTween(this._dragActor,
                                 { scale_x: scale * origScale,
                                   scale_y: scale * origScale,
                                   time: SCALE_ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onUpdate() {
                                       let currentScale = this._dragActor.scale_x / origScale;
                                       this._dragOffsetX = currentScale * origDragOffsetX;
                                       this._dragOffsetY = currentScale * origDragOffsetY;
                                       this._dragActor.set_position(this._dragX + this._dragOffsetX,
                                                                    this._dragY + this._dragOffsetY);
                                   },
                                   onUpdateScope: this });
            }
        }
    },

    _maybeStartDrag(event) {
        let [stageX, stageY] = event.get_coords();

        // See if the user has moved the mouse enough to trigger a drag
        let threshold = Gtk.Settings.get_default().gtk_dnd_drag_threshold;
        if ((Math.abs(stageX - this._dragStartX) > threshold ||
             Math.abs(stageY - this._dragStartY) > threshold)) {
            this.startDrag(stageX, stageY, event.get_time(), this._touchSequence);
            this._updateDragPosition(event);
        }

        return true;
    },

    _updateDragHover() {
        this._updateHoverId = 0;
        let target = this._dragActor.get_stage().get_actor_at_pos(Clutter.PickMode.ALL,
                                                                  this._dragX, this._dragY);
        let dragEvent = {
            x: this._dragX,
            y: this._dragY,
            dragActor: this._dragActor,
            source: this.actor._delegate,
            targetActor: target
        };
        for (let i = 0; i < dragMonitors.length; i++) {
            let motionFunc = dragMonitors[i].dragMotion;
            if (motionFunc) {
                let result = motionFunc(dragEvent);
                if (result != DragMotionResult.CONTINUE) {
                    global.screen.set_cursor(DRAG_CURSOR_MAP[result]);
                    return GLib.SOURCE_REMOVE;
                }
            }
        }

        while (target) {
            if (target._delegate && target._delegate.handleDragOver) {
                let [r, targX, targY] = target.transform_stage_point(this._dragX, this._dragY);
                // We currently loop through all parents on drag-over even if one of the children has handled it.
                // We can check the return value of the function and break the loop if it's true if we don't want
                // to continue checking the parents.
                let result = target._delegate.handleDragOver(this.actor._delegate,
                                                             this._dragActor,
                                                             targX,
                                                             targY,
                                                             0);
                if (result != DragMotionResult.CONTINUE) {
                    global.screen.set_cursor(DRAG_CURSOR_MAP[result]);
                    return GLib.SOURCE_REMOVE;
                }
            }
            target = target.get_parent();
        }
        global.screen.set_cursor(Meta.Cursor.DND_IN_DRAG);
        return GLib.SOURCE_REMOVE;
    },

    _queueUpdateDragHover() {
        if (this._updateHoverId)
            return;

        this._updateHoverId = GLib.idle_add(GLib.PRIORITY_DEFAULT,
                                            this._updateDragHover.bind(this));
        GLib.Source.set_name_by_id(this._updateHoverId, '[gnome-shell] this._updateDragHover');
    },

    _updateDragPosition(event) {
        let [stageX, stageY] = event.get_coords();
        this._dragX = stageX;
        this._dragY = stageY;
        this._dragActor.set_position(stageX + this._dragOffsetX,
                                     stageY + this._dragOffsetY);

        this._queueUpdateDragHover();
        return true;
    },

    _dragActorDropped(event) {
        let [dropX, dropY] = event.get_coords();
        let target = this._dragActor.get_stage().get_actor_at_pos(Clutter.PickMode.ALL,
                                                                  dropX, dropY);

        // We call observers only once per motion with the innermost
        // target actor. If necessary, the observer can walk the
        // parent itself.
        let dropEvent = {
            dropActor: this._dragActor,
            targetActor: target,
            clutterEvent: event
        };
        for (let i = 0; i < dragMonitors.length; i++) {
            let dropFunc = dragMonitors[i].dragDrop;
            if (dropFunc)
                switch (dropFunc(dropEvent)) {
                    case DragDropResult.FAILURE:
                    case DragDropResult.SUCCESS:
                        return true;
                    case DragDropResult.CONTINUE:
                        continue;
                }
        }

        // At this point it is too late to cancel a drag by destroying
        // the actor, the fate of which is decided by acceptDrop and its
        // side-effects
        this._dragCancellable = false;

        while (target) {
            if (target._delegate && target._delegate.acceptDrop) {
                let [r, targX, targY] = target.transform_stage_point(dropX, dropY);
                if (target._delegate.acceptDrop(this.actor._delegate,
                                                this._dragActor,
                                                targX,
                                                targY,
                                                event.get_time())) {
                    // If it accepted the drop without taking the actor,
                    // handle it ourselves.
                    if (this._dragActor && this._dragActor.get_parent() == Main.uiGroup) {
                        if (this._restoreOnSuccess) {
                            this._restoreDragActor(event.get_time());
                            return true;
                        } else
                            this._dragActor.destroy();
                    }

                    this._dragState = DragState.INIT;
                    global.screen.set_cursor(Meta.Cursor.DEFAULT);
                    this.emit('drag-end', event.get_time(), true);
                    this._dragComplete();
                    return true;
                }
            }
            target = target.get_parent();
        }

        this._cancelDrag(event.get_time());

        return true;
    },

    _getRestoreLocation() {
        let x, y, scale;

        if (this._dragActorSource && this._dragActorSource.visible) {
            // Snap the clone back to its source
            [x, y] = this._dragActorSource.get_transformed_position();
            let [sourceScaledWidth, sourceScaledHeight] = this._dragActorSource.get_transformed_size();
            scale = sourceScaledWidth ? this._dragActor.width / sourceScaledWidth : 0;
        } else if (this._dragOrigParent) {
            // Snap the actor back to its original position within
            // its parent, adjusting for the fact that the parent
            // may have been moved or scaled
            let [parentX, parentY] = this._dragOrigParent.get_transformed_position();
            let [parentWidth, parentHeight] = this._dragOrigParent.get_size();
            let [parentScaledWidth, parentScaledHeight] = this._dragOrigParent.get_transformed_size();
            let parentScale = 1.0;
            if (parentWidth != 0)
                parentScale = parentScaledWidth / parentWidth;

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
    },

    _cancelDrag(eventTime) {
        this.emit('drag-cancelled', eventTime);
        let wasCancelled = (this._dragState == DragState.CANCELLED);
        this._dragState = DragState.CANCELLED;

        if (this._actorDestroyed || wasCancelled) {
            global.screen.set_cursor(Meta.Cursor.DEFAULT);
            if (!this._buttonDown)
                this._dragComplete();
            this.emit('drag-end', eventTime, false);
            if (!this._dragOrigParent && this._dragActor)
                this._dragActor.destroy();

            return;
        }

        let [snapBackX, snapBackY, snapBackScale] = this._getRestoreLocation();

        this._animateDragEnd(eventTime,
                             { x: snapBackX,
                               y: snapBackY,
                               scale_x: snapBackScale,
                               scale_y: snapBackScale,
                               time: SNAP_BACK_ANIMATION_TIME,
                             });
    },

    _restoreDragActor(eventTime) {
        this._dragState = DragState.INIT;
        let [restoreX, restoreY, restoreScale] = this._getRestoreLocation();

        // fade the actor back in at its original location
        this._dragActor.set_position(restoreX, restoreY);
        this._dragActor.set_scale(restoreScale, restoreScale);
        this._dragActor.opacity = 0;

        this._animateDragEnd(eventTime,
                             { time: REVERT_ANIMATION_TIME });
    },

    _animateDragEnd(eventTime, params) {
        this._animationInProgress = true;

        params['opacity']          = this._dragOrigOpacity;
        params['transition']       = 'easeOutQuad';
        params['onComplete']       = this._onAnimationComplete;
        params['onCompleteScope']  = this;
        params['onCompleteParams'] = [this._dragActor, eventTime];

        // start the animation
        Tweener.addTween(this._dragActor, params)
    },

    _finishAnimation() {
        if (!this._animationInProgress)
            return

        this._animationInProgress = false;
        if (!this._buttonDown)
            this._dragComplete();

        global.screen.set_cursor(Meta.Cursor.DEFAULT);
    },

    _onAnimationComplete(dragActor, eventTime) {
        if (this._dragOrigParent) {
            Main.uiGroup.remove_child(this._dragActor);
            this._dragOrigParent.add_actor(this._dragActor);
            dragActor.set_scale(this._dragOrigScale, this._dragOrigScale);
            dragActor.set_position(this._dragOrigX, this._dragOrigY);
        } else {
            dragActor.destroy();
        }

        this.emit('drag-end', eventTime, false);
        this._finishAnimation();
    },

    _dragComplete() {
        if (!this._actorDestroyed && this._dragActor)
            Shell.util_set_hidden_from_pick(this._dragActor, false);

        this._ungrabEvents();
        global.sync_pointer();

        if (this._updateHoverId) {
            GLib.source_remove(this._updateHoverId);
            this._updateHoverId = 0;
        }

        if (this._dragActor) {
            this._dragActor.disconnect(this._dragActorDestroyId);
            this._dragActor = null;
        }

        this._dragState = DragState.INIT;
        currentDraggable = null;
    }
});

Signals.addSignalMethods(_Draggable.prototype);

/**
 * makeDraggable:
 * @actor: Source actor
 * @params: (optional) Additional parameters
 *
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
 */
function makeDraggable(actor, params) {
    return new _Draggable(actor, params);
}
