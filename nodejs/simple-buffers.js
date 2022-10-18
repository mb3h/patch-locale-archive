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

function _buffers_length(a) {
BUG(a instanceof Array);
let total = 0;
	for (b of a) {
BUG(b && typeof b.byteLength == 'number');
		total += b.byteLength;
	}
	return total;
}

function _buffers_lookup(a, target) {
let total, i;
	for (total = 0, i = 0; i < a.length && !(target < total + a[i].byteLength); ++i)
		total += a[i].byteLength;
	if (! (i < a.length))
		return [a.length, 0];
	return [i, target - total];
}

function _buffers_subref(a, offset, length) {
	if (! (0 < length)) return [];

let p, p_offset, q, q_offset;
	[p, p_offset] = _buffers_lookup(a, offset);
	[q, q_offset] = _buffers_lookup(a, offset + length);
	if (p < q && 0 == q_offset)
		q_offset = a[--q].byteLength;

const retval = [];
	for (let i = p; i < q +1; ++i) {
let begin, end;
		begin = (i == p && 0 < p_offset) ? p_offset : 0;
		end = (i == q) ? q_offset : a[i].byteLength;
		if (a[i] instanceof ArrayBuffer)
			retval.push(new Uint8Array(a[i], begin, end - begin));
		else
			retval.push(new Uint8Array(a[i].buffer, a[i].byteOffset + begin, end - begin));
	}
	return retval;
}

// NOTE: (*1) on deviding, type cast to Uint8Array execept for DataView.
//       (*2) item1 simply refered (not copied).
function _buffers_splice(a, byteStart, byteDeleteCount, item1) {
BUG(a instanceof Array);
	if (item1 instanceof ArrayBuffer)
		item1 = new Uint8Array(item1, 0, item1.byteLength); // ref
BUG(item1.buffer instanceof ArrayBuffer);

let i, i_offset, j, j_offset;
	[i, i_offset] = _buffers_lookup(a, byteStart);
	[j, j_offset] = _buffers_lookup(a, byteStart + byteDeleteCount);
	if (i == j && 0 < i_offset && i_offset < j_offset)
		a.splice(++j, 0, a[i]);
let ctor;
	if (0 < i_offset) {
		ctor = (a[i] instanceof DataView) ? DataView : Uint8Array; // (*1)
		a[i] = new ctor(a[i].buffer, a[i].byteOffset, i_offset), ++i;
	}
	if (0 < j_offset) {
		ctor = (a[i] instanceof DataView) ? DataView : Uint8Array; // (*1)
		a[j] = new ctor(a[j].buffer, a[j].byteOffset + j_offset, a[j].byteLength - j_offset);
	}
	a.splice(i, j -i, item1); // (*2)
}

module.exports._buffers_length = _buffers_length;
module.exports._buffers_lookup = _buffers_lookup;
module.exports._buffers_subref = _buffers_subref;
module.exports._buffers_splice = _buffers_splice;
