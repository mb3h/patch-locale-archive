// node_modules
const sprintf = require('sprintf-js').sprintf;
const md5sum = require('./modules-helper').md5sum;

const {
	OP_UPDATE,
	OP_FORWARD,
	OP_PADDING,
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
	memcmp,
	ArrayBuffer_concat,
} = require('./buffer-helper');
const {
	_buffers_length,
	_buffers_lookup,
	_buffers_subref,
	_buffers_splice,
} = require('./simple-buffers');

const {
	ARCHIVE_MAXSIZE,
	LC_CTYPE,
	LC_NUMERIC,
	LC_TIME,
	LC_COLLATE,
	LC_MONETARY,
	LC_MESSAGE,
	LC_ALL,
	LC_PAPER,
	LC_NAME,
	LC_ADDRESS,
	LC_TELEPHONE,
	LC_MEASUREMENT,
	LC_IDENTIFICATION,
	LC_LAST,
	sizeofHEADER,
	sizeofNAMEHASH,
	sizeofLOCREC,
	sizeofSUMHASH,
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
const CONFLICT = assert;

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
// local extent

Array.prototype.unique = function(compareFunction) {
BUG(typeof compareFunction === 'function');
let i;
	for (i = this.length; 1 < i; --i) {
		if (! (0 === compareFunction(this[i -2], this[i -1])))
			continue;
		this.splice(i -1, 1);
	}
}

///////////////////////////////////////////////////////////////
// utility

function _split(sep, src, offset, callback) {
BUG(src.buffer instanceof ArrayBuffer);
BUG(null == callback || typeof callback === 'function');
BUG(typeof sep === 'number');
	if (! (src instanceof Uint8Array))
		src = new Uint8Array(src.buffer, src.byteOffset, src.byteLength); // ref

let retval, next;
	for (retval = 0; offset < (next = src.indexOf(sep, offset)); offset = next +1) {
		if (callback)
			callback(offset, next -offset); // ref
		++retval;
	}
	return retval;
}

// cf) locale/programs/locarchive.h
/*
(*1) On javascript, result of shift opration ('<<' '>>') is rounded
     to signed 32-bits.
     So that 0x80000000..0xffffffff is treated as -0x80000000..-1,
     suitable recovery operation is needed.
 */
function _hashval(src/*, offset, length*/) {
BUG(src instanceof Uint8Array || src instanceof DataView/* || src instanceof ArrayBuffer*/);
	if (src instanceof DataView)
		src = new Uint8Array(src.buffer, src.byteOffset, src.byteLength);
//	else if (src instanceof ArrayBuffer)
//		src = new Uint8Array(sum, offset, length);
let retval
	= src.byteLength;
	for (b of src) {
		retval = retval << 9 | 0x1FF & retval >> 23; // 32bit rotate
		retval += b;
	}
	if (retval < 0)
		retval += 0x100000000; // (*1)
	return retval;
}

///////////////////////////////////////////////////////////////
// HEADER access

function _parse_header(archive, offset = 0) {
const endian
	= DataView_castOf(archive, offset, sizeofHEADER);
BUG(endian instanceof DataView);
BUG(sizeofHEADER <= endian.byteLength);

const retval = {
		namehash_offset  : endian.getUint32(+0x08, LE),
		namehash_size    : endian.getUint32(+0x10, LE),
		string_offset    : endian.getUint32(+0x14, LE),
		string_used      : endian.getUint32(+0x18, LE),
		locrectab_offset : endian.getUint32(+0x20, LE),
		locrectab_used   : endian.getUint32(+0x24, LE),
		sumhash_offset   : endian.getUint32(+0x2C, LE),
		sumhash_size     : endian.getUint32(+0x34, LE),
	};
	return retval;
}

///////////////////////////////////////////////////////////////
// NAMEHASH access

function _namehash_lookup_locale(namehash, name) {
BUG(name instanceof Uint8Array);

let hval, index;
	index = (hval = _hashval(name)) % namehash.maxsize;
const found
	= _read_namehash(namehash.data, namehash.offset + sizeofNAMEHASH * index);
	if (! (hval == found.hashval))
		return null; // PENDING: index collision not supported yet.

	return found;
}

///////////////////////////////////////////////////////////////
// STRING access

function _string_enum(strings, callback) {
BUG(typeof callback === 'function');
	_split(0x00, strings.data, strings.offset, (offset, length) => {
const bytes
		= new Uint8Array(strings.data.buffer, strings.data.byteOffset +offset, length);
const locale_name
		= Buffer.from(bytes).toString('utf8'); // TODO: avoid depending Node.js::Buffer
		callback(locale_name);
	});
}

function _string_get(strings, offset) {
BUG(strings.offset <= offset < strings.offset +strings.used);
const length
	= Uint8Array_castOf(strings.data, offset).indexOf(0x00);
BUG(0 < length);
	return Uint8Array_castOf(strings.data, offset, length); // ref
}

function _read_namehash(src, offset = 0) {
BUG(offset +sizeofNAMEHASH <= src.byteLength);
const endian
	= DataView_castOf(src, offset, sizeofNAMEHASH);

	return {
		hashval       : endian.getUint32(+0x00, LE),
		name_offset   : endian.getUint32(+0x04, LE),
		locrec_offset : endian.getUint32(+0x08, LE),
	};
}

///////////////////////////////////////////////////////////////
// LOCRECTAB access

const categories = new Map([
	  ['CTYPE'         , LC_CTYPE         ]
	, ['NUMERIC'       , LC_NUMERIC       ]
	, ['TIME'          , LC_TIME          ]
	, ['COLLATE'       , LC_COLLATE       ]
	, ['MONETARY'      , LC_MONETARY      ]
	, ['MESSAGE'       , LC_MESSAGE       ]
	, ['ALL'           , LC_ALL           ]
	, ['PAPER'         , LC_PAPER         ]
	, ['NAME'          , LC_NAME          ]
	, ['ADDRESS'       , LC_ADDRESS       ]
	, ['TELEPHONE'     , LC_TELEPHONE     ]
	, ['MEASUREMENT'   , LC_MEASUREMENT   ]
	, ['IDENTIFICATION', LC_IDENTIFICATION]
]);
const categories_toString = [];
for (const [desc, i] of categories.entries()) {
	categories_toString[i] = desc;
}

function _locrectab_index_from(locrectab_offset, locrec_offset) {
const locrec_index
	= (locrec_offset - locrectab_offset) / sizeofLOCREC;
	return locrec_index;
}

function _read_locrec(src, offset) {
const endian
	= DataView_castOf(src, offset +4, sizeofLOCREC -4); // without refcount
BUG(endian instanceof DataView);
	if (! (sizeofLOCREC -4 <= endian.byteLength))
		return null; // error: invalid offset

const retval
	= {};
	for (const [desc, i] of categories.entries()) {
		retval[i] = {
			offset: endian.getUint32(i * 8 +0x00, LE),
			length: endian.getUint32(i * 8 +0x04, LE),
		};
		retval[`LC_${desc}`] = retval[i];
	}
	return retval;
}

function _write_locrec(dst, offset, data) {
const endian
	= DataView_castOf(dst, offset +4, sizeofLOCREC -4); // without refcount
BUG(endian instanceof DataView);
	if (! (sizeofLOCREC -4 <= endian.byteLength))
		return null; // error: invalid offset

	for (let i = 0; i < LC_LAST; ++i) {
		endian.setUint32(i * 8 +0x00, data[i].offset, LE);
		endian.setUint32(i * 8 +0x04, data[i].length, LE);
	}
}

function _locrectab_forEach(locrectab, callback) {
BUG(typeof callback === 'function');
const list
	= [];
	for (let i = 0; i < locrectab.used; ++i)
		list.push(locrectab.offset + sizeofLOCREC * i);
	list.sort((lhs, rhs) => {
		return (lhs == rhs) ? 0 : (lhs < rhs) ? -1 : 1;
	});
	list.unique((lhs, rhs) => {
		return (lhs == rhs) ? 0 : (lhs < rhs) ? -1 : 1;
	});
	for (const locrec_offset of list)
		callback(locrec_offset);
}

function _locrec_enum_data(locrectab, locrec_offset, callback) {
BUG(typeof callback === 'function');

let retval
	= 0;
const l
	= _read_locrec(locrectab.data, locrec_offset);
	for (const i of categories.values()) {
		if (LC_ALL === i || 0 === l[i].length)
			continue;
		if (l.LC_ALL.offset <= l[i].offset
		 && l[i].offset < l.LC_ALL.offset + l.LC_ALL.length) {
//assert(l.LC_ALL.offset < l[i].offset + l[i].length
//    && l[i].offset + l[i].length <= l.LC_ALL.offset + l.LC_ALL.length)
			continue;
		}
		if (callback)
			callback(l[i].offset, l[i].length, i);
		++retval;
	}
	if (0 < l.LC_ALL.length) {
		callback(l.LC_ALL.offset, l.LC_ALL.length, LC_ALL);
		++retval;
	}
	return retval;
}

function _locrectab_enum_data(locrectab, callback) {
BUG(typeof callback === 'function');
const file_list
	= [];
	_locrectab_forEach(locrectab, (locrec_offset) => {
		_locrec_enum_data(locrectab, locrec_offset, (file_offset, file_length, category_index) => {
			file_list.push({
				data: { offset: file_offset, length: file_length }, // ref
				locrec_offset: locrec_offset,
				category_index: category_index,
			});
		});
	});
	file_list.sort((lhs, rhs) => {
		if (! (lhs.data.offset === rhs.data.offset))
			return (lhs.data.offset < rhs.data.offset) ? -1 : 1;
		if (! (lhs.data.length === rhs.data.length))
			return (lhs.data.length < rhs.data.length) ? -1 : 1;
		if (! (lhs.locrec_offset === rhs.locrec_offset))
			return (lhs.locrec_offset < rhs.locrec_offset) ? -1 : 1;
		return 0;
	});
	file_list.unique((lhs, rhs) => {
		if (! (lhs.data.offset === rhs.data.offset))
			return (lhs.data.offset < rhs.data.offset) ? -1 : 1;
		if (! (lhs.data.length == rhs.data.length))
			return (lhs.data.length < rhs.data.length) ? -1 : 1;
		return 0;
	});
	for (let i of file_list)
		callback(i.data, i.locrec_offset, categories_toString[i.category_index]);
}

function _locrectab_resize_data(locrectab, data_offset, old_length, new_length, opListener) {
// code readability
	function PADDING(align, x) { return (align -1) - (x + align -1) % align; }
	function ALIGN(align, x) { return x + PADDING(align, x); }

BUG(old_length < new_length);
const [insert_before, insert_size]
	= [data_offset + old_length, new_length - old_length];
	_locrectab_forEach(locrectab, (locrec_offset) => {
const l
		= _read_locrec(locrectab.data, locrec_offset);
let is_dirty = false;
		for (let id = 0; id < LC_LAST; ++id) {
			if (insert_before <= l[id].offset) {
				is_dirty = true;
				l[id].offset += insert_size;
				continue;
			}
let end;
			if (ALIGN(16, end = l[id].offset + l[id].length) < insert_before)
				continue;
BUG(end == insert_before || LC_ALL == id);
			is_dirty = true;
			l[id].length += insert_size;
		}
		if (is_dirty)
			_write_locrec(locrectab.data, locrec_offset, l);
	});

	if (opListener)
		opListener(locrectab.offset, sizeofLOCREC * locrectab.used, OP_UPDATE);
}

///////////////////////////////////////////////////////////////
// SUMHASH access

function _read_sumhash(src, offset) {
	if (src && src.buffer instanceof ArrayBuffer)
		offset += src.byteOffset, src = src.buffer;
BUG(src instanceof ArrayBuffer);
BUG(offset + sizeofSUMHASH <= src.byteLength);
const endian
	= new DataView(src, offset, sizeofSUMHASH); // ref
const retval = {
		file_offset: endian.getUint32(+0x10, LE),
		sum        : src.slice(offset, offset +0x10),
	};
	return retval;
}

function _write_sumhash(_dst, offset, sum, file_offset) {
	if (_dst && _dst.buffer instanceof ArrayBuffer)
		offset += _dst.byteOffset, _dst = _dst.buffer;
BUG(_dst instanceof ArrayBuffer);
BUG(offset + sizeofSUMHASH <= _dst.byteLength);
	if (sum instanceof ArrayBuffer)
		sum = new Uint8Array(sum);
BUG(sum instanceof Uint8Array);
BUG(0x10 <= sum.byteLength);
let endian, dst, src;
	endian = new DataView(_dst, offset, sizeofSUMHASH); // ref
	dst = new Uint8Array(_dst, offset, sizeofSUMHASH); // ref

	memcpy(dst, +0x00, sum, +0, 0x10);
	endian.setUint32(+0x10, file_offset, LE);
}

function _sumhash_lookup(sumhash, sum/*, offset, length*/) {
BUG(sum.buffer instanceof ArrayBuffer);
//	if (sum instanceof ArrayBuffer)
//		sum = new Uint8Array(sum, offset, length);
BUG(16 === sum.byteLength);
let hval, index;
	index = (hval = _hashval(sum)) % sumhash.maxsize;

const found
	= _read_sumhash(sumhash.data, sumhash.offset + sizeofSUMHASH * index);
	if (! (found && 0 < found.file_offset))
		return null;
	found.number = index +1;
	return found;
}

function _sumhash_remove(sumhash, index) {
	if (! (index < sumhash.maxsize))
		return false;
const offset
	= sumhash.offset + sizeofSUMHASH * index;
const target
	= Uint8Array_castOf(sumhash.data, offset, sizeofSUMHASH); // ref
	target.fill(0);
	return true;
}

function _sumhash_append(sumhash, sum, file_offset) {
	if (sum instanceof ArrayBuffer)
		sum = new Uint8Array(sum);
BUG(sum instanceof Uint8Array);

let hval, index, offset;
	index = (hval = _hashval(sum)) % sumhash.maxsize;
	offset = sumhash.offset + sizeofSUMHASH * index;
	// TODO: distination area zero check
	_write_sumhash(sumhash.data, offset, sum, file_offset);
}

function _sumhash_enum(sumhash, callback) {
const empty
	= new ArrayBuffer(16); // initialized to 0

	for (let index = 0; index < sumhash.maxsize; ++index) {
const sumhash_offset
		= sumhash.offset + sizeofSUMHASH * index;
const item
		= _read_sumhash(sumhash.data, sumhash_offset);
		if (0 == memcmp(empty, +0, item.sum, +0, 16) || 0 == item.file_offset)
			continue; // empty
		callback(sumhash_offset);
	}
}

function _sumhash_resize_data(sumhash, data_offset, old_length, new_length, opListener) {
BUG(old_length < new_length);
const [insert_before, insert_size]
	= [data_offset + old_length, new_length - old_length];
	_sumhash_enum(sumhash, (sumhash_offset) => {
const item
		= _read_sumhash(sumhash.data, sumhash_offset);
		if (! (insert_before <= item.file_offset))
			return;
		item.file_offset += insert_size;
		_write_sumhash(sumhash.data, sumhash_offset, item.sum, item.file_offset);
	});

	if (opListener)
		opListener(sumhash.offset, sizeofSUMHASH * sumhash.maxsize, OP_UPDATE);
}

///////////////////////////////////////////////////////////////
// ARCHIVE access

function _archive_get_data(buffers, data_offset, data_length) {
let v;
	if (! (v = _buffers_subref(buffers, data_offset, data_length)))
		return null;
const retval
	= ArrayBuffer_concat(v); // copy
	return retval;
}

function _archive_set_data(buffers, old_offset, old_length, new_data, opListener) {
	new_data = Uint8Array_castOf(new_data);
BUG(new_data instanceof Uint8Array);
// code readability
	function PADDING(align, x) { return (align -1) - (x + align -1) % align; }

const [new_padding, old_padding]
	= [PADDING(16, new_data.byteLength), PADDING(16, old_length)];
	if (opListener)
		opListener(old_offset + new_data.byteLength, new_padding, OP_PADDING, 0x00);
	if (! (old_padding == new_padding))
		_buffers_splice(buffers, old_offset + old_length, old_padding, new ArrayBuffer(new_padding));

let archive_end;
	if (opListener && old_offset + old_length + old_padding < (archive_end = _buffers_length(buffers)))
		opListener(
			old_offset + new_data.byteLength + new_padding,
			archive_end - (old_offset + old_length + old_padding),
			OP_FORWARD,
			(new_data.byteLength + new_padding) - (old_length + old_padding)
			);
	_buffers_splice(buffers, old_offset, old_length, new_data);
}

///////////////////////////////////////////////////////////////
// NAMEHASH access [public]

function ArchiveNameHash(_buffers, offset, maxsize) {
BUG(_buffers instanceof Array);

	Object.defineProperty(this, 'offset', { value: offset, writable: false });
	Object.defineProperty(this, 'maxsize', { value: maxsize, writable: false });
	Object.defineProperty(this, '_buffers', { value: _buffers, writable: false });
	Object.defineProperty(this, 'data', { get: () => {
BUG(this.offset + sizeofNAMEHASH * this.maxsize <= this._buffers[0].byteLength);
		return this._buffers[0];
	}, });
}

///////////////////////////////////////////////////////////////
// STRING access [public]

function ArchiveString(_buffers, offset, used) {
BUG(_buffers instanceof Array);

	Object.defineProperty(this, 'offset', { value: offset, writable: false });
	Object.defineProperty(this, 'used', { value: used, writable: false });
	Object.defineProperty(this, '_buffers', { value: _buffers, writable: false });
	Object.defineProperty(this, 'data', { get: () => {
BUG(this.offset + this.used <= this._buffers[0].byteLength);
		return this._buffers[0];
	}, });
}
ArchiveString.prototype.forEach = function(callback) {
	return _string_enum(this, callback);
}

///////////////////////////////////////////////////////////////
// LOCRECTAB access [public]

function ArchiveRecord(_buffers, offset, used) {
BUG(_buffers instanceof Array);

	Object.defineProperty(this, 'offset', { value: offset, writable: false });
	Object.defineProperty(this, 'used', { value: used, writable: false });
	Object.defineProperty(this, '_buffers', { value: _buffers, writable: false });
	Object.defineProperty(this, 'data', { get: () => {
BUG(this.offset + sizeofLOCREC * this.used <= this._buffers[0].byteLength);
		return this._buffers[0];
	}, });
}
ArchiveRecord.prototype.parse = function(locrec_offset) {
	return _read_locrec(this.data, locrec_offset);
}
ArchiveRecord.prototype.indexOf = function(locrec_offset) {
	return _locrectab_index_from(this.offset, locrec_offset);
}
ArchiveRecord.prototype.forEach = function(callback) {
	return _locrectab_forEach(this, callback);
}
ArchiveRecord.prototype.enumData = function(callback) {
	return _locrectab_enum_data(this, callback);
}

///////////////////////////////////////////////////////////////
// SUMHASH access [public]

function ArchiveSumHash(_buffers, offset, maxsize) {
BUG(_buffers instanceof Array);

	Object.defineProperty(this, 'offset', { value: offset, writable: false });
	Object.defineProperty(this, 'maxsize', { value: maxsize, writable: false });
	Object.defineProperty(this, '_buffers', { value: _buffers, writable: false });
	Object.defineProperty(this, 'data', { get: () => {
BUG(this.offset + sizeofSUMHASH * this.maxsize <= this._buffers[0].byteLength);
		return this._buffers[0];
	}, });
}
ArchiveSumHash.prototype.lookup = function(sum) {
	return _sumhash_lookup(this, sum);
}
ArchiveSumHash.prototype.remove = function(index) {
	return _sumhash_remove(this, index);
}

///////////////////////////////////////////////////////////////
// ARCHIVE access [public]

function LocaleArchive(archive) {
	if (!(this instanceof LocaleArchive)) new LocaleArchive(archive);
BUG(archive instanceof ArrayBuffer);
	if (! (archive.byteLength < ARCHIVE_MAXSIZE)) {
warn(sprintf(`${VTRR}warning${VT0}: %d < ${VTRR}%d${VT0}: file size too large. unexpected and unchecked on source code.`, ARCHIVE_MAXSIZE -1, archive.byteLength));
	}

	this._opListener = null;
	this._buffers = [new Uint8Array(archive, 0, archive.byteLength)];
	this._buffer = null;

	Object.defineProperty(this, 'byteOffset', { value: 0, writable: false });
	Object.defineProperty(this, 'byteLength', { get() {
		return _buffers_length(this._buffers);
	}});
	Object.defineProperty(this, 'buffer', { get() {
		if (!this._buffer)
			this._buffer = ArrayBuffer_concat(this._buffers);
		return this._buffer;
	}});

const head
	= _parse_header(archive);
	this.namehash  = new ArchiveNameHash(this._buffers, head.namehash_offset, head.namehash_size); // ref
	this.strings   = new ArchiveString(this._buffers, head.string_offset, head.string_used); // ref
	this.locrectab = new ArchiveRecord(this._buffers, head.locrectab_offset, head.locrectab_used); // ref
	this.sumhash   = new ArchiveSumHash(this._buffers, head.sumhash_offset, head.sumhash_size); // ref
}

/* overload:
	a) (data_offset, data_length)
	b) (locale_name, category_index)
 */
LocaleArchive.prototype.get = function(arg1, arg2) {
let target;
	if (typeof arg1 == 'number' && typeof arg2 == 'number')
		target = { offset: arg1, length: arg2 };
	else if (typeof arg1 == 'string' && typeof arg2 == 'number') {
let found, bytes, locale_name;
		if (! (found = _namehash_lookup_locale(this.namehash, bytes = Buffer.from(locale_name = arg1, 'utf8')))) // TODO: should avoid depending Node.js::Buffer
			return null;
CONFLICT(0 == memcmp(bytes, +0, _string_get(this.strings, found.name_offset), +0)); // invalid data
const l
		= _read_locrec(this.locrectab.data, found.locrec_offset);
let category_index;
		target = l[category_index = arg2];
	}
	else {
BUG(false);
	}
	return _archive_get_data(this._buffers, target.offset, target.length); // copy
}

/* PENDING: (*1) now runnable only once.
                 want MD5 init(), update() and finish() interface for changing this behavior.
 */
LocaleArchive.prototype.set = function(locale_name, category_index, new_data) {
BUG(typeof category_index == 'number');
let found, bytes;
	if (! (found = _namehash_lookup_locale(this.namehash, bytes = Buffer.from(locale_name, 'utf8')))) // TODO: should avoid depending Node.js::Buffer
		return false;
CONFLICT(0 == memcmp(bytes, +0, _string_get(this.strings, found.name_offset), +0)); // invalid data

const l
	= _read_locrec(this.locrectab.data, found.locrec_offset);
const old_data
	= l[category_index];
BUG(old_data.offset + old_data.length <= this._buffers[0].byteLength); // PENDING: (*1)
let oldsum, old_sumhash;
	oldsum = md5sum(this._buffers[0], old_data.offset, old_data.length);
	if (! (old_sumhash = this.sumhash.lookup(oldsum))) {
console.error(sprintf('%s: md5sum cannot found.', hexdump(oldsum, 16))); // PENDING: output beside caller.
		return false;
	}

	_archive_set_data(this._buffers, old_data.offset, old_data.length, new_data, this._opListener);
	this._buffer = null;

	if (old_data.length < new_data.byteLength) {
		_locrectab_resize_data(this.locrectab, old_data.offset, old_data.length, new_data.byteLength, this._opListener);
		_sumhash_resize_data(this.sumhash, old_data.offset, old_data.length, new_data.byteLength, this._opListener);
	}

	_sumhash_remove(this.sumhash, old_sumhash.number -1);
	_sumhash_append(this.sumhash, md5sum(new_data), old_data.offset);

	return true;
}

LocaleArchive.prototype.locrecOffsetOf = function(locale_name) {
let found, bytes;
	bytes = Buffer.from(locale_name, 'utf8');
	if (! (found = _namehash_lookup_locale(this.namehash, bytes))) // TODO: should avoid depending Node.js::Buffer
		return null;
CONFLICT(0 == memcmp(bytes, +0, _string_get(this.strings, found.name_offset), +0)); // invalid data

	return found.locrec_offset;
}
LocaleArchive.prototype.offsetOf = function(locale_name, category_index) {
BUG(typeof category_index == 'number');
let found, bytes;
	if (! (found = _namehash_lookup_locale(this.namehash, bytes = Buffer.from(locale_name, 'utf8')))) // TODO: should avoid depending Node.js::Buffer
		return null;
CONFLICT(0 == memcmp(bytes, +0, _string_get(this.strings, found.name_offset), +0)); // invalid data

const l
	= _read_locrec(this.locrectab.data, found.locrec_offset);
	return l[category_index].offset;
}

LocaleArchive.prototype.setOpListener = function(callback) {
	this._opListener = callback;
}

module.exports = LocaleArchive;
module.exports.setLogHandler = _setLogHandler;
