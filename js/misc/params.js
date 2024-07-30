/**
 * parse:
 *
 * @param {*} params caller-provided parameter object, or %null
 * @param {*} defaults provided defaults object
 * @param {boolean} [allowExtras] whether or not to allow properties not in `default`
 *
 * @summary Examines `params` and fills in default values from `defaults` for
 * any properties in `default` that don't appear in `params`. If
 * `allowExtras` is not %true, it will throw an error if `params`
 * contains any properties that aren't in `defaults`.
 *
 * If `params` is %null, this returns the values from `defaults`.
 *
 * @returns a new object, containing the merged parameters from
 * `params` and `defaults`
 */
export function parse(params = {}, defaults, allowExtras) {
    if (!allowExtras) {
        for (let prop in params) {
            if (!(prop in defaults))
                throw new Error(`Unrecognized parameter "${prop}"`);
        }
    }

    return {...defaults, ...params};
}
