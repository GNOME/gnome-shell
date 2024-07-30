// Test cases for Util.insertSorted

// Needed so that Util can bring some UI stuff
// we don't actually use
import 'resource:///org/gnome/shell/ui/environment.js';

import {insertSorted} from 'resource:///org/gnome/shell/misc/util.js';

describe('insertSorted()', () => {
    it('uses integer sorting by default', () => {
        const arrayInt = [1, 2, 3, 5, 6];
        insertSorted(arrayInt, 4);
        expect(arrayInt).toEqual([1, 2, 3, 4, 5, 6]);
    });

    it('inserts elements with a custom compare function', () => {
        const arrayInt = [6, 5, 3, 2, 1];
        insertSorted(arrayInt, 4, (one, two) => two - one);
        expect(arrayInt).toEqual([6, 5, 4, 3, 2, 1]);
    });

    it('inserts before first equal match', () => {
        const obj1 = {a: 1};
        const obj2 = {a: 2, b: 0};
        const obj3 = {a: 2, b: 1};
        const obj4 = {a: 3};

        const arrayObj = [obj1, obj3, obj4];
        insertSorted(arrayObj, obj2, (one, two) => one.a - two.a);
        expect(arrayObj).toEqual([obj1, obj2, obj3, obj4]);
    });

    it('does not call compare func when array was empty', () => {
        const cmp = jasmine.createSpy();
        const empty = [];
        insertSorted(empty, 3, cmp);
        expect(cmp).not.toHaveBeenCalled();
    });

    const checkedCmp = (one, two) => {
        if (typeof one !== 'number' || typeof two !== 'number')
            throw new TypeError('Invalid type passed to checkedCmp');

        return one - two;
    };

    it('does not access past bounds when inserting at end', () => {
        const array = [3];
        expect(() => {
            insertSorted(array, 4, checkedCmp);
            insertSorted(array, 5, checkedCmp);
        }).not.toThrow();
    });

    it('does not access past bounds when inserting at beginning', () => {
        const array = [4, 5];
        expect(() => {
            insertSorted(array, 1, checkedCmp);
            insertSorted(array, 2, checkedCmp);
        }).not.toThrow();
    });
});
