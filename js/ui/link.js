/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;

// Link is a clickable link. Right now it just handles properly capturing
// press and release events and short-circuiting the button handling in
// ClutterText, but more features like different colors for hover/pressed states
// or a different mouse cursor could be implemented.
//
// The properties passed in are forwarded to the Clutter.Text() constructor,
// so can include, 'text', 'font_name', etc.
function Link(props) {
    this._init(props);
}

Link.prototype = {
    _init : function(props) {
	let realProps = { reactive: true };
        // The user can pass in reactive: false to override the above and get
        // a non-reactive link (a link to the current page, perhaps)
	Lang.copyProperties(props, realProps);

	this.actor = new Clutter.Text(realProps);
	this.actor._delegate = this;
	this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
	this.actor.connect('button-release-event', Lang.bind(this, this._onButtonRelease));
	this.actor.connect('enter-event', Lang.bind(this, this._onEnter));
	this.actor.connect('leave-event', Lang.bind(this, this._onLeave));

	this._buttonDown = false;
	this._havePointer = false;
    },

    // Update the text of the link
    setText : function(text) {
	this.actor.text = text;
    },

    // We want to react on buttonDown, but if we override button-release-event for
    // ClutterText, but not button-press-event, we get a stuck grab. Tracking
    // buttonDown and doing the grab isn't really necessary, but doing it makes
    // the behavior perfectly correct if the user clicks on one actor, drags
    // to another and releases - that should not trigger either actor.
    _onButtonPress : function(actor, event) {
	this._buttonDown = true;
	this._havePointer = true; // Hack to work around poor enter/leave tracking in Clutter
	Clutter.grab_pointer(actor);

	return true;
    },

    _onButtonRelease : function(actor, event) {
	if (this._buttonDown) {
	    this._buttonDown = false;
	    Clutter.ungrab_pointer(actor);

	    if (this._havePointer)
		this.emit('clicked');
	}

	return true;
    },

    _onEnter : function(actor, event) {
	if (event.get_source() == actor)
	    this._havePointer = true;

        return false;
    },

    _onLeave : function(actor, event) {
	if (event.get_source() == actor)
	    this._havePointer = false;

        return false;
    }
};

Signals.addSignalMethods(Link.prototype);
