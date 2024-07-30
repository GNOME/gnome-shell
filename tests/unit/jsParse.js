import 'resource:///org/gnome/shell/ui/environment.js';

import * as JsParse from 'resource:///org/gnome/shell/misc/jsParse.js';

//
// Test javascript parsing
//
//
// TODO: We probably want to change all these to use
//       a table driven method, using for() inside
//       a test body hampers readibility and
//       debuggability when something goes wrong.
//
// NOTE: The inconsistent use of "" and '' quotes in this
//       file is largely to handle nesting without passing
//       escape markers to the functions under test. The
//       preferred style in the shell is single quotes
//       so we use that wherever possible.

describe('Matching quote search', () => {
    const FindMatchingQuoteParameters = [
        {
            name: 'only double quotes',
            input: "'double quotes'",
            output: 0,
        },
        {
            name: 'only single quotes',
            input: '\'single quotes\'',
            output: 0,
        },
        {
            name: 'some parts unquoted and other parts quoted',
            input: "some unquoted 'some quoted'",
            output: 14,
        },
        {
            name: 'mixed quotes',
            input: "'mixed \" quotes\"'",
            output: 0,
        },
        {
            name: 'escaped quotes',
            input: "'escaped \\' quote'",
            output: 0,
        },
    ];

    for (const {name, input, output} of FindMatchingQuoteParameters) {
        it(`finds a matching quote where there are ${name}`, () => {
            const match = JsParse.findMatchingQuote(input, input.length - 1);
            expect(match).toEqual(output);
        });
    }
});

describe('Matching slash search', () => {
    const FindMatchingSlashParameters = [
        {
            name: 'matching slashes',
            input: '/slash/',
            output: 0,
        },
        {
            name: 'matching slashes with extraneous characters in-between',
            input: '/slash " with $ funny ^\' stuff/',
            output: 0,
        },
        {
            name: 'mathcing slashes with some parts unslashed',
            input: 'some unslashed /some slashed/',
            output: 15,
        },
        {
            name: 'matching slashes with an escaped slash in the middle',
            input: '/escaped \\/ slash/',
            output: 0,
        },
    ];

    for (let {name, input, output} of FindMatchingSlashParameters) {
        it(`finds a matching slash where there are ${name}`, () => {
            let match = JsParse.findMatchingQuote(input, input.length - 1);
            expect(match).toEqual(output);
        });
    }
});

describe('Matching brace search', () => {
    const FindMatchingBraceParameters = [
        {
            name: 'square braces',
            input: '[square brace]',
            output: 0,
        },
        {
            name: 'round braces',
            input: '(round brace)',
            output: 0,
        },
        {
            name: 'braces with nesting',
            input: '([()][nesting!])',
            output: 0,
        },
        {
            name: 'braces with quoted braces in the middle',
            input: "[we have 'quoted [' braces]",
            output: 0,
        },
        {
            name: 'braces with regexed braces in the middle',
            input: '[we have /regex [/ braces]',
            output: 0,
        },
        {
            name: 'mismatched braces',
            input: '([[])[] mismatched braces ]',
            output: 1,
        },
    ];

    for (let {name, input, output} of FindMatchingBraceParameters) {
        it(`finds matching braces where there are ${name}`, () => {
            let match = JsParse.findMatchingBrace(input, input.length - 1);
            expect(match).toEqual(output);
        });
    }
});

describe('Beginning of expression search', () => {
    const ExpressionOffsetParameters = [
        {
            name: 'object property name',
            input: 'abc.123',
            output: 0,
        },
        {
            name: 'function call result property name',
            input: 'foo().bar',
            output: 0,
        },
        {
            name: 'centre of malformed function call expression',
            input: 'foo(bar',
            output: 4,
        },
        {
            name: 'complete nested expression',
            input: 'foo[abc.match(/"/)]',
            output: 0,
        },
    ];

    for (const {name, input, output} of ExpressionOffsetParameters) {
        it(`finds the beginning of a ${name}`, () => {
            const match = JsParse.getExpressionOffset(input, input.length - 1);
            expect(match).toEqual(output);
        });
    }
});

describe('Constant variable search', () => {
    const DeclaredConstantsParameters = [
        {
            name: 'two constants on one line with space between equals',
            input: 'const foo = X; const bar = Y',
            output: ['foo', 'bar'],
        },
        {
            name: 'two constants on one line with no space between equlas',
            input: 'const foo=X; const bar=Y;',
            output: ['foo', 'bar'],
        },
    ];

    for (const {name, input, output} of DeclaredConstantsParameters) {
        it(`finds ${name}`, () => {
            const match = JsParse.getDeclaredConstants(input);
            expect(match).toEqual(output);
        });
    }
});

describe('Expression safety determination', () => {
    const UnsafeExpressionParams = [
        {
            name: 'property access',
            input: 'foo.bar',
            output: false,
        },
        {
            name: 'property access by array',
            input: 'foo[\'bar\']',
            output: false,
        },
        {
            name: 'expression with syntax error',
            input: 'foo["a=b=c".match(/=/)',
            output: false,
        },
        {
            name: 'property access by array with nested const expression',
            input: 'foo[1==2]',
            output: false,
        },
        {
            name: 'bracketed assignment no whitespace',
            input: '(x=4)',
            output: true,
        },
        {
            name: 'bracked assignment with whitespace',
            input: '(x = 4)',
            output: true,
        },
        {
            name: 'bracketed implicit call',
            input: '(x;y)',
            output: true,
        },
    ];

    for (const {name, input, output} of UnsafeExpressionParams) {
        const isOrIsNot = output ? 'is' : 'is not';
        it(`finds that an expression which is a ${name} ${isOrIsNot} safe`, () => {
            let unsafe = JsParse.isUnsafeExpression(input);
            expect(unsafe).toEqual(output);
        });
    }
});

//
// Test safety of eval to get completions
//
describe('Expression evaluation', () => {
    const HARNESS_COMMAND_HEADER =
        'let imports = obj;' +
        'let global = obj;' +
        'let Main = obj;' +
        'let foo = obj;' +
        'let r = obj;';

    const ExpressionParameters = [
        "foo['a",
        "foo()['b'",
        "obj.foo()('a', 1, 2, 'b')().",
        'foo.[.',
        'foo]]]()))].',
        "123'ab\"",
        'Main.foo.bar = 3; bar.',
        '(Main.foo = 3).',
        'Main[Main.foo+=-1].',
    ];

    function evalIfSafe(text) {
        // Just as in JsParse.getCompletions, we will find the offset
        // of the expression, test whether it is unsafe, and then eval it.
        const offset = JsParse.getExpressionOffset(text, text.length - 1);
        if (offset < 0)
            return;

        const matches = text.slice(offset).match(/(.*)\.(.*)/);
        if (matches == null)
            return;

        const [, base] = matches;
        if (JsParse.isUnsafeExpression(base))
            return;

        eval(HARNESS_COMMAND_HEADER + base);
    }

    for (const expression of ExpressionParameters) {
        const globalPropsPre = Object.getOwnPropertyNames(globalThis).sort();

        it(`of ${expression} does not throw syntax errors with a known safe expression`, () => {
            expect(() => evalIfSafe(expression)).not.toThrow(SyntaxError);
        });

        const globalPropsPost = Object.getOwnPropertyNames(globalThis).sort();
        it(`of ${expression} does not modify the global scope`, () => {
            expect(globalPropsPre).toEqual(globalPropsPost);
        });
    }
});
