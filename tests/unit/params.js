const JsUnit = imports.jsUnit;
import * as Params from 'resource:///org/gnome/shell/misc/params.js';

/**
 * Asserts that two "param" objects have the same properties
 * with the same values.
 *
 * @param {object} params the parsed params
 * @param {object} expected the expected params
 */
function assertParamsEqual(params, expected) {
    for (let p in params) {
        JsUnit.assertTrue(p in expected);
        JsUnit.assertEquals(params[p], expected[p]);
    }
}

let defaults = {
    foo: 'This is a test',
    bar: null,
    baz: 42,
};

assertParamsEqual(
    Params.parse(null, defaults),
    defaults);

assertParamsEqual(
    Params.parse({bar: 23}, defaults),
    {foo: 'This is a test', bar: 23, baz: 42});

JsUnit.assertRaises(
    () => {
        Params.parse({extraArg: 'quz'}, defaults);
    });

assertParamsEqual(
    Params.parse({extraArg: 'quz'}, defaults, true),
    {foo: 'This is a test', bar: null, baz: 42, extraArg: 'quz'});
