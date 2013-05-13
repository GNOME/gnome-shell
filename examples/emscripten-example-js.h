
#ifndef _EMSCRIPTEN_EXAMPLE_JS_H_
#define _EMSCRIPTEN_EXAMPLE_JS_H_

/*
 * example_js_add_input_listener:
 *
 * Adds an input event listener to the browser's mainloop and whenever
 * input is received then the emscripten mainloop is resumed, if it
 * has been paused.
 *
 * This means we don't have to poll SDL for events and can instead go
 * to sleep waiting in the browser mainloop when there's no input and
 * nothing being animated.
 */
void example_js_add_input_listener (void);

#endif /* _EMSCRIPTEN_EXAMPLE_JS_H_ */
