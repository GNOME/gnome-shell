const JsUnit = imports.jsUnit;

/**
 * Asserts if two arrays have the same length and each element passes assertEquals
 *
 * @template T
 * @param {string} errorMessage an error message if the arrays are not equal
 * @param {T[]} array1 the first array
 * @param {T[]} array2 the second array
 */
export function assertArrayEquals(errorMessage, array1, array2) {
    JsUnit.assertEquals(`${errorMessage} length`, array1.length, array2.length);
    for (let j = 0; j < array1.length; j++)
        JsUnit.assertEquals(`${errorMessage} item ${j}`, array1[j], array2[j]);
}
