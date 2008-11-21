/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

function WindowManager() {
    this._init();
}

WindowManager.prototype = {
    _init : function() {
	let global = Shell.global_get();
        let shellwm = global.window_manager;

	shellwm.connect('switch-workspace',
	    function(o, from, to, direction) {
	        let actors = shellwm.get_switch_workspace_actors();
		for (let i = 0; i < actors.length; i++) {
		    if (actors[i].get_workspace() == from)
			actors[i].hide();
		    else if (actors[i].get_workspace() == to)
			actors[i].show();
		}
	    });
    }
};
