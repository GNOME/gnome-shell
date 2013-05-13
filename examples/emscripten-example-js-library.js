//"use strict";

var LibraryEXAMPLE = {
    $EXAMPLE__deps: ['$Browser'],
    $EXAMPLE: {
	receiveEvent: function(event) {
	    Browser.mainLoop.resume();
	},
    },
    example_js_add_input_listener: function() {
	['mousedown', 'mouseup', 'mousemove', 'DOMMouseScroll',
         'mousewheel', 'mouseout'].forEach(function(event) {
	    Module['canvas'].addEventListener(event, EXAMPLE.receiveEvent, true);
	});
    }
};

autoAddDeps(LibraryEXAMPLE, '$EXAMPLE');
mergeInto(LibraryManager.library, LibraryEXAMPLE);
