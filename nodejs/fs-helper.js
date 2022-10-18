const fs = require('fs');

const {
	Uint8Array_castOf,
} = require('./buffer-helper');

const { CodingError } = require('./custom-error');
///////////////////////////////////////////////////////////////
// assertion

function assert(expr, msg = '') {
	if (expr) return;
	throw new CodingError(`${VTRR}ASSERT${VT0} ${msg}`);
}
const BUG = assert;

///////////////////////////////////////////////////////////////

function _writeFile(path, src, offset, length) {
const view
	= Uint8Array_castOf(src, offset, length); // ref

	// erase for overwrite
	if (fs.existsSync(path)) {
		try {
			fs.rmSync(path);
		} catch (e) {
			// PENDING: correct handling TypeError('fs.rmSync is not a function')
			//          which sometimes happen for some reason.
		}
	}
	// write to filesystem
const fd
	= fs.openSync(path, 'w', 0o664);
	if (fd < 3)
		return 0; // invalid file descripter
const written
	= fs.writeSync(fd, view, 0);
	fs.closeSync(fd);
	return written;
}

function _readFile(path) {
const fd
	= fs.openSync(path, 'r');
const length
	= fs.fstatSync(fd).size;
const buf
	= new ArrayBuffer(length);
const got
	= fs.readSync(fd, new Uint8Array(buf), 0, length);
	fs.closeSync(fd);
	return buf;
}

function _appendFile(path, src, offset, length) {
const view
	= Uint8Array_castOf(src, offset, length); // ref

	// write to filesystem
const fd
	= fs.openSync(path, 'a');
const written
	= fs.writeSync(fd, view);
	fs.closeSync(fd);
	return written;
}

module.exports.writeFile  = _writeFile ;
module.exports.readFile   = _readFile  ;
module.exports.appendFile = _appendFile;
