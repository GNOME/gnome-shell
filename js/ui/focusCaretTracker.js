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
/* exported FocusCaretTracker */

const Atspi = imports.gi.Atspi;
const Signals = imports.misc.signals;

const CARETMOVED        = 'object:text-caret-moved';
const STATECHANGED      = 'object:state-changed';

var FocusCaretTracker = class FocusCaretTracker extends Signals.EventEmitter {
    constructor() {
        super();

        this._atspiListener = Atspi.EventListener.new(this._onChanged.bind(this));

        this._atspiInited = false;
        this._focusListenerRegistered = false;
        this._caretListenerRegistered = false;
    }

    _onChanged(event) {
        if (event.type.indexOf(STATECHANGED) == 0)
            this.emit('focus-changed', event);
        else if (event.type == CARETMOVED)
            this.emit('caret-moved', event);
    }

    _initAtspi() {
        if (!this._atspiInited && Atspi.init() == 0) {
            Atspi.set_timeout(250, 250);
            this._atspiInited = true;
        }

        return this._atspiInited;
    }

    registerFocusListener() {
        if (!this._initAtspi() || this._focusListenerRegistered)
            return;

        this._atspiListener.register(`${STATECHANGED}:focused`);
        this._atspiListener.register(`${STATECHANGED}:selected`);
        this._focusListenerRegistered = true;
    }

    registerCaretListener() {
        if (!this._initAtspi() || this._caretListenerRegistered)
            return;

        this._atspiListener.register(CARETMOVED);
        this._caretListenerRegistered = true;
    }

    deregisterFocusListener() {
        if (!this._focusListenerRegistered)
            return;

        this._atspiListener.deregister(`${STATECHANGED}:focused`);
        this._atspiListener.deregister(`${STATECHANGED}:selected`);
        this._focusListenerRegistered = false;
    }

    deregisterCaretListener() {
        if (!this._caretListenerRegistered)
            return;

        this._atspiListener.deregister(CARETMOVED);
        this._caretListenerRegistered = false;
    }
};
