/** -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2012 Inclusive Design Research Centre, OCAD University.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Joseph Scheuhammer <clown@alum.mit.edu>
 * Contributor:
 *   Magdalen Berns <m.berns@sms.ed.ac.uk>
 */

const Atspi = imports.gi.Atspi;
const Lang = imports.lang;
const Signals = imports.signals;

const CARETMOVED        = 'object:text-caret-moved';
const STATECHANGED      = 'object:state-changed';

const FocusCaretTracker = new Lang.Class({
    Name: 'FocusCaretTracker',

    _init: function() {
        Atspi.init();
        Atspi.set_timeout(250, 250);
        this._atspiListener = Atspi.EventListener.new(Lang.bind(this, this._onChanged));
    },

    _onChanged: function(event) {
        if (event.type.indexOf(STATECHANGED) == 0)
            this.emit('focus-changed', event);
        else if (event.type == CARETMOVED)
            this.emit('caret-moved', event);
    },

    registerFocusListener: function() {
        return this._atspiListener.register(STATECHANGED + ':focused') &&
               this._atspiListener.register(STATECHANGED + ':selected');
    },

    registerCaretListener: function() {
        return this._atspiListener.register(CARETMOVED);
    },

    deregisterFocusListener: function() {
        return this._atspiListener.deregister(STATECHANGED + ':focused') &&
               this._atspiListener.deregister(STATECHANGED + ':selected');
    },

    deregisterCaretListener: function() {
        return this._atspiListener.deregister(CARETMOVED);
    }
});
Signals.addSignalMethods(FocusCaretTracker.prototype);
