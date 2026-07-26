var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

var indexMinimal = {};

var minimal$1 = {};

var aspromise;
var hasRequiredAspromise;

function requireAspromise () {
	if (hasRequiredAspromise) return aspromise;
	hasRequiredAspromise = 1;
	aspromise = asPromise;

	/**
	 * Callback as used by {@link util.asPromise}.
	 * @typedef asPromiseCallback
	 * @type {function}
	 * @param {Error|null} error Error, if any
	 * @param {...*} params Additional arguments
	 * @returns {undefined}
	 */

	/**
	 * Returns a promise from a node-style callback function.
	 * @memberof util
	 * @param {asPromiseCallback} fn Function to call
	 * @param {*} ctx Function context
	 * @param {...*} params Function arguments
	 * @returns {Promise<*>} Promisified function
	 */
	function asPromise(fn, ctx/*, varargs */) {
	    var params  = new Array(arguments.length - 1),
	        offset  = 0,
	        index   = 2,
	        pending = true;
	    while (index < arguments.length)
	        params[offset++] = arguments[index++];
	    return new Promise(function executor(resolve, reject) {
	        params[offset] = function callback(err/*, varargs */) {
	            if (pending) {
	                pending = false;
	                if (err)
	                    reject(err);
	                else {
	                    var params = new Array(arguments.length - 1),
	                        offset = 0;
	                    while (offset < params.length)
	                        params[offset++] = arguments[offset];
	                    resolve.apply(null, params);
	                }
	            }
	        };
	        try {
	            fn.apply(ctx || null, params);
	        } catch (err) {
	            if (pending) {
	                pending = false;
	                reject(err);
	            }
	        }
	    });
	}
	return aspromise;
}

var base64$1 = {};

var hasRequiredBase64;

function requireBase64 () {
	if (hasRequiredBase64) return base64$1;
	hasRequiredBase64 = 1;
	(function (exports) {

		/**
		 * A minimal base64 implementation for number arrays.
		 * @memberof util
		 * @namespace
		 */
		var base64 = exports;

		/**
		 * Calculates the byte length of a base64 encoded string.
		 * @param {string} string Base64 encoded string
		 * @returns {number} Byte length
		 */
		base64.length = function length(string) {
		    var p = string.length;
		    if (!p)
		        return 0;
		    var n = 0;
		    while (--p % 4 > 1 && string.charAt(p) === "=")
		        ++n;
		    return Math.ceil(string.length * 3) / 4 - n;
		};

		// Base64 encoding table
		var b64 = new Array(64);

		// Base64 decoding table
		var s64 = new Array(123);

		// 65..90, 97..122, 48..57, 43, 47
		for (var i = 0; i < 64;)
		    s64[b64[i] = i < 26 ? i + 65 : i < 52 ? i + 71 : i < 62 ? i - 4 : i - 59 | 43] = i++;

		/**
		 * Encodes a buffer to a base64 encoded string.
		 * @param {Uint8Array} buffer Source buffer
		 * @param {number} start Source start
		 * @param {number} end Source end
		 * @returns {string} Base64 encoded string
		 */
		base64.encode = function encode(buffer, start, end) {
		    var parts = null,
		        chunk = [];
		    var i = 0, // output index
		        j = 0, // goto index
		        t;     // temporary
		    while (start < end) {
		        var b = buffer[start++];
		        switch (j) {
		            case 0:
		                chunk[i++] = b64[b >> 2];
		                t = (b & 3) << 4;
		                j = 1;
		                break;
		            case 1:
		                chunk[i++] = b64[t | b >> 4];
		                t = (b & 15) << 2;
		                j = 2;
		                break;
		            case 2:
		                chunk[i++] = b64[t | b >> 6];
		                chunk[i++] = b64[b & 63];
		                j = 0;
		                break;
		        }
		        if (i > 8191) {
		            (parts || (parts = [])).push(String.fromCharCode.apply(String, chunk));
		            i = 0;
		        }
		    }
		    if (j) {
		        chunk[i++] = b64[t];
		        chunk[i++] = 61;
		        if (j === 1)
		            chunk[i++] = 61;
		    }
		    if (parts) {
		        if (i)
		            parts.push(String.fromCharCode.apply(String, chunk.slice(0, i)));
		        return parts.join("");
		    }
		    return String.fromCharCode.apply(String, chunk.slice(0, i));
		};

		var invalidEncoding = "invalid encoding";

		/**
		 * Decodes a base64 encoded string to a buffer.
		 * @param {string} string Source string
		 * @param {Uint8Array} buffer Destination buffer
		 * @param {number} offset Destination offset
		 * @returns {number} Number of bytes written
		 * @throws {Error} If encoding is invalid
		 */
		base64.decode = function decode(string, buffer, offset) {
		    var start = offset;
		    var j = 0, // goto index
		        t;     // temporary
		    for (var i = 0; i < string.length;) {
		        var c = string.charCodeAt(i++);
		        if (c === 61 && j > 1)
		            break;
		        if ((c = s64[c]) === undefined)
		            throw Error(invalidEncoding);
		        switch (j) {
		            case 0:
		                t = c;
		                j = 1;
		                break;
		            case 1:
		                buffer[offset++] = t << 2 | (c & 48) >> 4;
		                t = c;
		                j = 2;
		                break;
		            case 2:
		                buffer[offset++] = (t & 15) << 4 | (c & 60) >> 2;
		                t = c;
		                j = 3;
		                break;
		            case 3:
		                buffer[offset++] = (t & 3) << 6 | c;
		                j = 0;
		                break;
		        }
		    }
		    if (j === 1)
		        throw Error(invalidEncoding);
		    return offset - start;
		};

		/**
		 * Tests if the specified string appears to be base64 encoded.
		 * @param {string} string String to test
		 * @returns {boolean} `true` if probably base64 encoded, otherwise false
		 */
		base64.test = function test(string) {
		    return /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(string);
		}; 
	} (base64$1));
	return base64$1;
}

var eventemitter;
var hasRequiredEventemitter;

function requireEventemitter () {
	if (hasRequiredEventemitter) return eventemitter;
	hasRequiredEventemitter = 1;
	eventemitter = EventEmitter;

	/**
	 * Constructs a new event emitter instance.
	 * @classdesc A minimal event emitter.
	 * @memberof util
	 * @constructor
	 */
	function EventEmitter() {

	    /**
	     * Registered listeners.
	     * @type {Object.<string,*>}
	     * @private
	     */
	    this._listeners = {};
	}

	/**
	 * Registers an event listener.
	 * @param {string} evt Event name
	 * @param {function} fn Listener
	 * @param {*} [ctx] Listener context
	 * @returns {util.EventEmitter} `this`
	 */
	EventEmitter.prototype.on = function on(evt, fn, ctx) {
	    (this._listeners[evt] || (this._listeners[evt] = [])).push({
	        fn  : fn,
	        ctx : ctx || this
	    });
	    return this;
	};

	/**
	 * Removes an event listener or any matching listeners if arguments are omitted.
	 * @param {string} [evt] Event name. Removes all listeners if omitted.
	 * @param {function} [fn] Listener to remove. Removes all listeners of `evt` if omitted.
	 * @returns {util.EventEmitter} `this`
	 */
	EventEmitter.prototype.off = function off(evt, fn) {
	    if (evt === undefined)
	        this._listeners = {};
	    else {
	        if (fn === undefined)
	            this._listeners[evt] = [];
	        else {
	            var listeners = this._listeners[evt];
	            for (var i = 0; i < listeners.length;)
	                if (listeners[i].fn === fn)
	                    listeners.splice(i, 1);
	                else
	                    ++i;
	        }
	    }
	    return this;
	};

	/**
	 * Emits an event by calling its listeners with the specified arguments.
	 * @param {string} evt Event name
	 * @param {...*} args Arguments
	 * @returns {util.EventEmitter} `this`
	 */
	EventEmitter.prototype.emit = function emit(evt) {
	    var listeners = this._listeners[evt];
	    if (listeners) {
	        var args = [],
	            i = 1;
	        for (; i < arguments.length;)
	            args.push(arguments[i++]);
	        for (i = 0; i < listeners.length;)
	            listeners[i].fn.apply(listeners[i++].ctx, args);
	    }
	    return this;
	};
	return eventemitter;
}

var float;
var hasRequiredFloat;

function requireFloat () {
	if (hasRequiredFloat) return float;
	hasRequiredFloat = 1;

	float = factory(factory);

	/**
	 * Reads / writes floats / doubles from / to buffers.
	 * @name util.float
	 * @namespace
	 */

	/**
	 * Writes a 32 bit float to a buffer using little endian byte order.
	 * @name util.float.writeFloatLE
	 * @function
	 * @param {number} val Value to write
	 * @param {Uint8Array} buf Target buffer
	 * @param {number} pos Target buffer offset
	 * @returns {undefined}
	 */

	/**
	 * Writes a 32 bit float to a buffer using big endian byte order.
	 * @name util.float.writeFloatBE
	 * @function
	 * @param {number} val Value to write
	 * @param {Uint8Array} buf Target buffer
	 * @param {number} pos Target buffer offset
	 * @returns {undefined}
	 */

	/**
	 * Reads a 32 bit float from a buffer using little endian byte order.
	 * @name util.float.readFloatLE
	 * @function
	 * @param {Uint8Array} buf Source buffer
	 * @param {number} pos Source buffer offset
	 * @returns {number} Value read
	 */

	/**
	 * Reads a 32 bit float from a buffer using big endian byte order.
	 * @name util.float.readFloatBE
	 * @function
	 * @param {Uint8Array} buf Source buffer
	 * @param {number} pos Source buffer offset
	 * @returns {number} Value read
	 */

	/**
	 * Writes a 64 bit double to a buffer using little endian byte order.
	 * @name util.float.writeDoubleLE
	 * @function
	 * @param {number} val Value to write
	 * @param {Uint8Array} buf Target buffer
	 * @param {number} pos Target buffer offset
	 * @returns {undefined}
	 */

	/**
	 * Writes a 64 bit double to a buffer using big endian byte order.
	 * @name util.float.writeDoubleBE
	 * @function
	 * @param {number} val Value to write
	 * @param {Uint8Array} buf Target buffer
	 * @param {number} pos Target buffer offset
	 * @returns {undefined}
	 */

	/**
	 * Reads a 64 bit double from a buffer using little endian byte order.
	 * @name util.float.readDoubleLE
	 * @function
	 * @param {Uint8Array} buf Source buffer
	 * @param {number} pos Source buffer offset
	 * @returns {number} Value read
	 */

	/**
	 * Reads a 64 bit double from a buffer using big endian byte order.
	 * @name util.float.readDoubleBE
	 * @function
	 * @param {Uint8Array} buf Source buffer
	 * @param {number} pos Source buffer offset
	 * @returns {number} Value read
	 */

	// Factory function for the purpose of node-based testing in modified global environments
	function factory(exports) {

	    // float: typed array
	    if (typeof Float32Array !== "undefined") (function() {

	        var f32 = new Float32Array([ -0 ]),
	            f8b = new Uint8Array(f32.buffer),
	            le  = f8b[3] === 128;

	        function writeFloat_f32_cpy(val, buf, pos) {
	            f32[0] = val;
	            buf[pos    ] = f8b[0];
	            buf[pos + 1] = f8b[1];
	            buf[pos + 2] = f8b[2];
	            buf[pos + 3] = f8b[3];
	        }

	        function writeFloat_f32_rev(val, buf, pos) {
	            f32[0] = val;
	            buf[pos    ] = f8b[3];
	            buf[pos + 1] = f8b[2];
	            buf[pos + 2] = f8b[1];
	            buf[pos + 3] = f8b[0];
	        }

	        /* istanbul ignore next */
	        exports.writeFloatLE = le ? writeFloat_f32_cpy : writeFloat_f32_rev;
	        /* istanbul ignore next */
	        exports.writeFloatBE = le ? writeFloat_f32_rev : writeFloat_f32_cpy;

	        function readFloat_f32_cpy(buf, pos) {
	            f8b[0] = buf[pos    ];
	            f8b[1] = buf[pos + 1];
	            f8b[2] = buf[pos + 2];
	            f8b[3] = buf[pos + 3];
	            return f32[0];
	        }

	        function readFloat_f32_rev(buf, pos) {
	            f8b[3] = buf[pos    ];
	            f8b[2] = buf[pos + 1];
	            f8b[1] = buf[pos + 2];
	            f8b[0] = buf[pos + 3];
	            return f32[0];
	        }

	        /* istanbul ignore next */
	        exports.readFloatLE = le ? readFloat_f32_cpy : readFloat_f32_rev;
	        /* istanbul ignore next */
	        exports.readFloatBE = le ? readFloat_f32_rev : readFloat_f32_cpy;

	    // float: ieee754
	    })(); else (function() {

	        function writeFloat_ieee754(writeUint, val, buf, pos) {
	            var sign = val < 0 ? 1 : 0;
	            if (sign)
	                val = -val;
	            if (val === 0)
	                writeUint(1 / val > 0 ? /* positive */ 0 : /* negative 0 */ 2147483648, buf, pos);
	            else if (isNaN(val))
	                writeUint(2143289344, buf, pos);
	            else if (val > 3.4028234663852886e+38) // +-Infinity
	                writeUint((sign << 31 | 2139095040) >>> 0, buf, pos);
	            else if (val < 1.1754943508222875e-38) // denormal
	                writeUint((sign << 31 | Math.round(val / 1.401298464324817e-45)) >>> 0, buf, pos);
	            else {
	                var exponent = Math.floor(Math.log(val) / Math.LN2),
	                    mantissa = Math.round(val * Math.pow(2, -exponent) * 8388608) & 8388607;
	                writeUint((sign << 31 | exponent + 127 << 23 | mantissa) >>> 0, buf, pos);
	            }
	        }

	        exports.writeFloatLE = writeFloat_ieee754.bind(null, writeUintLE);
	        exports.writeFloatBE = writeFloat_ieee754.bind(null, writeUintBE);

	        function readFloat_ieee754(readUint, buf, pos) {
	            var uint = readUint(buf, pos),
	                sign = (uint >> 31) * 2 + 1,
	                exponent = uint >>> 23 & 255,
	                mantissa = uint & 8388607;
	            return exponent === 255
	                ? mantissa
	                ? NaN
	                : sign * Infinity
	                : exponent === 0 // denormal
	                ? sign * 1.401298464324817e-45 * mantissa
	                : sign * Math.pow(2, exponent - 150) * (mantissa + 8388608);
	        }

	        exports.readFloatLE = readFloat_ieee754.bind(null, readUintLE);
	        exports.readFloatBE = readFloat_ieee754.bind(null, readUintBE);

	    })();

	    // double: typed array
	    if (typeof Float64Array !== "undefined") (function() {

	        var f64 = new Float64Array([-0]),
	            f8b = new Uint8Array(f64.buffer),
	            le  = f8b[7] === 128;

	        function writeDouble_f64_cpy(val, buf, pos) {
	            f64[0] = val;
	            buf[pos    ] = f8b[0];
	            buf[pos + 1] = f8b[1];
	            buf[pos + 2] = f8b[2];
	            buf[pos + 3] = f8b[3];
	            buf[pos + 4] = f8b[4];
	            buf[pos + 5] = f8b[5];
	            buf[pos + 6] = f8b[6];
	            buf[pos + 7] = f8b[7];
	        }

	        function writeDouble_f64_rev(val, buf, pos) {
	            f64[0] = val;
	            buf[pos    ] = f8b[7];
	            buf[pos + 1] = f8b[6];
	            buf[pos + 2] = f8b[5];
	            buf[pos + 3] = f8b[4];
	            buf[pos + 4] = f8b[3];
	            buf[pos + 5] = f8b[2];
	            buf[pos + 6] = f8b[1];
	            buf[pos + 7] = f8b[0];
	        }

	        /* istanbul ignore next */
	        exports.writeDoubleLE = le ? writeDouble_f64_cpy : writeDouble_f64_rev;
	        /* istanbul ignore next */
	        exports.writeDoubleBE = le ? writeDouble_f64_rev : writeDouble_f64_cpy;

	        function readDouble_f64_cpy(buf, pos) {
	            f8b[0] = buf[pos    ];
	            f8b[1] = buf[pos + 1];
	            f8b[2] = buf[pos + 2];
	            f8b[3] = buf[pos + 3];
	            f8b[4] = buf[pos + 4];
	            f8b[5] = buf[pos + 5];
	            f8b[6] = buf[pos + 6];
	            f8b[7] = buf[pos + 7];
	            return f64[0];
	        }

	        function readDouble_f64_rev(buf, pos) {
	            f8b[7] = buf[pos    ];
	            f8b[6] = buf[pos + 1];
	            f8b[5] = buf[pos + 2];
	            f8b[4] = buf[pos + 3];
	            f8b[3] = buf[pos + 4];
	            f8b[2] = buf[pos + 5];
	            f8b[1] = buf[pos + 6];
	            f8b[0] = buf[pos + 7];
	            return f64[0];
	        }

	        /* istanbul ignore next */
	        exports.readDoubleLE = le ? readDouble_f64_cpy : readDouble_f64_rev;
	        /* istanbul ignore next */
	        exports.readDoubleBE = le ? readDouble_f64_rev : readDouble_f64_cpy;

	    // double: ieee754
	    })(); else (function() {

	        function writeDouble_ieee754(writeUint, off0, off1, val, buf, pos) {
	            var sign = val < 0 ? 1 : 0;
	            if (sign)
	                val = -val;
	            if (val === 0) {
	                writeUint(0, buf, pos + off0);
	                writeUint(1 / val > 0 ? /* positive */ 0 : /* negative 0 */ 2147483648, buf, pos + off1);
	            } else if (isNaN(val)) {
	                writeUint(0, buf, pos + off0);
	                writeUint(2146959360, buf, pos + off1);
	            } else if (val > 1.7976931348623157e+308) { // +-Infinity
	                writeUint(0, buf, pos + off0);
	                writeUint((sign << 31 | 2146435072) >>> 0, buf, pos + off1);
	            } else {
	                var mantissa;
	                if (val < 2.2250738585072014e-308) { // denormal
	                    mantissa = val / 5e-324;
	                    writeUint(mantissa >>> 0, buf, pos + off0);
	                    writeUint((sign << 31 | mantissa / 4294967296) >>> 0, buf, pos + off1);
	                } else {
	                    var exponent = Math.floor(Math.log(val) / Math.LN2);
	                    if (exponent === 1024)
	                        exponent = 1023;
	                    mantissa = val * Math.pow(2, -exponent);
	                    writeUint(mantissa * 4503599627370496 >>> 0, buf, pos + off0);
	                    writeUint((sign << 31 | exponent + 1023 << 20 | mantissa * 1048576 & 1048575) >>> 0, buf, pos + off1);
	                }
	            }
	        }

	        exports.writeDoubleLE = writeDouble_ieee754.bind(null, writeUintLE, 0, 4);
	        exports.writeDoubleBE = writeDouble_ieee754.bind(null, writeUintBE, 4, 0);

	        function readDouble_ieee754(readUint, off0, off1, buf, pos) {
	            var lo = readUint(buf, pos + off0),
	                hi = readUint(buf, pos + off1);
	            var sign = (hi >> 31) * 2 + 1,
	                exponent = hi >>> 20 & 2047,
	                mantissa = 4294967296 * (hi & 1048575) + lo;
	            return exponent === 2047
	                ? mantissa
	                ? NaN
	                : sign * Infinity
	                : exponent === 0 // denormal
	                ? sign * 5e-324 * mantissa
	                : sign * Math.pow(2, exponent - 1075) * (mantissa + 4503599627370496);
	        }

	        exports.readDoubleLE = readDouble_ieee754.bind(null, readUintLE, 0, 4);
	        exports.readDoubleBE = readDouble_ieee754.bind(null, readUintBE, 4, 0);

	    })();

	    return exports;
	}

	// uint helpers

	function writeUintLE(val, buf, pos) {
	    buf[pos    ] =  val        & 255;
	    buf[pos + 1] =  val >>> 8  & 255;
	    buf[pos + 2] =  val >>> 16 & 255;
	    buf[pos + 3] =  val >>> 24;
	}

	function writeUintBE(val, buf, pos) {
	    buf[pos    ] =  val >>> 24;
	    buf[pos + 1] =  val >>> 16 & 255;
	    buf[pos + 2] =  val >>> 8  & 255;
	    buf[pos + 3] =  val        & 255;
	}

	function readUintLE(buf, pos) {
	    return (buf[pos    ]
	          | buf[pos + 1] << 8
	          | buf[pos + 2] << 16
	          | buf[pos + 3] << 24) >>> 0;
	}

	function readUintBE(buf, pos) {
	    return (buf[pos    ] << 24
	          | buf[pos + 1] << 16
	          | buf[pos + 2] << 8
	          | buf[pos + 3]) >>> 0;
	}
	return float;
}

var inquire_1;
var hasRequiredInquire;

function requireInquire () {
	if (hasRequiredInquire) return inquire_1;
	hasRequiredInquire = 1;
	inquire_1 = inquire;

	/**
	 * Requires a module only if available.
	 * @memberof util
	 * @param {string} moduleName Module to require
	 * @returns {?Object} Required module if available and not empty, otherwise `null`
	 */
	function inquire(moduleName) {
	    try {
	        var mod = eval("quire".replace(/^/,"re"))(moduleName); // eslint-disable-line no-eval
	        if (mod && (mod.length || Object.keys(mod).length))
	            return mod;
	    } catch (e) {} // eslint-disable-line no-empty
	    return null;
	}
	return inquire_1;
}

var utf8$2 = {};

var hasRequiredUtf8;

function requireUtf8 () {
	if (hasRequiredUtf8) return utf8$2;
	hasRequiredUtf8 = 1;
	(function (exports) {

		/**
		 * A minimal UTF8 implementation for number arrays.
		 * @memberof util
		 * @namespace
		 */
		var utf8 = exports;

		/**
		 * Calculates the UTF8 byte length of a string.
		 * @param {string} string String
		 * @returns {number} Byte length
		 */
		utf8.length = function utf8_length(string) {
		    var len = 0,
		        c = 0;
		    for (var i = 0; i < string.length; ++i) {
		        c = string.charCodeAt(i);
		        if (c < 128)
		            len += 1;
		        else if (c < 2048)
		            len += 2;
		        else if ((c & 0xFC00) === 0xD800 && (string.charCodeAt(i + 1) & 0xFC00) === 0xDC00) {
		            ++i;
		            len += 4;
		        } else
		            len += 3;
		    }
		    return len;
		};

		/**
		 * Reads UTF8 bytes as a string.
		 * @param {Uint8Array} buffer Source buffer
		 * @param {number} start Source start
		 * @param {number} end Source end
		 * @returns {string} String read
		 */
		utf8.read = function utf8_read(buffer, start, end) {
		    var len = end - start;
		    if (len < 1)
		        return "";
		    var parts = null,
		        chunk = [],
		        i = 0, // char offset
		        t;     // temporary
		    while (start < end) {
		        t = buffer[start++];
		        if (t < 128)
		            chunk[i++] = t;
		        else if (t > 191 && t < 224)
		            chunk[i++] = (t & 31) << 6 | buffer[start++] & 63;
		        else if (t > 239 && t < 365) {
		            t = ((t & 7) << 18 | (buffer[start++] & 63) << 12 | (buffer[start++] & 63) << 6 | buffer[start++] & 63) - 0x10000;
		            chunk[i++] = 0xD800 + (t >> 10);
		            chunk[i++] = 0xDC00 + (t & 1023);
		        } else
		            chunk[i++] = (t & 15) << 12 | (buffer[start++] & 63) << 6 | buffer[start++] & 63;
		        if (i > 8191) {
		            (parts || (parts = [])).push(String.fromCharCode.apply(String, chunk));
		            i = 0;
		        }
		    }
		    if (parts) {
		        if (i)
		            parts.push(String.fromCharCode.apply(String, chunk.slice(0, i)));
		        return parts.join("");
		    }
		    return String.fromCharCode.apply(String, chunk.slice(0, i));
		};

		/**
		 * Writes a string as UTF8 bytes.
		 * @param {string} string Source string
		 * @param {Uint8Array} buffer Destination buffer
		 * @param {number} offset Destination offset
		 * @returns {number} Bytes written
		 */
		utf8.write = function utf8_write(string, buffer, offset) {
		    var start = offset,
		        c1, // character 1
		        c2; // character 2
		    for (var i = 0; i < string.length; ++i) {
		        c1 = string.charCodeAt(i);
		        if (c1 < 128) {
		            buffer[offset++] = c1;
		        } else if (c1 < 2048) {
		            buffer[offset++] = c1 >> 6       | 192;
		            buffer[offset++] = c1       & 63 | 128;
		        } else if ((c1 & 0xFC00) === 0xD800 && ((c2 = string.charCodeAt(i + 1)) & 0xFC00) === 0xDC00) {
		            c1 = 0x10000 + ((c1 & 0x03FF) << 10) + (c2 & 0x03FF);
		            ++i;
		            buffer[offset++] = c1 >> 18      | 240;
		            buffer[offset++] = c1 >> 12 & 63 | 128;
		            buffer[offset++] = c1 >> 6  & 63 | 128;
		            buffer[offset++] = c1       & 63 | 128;
		        } else {
		            buffer[offset++] = c1 >> 12      | 224;
		            buffer[offset++] = c1 >> 6  & 63 | 128;
		            buffer[offset++] = c1       & 63 | 128;
		        }
		    }
		    return offset - start;
		}; 
	} (utf8$2));
	return utf8$2;
}

var pool_1;
var hasRequiredPool;

function requirePool () {
	if (hasRequiredPool) return pool_1;
	hasRequiredPool = 1;
	pool_1 = pool;

	/**
	 * An allocator as used by {@link util.pool}.
	 * @typedef PoolAllocator
	 * @type {function}
	 * @param {number} size Buffer size
	 * @returns {Uint8Array} Buffer
	 */

	/**
	 * A slicer as used by {@link util.pool}.
	 * @typedef PoolSlicer
	 * @type {function}
	 * @param {number} start Start offset
	 * @param {number} end End offset
	 * @returns {Uint8Array} Buffer slice
	 * @this {Uint8Array}
	 */

	/**
	 * A general purpose buffer pool.
	 * @memberof util
	 * @function
	 * @param {PoolAllocator} alloc Allocator
	 * @param {PoolSlicer} slice Slicer
	 * @param {number} [size=8192] Slab size
	 * @returns {PoolAllocator} Pooled allocator
	 */
	function pool(alloc, slice, size) {
	    var SIZE   = size || 8192;
	    var MAX    = SIZE >>> 1;
	    var slab   = null;
	    var offset = SIZE;
	    return function pool_alloc(size) {
	        if (size < 1 || size > MAX)
	            return alloc(size);
	        if (offset + size > SIZE) {
	            slab = alloc(SIZE);
	            offset = 0;
	        }
	        var buf = slice.call(slab, offset, offset += size);
	        if (offset & 7) // align to 32 bit
	            offset = (offset | 7) + 1;
	        return buf;
	    };
	}
	return pool_1;
}

var longbits;
var hasRequiredLongbits;

function requireLongbits () {
	if (hasRequiredLongbits) return longbits;
	hasRequiredLongbits = 1;
	longbits = LongBits;

	var util = requireMinimal();

	/**
	 * Constructs new long bits.
	 * @classdesc Helper class for working with the low and high bits of a 64 bit value.
	 * @memberof util
	 * @constructor
	 * @param {number} lo Low 32 bits, unsigned
	 * @param {number} hi High 32 bits, unsigned
	 */
	function LongBits(lo, hi) {

	    // note that the casts below are theoretically unnecessary as of today, but older statically
	    // generated converter code might still call the ctor with signed 32bits. kept for compat.

	    /**
	     * Low bits.
	     * @type {number}
	     */
	    this.lo = lo >>> 0;

	    /**
	     * High bits.
	     * @type {number}
	     */
	    this.hi = hi >>> 0;
	}

	/**
	 * Zero bits.
	 * @memberof util.LongBits
	 * @type {util.LongBits}
	 */
	var zero = LongBits.zero = new LongBits(0, 0);

	zero.toNumber = function() { return 0; };
	zero.zzEncode = zero.zzDecode = function() { return this; };
	zero.length = function() { return 1; };

	/**
	 * Zero hash.
	 * @memberof util.LongBits
	 * @type {string}
	 */
	var zeroHash = LongBits.zeroHash = "\0\0\0\0\0\0\0\0";

	/**
	 * Constructs new long bits from the specified number.
	 * @param {number} value Value
	 * @returns {util.LongBits} Instance
	 */
	LongBits.fromNumber = function fromNumber(value) {
	    if (value === 0)
	        return zero;
	    var sign = value < 0;
	    if (sign)
	        value = -value;
	    var lo = value >>> 0,
	        hi = (value - lo) / 4294967296 >>> 0;
	    if (sign) {
	        hi = ~hi >>> 0;
	        lo = ~lo >>> 0;
	        if (++lo > 4294967295) {
	            lo = 0;
	            if (++hi > 4294967295)
	                hi = 0;
	        }
	    }
	    return new LongBits(lo, hi);
	};

	/**
	 * Constructs new long bits from a number, long or string.
	 * @param {Long|number|string} value Value
	 * @returns {util.LongBits} Instance
	 */
	LongBits.from = function from(value) {
	    if (typeof value === "number")
	        return LongBits.fromNumber(value);
	    if (util.isString(value)) {
	        /* istanbul ignore else */
	        if (util.Long)
	            value = util.Long.fromString(value);
	        else
	            return LongBits.fromNumber(parseInt(value, 10));
	    }
	    return value.low || value.high ? new LongBits(value.low >>> 0, value.high >>> 0) : zero;
	};

	/**
	 * Converts this long bits to a possibly unsafe JavaScript number.
	 * @param {boolean} [unsigned=false] Whether unsigned or not
	 * @returns {number} Possibly unsafe number
	 */
	LongBits.prototype.toNumber = function toNumber(unsigned) {
	    if (!unsigned && this.hi >>> 31) {
	        var lo = ~this.lo + 1 >>> 0,
	            hi = ~this.hi     >>> 0;
	        if (!lo)
	            hi = hi + 1 >>> 0;
	        return -(lo + hi * 4294967296);
	    }
	    return this.lo + this.hi * 4294967296;
	};

	/**
	 * Converts this long bits to a long.
	 * @param {boolean} [unsigned=false] Whether unsigned or not
	 * @returns {Long} Long
	 */
	LongBits.prototype.toLong = function toLong(unsigned) {
	    return util.Long
	        ? new util.Long(this.lo | 0, this.hi | 0, Boolean(unsigned))
	        /* istanbul ignore next */
	        : { low: this.lo | 0, high: this.hi | 0, unsigned: Boolean(unsigned) };
	};

	var charCodeAt = String.prototype.charCodeAt;

	/**
	 * Constructs new long bits from the specified 8 characters long hash.
	 * @param {string} hash Hash
	 * @returns {util.LongBits} Bits
	 */
	LongBits.fromHash = function fromHash(hash) {
	    if (hash === zeroHash)
	        return zero;
	    return new LongBits(
	        ( charCodeAt.call(hash, 0)
	        | charCodeAt.call(hash, 1) << 8
	        | charCodeAt.call(hash, 2) << 16
	        | charCodeAt.call(hash, 3) << 24) >>> 0
	    ,
	        ( charCodeAt.call(hash, 4)
	        | charCodeAt.call(hash, 5) << 8
	        | charCodeAt.call(hash, 6) << 16
	        | charCodeAt.call(hash, 7) << 24) >>> 0
	    );
	};

	/**
	 * Converts this long bits to a 8 characters long hash.
	 * @returns {string} Hash
	 */
	LongBits.prototype.toHash = function toHash() {
	    return String.fromCharCode(
	        this.lo        & 255,
	        this.lo >>> 8  & 255,
	        this.lo >>> 16 & 255,
	        this.lo >>> 24      ,
	        this.hi        & 255,
	        this.hi >>> 8  & 255,
	        this.hi >>> 16 & 255,
	        this.hi >>> 24
	    );
	};

	/**
	 * Zig-zag encodes this long bits.
	 * @returns {util.LongBits} `this`
	 */
	LongBits.prototype.zzEncode = function zzEncode() {
	    var mask =   this.hi >> 31;
	    this.hi  = ((this.hi << 1 | this.lo >>> 31) ^ mask) >>> 0;
	    this.lo  = ( this.lo << 1                   ^ mask) >>> 0;
	    return this;
	};

	/**
	 * Zig-zag decodes this long bits.
	 * @returns {util.LongBits} `this`
	 */
	LongBits.prototype.zzDecode = function zzDecode() {
	    var mask = -(this.lo & 1);
	    this.lo  = ((this.lo >>> 1 | this.hi << 31) ^ mask) >>> 0;
	    this.hi  = ( this.hi >>> 1                  ^ mask) >>> 0;
	    return this;
	};

	/**
	 * Calculates the length of this longbits when encoded as a varint.
	 * @returns {number} Length
	 */
	LongBits.prototype.length = function length() {
	    var part0 =  this.lo,
	        part1 = (this.lo >>> 28 | this.hi << 4) >>> 0,
	        part2 =  this.hi >>> 24;
	    return part2 === 0
	         ? part1 === 0
	           ? part0 < 16384
	             ? part0 < 128 ? 1 : 2
	             : part0 < 2097152 ? 3 : 4
	           : part1 < 16384
	             ? part1 < 128 ? 5 : 6
	             : part1 < 2097152 ? 7 : 8
	         : part2 < 128 ? 9 : 10;
	};
	return longbits;
}

var hasRequiredMinimal;

function requireMinimal () {
	if (hasRequiredMinimal) return minimal$1;
	hasRequiredMinimal = 1;
	(function (exports) {
		var util = exports;

		// used to return a Promise where callback is omitted
		util.asPromise = requireAspromise();

		// converts to / from base64 encoded strings
		util.base64 = requireBase64();

		// base class of rpc.Service
		util.EventEmitter = requireEventemitter();

		// float handling accross browsers
		util.float = requireFloat();

		// requires modules optionally and hides the call from bundlers
		util.inquire = requireInquire();

		// converts to / from utf8 encoded strings
		util.utf8 = requireUtf8();

		// provides a node-like buffer pool in the browser
		util.pool = requirePool();

		// utility to work with the low and high bits of a 64 bit value
		util.LongBits = requireLongbits();

		/**
		 * Whether running within node or not.
		 * @memberof util
		 * @type {boolean}
		 */
		util.isNode = Boolean(typeof commonjsGlobal !== "undefined"
		                   && commonjsGlobal
		                   && commonjsGlobal.process
		                   && commonjsGlobal.process.versions
		                   && commonjsGlobal.process.versions.node);

		/**
		 * Global object reference.
		 * @memberof util
		 * @type {Object}
		 */
		util.global = util.isNode && commonjsGlobal
		           || typeof window !== "undefined" && window
		           || typeof self   !== "undefined" && self
		           || commonjsGlobal; // eslint-disable-line no-invalid-this

		/**
		 * An immuable empty array.
		 * @memberof util
		 * @type {Array.<*>}
		 * @const
		 */
		util.emptyArray = Object.freeze ? Object.freeze([]) : /* istanbul ignore next */ []; // used on prototypes

		/**
		 * An immutable empty object.
		 * @type {Object}
		 * @const
		 */
		util.emptyObject = Object.freeze ? Object.freeze({}) : /* istanbul ignore next */ {}; // used on prototypes

		/**
		 * Tests if the specified value is an integer.
		 * @function
		 * @param {*} value Value to test
		 * @returns {boolean} `true` if the value is an integer
		 */
		util.isInteger = Number.isInteger || /* istanbul ignore next */ function isInteger(value) {
		    return typeof value === "number" && isFinite(value) && Math.floor(value) === value;
		};

		/**
		 * Tests if the specified value is a string.
		 * @param {*} value Value to test
		 * @returns {boolean} `true` if the value is a string
		 */
		util.isString = function isString(value) {
		    return typeof value === "string" || value instanceof String;
		};

		/**
		 * Tests if the specified value is a non-null object.
		 * @param {*} value Value to test
		 * @returns {boolean} `true` if the value is a non-null object
		 */
		util.isObject = function isObject(value) {
		    return value && typeof value === "object";
		};

		/**
		 * Checks if a property on a message is considered to be present.
		 * This is an alias of {@link util.isSet}.
		 * @function
		 * @param {Object} obj Plain object or message instance
		 * @param {string} prop Property name
		 * @returns {boolean} `true` if considered to be present, otherwise `false`
		 */
		util.isset =

		/**
		 * Checks if a property on a message is considered to be present.
		 * @param {Object} obj Plain object or message instance
		 * @param {string} prop Property name
		 * @returns {boolean} `true` if considered to be present, otherwise `false`
		 */
		util.isSet = function isSet(obj, prop) {
		    var value = obj[prop];
		    if (value != null && obj.hasOwnProperty(prop)) // eslint-disable-line eqeqeq, no-prototype-builtins
		        return typeof value !== "object" || (Array.isArray(value) ? value.length : Object.keys(value).length) > 0;
		    return false;
		};

		/**
		 * Any compatible Buffer instance.
		 * This is a minimal stand-alone definition of a Buffer instance. The actual type is that exported by node's typings.
		 * @interface Buffer
		 * @extends Uint8Array
		 */

		/**
		 * Node's Buffer class if available.
		 * @type {Constructor<Buffer>}
		 */
		util.Buffer = (function() {
		    try {
		        var Buffer = util.inquire("buffer").Buffer;
		        // refuse to use non-node buffers if not explicitly assigned (perf reasons):
		        return Buffer.prototype.utf8Write ? Buffer : /* istanbul ignore next */ null;
		    } catch (e) {
		        /* istanbul ignore next */
		        return null;
		    }
		})();

		// Internal alias of or polyfull for Buffer.from.
		util._Buffer_from = null;

		// Internal alias of or polyfill for Buffer.allocUnsafe.
		util._Buffer_allocUnsafe = null;

		/**
		 * Creates a new buffer of whatever type supported by the environment.
		 * @param {number|number[]} [sizeOrArray=0] Buffer size or number array
		 * @returns {Uint8Array|Buffer} Buffer
		 */
		util.newBuffer = function newBuffer(sizeOrArray) {
		    /* istanbul ignore next */
		    return typeof sizeOrArray === "number"
		        ? util.Buffer
		            ? util._Buffer_allocUnsafe(sizeOrArray)
		            : new util.Array(sizeOrArray)
		        : util.Buffer
		            ? util._Buffer_from(sizeOrArray)
		            : typeof Uint8Array === "undefined"
		                ? sizeOrArray
		                : new Uint8Array(sizeOrArray);
		};

		/**
		 * Array implementation used in the browser. `Uint8Array` if supported, otherwise `Array`.
		 * @type {Constructor<Uint8Array>}
		 */
		util.Array = typeof Uint8Array !== "undefined" ? Uint8Array /* istanbul ignore next */ : Array;

		/**
		 * Any compatible Long instance.
		 * This is a minimal stand-alone definition of a Long instance. The actual type is that exported by long.js.
		 * @interface Long
		 * @property {number} low Low bits
		 * @property {number} high High bits
		 * @property {boolean} unsigned Whether unsigned or not
		 */

		/**
		 * Long.js's Long class if available.
		 * @type {Constructor<Long>}
		 */
		util.Long = /* istanbul ignore next */ util.global.dcodeIO && /* istanbul ignore next */ util.global.dcodeIO.Long
		         || /* istanbul ignore next */ util.global.Long
		         || util.inquire("long");

		/**
		 * Regular expression used to verify 2 bit (`bool`) map keys.
		 * @type {RegExp}
		 * @const
		 */
		util.key2Re = /^true|false|0|1$/;

		/**
		 * Regular expression used to verify 32 bit (`int32` etc.) map keys.
		 * @type {RegExp}
		 * @const
		 */
		util.key32Re = /^-?(?:0|[1-9][0-9]*)$/;

		/**
		 * Regular expression used to verify 64 bit (`int64` etc.) map keys.
		 * @type {RegExp}
		 * @const
		 */
		util.key64Re = /^(?:[\\x00-\\xff]{8}|-?(?:0|[1-9][0-9]*))$/;

		/**
		 * Converts a number or long to an 8 characters long hash string.
		 * @param {Long|number} value Value to convert
		 * @returns {string} Hash
		 */
		util.longToHash = function longToHash(value) {
		    return value
		        ? util.LongBits.from(value).toHash()
		        : util.LongBits.zeroHash;
		};

		/**
		 * Converts an 8 characters long hash string to a long or number.
		 * @param {string} hash Hash
		 * @param {boolean} [unsigned=false] Whether unsigned or not
		 * @returns {Long|number} Original value
		 */
		util.longFromHash = function longFromHash(hash, unsigned) {
		    var bits = util.LongBits.fromHash(hash);
		    if (util.Long)
		        return util.Long.fromBits(bits.lo, bits.hi, unsigned);
		    return bits.toNumber(Boolean(unsigned));
		};

		/**
		 * Merges the properties of the source object into the destination object.
		 * @memberof util
		 * @param {Object.<string,*>} dst Destination object
		 * @param {Object.<string,*>} src Source object
		 * @param {boolean} [ifNotSet=false] Merges only if the key is not already set
		 * @returns {Object.<string,*>} Destination object
		 */
		function merge(dst, src, ifNotSet) { // used by converters
		    for (var keys = Object.keys(src), i = 0; i < keys.length; ++i)
		        if (dst[keys[i]] === undefined || !ifNotSet)
		            dst[keys[i]] = src[keys[i]];
		    return dst;
		}

		util.merge = merge;

		/**
		 * Converts the first character of a string to lower case.
		 * @param {string} str String to convert
		 * @returns {string} Converted string
		 */
		util.lcFirst = function lcFirst(str) {
		    return str.charAt(0).toLowerCase() + str.substring(1);
		};

		/**
		 * Creates a custom error constructor.
		 * @memberof util
		 * @param {string} name Error name
		 * @returns {Constructor<Error>} Custom error constructor
		 */
		function newError(name) {

		    function CustomError(message, properties) {

		        if (!(this instanceof CustomError))
		            return new CustomError(message, properties);

		        // Error.call(this, message);
		        // ^ just returns a new error instance because the ctor can be called as a function

		        Object.defineProperty(this, "message", { get: function() { return message; } });

		        /* istanbul ignore next */
		        if (Error.captureStackTrace) // node
		            Error.captureStackTrace(this, CustomError);
		        else
		            Object.defineProperty(this, "stack", { value: new Error().stack || "" });

		        if (properties)
		            merge(this, properties);
		    }

		    CustomError.prototype = Object.create(Error.prototype, {
		        constructor: {
		            value: CustomError,
		            writable: true,
		            enumerable: false,
		            configurable: true,
		        },
		        name: {
		            get: function get() { return name; },
		            set: undefined,
		            enumerable: false,
		            // configurable: false would accurately preserve the behavior of
		            // the original, but I'm guessing that was not intentional.
		            // For an actual error subclass, this property would
		            // be configurable.
		            configurable: true,
		        },
		        toString: {
		            value: function value() { return this.name + ": " + this.message; },
		            writable: true,
		            enumerable: false,
		            configurable: true,
		        },
		    });

		    return CustomError;
		}

		util.newError = newError;

		/**
		 * Constructs a new protocol error.
		 * @classdesc Error subclass indicating a protocol specifc error.
		 * @memberof util
		 * @extends Error
		 * @template T extends Message<T>
		 * @constructor
		 * @param {string} message Error message
		 * @param {Object.<string,*>} [properties] Additional properties
		 * @example
		 * try {
		 *     MyMessage.decode(someBuffer); // throws if required fields are missing
		 * } catch (e) {
		 *     if (e instanceof ProtocolError && e.instance)
		 *         console.log("decoded so far: " + JSON.stringify(e.instance));
		 * }
		 */
		util.ProtocolError = newError("ProtocolError");

		/**
		 * So far decoded message instance.
		 * @name util.ProtocolError#instance
		 * @type {Message<T>}
		 */

		/**
		 * A OneOf getter as returned by {@link util.oneOfGetter}.
		 * @typedef OneOfGetter
		 * @type {function}
		 * @returns {string|undefined} Set field name, if any
		 */

		/**
		 * Builds a getter for a oneof's present field name.
		 * @param {string[]} fieldNames Field names
		 * @returns {OneOfGetter} Unbound getter
		 */
		util.oneOfGetter = function getOneOf(fieldNames) {
		    var fieldMap = {};
		    for (var i = 0; i < fieldNames.length; ++i)
		        fieldMap[fieldNames[i]] = 1;

		    /**
		     * @returns {string|undefined} Set field name, if any
		     * @this Object
		     * @ignore
		     */
		    return function() { // eslint-disable-line consistent-return
		        for (var keys = Object.keys(this), i = keys.length - 1; i > -1; --i)
		            if (fieldMap[keys[i]] === 1 && this[keys[i]] !== undefined && this[keys[i]] !== null)
		                return keys[i];
		    };
		};

		/**
		 * A OneOf setter as returned by {@link util.oneOfSetter}.
		 * @typedef OneOfSetter
		 * @type {function}
		 * @param {string|undefined} value Field name
		 * @returns {undefined}
		 */

		/**
		 * Builds a setter for a oneof's present field name.
		 * @param {string[]} fieldNames Field names
		 * @returns {OneOfSetter} Unbound setter
		 */
		util.oneOfSetter = function setOneOf(fieldNames) {

		    /**
		     * @param {string} name Field name
		     * @returns {undefined}
		     * @this Object
		     * @ignore
		     */
		    return function(name) {
		        for (var i = 0; i < fieldNames.length; ++i)
		            if (fieldNames[i] !== name)
		                delete this[fieldNames[i]];
		    };
		};

		/**
		 * Default conversion options used for {@link Message#toJSON} implementations.
		 *
		 * These options are close to proto3's JSON mapping with the exception that internal types like Any are handled just like messages. More precisely:
		 *
		 * - Longs become strings
		 * - Enums become string keys
		 * - Bytes become base64 encoded strings
		 * - (Sub-)Messages become plain objects
		 * - Maps become plain objects with all string keys
		 * - Repeated fields become arrays
		 * - NaN and Infinity for float and double fields become strings
		 *
		 * @type {IConversionOptions}
		 * @see https://developers.google.com/protocol-buffers/docs/proto3?hl=en#json
		 */
		util.toJSONOptions = {
		    longs: String,
		    enums: String,
		    bytes: String,
		    json: true
		};

		// Sets up buffer utility according to the environment (called in index-minimal)
		util._configure = function() {
		    var Buffer = util.Buffer;
		    /* istanbul ignore if */
		    if (!Buffer) {
		        util._Buffer_from = util._Buffer_allocUnsafe = null;
		        return;
		    }
		    // because node 4.x buffers are incompatible & immutable
		    // see: https://github.com/dcodeIO/protobuf.js/pull/665
		    util._Buffer_from = Buffer.from !== Uint8Array.from && Buffer.from ||
		        /* istanbul ignore next */
		        function Buffer_from(value, encoding) {
		            return new Buffer(value, encoding);
		        };
		    util._Buffer_allocUnsafe = Buffer.allocUnsafe ||
		        /* istanbul ignore next */
		        function Buffer_allocUnsafe(size) {
		            return new Buffer(size);
		        };
		}; 
	} (minimal$1));
	return minimal$1;
}

var writer = Writer$1;

var util$4      = requireMinimal();

var BufferWriter$1; // cyclic

var LongBits$1  = util$4.LongBits,
    base64    = util$4.base64,
    utf8$1      = util$4.utf8;

/**
 * Constructs a new writer operation instance.
 * @classdesc Scheduled writer operation.
 * @constructor
 * @param {function(*, Uint8Array, number)} fn Function to call
 * @param {number} len Value byte length
 * @param {*} val Value to write
 * @ignore
 */
function Op(fn, len, val) {

    /**
     * Function to call.
     * @type {function(Uint8Array, number, *)}
     */
    this.fn = fn;

    /**
     * Value byte length.
     * @type {number}
     */
    this.len = len;

    /**
     * Next operation.
     * @type {Writer.Op|undefined}
     */
    this.next = undefined;

    /**
     * Value to write.
     * @type {*}
     */
    this.val = val; // type varies
}

/* istanbul ignore next */
function noop() {} // eslint-disable-line no-empty-function

/**
 * Constructs a new writer state instance.
 * @classdesc Copied writer state.
 * @memberof Writer
 * @constructor
 * @param {Writer} writer Writer to copy state from
 * @ignore
 */
function State(writer) {

    /**
     * Current head.
     * @type {Writer.Op}
     */
    this.head = writer.head;

    /**
     * Current tail.
     * @type {Writer.Op}
     */
    this.tail = writer.tail;

    /**
     * Current buffer length.
     * @type {number}
     */
    this.len = writer.len;

    /**
     * Next state.
     * @type {State|null}
     */
    this.next = writer.states;
}

/**
 * Constructs a new writer instance.
 * @classdesc Wire format writer using `Uint8Array` if available, otherwise `Array`.
 * @constructor
 */
function Writer$1() {

    /**
     * Current length.
     * @type {number}
     */
    this.len = 0;

    /**
     * Operations head.
     * @type {Object}
     */
    this.head = new Op(noop, 0, 0);

    /**
     * Operations tail
     * @type {Object}
     */
    this.tail = this.head;

    /**
     * Linked forked states.
     * @type {Object|null}
     */
    this.states = null;

    // When a value is written, the writer calculates its byte length and puts it into a linked
    // list of operations to perform when finish() is called. This both allows us to allocate
    // buffers of the exact required size and reduces the amount of work we have to do compared
    // to first calculating over objects and then encoding over objects. In our case, the encoding
    // part is just a linked list walk calling operations with already prepared values.
}

var create$1 = function create() {
    return util$4.Buffer
        ? function create_buffer_setup() {
            return (Writer$1.create = function create_buffer() {
                return new BufferWriter$1();
            })();
        }
        /* istanbul ignore next */
        : function create_array() {
            return new Writer$1();
        };
};

/**
 * Creates a new writer.
 * @function
 * @returns {BufferWriter|Writer} A {@link BufferWriter} when Buffers are supported, otherwise a {@link Writer}
 */
Writer$1.create = create$1();

/**
 * Allocates a buffer of the specified size.
 * @param {number} size Buffer size
 * @returns {Uint8Array} Buffer
 */
Writer$1.alloc = function alloc(size) {
    return new util$4.Array(size);
};

// Use Uint8Array buffer pool in the browser, just like node does with buffers
/* istanbul ignore else */
if (util$4.Array !== Array)
    Writer$1.alloc = util$4.pool(Writer$1.alloc, util$4.Array.prototype.subarray);

/**
 * Pushes a new operation to the queue.
 * @param {function(Uint8Array, number, *)} fn Function to call
 * @param {number} len Value byte length
 * @param {number} val Value to write
 * @returns {Writer} `this`
 * @private
 */
Writer$1.prototype._push = function push(fn, len, val) {
    this.tail = this.tail.next = new Op(fn, len, val);
    this.len += len;
    return this;
};

function writeByte(val, buf, pos) {
    buf[pos] = val & 255;
}

function writeVarint32(val, buf, pos) {
    while (val > 127) {
        buf[pos++] = val & 127 | 128;
        val >>>= 7;
    }
    buf[pos] = val;
}

/**
 * Constructs a new varint writer operation instance.
 * @classdesc Scheduled varint writer operation.
 * @extends Op
 * @constructor
 * @param {number} len Value byte length
 * @param {number} val Value to write
 * @ignore
 */
function VarintOp(len, val) {
    this.len = len;
    this.next = undefined;
    this.val = val;
}

VarintOp.prototype = Object.create(Op.prototype);
VarintOp.prototype.fn = writeVarint32;

/**
 * Writes an unsigned 32 bit value as a varint.
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.uint32 = function write_uint32(value) {
    // here, the call to this.push has been inlined and a varint specific Op subclass is used.
    // uint32 is by far the most frequently used operation and benefits significantly from this.
    this.len += (this.tail = this.tail.next = new VarintOp(
        (value = value >>> 0)
                < 128       ? 1
        : value < 16384     ? 2
        : value < 2097152   ? 3
        : value < 268435456 ? 4
        :                     5,
    value)).len;
    return this;
};

/**
 * Writes a signed 32 bit value as a varint.
 * @function
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.int32 = function write_int32(value) {
    return value < 0
        ? this._push(writeVarint64, 10, LongBits$1.fromNumber(value)) // 10 bytes per spec
        : this.uint32(value);
};

/**
 * Writes a 32 bit value as a varint, zig-zag encoded.
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.sint32 = function write_sint32(value) {
    return this.uint32((value << 1 ^ value >> 31) >>> 0);
};

function writeVarint64(val, buf, pos) {
    while (val.hi) {
        buf[pos++] = val.lo & 127 | 128;
        val.lo = (val.lo >>> 7 | val.hi << 25) >>> 0;
        val.hi >>>= 7;
    }
    while (val.lo > 127) {
        buf[pos++] = val.lo & 127 | 128;
        val.lo = val.lo >>> 7;
    }
    buf[pos++] = val.lo;
}

/**
 * Writes an unsigned 64 bit value as a varint.
 * @param {Long|number|string} value Value to write
 * @returns {Writer} `this`
 * @throws {TypeError} If `value` is a string and no long library is present.
 */
Writer$1.prototype.uint64 = function write_uint64(value) {
    var bits = LongBits$1.from(value);
    return this._push(writeVarint64, bits.length(), bits);
};

/**
 * Writes a signed 64 bit value as a varint.
 * @function
 * @param {Long|number|string} value Value to write
 * @returns {Writer} `this`
 * @throws {TypeError} If `value` is a string and no long library is present.
 */
Writer$1.prototype.int64 = Writer$1.prototype.uint64;

/**
 * Writes a signed 64 bit value as a varint, zig-zag encoded.
 * @param {Long|number|string} value Value to write
 * @returns {Writer} `this`
 * @throws {TypeError} If `value` is a string and no long library is present.
 */
Writer$1.prototype.sint64 = function write_sint64(value) {
    var bits = LongBits$1.from(value).zzEncode();
    return this._push(writeVarint64, bits.length(), bits);
};

/**
 * Writes a boolish value as a varint.
 * @param {boolean} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.bool = function write_bool(value) {
    return this._push(writeByte, 1, value ? 1 : 0);
};

function writeFixed32(val, buf, pos) {
    buf[pos    ] =  val         & 255;
    buf[pos + 1] =  val >>> 8   & 255;
    buf[pos + 2] =  val >>> 16  & 255;
    buf[pos + 3] =  val >>> 24;
}

/**
 * Writes an unsigned 32 bit value as fixed 32 bits.
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.fixed32 = function write_fixed32(value) {
    return this._push(writeFixed32, 4, value >>> 0);
};

/**
 * Writes a signed 32 bit value as fixed 32 bits.
 * @function
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.sfixed32 = Writer$1.prototype.fixed32;

/**
 * Writes an unsigned 64 bit value as fixed 64 bits.
 * @param {Long|number|string} value Value to write
 * @returns {Writer} `this`
 * @throws {TypeError} If `value` is a string and no long library is present.
 */
Writer$1.prototype.fixed64 = function write_fixed64(value) {
    var bits = LongBits$1.from(value);
    return this._push(writeFixed32, 4, bits.lo)._push(writeFixed32, 4, bits.hi);
};

/**
 * Writes a signed 64 bit value as fixed 64 bits.
 * @function
 * @param {Long|number|string} value Value to write
 * @returns {Writer} `this`
 * @throws {TypeError} If `value` is a string and no long library is present.
 */
Writer$1.prototype.sfixed64 = Writer$1.prototype.fixed64;

/**
 * Writes a float (32 bit).
 * @function
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.float = function write_float(value) {
    return this._push(util$4.float.writeFloatLE, 4, value);
};

/**
 * Writes a double (64 bit float).
 * @function
 * @param {number} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.double = function write_double(value) {
    return this._push(util$4.float.writeDoubleLE, 8, value);
};

var writeBytes = util$4.Array.prototype.set
    ? function writeBytes_set(val, buf, pos) {
        buf.set(val, pos); // also works for plain array values
    }
    /* istanbul ignore next */
    : function writeBytes_for(val, buf, pos) {
        for (var i = 0; i < val.length; ++i)
            buf[pos + i] = val[i];
    };

/**
 * Writes a sequence of bytes.
 * @param {Uint8Array|string} value Buffer or base64 encoded string to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.bytes = function write_bytes(value) {
    var len = value.length >>> 0;
    if (!len)
        return this._push(writeByte, 1, 0);
    if (util$4.isString(value)) {
        var buf = Writer$1.alloc(len = base64.length(value));
        base64.decode(value, buf, 0);
        value = buf;
    }
    return this.uint32(len)._push(writeBytes, len, value);
};

/**
 * Writes a string.
 * @param {string} value Value to write
 * @returns {Writer} `this`
 */
Writer$1.prototype.string = function write_string(value) {
    var len = utf8$1.length(value);
    return len
        ? this.uint32(len)._push(utf8$1.write, len, value)
        : this._push(writeByte, 1, 0);
};

/**
 * Forks this writer's state by pushing it to a stack.
 * Calling {@link Writer#reset|reset} or {@link Writer#ldelim|ldelim} resets the writer to the previous state.
 * @returns {Writer} `this`
 */
Writer$1.prototype.fork = function fork() {
    this.states = new State(this);
    this.head = this.tail = new Op(noop, 0, 0);
    this.len = 0;
    return this;
};

/**
 * Resets this instance to the last state.
 * @returns {Writer} `this`
 */
Writer$1.prototype.reset = function reset() {
    if (this.states) {
        this.head   = this.states.head;
        this.tail   = this.states.tail;
        this.len    = this.states.len;
        this.states = this.states.next;
    } else {
        this.head = this.tail = new Op(noop, 0, 0);
        this.len  = 0;
    }
    return this;
};

/**
 * Resets to the last state and appends the fork state's current write length as a varint followed by its operations.
 * @returns {Writer} `this`
 */
Writer$1.prototype.ldelim = function ldelim() {
    var head = this.head,
        tail = this.tail,
        len  = this.len;
    this.reset().uint32(len);
    if (len) {
        this.tail.next = head.next; // skip noop
        this.tail = tail;
        this.len += len;
    }
    return this;
};

/**
 * Finishes the write operation.
 * @returns {Uint8Array} Finished buffer
 */
Writer$1.prototype.finish = function finish() {
    var head = this.head.next, // skip noop
        buf  = this.constructor.alloc(this.len),
        pos  = 0;
    while (head) {
        head.fn(head.val, buf, pos);
        pos += head.len;
        head = head.next;
    }
    // this.head = this.tail = null;
    return buf;
};

Writer$1._configure = function(BufferWriter_) {
    BufferWriter$1 = BufferWriter_;
    Writer$1.create = create$1();
    BufferWriter$1._configure();
};

var writer_buffer = BufferWriter;

// extends Writer
var Writer = writer;
(BufferWriter.prototype = Object.create(Writer.prototype)).constructor = BufferWriter;

var util$3 = requireMinimal();

/**
 * Constructs a new buffer writer instance.
 * @classdesc Wire format writer using node buffers.
 * @extends Writer
 * @constructor
 */
function BufferWriter() {
    Writer.call(this);
}

BufferWriter._configure = function () {
    /**
     * Allocates a buffer of the specified size.
     * @function
     * @param {number} size Buffer size
     * @returns {Buffer} Buffer
     */
    BufferWriter.alloc = util$3._Buffer_allocUnsafe;

    BufferWriter.writeBytesBuffer = util$3.Buffer && util$3.Buffer.prototype instanceof Uint8Array && util$3.Buffer.prototype.set.name === "set"
        ? function writeBytesBuffer_set(val, buf, pos) {
          buf.set(val, pos); // faster than copy (requires node >= 4 where Buffers extend Uint8Array and set is properly inherited)
          // also works for plain array values
        }
        /* istanbul ignore next */
        : function writeBytesBuffer_copy(val, buf, pos) {
          if (val.copy) // Buffer values
            val.copy(buf, pos, 0, val.length);
          else for (var i = 0; i < val.length;) // plain array values
            buf[pos++] = val[i++];
        };
};


/**
 * @override
 */
BufferWriter.prototype.bytes = function write_bytes_buffer(value) {
    if (util$3.isString(value))
        value = util$3._Buffer_from(value, "base64");
    var len = value.length >>> 0;
    this.uint32(len);
    if (len)
        this._push(BufferWriter.writeBytesBuffer, len, value);
    return this;
};

function writeStringBuffer(val, buf, pos) {
    if (val.length < 40) // plain js is faster for short strings (probably due to redundant assertions)
        util$3.utf8.write(val, buf, pos);
    else if (buf.utf8Write)
        buf.utf8Write(val, pos);
    else
        buf.write(val, pos);
}

/**
 * @override
 */
BufferWriter.prototype.string = function write_string_buffer(value) {
    var len = util$3.Buffer.byteLength(value);
    this.uint32(len);
    if (len)
        this._push(writeStringBuffer, len, value);
    return this;
};


/**
 * Finishes the write operation.
 * @name BufferWriter#finish
 * @function
 * @returns {Buffer} Finished buffer
 */

BufferWriter._configure();

var reader = Reader$1;

var util$2      = requireMinimal();

var BufferReader$1; // cyclic

var LongBits  = util$2.LongBits,
    utf8      = util$2.utf8;

/* istanbul ignore next */
function indexOutOfRange(reader, writeLength) {
    return RangeError("index out of range: " + reader.pos + " + " + (writeLength || 1) + " > " + reader.len);
}

/**
 * Constructs a new reader instance using the specified buffer.
 * @classdesc Wire format reader using `Uint8Array` if available, otherwise `Array`.
 * @constructor
 * @param {Uint8Array} buffer Buffer to read from
 */
function Reader$1(buffer) {

    /**
     * Read buffer.
     * @type {Uint8Array}
     */
    this.buf = buffer;

    /**
     * Read buffer position.
     * @type {number}
     */
    this.pos = 0;

    /**
     * Read buffer length.
     * @type {number}
     */
    this.len = buffer.length;
}

var create_array = typeof Uint8Array !== "undefined"
    ? function create_typed_array(buffer) {
        if (buffer instanceof Uint8Array || Array.isArray(buffer))
            return new Reader$1(buffer);
        throw Error("illegal buffer");
    }
    /* istanbul ignore next */
    : function create_array(buffer) {
        if (Array.isArray(buffer))
            return new Reader$1(buffer);
        throw Error("illegal buffer");
    };

var create = function create() {
    return util$2.Buffer
        ? function create_buffer_setup(buffer) {
            return (Reader$1.create = function create_buffer(buffer) {
                return util$2.Buffer.isBuffer(buffer)
                    ? new BufferReader$1(buffer)
                    /* istanbul ignore next */
                    : create_array(buffer);
            })(buffer);
        }
        /* istanbul ignore next */
        : create_array;
};

/**
 * Creates a new reader using the specified buffer.
 * @function
 * @param {Uint8Array|Buffer} buffer Buffer to read from
 * @returns {Reader|BufferReader} A {@link BufferReader} if `buffer` is a Buffer, otherwise a {@link Reader}
 * @throws {Error} If `buffer` is not a valid buffer
 */
Reader$1.create = create();

Reader$1.prototype._slice = util$2.Array.prototype.subarray || /* istanbul ignore next */ util$2.Array.prototype.slice;

/**
 * Reads a varint as an unsigned 32 bit value.
 * @function
 * @returns {number} Value read
 */
Reader$1.prototype.uint32 = (function read_uint32_setup() {
    var value = 4294967295; // optimizer type-hint, tends to deopt otherwise (?!)
    return function read_uint32() {
        value = (         this.buf[this.pos] & 127       ) >>> 0; if (this.buf[this.pos++] < 128) return value;
        value = (value | (this.buf[this.pos] & 127) <<  7) >>> 0; if (this.buf[this.pos++] < 128) return value;
        value = (value | (this.buf[this.pos] & 127) << 14) >>> 0; if (this.buf[this.pos++] < 128) return value;
        value = (value | (this.buf[this.pos] & 127) << 21) >>> 0; if (this.buf[this.pos++] < 128) return value;
        value = (value | (this.buf[this.pos] &  15) << 28) >>> 0; if (this.buf[this.pos++] < 128) return value;

        /* istanbul ignore if */
        if ((this.pos += 5) > this.len) {
            this.pos = this.len;
            throw indexOutOfRange(this, 10);
        }
        return value;
    };
})();

/**
 * Reads a varint as a signed 32 bit value.
 * @returns {number} Value read
 */
Reader$1.prototype.int32 = function read_int32() {
    return this.uint32() | 0;
};

/**
 * Reads a zig-zag encoded varint as a signed 32 bit value.
 * @returns {number} Value read
 */
Reader$1.prototype.sint32 = function read_sint32() {
    var value = this.uint32();
    return value >>> 1 ^ -(value & 1) | 0;
};

/* eslint-disable no-invalid-this */

function readLongVarint() {
    // tends to deopt with local vars for octet etc.
    var bits = new LongBits(0, 0);
    var i = 0;
    if (this.len - this.pos > 4) { // fast route (lo)
        for (; i < 4; ++i) {
            // 1st..4th
            bits.lo = (bits.lo | (this.buf[this.pos] & 127) << i * 7) >>> 0;
            if (this.buf[this.pos++] < 128)
                return bits;
        }
        // 5th
        bits.lo = (bits.lo | (this.buf[this.pos] & 127) << 28) >>> 0;
        bits.hi = (bits.hi | (this.buf[this.pos] & 127) >>  4) >>> 0;
        if (this.buf[this.pos++] < 128)
            return bits;
        i = 0;
    } else {
        for (; i < 3; ++i) {
            /* istanbul ignore if */
            if (this.pos >= this.len)
                throw indexOutOfRange(this);
            // 1st..3th
            bits.lo = (bits.lo | (this.buf[this.pos] & 127) << i * 7) >>> 0;
            if (this.buf[this.pos++] < 128)
                return bits;
        }
        // 4th
        bits.lo = (bits.lo | (this.buf[this.pos++] & 127) << i * 7) >>> 0;
        return bits;
    }
    if (this.len - this.pos > 4) { // fast route (hi)
        for (; i < 5; ++i) {
            // 6th..10th
            bits.hi = (bits.hi | (this.buf[this.pos] & 127) << i * 7 + 3) >>> 0;
            if (this.buf[this.pos++] < 128)
                return bits;
        }
    } else {
        for (; i < 5; ++i) {
            /* istanbul ignore if */
            if (this.pos >= this.len)
                throw indexOutOfRange(this);
            // 6th..10th
            bits.hi = (bits.hi | (this.buf[this.pos] & 127) << i * 7 + 3) >>> 0;
            if (this.buf[this.pos++] < 128)
                return bits;
        }
    }
    /* istanbul ignore next */
    throw Error("invalid varint encoding");
}

/* eslint-enable no-invalid-this */

/**
 * Reads a varint as a signed 64 bit value.
 * @name Reader#int64
 * @function
 * @returns {Long} Value read
 */

/**
 * Reads a varint as an unsigned 64 bit value.
 * @name Reader#uint64
 * @function
 * @returns {Long} Value read
 */

/**
 * Reads a zig-zag encoded varint as a signed 64 bit value.
 * @name Reader#sint64
 * @function
 * @returns {Long} Value read
 */

/**
 * Reads a varint as a boolean.
 * @returns {boolean} Value read
 */
Reader$1.prototype.bool = function read_bool() {
    return this.uint32() !== 0;
};

function readFixed32_end(buf, end) { // note that this uses `end`, not `pos`
    return (buf[end - 4]
          | buf[end - 3] << 8
          | buf[end - 2] << 16
          | buf[end - 1] << 24) >>> 0;
}

/**
 * Reads fixed 32 bits as an unsigned 32 bit integer.
 * @returns {number} Value read
 */
Reader$1.prototype.fixed32 = function read_fixed32() {

    /* istanbul ignore if */
    if (this.pos + 4 > this.len)
        throw indexOutOfRange(this, 4);

    return readFixed32_end(this.buf, this.pos += 4);
};

/**
 * Reads fixed 32 bits as a signed 32 bit integer.
 * @returns {number} Value read
 */
Reader$1.prototype.sfixed32 = function read_sfixed32() {

    /* istanbul ignore if */
    if (this.pos + 4 > this.len)
        throw indexOutOfRange(this, 4);

    return readFixed32_end(this.buf, this.pos += 4) | 0;
};

/* eslint-disable no-invalid-this */

function readFixed64(/* this: Reader */) {

    /* istanbul ignore if */
    if (this.pos + 8 > this.len)
        throw indexOutOfRange(this, 8);

    return new LongBits(readFixed32_end(this.buf, this.pos += 4), readFixed32_end(this.buf, this.pos += 4));
}

/* eslint-enable no-invalid-this */

/**
 * Reads fixed 64 bits.
 * @name Reader#fixed64
 * @function
 * @returns {Long} Value read
 */

/**
 * Reads zig-zag encoded fixed 64 bits.
 * @name Reader#sfixed64
 * @function
 * @returns {Long} Value read
 */

/**
 * Reads a float (32 bit) as a number.
 * @function
 * @returns {number} Value read
 */
Reader$1.prototype.float = function read_float() {

    /* istanbul ignore if */
    if (this.pos + 4 > this.len)
        throw indexOutOfRange(this, 4);

    var value = util$2.float.readFloatLE(this.buf, this.pos);
    this.pos += 4;
    return value;
};

/**
 * Reads a double (64 bit float) as a number.
 * @function
 * @returns {number} Value read
 */
Reader$1.prototype.double = function read_double() {

    /* istanbul ignore if */
    if (this.pos + 8 > this.len)
        throw indexOutOfRange(this, 4);

    var value = util$2.float.readDoubleLE(this.buf, this.pos);
    this.pos += 8;
    return value;
};

/**
 * Reads a sequence of bytes preceeded by its length as a varint.
 * @returns {Uint8Array} Value read
 */
Reader$1.prototype.bytes = function read_bytes() {
    var length = this.uint32(),
        start  = this.pos,
        end    = this.pos + length;

    /* istanbul ignore if */
    if (end > this.len)
        throw indexOutOfRange(this, length);

    this.pos += length;
    if (Array.isArray(this.buf)) // plain array
        return this.buf.slice(start, end);

    if (start === end) { // fix for IE 10/Win8 and others' subarray returning array of size 1
        var nativeBuffer = util$2.Buffer;
        return nativeBuffer
            ? nativeBuffer.alloc(0)
            : new this.buf.constructor(0);
    }
    return this._slice.call(this.buf, start, end);
};

/**
 * Reads a string preceeded by its byte length as a varint.
 * @returns {string} Value read
 */
Reader$1.prototype.string = function read_string() {
    var bytes = this.bytes();
    return utf8.read(bytes, 0, bytes.length);
};

/**
 * Skips the specified number of bytes if specified, otherwise skips a varint.
 * @param {number} [length] Length if known, otherwise a varint is assumed
 * @returns {Reader} `this`
 */
Reader$1.prototype.skip = function skip(length) {
    if (typeof length === "number") {
        /* istanbul ignore if */
        if (this.pos + length > this.len)
            throw indexOutOfRange(this, length);
        this.pos += length;
    } else {
        do {
            /* istanbul ignore if */
            if (this.pos >= this.len)
                throw indexOutOfRange(this);
        } while (this.buf[this.pos++] & 128);
    }
    return this;
};

/**
 * Skips the next element of the specified wire type.
 * @param {number} wireType Wire type received
 * @returns {Reader} `this`
 */
Reader$1.prototype.skipType = function(wireType) {
    switch (wireType) {
        case 0:
            this.skip();
            break;
        case 1:
            this.skip(8);
            break;
        case 2:
            this.skip(this.uint32());
            break;
        case 3:
            while ((wireType = this.uint32() & 7) !== 4) {
                this.skipType(wireType);
            }
            break;
        case 5:
            this.skip(4);
            break;

        /* istanbul ignore next */
        default:
            throw Error("invalid wire type " + wireType + " at offset " + this.pos);
    }
    return this;
};

Reader$1._configure = function(BufferReader_) {
    BufferReader$1 = BufferReader_;
    Reader$1.create = create();
    BufferReader$1._configure();

    var fn = util$2.Long ? "toLong" : /* istanbul ignore next */ "toNumber";
    util$2.merge(Reader$1.prototype, {

        int64: function read_int64() {
            return readLongVarint.call(this)[fn](false);
        },

        uint64: function read_uint64() {
            return readLongVarint.call(this)[fn](true);
        },

        sint64: function read_sint64() {
            return readLongVarint.call(this).zzDecode()[fn](false);
        },

        fixed64: function read_fixed64() {
            return readFixed64.call(this)[fn](true);
        },

        sfixed64: function read_sfixed64() {
            return readFixed64.call(this)[fn](false);
        }

    });
};

var reader_buffer = BufferReader;

// extends Reader
var Reader = reader;
(BufferReader.prototype = Object.create(Reader.prototype)).constructor = BufferReader;

var util$1 = requireMinimal();

/**
 * Constructs a new buffer reader instance.
 * @classdesc Wire format reader using node buffers.
 * @extends Reader
 * @constructor
 * @param {Buffer} buffer Buffer to read from
 */
function BufferReader(buffer) {
    Reader.call(this, buffer);

    /**
     * Read buffer.
     * @name BufferReader#buf
     * @type {Buffer}
     */
}

BufferReader._configure = function () {
    /* istanbul ignore else */
    if (util$1.Buffer)
        BufferReader.prototype._slice = util$1.Buffer.prototype.slice;
};


/**
 * @override
 */
BufferReader.prototype.string = function read_string_buffer() {
    var len = this.uint32(); // modifies pos
    return this.buf.utf8Slice
        ? this.buf.utf8Slice(this.pos, this.pos = Math.min(this.pos + len, this.len))
        : this.buf.toString("utf-8", this.pos, this.pos = Math.min(this.pos + len, this.len));
};

/**
 * Reads a sequence of bytes preceeded by its length as a varint.
 * @name BufferReader#bytes
 * @function
 * @returns {Buffer} Value read
 */

BufferReader._configure();

var rpc = {};

var service = Service;

var util = requireMinimal();

// Extends EventEmitter
(Service.prototype = Object.create(util.EventEmitter.prototype)).constructor = Service;

/**
 * A service method callback as used by {@link rpc.ServiceMethod|ServiceMethod}.
 *
 * Differs from {@link RPCImplCallback} in that it is an actual callback of a service method which may not return `response = null`.
 * @typedef rpc.ServiceMethodCallback
 * @template TRes extends Message<TRes>
 * @type {function}
 * @param {Error|null} error Error, if any
 * @param {TRes} [response] Response message
 * @returns {undefined}
 */

/**
 * A service method part of a {@link rpc.Service} as created by {@link Service.create}.
 * @typedef rpc.ServiceMethod
 * @template TReq extends Message<TReq>
 * @template TRes extends Message<TRes>
 * @type {function}
 * @param {TReq|Properties<TReq>} request Request message or plain object
 * @param {rpc.ServiceMethodCallback<TRes>} [callback] Node-style callback called with the error, if any, and the response message
 * @returns {Promise<Message<TRes>>} Promise if `callback` has been omitted, otherwise `undefined`
 */

/**
 * Constructs a new RPC service instance.
 * @classdesc An RPC service as returned by {@link Service#create}.
 * @exports rpc.Service
 * @extends util.EventEmitter
 * @constructor
 * @param {RPCImpl} rpcImpl RPC implementation
 * @param {boolean} [requestDelimited=false] Whether requests are length-delimited
 * @param {boolean} [responseDelimited=false] Whether responses are length-delimited
 */
function Service(rpcImpl, requestDelimited, responseDelimited) {

    if (typeof rpcImpl !== "function")
        throw TypeError("rpcImpl must be a function");

    util.EventEmitter.call(this);

    /**
     * RPC implementation. Becomes `null` once the service is ended.
     * @type {RPCImpl|null}
     */
    this.rpcImpl = rpcImpl;

    /**
     * Whether requests are length-delimited.
     * @type {boolean}
     */
    this.requestDelimited = Boolean(requestDelimited);

    /**
     * Whether responses are length-delimited.
     * @type {boolean}
     */
    this.responseDelimited = Boolean(responseDelimited);
}

/**
 * Calls a service method through {@link rpc.Service#rpcImpl|rpcImpl}.
 * @param {Method|rpc.ServiceMethod<TReq,TRes>} method Reflected or static method
 * @param {Constructor<TReq>} requestCtor Request constructor
 * @param {Constructor<TRes>} responseCtor Response constructor
 * @param {TReq|Properties<TReq>} request Request message or plain object
 * @param {rpc.ServiceMethodCallback<TRes>} callback Service callback
 * @returns {undefined}
 * @template TReq extends Message<TReq>
 * @template TRes extends Message<TRes>
 */
Service.prototype.rpcCall = function rpcCall(method, requestCtor, responseCtor, request, callback) {

    if (!request)
        throw TypeError("request must be specified");

    var self = this;
    if (!callback)
        return util.asPromise(rpcCall, self, method, requestCtor, responseCtor, request);

    if (!self.rpcImpl) {
        setTimeout(function() { callback(Error("already ended")); }, 0);
        return undefined;
    }

    try {
        return self.rpcImpl(
            method,
            requestCtor[self.requestDelimited ? "encodeDelimited" : "encode"](request).finish(),
            function rpcCallback(err, response) {

                if (err) {
                    self.emit("error", err, method);
                    return callback(err);
                }

                if (response === null) {
                    self.end(/* endedByRPC */ true);
                    return undefined;
                }

                if (!(response instanceof responseCtor)) {
                    try {
                        response = responseCtor[self.responseDelimited ? "decodeDelimited" : "decode"](response);
                    } catch (err) {
                        self.emit("error", err, method);
                        return callback(err);
                    }
                }

                self.emit("data", response, method);
                return callback(null, response);
            }
        );
    } catch (err) {
        self.emit("error", err, method);
        setTimeout(function() { callback(err); }, 0);
        return undefined;
    }
};

/**
 * Ends this service and emits the `end` event.
 * @param {boolean} [endedByRPC=false] Whether the service has been ended by the RPC implementation.
 * @returns {rpc.Service} `this`
 */
Service.prototype.end = function end(endedByRPC) {
    if (this.rpcImpl) {
        if (!endedByRPC) // signal end to rpcImpl
            this.rpcImpl(null, null, null);
        this.rpcImpl = null;
        this.emit("end").off();
    }
    return this;
};

(function (exports) {

	/**
	 * Streaming RPC helpers.
	 * @namespace
	 */
	var rpc = exports;

	/**
	 * RPC implementation passed to {@link Service#create} performing a service request on network level, i.e. by utilizing http requests or websockets.
	 * @typedef RPCImpl
	 * @type {function}
	 * @param {Method|rpc.ServiceMethod<Message<{}>,Message<{}>>} method Reflected or static method being called
	 * @param {Uint8Array} requestData Request data
	 * @param {RPCImplCallback} callback Callback function
	 * @returns {undefined}
	 * @example
	 * function rpcImpl(method, requestData, callback) {
	 *     if (protobuf.util.lcFirst(method.name) !== "myMethod") // compatible with static code
	 *         throw Error("no such method");
	 *     asynchronouslyObtainAResponse(requestData, function(err, responseData) {
	 *         callback(err, responseData);
	 *     });
	 * }
	 */

	/**
	 * Node-style callback as used by {@link RPCImpl}.
	 * @typedef RPCImplCallback
	 * @type {function}
	 * @param {Error|null} error Error, if any, otherwise `null`
	 * @param {Uint8Array|null} [response] Response data or `null` to signal end of stream, if there hasn't been an error
	 * @returns {undefined}
	 */

	rpc.Service = service; 
} (rpc));

var roots = {};

(function (exports) {
	var protobuf = exports;

	/**
	 * Build type, one of `"full"`, `"light"` or `"minimal"`.
	 * @name build
	 * @type {string}
	 * @const
	 */
	protobuf.build = "minimal";

	// Serialization
	protobuf.Writer       = writer;
	protobuf.BufferWriter = writer_buffer;
	protobuf.Reader       = reader;
	protobuf.BufferReader = reader_buffer;

	// Utility
	protobuf.util         = requireMinimal();
	protobuf.rpc          = rpc;
	protobuf.roots        = roots;
	protobuf.configure    = configure;

	/* istanbul ignore next */
	/**
	 * Reconfigures the library according to the environment.
	 * @returns {undefined}
	 */
	function configure() {
	    protobuf.util._configure();
	    protobuf.Writer._configure(protobuf.BufferWriter);
	    protobuf.Reader._configure(protobuf.BufferReader);
	}

	// Set up buffer utility according to the environment
	configure(); 
} (indexMinimal));

var minimal = indexMinimal;

/*eslint-disable block-scoped-var, id-length, no-control-regex, no-magic-numbers, no-prototype-builtins, no-redeclare, no-shadow, no-var, sort-vars*/

const $util = minimal.util;

const $root = minimal.roots["default"] || (minimal.roots["default"] = {});

const gdm = $root.gdm = (() => {

    const gdm = {};

    gdm.DataType = (function() {
        const valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "unknownType"] = 0;
        values[valuesById[1] = "hello"] = 1;
        values[valuesById[2] = "event"] = 2;
        values[valuesById[3] = "eventAck"] = 3;
        values[valuesById[4] = "request"] = 4;
        values[valuesById[5] = "response"] = 5;
        values[valuesById[6] = "poll"] = 6;
        values[valuesById[7] = "pollResponse"] = 7;
        return values;
    })();

    gdm.Data = (function() {

        function Data(properties) {
            this.pollResponse = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        Data.prototype.type = 0;
        Data.prototype.hello = null;
        Data.prototype.request = null;
        Data.prototype.response = null;
        Data.prototype.event = null;
        Data.prototype.pollResponse = $util.emptyArray;

        let $oneOfFields;

        Object.defineProperty(Data.prototype, "_hello", {
            get: $util.oneOfGetter($oneOfFields = ["hello"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(Data.prototype, "_request", {
            get: $util.oneOfGetter($oneOfFields = ["request"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(Data.prototype, "_response", {
            get: $util.oneOfGetter($oneOfFields = ["response"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(Data.prototype, "_event", {
            get: $util.oneOfGetter($oneOfFields = ["event"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Data.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.Data)
                return object;
            let message = new $root.gdm.Data();
            switch (object.type) {
            default:
                if (typeof object.type === "number") {
                    message.type = object.type;
                    break;
                }
                break;
            case "unknownType":
            case 0:
                message.type = 0;
                break;
            case "hello":
            case 1:
                message.type = 1;
                break;
            case "event":
            case 2:
                message.type = 2;
                break;
            case "eventAck":
            case 3:
                message.type = 3;
                break;
            case "request":
            case 4:
                message.type = 4;
                break;
            case "response":
            case 5:
                message.type = 5;
                break;
            case "poll":
            case 6:
                message.type = 6;
                break;
            case "pollResponse":
            case 7:
                message.type = 7;
                break;
            }
            if (object.hello != null) {
                if (typeof object.hello !== "object")
                    throw TypeError(".gdm.Data.hello: object expected");
                message.hello = $root.gdm.HelloData.fromObject(object.hello);
            }
            if (object.request != null) {
                if (typeof object.request !== "object")
                    throw TypeError(".gdm.Data.request: object expected");
                message.request = $root.gdm.RequestData.fromObject(object.request);
            }
            if (object.response != null) {
                if (typeof object.response !== "object")
                    throw TypeError(".gdm.Data.response: object expected");
                message.response = $root.gdm.ResponseData.fromObject(object.response);
            }
            if (object.event != null) {
                if (typeof object.event !== "object")
                    throw TypeError(".gdm.Data.event: object expected");
                message.event = $root.gdm.EventData.fromObject(object.event);
            }
            if (object.pollResponse) {
                if (!Array.isArray(object.pollResponse))
                    throw TypeError(".gdm.Data.pollResponse: array expected");
                message.pollResponse = [];
                for (let i = 0; i < object.pollResponse.length; ++i) {
                    if (typeof object.pollResponse[i] !== "object")
                        throw TypeError(".gdm.Data.pollResponse: object expected");
                    message.pollResponse[i] = $root.gdm.EventData.fromObject(object.pollResponse[i]);
                }
            }
            return message;
        };

        Data.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.pollResponse = [];
            if (options.defaults)
                object.type = options.enums === String ? "unknownType" : 0;
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = options.enums === String ? $root.gdm.DataType[message.type] === undefined ? message.type : $root.gdm.DataType[message.type] : message.type;
            if (message.hello != null && message.hasOwnProperty("hello")) {
                object.hello = $root.gdm.HelloData.toObject(message.hello, options);
                if (options.oneofs)
                    object._hello = "hello";
            }
            if (message.request != null && message.hasOwnProperty("request")) {
                object.request = $root.gdm.RequestData.toObject(message.request, options);
                if (options.oneofs)
                    object._request = "request";
            }
            if (message.response != null && message.hasOwnProperty("response")) {
                object.response = $root.gdm.ResponseData.toObject(message.response, options);
                if (options.oneofs)
                    object._response = "response";
            }
            if (message.event != null && message.hasOwnProperty("event")) {
                object.event = $root.gdm.EventData.toObject(message.event, options);
                if (options.oneofs)
                    object._event = "event";
            }
            if (message.pollResponse && message.pollResponse.length) {
                object.pollResponse = [];
                for (let j = 0; j < message.pollResponse.length; ++j)
                    object.pollResponse[j] = $root.gdm.EventData.toObject(message.pollResponse[j], options);
            }
            return object;
        };

        Data.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return Data;
    })();

    gdm.HelloData = (function() {

        function HelloData(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        HelloData.prototype.version = 0;

        HelloData.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.HelloData)
                return object;
            let message = new $root.gdm.HelloData();
            if (object.version != null)
                message.version = object.version >>> 0;
            return message;
        };

        HelloData.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.version = 0;
            if (message.version != null && message.hasOwnProperty("version"))
                object.version = message.version;
            return object;
        };

        HelloData.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return HelloData;
    })();

    gdm.RequestType = (function() {
        const valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "unknownRequest"] = 0;
        values[valuesById[1] = "updateBrokersList"] = 1;
        values[valuesById[2] = "composeAuthenticationView"] = 2;
        values[valuesById[3] = "uiLayoutCapabilities"] = 3;
        values[valuesById[4] = "changeStage"] = 4;
        return values;
    })();

    gdm.Requests = (function() {

        function Requests(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        Requests.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.Requests)
                return object;
            return new $root.gdm.Requests();
        };

        Requests.toObject = function toObject() {
            return {};
        };

        Requests.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        Requests.UiLayoutCapabilities = (function() {

            function UiLayoutCapabilities(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            UiLayoutCapabilities.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Requests.UiLayoutCapabilities)
                    return object;
                return new $root.gdm.Requests.UiLayoutCapabilities();
            };

            UiLayoutCapabilities.toObject = function toObject() {
                return {};
            };

            UiLayoutCapabilities.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return UiLayoutCapabilities;
        })();

        Requests.ChangeStage = (function() {

            function ChangeStage(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            ChangeStage.prototype.stage = 0;

            ChangeStage.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Requests.ChangeStage)
                    return object;
                let message = new $root.gdm.Requests.ChangeStage();
                switch (object.stage) {
                default:
                    if (typeof object.stage === "number") {
                        message.stage = object.stage;
                        break;
                    }
                    break;
                case "userSelection":
                case 0:
                    message.stage = 0;
                    break;
                case "brokerSelection":
                case 1:
                    message.stage = 1;
                    break;
                case "authModeSelection":
                case 2:
                    message.stage = 2;
                    break;
                case "challenge":
                case 3:
                    message.stage = 3;
                    break;
                }
                return message;
            };

            ChangeStage.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.stage = options.enums === String ? "userSelection" : 0;
                if (message.stage != null && message.hasOwnProperty("stage"))
                    object.stage = options.enums === String ? $root.pam.Stage[message.stage] === undefined ? message.stage : $root.pam.Stage[message.stage] : message.stage;
                return object;
            };

            ChangeStage.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return ChangeStage;
        })();

        return Requests;
    })();

    gdm.RequestData = (function() {

        function RequestData(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        RequestData.prototype.type = 0;
        RequestData.prototype.uiLayoutCapabilities = null;
        RequestData.prototype.changeStage = null;

        let $oneOfFields;

        Object.defineProperty(RequestData.prototype, "data", {
            get: $util.oneOfGetter($oneOfFields = ["uiLayoutCapabilities", "changeStage"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        RequestData.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.RequestData)
                return object;
            let message = new $root.gdm.RequestData();
            switch (object.type) {
            default:
                if (typeof object.type === "number") {
                    message.type = object.type;
                    break;
                }
                break;
            case "unknownRequest":
            case 0:
                message.type = 0;
                break;
            case "updateBrokersList":
            case 1:
                message.type = 1;
                break;
            case "composeAuthenticationView":
            case 2:
                message.type = 2;
                break;
            case "uiLayoutCapabilities":
            case 3:
                message.type = 3;
                break;
            case "changeStage":
            case 4:
                message.type = 4;
                break;
            }
            if (object.uiLayoutCapabilities != null) {
                if (typeof object.uiLayoutCapabilities !== "object")
                    throw TypeError(".gdm.RequestData.uiLayoutCapabilities: object expected");
                message.uiLayoutCapabilities = $root.gdm.Requests.UiLayoutCapabilities.fromObject(object.uiLayoutCapabilities);
            }
            if (object.changeStage != null) {
                if (typeof object.changeStage !== "object")
                    throw TypeError(".gdm.RequestData.changeStage: object expected");
                message.changeStage = $root.gdm.Requests.ChangeStage.fromObject(object.changeStage);
            }
            return message;
        };

        RequestData.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.type = options.enums === String ? "unknownRequest" : 0;
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = options.enums === String ? $root.gdm.RequestType[message.type] === undefined ? message.type : $root.gdm.RequestType[message.type] : message.type;
            if (message.uiLayoutCapabilities != null && message.hasOwnProperty("uiLayoutCapabilities")) {
                object.uiLayoutCapabilities = $root.gdm.Requests.UiLayoutCapabilities.toObject(message.uiLayoutCapabilities, options);
                if (options.oneofs)
                    object.data = "uiLayoutCapabilities";
            }
            if (message.changeStage != null && message.hasOwnProperty("changeStage")) {
                object.changeStage = $root.gdm.Requests.ChangeStage.toObject(message.changeStage, options);
                if (options.oneofs)
                    object.data = "changeStage";
            }
            return object;
        };

        RequestData.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return RequestData;
    })();

    gdm.Responses = (function() {

        function Responses(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        Responses.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.Responses)
                return object;
            return new $root.gdm.Responses();
        };

        Responses.toObject = function toObject() {
            return {};
        };

        Responses.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        Responses.Ack = (function() {

            function Ack(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            Ack.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Responses.Ack)
                    return object;
                return new $root.gdm.Responses.Ack();
            };

            Ack.toObject = function toObject() {
                return {};
            };

            Ack.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return Ack;
        })();

        Responses.UiLayoutCapabilities = (function() {

            function UiLayoutCapabilities(properties) {
                this.supportedUiLayouts = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            UiLayoutCapabilities.prototype.supportedUiLayouts = $util.emptyArray;

            UiLayoutCapabilities.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Responses.UiLayoutCapabilities)
                    return object;
                let message = new $root.gdm.Responses.UiLayoutCapabilities();
                if (object.supportedUiLayouts) {
                    if (!Array.isArray(object.supportedUiLayouts))
                        throw TypeError(".gdm.Responses.UiLayoutCapabilities.supportedUiLayouts: array expected");
                    message.supportedUiLayouts = [];
                    for (let i = 0; i < object.supportedUiLayouts.length; ++i) {
                        if (typeof object.supportedUiLayouts[i] !== "object")
                            throw TypeError(".gdm.Responses.UiLayoutCapabilities.supportedUiLayouts: object expected");
                        message.supportedUiLayouts[i] = $root.authd.UILayout.fromObject(object.supportedUiLayouts[i]);
                    }
                }
                return message;
            };

            UiLayoutCapabilities.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults)
                    object.supportedUiLayouts = [];
                if (message.supportedUiLayouts && message.supportedUiLayouts.length) {
                    object.supportedUiLayouts = [];
                    for (let j = 0; j < message.supportedUiLayouts.length; ++j)
                        object.supportedUiLayouts[j] = $root.authd.UILayout.toObject(message.supportedUiLayouts[j], options);
                }
                return object;
            };

            UiLayoutCapabilities.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return UiLayoutCapabilities;
        })();

        return Responses;
    })();

    gdm.ResponseData = (function() {

        function ResponseData(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        ResponseData.prototype.type = 0;
        ResponseData.prototype.ack = null;
        ResponseData.prototype.uiLayoutCapabilities = null;

        let $oneOfFields;

        Object.defineProperty(ResponseData.prototype, "data", {
            get: $util.oneOfGetter($oneOfFields = ["ack", "uiLayoutCapabilities"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        ResponseData.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.ResponseData)
                return object;
            let message = new $root.gdm.ResponseData();
            switch (object.type) {
            default:
                if (typeof object.type === "number") {
                    message.type = object.type;
                    break;
                }
                break;
            case "unknownRequest":
            case 0:
                message.type = 0;
                break;
            case "updateBrokersList":
            case 1:
                message.type = 1;
                break;
            case "composeAuthenticationView":
            case 2:
                message.type = 2;
                break;
            case "uiLayoutCapabilities":
            case 3:
                message.type = 3;
                break;
            case "changeStage":
            case 4:
                message.type = 4;
                break;
            }
            if (object.ack != null) {
                if (typeof object.ack !== "object")
                    throw TypeError(".gdm.ResponseData.ack: object expected");
                message.ack = $root.gdm.Responses.Ack.fromObject(object.ack);
            }
            if (object.uiLayoutCapabilities != null) {
                if (typeof object.uiLayoutCapabilities !== "object")
                    throw TypeError(".gdm.ResponseData.uiLayoutCapabilities: object expected");
                message.uiLayoutCapabilities = $root.gdm.Responses.UiLayoutCapabilities.fromObject(object.uiLayoutCapabilities);
            }
            return message;
        };

        ResponseData.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.type = options.enums === String ? "unknownRequest" : 0;
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = options.enums === String ? $root.gdm.RequestType[message.type] === undefined ? message.type : $root.gdm.RequestType[message.type] : message.type;
            if (message.ack != null && message.hasOwnProperty("ack")) {
                object.ack = $root.gdm.Responses.Ack.toObject(message.ack, options);
                if (options.oneofs)
                    object.data = "ack";
            }
            if (message.uiLayoutCapabilities != null && message.hasOwnProperty("uiLayoutCapabilities")) {
                object.uiLayoutCapabilities = $root.gdm.Responses.UiLayoutCapabilities.toObject(message.uiLayoutCapabilities, options);
                if (options.oneofs)
                    object.data = "uiLayoutCapabilities";
            }
            return object;
        };

        ResponseData.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return ResponseData;
    })();

    gdm.EventType = (function() {
        const valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "unknownEvent"] = 0;
        values[valuesById[1] = "userSelected"] = 1;
        values[valuesById[2] = "brokersReceived"] = 2;
        values[valuesById[3] = "brokerSelected"] = 3;
        values[valuesById[4] = "authModesReceived"] = 4;
        values[valuesById[5] = "authModeSelected"] = 5;
        values[valuesById[6] = "reselectAuthMode"] = 6;
        values[valuesById[7] = "authEvent"] = 7;
        values[valuesById[8] = "uiLayoutReceived"] = 8;
        values[valuesById[9] = "startAuthentication"] = 9;
        values[valuesById[10] = "isAuthenticatedRequested"] = 10;
        values[valuesById[11] = "isAuthenticatedCancelled"] = 11;
        values[valuesById[12] = "stageChanged"] = 12;
        return values;
    })();

    gdm.Events = (function() {

        function Events(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        Events.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.Events)
                return object;
            return new $root.gdm.Events();
        };

        Events.toObject = function toObject() {
            return {};
        };

        Events.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        Events.BrokersReceived = (function() {

            function BrokersReceived(properties) {
                this.brokersInfos = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            BrokersReceived.prototype.brokersInfos = $util.emptyArray;

            BrokersReceived.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.BrokersReceived)
                    return object;
                let message = new $root.gdm.Events.BrokersReceived();
                if (object.brokersInfos) {
                    if (!Array.isArray(object.brokersInfos))
                        throw TypeError(".gdm.Events.BrokersReceived.brokersInfos: array expected");
                    message.brokersInfos = [];
                    for (let i = 0; i < object.brokersInfos.length; ++i) {
                        if (typeof object.brokersInfos[i] !== "object")
                            throw TypeError(".gdm.Events.BrokersReceived.brokersInfos: object expected");
                        message.brokersInfos[i] = $root.authd.ABResponse.BrokerInfo.fromObject(object.brokersInfos[i]);
                    }
                }
                return message;
            };

            BrokersReceived.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults)
                    object.brokersInfos = [];
                if (message.brokersInfos && message.brokersInfos.length) {
                    object.brokersInfos = [];
                    for (let j = 0; j < message.brokersInfos.length; ++j)
                        object.brokersInfos[j] = $root.authd.ABResponse.BrokerInfo.toObject(message.brokersInfos[j], options);
                }
                return object;
            };

            BrokersReceived.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return BrokersReceived;
        })();

        Events.BrokerSelected = (function() {

            function BrokerSelected(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            BrokerSelected.prototype.brokerId = "";

            BrokerSelected.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.BrokerSelected)
                    return object;
                let message = new $root.gdm.Events.BrokerSelected();
                if (object.brokerId != null)
                    message.brokerId = String(object.brokerId);
                return message;
            };

            BrokerSelected.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.brokerId = "";
                if (message.brokerId != null && message.hasOwnProperty("brokerId"))
                    object.brokerId = message.brokerId;
                return object;
            };

            BrokerSelected.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return BrokerSelected;
        })();

        Events.UserSelected = (function() {

            function UserSelected(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            UserSelected.prototype.userId = "";

            UserSelected.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.UserSelected)
                    return object;
                let message = new $root.gdm.Events.UserSelected();
                if (object.userId != null)
                    message.userId = String(object.userId);
                return message;
            };

            UserSelected.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.userId = "";
                if (message.userId != null && message.hasOwnProperty("userId"))
                    object.userId = message.userId;
                return object;
            };

            UserSelected.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return UserSelected;
        })();

        Events.StartAuthentication = (function() {

            function StartAuthentication(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            StartAuthentication.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.StartAuthentication)
                    return object;
                return new $root.gdm.Events.StartAuthentication();
            };

            StartAuthentication.toObject = function toObject() {
                return {};
            };

            StartAuthentication.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return StartAuthentication;
        })();

        Events.AuthModesReceived = (function() {

            function AuthModesReceived(properties) {
                this.authModes = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            AuthModesReceived.prototype.authModes = $util.emptyArray;

            AuthModesReceived.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.AuthModesReceived)
                    return object;
                let message = new $root.gdm.Events.AuthModesReceived();
                if (object.authModes) {
                    if (!Array.isArray(object.authModes))
                        throw TypeError(".gdm.Events.AuthModesReceived.authModes: array expected");
                    message.authModes = [];
                    for (let i = 0; i < object.authModes.length; ++i) {
                        if (typeof object.authModes[i] !== "object")
                            throw TypeError(".gdm.Events.AuthModesReceived.authModes: object expected");
                        message.authModes[i] = $root.authd.GAMResponse.AuthenticationMode.fromObject(object.authModes[i]);
                    }
                }
                return message;
            };

            AuthModesReceived.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults)
                    object.authModes = [];
                if (message.authModes && message.authModes.length) {
                    object.authModes = [];
                    for (let j = 0; j < message.authModes.length; ++j)
                        object.authModes[j] = $root.authd.GAMResponse.AuthenticationMode.toObject(message.authModes[j], options);
                }
                return object;
            };

            AuthModesReceived.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return AuthModesReceived;
        })();

        Events.AuthModeSelected = (function() {

            function AuthModeSelected(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            AuthModeSelected.prototype.authModeId = "";

            AuthModeSelected.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.AuthModeSelected)
                    return object;
                let message = new $root.gdm.Events.AuthModeSelected();
                if (object.authModeId != null)
                    message.authModeId = String(object.authModeId);
                return message;
            };

            AuthModeSelected.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.authModeId = "";
                if (message.authModeId != null && message.hasOwnProperty("authModeId"))
                    object.authModeId = message.authModeId;
                return object;
            };

            AuthModeSelected.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return AuthModeSelected;
        })();

        Events.AuthEvent = (function() {

            function AuthEvent(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            AuthEvent.prototype.response = null;

            AuthEvent.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.AuthEvent)
                    return object;
                let message = new $root.gdm.Events.AuthEvent();
                if (object.response != null) {
                    if (typeof object.response !== "object")
                        throw TypeError(".gdm.Events.AuthEvent.response: object expected");
                    message.response = $root.authd.IAResponse.fromObject(object.response);
                }
                return message;
            };

            AuthEvent.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.response = null;
                if (message.response != null && message.hasOwnProperty("response"))
                    object.response = $root.authd.IAResponse.toObject(message.response, options);
                return object;
            };

            AuthEvent.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return AuthEvent;
        })();

        Events.ReselectAuthMode = (function() {

            function ReselectAuthMode(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            ReselectAuthMode.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.ReselectAuthMode)
                    return object;
                return new $root.gdm.Events.ReselectAuthMode();
            };

            ReselectAuthMode.toObject = function toObject() {
                return {};
            };

            ReselectAuthMode.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return ReselectAuthMode;
        })();

        Events.IsAuthenticatedRequested = (function() {

            function IsAuthenticatedRequested(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            IsAuthenticatedRequested.prototype.authenticationData = null;

            IsAuthenticatedRequested.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.IsAuthenticatedRequested)
                    return object;
                let message = new $root.gdm.Events.IsAuthenticatedRequested();
                if (object.authenticationData != null) {
                    if (typeof object.authenticationData !== "object")
                        throw TypeError(".gdm.Events.IsAuthenticatedRequested.authenticationData: object expected");
                    message.authenticationData = $root.authd.IARequest.AuthenticationData.fromObject(object.authenticationData);
                }
                return message;
            };

            IsAuthenticatedRequested.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.authenticationData = null;
                if (message.authenticationData != null && message.hasOwnProperty("authenticationData"))
                    object.authenticationData = $root.authd.IARequest.AuthenticationData.toObject(message.authenticationData, options);
                return object;
            };

            IsAuthenticatedRequested.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return IsAuthenticatedRequested;
        })();

        Events.IsAuthenticatedCancelled = (function() {

            function IsAuthenticatedCancelled(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            IsAuthenticatedCancelled.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.IsAuthenticatedCancelled)
                    return object;
                return new $root.gdm.Events.IsAuthenticatedCancelled();
            };

            IsAuthenticatedCancelled.toObject = function toObject() {
                return {};
            };

            IsAuthenticatedCancelled.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return IsAuthenticatedCancelled;
        })();

        Events.StageChanged = (function() {

            function StageChanged(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            StageChanged.prototype.stage = 0;

            StageChanged.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.StageChanged)
                    return object;
                let message = new $root.gdm.Events.StageChanged();
                switch (object.stage) {
                default:
                    if (typeof object.stage === "number") {
                        message.stage = object.stage;
                        break;
                    }
                    break;
                case "userSelection":
                case 0:
                    message.stage = 0;
                    break;
                case "brokerSelection":
                case 1:
                    message.stage = 1;
                    break;
                case "authModeSelection":
                case 2:
                    message.stage = 2;
                    break;
                case "challenge":
                case 3:
                    message.stage = 3;
                    break;
                }
                return message;
            };

            StageChanged.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.stage = options.enums === String ? "userSelection" : 0;
                if (message.stage != null && message.hasOwnProperty("stage"))
                    object.stage = options.enums === String ? $root.pam.Stage[message.stage] === undefined ? message.stage : $root.pam.Stage[message.stage] : message.stage;
                return object;
            };

            StageChanged.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return StageChanged;
        })();

        Events.UiLayoutReceived = (function() {

            function UiLayoutReceived(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            UiLayoutReceived.prototype.uiLayout = null;

            UiLayoutReceived.fromObject = function fromObject(object) {
                if (object instanceof $root.gdm.Events.UiLayoutReceived)
                    return object;
                let message = new $root.gdm.Events.UiLayoutReceived();
                if (object.uiLayout != null) {
                    if (typeof object.uiLayout !== "object")
                        throw TypeError(".gdm.Events.UiLayoutReceived.uiLayout: object expected");
                    message.uiLayout = $root.authd.UILayout.fromObject(object.uiLayout);
                }
                return message;
            };

            UiLayoutReceived.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.uiLayout = null;
                if (message.uiLayout != null && message.hasOwnProperty("uiLayout"))
                    object.uiLayout = $root.authd.UILayout.toObject(message.uiLayout, options);
                return object;
            };

            UiLayoutReceived.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return UiLayoutReceived;
        })();

        return Events;
    })();

    gdm.EventData = (function() {

        function EventData(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        EventData.prototype.type = 0;
        EventData.prototype.brokersReceived = null;
        EventData.prototype.brokerSelected = null;
        EventData.prototype.authModesReceived = null;
        EventData.prototype.authModeSelected = null;
        EventData.prototype.isAuthenticatedRequested = null;
        EventData.prototype.stageChanged = null;
        EventData.prototype.uiLayoutReceived = null;
        EventData.prototype.authEvent = null;
        EventData.prototype.reselectAuthMode = null;
        EventData.prototype.startAuthentication = null;
        EventData.prototype.userSelected = null;
        EventData.prototype.isAuthenticatedCancelled = null;

        let $oneOfFields;

        Object.defineProperty(EventData.prototype, "data", {
            get: $util.oneOfGetter($oneOfFields = ["brokersReceived", "brokerSelected", "authModesReceived", "authModeSelected", "isAuthenticatedRequested", "stageChanged", "uiLayoutReceived", "authEvent", "reselectAuthMode", "startAuthentication", "userSelected", "isAuthenticatedCancelled"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        EventData.fromObject = function fromObject(object) {
            if (object instanceof $root.gdm.EventData)
                return object;
            let message = new $root.gdm.EventData();
            switch (object.type) {
            default:
                if (typeof object.type === "number") {
                    message.type = object.type;
                    break;
                }
                break;
            case "unknownEvent":
            case 0:
                message.type = 0;
                break;
            case "userSelected":
            case 1:
                message.type = 1;
                break;
            case "brokersReceived":
            case 2:
                message.type = 2;
                break;
            case "brokerSelected":
            case 3:
                message.type = 3;
                break;
            case "authModesReceived":
            case 4:
                message.type = 4;
                break;
            case "authModeSelected":
            case 5:
                message.type = 5;
                break;
            case "reselectAuthMode":
            case 6:
                message.type = 6;
                break;
            case "authEvent":
            case 7:
                message.type = 7;
                break;
            case "uiLayoutReceived":
            case 8:
                message.type = 8;
                break;
            case "startAuthentication":
            case 9:
                message.type = 9;
                break;
            case "isAuthenticatedRequested":
            case 10:
                message.type = 10;
                break;
            case "isAuthenticatedCancelled":
            case 11:
                message.type = 11;
                break;
            case "stageChanged":
            case 12:
                message.type = 12;
                break;
            }
            if (object.brokersReceived != null) {
                if (typeof object.brokersReceived !== "object")
                    throw TypeError(".gdm.EventData.brokersReceived: object expected");
                message.brokersReceived = $root.gdm.Events.BrokersReceived.fromObject(object.brokersReceived);
            }
            if (object.brokerSelected != null) {
                if (typeof object.brokerSelected !== "object")
                    throw TypeError(".gdm.EventData.brokerSelected: object expected");
                message.brokerSelected = $root.gdm.Events.BrokerSelected.fromObject(object.brokerSelected);
            }
            if (object.authModesReceived != null) {
                if (typeof object.authModesReceived !== "object")
                    throw TypeError(".gdm.EventData.authModesReceived: object expected");
                message.authModesReceived = $root.gdm.Events.AuthModesReceived.fromObject(object.authModesReceived);
            }
            if (object.authModeSelected != null) {
                if (typeof object.authModeSelected !== "object")
                    throw TypeError(".gdm.EventData.authModeSelected: object expected");
                message.authModeSelected = $root.gdm.Events.AuthModeSelected.fromObject(object.authModeSelected);
            }
            if (object.isAuthenticatedRequested != null) {
                if (typeof object.isAuthenticatedRequested !== "object")
                    throw TypeError(".gdm.EventData.isAuthenticatedRequested: object expected");
                message.isAuthenticatedRequested = $root.gdm.Events.IsAuthenticatedRequested.fromObject(object.isAuthenticatedRequested);
            }
            if (object.stageChanged != null) {
                if (typeof object.stageChanged !== "object")
                    throw TypeError(".gdm.EventData.stageChanged: object expected");
                message.stageChanged = $root.gdm.Events.StageChanged.fromObject(object.stageChanged);
            }
            if (object.uiLayoutReceived != null) {
                if (typeof object.uiLayoutReceived !== "object")
                    throw TypeError(".gdm.EventData.uiLayoutReceived: object expected");
                message.uiLayoutReceived = $root.gdm.Events.UiLayoutReceived.fromObject(object.uiLayoutReceived);
            }
            if (object.authEvent != null) {
                if (typeof object.authEvent !== "object")
                    throw TypeError(".gdm.EventData.authEvent: object expected");
                message.authEvent = $root.gdm.Events.AuthEvent.fromObject(object.authEvent);
            }
            if (object.reselectAuthMode != null) {
                if (typeof object.reselectAuthMode !== "object")
                    throw TypeError(".gdm.EventData.reselectAuthMode: object expected");
                message.reselectAuthMode = $root.gdm.Events.ReselectAuthMode.fromObject(object.reselectAuthMode);
            }
            if (object.startAuthentication != null) {
                if (typeof object.startAuthentication !== "object")
                    throw TypeError(".gdm.EventData.startAuthentication: object expected");
                message.startAuthentication = $root.gdm.Events.StartAuthentication.fromObject(object.startAuthentication);
            }
            if (object.userSelected != null) {
                if (typeof object.userSelected !== "object")
                    throw TypeError(".gdm.EventData.userSelected: object expected");
                message.userSelected = $root.gdm.Events.UserSelected.fromObject(object.userSelected);
            }
            if (object.isAuthenticatedCancelled != null) {
                if (typeof object.isAuthenticatedCancelled !== "object")
                    throw TypeError(".gdm.EventData.isAuthenticatedCancelled: object expected");
                message.isAuthenticatedCancelled = $root.gdm.Events.IsAuthenticatedCancelled.fromObject(object.isAuthenticatedCancelled);
            }
            return message;
        };

        EventData.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.type = options.enums === String ? "unknownEvent" : 0;
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = options.enums === String ? $root.gdm.EventType[message.type] === undefined ? message.type : $root.gdm.EventType[message.type] : message.type;
            if (message.brokersReceived != null && message.hasOwnProperty("brokersReceived")) {
                object.brokersReceived = $root.gdm.Events.BrokersReceived.toObject(message.brokersReceived, options);
                if (options.oneofs)
                    object.data = "brokersReceived";
            }
            if (message.brokerSelected != null && message.hasOwnProperty("brokerSelected")) {
                object.brokerSelected = $root.gdm.Events.BrokerSelected.toObject(message.brokerSelected, options);
                if (options.oneofs)
                    object.data = "brokerSelected";
            }
            if (message.authModesReceived != null && message.hasOwnProperty("authModesReceived")) {
                object.authModesReceived = $root.gdm.Events.AuthModesReceived.toObject(message.authModesReceived, options);
                if (options.oneofs)
                    object.data = "authModesReceived";
            }
            if (message.authModeSelected != null && message.hasOwnProperty("authModeSelected")) {
                object.authModeSelected = $root.gdm.Events.AuthModeSelected.toObject(message.authModeSelected, options);
                if (options.oneofs)
                    object.data = "authModeSelected";
            }
            if (message.isAuthenticatedRequested != null && message.hasOwnProperty("isAuthenticatedRequested")) {
                object.isAuthenticatedRequested = $root.gdm.Events.IsAuthenticatedRequested.toObject(message.isAuthenticatedRequested, options);
                if (options.oneofs)
                    object.data = "isAuthenticatedRequested";
            }
            if (message.stageChanged != null && message.hasOwnProperty("stageChanged")) {
                object.stageChanged = $root.gdm.Events.StageChanged.toObject(message.stageChanged, options);
                if (options.oneofs)
                    object.data = "stageChanged";
            }
            if (message.uiLayoutReceived != null && message.hasOwnProperty("uiLayoutReceived")) {
                object.uiLayoutReceived = $root.gdm.Events.UiLayoutReceived.toObject(message.uiLayoutReceived, options);
                if (options.oneofs)
                    object.data = "uiLayoutReceived";
            }
            if (message.authEvent != null && message.hasOwnProperty("authEvent")) {
                object.authEvent = $root.gdm.Events.AuthEvent.toObject(message.authEvent, options);
                if (options.oneofs)
                    object.data = "authEvent";
            }
            if (message.reselectAuthMode != null && message.hasOwnProperty("reselectAuthMode")) {
                object.reselectAuthMode = $root.gdm.Events.ReselectAuthMode.toObject(message.reselectAuthMode, options);
                if (options.oneofs)
                    object.data = "reselectAuthMode";
            }
            if (message.startAuthentication != null && message.hasOwnProperty("startAuthentication")) {
                object.startAuthentication = $root.gdm.Events.StartAuthentication.toObject(message.startAuthentication, options);
                if (options.oneofs)
                    object.data = "startAuthentication";
            }
            if (message.userSelected != null && message.hasOwnProperty("userSelected")) {
                object.userSelected = $root.gdm.Events.UserSelected.toObject(message.userSelected, options);
                if (options.oneofs)
                    object.data = "userSelected";
            }
            if (message.isAuthenticatedCancelled != null && message.hasOwnProperty("isAuthenticatedCancelled")) {
                object.isAuthenticatedCancelled = $root.gdm.Events.IsAuthenticatedCancelled.toObject(message.isAuthenticatedCancelled, options);
                if (options.oneofs)
                    object.data = "isAuthenticatedCancelled";
            }
            return object;
        };

        EventData.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return EventData;
    })();

    return gdm;
})();

const authd = $root.authd = (() => {

    const authd = {};

    authd.Empty = (function() {

        function Empty(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        Empty.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.Empty)
                return object;
            return new $root.authd.Empty();
        };

        Empty.toObject = function toObject() {
            return {};
        };

        Empty.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return Empty;
    })();

    authd.GPBRequest = (function() {

        function GPBRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GPBRequest.prototype.username = "";

        GPBRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GPBRequest)
                return object;
            let message = new $root.authd.GPBRequest();
            if (object.username != null)
                message.username = String(object.username);
            return message;
        };

        GPBRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.username = "";
            if (message.username != null && message.hasOwnProperty("username"))
                object.username = message.username;
            return object;
        };

        GPBRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GPBRequest;
    })();

    authd.GPBResponse = (function() {

        function GPBResponse(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GPBResponse.prototype.previousBroker = "";

        GPBResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GPBResponse)
                return object;
            let message = new $root.authd.GPBResponse();
            if (object.previousBroker != null)
                message.previousBroker = String(object.previousBroker);
            return message;
        };

        GPBResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.previousBroker = "";
            if (message.previousBroker != null && message.hasOwnProperty("previousBroker"))
                object.previousBroker = message.previousBroker;
            return object;
        };

        GPBResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GPBResponse;
    })();

    authd.ABResponse = (function() {

        function ABResponse(properties) {
            this.brokersInfos = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        ABResponse.prototype.brokersInfos = $util.emptyArray;

        ABResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.ABResponse)
                return object;
            let message = new $root.authd.ABResponse();
            if (object.brokersInfos) {
                if (!Array.isArray(object.brokersInfos))
                    throw TypeError(".authd.ABResponse.brokersInfos: array expected");
                message.brokersInfos = [];
                for (let i = 0; i < object.brokersInfos.length; ++i) {
                    if (typeof object.brokersInfos[i] !== "object")
                        throw TypeError(".authd.ABResponse.brokersInfos: object expected");
                    message.brokersInfos[i] = $root.authd.ABResponse.BrokerInfo.fromObject(object.brokersInfos[i]);
                }
            }
            return message;
        };

        ABResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.brokersInfos = [];
            if (message.brokersInfos && message.brokersInfos.length) {
                object.brokersInfos = [];
                for (let j = 0; j < message.brokersInfos.length; ++j)
                    object.brokersInfos[j] = $root.authd.ABResponse.BrokerInfo.toObject(message.brokersInfos[j], options);
            }
            return object;
        };

        ABResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        ABResponse.BrokerInfo = (function() {

            function BrokerInfo(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            BrokerInfo.prototype.id = "";
            BrokerInfo.prototype.name = "";
            BrokerInfo.prototype.brandIcon = null;

            let $oneOfFields;

            Object.defineProperty(BrokerInfo.prototype, "_brandIcon", {
                get: $util.oneOfGetter($oneOfFields = ["brandIcon"]),
                set: $util.oneOfSetter($oneOfFields)
            });

            BrokerInfo.fromObject = function fromObject(object) {
                if (object instanceof $root.authd.ABResponse.BrokerInfo)
                    return object;
                let message = new $root.authd.ABResponse.BrokerInfo();
                if (object.id != null)
                    message.id = String(object.id);
                if (object.name != null)
                    message.name = String(object.name);
                if (object.brandIcon != null)
                    message.brandIcon = String(object.brandIcon);
                return message;
            };

            BrokerInfo.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.id = "";
                    object.name = "";
                }
                if (message.id != null && message.hasOwnProperty("id"))
                    object.id = message.id;
                if (message.name != null && message.hasOwnProperty("name"))
                    object.name = message.name;
                if (message.brandIcon != null && message.hasOwnProperty("brandIcon")) {
                    object.brandIcon = message.brandIcon;
                    if (options.oneofs)
                        object._brandIcon = "brandIcon";
                }
                return object;
            };

            BrokerInfo.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return BrokerInfo;
        })();

        return ABResponse;
    })();

    authd.StringResponse = (function() {

        function StringResponse(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        StringResponse.prototype.msg = "";

        StringResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.StringResponse)
                return object;
            let message = new $root.authd.StringResponse();
            if (object.msg != null)
                message.msg = String(object.msg);
            return message;
        };

        StringResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.msg = "";
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = message.msg;
            return object;
        };

        StringResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return StringResponse;
    })();

    authd.SessionMode = (function() {
        const valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "UNDEFINED"] = 0;
        values[valuesById[1] = "LOGIN"] = 1;
        values[valuesById[2] = "CHANGE_PASSWORD"] = 2;
        return values;
    })();

    authd.SBRequest = (function() {

        function SBRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        SBRequest.prototype.brokerId = "";
        SBRequest.prototype.username = "";
        SBRequest.prototype.lang = "";
        SBRequest.prototype.mode = 0;

        SBRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.SBRequest)
                return object;
            let message = new $root.authd.SBRequest();
            if (object.brokerId != null)
                message.brokerId = String(object.brokerId);
            if (object.username != null)
                message.username = String(object.username);
            if (object.lang != null)
                message.lang = String(object.lang);
            switch (object.mode) {
            default:
                if (typeof object.mode === "number") {
                    message.mode = object.mode;
                    break;
                }
                break;
            case "UNDEFINED":
            case 0:
                message.mode = 0;
                break;
            case "LOGIN":
            case 1:
                message.mode = 1;
                break;
            case "CHANGE_PASSWORD":
            case 2:
                message.mode = 2;
                break;
            }
            return message;
        };

        SBRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.brokerId = "";
                object.username = "";
                object.lang = "";
                object.mode = options.enums === String ? "UNDEFINED" : 0;
            }
            if (message.brokerId != null && message.hasOwnProperty("brokerId"))
                object.brokerId = message.brokerId;
            if (message.username != null && message.hasOwnProperty("username"))
                object.username = message.username;
            if (message.lang != null && message.hasOwnProperty("lang"))
                object.lang = message.lang;
            if (message.mode != null && message.hasOwnProperty("mode"))
                object.mode = options.enums === String ? $root.authd.SessionMode[message.mode] === undefined ? message.mode : $root.authd.SessionMode[message.mode] : message.mode;
            return object;
        };

        SBRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return SBRequest;
    })();

    authd.SBResponse = (function() {

        function SBResponse(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        SBResponse.prototype.sessionId = "";
        SBResponse.prototype.encryptionKey = "";

        SBResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.SBResponse)
                return object;
            let message = new $root.authd.SBResponse();
            if (object.sessionId != null)
                message.sessionId = String(object.sessionId);
            if (object.encryptionKey != null)
                message.encryptionKey = String(object.encryptionKey);
            return message;
        };

        SBResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.sessionId = "";
                object.encryptionKey = "";
            }
            if (message.sessionId != null && message.hasOwnProperty("sessionId"))
                object.sessionId = message.sessionId;
            if (message.encryptionKey != null && message.hasOwnProperty("encryptionKey"))
                object.encryptionKey = message.encryptionKey;
            return object;
        };

        SBResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return SBResponse;
    })();

    authd.GAMRequest = (function() {

        function GAMRequest(properties) {
            this.supportedUiLayouts = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GAMRequest.prototype.sessionId = "";
        GAMRequest.prototype.supportedUiLayouts = $util.emptyArray;

        GAMRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GAMRequest)
                return object;
            let message = new $root.authd.GAMRequest();
            if (object.sessionId != null)
                message.sessionId = String(object.sessionId);
            if (object.supportedUiLayouts) {
                if (!Array.isArray(object.supportedUiLayouts))
                    throw TypeError(".authd.GAMRequest.supportedUiLayouts: array expected");
                message.supportedUiLayouts = [];
                for (let i = 0; i < object.supportedUiLayouts.length; ++i) {
                    if (typeof object.supportedUiLayouts[i] !== "object")
                        throw TypeError(".authd.GAMRequest.supportedUiLayouts: object expected");
                    message.supportedUiLayouts[i] = $root.authd.UILayout.fromObject(object.supportedUiLayouts[i]);
                }
            }
            return message;
        };

        GAMRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.supportedUiLayouts = [];
            if (options.defaults)
                object.sessionId = "";
            if (message.sessionId != null && message.hasOwnProperty("sessionId"))
                object.sessionId = message.sessionId;
            if (message.supportedUiLayouts && message.supportedUiLayouts.length) {
                object.supportedUiLayouts = [];
                for (let j = 0; j < message.supportedUiLayouts.length; ++j)
                    object.supportedUiLayouts[j] = $root.authd.UILayout.toObject(message.supportedUiLayouts[j], options);
            }
            return object;
        };

        GAMRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GAMRequest;
    })();

    authd.UILayout = (function() {

        function UILayout(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        UILayout.prototype.type = "";
        UILayout.prototype.label = null;
        UILayout.prototype.button = null;
        UILayout.prototype.wait = null;
        UILayout.prototype.entry = null;
        UILayout.prototype.content = null;
        UILayout.prototype.code = null;
        UILayout.prototype.rendersQrcode = null;

        let $oneOfFields;

        Object.defineProperty(UILayout.prototype, "_label", {
            get: $util.oneOfGetter($oneOfFields = ["label"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_button", {
            get: $util.oneOfGetter($oneOfFields = ["button"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_wait", {
            get: $util.oneOfGetter($oneOfFields = ["wait"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_entry", {
            get: $util.oneOfGetter($oneOfFields = ["entry"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_content", {
            get: $util.oneOfGetter($oneOfFields = ["content"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_code", {
            get: $util.oneOfGetter($oneOfFields = ["code"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        Object.defineProperty(UILayout.prototype, "_rendersQrcode", {
            get: $util.oneOfGetter($oneOfFields = ["rendersQrcode"]),
            set: $util.oneOfSetter($oneOfFields)
        });

        UILayout.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.UILayout)
                return object;
            let message = new $root.authd.UILayout();
            if (object.type != null)
                message.type = String(object.type);
            if (object.label != null)
                message.label = String(object.label);
            if (object.button != null)
                message.button = String(object.button);
            if (object.wait != null)
                message.wait = String(object.wait);
            if (object.entry != null)
                message.entry = String(object.entry);
            if (object.content != null)
                message.content = String(object.content);
            if (object.code != null)
                message.code = String(object.code);
            if (object.rendersQrcode != null)
                message.rendersQrcode = Boolean(object.rendersQrcode);
            return message;
        };

        UILayout.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.type = "";
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = message.type;
            if (message.label != null && message.hasOwnProperty("label")) {
                object.label = message.label;
                if (options.oneofs)
                    object._label = "label";
            }
            if (message.button != null && message.hasOwnProperty("button")) {
                object.button = message.button;
                if (options.oneofs)
                    object._button = "button";
            }
            if (message.wait != null && message.hasOwnProperty("wait")) {
                object.wait = message.wait;
                if (options.oneofs)
                    object._wait = "wait";
            }
            if (message.entry != null && message.hasOwnProperty("entry")) {
                object.entry = message.entry;
                if (options.oneofs)
                    object._entry = "entry";
            }
            if (message.content != null && message.hasOwnProperty("content")) {
                object.content = message.content;
                if (options.oneofs)
                    object._content = "content";
            }
            if (message.code != null && message.hasOwnProperty("code")) {
                object.code = message.code;
                if (options.oneofs)
                    object._code = "code";
            }
            if (message.rendersQrcode != null && message.hasOwnProperty("rendersQrcode")) {
                object.rendersQrcode = message.rendersQrcode;
                if (options.oneofs)
                    object._rendersQrcode = "rendersQrcode";
            }
            return object;
        };

        UILayout.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return UILayout;
    })();

    authd.GAMResponse = (function() {

        function GAMResponse(properties) {
            this.authenticationModes = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GAMResponse.prototype.authenticationModes = $util.emptyArray;

        GAMResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GAMResponse)
                return object;
            let message = new $root.authd.GAMResponse();
            if (object.authenticationModes) {
                if (!Array.isArray(object.authenticationModes))
                    throw TypeError(".authd.GAMResponse.authenticationModes: array expected");
                message.authenticationModes = [];
                for (let i = 0; i < object.authenticationModes.length; ++i) {
                    if (typeof object.authenticationModes[i] !== "object")
                        throw TypeError(".authd.GAMResponse.authenticationModes: object expected");
                    message.authenticationModes[i] = $root.authd.GAMResponse.AuthenticationMode.fromObject(object.authenticationModes[i]);
                }
            }
            return message;
        };

        GAMResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.authenticationModes = [];
            if (message.authenticationModes && message.authenticationModes.length) {
                object.authenticationModes = [];
                for (let j = 0; j < message.authenticationModes.length; ++j)
                    object.authenticationModes[j] = $root.authd.GAMResponse.AuthenticationMode.toObject(message.authenticationModes[j], options);
            }
            return object;
        };

        GAMResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        GAMResponse.AuthenticationMode = (function() {

            function AuthenticationMode(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            AuthenticationMode.prototype.id = "";
            AuthenticationMode.prototype.label = "";

            AuthenticationMode.fromObject = function fromObject(object) {
                if (object instanceof $root.authd.GAMResponse.AuthenticationMode)
                    return object;
                let message = new $root.authd.GAMResponse.AuthenticationMode();
                if (object.id != null)
                    message.id = String(object.id);
                if (object.label != null)
                    message.label = String(object.label);
                return message;
            };

            AuthenticationMode.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.id = "";
                    object.label = "";
                }
                if (message.id != null && message.hasOwnProperty("id"))
                    object.id = message.id;
                if (message.label != null && message.hasOwnProperty("label"))
                    object.label = message.label;
                return object;
            };

            AuthenticationMode.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return AuthenticationMode;
        })();

        return GAMResponse;
    })();

    authd.SAMRequest = (function() {

        function SAMRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        SAMRequest.prototype.sessionId = "";
        SAMRequest.prototype.authenticationModeId = "";

        SAMRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.SAMRequest)
                return object;
            let message = new $root.authd.SAMRequest();
            if (object.sessionId != null)
                message.sessionId = String(object.sessionId);
            if (object.authenticationModeId != null)
                message.authenticationModeId = String(object.authenticationModeId);
            return message;
        };

        SAMRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.sessionId = "";
                object.authenticationModeId = "";
            }
            if (message.sessionId != null && message.hasOwnProperty("sessionId"))
                object.sessionId = message.sessionId;
            if (message.authenticationModeId != null && message.hasOwnProperty("authenticationModeId"))
                object.authenticationModeId = message.authenticationModeId;
            return object;
        };

        SAMRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return SAMRequest;
    })();

    authd.SAMResponse = (function() {

        function SAMResponse(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        SAMResponse.prototype.uiLayoutInfo = null;

        SAMResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.SAMResponse)
                return object;
            let message = new $root.authd.SAMResponse();
            if (object.uiLayoutInfo != null) {
                if (typeof object.uiLayoutInfo !== "object")
                    throw TypeError(".authd.SAMResponse.uiLayoutInfo: object expected");
                message.uiLayoutInfo = $root.authd.UILayout.fromObject(object.uiLayoutInfo);
            }
            return message;
        };

        SAMResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.uiLayoutInfo = null;
            if (message.uiLayoutInfo != null && message.hasOwnProperty("uiLayoutInfo"))
                object.uiLayoutInfo = $root.authd.UILayout.toObject(message.uiLayoutInfo, options);
            return object;
        };

        SAMResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return SAMResponse;
    })();

    authd.IARequest = (function() {

        function IARequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        IARequest.prototype.sessionId = "";
        IARequest.prototype.authenticationData = null;

        IARequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.IARequest)
                return object;
            let message = new $root.authd.IARequest();
            if (object.sessionId != null)
                message.sessionId = String(object.sessionId);
            if (object.authenticationData != null) {
                if (typeof object.authenticationData !== "object")
                    throw TypeError(".authd.IARequest.authenticationData: object expected");
                message.authenticationData = $root.authd.IARequest.AuthenticationData.fromObject(object.authenticationData);
            }
            return message;
        };

        IARequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.sessionId = "";
                object.authenticationData = null;
            }
            if (message.sessionId != null && message.hasOwnProperty("sessionId"))
                object.sessionId = message.sessionId;
            if (message.authenticationData != null && message.hasOwnProperty("authenticationData"))
                object.authenticationData = $root.authd.IARequest.AuthenticationData.toObject(message.authenticationData, options);
            return object;
        };

        IARequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        IARequest.AuthenticationData = (function() {

            function AuthenticationData(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            AuthenticationData.prototype.secret = null;
            AuthenticationData.prototype.wait = null;
            AuthenticationData.prototype.skip = null;

            let $oneOfFields;

            Object.defineProperty(AuthenticationData.prototype, "item", {
                get: $util.oneOfGetter($oneOfFields = ["secret", "wait", "skip"]),
                set: $util.oneOfSetter($oneOfFields)
            });

            AuthenticationData.fromObject = function fromObject(object) {
                if (object instanceof $root.authd.IARequest.AuthenticationData)
                    return object;
                let message = new $root.authd.IARequest.AuthenticationData();
                if (object.secret != null)
                    message.secret = String(object.secret);
                if (object.wait != null)
                    message.wait = String(object.wait);
                if (object.skip != null)
                    message.skip = String(object.skip);
                return message;
            };

            AuthenticationData.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (message.secret != null && message.hasOwnProperty("secret")) {
                    object.secret = message.secret;
                    if (options.oneofs)
                        object.item = "secret";
                }
                if (message.wait != null && message.hasOwnProperty("wait")) {
                    object.wait = message.wait;
                    if (options.oneofs)
                        object.item = "wait";
                }
                if (message.skip != null && message.hasOwnProperty("skip")) {
                    object.skip = message.skip;
                    if (options.oneofs)
                        object.item = "skip";
                }
                return object;
            };

            AuthenticationData.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, minimal.util.toJSONOptions);
            };

            return AuthenticationData;
        })();

        return IARequest;
    })();

    authd.IAResponse = (function() {

        function IAResponse(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        IAResponse.prototype.access = "";
        IAResponse.prototype.msg = "";

        IAResponse.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.IAResponse)
                return object;
            let message = new $root.authd.IAResponse();
            if (object.access != null)
                message.access = String(object.access);
            if (object.msg != null)
                message.msg = String(object.msg);
            return message;
        };

        IAResponse.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.access = "";
                object.msg = "";
            }
            if (message.access != null && message.hasOwnProperty("access"))
                object.access = message.access;
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = message.msg;
            return object;
        };

        IAResponse.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return IAResponse;
    })();

    authd.SDBFURequest = (function() {

        function SDBFURequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        SDBFURequest.prototype.brokerId = "";
        SDBFURequest.prototype.username = "";

        SDBFURequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.SDBFURequest)
                return object;
            let message = new $root.authd.SDBFURequest();
            if (object.brokerId != null)
                message.brokerId = String(object.brokerId);
            if (object.username != null)
                message.username = String(object.username);
            return message;
        };

        SDBFURequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.brokerId = "";
                object.username = "";
            }
            if (message.brokerId != null && message.hasOwnProperty("brokerId"))
                object.brokerId = message.brokerId;
            if (message.username != null && message.hasOwnProperty("username"))
                object.username = message.username;
            return object;
        };

        SDBFURequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return SDBFURequest;
    })();

    authd.ESRequest = (function() {

        function ESRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        ESRequest.prototype.sessionId = "";

        ESRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.ESRequest)
                return object;
            let message = new $root.authd.ESRequest();
            if (object.sessionId != null)
                message.sessionId = String(object.sessionId);
            return message;
        };

        ESRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.sessionId = "";
            if (message.sessionId != null && message.hasOwnProperty("sessionId"))
                object.sessionId = message.sessionId;
            return object;
        };

        ESRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return ESRequest;
    })();

    authd.GetPasswdByNameRequest = (function() {

        function GetPasswdByNameRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GetPasswdByNameRequest.prototype.name = "";
        GetPasswdByNameRequest.prototype.shouldPreCheck = false;

        GetPasswdByNameRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GetPasswdByNameRequest)
                return object;
            let message = new $root.authd.GetPasswdByNameRequest();
            if (object.name != null)
                message.name = String(object.name);
            if (object.shouldPreCheck != null)
                message.shouldPreCheck = Boolean(object.shouldPreCheck);
            return message;
        };

        GetPasswdByNameRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.name = "";
                object.shouldPreCheck = false;
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.shouldPreCheck != null && message.hasOwnProperty("shouldPreCheck"))
                object.shouldPreCheck = message.shouldPreCheck;
            return object;
        };

        GetPasswdByNameRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GetPasswdByNameRequest;
    })();

    authd.GetGroupByNameRequest = (function() {

        function GetGroupByNameRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GetGroupByNameRequest.prototype.name = "";

        GetGroupByNameRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GetGroupByNameRequest)
                return object;
            let message = new $root.authd.GetGroupByNameRequest();
            if (object.name != null)
                message.name = String(object.name);
            return message;
        };

        GetGroupByNameRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.name = "";
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            return object;
        };

        GetGroupByNameRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GetGroupByNameRequest;
    })();

    authd.GetShadowByNameRequest = (function() {

        function GetShadowByNameRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GetShadowByNameRequest.prototype.name = "";

        GetShadowByNameRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GetShadowByNameRequest)
                return object;
            let message = new $root.authd.GetShadowByNameRequest();
            if (object.name != null)
                message.name = String(object.name);
            return message;
        };

        GetShadowByNameRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.name = "";
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            return object;
        };

        GetShadowByNameRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GetShadowByNameRequest;
    })();

    authd.GetByIDRequest = (function() {

        function GetByIDRequest(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GetByIDRequest.prototype.id = 0;

        GetByIDRequest.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GetByIDRequest)
                return object;
            let message = new $root.authd.GetByIDRequest();
            if (object.id != null)
                message.id = object.id >>> 0;
            return message;
        };

        GetByIDRequest.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults)
                object.id = 0;
            if (message.id != null && message.hasOwnProperty("id"))
                object.id = message.id;
            return object;
        };

        GetByIDRequest.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GetByIDRequest;
    })();

    authd.PasswdEntry = (function() {

        function PasswdEntry(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        PasswdEntry.prototype.name = "";
        PasswdEntry.prototype.passwd = "";
        PasswdEntry.prototype.uid = 0;
        PasswdEntry.prototype.gid = 0;
        PasswdEntry.prototype.gecos = "";
        PasswdEntry.prototype.homedir = "";
        PasswdEntry.prototype.shell = "";

        PasswdEntry.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.PasswdEntry)
                return object;
            let message = new $root.authd.PasswdEntry();
            if (object.name != null)
                message.name = String(object.name);
            if (object.passwd != null)
                message.passwd = String(object.passwd);
            if (object.uid != null)
                message.uid = object.uid >>> 0;
            if (object.gid != null)
                message.gid = object.gid >>> 0;
            if (object.gecos != null)
                message.gecos = String(object.gecos);
            if (object.homedir != null)
                message.homedir = String(object.homedir);
            if (object.shell != null)
                message.shell = String(object.shell);
            return message;
        };

        PasswdEntry.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.name = "";
                object.passwd = "";
                object.uid = 0;
                object.gid = 0;
                object.gecos = "";
                object.homedir = "";
                object.shell = "";
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.passwd != null && message.hasOwnProperty("passwd"))
                object.passwd = message.passwd;
            if (message.uid != null && message.hasOwnProperty("uid"))
                object.uid = message.uid;
            if (message.gid != null && message.hasOwnProperty("gid"))
                object.gid = message.gid;
            if (message.gecos != null && message.hasOwnProperty("gecos"))
                object.gecos = message.gecos;
            if (message.homedir != null && message.hasOwnProperty("homedir"))
                object.homedir = message.homedir;
            if (message.shell != null && message.hasOwnProperty("shell"))
                object.shell = message.shell;
            return object;
        };

        PasswdEntry.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return PasswdEntry;
    })();

    authd.PasswdEntries = (function() {

        function PasswdEntries(properties) {
            this.entries = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        PasswdEntries.prototype.entries = $util.emptyArray;

        PasswdEntries.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.PasswdEntries)
                return object;
            let message = new $root.authd.PasswdEntries();
            if (object.entries) {
                if (!Array.isArray(object.entries))
                    throw TypeError(".authd.PasswdEntries.entries: array expected");
                message.entries = [];
                for (let i = 0; i < object.entries.length; ++i) {
                    if (typeof object.entries[i] !== "object")
                        throw TypeError(".authd.PasswdEntries.entries: object expected");
                    message.entries[i] = $root.authd.PasswdEntry.fromObject(object.entries[i]);
                }
            }
            return message;
        };

        PasswdEntries.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.entries = [];
            if (message.entries && message.entries.length) {
                object.entries = [];
                for (let j = 0; j < message.entries.length; ++j)
                    object.entries[j] = $root.authd.PasswdEntry.toObject(message.entries[j], options);
            }
            return object;
        };

        PasswdEntries.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return PasswdEntries;
    })();

    authd.GroupEntry = (function() {

        function GroupEntry(properties) {
            this.members = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GroupEntry.prototype.name = "";
        GroupEntry.prototype.passwd = "";
        GroupEntry.prototype.gid = 0;
        GroupEntry.prototype.members = $util.emptyArray;

        GroupEntry.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GroupEntry)
                return object;
            let message = new $root.authd.GroupEntry();
            if (object.name != null)
                message.name = String(object.name);
            if (object.passwd != null)
                message.passwd = String(object.passwd);
            if (object.gid != null)
                message.gid = object.gid >>> 0;
            if (object.members) {
                if (!Array.isArray(object.members))
                    throw TypeError(".authd.GroupEntry.members: array expected");
                message.members = [];
                for (let i = 0; i < object.members.length; ++i)
                    message.members[i] = String(object.members[i]);
            }
            return message;
        };

        GroupEntry.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.members = [];
            if (options.defaults) {
                object.name = "";
                object.passwd = "";
                object.gid = 0;
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.passwd != null && message.hasOwnProperty("passwd"))
                object.passwd = message.passwd;
            if (message.gid != null && message.hasOwnProperty("gid"))
                object.gid = message.gid;
            if (message.members && message.members.length) {
                object.members = [];
                for (let j = 0; j < message.members.length; ++j)
                    object.members[j] = message.members[j];
            }
            return object;
        };

        GroupEntry.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GroupEntry;
    })();

    authd.GroupEntries = (function() {

        function GroupEntries(properties) {
            this.entries = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        GroupEntries.prototype.entries = $util.emptyArray;

        GroupEntries.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.GroupEntries)
                return object;
            let message = new $root.authd.GroupEntries();
            if (object.entries) {
                if (!Array.isArray(object.entries))
                    throw TypeError(".authd.GroupEntries.entries: array expected");
                message.entries = [];
                for (let i = 0; i < object.entries.length; ++i) {
                    if (typeof object.entries[i] !== "object")
                        throw TypeError(".authd.GroupEntries.entries: object expected");
                    message.entries[i] = $root.authd.GroupEntry.fromObject(object.entries[i]);
                }
            }
            return message;
        };

        GroupEntries.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.entries = [];
            if (message.entries && message.entries.length) {
                object.entries = [];
                for (let j = 0; j < message.entries.length; ++j)
                    object.entries[j] = $root.authd.GroupEntry.toObject(message.entries[j], options);
            }
            return object;
        };

        GroupEntries.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return GroupEntries;
    })();

    authd.ShadowEntry = (function() {

        function ShadowEntry(properties) {
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        ShadowEntry.prototype.name = "";
        ShadowEntry.prototype.passwd = "";
        ShadowEntry.prototype.lastChange = 0;
        ShadowEntry.prototype.changeMinDays = 0;
        ShadowEntry.prototype.changeMaxDays = 0;
        ShadowEntry.prototype.changeWarnDays = 0;
        ShadowEntry.prototype.changeInactiveDays = 0;
        ShadowEntry.prototype.expireDate = 0;

        ShadowEntry.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.ShadowEntry)
                return object;
            let message = new $root.authd.ShadowEntry();
            if (object.name != null)
                message.name = String(object.name);
            if (object.passwd != null)
                message.passwd = String(object.passwd);
            if (object.lastChange != null)
                message.lastChange = object.lastChange | 0;
            if (object.changeMinDays != null)
                message.changeMinDays = object.changeMinDays | 0;
            if (object.changeMaxDays != null)
                message.changeMaxDays = object.changeMaxDays | 0;
            if (object.changeWarnDays != null)
                message.changeWarnDays = object.changeWarnDays | 0;
            if (object.changeInactiveDays != null)
                message.changeInactiveDays = object.changeInactiveDays | 0;
            if (object.expireDate != null)
                message.expireDate = object.expireDate | 0;
            return message;
        };

        ShadowEntry.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.defaults) {
                object.name = "";
                object.passwd = "";
                object.lastChange = 0;
                object.changeMinDays = 0;
                object.changeMaxDays = 0;
                object.changeWarnDays = 0;
                object.changeInactiveDays = 0;
                object.expireDate = 0;
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.passwd != null && message.hasOwnProperty("passwd"))
                object.passwd = message.passwd;
            if (message.lastChange != null && message.hasOwnProperty("lastChange"))
                object.lastChange = message.lastChange;
            if (message.changeMinDays != null && message.hasOwnProperty("changeMinDays"))
                object.changeMinDays = message.changeMinDays;
            if (message.changeMaxDays != null && message.hasOwnProperty("changeMaxDays"))
                object.changeMaxDays = message.changeMaxDays;
            if (message.changeWarnDays != null && message.hasOwnProperty("changeWarnDays"))
                object.changeWarnDays = message.changeWarnDays;
            if (message.changeInactiveDays != null && message.hasOwnProperty("changeInactiveDays"))
                object.changeInactiveDays = message.changeInactiveDays;
            if (message.expireDate != null && message.hasOwnProperty("expireDate"))
                object.expireDate = message.expireDate;
            return object;
        };

        ShadowEntry.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return ShadowEntry;
    })();

    authd.ShadowEntries = (function() {

        function ShadowEntries(properties) {
            this.entries = [];
            if (properties)
                for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        ShadowEntries.prototype.entries = $util.emptyArray;

        ShadowEntries.fromObject = function fromObject(object) {
            if (object instanceof $root.authd.ShadowEntries)
                return object;
            let message = new $root.authd.ShadowEntries();
            if (object.entries) {
                if (!Array.isArray(object.entries))
                    throw TypeError(".authd.ShadowEntries.entries: array expected");
                message.entries = [];
                for (let i = 0; i < object.entries.length; ++i) {
                    if (typeof object.entries[i] !== "object")
                        throw TypeError(".authd.ShadowEntries.entries: object expected");
                    message.entries[i] = $root.authd.ShadowEntry.fromObject(object.entries[i]);
                }
            }
            return message;
        };

        ShadowEntries.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            let object = {};
            if (options.arrays || options.defaults)
                object.entries = [];
            if (message.entries && message.entries.length) {
                object.entries = [];
                for (let j = 0; j < message.entries.length; ++j)
                    object.entries[j] = $root.authd.ShadowEntry.toObject(message.entries[j], options);
            }
            return object;
        };

        ShadowEntries.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, minimal.util.toJSONOptions);
        };

        return ShadowEntries;
    })();

    return authd;
})();

const pam = $root.pam = (() => {

    const pam = {};

    pam.Stage = (function() {
        const valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "userSelection"] = 0;
        values[valuesById[1] = "brokerSelection"] = 1;
        values[valuesById[2] = "authModeSelection"] = 2;
        values[valuesById[3] = "challenge"] = 3;
        return values;
    })();

    return pam;
})();

export { authd, $root as default, gdm, pam };
