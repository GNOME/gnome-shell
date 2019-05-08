// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

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

var RadialShaderQuad = GObject.registerClass(
class RadialShaderQuad extends Shell.GLSLQuad {
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

var LightboxIface = GObject.registerClass({
    Requires: [Clutter.Actor],
    Signals: { 'shown': {} }
}, class LightboxIface extends GObject.Interface {
    _init(container, params) {
        this._container = container;
        this._children = container.get_children();
        this._fadeFactor = params.fadeFactor;
        this.visible = false;

        container.add_actor(this);
        this.raise_top();
        this.shown = false;

        this.connect('destroy', this._onDestroy.bind(this));

        if (params.width && params.height) {
            this.width = params.width;
            this.height = params.height;
        } else {
            let constraint = new Clutter.BindConstraint({ source: container,
                                                          coordinate: Clutter.BindCoordinate.ALL });
            this.add_constraint(constraint);
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

    setShown(shown) {
        this.shown = shown;

        if (shown)
            this.emit('shown');
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
        this._container.disconnect(this._actorAddedSignalId);
        this._container.disconnect(this._actorRemovedSignalId);

        this.highlight(null);
    }
});

var SimpleLightBox = GObject.registerClass({
    Implements: [LightboxIface]
}, class SimpleLightBox extends St.Bin {
    _init(container, params) {
        super._init({
            x: 0, y: 0,
            opacity: 0,
            style_class: 'lightbox',
            reactive: params.inhibitEvents
        });
        LightboxIface._init(this, container, params);
    }

    show(fadeInTime) {
        fadeInTime = fadeInTime || 0;

        Tweener.removeTweens(this);
        Tweener.addTween(this, {
            opacity: 255 * this._fadeFactor,
            time: fadeInTime,
            transition: 'easeOutQuad',
            onComplete: () => LightboxIface.setShown(this, true)
        });
        super.show();
    }

    hide(fadeOutTime) {
        fadeOutTime = fadeOutTime || 0;

        LightboxIface.setShown(this, false);
        Tweener.removeTweens(this);
        Tweener.addTween(this, {
            opacity: 0,
            time: fadeOutTime,
            transition: 'easeOutQuad',
            onComplete: () => super.hide()
        });
    }
});

var RadialShaderLightBox = GObject.registerClass({
    Implements: [LightboxIface]
}, class RadialShaderLightBox extends RadialShaderQuad {
    _init(container, params) {
        super._init({
            x: 0, y: 0,
            reactive: params.inhibitEvents
        });
        LightboxIface._init(this, container, params);
    }

    show(fadeInTime) {
        fadeInTime = fadeInTime || 0;

        Tweener.removeTweens(this);
        Tweener.addTween(this, {
            brightness: VIGNETTE_BRIGHTNESS,
            vignetteSharpness: VIGNETTE_SHARPNESS,
            time: fadeInTime,
            transition: 'easeOutQuad',
            onComplete: () => LightboxIface.setShown(this, true)
        });
        super.show();
    }

    hide(fadeOutTime) {
        fadeOutTime = fadeOutTime || 0;
        LightboxIface.setShown(this, false);
        Tweener.removeTweens(this);
        Tweener.addTween(this, {
            brightness: 1.0,
            vignetteSharpness: 0.0,
            opacity: 0,
            time: fadeOutTime,
            transition: 'easeOutQuad',
            onComplete: () => super.hide()
        });
    }
});

var LightboxClass = function(params) {
    params = Params.parse(params, {
        inhibitEvents: false,
        width: null,
        height: null,
        fadeFactor: DEFAULT_FADE_FACTOR,
        radialEffect: false,
    });

    if (params.radialEffect &&
        Clutter.feature_available(Clutter.FeatureFlags.SHADERS_GLSL))
        return RadialShaderLightBox;

    return SimpleLightBox;
};

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
var Lightbox = function() {
    return LightboxClass();
}();

/**
 * RadialLightbox:
 * @container: parent Clutter.Container
 * @params: (optional) additional parameters:
 *           - inhibitEvents: whether to inhibit events for @container
 *           - width: shade actor width
 *           - height: shade actor height
 *           - fadeInTime: seconds used to fade in
 *           - fadeOutTime: seconds used to fade out
 *
 * RadialLightbox creates a dark translucent "shade" actor to hide the
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
 *
 * If GLSL shaders are supported, a radial effect will be applied,
 * otherwise we fallback to a simple #Lightbox actor.
 */
var RadialLightbox = function() {
    return LightboxClass({ radialEffect: true });
}();
