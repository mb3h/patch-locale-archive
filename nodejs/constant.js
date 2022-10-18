const ARCHIVE_MAXSIZE = 0x800000;
const DEFAULT_INPUT_PATH = '/usr/lib/locale/locale-archive';
const DEFAULT_OUTPUT_PATH = 'locale-archive';

const [ // PENDING: want enum syntax.
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
] = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ];

const sizeofHEADER   = 0x38;
const sizeofNAMEHASH = 12;
const sizeofLOCREC   = 4 + 8 * LC_LAST;
const sizeofSUMHASH  = 20;

const MAGIC_CTYPE = 0x20090720;

function _NL_ITEM(category, index) { return (category) << 16 | (index); }
const NL_CTYPE_WIDTH = _NL_ITEM(LC_CTYPE, 12);

module.exports = {
	ARCHIVE_MAXSIZE,
	DEFAULT_INPUT_PATH,
	DEFAULT_OUTPUT_PATH,

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

	MAGIC_CTYPE,
	NL_CTYPE_WIDTH,
};
