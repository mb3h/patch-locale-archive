const {
	OP_UPDATE,
	OP_INSERT,
} = require('./whatdo-constant');
const {
	LL_WARN,
	LL_INFO,
	LL_VERBOSE,
	LL_TRACE,
} = require('./log-constant');
const {
	VT0, VTRR, VTGG, VTYY
} = require('./vt100-constant');
const {
	Uint8Array_castOf,
	DataView_castOf,
	memcpy,
	ArrayBuffer_concat,
} = require('./buffer-helper');
const {
	_buffers_length,
	_buffers_lookup,
	_buffers_splice,
} = require('./simple-buffers');

const BE = false, LE = true; // syntax sugar for DataView

const { CodingError } = require('./custom-error');
///////////////////////////////////////////////////////////////
// assertion

function assert(expr, msg = '') {
	if (expr) return;
	throw new CodingError(`${VTRR}ASSERT${VT0} ${msg}`);
}
const BUG = assert;

///////////////////////////////////////////////////////////////
// log

let g_logHandler = undefined;
function _setLogHandler(cb) {
	g_logHandler = cb;
}

function warn(msg) {
	if (g_logHandler) return;
	g_logHandler(LL_WARN, msg);
}

function info(msg) {
	if (g_logHandler) return;
	g_logHandler(LL_INFO, msg);
}

function verbose(msg) {
	if (g_logHandler) return;
	g_logHandler(LL_VERBOSE, msg);
}

function trace(msg) {
	if (g_logHandler) return;
	g_logHandler(LL_TRACE, msg);
}

///////////////////////////////////////////////////////////////
// local glue

Uint8Array.prototype.equals = function(rhs) {
BUG(rhs instanceof Uint8Array);
	if (! (this.byteLength == rhs.byteLength))
		return false;
	for (let i = 0; i < this.byteLength; ++i)
		if (! (this[i] == rhs[i]))
			return false;
	return true;
}

///////////////////////////////////////////////////////////////
// header access

function _parse_header(src) {
const endian
	= DataView_castOf(src); // ref

const retval = {
		shift1      : endian.getUint32(+0x00, LE),
		bound       : endian.getUint32(+0x04, LE),
		shift2      : endian.getUint32(+0x08, LE),
		mask2       : endian.getUint32(+0x0C, LE),
		mask3       : endian.getUint32(+0x10, LE),
		lookup1_top : +0x14,
	};
	return retval;
}

///////////////////////////////////////////////////////////////
// level1 level2 table access

function _lookup2_from(src, unicode) {
const endian
	= DataView_castOf(src); // ref

const head
	= _parse_header(src);
// read level1 table
let lookup1, index1;
	lookup1 = head.lookup1_top +(index1 = unicode >> head.shift1) * 4;
	if (! (lookup1 +3 < src.byteLength))
		return 0;
const lookup2_top
	= endian.getUint32(lookup1, LE);

// read level2 table
let lookup2, index2;
	lookup2 = lookup2_top +(index2 = head.mask2 & unicode >> head.shift2) * 4;
	if (! (0 < lookup2_top && lookup2 +3 < src.byteLength))
		return 0;

	return lookup2;
}

function _table3_offset_of(src, unicode) {
const endian
	= DataView_castOf(src); // ref

let lookup2;
	if (! (lookup2 = _lookup2_from(src, unicode)))
		return 0;
const lookup3_top
	= endian.getUint32(lookup2, LE);

const head
	= _parse_header(src);
	if (! (0 < lookup3_top && lookup3_top +(1 << head.shift2) -1 < src.byteLength))
		return 0;
	return lookup3_top;
}

///////////////////////////////////////////////////////////////

function _modify(table3_lookup, table3_data, head, unicode, width) {
// code readability
	function _is_forked(table3) { return (table3.appending) ? true : false; }

const lookup
	= table3_lookup[unicode >> head.shift2];
const old_table3
	= table3_data[lookup.data_index];
const old_data
	= _is_forked(old_table3) ? old_table3.appending : old_table3.existed;

// Is specified width same to current width ?
let index3;
	if (width == old_data[index3 = head.mask3 & unicode])
		return;
	lookup.is_dirty = true;

// If not shared any other, modify directly.
	if (1 == old_table3.refcount && !_is_forked(old_table3)) {
		old_data[index3] = width;
		return;
	}

// Is same level3 table existing ?
const new_data
	= new Uint8Array(new ArrayBuffer(1 << head.shift2));
	memcpy(new_data, +0, old_data, +0, new_data.byteLength);
	new_data[index3] = width;
let data_index
	= table3_data.findIndex((table3) => {
		if (0 == table3.refcount) return false;
const other_data
		= _is_forked(table3) ? table3.appending : table3.existed;
		return other_data.equals(new_data);
	});

	if (0 <= data_index)
		++table3_data[data_index].refcount;
	else if (1 == old_table3.refcount) {
		old_data[index3] = width;
		return;
	}
	else {
		data_index = table3_data.push({
			refcount: 1,
			offset: 0, // initialized when _commit()
			appending: new_data,
			existed: null,
		}) -1;
	}

	if (_is_forked(old_table3)) {
BUG(0 < old_table3.refcount);
		--old_table3.refcount;
	}
	lookup.data_index = data_index;
}

function _commit(table3_lookup, table3_data, src, dst, offset) {
// code readability
	function _is_forked(table3) { return (table3.appending) ? true : false; }

const head
	= _parse_header(src);
const sizeofTABLE3
	= 1 << head.shift2;
BUG(typeof offset === 'number');
	if (dst && dst.buffer)
		offset += dst.byteOffset, dst = dst.buffer;

let is_dirty = false;
	for (const lookup of table3_lookup)
		if (lookup && lookup.is_dirty) {
			is_dirty = true;
			break;
		}
	if (!is_dirty)
		; // TODO:

// calculate total expanding size
let expand_count = 0;
	for (const table3 of table3_data)
		if (_is_forked(table3))
			++expand_count;
	if (!dst)
		return expand_count * sizeofTABLE3;

// output all expanding data
BUG(dst instanceof ArrayBuffer);
	dst = new Uint8Array(dst);
let used = 0;
	for (const table3 of table3_data) {
		if (!_is_forked(table3))
			continue;
		if (! (offset +used +sizeofTABLE3 <= dst.byteLength))
			return used; // not enough buffer
		memcpy(dst, offset +used, table3.appending, 0, sizeofTABLE3);
		table3.offset = src.byteLength +used;
		used += sizeofTABLE3;
	}
BUG(used == expand_count * sizeofTABLE3);

// modify level2 table
const endian
	= DataView_castOf(src); // ref
const unicode_max
	= head.bound << head.shift1;
	for (let unicode = 0; unicode < unicode_max; unicode += 1 << head.shift2) {
let lookup;
		if (!(lookup = table3_lookup[unicode >> head.shift2]) || !lookup.is_dirty)
			continue;
const lookup3_top
		= (table3 = table3_data[lookup.data_index]).offset;
const lookup2
		= _lookup2_from(src, unicode);
BUG(0 < lookup2);
		endian.setUint32(lookup2, lookup3_top, LE);
	}
	return used;
}

///////////////////////////////////////////////////////////////
// dtor/ctor
// (LC_CTYPE#)_NL_WIDTH access [public]

function LocaleWcWidth(src, offset, length) {
	if (null == src) return null; // pass through
	if (!(this instanceof LocaleWcWidth)) new LocaleWcWidth(src, offset, length);
const wcwidth
	= Uint8Array_castOf(src, offset, length);

	this._opListener = null;
	this._buffers = [wcwidth]; // ref
	this._buffer = null;

	Object.defineProperty(this, 'offset', { value: 0, writable: false });
	Object.defineProperty(this, 'length', { get() {
		return _buffers_length(this._buffers);
	}});
	Object.defineProperty(this, 'buffer', { get() {
		if (!this._buffer)
			this._buffer = ArrayBuffer_concat(this._buffers);
		return this._buffer;
	}});

const offset_lookup
	= new Map();

const head
	= _parse_header(wcwidth);
const unicode_max
	= head.bound << head.shift1;
	this.table3_lookup = new Array(unicode_max >> head.shift2);
	for (let unicode = 0; unicode < unicode_max; unicode += 1 << head.shift2) {
let table3_offset;
		if (0 == (table3_offset = _table3_offset_of(wcwidth, unicode)))
			continue; // error or level3-table not linked
let i;
		if (offset_lookup.has(table3_offset)) {
let v;
			i = (v = offset_lookup.get(table3_offset)).data_index;
			++v.refcount;
		}
		else {
			i = offset_lookup.size;
			offset_lookup.set(table3_offset, {
				data_index: i,
				refcount: 1,
			});
		}

		this.table3_lookup[unicode >> head.shift2] = {
			is_dirty:   false,
			data_index: i,
		};
	}

	this.table3_data = new Array(offset_lookup.size);
	offset_lookup.forEach((v, table3_offset) => {
		this.table3_data[v.data_index] = {
			refcount: v.refcount,
			offset: table3_offset,
			appending: null,
			existed: Uint8Array_castOf(wcwidth, +table3_offset, 1 << head.shift2), // ref
		};
	});
}

///////////////////////////////////////////////////////////////
// public method

LocaleWcWidth.prototype.modify = function(unicode, width) {
const head
	= _parse_header(this._buffers[0]);
	return _modify(this.table3_lookup, this.table3_data, head, unicode, width);
}

LocaleWcWidth.prototype.commit = function() {
const expand_length
	= _commit(this.table3_lookup, this.table3_data, this._buffers[0], null, 0);

	if (this._opListener) {
		this._opListener(0, this._buffers[0].byteLength, OP_UPDATE); // PENDING: skip when modified nothing
		if (0 < expand_length)
			this._opListener(this._buffers[0].byteLength, expand_length, OP_INSERT);
	}

	this._buffers.splice(1);
	if (0 < expand_length) {
		this._buffers.push(new Uint8Array(expand_length));
		_commit(this.table3_lookup, this.table3_data, this._buffers[0], this._buffers[1], 0);
	}
	this._buffer = null;
}

LocaleWcWidth.prototype.setOpListener = function(callback) {
	this._opListener = callback;
}

module.exports = LocaleWcWidth;
module.exports.setLogHandler = _setLogHandler;
