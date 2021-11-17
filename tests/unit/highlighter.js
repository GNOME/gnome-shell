// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Test cases for SearchResult description match highlighter

const JsUnit = imports.jsUnit;
const Pango = imports.gi.Pango;

const Environment = imports.ui.environment;
Environment.init();

const Util = imports.misc.util;

const tests = [
    { input: 'abc cba',
      terms: null,
      output: 'abc cba' },
    { input: 'abc cba',
      terms: [],
      output: 'abc cba' },
    { input: 'abc cba',
      terms: [''],
      output: 'abc cba' },
    { input: 'abc cba',
      terms: ['a'],
      output: '<b>a</b>bc cb<b>a</b>' },
    { input: 'abc cba',
      terms: ['a', 'a'],
      output: '<b>a</b>bc cb<b>a</b>' },
    { input: 'CaSe InSenSiTiVe',
      terms: ['cas', 'sens'],
      output: '<b>CaS</b>e In<b>SenS</b>iTiVe' },
    { input: 'This contains the < character',
      terms: null,
      output: 'This contains the &lt; character' },
    { input: 'Don\'t',
      terms: ['t'],
      output: 'Don&apos;<b>t</b>' },
    { input: 'Don\'t',
      terms: ['n\'t'],
      output: 'Do<b>n&apos;t</b>' },
    { input: 'Don\'t',
      terms: ['o', 't'],
      output: 'D<b>o</b>n&apos;<b>t</b>' },
    { input: 'salt&pepper',
      terms: ['salt'],
      output: '<b>salt</b>&amp;pepper' },
    { input: 'salt&pepper',
      terms: ['salt', 'alt'],
      output: '<b>salt</b>&amp;pepper' },
    { input: 'salt&pepper',
      terms: ['pepper'],
      output: 'salt&amp;<b>pepper</b>' },
    { input: 'salt&pepper',
      terms: ['salt', 'pepper'],
      output: '<b>salt</b>&amp;<b>pepper</b>' },
    { input: 'salt&pepper',
      terms: ['t', 'p'],
      output: 'sal<b>t</b>&amp;<b>p</b>e<b>p</b><b>p</b>er' },
    { input: 'salt&pepper',
      terms: ['t', '&', 'p'],
      output: 'sal<b>t</b><b>&amp;</b><b>p</b>e<b>p</b><b>p</b>er' },
    { input: 'salt&pepper',
      terms: ['e'],
      output: 'salt&amp;p<b>e</b>pp<b>e</b>r' },
    { input: 'salt&pepper',
      terms: ['&a', '&am', '&amp', '&amp;'],
      output: 'salt&amp;pepper' },
    { input: '&&&&&',
      terms: ['a'],
      output: '&amp;&amp;&amp;&amp;&amp;' },
    { input: '&;&;&;&;&;',
      terms: ['a'],
      output: '&amp;;&amp;;&amp;;&amp;;&amp;;' },
    { input: '&;&;&;&;&;',
      terms: [';'],
      output: '&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>' },
    { input: '&amp;',
      terms: ['a'],
      output: '&amp;<b>a</b>mp;' }
];

try {
    for (let i = 0; i < tests.length; i++) {
        let highlighter = new Util.Highlighter(tests[i].terms);
        let output = highlighter.highlight(tests[i].input);

        JsUnit.assertEquals(`Test ${i + 1} highlight ` +
            `"${tests[i].terms}" in "${tests[i].input}"`,
            output, tests[i].output);

        let parsed = false;
        try {
            Pango.parse_markup(output, -1, '');
            parsed = true;
        } catch (e) {}
        JsUnit.assertEquals(`Test ${i + 1} is valid markup`, true, parsed);
    }
} catch (e) {
    if (typeof(e.isJsUnitException) != 'undefined'
        && e.isJsUnitException)
    {
        if (e.comment)
            log(`Error in: ${e.comment}`);
    }
    throw e;
}
