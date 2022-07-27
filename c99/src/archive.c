#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

#include <stdio.h>
#include <stdlib.h> // exit

#include <memory.h>
#include <malloc.h>
#include <alloca.h>

#include "archive.h"
#include "md5.h"
#include "vt100.h"
#include "assert.h"
#include "endian-helper.h"

typedef uint32_t u32;
typedef  uint8_t u8, u4;

#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))
#define PADDING(align, x) (((align) -1) - ((x) +(align) -1) % (align))
#define ALIGN(align, x) ((x) + PADDING(align, x))
///////////////////////////////////////////////////////////////

static int u32_ascend (const void *lhs_, const void *rhs_)
{
u32 lhs, rhs;
	lhs = *(const u32 *)lhs_, rhs = *(const u32 *)rhs_;
	return (lhs == rhs) ? 0 : (lhs < rhs) ? -1 : 1;
}

// NOTE: src == dst available.
static unsigned u32_unique (const u32 *src, unsigned len, u32 *dst)
{
unsigned retval, i;
	for (retval = i = 0; i < len; ++i) {
		if (0 < i && src[i -1] == src[i])
			continue;
		dst[retval++] = src[i];
	}
	return retval;
}

///////////////////////////////////////////////////////////////

static const char *category2text[] = {
	  "CTYPE"
	, "NUMERIC"
	, "TIME"
	, "COLLATE"
	, "MONETARY"
	, "MESSAGE"
	, "ALL"
	, "PAPER"
	, "NAME"
	, "ADDRESS"
	, "TELEPHONE"
	, "MEASUREMENT"
	, "IDENTIFICATION"
};
const char *strcategory (unsigned i)
{
	if (! (i < arraycountof(category2text)))
		return "";
	return category2text[i];
}

// cf) locale/programs/locarchive.h
static u32 archive_hashval (const void *key, unsigned keylen)
{
const u8 *src, *end;
	src = (const u8 *)key, end = src + keylen;

u32 retval;
	for (retval = keylen; src < end; ++src) {
		retval = retval << 9 | retval >> 32 - 9; // rotate
		retval += (u32)*src;
	}
	if (0 == retval)
		return (u32)-1;
	return retval;
}

///////////////////////////////////////////////////////////////
// HEADER access

void archive_read_header (const void *src_, locarhead_s *dst)
{
ASSERTE(src_ && dst)
const u8 *src;
	src = (const u8 *)src_;
	dst->namehash_offset  = load_le32 (src +0x08);
	dst->namehash_size    = load_le32 (src +0x10);
	dst->string_offset    = load_le32 (src +0x14);
	dst->string_used      = load_le32 (src +0x18);
	dst->locrectab_offset = load_le32 (src +0x20);
	dst->locrectab_used   = load_le32 (src +0x24);
	dst->sumhash_offset   = load_le32 (src +0x2C);
	dst->sumhash_size     = load_le32 (src +0x34);
}

///////////////////////////////////////////////////////////////
// NAMEHASH / STRING access

typedef struct namehashent {
	u32 hashval;
	u32 name_offset;
	u32 locrec_offset;
} namehashent_s;

static void archive_read_namehash (const void *src_, namehashent_s *dst)
{
ASSERTE(src_ && dst)
const u8 *src;
	src = (const u8 *)src_;
	dst->hashval       = load_le32 (src +0x00);
	dst->name_offset   = load_le32 (src +0x04);
	dst->locrec_offset = load_le32 (src +0x08);
}

static bool archive_find_locale (const void *ar, const char *name, namehashent_s *dst)
{
ASSERTE(ar && name && dst)
locarhead_s head;
	archive_read_header (ar, &head);
u32 hval, index, offset;
	hval = archive_hashval (name, strlen (name));
	index = hval % head.namehash_size;
	offset = head.namehash_offset + sizeof(namehashent_s) * index;
	archive_read_namehash ((const u8 *)ar + offset, dst);
const char *found;
	found = (const char *)((const u8 *)ar + dst->name_offset);
	if (! (hval == dst->hashval && 0 == strcmp (name, found)))
		return false; // PENDING: index collision not supported yet.
	return true;
}

unsigned archive_enum_locale (const void *src, void (*lpfn)(const char *name, u32 locrec_offset, void *priv), void *priv)
{
locarhead_s head;
	archive_read_header (src, &head);
unsigned retval; const u8 *p, *next, *end;
	retval = 0;
	p = (const u8 *)src + head.string_offset, end = p + head.string_used;
	for (; next = (const u8 *)memchr (p, '\0', end - p); ++retval, p = next +1) {
		if (!lpfn)
			continue;
const char *name; namehashent_s namehashent;
		name = (const char *)p;
		if (!archive_find_locale (src, name, &namehashent))
			namehashent.locrec_offset = 0;
		if (!lpfn)
			continue;
		lpfn (name, namehashent.locrec_offset, priv);
	}
	return retval;
}

///////////////////////////////////////////////////////////////
// LOCRECTAB access

void archive_read_locrec (const void *src_, locrecent_s *dst)
{
ASSERTE(src_ && dst)
const u8 *src;
	src = (const u8 *)src_;
	dst->refs = load_le32 (src +0x00);
int i;
	for (i = 0; i < LC_LAST; ++i) {
		dst->record[i].offset = load_le32 (src +0x04 + i * 8);
		dst->record[i].size   = load_le32 (src +0x04 + i * 8 +4);
	}
}

static void archive_write_locrec (const locrecent_s *src, void *dst_)
{
ASSERTE(src && dst_)
u8 *dst;
	dst = (u8 *)dst_;
	store_le32 (dst +0x00, src->refs);
int i;
	for (i = 0; i < LC_LAST; ++i) {
		store_le32 (dst +0x04 + i * 8 +0, src->record[i].offset);
		store_le32 (dst +0x04 + i * 8 +4, src->record[i].size);
	}
}

u32 archive_find_locrec (const void *ar, const char *locale_name, u4 category, struct locrecent_record *dst)
{
BUG(ar && locale_name && category < LC_LAST)
namehashent_s namehashent; locrecent_s locrecent;
	if (!archive_find_locale (ar, locale_name, &namehashent))
		return 0;
	archive_read_locrec (ar + namehashent.locrec_offset, &locrecent);
	if (dst)
		memcpy (dst, &locrecent.record[category], sizeof(locrecent.record[0]));
locarhead_s head; unsigned number;
	archive_read_header (ar, &head);
	number = 1 + (namehashent.locrec_offset - head.locrectab_offset) / sizeof(locrecent_s);
	return number;
}

typedef struct locrec_list {
	u32 *offsets;
	unsigned used;
} locrec_list_s;
static void _locrec_list_append (const char *name, u32 locrec_offset, void *priv)
{
locrec_list_s *list;
	list = (locrec_list_s *)priv;
	list->offsets[list->used++] = locrec_offset;
}
unsigned archive_enum_locrec (const void *ar, void (*lpfn)(u32 locrec_offset, void *priv), void *priv)
{
unsigned locale_count;
	locale_count = archive_enum_locale (ar, NULL, NULL);
locrec_list_s list;
	list.used = 0;
	list.offsets = (u32 *)alloca (sizeof(u32) * locale_count);
	archive_enum_locale (ar, _locrec_list_append, &list);
	qsort (list.offsets, list.used, sizeof(list.offsets[0]), u32_ascend);
	list.used = u32_unique (list.offsets, list.used, list.offsets);
unsigned i;
	if (lpfn)
		for (i = 0; i < list.used; ++i)
			lpfn (list.offsets[i], priv);
	return list.used;
}

///////////////////////////////////////////////////////////////
// SUMHASH access

static void archive_read_sumhash (const void *src_, sumhashent_s *dst)
{
ASSERTE(src_ && dst)
const u8 *src;
	src = (const u8 *)src_;
	memcpy (dst->sum, src +0x00, sizeof(dst->sum));
	dst->file_offset = load_le32 (src +0x10);
}

// PENDING: index collision-chain not supported yet.
bool archive_remove_sumhash (const void *ar, u32 index)
{
BUG(ar)
locarhead_s head;
	archive_read_header (ar, &head);
u32 offset;
	if (! (index < head.sumhash_size))
		return false;
	offset = head.sumhash_offset + sizeof(sumhashent_s) * index;
	memset ((u8 *)ar + offset, 0, sizeof(sumhashent_s));
	return true;
}

bool archive_append_sumhash (void *ar, const u8 *sum, u32 file_offset)
{
BUG(ar && sum)
locarhead_s head;
	archive_read_header (ar, &head);
sumhashent_s test;
u32 hval, index, offset;
	hval = archive_hashval (sum, sizeof(test.sum));
	index = hval % head.sumhash_size;
	offset = head.sumhash_offset + sizeof(sumhashent_s) * index;
u8 *dst;
	dst = (u8 *)ar + offset;
	archive_read_sumhash (dst, &test);
	if (! (0 == test.file_offset))
		return false; // PENDING: index collision not supported yet.
	memcpy (dst +0x00, sum, 16);
	store_le32 (dst +0x10, file_offset);
	return true;
}

u32 archive_find_sumhash (const void *ar, const void *sum, sumhashent_s *dst)
{
ASSERTE(ar && sum && dst)
locarhead_s head;
	archive_read_header (ar, &head);
u32 hval, index, offset;
	hval = archive_hashval (sum, sizeof(dst->sum));
	index = hval % head.sumhash_size;
	offset = head.sumhash_offset + sizeof(sumhashent_s) * index;
	archive_read_sumhash ((const u8 *)ar + offset, dst);
	if (! (0 == memcmp (dst->sum, sum, sizeof(dst->sum))))
		return 0; // PENDING: index collision not supported yet.
	return index +1;
}

static unsigned locrec_enum_sumhash (const void *ar, u32 locrec_offset, void (*lpfn)(u32 file_offset, u32 file_size, u32 locrec_offset, u8 category, void *priv), void *priv)
{
locrecent_s src;
	archive_read_locrec ((const u8 *)ar + locrec_offset, &src);
const struct locrecent_record *all, *p;
	all = &src.record[LC_ALL];
unsigned retval;
	for (retval = 0, p = &src.record[0]; p < &src.record[LC_LAST]; ++p) {
		if (p == all || 0 == p->size)
			continue;
		if (all->offset <= p->offset && p->offset < all->offset + all->size) {
ASSERTE(all->offset < p->offset + p->size && p->offset + p->size <= all->offset + all->size)
			continue;
		}
		if (lpfn)
			lpfn (p->offset, p->size, locrec_offset, p - &src.record[0], priv);
		++retval;
	}
	if (0 < all->size) {
		lpfn (all->offset, all->size, locrec_offset, all - &src.record[0], priv);
		++retval;
	}
	return retval;
}
typedef struct sumhash_detail {
	u32 offset; // for sort only
	u32 size; // for sort only
	u32 locrec_offset;
	u8 category;
} sumhash_detail_s;
static int sumhash_detail_ascend (const void *lhs_, const void *rhs_)
{
const sumhash_detail_s *lhs, *rhs;
	lhs = (const sumhash_detail_s *)lhs_, rhs = (const sumhash_detail_s *)rhs_;
	if (! (lhs->offset == rhs->offset))
		return (lhs->offset < rhs->offset) ? -1 : 1;
	if (! (lhs->size == rhs->size))
		return (lhs->size < rhs->size) ? -1 : 1;
	return 0;
}
static unsigned sumhash_detail_unique (const sumhash_detail_s *src, unsigned len, sumhash_detail_s *dst)
{
unsigned retval, i;
	for (retval = i = 0; i < len; ++i) {
		if (0 < i && src[i -1].offset == src[i].offset && src[i -1].size == src[i].size) {
ASSERTE(src[i -1].category == src[i].category)
			continue;
		}
		memcpy (&dst[retval], &src[i], sizeof(dst[0]));
		++retval;
	}
	return retval;
}
typedef struct sumhash_list {
	sumhash_detail_s *list;
	unsigned maxlen;
	unsigned used;
} sumhash_list_s;
static void _sumhash_list_append (u32 file_offset, u32 file_size, u32 locrec_offset, u8 category, void *priv)
{
sumhash_list_s *m_;
	m_ = (sumhash_list_s *)priv;
	if (! (m_->used < m_->maxlen)) {
		m_->maxlen *= 2;
		m_->list = (sumhash_detail_s *)realloc (m_->list, sizeof(m_->list[0]) * m_->maxlen);
	}
	m_->list[m_->used].offset        = file_offset;
	m_->list[m_->used].size          = file_size  ;
	m_->list[m_->used].locrec_offset = locrec_offset;
	m_->list[m_->used].category      = category;
	++m_->used;
}

typedef struct sumhash_from_locrec {
	const void *ar;
	void *priv;
} sumhash_from_locrec_s;
static void _enum_sumhash_from_locrec_thunk (u32 locrec_offset, void *priv)
{
sumhash_from_locrec_s *m_;
	m_ = (sumhash_from_locrec_s *)priv;
	locrec_enum_sumhash (m_->ar, locrec_offset, _sumhash_list_append, m_->priv);
}
unsigned archive_enum_sumhash (const void *ar, void (*lpfn)(u32 sumhash_offset, u32 locrec_offset, u8 category, void *priv), void *priv)
{
sumhash_list_s u, *m_ = &u;
	memset (m_, 0, sizeof(*m_));
	m_->list = (sumhash_detail_s *)malloc (sizeof(m_->list[0]) * (m_->maxlen = 16));

sumhash_from_locrec_s thunk
	= { .ar = ar, .priv = m_ };
	archive_enum_locrec (ar, _enum_sumhash_from_locrec_thunk, &thunk);
	qsort (m_->list, m_->used, sizeof(m_->list[0]), sumhash_detail_ascend);
	m_->used = sumhash_detail_unique (m_->list, m_->used, m_->list);
	if (lpfn) {
locarhead_s head;
		archive_read_header (ar, &head);
unsigned i;
		for (i = 0; i < m_->used; ++i) {
u8 sum[16]; u32 hval, index, offset;
			md5sum (ar + m_->list[i].offset, m_->list[i].size, sum);
			hval = archive_hashval (sum, sizeof(sum));
			index = hval % head.sumhash_size;
			offset = head.sumhash_offset + sizeof(sumhashent_s) * index;
			lpfn (offset, m_->list[i].locrec_offset, m_->list[i].category, priv);
		}
	}

	free (m_->list), m_->list = NULL;
	return m_->used;
}

///////////////////////////////////////////////////////////////

typedef struct expand_file {
	u8 *ar;
	u32 insert_before;
	u32 insert_size;
} expand_file_s;

static void _expand_file_locrec (u32 locrec_offset, void *priv)
{
const expand_file_s *m_;
	m_ = (const expand_file_s *)priv;
locrecent_s l; struct locrecent_record *all;
	archive_read_locrec (m_->ar + locrec_offset, &l), all = &l.record[LC_ALL];
locarhead_s head; unsigned number;
	archive_read_header (m_->ar, &head);
	number = 1 + (locrec_offset - head.locrectab_offset) / sizeof(locrecent_s);

unsigned i;
	for (i = 0; i < LC_LAST; ++i) {
struct locrecent_record *r;
		r = &l.record[i];
		if (m_->insert_before <= r->offset) {
			r->offset += m_->insert_size;
			continue;
		}
u32 end;
		if ((end = ALIGN(16, r->offset + r->size)) < m_->insert_before)
			continue;
ASSERTE(end == m_->insert_before || LC_ALL == i)
		r->size += m_->insert_size;
	}
	archive_write_locrec (&l, m_->ar + locrec_offset);
}

static void _expand_file_sumhash (u32 sumhash_offset, u32 locrec_offset, u8 category, void *priv)
{
const expand_file_s *m_;
	m_ = (const expand_file_s *)priv;
locarhead_s head; u32 sumhash_number; sumhashent_s sumhashent;
	archive_read_header (m_->ar, &head);
	sumhash_number = 1 + (sumhash_offset - head.sumhash_offset) / sizeof(sumhashent_s);
	archive_read_sumhash (m_->ar + sumhash_offset, &sumhashent);
	if (m_->insert_before <= sumhashent.file_offset) {
		sumhashent.file_offset += m_->insert_size;
		store_le32 (m_->ar + sumhash_offset + offsetof(sumhashent_s, file_offset), sumhashent.file_offset);
	}
}

void archive_resize_file (void *ar, u32 insert_before, u32 insert_size)
{
expand_file_s u, *this_ = &u;
	this_->ar            = ar;
	this_->insert_before = insert_before;
	this_->insert_size   = insert_size;
	archive_enum_locrec (ar, _expand_file_locrec, this_);
	archive_enum_sumhash (ar, _expand_file_sumhash, this_);
}
