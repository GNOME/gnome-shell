/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const St = imports.gi.St;
const Gettext_gtk20 = imports.gettext.domain('gtk20');

const Tweener = imports.ui.tweener;

const Format = imports.misc.format;

// "monkey patch" in some varargs ClutterContainer methods; we need
// to do this per-container class since there is no representation
// of interfaces in Javascript
function _patchContainerClass(containerClass) {
    // This one is a straightforward mapping of the C method
    containerClass.prototype.child_set = function(actor, props) {
        let meta = this.get_child_meta(actor);
        for (prop in props)
            meta[prop] = props[prop];
    };

    // clutter_container_add() actually is a an add-many-actors
    // method. We conveniently, but somewhat dubiously, take the
    // this opportunity to make it do something more useful.
    containerClass.prototype.add = function(actor, props) {
        this.add_actor(actor);
        if (props)
            this.child_set(actor, props);
    };
}

_patchContainerClass(St.BoxLayout);
_patchContainerClass(St.Table);

function init() {
    Tweener.init();
    String.prototype.format = Format.format;

    // Set the default direction for St widgets (this needs to be done before any use of St)
    if (Gettext_gtk20.gettext("default:LTR") == "default:RTL") {
        St.Widget.set_default_direction(St.TextDirection.RTL);
    }
}
