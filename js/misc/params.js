/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

// parse:
// @params: caller-provided parameter object, or %null
// @default: function-provided defaults object
// @allowExtras: whether or not to allow properties not in @default
//
// Examines @params and fills in default values from @defaults for
// any properties in @defaults that don't appear in @params. If
// @allowExtras is not %true, it will throw an error if @params
// contains any properties that aren't in @defaults.
//
// If @params is %null, this returns @defaults.
//
// Return value: the updated params
function parse(params, defaults, allowExtras) {
    if (!params)
        return defaults;

    if (!allowExtras) {
        for (let prop in params) {
            if (!(prop in defaults))
                throw new Error('Unrecognized parameter "' + prop + '"');
        }
    }

    for (let prop in defaults) {
        if (!(prop in params))
            params[prop] = defaults[prop];
    }

    return params;
}