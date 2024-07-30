// Test cases for version comparison

import 'resource:///org/gnome/shell/ui/environment.js';
import {GNOMEversionCompare} from 'resource:///org/gnome/shell/misc/util.js';

describe('GNOMEversionCompare()', () => {
    it('compares matching versions', () => {
        expect(GNOMEversionCompare('40', '40')).toBe(0);
    });

    it('compares older versions', () => {
        expect(GNOMEversionCompare('40', '42')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.0', '40.1')).toBeLessThan(0);
    });

    it('compares newer versions', () => {
        expect(GNOMEversionCompare('42', '40')).toBeGreaterThan(0);
        expect(GNOMEversionCompare('40.1', '40.0')).toBeGreaterThan(0);
    });

    it('compares legacy versions', () => {
        expect(GNOMEversionCompare('3.38.0', '40')).toBeLessThan(0);
        expect(GNOMEversionCompare('40', '3.38.0')).toBeGreaterThan(0);
    });

    it('compares pre-release versions', () => {
        expect(GNOMEversionCompare('40.beta', '40')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.alpha', '40')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.rc', '40')).toBeLessThan(0);

        expect(GNOMEversionCompare('40', '40.beta')).toBeGreaterThan(0);
        expect(GNOMEversionCompare('40', '40.alpha')).toBeGreaterThan(0);
        expect(GNOMEversionCompare('40', '40.rc')).toBeGreaterThan(0);

        expect(GNOMEversionCompare('40.alpha', '40.beta')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.beta', '40.rc')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.rc', '40.0')).toBeLessThan(0);
    });

    it('compares point-release versions', () => {
        expect(GNOMEversionCompare('40.1', '40')).toBeGreaterThan(0);
        expect(GNOMEversionCompare('40.alpha.1', '40.alpha')).toBeGreaterThan(0);
        expect(GNOMEversionCompare('40.beta.1.1', '40.beta.1')).toBeGreaterThan(0);

        expect(GNOMEversionCompare('40', '40.1')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.alpha', '40.alpha.1')).toBeLessThan(0);
        expect(GNOMEversionCompare('40.beta.1', '40.beta.1.1')).toBeLessThan(0);
    });

    it('compares empty versions', () => {
        expect(GNOMEversionCompare('', '40')).toBeLessThan(0);
    });
});
