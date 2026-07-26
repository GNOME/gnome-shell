import Clutter from 'gi://Clutter';

import * as Batch from './batch.js';
import * as Main from '../ui/main.js';

export const CLONE_FADE_ANIMATION_TIME = 250;

export const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
export const BANNER_MESSAGE_KEY = 'banner-message-enable';
export const BANNER_MESSAGE_SOURCE_KEY = 'banner-message-source';
export const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
export const BANNER_MESSAGE_PATH_KEY = 'banner-message-path';
export const ALLOWED_FAILURES_KEY = 'allowed-failures';

export const LOGO_KEY = 'logo';
export const DISABLE_USER_LIST_KEY = 'disable-user-list';

/**
 * @param {Clutter.Actor} actor
 */
export function cloneAndFadeOutActor(actor) {
    // Immediately hide actor so its sibling can have its space
    // and position, but leave a non-reactive clone on-screen,
    // so from the user's point of view it smoothly fades away
    // and reveals its sibling.
    actor.hide();

    const clone = new Clutter.Clone({
        source: actor,
        reactive: false,
    });

    Main.uiGroup.add_child(clone);

    const [x, y] = actor.get_transformed_position();
    clone.set_position(x, y);

    const hold = new Batch.Hold();
    clone.ease({
        opacity: 0,
        duration: CLONE_FADE_ANIMATION_TIME,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
            clone.destroy();
            hold.release();
        },
    });
    return hold;
}
