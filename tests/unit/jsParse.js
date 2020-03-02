/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

// Test cases for MessageTray URLification

const JsUnit = imports.jsUnit;

const Environment = imports.ui.environment;
Environment.init();

const JsParse = imports.misc.jsParse;

const HARNESS_COMMAND_HEADER = "let imports = obj;" +
                               "let global = obj;" +
                               "let Main = obj;" +
                               "let foo = obj;" +
                               "let r = obj;";

const testsFindMatchingQuote = [
    { input: '"double quotes"',
      output: 0 },
    { input: '\'single quotes\'',
      output: 0 },
    { input: 'some unquoted "some quoted"',
      output: 14 },
    { input: '"mixed \' quotes\'"',
      output: 0 },
    { input: '"escaped \\" quote"',
      output: 0 }
];
const testsFindMatchingSlash = [
    { input: '/slash/',
      output: 0 },
    { input: '/slash " with $ funny ^\' stuff/',
      output: 0  },
    { input: 'some unslashed /some slashed/',
      output: 15 },
    { input: '/escaped \\/ slash/',
      output: 0 }
];
const testsFindMatchingBrace = [
    { input: '[square brace]',
      output: 0 },
    { input: '(round brace)',
      output: 0  },
    { input: '([()][nesting!])',
      output: 0 },
    { input: '[we have "quoted [" braces]',
      output: 0 },
    { input: '[we have /regex [/ braces]',
      output: 0 },
    { input: '([[])[] mismatched braces ]',
      output: 1 }
];
const testsGetExpressionOffset = [
    { input: 'abc.123',
      output: 0 },
    { input: 'foo().bar',
      output: 0  },
    { input: 'foo(bar',
      output: 4 },
    { input: 'foo[abc.match(/"/)]',
      output: 0 }
];
const testsGetDeclaredConstants = [
    { input: 'const foo = X; const bar = Y;',
      output: ['foo', 'bar'] },
    { input: 'const foo=X; const bar=Y',
      output: ['foo', 'bar'] }
];
const testsIsUnsafeExpression = [
    { input: 'foo.bar',
      output: false },
    { input: 'foo[\'bar\']',
      output: false  },
    { input: 'foo["a=b=c".match(/=/)',
      output: false },
    { input: 'foo[1==2]',
      output: false },
    { input: '(x=4)',
      output: true },
    { input: '(x = 4)',
      output: true },
    { input: '(x;y)',
      output: true }
];
const testsModifyScope = [
    "foo['a",
    "foo()['b'",
    "obj.foo()('a', 1, 2, 'b')().",
    "foo.[.",
    "foo]]]()))].",
    "123'ab\"",
    "Main.foo.bar = 3; bar.",
    "(Main.foo = 3).",
    "Main[Main.foo+=-1]."
];



// Utility function for comparing arrays
function assertArrayEquals(errorMessage, array1, array2) {
    JsUnit.assertEquals(errorMessage + ' length',
                        array1.length, array2.length);
    for (let j = 0; j < array1.length; j++) {
        JsUnit.assertEquals(errorMessage + ' item ' + j,
                            array1[j], array2[j]);
    }
}

//
// Test javascript parsing
//

for (let i = 0; i < testsFindMatchingQuote.length; i++) {
    let text = testsFindMatchingQuote[i].input;
    let match = JsParse.findMatchingQuote(text, text.length - 1);

    JsUnit.assertEquals('Test testsFindMatchingQuote ' + i,
			match, testsFindMatchingQuote[i].output);
}

for (let i = 0; i < testsFindMatchingSlash.length; i++) {
    let text = testsFindMatchingSlash[i].input;
    let match = JsParse.findMatchingSlash(text, text.length - 1);

    JsUnit.assertEquals('Test testsFindMatchingSlash ' + i,
			match, testsFindMatchingSlash[i].output);
}

for (let i = 0; i < testsFindMatchingBrace.length; i++) {
    let text = testsFindMatchingBrace[i].input;
    let match = JsParse.findMatchingBrace(text, text.length - 1);

    JsUnit.assertEquals('Test testsFindMatchingBrace ' + i,
			match, testsFindMatchingBrace[i].output);
}

for (let i = 0; i < testsGetExpressionOffset.length; i++) {
    let text = testsGetExpressionOffset[i].input;
    let match = JsParse.getExpressionOffset(text, text.length - 1);

    JsUnit.assertEquals('Test testsGetExpressionOffset ' + i,
			match, testsGetExpressionOffset[i].output);
}

for (let i = 0; i < testsGetDeclaredConstants.length; i++) {
    let text = testsGetDeclaredConstants[i].input;
    let match = JsParse.getDeclaredConstants(text);

    assertArrayEquals('Test testsGetDeclaredConstants ' + i,
		      match, testsGetDeclaredConstants[i].output);
}

for (let i = 0; i < testsIsUnsafeExpression.length; i++) {
    let text = testsIsUnsafeExpression[i].input;
    let unsafe = JsParse.isUnsafeExpression(text);

    JsUnit.assertEquals('Test testsIsUnsafeExpression ' + i,
			unsafe, testsIsUnsafeExpression[i].output);
}

//
// Test safety of eval to get completions
//

for (let i = 0; i < testsModifyScope.length; i++) {
    let text = testsModifyScope[i];
    // We need to use var here for the with statement
    var obj = {};

    // Just as in JsParse.getCompletions, we will find the offset
    // of the expression, test whether it is unsafe, and then eval it.
    let offset = JsParse.getExpressionOffset(text, text.length - 1);
    if (offset >= 0) {
        text = text.slice(offset);

        let matches = text.match(/(.*)\.(.*)/);
        if (matches) {
            let [expr, base, attrHead] = matches;

            if (!JsParse.isUnsafeExpression(base)) {
                with (obj) {
                    try {
                        eval(HARNESS_COMMAND_HEADER + base);
                    } catch (e) {
                        JsUnit.assertNotEquals("Code '" + base + "' is valid code", e.constructor, SyntaxError);
                    }
                }
            }
        }
    }
    let propertyNames = Object.getOwnPropertyNames(obj);
    JsUnit.assertEquals("The context '" + JSON.stringify(obj) + "' was not modified", propertyNames.length, 0);
}
