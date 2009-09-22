/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const SHADE_COLOR = new Clutter.Color();
SHADE_COLOR.from_pixel(0x00000044);

/**
 * Lightbox:
 * @container: parent Clutter.Container
 * @width: (optional) shade actor width
 * @height: (optional) shade actor height
 *
 * Lightbox creates a dark translucent "shade" actor to hide the
 * contents of @container, and allows you to specify particular actors
 * in @container to highlight by bringing them above the shade. It
 * tracks added and removed actors in @container while the lightboxing
 * is active, and ensures that all actors are returned to their
 * original stacking order when the lightboxing is removed. (However,
 * if actors are restacked by outside code while the lightboxing is
 * active, the lightbox may later revert them back to their original
 * order.)
 *
 * By default, the shade window will have the height and width of
 * @container and will track any changes in its size. You can override
 * this by passing an explicit width and height
 */
function Lightbox(container, width, height) {
    this._init(container, width, height);
}

Lightbox.prototype = {
    _init : function(container, width, height) {
        this._container = container;
        this._children = container.get_children();
        this.actor = new Clutter.Rectangle({ color: SHADE_COLOR,
                                             x: 0,
                                             y: 0,
                                             border_width: 0,
                                             reactive: true });

        container.add_actor(this.actor);
        this.actor.raise_top();

        this._destroySignalId = this.actor.connect('destroy', Lang.bind(this, this.destroy));

        if (width && height) {
            this.actor.width = width;
            this.actor.height = height;
            this._allocationChangedSignalId = 0;
        } else {
            this.actor.width = container.width;
            this.actor.height = container.height;
            this._allocationChangedSignalId = container.connect('allocation-changed', Lang.bind(this, this._allocationChanged));
        }

        this._actorAddedSignalId = container.connect('actor-added', Lang.bind(this, this._actorAdded));
        this._actorRemovedSignalId = container.connect('actor-removed', Lang.bind(this, this._actorRemoved));

        this._highlighted = null;
    },

    _allocationChanged : function(container, box, flags) {
        this.actor.width = this._container.width;
        this.actor.height = this._container.height;
    },

    _actorAdded : function(container, newChild) {
        let children = this._container.get_children();
        let myIndex = children.indexOf(this.actor);
        let newChildIndex = children.indexOf(newChild);

        if (newChildIndex > myIndex) {
            // The child was added above the shade (presumably it was
            // made the new top-most child). Move it below the shade,
            // and add it to this._children as the new topmost actor.
            newChild.lower(this.actor);
            this._children.push(newChild);
        } else if (newChildIndex == 0) {
            // Bottom of stack
            this._children.unshift(newChild);
        } else {
            // Somewhere else; insert it into the correct spot
            let prevChild = this._children.indexOf(children[newChildIndex - 1]);
            if (prevChild != -1) // paranoia
                this._children.splice(prevChild + 1, 0, newChild);
        }
    },

    _actorRemoved : function(container, child) {
        let index = this._children.indexOf(child);
        if (index != -1) // paranoia
            this._children.splice(index, 1);

        if (child == this._highlighted)
            this._highlighted = null;
    },

    /**
     * highlight:
     * @window: actor to highlight
     *
     * Highlights the indicated actor and unhighlights any other
     * currently-highlighted actor. With no arguments or a false/null
     * argument, all actors will be unhighlighted.
     */
    highlight : function(window) {
        if (this._highlighted == window)
            return;

        // Walk this._children raising and lowering actors as needed.
        // Things get a little tricky if the to-be-raised and
        // to-be-lowered actors were originally adjacent, in which
        // case we may need to indicate some *other* actor as the new
        // sibling of the to-be-lowered one.

        let below = this.actor;
        for (let i = this._children.length - 1; i >= 0; i--) {
            if (this._children[i] == window)
                this._children[i].raise_top();
            else if (this._children[i] == this._highlighted)
                this._children[i].lower(below);
            else
                below = this._children[i];
        }

        this._highlighted = window;
    },

    /**
     * destroy:
     *
     * Destroys the lightbox. This is called automatically if the
     * lightbox's container is destroyed.
     */
    destroy : function() {
        if (this._allocationChangedSignalId != 0)
            this._container.disconnect(this._allocationChangedSignalId);
        this._container.disconnect(this._actorAddedSignalId);
        this._container.disconnect(this._actorRemovedSignalId);

        this.actor.disconnect(this._destroySignalId);

        this.highlight(null);
        this.actor.destroy();
    }
};
