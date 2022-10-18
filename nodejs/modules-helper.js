// node_modules
const md5 = require('md5');

const {
	VT0, VTRR, VTGG, VTYY
} = require('./vt100-constant');
const {
	Uint8Array_castOf,
} = require('./buffer-helper');

const { CodingError } = require('./custom-error');
///////////////////////////////////////////////////////////////
// assertion

// cf)internal/errors.js
function assert(expr, msg = '') {
	if (expr) return;
	throw new CodingError(`${VTRR}ASSERT${VT0} ${msg}`);
}
const BUG = assert;

///////////////////////////////////////////////////////////////
// foreign library glue

const BE = false, LE = true; // syntax sugar for DataView

function _md5sum(src, offset, length) {
const target
	= Uint8Array_castOf(src, offset, length); // ref

const result
	= md5(target);
BUG(typeof result == 'string' && 32 == result.length);
const retval
	= new ArrayBuffer(16);
const writer
	= new DataView(retval);
const hex2bin32
	= function *() {
		yield { from:  0, to: 0x00 };
		yield { from:  8, to: 0x04 };
		yield { from: 16, to: 0x08 };
		yield { from: 24, to: 0x0c };
	};
	for (const it of hex2bin32()) {
const u32
		= parseInt(result.substring(it.from, it.from + 8), 16);
		writer.setUint32(it.to, u32, BE);
	}
	return new Uint8Array(retval);
}

///////////////////////////////////////////////////////////////

module.exports.md5sum = _md5sum;
