// Test cases for MessageList markup parsing

import Pango from 'gi://Pango';

import 'resource:///org/gnome/shell/ui/environment.js';
import {fixMarkup} from 'resource:///org/gnome/shell/misc/util.js';

describe('fixMarkup()', () => {
    function convertAndEscape(text) {
        return {
            converted: fixMarkup(text, true),
            escaped: fixMarkup(text, false),
        };
    }

    beforeAll(() => {
        jasmine.addMatchers({
            toParseCorrectlyAndMatch: () => {
                function isMarkupValid(markup) {
                    try {
                        Pango.parse_markup(markup, -1, '');
                    } catch {
                        return false;
                    }
                    return true;
                }

                return {
                    compare: (actual, expected) => {
                        if (!expected)
                            expected = actual;

                        return {
                            pass: isMarkupValid(actual) && actual === expected,
                            message: `Expected "${actual}" to parse correctly and equal "${expected}"`,
                        };
                    },
                };
            },
        });
    });

    it('does not do anything on no markup', () => {
        const text = 'foo';
        const result = convertAndEscape(text);
        expect(result.converted).toParseCorrectlyAndMatch(text);
        expect(result.escaped).toParseCorrectlyAndMatch(text);
    });

    it('converts and escapes bold markup', () => {
        const text = '<b>foo</b>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;foo&lt;/b&gt;');
    });

    it('converts and escapes italic markup', () => {
        const text = 'something <i>foo</i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('something &lt;i&gt;foo&lt;/i&gt;');
    });

    it('converts and escapes underlined markup', () => {
        const text = '<u>foo</u> something';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('&lt;u&gt;foo&lt;/u&gt; something');
    });

    it('converts and escapes non-nested bold italic and underline markup', () => {
        const text = '<b>bold</b> <i>italic <u>and underlined</u></i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;bold&lt;/b&gt; &lt;i&gt;italic &lt;u&gt;and underlined&lt;/u&gt;&lt;/i&gt;');
    });

    it('converts and escapes ampersands', () => {
        const text = 'this &amp; that';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('this &amp;amp; that');
    });

    it('converts and escapes <', () => {
        const text = 'this &lt; that';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('this &amp;lt; that');
    });

    it('converts and escapes >', () => {
        const text = 'this &lt; that &gt; the other';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('this &amp;lt; that &amp;gt; the other');
    });

    it('converts and escapes HTML markup inside escaped tags', () => {
        const text = 'this &lt;<i>that</i>&gt;';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('this &amp;lt;&lt;i&gt;that&lt;/i&gt;&amp;gt;');
    });

    it('converts and escapes angle brackets within HTML markup', () => {
        const text = '<b>this</b> > <i>that</i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;this&lt;/b&gt; &gt; &lt;i&gt;that&lt;/i&gt;');
    });

    it('converts and escapes markup whilst still keeping an unrecognized entity', () => {
        const text = '<b>smile</b> &#9786;!';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch();
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;smile&lt;/b&gt; &amp;#9786;!');
    });

    it('converts and escapes markup and a stray ampersand', () => {
        const text = '<b>this</b> & <i>that</i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('<b>this</b> &amp; <i>that</i>');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;this&lt;/b&gt; &amp; &lt;i&gt;that&lt;/i&gt;');
    });

    it('converts and escapes a stray <', () => {
        const text = 'this < that';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('this &lt; that');
        expect(escaped).toParseCorrectlyAndMatch('this &lt; that');
    });

    it('converts and escapes markup with a stray <', () => {
        const text = '<b>this</b> < <i>that</i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('<b>this</b> &lt; <i>that</i>');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;this&lt;/b&gt; &lt; &lt;i&gt;that&lt;/i&gt;');
    });

    it('converts and escapes stray less than and greater than characters that do not form tags', () => {
        const text = 'this < that > the other';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('this &lt; that > the other');
        expect(escaped).toParseCorrectlyAndMatch('this &lt; that &gt; the other');
    });

    it('converts and escapes stray less than and greater than characters next to HTML markup tags', () => {
        const text = 'this <<i>that</i>>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('this &lt;<i>that</i>>');
        expect(escaped).toParseCorrectlyAndMatch('this &lt;&lt;i&gt;that&lt;/i&gt;&gt;');
    });

    it('converts and escapes angle brackets around unknown tags', () => {
        const text = '<unknown>tag</unknown>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;unknown>tag&lt;/unknown>');
        expect(escaped).toParseCorrectlyAndMatch('&lt;unknown&gt;tag&lt;/unknown&gt;');
    });

    it('converts and escapes angle brackets around unknown tags where the first letter might otherwise be valid HTML markup', () => {
        const text = '<bunknown>tag</bunknown>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;bunknown>tag&lt;/bunknown>');
        expect(escaped).toParseCorrectlyAndMatch('&lt;bunknown&gt;tag&lt;/bunknown&gt;');
    });

    it('converts good tags but escapes bad tags', () => {
        const text = '<i>known</i> and <unknown>tag</unknown>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('<i>known</i> and &lt;unknown>tag&lt;/unknown>');
        expect(escaped).toParseCorrectlyAndMatch('&lt;i&gt;known&lt;/i&gt; and &lt;unknown&gt;tag&lt;/unknown&gt;');
    });

    it('completely escapes mismatched tags where the mismatch is at the beginning', () => {
        const text = '<b>in<i>com</i>plete';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;b&gt;in&lt;i&gt;com&lt;/i&gt;plete');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;in&lt;i&gt;com&lt;/i&gt;plete');
    });

    it('completely escapes mismatched tags where the mismatch is at the end', () => {
        const text = 'in<i>com</i>plete</b>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('in&lt;i&gt;com&lt;/i&gt;plete&lt;/b&gt;');
        expect(escaped).toParseCorrectlyAndMatch('in&lt;i&gt;com&lt;/i&gt;plete&lt;/b&gt;');
    });

    it('escapes all tags where there are attributes', () => {
        const text = '<b>good</b> and <b style=\'bad\'>bad</b>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;b&gt;good&lt;/b&gt; and &lt;b style=&apos;bad&apos;&gt;bad&lt;/b&gt;');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;good&lt;/b&gt; and &lt;b style=&apos;bad&apos;&gt;bad&lt;/b&gt;');
    });
    it('escapes all tags where syntax is invalid', () => {
        const text = '<b>unrecognized</b stuff>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;b&gt;unrecognized&lt;/b stuff&gt;');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;unrecognized&lt;/b stuff&gt;');
    });

    it('escapes completely mismatched tags', () => {
        const text = '<b>mismatched</i>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;b&gt;mismatched&lt;/i&gt;');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;mismatched&lt;/i&gt;');
    });

    it('escapes mismatched tags where the first character is mismatched', () => {
        const text = '<b>mismatched/unknown</bunknown>';
        const {converted, escaped} = convertAndEscape(text);
        expect(converted).toParseCorrectlyAndMatch('&lt;b&gt;mismatched/unknown&lt;/bunknown&gt;');
        expect(escaped).toParseCorrectlyAndMatch('&lt;b&gt;mismatched/unknown&lt;/bunknown&gt;');
    });
});
