/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;
const St = imports.gi.St;

function Link(props) {
    this._init(props);
}

Link.prototype = {
    _init : function(props) {
        let realProps = { reactive: true,
                          style_class: 'shell-link' };
        // The user can pass in reactive: false to override the above and get
        // a non-reactive link (a link to the current page, perhaps)
        Lang.copyProperties(props, realProps);

        this.actor = new St.Button(realProps);
    }
};

Signals.addSignalMethods(Link.prototype);
