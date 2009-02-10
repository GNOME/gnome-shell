/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Signals = imports.signals;

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Tweener = imports.ui.tweener;

const SNAP_BACK_ANIMATION_TIME = 0.25;

function _Draggable(actor) {
    this._init(actor);
}

_Draggable.prototype = {
    _init : function(actor) {
        this.actor = actor;
        this.actor.connect('button-press-event',
                           Lang.bind(this, this._onButtonPress));
    },

    _onButtonPress : function (actor, event) {
        // FIXME: we should make sure it's button 1, but we can't currently
        // check that from JavaScript
        if (Tweener.getTweenCount(actor))
            return false;

        this._grabActor(actor);

        let [stageX, stageY] = event.get_coords();
        this._dragStartX = stageX;
        this._dragStartY = stageY;

        return false;
    },
    
    _grabActor : function (actor) {
        Clutter.grab_pointer(actor);

        // We intercept motion and button-release events so that when
        // you release after dragging, the app doesn't see that and
        // think you just clicked. We connect to 'event' rather than
        // 'captured-event' because the capturing phase doesn't happen
        // when you've grabbed the pointer.
        this._onEventId = actor.connect('event',
                                        Lang.bind(this, this._onEvent));
    },

    _ungrabActor : function (actor) {
        Clutter.ungrab_pointer();
        actor.disconnect(this._onEventId);
    },

    _onEvent : function (actor, event) {
        if (this._dragActor) {
            if (actor != this._dragActor )
                return false;
        } else if (actor != this.actor)
            return false;

        if (event.type() == Clutter.EventType.BUTTON_RELEASE)
            return this._onButtonRelease(actor, event);
        else if (event.type() == Clutter.EventType.MOTION)
            return this._onMotion(actor, event);
        else
            return false;
    },

    _onMotion : function (actor, event) {
        let [stageX, stageY] = event.get_coords();

        // If we haven't begun a drag, see if the user has moved the
        // mouse enough to trigger a drag
        let threshold = Gtk.Settings.get_default().gtk_dnd_drag_threshold;
        if (!this._dragActor &&
            (Math.abs(stageX - this._dragStartX) > threshold ||
             Math.abs(stageY - this._dragStartY) > threshold)) {
            this.emit('drag-begin', event.get_time());

            if (this.actor._delegate && this.actor._delegate.getDragActor) {
                this._dragActor = this.actor._delegate.getDragActor(this._dragStartX, this._dragStartY);
                this._dragOrigParent = undefined;
                this._ungrabActor(actor);
                this._grabActor(this._dragActor);

                this._dragOffsetX = this._dragActor.x - this._dragStartX;
                this._dragOffsetY = this._dragActor.y - this._dragStartY;
            } else {
                this._dragActor = actor;
                this._dragOrigParent = actor.get_parent();
                this._dragOrigX = this._dragActor.x;
                this._dragOrigY = this._dragActor.y;
                this._dragOrigScale = this._dragActor.scale_x;

                let [actorStageX, actorStageY] = actor.get_transformed_position();
                this._dragOffsetX = actorStageX - this._dragStartX;
                this._dragOffsetY = actorStageY - this._dragStartY;

                // Set the actor's scale such that it will keep the same
                // transformed size when it's reparented to the stage
                let [scaledWidth, scaledHeight] = actor.get_transformed_size();
                actor.set_scale(scaledWidth / actor.width,
                                scaledHeight / actor.height);
            }

            this._dragActor.reparent(actor.get_stage());
            this._dragActor.raise_top();
        }

        // If we are dragging, update the position
        if (this._dragActor) {
            this._dragActor.set_position(stageX + this._dragOffsetX,
                                         stageY + this._dragOffsetY);
        }

        return true;
    },

    _onButtonRelease : function (actor, event) {
        this._ungrabActor(actor);

        let dragging = (actor == this._dragActor);
        this._dragActor = undefined;

        if (!dragging)
            return false;

        this.emit('drag-end', event.get_time());

        // Find a drop target
        actor.hide();
        let [dropX, dropY] = event.get_coords();
        let target = actor.get_stage().get_actor_at_pos(dropX, dropY);
        actor.show();
        while (target) {
            if (target._delegate && target._delegate.acceptDrop) {
                let [targX, targY] = target.get_transformed_position();
                if (target._delegate.acceptDrop(this.actor._delegate, actor,
                                                (dropX + this._xOffset - targX) / target.scale_x,
                                                (dropY + this._yOffset - targY) / target.scale_y,
                                                event.get_time())) {
                    // If it accepted the drop without taking the actor,
                    // destroy it.
                    if (actor.get_parent() == actor.get_stage())
                        actor.destroy();

                    return true;
                }
            }
            target = target.get_parent();
        }

        // No target, so snap back
        Tweener.addTween(actor,
                         { x: this._dragStartX + this._dragOffsetX,
                           y: this._dragStartY + this._dragOffsetY,
                           time: SNAP_BACK_ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._onSnapBackComplete,
                           onCompleteScope: this,
                           onCompleteParams: [actor]
                         });
        return true;
    },

    _onSnapBackComplete : function (dragActor) {
        if (this._dragOrigParent) {
            dragActor.reparent(this._dragOrigParent);
            dragActor.set_scale(this._dragOrigScale, this._dragOrigScale);
            dragActor.set_position(this._dragOrigX, this._dragOrigY);
        } else
            dragActor.destroy();
    }
};

Signals.addSignalMethods(_Draggable.prototype);

function makeDraggable(actor) {
    return new _Draggable(actor);
}