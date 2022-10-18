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

/* NOTE: acceptable 'src' must need either of following two specification.
         1. <ArrayBuffer>
         2. has properties { buffer: <ArrayBuffer>, byteOffset: <number>, byteLength: <number> }.
            (*)not limited either <Uint8Array> or <DataView>.
 */
function _Uint8Array_castOf(src, offset = 0, length = 0) {
BUG(src && typeof src.byteLength == 'number');
	if (0 == length || src.byteLength < offset +length)
		length = src.byteLength - offset;

	if (src instanceof Uint8Array && 0 == offset && src.byteLength == length)
		return src; // cast not needed
	if (! (0 < length))
		return null; // offset over buffer

	if (src && src.buffer instanceof ArrayBuffer) // Uint8Array DataView etc.
		return new Uint8Array(src.buffer, src.byteOffset +offset, length); // ref
	else if (src instanceof ArrayBuffer)
		return new Uint8Array(src, +offset, length); // ref
	else
		return null; // cast cannot applied
}

function _DataView_castOf(src, offset = 0, length = 0) {
BUG(src && typeof src.byteLength == 'number');
	if (0 == length || src.byteLength < offset +length)
		length = src.byteLength - offset;

	if (src instanceof DataView && 0 == offset && src.byteLength == length)
		return src; // cast not needed
	if (! (0 < length))
		return null; // offset over buffer

	if (src && src.buffer instanceof ArrayBuffer) // Uint8Array DataView etc.
		return new DataView(src.buffer, src.byteOffset +offset, length); // ref
	else if (src instanceof ArrayBuffer)
		return new DataView(src, +offset, length); // ref
	else
		return null; // cast cannot applied
}

///////////////////////////////////////////////////////////////

// PENDING: performance (*1)
function _memcpy(dst, dst_offset, src, src_offset, length) {
	dst = _Uint8Array_castOf(dst, dst_offset, length);
	src = _Uint8Array_castOf(src, src_offset, length);
BUG(dst instanceof Uint8Array && length <= dst.byteLength);
BUG(src instanceof Uint8Array && length <= src.byteLength);

	for (let i = 0; i < length; ++i)
		dst[i] = src[i]; // (*1)
}

///////////////////////////////////////////////////////////////

// PENDING: performance (*1)
function _memcmp(lhs, lhs_offset, rhs, rhs_offset, length = 0) {
	lhs = _Uint8Array_castOf(lhs, lhs_offset, length);
	rhs = _Uint8Array_castOf(rhs, rhs_offset, length);
	length = Math.min(lhs.byteLength, rhs.byteLength);

	for (let i = 0; i < length; ++i) {
		if (lhs[i] == rhs[i]) // (*1)
			continue;
		return (lhs[i] < rhs[i]) ? -1 : 1;
	}
	if (! (lhs.byteLength == rhs.byteLength))
		return (lhs.byteLength < rhs.byteLength) ? -1 : 1;
	return 0;
}

///////////////////////////////////////////////////////////////

function _ArrayBuffer_concat(array) {
//BUG(this == undefined || this.buffer instanceof ArrayBuffer);
	for (const e of array) {
BUG(e.buffer instanceof ArrayBuffer);
	}

let total_size = 0;
//	if (this)
//		total_size += this.byteLength;
	for (const e of array)
		total_size += e.byteLength;
const result
	= new Uint8Array(new ArrayBuffer(total_size));

	function _copy(dst, seek, _src) {
BUG(dst instanceof Uint8Array);
	src = _Uint8Array_castOf(_src);
BUG(src instanceof Uint8Array);
BUG(seek + src.byteLength <= dst.byteLength);
		_memcpy(dst, +seek, src, +0, src.byteLength);
		return src.byteLength;
	}

let progress = 0;
//	if (this)
//		progress += _copy(result, progress, this);
	for (const e of array)
		progress += _copy(result, progress, e);

	return result.buffer;
}

///////////////////////////////////////////////////////////////

module.exports.Uint8Array_castOf = _Uint8Array_castOf;
module.exports.DataView_castOf = _DataView_castOf;
module.exports.memcpy = _memcpy;
module.exports.memcmp = _memcmp;
module.exports.ArrayBuffer_concat = _ArrayBuffer_concat;
