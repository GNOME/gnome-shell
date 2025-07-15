// Test cases for MessageTray URLification

import 'resource:///org/gnome/shell/ui/environment.js';
import {findUrls} from 'resource:///org/gnome/shell/misc/util.js';

describe('findUrls()', () => {
    const urlParameters = [
        {
            input: 'This is a test',
            output: [],
        },
        {
            input: 'This is http://www.gnome.org a test',
            output: [{url: 'http://www.gnome.org', pos: 8}],
        },
        {
            input: 'This is http://www.gnome.org',
            output: [{url: 'http://www.gnome.org', pos: 8}],
        },
        {
            input: 'http://www.gnome.org a test',
            output: [{url: 'http://www.gnome.org', pos: 0}],
        },
        {
            input: 'http://www.gnome.org',
            output: [{url: 'http://www.gnome.org', pos: 0}],
        },
        {
            input: 'Go to http://www.gnome.org.',
            output: [{url: 'http://www.gnome.org', pos: 6}],
        },
        {
            input: 'Go to http://www.gnome.org/.',
            output: [{url: 'http://www.gnome.org/', pos: 6}],
        },
        {
            input: '(Go to http://www.gnome.org!)',
            output: [{url: 'http://www.gnome.org', pos: 7}],
        },
        {
            input: 'Use GNOME (http://www.gnome.org).',
            output: [{url: 'http://www.gnome.org', pos: 11}],
        },
        {
            input: 'This is a http://www.gnome.org/path test.',
            output: [{url: 'http://www.gnome.org/path', pos: 10}],
        },
        {
            input: 'This is a www.gnome.org scheme-less test.',
            output: [{url: 'www.gnome.org', pos: 10}],
        },
        {
            input: 'This is a www.gnome.org/scheme-less test.',
            output: [{url: 'www.gnome.org/scheme-less', pos: 10}],
        },
        {
            input: 'This is a gnome.org/scheme-less test without www.',
            output: [{url: 'gnome.org/scheme-less', pos: 10}],
        },
        {
            input: 'This is a status.gnome.org/scheme-less test.',
            output: [{url: 'status.gnome.org/scheme-less', pos: 10}],
        },
        {
            input: 'This is a http://www.gnome.org:99/port test.',
            output: [{url: 'http://www.gnome.org:99/port', pos: 10}],
        },
        {
            input: 'This is an ftp://www.gnome.org/ test.',
            output: [{url: 'ftp://www.gnome.org/', pos: 11}],
        },
        {
            input: 'https://www.gnome.org/(some_url,_with_very_unusual_characters)',
            output: [{url: 'https://www.gnome.org/(some_url,_with_very_unusual_characters)', pos: 0}],
        },
        {
            input: 'https://www.gnome.org/(some_url_with_unbalanced_parenthesis',
            output: [{url: 'https://www.gnome.org/', pos: 0}],
        },
        {
            input: 'https://www.gnome.org/‎ plus trailing junk',
            output: [{url: 'https://www.gnome.org/', pos: 0}],
        },

        {
            input: 'Visit http://www.gnome.org/ and http://developer.gnome.org',
            output: [{url: 'http://www.gnome.org/', pos: 6},
                {url: 'http://developer.gnome.org', pos: 32}],
        },

        {
            input: 'This is not.a.domain test.',
            output: [],
        },
        {
            input: 'This is not:a.url test.',
            output: [],
        },
        {
            input: 'This is not:/a.url/ test.',
            output: [],
        },
        {
            input: 'This is not:/a.url/ test.',
            output: [],
        },
        {
            input: 'This is not@a.url/ test.',
            output: [],
        },
        {
            input: 'This is surely@not.a/url test.',
            output: [],
        },
        {
            input: 'This is not..aa/url test.',
            output: [],
        },
        {
            input: 'This is ..not/a-url test.',
            output: [],
        },
        {
            input: 'This is .absolutely.not/a-url test.',
            output: [],
        },
    ];

    for (const param of urlParameters) {
        const {input, output} = param;
        const findsOrDoesNotFind = output.length > 0
            ? 'finds'
            : 'does not find';

        it(`${findsOrDoesNotFind} URLs in "${input}"`, () => {
            expect(findUrls(input)).toEqual(output);
        });
    }
});
