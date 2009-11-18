/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;

const Main = imports.ui.main;

const GnomeShellIface = {
    name: "org.gnome.Shell",
    methods: [{ name: "Eval",
                inSignature: "s",
                outSignature: "bs"
              }
             ],
    signals: [],
    properties: [{ name: "OverviewActive",
                   signature: "b",
                   access: "readwrite" }]
};

function GnomeShell() {
    this._init();
}

GnomeShell.prototype = {
    _init: function() {
        DBus.session.exportObject('/org/gnome/Shell', this);
    },

    /**
     * Eval:
     * @code: A string containing JavaScript code
     *
     * This function executes arbitrary code in the main
     * loop, and returns a boolean success and
     * JSON representation of the object as a string.
     *
     * If evaluation completes without throwing an exception,
     * then the return value will be [true, JSON.stringify(result)].
     * If evaluation fails, then the return value will be
     * [false, JSON.stringify(exception)];
     *
     */
    Eval: function(code) {
        let returnValue;
        let success;
        try {
            returnValue = JSON.stringify(eval(code));
            // A hack; DBus doesn't have null/undefined
            if (returnValue == undefined)
                returnValue = "";
            success = true;
        } catch (e) {
            returnValue = JSON.stringify(e);
            success = false;
        }
        return [success, returnValue];
    },

    get OverviewActive() {
        return Main.overview.visible;
    },

    set OverviewActive(visible) {
        if (visible)
            Main.overview.show();
        else
            Main.overview.hide();
    }
};

DBus.conformExport(GnomeShell.prototype, GnomeShellIface);

