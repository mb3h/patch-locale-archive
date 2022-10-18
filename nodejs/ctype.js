const {
	OP_UPDATE,
	OP_FORWARD,
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
	ArrayBuffer_concat,
} = require('./buffer-helper');
const {
	_buffers_length,
	_buffers_lookup,
	_buffers_splice,
} = require('./simple-buffers');

const {
	MAGIC_CTYPE,
	LC_CTYPE,
} = require('./constant');

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

function _get_n_elements(header) {
BUG(header instanceof DataView);
const n_elements
	= header.getUint32(+0x04, LE);
//BUG(+0x08 + n_elements * 4 <= header.byteLength);
	return n_elements;
}

function _is_valid_header(header) {
BUG(header instanceof DataView);
	if (! (+0x00 +8 <= header.byteLength))
		return false;
const magic
	= header.getUint32(+0x00, LE);
	if (! ((MAGIC_CTYPE ^ LC_CTYPE) == magic))
		return false; // error 'not LC_CTYPE' cf) locale/localeinfo.h
const n_elements
	= _get_n_elements(header);
	if (! (+0x08 + n_elements * 4 <= header.byteLength))
		return false; // data error
	return true;
}

function _get_offset(header, index) {
BUG(header instanceof DataView);
BUG(typeof index == 'number' && index < _get_n_elements(header));
const offset
	= header.getUint32(+0x08 +index * 4, LE);
	return offset;
}

function _set_offset(header, index, offset) {
BUG(header instanceof DataView);
BUG(typeof index == 'number' && index < _get_n_elements(header));
	header.setUint32(+0x08 +index * 4, offset, LE);
}

function _resize_block(header, index, expand_size, opListener) {
BUG(header instanceof DataView);
const n_elements
	= _get_n_elements(header);
BUG(index < n_elements);
const expand_block_offset
	= _get_offset(header, index);

let is_modified = false;
	for (let i = 0; i < n_elements; ++i) {
let old_offset;
		if (! (expand_block_offset < (old_offset = _get_offset(header, i))))
			continue;
		is_modified = true;
		_set_offset(header, i, old_offset + expand_size);
	}
	if (opListener && is_modified)
		opListener(+0x08, _get_n_elements(header) * 4, OP_UPDATE);
}

function _enum_offsets(header, callback) {
BUG(header instanceof DataView);
BUG(typeof callback == 'function');
	header = new DataView(header.buffer, header.byteOffset, header.byteLength); // ref
	if (!_is_valid_header(header))
		return; // TODO: error handling
const n_elements
	= _get_n_elements(header);
	for (let i = 0; i < n_elements; ++i) {
const offset
		= _get_offset(header, i);
		callback(offset, i);
	}
}

function _lookup_offsets(header, index) {
BUG(header instanceof DataView);
BUG(typeof index == 'number');
const begin
	= _get_offset(header, index);
	// NOTE: white paper not found, so that 
	//       don't expect address ordering.
let end
	= header.byteLength;
	_enum_offsets(header, (offset, index) => {
		if (begin < offset && offset < end)
			end = offset;
	});
BUG(begin < end);
	return [begin, end];
}

function _get(buffers, index) {
BUG(buffers instanceof Array);
let begin, end;
	[begin, end] = _lookup_offsets(buffers[0], index);

let i, i_offset, j, j_offset;
	[i, i_offset] = _buffers_lookup(buffers, begin);
	[j, j_offset] = _buffers_lookup(buffers, end);
BUG(i == j || i +1 == j && 0 == j_offset);
const retval = new Uint8Array(
		  buffers[i].buffer
		, buffers[i].byteOffset + i_offset
		, end - begin
	); // ref
	return retval;
}

function _set(buffers, index, new_data, opListener) {
BUG(buffers[0] instanceof DataView);
BUG(typeof index == 'number');
	new_data = Uint8Array_castOf(new_data);

let begin, end;
	[begin, end] = _lookup_offsets(buffers[0], index);

const expand_size
	= new_data.byteLength - (end - begin);
let ctype_end;
	if (opListener && end < (ctype_end = _buffers_length(buffers)))
		opListener(begin + new_data.byteLength, ctype_end - end, OP_FORWARD, expand_size);

	_buffers_splice(buffers, begin, end - begin, new_data);

	_resize_block(buffers[0], index, expand_size, opListener);
}

///////////////////////////////////////////////////////////////
// LC_CTYPE access [public]

function LocaleCType(src, offset, length) {
	if (null == src) return null; // pass through
	if (!(this instanceof LocaleCType)) new LocaleCType(src, offset, length);

	this._opListener = null;
	this._buffers = [DataView_castOf(src, offset, length)];

	Object.defineProperty(this, 'offset', { value: 0, writable: false });
	Object.defineProperty(this, 'length', { get() {
		return _buffers_length(this._buffers);
	}});
	Object.defineProperty(this, 'buffer', { get() {
		if (!this._buffer)
			this._buffer = ArrayBuffer_concat(this._buffers);
		return this._buffer;
	}});
}

LocaleCType.prototype.resolve = function(index) { return _get_offset(this._buffers[0], index); }
LocaleCType.prototype.get = function(index) { return _get(this._buffers, index); }
LocaleCType.prototype.set = function(index, data) {
	_set(this._buffers, index, data, this._opListener);
	this._buffer = null;
}
LocaleCType.prototype.setOpListener = function(callback) {
	this._opListener = callback;
}

module.exports = LocaleCType;
module.exports.setLogHandler = _setLogHandler;
