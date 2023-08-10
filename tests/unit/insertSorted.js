/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

// Test cases for Util.insertSorted

// Needed so that Util can bring some UI stuff
// we don't actually use
import 'resource:///org/gnome/shell/ui/environment.js';

import * as Assertions from '../common/assertions.js';

import * as Util from 'resource:///org/gnome/shell/misc/util.js';

let arrayInt = [1, 2, 3, 5, 6];
Util.insertSorted(arrayInt, 4, (one, two) => one - two);

Assertions.assertArrayEquals('first test', [1, 2, 3, 4, 5, 6], arrayInt);

// no comparator, integer sorting is implied
Util.insertSorted(arrayInt, 3);

Assertions.assertArrayEquals('second test', [1, 2, 3, 3, 4, 5, 6], arrayInt);

let obj1 = {a: 1};
let obj2 = {a: 2, b: 0};
let obj3 = {a: 2, b: 1};
let obj4 = {a: 3};

let arrayObj = [obj1, obj3, obj4];

// obj2 compares equivalent to obj3, should be
// inserted before
Util.insertSorted(arrayObj, obj2, (one, two) => one.a - two.a);

Assertions.assertArrayEquals('object test', [obj1, obj2, obj3, obj4], arrayObj);

const checkedCmp = (one, two) => {
    if (typeof one != 'number' || typeof two != 'number')
        throw new TypeError('Invalid type passed to checkedCmp');

    return one - two;
};

let arrayEmpty = [];

// check that no comparisons are made when
// inserting in a empty array
Util.insertSorted(arrayEmpty, 3, checkedCmp);

// Insert at the end and check that we don't
// access past it
Util.insertSorted(arrayEmpty, 4, checkedCmp);
Util.insertSorted(arrayEmpty, 5, checkedCmp);

// Some more insertions
Util.insertSorted(arrayEmpty, 2, checkedCmp);
Util.insertSorted(arrayEmpty, 1, checkedCmp);

Assertions.assertArrayEquals('checkedCmp test', [1, 2, 3, 4, 5], arrayEmpty);
