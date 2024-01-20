import * as Params from 'resource:///org/gnome/shell/misc/params.js';

describe('Params.parse()', () => {
    const defaults = {
        foo: 'This is a test',
        bar: null,
        baz: 42,
    };

    it('applies default values', () => {
        expect(Params.parse(null, defaults)).toEqual(defaults);
    });

    it('applies provided params', () => {
        expect(Params.parse({bar: 23}, defaults))
            .toEqual({foo: 'This is a test', bar: 23, baz: 42});
    });

    it('does not allow extra args by default', () => {
        expect(() => Params.parse({extraArg: 'quz'}, defaults)).toThrow();
    });

    it('does allow extra args when requested', () => {
        expect(Params.parse({extraArg: 'quz'}, defaults, true))
            .toEqual({foo: 'This is a test', bar: null, baz: 42, extraArg: 'quz'});
    });
});
