// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const DEFAULT_FADE_FACTOR = 0.4;

const GLSL_DIM_EFFECT_DECLARATIONS = '\
float compute_dim_factor (const vec2 coords) {\
   vec2 dist = coords - vec2(0.5, 0.5); \
   float elipse_radius = 0.5; \
   /* interpolate darkening value, based on distance from screen center */ \
   float val = min(length(dist), elipse_radius); \
   return mix(0.3, 1.0, val / elipse_radius) * 0.4; \
}';
const GLSL_DIM_EFFECT_CODE = '\
   float a = compute_dim_factor (cogl_tex_coord0_in.xy);\
   cogl_color_out = vec4(0, 0, 0, cogl_color_in.a * a);'
;

const RadialShaderQuad = new Lang.Class({
    Name: 'RadialShaderQuad',
    Extends: Shell.GLSLQuad,

    vfunc_build_pipeline: function() {
        this.add_glsl_snippet(Shell.SnippetHook.FRAGMENT,
                              GLSL_DIM_EFFECT_DECLARATIONS,
                              GLSL_DIM_EFFECT_CODE,
                              true);
    },
});

/**
 * Lightbox:
 * @container: parent Clutter.Container
 * @params: (optional) additional parameters:
 *           - inhibitEvents: whether to inhibit events for @container
 *           - width: shade actor width
 *           - height: shade actor height
 *           - fadeInTime: seconds used to fade in
 *           - fadeOutTime: seconds used to fade out
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
 * this by passing an explicit width and height in @params.
 */
const Lightbox = new Lang.Class({
    Name: 'Lightbox',

    _init : function(container, params) {
        params = Params.parse(params, { inhibitEvents: false,
                                        width: null,
                                        height: null,
                                        fadeFactor: DEFAULT_FADE_FACTOR,
                                        radialEffect: false,
                                      });

        this._container = container;
        this._children = container.get_children();
        this._fadeFactor = params.fadeFactor;
        if (params.radialEffect)
            this.actor = new RadialShaderQuad({ x: 0,
                                                y: 0,
                                                reactive: params.inhibitEvents });
        else
            this.actor = new St.Bin({ x: 0,
                                      y: 0,
                                      style_class: 'lightbox',
                                      reactive: params.inhibitEvents });

        container.add_actor(this.actor);
        this.actor.raise_top();
        this.actor.hide();
        this.shown = false;

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        if (params.width && params.height) {
            this.actor.width = params.width;
            this.actor.height = params.height;
        } else {
            let constraint = new Clutter.BindConstraint({ source: container,
                                                          coordinate: Clutter.BindCoordinate.ALL });
            this.actor.add_constraint(constraint);
        }

        this._actorAddedSignalId = container.connect('actor-added', Lang.bind(this, this._actorAdded));
        this._actorRemovedSignalId = container.connect('actor-removed', Lang.bind(this, this._actorRemoved));

        this._highlighted = null;
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

    show: function(fadeInTime) {
        fadeInTime = fadeInTime || 0;

        Tweener.removeTweens(this.actor);
        if (fadeInTime != 0) {
            this.shown = false;
            this.actor.opacity = 0;
            Tweener.addTween(this.actor,
                             { opacity: 255 * this._fadeFactor,
                               time: fadeInTime,
                               transition: 'easeOutQuad',
                               onComplete: Lang.bind(this, function() {
                                   this.shown = true;
                                   this.emit('shown');
                               })
                             });
        } else {
            this.actor.opacity = 255 * this._fadeFactor;
            this.shown = true;
            this.emit('shown');
        }
        this.actor.show();
    },

    hide: function(fadeOutTime) {
        fadeOutTime = fadeOutTime || 0;

        this.shown = false;
        Tweener.removeTweens(this.actor);
        if (fadeOutTime != 0) {
            Tweener.addTween(this.actor,
                             { opacity: 0,
                               time: fadeOutTime,
                               transition: 'easeOutQuad',
                               onComplete: Lang.bind(this, function() {
                                   this.actor.hide();
                               })
                             });
        } else {
            this.actor.hide();
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
     * Destroys the lightbox.
     */
    destroy : function() {
        this.actor.destroy();
    },

    /**
     * _onDestroy:
     *
     * This is called when the lightbox' actor is destroyed, either
     * by destroying its container or by explicitly calling this.destroy().
     */
    _onDestroy: function() {
        this._container.disconnect(this._actorAddedSignalId);
        this._container.disconnect(this._actorRemovedSignalId);

        this.highlight(null);
    }
});
Signals.addSignalMethods(Lightbox.prototype);
