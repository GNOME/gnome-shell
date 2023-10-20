import St from 'gi://St';
import Clutter from 'gi://Clutter';

import * as Params from './params.js';

const SCROLL_TIME = 100;

const WIGGLE_OFFSET = 6;
const WIGGLE_DURATION = 65;
const N_WIGGLES = 3;

/**
 * adjustAnimationTime:
 *
 * @param {number} msecs - time in milliseconds
 * @param {object} params - optional parameters
 * @param {boolean=} params.animationRequired - whether to ignore the enable-animations setting
 *
 * Adjust `msecs` to account for St's enable-animations
 * and slow-down-factor settings
 */
export function adjustAnimationTime(msecs, params) {
    params = Params.parse(params, {
        animationRequired: false,
    });

    const settings = St.Settings.get();

    if (!settings.enable_animations && !params.animationRequired)
        return 0;
    return settings.slow_down_factor * msecs;
}

/**
 * Animate scrolling a scrollview until an actor is visible.
 *
 * @param {St.ScrollView} scrollView - the scroll view the actor is in
 * @param {Clutter.Actor} actor - the actor
 */
export function ensureActorVisibleInScrollView(scrollView, actor) {
    const adjustment = scrollView.vadjustment;
    let [value, lower_, upper, stepIncrement_, pageIncrement_, pageSize] = adjustment.get_values();

    let offset = 0;
    const vfade = scrollView.get_effect('fade');
    if (vfade)
        offset = vfade.fade_margins.top;

    let box = actor.get_allocation_box();
    let y1 = box.y1, y2 = box.y2;

    let parent = actor.get_parent();
    while (parent !== scrollView) {
        if (!parent)
            throw new Error('actor not in scroll view');

        box = parent.get_allocation_box();
        y1 += box.y1;
        y2 += box.y1;
        parent = parent.get_parent();
    }

    if (y1 < value + offset)
        value = Math.max(0, y1 - offset);
    else if (y2 > value + pageSize - offset)
        value = Math.min(upper, y2 + offset - pageSize);
    else
        return;

    adjustment.ease(value, {
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        duration: SCROLL_TIME,
    });
}

/**
 * "Wiggles" a clutter actor. A "wiggle" is an animation the moves an actor
 * back and forth on the X axis a specified amount of times.
 *
 * @param {Clutter.Actor} actor - an actor to animate
 * @param {object} params - options for the animation
 * @param {number} params.offset - the offset to move the actor by per-wiggle
 * @param {number} params.duration - the amount of time to move the actor per-wiggle
 * @param {number} params.wiggleCount - the number of times to wiggle the actor
 */
export function wiggle(actor, params) {
    if (!St.Settings.get().enable_animations)
        return;

    params = Params.parse(params, {
        offset: WIGGLE_OFFSET,
        duration: WIGGLE_DURATION,
        wiggleCount: N_WIGGLES,
    });
    actor.translation_x = 0;

    // Accelerate before wiggling
    actor.ease({
        translation_x: -params.offset,
        duration: params.duration,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
            // Wiggle
            actor.ease({
                translation_x: params.offset,
                duration: params.duration,
                mode: Clutter.AnimationMode.LINEAR,
                repeatCount: params.wiggleCount,
                autoReverse: true,
                onComplete: () => {
                    // Decelerate and return to the original position
                    actor.ease({
                        translation_x: 0,
                        duration: params.duration,
                        mode: Clutter.AnimationMode.EASE_IN_QUAD,
                    });
                },
            });
        },
    });
}
