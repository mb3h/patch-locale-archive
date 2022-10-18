const {
	VT0, VTRR, VTGG, VTYY
} = require('./vt100-constant');

const { CodingError } = require('./custom-error');
///////////////////////////////////////////////////////////////
// assertion

function assert(expr, msg = '') {
	if (expr) return;
	throw new CodingError(`${VTRR}ASSERT${VT0} ${msg}`);
}
const BUG = assert;

///////////////////////////////////////////////////////////////
// internal

function _hexdump(src, sep, start, count) {
BUG(src instanceof Uint8Array || src instanceof DataView || src instanceof ArrayBuffer);
BUG(undefined === sep || typeof sep === 'string');
BUG(undefined === start || typeof start === 'number')
BUG(undefined === count || typeof count === 'number')
	start = start || 0;
	count = count || src.byteLength - start;
	if (src instanceof ArrayBuffer)
		src = new Uint8Array(src, start, count);
	else if (! (src instanceof Uint8Array && 0 == start && src.byteLength == start))
		src = new Uint8Array(src.buffer, src.byteOffset + start, count);
let retval = '';
	for (let i = 0; i < count; ++i) {
const s
		= '0' + src[i].toString(16);
		retval += (0 < i && sep ? sep : '') + s.substring(s.length -2, s.length);
	}
	return retval;
}

function _padding(len, src, pad) {
	src = (typeof src === 'number') ? src.toString() : src;
	pad = pad || ' ';
BUG(typeof len === 'number' && !(0 === len));
BUG(typeof src === 'string');
BUG(typeof pad === 'string');
let retval;
	if (len < 0) {
		retval = src + pad.repeat(-len);
		retval = retval.substr(0, Math.max(-len, src.length));
	}
	else {
		retval = pad.repeat(len) + src;
		retval = retval.substr(retval.length -Math.max(len, src.length), retval.length);
	}
	return retval;
}

///////////////////////////////////////////////////////////////
// public

module.exports.hexdump = _hexdump;
module.exports.padding = _padding;
