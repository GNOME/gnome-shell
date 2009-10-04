/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
 * This function is intended to extend the String object and provide
 * an String.format API for string formatting.
 * It has to be set up using String.prototype.format = Format.format;
 * Usage:
 * "somestring %s %d".format('hello', 5);
 * It supports %s, %d and %f, for %f it also support precisions like
 * "%.2f".format(1.526)
 */

function format() {
    let str = this;
    let i = 0;
    let args = arguments;

    return str.replace(/%(?:\.([0-9]+))?(.)/g, function (str, precisionGroup, genericGroup) {

                    if (precisionGroup != '' && genericGroup != 'f')
                        throw new Error("Precision can only be specified for 'f'");

                    switch (genericGroup) {
                        case '%':
                            return '%';
                            break;
                        case 's':
                            return args[i++].toString();
                            break;
                        case 'd':
                            return parseInt(args[i++]);
                            break;
                        case 'f':
                            if (precisionGroup == '')
                                return parseFloat(args[i++]);
                            else
                                return parseFloat(args[i++]).toFixed(parseInt(precisionGroup));
                            break;
                        default:
                            throw new Error('Unsupported conversion character %' + genericGroup);
                    }

                });
}
