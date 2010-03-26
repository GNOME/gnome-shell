/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Shell = imports.gi.Shell;
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

// Replace @method with something that throws an error instead
function _blockMethod(method, replacement, reason) {
    let match = method.match(/^(.+)\.([^.]+)$/);
    if (!match)
        throw new Error('Bad method name "' + method + '"');
    let proto = 'imports.gi.' + match[1] + '.prototype';
    let property = match[2];

    if (!global.set_property_mutable(proto, property, true))
        throw new Error('Bad method name "' + method + '"');

    // eval() is evil in general, but we know it's safe here since
    // set_property_mutable() would have failed if proto was
    // malformed.
    let node = eval(proto);

    let msg = 'Do not use "' + method + '".';
    if (replacement)
        msg += ' Use "' + replacement + '" instead.';
    if (reason)
        msg += ' (' + reason + ')';

    node[property] = function() {
        throw new Error(msg);
    };

    global.set_property_mutable(proto, property, false);
}

function init() {
    Tweener.init();
    String.prototype.format = Format.format;

    // Set the default direction for St widgets (this needs to be done before any use of St)
    if (Gettext_gtk20.gettext('default:LTR') == 'default:RTL') {
        St.Widget.set_default_direction(St.TextDirection.RTL);
    }

    _patchContainerClass(St.BoxLayout);
    _patchContainerClass(St.Table);

    _blockMethod('Clutter.Event.get_state', 'Shell.get_event_state',
                 'gjs\'s handling of Clutter.ModifierType is broken. See bug 597292.');
    _blockMethod('Gdk.Display.get_pointer', 'global.get_pointer',
                 'gjs\'s handling of Gdk.ModifierType is broken. See bug 597292.');
    _blockMethod('Gdk.Window.get_pointer', 'global.get_pointer',
                 'gjs\'s handling of Gdk.ModifierType is broken. See bug 597292.');

    // Now close the back door to prevent extensions from trying to
    // abuse it. We can't actually delete it since
    // Shell.Global.prototype itself is read-only.
    global.set_property_mutable('imports.gi.Shell.Global.prototype', 'set_property_mutable', true);
    Shell.Global.prototype.set_property_mutable = undefined;
}
