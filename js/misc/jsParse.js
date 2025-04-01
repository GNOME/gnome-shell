const AsyncFunction = async function () {}.constructor;

/**
 * Returns a list of potential completions for text. Completions either
 * follow a dot (e.g. foo.ba -> bar) or they are picked from globalCompletionList (e.g. fo -> foo)
 * commandHeader is prefixed on any expression before it is eval'ed.  It will most likely
 * consist of global constants that might not carry over from the calling environment.
 *
 * This function is likely the one you want to call from external modules
 *
 * @param {string} text
 * @param {string} commandHeader
 * @param {readonly string[]} [globalCompletionList]
 */
export async function getCompletions(text, commandHeader, globalCompletionList) {
    let methods = [];
    let expr_, base;
    let attrHead = '';
    if (globalCompletionList == null)
        globalCompletionList = [];

    let offset = getExpressionOffset(text, text.length - 1);
    if (offset >= 0) {
        text = text.slice(offset);

        // Look for expressions like "Main.panel.foo" and match Main.panel and foo
        let matches = text.match(/(.*)\.(.*)/);
        if (matches) {
            [expr_, base, attrHead] = matches;

            methods = (await getPropertyNamesFromExpression(base, commandHeader)).filter(
                attr => attr.slice(0, attrHead.length) === attrHead);
        }

        // Look for the empty expression or partially entered words
        // not proceeded by a dot and match them against global constants
        matches = text.match(/^(\w*)$/);
        if (text === '' || matches) {
            [expr_, attrHead] = matches;
            methods = globalCompletionList.filter(
                attr => attr.slice(0, attrHead.length) === attrHead);
        }
    }

    return [methods, attrHead];
}


/**
 * A few functions for parsing strings of javascript code.
 */

/**
 * Identify characters that delimit an expression.  That is,
 * if we encounter anything that isn't a letter, '.', ')', or ']',
 * we should stop parsing.
 *
 * @param {string} c
 */
function isStopChar(c) {
    return !c.match(/[\w.)\]]/);
}

/**
 * Given the ending position of a quoted string, find where it starts
 *
 * @param {string} expr
 * @param {number} offset
 */
export function findMatchingQuote(expr, offset) {
    let quoteChar = expr.charAt(offset);
    for (let i = offset - 1; i >= 0; --i) {
        if (expr.charAt(i) === quoteChar && expr.charAt(i - 1) !== '\\')
            return i;
    }
    return -1;
}

/**
 * Given the ending position of a regex, find where it starts
 *
 * @param {string} expr
 * @param {number} offset
 */
export function findMatchingSlash(expr, offset) {
    for (let i = offset - 1; i >= 0; --i) {
        if (expr.charAt(i) === '/' && expr.charAt(i - 1) !== '\\')
            return i;
    }
    return -1;
}

/**
 * If expr.charAt(offset) is ')' or ']',
 * return the position of the corresponding '(' or '[' bracket.
 * This function does not check for syntactic correctness.  e.g.,
 * findMatchingBrace("[(])", 3) returns 1.
 *
 * @param {string} expr
 * @param {number} offset
 */
export function findMatchingBrace(expr, offset) {
    let closeBrace = expr.charAt(offset);
    let openBrace = {')': '(', ']': '['}[closeBrace];

    return findTheBrace(expr, offset - 1, openBrace, closeBrace);
}

/**
 * @param {*} expr
 * @param {*} offset
 * @param  {...any} braces
 * @returns {number}
 */
export function findTheBrace(expr, offset, ...braces) {
    let [openBrace, closeBrace] = braces;

    if (offset < 0)
        return -1;

    if (expr.charAt(offset) === openBrace)
        return offset;

    if (expr.charAt(offset).match(/['"]/))
        return findTheBrace(expr, findMatchingQuote(expr, offset) - 1, ...braces);

    if (expr.charAt(offset) === '/')
        return findTheBrace(expr, findMatchingSlash(expr, offset) - 1, ...braces);

    if (expr.charAt(offset) === closeBrace)
        return findTheBrace(expr, findTheBrace(expr, offset - 1, ...braces) - 1, ...braces);

    return findTheBrace(expr, offset - 1, ...braces);
}

/**
 * Walk expr backwards from offset looking for the beginning of an
 * expression suitable for passing to eval.
 * There is no guarantee of correct javascript syntax between the return
 * value and offset.  This function is meant to take a string like
 * "foo(Obj.We.Are.Completing" and allow you to extract "Obj.We.Are.Completing"
 *
 * @param {string} expr
 * @param {number} offset
 */
export function getExpressionOffset(expr, offset) {
    while (offset >= 0) {
        let currChar = expr.charAt(offset);

        if (isStopChar(currChar))
            return offset + 1;

        if (currChar.match(/[)\]]/))
            offset = findMatchingBrace(expr, offset);

        --offset;
    }

    return offset + 1;
}

/**
 * Things with non-word characters or that start with a number
 * are not accessible via .foo notation and so aren't returned
 *
 * @param {string} w
 */
function isValidPropertyName(w) {
    return !(w.match(/\W/) || w.match(/^\d/));
}

/**
 * To get all properties (enumerable and not), we need to walk
 * the prototype chain ourselves
 *
 * @param {object} obj
 */
export function getAllProps(obj) {
    if (obj === null || obj === undefined)
        return [];

    return Object.getOwnPropertyNames(obj).concat(getAllProps(Object.getPrototypeOf(obj)));
}

/**
 * Given a string _expr_, returns all methods
 * that can be accessed via '.' notation.
 * e.g., expr="({ foo: null, bar: null, 4: null })" will
 * return ["foo", "bar", ...] but the list will not include "4",
 * since methods accessed with '.' notation must star with a letter or _.
 *
 * @param {string} expr
 * @param {string=} commandHeader
 */
export async function getPropertyNamesFromExpression(expr, commandHeader = '') {
    let obj = {};
    if (!isUnsafeExpression(expr)) {
        try {
            const lines = expr.split('\n');
            lines.push(`return ${lines.pop()}`);
            obj = await AsyncFunction(commandHeader + lines.join(';'))();
        } catch {
            return [];
        }
    } else {
        return [];
    }

    let propsUnique = {};
    if (typeof obj === 'object') {
        let allProps = getAllProps(obj);
        // Get only things we are allowed to complete following a '.'
        allProps = allProps.filter(isValidPropertyName);

        // Make sure propsUnique contains one key for every
        // property so we end up with a unique list of properties
        allProps.map(p => (propsUnique[p] = null));
    }
    return Object.keys(propsUnique).sort();
}

/**
 * Given a list of words, returns the longest prefix they all have in common
 *
 * @param {readonly string[]} words
 */
export function getCommonPrefix(words) {
    let word = words[0];
    for (let i = 0; i < word.length; i++) {
        for (let w = 1; w < words.length; w++) {
            if (words[w].charAt(i) !== word.charAt(i))
                return word.slice(0, i);
        }
    }
    return word;
}

/**
 * Remove any blocks that are quoted or are in a regex
 *
 * @param {string} str
 */
export function removeLiterals(str) {
    if (str.length === 0)
        return '';

    let currChar = str.charAt(str.length - 1);
    if (currChar === '"' || currChar === '\'') {
        return removeLiterals(
            str.slice(0, findMatchingQuote(str, str.length - 1)));
    } else if (currChar === '/') {
        return removeLiterals(
            str.slice(0, findMatchingSlash(str, str.length - 1)));
    }

    return removeLiterals(str.slice(0, str.length - 1)) + currChar;
}

/**
 * Returns true if there is reason to think that eval(str)
 * will modify the global scope
 *
 * @param {string} str
 */
export function isUnsafeExpression(str) {
    // Check for any sort of assignment
    // The strategy used is dumb: remove any quotes
    // or regexs and comparison operators and see if there is an '=' character.
    // If there is, it might be an unsafe assignment.

    let prunedStr = removeLiterals(str);
    prunedStr = prunedStr.replace(/[=!]==/g, '');    // replace === and !== with nothing
    prunedStr = prunedStr.replace(/[=<>!]=/g, '');    // replace ==, <=, >=, != with nothing

    if (prunedStr.match(/[=]/)) {
        return true;
    } else if (prunedStr.match(/;/)) {
        // If we contain a semicolon not inside of a quote/regex, assume we're unsafe as well
        return true;
    }

    return false;
}

/**
 * Returns a list of global keywords derived from str
 *
 * @param {string} str
 */
export function getDeclaredConstants(str) {
    let ret = [];
    str.split(';').forEach(s => {
        let base_, keyword;
        let match = s.match(/const\s+(\w+)\s*=/);
        if (match) {
            [base_, keyword] = match;
            ret.push(keyword);
        }
    });

    return ret;
}
