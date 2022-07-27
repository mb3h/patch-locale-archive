/* gcc -std=c99 -o patch-locale-archive patch-locale-archive.c
 */
//#include "config.h"
#define ARCHIVE_MAXSIZE 0x800000
#define DEFAULT_INPUT_PATH "/usr/lib/locale/locale-archive"
#define DEFAULT_OUTPUT_PATH "locale-archive"

#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

#include <stdio.h>
#include <stdlib.h> // exit

#include <string.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <alloca.h>
#include <fcntl.h>
#include <unistd.h> // write

#include "wcwidth.h"
#include "ctype.h"
#include "archive.h"
#include "md5.h"
#include "vt100.h"
#include "assert.h"
#include "endian-helper.h"
#include "unicode-helper.h"
#include "log-helper.h"

typedef uint32_t u32, u21, u23;
typedef  uint8_t u8, u4, u5;
typedef uint16_t u16, u9;

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#define PADDING(align, x) (((align) -1) - ((x) +(align) -1) % (align))
#define ALIGN(align, x) ((x) + PADDING(align, x))
///////////////////////////////////////////////////////////////

static void _print_locale_caption ()
{
printf (VTYY "STRING %*s LOCREC %*s LC_CTYPE" VTO "\n", 4, "", 1, "");
}
static void _print_locale (const char *name, u32 locrec_offset, void *priv)
{
const u8 *ar;
	ar = (const u8 *)priv;
locrecent_s locrecent;
	archive_read_locrec (ar + locrec_offset, &locrecent);
locarhead_s head; unsigned locrec_index;
	archive_read_header (ar, &head);
	locrec_index = (locrec_offset - head.locrectab_offset) / sizeof(locrecent_s);
printf ("%-10s  #%-2d  %2X%04X,%05X" "\n"
, name
, locrec_index +1
, locrecent.record[LC_CTYPE].offset / 0x10000
, locrecent.record[LC_CTYPE].offset % 0x10000
, locrecent.record[LC_CTYPE].size
);
}

static void _print_locrec_caption ()
{
printf (VTYY "LOCREC %*s LC_ALL %*s LC_CTYPE %*s LC_COLLATE" VTO "\n", 2, "", 4, "", 3, "");
}
static void _print_locrec (u32 locrec_offset, void *priv)
{
const u8 *ar;
	ar = (const u8 *)priv;
locrecent_s l; struct locrecent_record *all;
	archive_read_locrec (ar + locrec_offset, &l), all = &l.record[LC_ALL];
locarhead_s head; unsigned index;
	archive_read_header (ar, &head);
	index = (locrec_offset - head.locrectab_offset) / sizeof(locrecent_s);
printf ("#%-2d  %3X%03X,%-4X  %3X%03X,%-5X  %3X%03X,%-6X"
, index +1
, all->offset / 0x1000
, all->offset % 0x1000
, all->size
, l.record[LC_CTYPE].offset / 0x1000
, l.record[LC_CTYPE].offset % 0x1000
, l.record[LC_CTYPE].size
, l.record[LC_COLLATE].offset / 0x1000
, l.record[LC_COLLATE].offset % 0x1000
, l.record[LC_COLLATE].size
);
unsigned i;
	for (i = 0; i < LC_LAST; ++i) {
		if (LC_ALL == i || LC_CTYPE == i || LC_COLLATE == i)
			continue;
		if (! (l.record[i].offset < all->offset || all->offset + all->size <= l.record[i].offset))
			continue;
printf ("  %3X%03X,%-4X " VTYY "LC_%s" VTO
, l.record[i].offset / 0x1000
, l.record[i].offset % 0x1000
, l.record[i].size
, strcategory (i)
);
	}
printf ("\n");
}

static void _print_sumhash_caption ()
{
printf (VTYY "LOCREC-table %*s DATA-sum %*s SUMHASH-lookup" VTO "\n", 18, "", 24, "");
}
static void _print_sumhash (u32 sumhash_offset, u32 locrec_offset, u8 category, void *priv)
{
const u8 *ar;
	ar = (const u8 *)priv;
locrecent_s locrecent; struct locrecent_record *r;
	archive_read_locrec (ar + locrec_offset, &locrecent);
	r = &locrecent.record[category];
u8 md5[16]; sumhashent_s sumhashent; u32 number;
	md5sum (ar +r->offset, r->size, md5);
	number = archive_find_sumhash (ar, md5, &sumhashent);
locarhead_s head; unsigned locrec_index;
	archive_read_header (ar, &head);
	locrec_index = (locrec_offset - head.locrectab_offset) / sizeof(locrecent_s);
printf ("#%-2d  LC_%-7s  %6X,%-6X  %s  #%-4d  %6X  %s" "\n"
, locrec_index +1
, strcategory (category)
, r->offset
, r->size
, hexdump (md5, 16)
, number
, sumhashent.file_offset
, (0 == number) ? "(" VTRR "not found" VTO ")" : hexdump (sumhashent.sum, 16)
);
}

///////////////////////////////////////////////////////////////
// default log

struct operation_log {
	u23 ctype_offset  ;
	u23 ctype_size    ;
	u23 wcwidth_offset;
	u23 wcwidth_size  ;
};
static void operation_log_caption (struct operation_log *m_)
{
printf ("   " VTGG "operation" VTO " %*s " VTGG "length" VTO "  ", 5, "");
if (0 < m_->ctype_offset)
	printf (VTGG "archive" VTO " %*s ", 0, "");
printf (VTYY "LC_CTYPE" VTO " %*s " VTYY "_WIDTH" VTO "\n", 4, "");

printf ("%*s  ", 25, "");
if (0 < m_->ctype_offset)
	printf ("%*s  ", 7, "");
printf ("%6X,%-5x  %6X,%-5x" "\n", m_->ctype_offset, m_->ctype_size, m_->wcwidth_offset, m_->wcwidth_size);
}
static void operation_log_shared (struct operation_log *m_, const char *sgr, const char *name, u23 size, u23 target)
{
printf ("%s" "%s" VTO " %6x " "%s" "bytes" VTO "  ", sgr, name, size, sgr);
	if (0 < m_->ctype_offset)
printf ("%06X:  ", target);
	if (m_->ctype_offset <= target && target <= m_->ctype_offset + ALIGN(16, m_->ctype_size))
printf ("+%05x:%*s", target - m_->ctype_offset, 5, "");
	if (m_->wcwidth_offset <= target && target <= m_->wcwidth_offset + m_->wcwidth_size)
printf ("  %*s+%04x:", 1, "", target - m_->wcwidth_offset);
printf ("\n");
}
static void operation_log_forward (struct operation_log *m_, u23 forward, u23 size, u23 target)
{
printf ("+%-3x ", forward);
	operation_log_shared (m_, VTMM, "forward", size, target);
}
static void operation_log_padding (struct operation_log *m_, int c, u23 size, u23 target)
{
printf ("  %02x ", c);
	operation_log_shared (m_, VTBB, "padding", size, target);
}
static void operation_log_insert (struct operation_log *m_, u23 size, u23 target)
{
printf ("     ");
	operation_log_shared (m_, VTGG, " insert", size, target);
}
static void operation_log_update (struct operation_log *m_, u23 size, u23 target)
{
printf ("     ");
	operation_log_shared (m_, VTYY, " update", size, target);
}

///////////////////////////////////////////////////////////////

// cf) locale/langinfo.h
#define _NL_ITEM(category, index) ((category) << 16 | (index))
#define _NL_CTYPE_WIDTH _NL_ITEM(LC_CTYPE, 12)

enum {
	OPT_HEX          = 1,
	OPT_UTF16        = 2,
	OPT_STRING_DUMP  = 4,
	OPT_LOCREC_DUMP  = 8,
	OPT_SUMHASH_DUMP = 16,
};

static bool _patch (const char *path, const char *locale_name, unsigned flags, unsigned list_len, wchar_t *list, int outfd, const char *outpath)
{
ASSERTE(path && (locale_name && list && 0 < list_len || OPT_STRING_DUMP + OPT_LOCREC_DUMP + OPT_SUMHASH_DUMP & flags))
	// locale-archive read
FILE *fp;
	if (! (fp = fopen (path, "rb"))) {
fprintf (stderr, VTRR "%s" VTO ": file cannot open" "\n", path);
		return -1;
	}
printf (VTYY "%s" VTO ": reading ..." "\n", path);
unsigned archive_size;
	fseek (fp, 0, SEEK_END);
	archive_size = (unsigned)ftell (fp);
	fseek (fp, 0, SEEK_SET);
u8 *ar;
	ar = (u8 *)malloc (archive_size);
	fread (ar, 1, archive_size, fp);
	fclose (fp);

	if (! (archive_size < ARCHIVE_MAXSIZE)) {
fprintf (stderr, VTRR "warning" VTO ": %d < " VTRR "%d" VTO ": file size too large. unexpected and unchecked on source code." "\n", ARCHIVE_MAXSIZE -1, archive_size);
	}

bool retval;
	retval = false;
do {
	if (OPT_STRING_DUMP & flags) {
		_print_locale_caption ();
		archive_enum_locale (ar, _print_locale, ar);
	}
	if (OPT_LOCREC_DUMP & flags) {
		_print_locrec_caption ();
		archive_enum_locrec (ar, _print_locrec, ar);
	}
	if (OPT_SUMHASH_DUMP & flags) {
		_print_sumhash_caption ();
		archive_enum_sumhash (ar, _print_sumhash, ar);
	}
	if (OPT_STRING_DUMP + OPT_LOCREC_DUMP + OPT_SUMHASH_DUMP & flags) {
		retval = true;
		break;
	}

struct locrecent_record ctype;
	if (!archive_find_locrec (ar, locale_name, LC_CTYPE, &ctype)) {
fprintf (stderr, "%s: locale cannot found." "\n", locale_name);
		break;
	}

u8 oldsum[16]; sumhashent_s sumhashent; u32 sumhash_number;
	md5sum (ar + ctype.offset, ctype.size, oldsum);
	if (! (sumhash_number = archive_find_sumhash (ar, oldsum, &sumhashent))) {
fprintf (stderr, "%s: md5sum cannot found." "\n", hexdump (oldsum, 16));
		break;
	}

u23 wcwidth_offset, wcwidth_size;
	if (! (wcwidth_size = ctype_read_position (ar + ctype.offset, ctype.size, _NL_CTYPE_WIDTH, &wcwidth_offset))) {
fprintf (stderr, "+%x,%x: LC_CTYPE: #%d(wcwidth-data) cannot read." "\n", ctype.offset, ctype.size, _NL_CTYPE_WIDTH);
		break;
	}
	wcwidth_offset += ctype.offset;

u8 wct[116 +12]; // bound=17, safety-margin=+12
	if (!wcwidth_ctor (wct, sizeof(wct), ar + wcwidth_offset, wcwidth_size))
		return false;


	// SUMHASH <- clear old hash
	archive_remove_sumhash (ar, sumhash_number -1);
unsigned i;
	for (i = 0; i < list_len; ++i) {
wchar_t w;
		w = list[i];
		wcwidth_modify (wct, ar + wcwidth_offset, w, 2);
	}
unsigned wcwidth_expand;
	wcwidth_expand = wcwidth_commit (wct, NULL, NULL, 0);

struct operation_log log;
	log.ctype_offset   = ctype.offset  ;
	log.ctype_size     = ctype.size     + wcwidth_expand;
	log.wcwidth_offset = wcwidth_offset;
	log.wcwidth_size   = wcwidth_size   + wcwidth_expand;
operation_log_caption (&log);

u32 ctype_old_align, archive_expand;
	if (0 < wcwidth_expand) {
		// LC_CTYPE <- expanded new data
u32 ctype_old_end, ctype_new_end, ctype_new_align;
		ctype_old_end = ctype.offset + ctype.size, ctype_new_end = ctype_old_end + wcwidth_expand;
		ctype_old_align = ALIGN(16, ctype_old_end), ctype_new_align = ALIGN(16, ctype_new_end);
		if (0 < (archive_expand = ctype_new_align - ctype_old_align)) {
operation_log_forward (&log, ctype_new_align - ctype_old_align, archive_size - ctype_old_align, ctype_new_align);
			ar = (u8 *)realloc (ar, archive_size + archive_expand);
			memmove (ar +ctype_new_align, ar +ctype_old_align, archive_size - ctype_old_align);
		}
		if (ctype_new_end < ctype_new_align) {
operation_log_padding (&log, 0x00, ctype_new_align - ctype_new_end, ctype_new_end);
			memset (ar +ctype_new_end, 0, ctype_new_align - ctype_new_end);
		}
u32 wcwidth_old_end, wcwidth_new_end;
		wcwidth_old_end = wcwidth_offset + wcwidth_size, wcwidth_new_end = wcwidth_old_end + wcwidth_expand;
		if (wcwidth_old_end < ctype_old_end) {
operation_log_forward (&log, ctype_new_align - ctype_old_align, ctype_old_end - wcwidth_old_end, wcwidth_new_end);
			memmove (ar + wcwidth_new_end, ar + wcwidth_old_end, ctype_old_end - wcwidth_old_end);
		}
operation_log_insert (&log, wcwidth_expand, wcwidth_offset + wcwidth_size);
	}
operation_log_update (&log, wcwidth_size, wcwidth_offset);
	wcwidth_commit (wct, ar + wcwidth_offset, ar +wcwidth_offset + wcwidth_size, wcwidth_expand);
	wcwidth_dtor (wct);

	// LC_CTYPE <- modify header
	if (0 < wcwidth_expand) {
u23 table_begin, table_end;
		table_end = ctype_read_header (ar + ctype.offset, NULL);
		table_begin = offsetof(ctype_header_s, offsets);
operation_log_update (&log, table_end - table_begin, ctype.offset + table_begin);
		ctype_resize_block (ar + ctype.offset, _NL_CTYPE_WIDTH, wcwidth_expand);
	}

locarhead_s head;
	archive_read_header (ar, &head);
	// SUMHASH <- entry new hash (PENDING: collision recovery)
operation_log_update (&log, sizeof(sumhashent_s) * head.sumhash_size, head.sumhash_offset);
u8 newsum[16];
	md5sum (ar + ctype.offset, ctype.size + wcwidth_expand, newsum);
	archive_append_sumhash (ar, newsum, ctype.offset);
	// LOCRECTAB <- modify .offset and .size if needed
	if (0< wcwidth_expand) {
operation_log_update (&log, sizeof(locrecent_s) * head.locrectab_used, head.locrectab_offset);
		archive_resize_file (ar, ctype_old_align, archive_expand);
		archive_size += archive_expand;
	}

	if (outfd) {
		if (outpath)
printf (VTYY "%s" VTO ": writing ..." "\n", outpath);
int written;
		for (i = 0; i < archive_size && 0 <= (written = (int)write (outfd, ar +i, archive_size -i));)
			i += written;
		if (written < 0) {
fprintf (stderr, VTRR "%s" VTO ": file cannot write. (" VTRR "%s" VTO ")" "\n", (outpath) ? outpath : "<STDOUT>", strerror (errno));
			break;
		}
	}
	retval = true;
} while (0);

	free (ar);
	return retval;
}

static void print_usage ()
{
printf ("usage: [<opt>] <locale-name> [-|<char> <char>...]" "\n");
printf ("   -i <archive-path>" "\n");
printf ("   -o <output-path>" "\n");
printf ("   -O, --to-stdout" "\n");
printf ("   -h, --hex" "\n");
printf ("   -w, --utf16" "\n");
printf ("   -s, --string-dump" "\n");
printf ("   -r, --record-dump" "\n");
printf ("   -m, --sumhash-dump" "\n");
}

int main (int argc, char *argv[])
{
	if (argc < 2) {
		print_usage ();
		return -1;
	}
enum {
	LIST_CMDLINE = 0,
	LIST_STDIN
} target_wchar;
const char *input, *output; unsigned flags, errcount; bool is_help;
	input = DEFAULT_INPUT_PATH, output = DEFAULT_OUTPUT_PATH;
	target_wchar = 0, flags = 0, errcount = 0; is_help = false;
int cmdc, i; char **cmdv;
	cmdc = 0, cmdv = (char **)alloca (sizeof(char *) * (argc -1));
	for (i = 1; i < argc; ++i) {
const char *opt;
		if (! ('-' == (opt = argv[i])[0])) {
			cmdv[cmdc++] = argv[i];
			continue;
		}
		if ('\0' == opt[1])
			target_wchar = LIST_STDIN;
		else if (0 == strcmp ("--help", opt))
			is_help = true;
		else if (0 == strcmp ("-h", opt) || 0 == strcmp ("--hex", opt))
			flags |= OPT_HEX;
		else if (0 == strcmp ("-w", opt) || 0 == strcmp ("--utf16", opt))
			flags |= OPT_UTF16;
		else if (0 == strcmp ("-s", opt) || 0 == strcmp ("--string-dump", opt))
			flags |= OPT_STRING_DUMP;
		else if (0 == strcmp ("-r", opt) || 0 == strcmp ("--record-dump", opt))
			flags |= OPT_LOCREC_DUMP;
		else if (0 == strcmp ("-m", opt) || 0 == strcmp ("--checksum-dump", opt))
			flags |= OPT_SUMHASH_DUMP;
		else if (0 == strcmp ("-O", opt) || 0 == strcmp ("--to-stdout", opt))
			output = NULL;
		else if (0 == strcmp ("-i", opt) && i +1 < argc)
			input = argv[++i];
		else if (0 == strcmp ("-o", opt) && i +1 < argc)
			output = argv[++i];
		else {
fprintf (stderr, "'%s': " VTRR "invalid option" VTO "\n", opt);
			++errcount;
		}
	}
	if (is_help) {
		print_usage ();
		return 0;
	}
	if (0 == cmdc && !(OPT_STRING_DUMP + OPT_LOCREC_DUMP + OPT_SUMHASH_DUMP & flags)) {
fprintf (stderr, "<locale-name> not specified." "\n");
		return -1;
	}
	if (0 < errcount)
		return -1;

wchar_t *list; unsigned list_maxlen;
	list = (wchar_t *)malloc (sizeof(wchar_t) * (list_maxlen = 128));

unsigned list_len;
	list_len = 0, i = 1;
	while (1) {
const char *src; char linebuf[128];
		if (LIST_CMDLINE == target_wchar) {
			if (! (i < cmdc))
				break;
			src = cmdv[i++];
			if ('-' == src[0])
				continue;
		}
		else /*if (LIST_STDIN == target_wchar)*/ {
			if (feof (stdin))
				break;
			*linebuf = '\0';
			fgets (linebuf, sizeof(linebuf), stdin);
			linebuf[sizeof(linebuf) -1] = '\0';
char *tail;
			tail = strchr (linebuf, '\0');
			while (linebuf < tail && strchr (" \t\r\n", tail[-1]))
				*--tail = '\0';
			if (! (linebuf < tail))
				continue;
			src = linebuf;
			while (src < tail && strchr (" \t", *src))
				++src;
		}
wchar_t w;
		if (OPT_HEX & flags && OPT_UTF16 & flags) {
char *endptr;
			w = (wchar_t)strtoul (src, &endptr, 16);
		}
		else if (OPT_HEX & flags && !(OPT_UTF16 & flags)) {
u32 utf8; char *endptr;
			utf8 = (u32)strtoul (src, &endptr, 16);
unsigned len; u8 tmp[3];
			if (! (0xFFFF & utf8))
				len = 1, tmp[0] = (u8)(0xFF & utf8);
			else if (! (0xFF & utf8))
				len = 2, tmp[0] = (u8)(0xFF & utf8 >> 8), tmp[1] = (u8)(0xFF & utf8);
			else
				len = 3, tmp[0] = (u8)(0xFF & utf8 >> 16), tmp[1] = (u8)(0xFF & utf8 >> 8), tmp[2] = (u8)(0xFF & utf8);
			utf8to16 (&w, tmp, len);
		}
		else if (!(OPT_HEX & flags) && OPT_UTF16 & flags)
			w = (wchar_t)load_le16 (src);
		else /*if (!(OPT_HEX & flags) && !(OPT_UTF16 & flags))*/ {
unsigned len;
			len = utf8len (src);
			utf8to16 (&w, src, len);
		}

		if (! (list_len < list_maxlen)) {
			list_maxlen *= 2;
			list = (wchar_t *)realloc (list, sizeof(wchar_t) * list_maxlen);
ASSERTE(list)
		}
		list[list_len++] = w;
	}

int retval;
	retval = -1;
do {
int outfd;
	if (OPT_STRING_DUMP + OPT_LOCREC_DUMP + OPT_SUMHASH_DUMP & flags)
		outfd = 0;
	else if (NULL == output) {
		if (-1 == (outfd = dup (STDOUT_FILENO))) {
fprintf (stderr, VTRR "fatal" VTO ": !dup(STDOUT)" "\n");
			break;
		}
		if (-1 == dup2 (STDERR_FILENO, STDOUT_FILENO)) {
fprintf (stderr, VTRR "fatal" VTO ": !dup2(STDERR,STDOUT)" "\n");
			break;
		}
	}
	else if (-1 == (outfd = open (output, O_WRONLY | O_CREAT | O_TRUNC, 0644))) {
fprintf (stderr, VTRR "%s" VTO ": file cannot open." "\n", output);
		break;
	}
	_patch (input, cmdv[0], flags, list_len, list, outfd, output);
	retval = 0;
} while (0);

	free (list);
	return retval;
}
