// Test cases for SearchResult description match highlighter

import Pango from 'gi://Pango';

import 'resource:///org/gnome/shell/ui/environment.js';
import {Highlighter} from 'resource:///org/gnome/shell/misc/util.js';

describe('Highlighter', () => {
    const tests = [
        {
            input: 'abc cba',
            terms: null,
            output: 'abc cba',
        },
        {
            input: 'abc cba',
            terms: [],
            output: 'abc cba',
        },
        {
            input: 'abc cba',
            terms: [''],
            output: 'abc cba',
        },
        {
            input: 'abc cba',
            terms: ['a'],
            output: '<b>a</b>bc cb<b>a</b>',
        },
        {
            input: 'abc cba',
            terms: ['a', 'a'],
            output: '<b>a</b>bc cb<b>a</b>',
        },
        {
            input: 'CaSe InSenSiTiVe',
            terms: ['cas', 'sens'],
            output: '<b>CaS</b>e In<b>SenS</b>iTiVe',
        },
        {
            input: 'This contains the < character',
            terms: null,
            output: 'This contains the &lt; character',
        },
        {
            input: 'Don\'t',
            terms: ['t'],
            output: 'Don&apos;<b>t</b>',
        },
        {
            input: 'Don\'t',
            terms: ['n\'t'],
            output: 'Do<b>n&apos;t</b>',
        },
        {
            input: 'Don\'t',
            terms: ['o', 't'],
            output: 'D<b>o</b>n&apos;<b>t</b>',
        },
        {
            input: 'salt&pepper',
            terms: ['salt'],
            output: '<b>salt</b>&amp;pepper',
        },
        {
            input: 'salt&pepper',
            terms: ['salt', 'alt'],
            output: '<b>salt</b>&amp;pepper',
        },
        {
            input: 'salt&pepper',
            terms: ['pepper'],
            output: 'salt&amp;<b>pepper</b>',
        },
        {
            input: 'salt&pepper',
            terms: ['salt', 'pepper'],
            output: '<b>salt</b>&amp;<b>pepper</b>',
        },
        {
            input: 'salt&pepper',
            terms: ['t', 'p'],
            output: 'sal<b>t</b>&amp;<b>p</b>e<b>p</b><b>p</b>er',
        },
        {
            input: 'salt&pepper',
            terms: ['t', '&', 'p'],
            output: 'sal<b>t</b><b>&amp;</b><b>p</b>e<b>p</b><b>p</b>er',
        },
        {
            input: 'salt&pepper',
            terms: ['e'],
            output: 'salt&amp;p<b>e</b>pp<b>e</b>r',
        },
        {
            input: 'salt&pepper',
            terms: ['&a', '&am', '&amp', '&amp;'],
            output: 'salt&amp;pepper',
        },
        {
            input: '&&&&&',
            terms: ['a'],
            output: '&amp;&amp;&amp;&amp;&amp;',
        },
        {
            input: '&;&;&;&;&;',
            terms: ['a'],
            output: '&amp;;&amp;;&amp;;&amp;;&amp;;',
        },
        {
            input: '&;&;&;&;&;',
            terms: [';'],
            output: '&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>&amp;<b>;</b>',
        },
        {
            input: '&amp;',
            terms: ['a'],
            output: '&amp;<b>a</b>mp;',
        },
    ];

    for (const test of tests) {
        const {terms, input, output: expected} = test;

        it(`highlights ${JSON.stringify(terms)} in "${input}"`, () => {
            const highlighter = new Highlighter(terms);
            const output = highlighter.highlight(input);

            expect(output).toEqual(expected);
            expect(() => Pango.parse_markup(output, -1, '')).not.toThrow();
        });
    }
});
