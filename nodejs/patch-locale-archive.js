#!/usr/bin/node
const exit = process.exit;
const EventEmitter = require('events');
const fs = require('fs');
const readline = require('readline');
// node_modules
const sprintf = require('sprintf-js').sprintf;
const md5sum = require('./modules-helper').md5sum;

const {
	OP_UPDATE,
	OP_INSERT,
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
	VT0, VTRR, VTGG, VTYY, VTBB, VTMM
} = require('./vt100-constant');
const {
	readFile,
} = require('./fs-helper');
const {
} = require('./printf-helper');

const {
	DEFAULT_INPUT_PATH,
	DEFAULT_OUTPUT_PATH,
	LC_CTYPE,
	NL_CTYPE_WIDTH,
} = require('./constant');
const hexdump = require('./printf-helper').hexdump;
const LocaleArchive = require('./archive'); LocaleArchive.setLogHandler(applog);
const CType = require('./ctype');
const WcWidth = require('./wcwidth');

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
// log

var g_loglevel = LL_INFO;

var g_logfd = 0;
function log_shutdown() {
	if (!g_logfd) return;
	fs.closeSync(g_logfd);
	g_logfd = 0;
}
function log_warmup() {
//	if (g_logfd || !LOGPATH) return;
//	g_logfd = fs.openSync(LOGPATH, 'a');
}
function filelog(s = '') {
	log_warmup();
	if (!g_logfd) return;
	fs.writeSync(g_logfd, s + '\n');
}
function applog(loglevel, s) {
	if (g_loglevel < loglevel) return;
	console.log(s);
	filelog(s);
}
function debug  (s = '') { applog(LL_DEBUG  , s); }
function verbose(s = '') { applog(LL_VERBOSE, s); }
function info   (s = '') { applog(LL_INFO   , s); }
function warn   (s = '') { applog(LL_WARN   , s); }

///////////////////////////////////////////////////////////////
// display archive information

function _strings_dump(archive) {
console.log(sprintf(`${VTYY}STRING %4s LOCREC %1s LC_CTYPE${VT0}`, '', ''));
	archive.strings.forEach((locale_name) => {
let locrec_offset;
		if (! (locrec_offset = archive.locrecOffsetOf(locale_name)))
			return;
let l;
		l = archive.locrectab.parse(locrec_offset);
console.log(sprintf('%-10s  #%-2d  %2X%04X,%05X'
		, locale_name
		, archive.locrectab.indexOf(locrec_offset) +1
		, l.LC_CTYPE.offset / 0x10000
		, l.LC_CTYPE.offset % 0x10000
		, l.LC_CTYPE.length
		));
	});
}

function _locrectab_dump(archive) {
console.log(sprintf(`${VTYY}LOCREC %2s LC_ALL %4s LC_CTYPE %3s LC_COLLATE${VT0}`, '', '', ''));
	archive.locrectab.forEach((locrec_offset) => {
let l;
		l = archive.locrectab.parse(locrec_offset);
console.log(sprintf('#%-2d  %3X%03X,%-4X  %3X%03X,%-5X  %3X%03X,%-6X'
		, archive.locrectab.indexOf(locrec_offset) +1
		, l.LC_ALL.offset / 0x1000
		, l.LC_ALL.offset % 0x1000
		, l.LC_ALL.length
		, l.LC_CTYPE.offset / 0x1000
		, l.LC_CTYPE.offset % 0x1000
		, l.LC_CTYPE.length
		, l.LC_COLLATE.offset / 0x1000
		, l.LC_COLLATE.offset % 0x1000
		, l.LC_COLLATE.length
		));
	});
}

function _sumhash_dump(archive) {
console.log(sprintf(`${VTYY}LOCREC-table %18s DATA-sum %24s SUMHASH-lookup${VT0}`, '', ''));
	archive.locrectab.enumData((data, locrec_offset, category_name) => {
let sum, sumhash;
		sum = md5sum(archive.get(data.offset, data.length)); // ref
		sumhash = archive.sumhash.lookup(sum);
console.log(sprintf('#%-2d  LC_%-7s  %6X,%-6X  %s  #%-4d  %6X  %s'
		, archive.locrectab.indexOf(locrec_offset) +1
		, category_name
		, data.offset
		, data.length
		, hexdump(sum)
		, sumhash.number
		, sumhash.file_offset
		, (0 == sumhash.number) ? `(${VTRR}not found${VT0})` : hexdump(sumhash.sum)
		));
	});
}

///////////////////////////////////////////////////////////////
// operation log

const [ ID_ARCHIVE, ID_CTYPE, ID_WCWIDTH ] = [0, 1, 2];

function OpLog() {
	if (!(this instanceof OpLog)) new OpLog();
	this.op_list = new Array();
	this.base_blocks = new Map();
}

const [ BEFORE/*, AFTER*/ ] = [ 1/*, 2*/ ];
OpLog.prototype.entry = function(desc, length, flags, base_desc, offset) {
BUG(!this.base_blocks.has(desc));
const item1
	= { offset: 0, length: length };
	if (undefined === flags) {
BUG(undefined === base_desc && 0 == this.base_blocks.size);
		this.base_blocks.set(desc, item1);
		return;
	}
BUG(BEFORE === flags);
// PENDING: insert not top
	this.base_blocks.forEach((v) => {
		v.offset += offset;
	});
	this.base_blocks.set(desc, item1);
}

OpLog.prototype.done = function(desc, offset, length, op_id, aux) {
	this.op_list.push({ desc: desc, offset: offset, length: length, op_id: op_id, aux: aux });
}


OpLog.prototype.commit = function() {
// caption header
const caption1
	= sprintf(`${VTGG}%12s %12s %8s${VT0}  ${VTYY}%-12s  %s${VT0}`
		, 'operation', 'length', 'archive', 'LC_CTYPE', '_WIDTH'
		);
console.log(caption1);

// archive address calculation
	for (const o of this.op_list) {
BUG(this.base_blocks.has(o.desc));
		o.addr = this.base_blocks.get(o.desc).offset + o.offset;
	}

// descent sort
	this.op_list.sort((lhs, rhs) => {
		return (lhs.addr > rhs.addr) ? -1 : (lhs.addr == rhs.addr) ? 0 : 1;
	});

//
BUG(this.base_blocks.get(ID_CTYPE));
BUG(this.base_blocks.get(ID_WCWIDTH));
const 
	ctype_begin = this.base_blocks.get(ID_CTYPE).offset,
	ctype_end   = this.base_blocks.get(ID_CTYPE).offset + this.base_blocks.get(ID_CTYPE).length,
	wcwidth_begin = this.base_blocks.get(ID_WCWIDTH).offset,
	wcwidth_end   = this.base_blocks.get(ID_WCWIDTH).offset + this.base_blocks.get(ID_WCWIDTH).length;

// expanded range output
const caption2
	= sprintf(`%12s %12s %8s  %6X,%-5x  %6X,%x`
		, '', '', ''
		, ctype_begin, ctype_end - ctype_begin
		, wcwidth_begin, wcwidth_end - wcwidth_begin
		);
console.log(caption2);
		
// body output
const output_info = new Map([
	[OP_UPDATE , { name: 'update' , sgr: VTYY }],
	[OP_INSERT , { name: 'insert' , sgr: VTGG }],
	[OP_FORWARD, { name: 'forward', sgr: VTMM }],
	[OP_PADDING, { name: 'padding', sgr: VTBB }],
]);

	for (const o of this.op_list) {
const v
		= output_info.get(o.op_id);
let aux = '    ';
		if (OP_FORWARD == o.op_id)
			aux = sprintf('+%-3x', o.aux);
		else if (OP_PADDING == o.op_id)
			aux = sprintf('  %02x', o.aux);
let s
		= sprintf(`${aux} ${v.sgr}%7s${VT0} %6x ${v.sgr}bytes${VT0}  %06X:`, v.name, o.length, o.addr);
		if (ctype_begin <= o.addr && o.addr <= ctype_end) {
			s += sprintf('  +%05x:', o.addr - ctype_begin);
		}
		if (wcwidth_begin <= o.addr && o.addr <= wcwidth_end) {
			s += sprintf('  +%04x:', o.addr - wcwidth_begin);
		}
console.log(s);
	}
}

///////////////////////////////////////////////////////////////
// dispatching commands

const 
	STDIN_INPUT  = 1,
	HEX_INPUT    = 2,
	UTF16_INPUT  = 4,
	STRING_DUMP  = 8,
	LOCREC_DUMP  = 16,
	SUMHASH_DUMP = 32,
	ANY_DUMP = STRING_DUMP | LOCREC_DUMP | SUMHASH_DUMP
;

function _inspect(archive, options) {
BUG(archive instanceof LocaleArchive);

	if (STRING_DUMP & options.flags)
		_strings_dump(archive);
	if (LOCREC_DUMP & options.flags)
		_locrectab_dump(archive);
	if (SUMHASH_DUMP & options.flags)
		_sumhash_dump(archive);
}

function _patch(archive, options, locale_name, unicode_list, outfd) {
BUG(archive instanceof LocaleArchive);
// code readability
	function PADDING(align, x) { return (align -1) - (x + align -1) % align; }

let ctype;
	if (! (ctype = new CType(archive.get(locale_name, LC_CTYPE)))) { // copy
console.error(sprintf('%s: locale cannot found.', locale_name));
		return false;
	}

let wcwidth;
	if (! (wcwidth = new WcWidth(ctype.get(NL_CTYPE_WIDTH)))) {
console.error(sprintf('+%x,%x: LC_CTYPE: #%d(wcwidth-data) cannot read.', ctype.offset, ctype.length, _NL_CTYPE_WIDTH));
		return false;
	}

const oplog
	= new OpLog();
	wcwidth.setOpListener((offset, length, op_id, aux) => { oplog.done(ID_WCWIDTH, offset, length, op_id, aux); });
	  ctype.setOpListener((offset, length, op_id, aux) => { oplog.done(ID_CTYPE  , offset, length, op_id, aux); });
	archive.setOpListener((offset, length, op_id, aux) => { oplog.done(ID_ARCHIVE, offset, length, op_id, aux); });

	for (let u21 of unicode_list)
		wcwidth.modify(u21, 2);
	wcwidth.commit();
	oplog.entry(ID_WCWIDTH, wcwidth.length);

	ctype.set(NL_CTYPE_WIDTH, wcwidth.buffer);
	oplog.entry(ID_CTYPE, ctype.length, BEFORE, ID_WCWIDTH, ctype.resolve(NL_CTYPE_WIDTH));

	archive.set(locale_name, LC_CTYPE, ctype.buffer);
	oplog.entry(ID_ARCHIVE, archive.byteLength, BEFORE, ID_CTYPE, archive.offsetOf(locale_name, LC_CTYPE));
	oplog.commit();

console.log(`${VTYY}${options.outpath}${VT0}: writing ...`);
	fs.writeSync(outfd, new Uint8Array(archive.buffer)); // ref
	return true;
}

///////////////////////////////////////////////////////////////
// command line option(s) parser

function OptionsParser(options) {
	if (!(this instanceof OptionsParser)) new OptionsParser();

	this.errcount = 0;
	this.isHelp = false;
	this.state = null;

	if (!options)
		options = new Object(); // dummy
BUG(options instanceof Object);
	options.inpath = DEFAULT_INPUT_PATH;
	options.outpath = DEFAULT_OUTPUT_PATH;
	options.flags = 0;
	this.options = options;
}

OptionsParser.prototype.accept = function(s) {
	if (!this.state) {
		// not option
		if (!s.startsWith('-'))
			return false;
		// single token option
		else if ('--help' === s)
			this.isHelp = true;
		else if ('-' === s)
			this.options.flags |= STDIN_INPUT;
		else if ('-h' === s || '--hex' === s)
			this.options.flags |= HEX_INPUT;
		else if ('-w' === s || '--utf16' === s)
			this.options.flags |= UTF16_INPUT;
		else if ('-s' === s || '--string-dump' === s)
			this.options.flags |= STRING_DUMP;
		else if ('-r' === s || '--record-dump' === s)
			this.options.flags |= LOCREC_DUMP;
		else if ('-m' === s || '--checksum-dump' === s)
			this.options.flags |= SUMHASH_DUMP;
		// multi token option first
		else if ('-i' === s)
			this.state = 'i';
		else if ('-o' === s)
			this.state = 'o';
		else {
console.log(`${s}: ${VTRR}invalid option${VT0}`);
			++this.errcount;
		}
		return true;
	}
	// multi token option later
	if ('i' === this.state)
		this.options.inpath = s;
	else /*if ('o' === this.state)*/
		this.options.outpath = s;
	this.state = null;
	return true;
}

///////////////////////////////////////////////////////////////
// help

function _printUsage() {
console.log('usage: [<opt>] <locale-name> [-|<char> <char>...]');
console.log('   -i <archive-path>');
console.log('   -o <output-path>');
console.log('   -O, --to-stdout');
console.log('   -h, --hex');
console.log('   -w, --utf16');
console.log('   -s, --string-dump');
console.log('   -r, --record-dump');
console.log('   -m, --sumhash-dump');
}

///////////////////////////////////////////////////////////////
// main

function Main() {
	if (!(this instanceof Main)) new Main();

BUG(this instanceof EventEmitter)
	EventEmitter.call(this);
}
Object.setPrototypeOf(Main.prototype, EventEmitter.prototype);
//Object.setPrototypeOf(Main, EventEmitter); // PENDING: correctly understand what is caused.

Main.prototype.execute = function(argv) {
// code readability
	function _easy_tokenize(s, index) {
		let a = s.split(/\s+/);
		if (0 < a.length && '' == a[0]) a.shift();
		return (0 <= index && index < a.length) ? a[index] : '';
	}
	function _parse_unicode(s, flags) {
		if (! ('string' === typeof s && 0 < s.length)) return 0;
		if (! (HEX_INPUT & flags)) return s.charCodeAt(0);
		if (! (-1 == s.search(/[^0-9A-Fa-f]/))) return 0;
		if (UTF16_INPUT & flags) return Number.parseInt(s, 16);
		return 0; // TODO: utf8 -> utf16 conversion
	}

// options parsing
	argv.shift(); // '/usr/bin/node'
	argv.shift(); // '<this-module>.js'
const options = {}, argv_without_options = [];
const parser
	= new OptionsParser(options);
	for (const s of argv)
		if (!parser.accept(s))
			argv_without_options.push(s);
	if (0 < parser.errcount)
		this.emit('abort');
	if (parser.isHelp) {
		_printUsage ();
		return;
	}

// original 'locale-archive' reading
console.log(`${VTYY}${options.inpath}${VT0}: reading ...`);
const archive
	= new LocaleArchive(readFile(options.inpath));

// print overview of specified 'locale-archive'
	if (ANY_DUMP & options.flags) {
		_inspect(archive, options);
		return;
	}

	if (0 == argv_without_options.length) {
console.log('<locale-name> not specified.');
		this.emit('abort');
	}
const locale_name
	= argv_without_options.shift();

// create unicode list of target wide-character(s)
const unicode_list = [];
let u21;
	if (STDIN_INPUT & options.flags) {
		process.stdin.setEncoding('utf8'); // PENDING: utf16 text input support
		readline.createInterface({ input: process.stdin })
		.on('line', (s) => {
			if (u21 = _parse_unicode(_easy_tokenize(s, 0), options.flags))
				unicode_list.push(u21);
		})
		.on('close', () => {
			this.emit('unicode-list-ready', archive, options, locale_name, unicode_list);
		});
	}
	else { // specify by command-line
		for (s of argv_without_options)
			if (u21 = _parse_unicode(s, options.flags))
				unicode_list.push(u21);
		this.emit('unicode-list-ready', archive, options, locale_name, unicode_list);
	}
}

function _on_unicode_list_ready(archive, options, locale_name, unicode_list) {
// 'locale-archive' patching and writing
let outfd;
	try {
		outfd = fs.openSync(options.outpath, 'w', 0o644);
assert(0 < outfd); // 0:stdin 1:stdout 2:stderr ...
	} catch (e) {
console.log(`${VTRR}${options.outpath}${VT0}: file cannot open.`);
		this.emit('abort');
	}
	_patch(archive, options, locale_name, unicode_list, outfd);
	fs.closeSync(outfd);
}

log_warmup();
new Main()
.on('abort', () => {
	log_shutdown();
	exit(-1);
})
.on('unicode-list-ready', _on_unicode_list_ready)
.execute(
	process.argv
);
log_shutdown();
//exit(0);
