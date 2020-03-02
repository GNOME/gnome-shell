// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported parse */

// parse:
// @params: caller-provided parameter object, or %null
// @defaults-provided defaults object
// @allowExtras: whether or not to allow properties not in @default
//
// Examines @params and fills in default values from @defaults for
// any properties in @defaults that don't appear in @params. If
// @allowExtras is not %true, it will throw an error if @params
// contains any properties that aren't in @defaults.
//
// If @params is %null, this returns the values from @defaults.
//
// Return value: a new object, containing the merged parameters from
// @params and @defaults
function parse(params = {}, defaults, allowExtras) {
    if (!allowExtras) {
        for (let prop in params) {
            if (!(prop in defaults))
                throw new Error(`Unrecognized parameter "${prop}"`);
        }
    }

    let defaultsCopy = Object.assign({}, defaults);
    return Object.assign(defaultsCopy, params);
}
