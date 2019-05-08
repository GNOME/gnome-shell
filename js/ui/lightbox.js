// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Lightbox */

const { Clutter, GObject, Shell, St } = imports.gi;

const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

var DEFAULT_FADE_FACTOR = 0.4;
var VIGNETTE_BRIGHTNESS = 0.2;
var VIGNETTE_SHARPNESS = 0.7;

const VIGNETTE_DECLARATIONS = '\
uniform float brightness;\n\
uniform float vignette_sharpness;\n';

const VIGNETTE_CODE = '\
cogl_color_out.a = cogl_color_in.a;\n\
cogl_color_out.rgb = vec3(0.0, 0.0, 0.0);\n\
vec2 position = cogl_tex_coord_in[0].xy - 0.5;\n\
float t = length(2.0 * position);\n\
t = clamp(t, 0.0, 1.0);\n\
float pixel_brightness = mix(1.0, 1.0 - vignette_sharpness, t);\n\
cogl_color_out.a = cogl_color_out.a * (1 - pixel_brightness * brightness);';

var RadialShaderEffect = GObject.registerClass(
class RadialShaderEffect extends Shell.GLSLEffect {
    _init(params) {
        super._init(params);

        this._brightnessLocation = this.get_uniform_location('brightness');
        this._sharpnessLocation = this.get_uniform_location('vignette_sharpness');

        this.brightness = 1.0;
        this.vignetteSharpness = 0.0;
    }

    vfunc_build_pipeline() {
        this.add_glsl_snippet(Shell.SnippetHook.FRAGMENT,
                              VIGNETTE_DECLARATIONS, VIGNETTE_CODE, true);
    }

    get brightness() {
        return this._brightness;
    }

    set brightness(v) {
        this._brightness = v;
        this.set_uniform_float(this._brightnessLocation,
                               1, [this._brightness]);
    }

    get vignetteSharpness() {
        return this._sharpness;
    }

    set vignetteSharpness(v) {
        this._sharpness = v;
        this.set_uniform_float(this._sharpnessLocation,
                               1, [this._sharpness]);
    }
});

/**
 * Lightbox:
 * @container: parent Clutter.Container
 * @params: (optional) additional parameters:
 *           - inhibitEvents: whether to inhibit events for @container
 *           - width: shade actor width
 *           - height: shade actor height
 *           - fadeFactor: fading opacity factor
 *           - radialEffect: whether to enable the GLSL radial effect
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
var Lightbox = GObject.registerClass({
    Signals: { 'shown': {} },
    Properties: {
        'inhibit-events': GObject.ParamSpec.boolean('inhibit-events',
                                                    'inhibit-events',
                                                    'inhibit-events',
                                                    GObject.ParamFlags.READWRITE |
                                                    GObject.ParamFlags.CONSTRUCT_ONLY,
                                                    false),
        'radial-effect': GObject.ParamSpec.boolean('radial-effect',
                                                   'radial-effect',
                                                   'radial-effect',
                                                   GObject.ParamFlags.READWRITE |
                                                   GObject.ParamFlags.CONSTRUCT_ONLY,
                                                   false),
        'fade-factor': GObject.ParamSpec.double('fade-factor',
                                                'fade-factor',
                                                'fade-factor',
                                                 GObject.ParamFlags.READWRITE |
                                                 GObject.ParamFlags.CONSTRUCT_ONLY,
                                                 0, 1, DEFAULT_FADE_FACTOR),
    }
}, class Lightbox extends St.Bin {
    _init(container, params = {}) {
        super._init({ visible: false, ...params });

        this.bind_property('inhibit-events', this, 'reactive',
                           GObject.BindingFlags.SYNC_CREATE);

        this.shown = true;
        this._container = container;
        this._children = container.get_children();

        if (this.radial_effect &&
            !Clutter.feature_available(Clutter.FeatureFlags.SHADERS_GLSL))
            this.radial_effect = false;

        if (this.radial_effect)
            this.add_effect(new RadialShaderEffect({ name: 'radial' }));
        else
            this.set({ opacity: 0, style_class: 'lightbox' });

        container.add_actor(this);
        this.raise_top();

        this.connect('destroy', this._onDestroy.bind(this));

        if (!params.width || !params.height) {
            this.set({
                width: 0, height: 0,
                style_class: 'lightbox',
                constraints: new Clutter.BindConstraint({
                    source: container,
                    coordinate: Clutter.BindCoordinate.ALL
                })
            });
        }

        this._actorAddedSignalId = container.connect('actor-added', this._actorAdded.bind(this));
        this._actorRemovedSignalId = container.connect('actor-removed', this._actorRemoved.bind(this));

        this._highlighted = null;
    }

    _actorAdded(container, newChild) {
        let children = this._container.get_children();
        let myIndex = children.indexOf(this);
        let newChildIndex = children.indexOf(newChild);

        if (newChildIndex > myIndex) {
            // The child was added above the shade (presumably it was
            // made the new top-most child). Move it below the shade,
            // and add it to this._children as the new topmost actor.
            newChild.lower(this);
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
    }

    show(fadeInTime) {
        let tweenTarget = this;
        let showTweenParams = {
            time: fadeInTime || 0,
            transition: 'easeOutQuad',
            onComplete: () => {
                this.shown = true;
                this.emit('shown');
            }
        };

        if (this.radial_effect) {
            tweenTarget = this.get_effect('radial');
            showTweenParams.brightness = VIGNETTE_BRIGHTNESS;
            showTweenParams.vignetteSharpness = VIGNETTE_SHARPNESS;
        } else {
            showTweenParams.opacity = 255 * this._fadeFactor;
        }

        Tweener.removeTweens(tweenTarget);
        Tweener.addTween(tweenTarget, showTweenParams);

        super.show();
    }

    hide(fadeOutTime) {
        let tweenTarget = this;
        let hideTweenParams = {
            opacity: 0,
            time: fadeOutTime || 0,
            transition: 'easeOutQuad',
            onComplete: () => super.hide()
        };

        if (this.radial_effect) {
            tweenTarget = this.get_effect('radial');
            hideTweenParams.brightness = 1.0;
            hideTweenParams.vignetteSharpness = 0.0;
        }

        this.shown = false;
        Tweener.removeTweens(tweenTarget);
        Tweener.addTween(tweenTarget, hideTweenParams);
    }

    _actorRemoved(container, child) {
        let index = this._children.indexOf(child);
        if (index != -1) // paranoia
            this._children.splice(index, 1);

        if (child == this._highlighted)
            this._highlighted = null;
    }

    /**
     * highlight:
     * @window: actor to highlight
     *
     * Highlights the indicated actor and unhighlights any other
     * currently-highlighted actor. With no arguments or a false/null
     * argument, all actors will be unhighlighted.
     */
    highlight(window) {
        if (this._highlighted == window)
            return;

        // Walk this._children raising and lowering actors as needed.
        // Things get a little tricky if the to-be-raised and
        // to-be-lowered actors were originally adjacent, in which
        // case we may need to indicate some *other* actor as the new
        // sibling of the to-be-lowered one.

        let below = this;
        for (let i = this._children.length - 1; i >= 0; i--) {
            if (this._children[i] == window)
                this._children[i].raise_top();
            else if (this._children[i] == this._highlighted)
                this._children[i].lower(below);
            else
                below = this._children[i];
        }

        this._highlighted = window;
    }

    /**
     * _onDestroy:
     *
     * This is called when the lightbox' actor is destroyed, either
     * by destroying its container or by explicitly calling this.destroy().
     */
    _onDestroy() {
        if (this._actorAddedSignalId) {
            this._container.disconnect(this._actorAddedSignalId);
            this._actorAddedSignalId = 0;
        }
        if (this._actorRemovedSignalId) {
            this._container.disconnect(this._actorRemovedSignalId);
            this._actorRemovedSignalId = 0;
        }

        this.highlight(null);
    }
});
