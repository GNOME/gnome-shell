const JsUnit = imports.jsUnit;
const Params = imports.misc.params;

function assertParamsEqual(params, expected) {
    for (let p in params) {
        JsUnit.assertTrue(p in expected);
        JsUnit.assertEquals(params[p], expected[p]);
    }
}

let defaults = {
    foo: 'This is a test',
    bar: null,
    baz: 42
};

assertParamsEqual(
    Params.parse(null, defaults),
    defaults);

assertParamsEqual(
    Params.parse({ bar: 23 }, defaults),
    { foo: 'This is a test', bar: 23, baz: 42 });

JsUnit.assertRaises(
    () => {
        Params.parse({ extraArg: 'quz' }, defaults);
    });

assertParamsEqual(
    Params.parse({ extraArg: 'quz' }, defaults, true),
    { foo: 'This is a test', bar: null, baz: 42, extraArg: 'quz' });
