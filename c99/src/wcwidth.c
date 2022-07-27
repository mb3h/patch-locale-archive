#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

#include <stdio.h>
#include <stdlib.h> // exit

#include <memory.h>
#include <malloc.h>
#include <alloca.h>

#include "wcwidth.h"
#include "vt100.h"
#include "assert.h"
#include "endian-helper.h"

//#include "config.h"
#define MINIMUM_SHIFT2 6
/* (*1)on 21bit unicode, level3 table size=64 (.shift2=6) -> maxcount=0x8000(1 << 21-6)
                       ,                  =128(.shift2=7) ->         =0x4000(1 << 21-7) [default]
                       ,                  =256(.shift2=8) ->         =0x2000(1 << 21-8)
                       , ...
 */

typedef uint32_t u32, u21, u23;
typedef  uint8_t u8, u5;
typedef uint16_t u16, u9;
///////////////////////////////////////////////////////////////
// internal

typedef struct wcwidth_header {
	u32 shift1;
	u32 bound;
	u32 shift2;
	u32 mask2;
	u32 mask3;
	u32 lookup1_top[1];
} wcwidth_header_s;

static bool _read_header (const void *src_, wcwidth_header_s *dst, unsigned cb)
{
BUG(src_ && dst && 0 < cb)
const u8 *src; u32 bound;
	src = (const u8 *)src_; bound = load_le32 (src +0x04);
#ifndef __cplusplus
ASSERT2(offsetof(wcwidth_header_s, lookup1_top[bound]) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)offsetof(wcwidth_header_s, lookup1_top[bound]), cb)
#endif //ndef __cplusplus
	if (cb < offsetof(wcwidth_header_s, lookup1_top[bound]))
		return false;
	dst->shift1 = load_le32 (src +0x00);
	dst->bound  = bound;
	dst->shift2 = load_le32 (src +0x08);
	dst->mask2  = load_le32 (src +0x0c);
	dst->mask3  = load_le32 (src +0x10);
	memcpy (dst->lookup1_top, src +0x14, sizeof(dst->lookup1_top[0]) * bound);
	return true;
}

typedef struct table3_lookup {
	u32 wc        :21;
	u32 padding_3 :10;
	u32 is_dirty  : 1;
	union {
		u23 address; // (*)only used in wcwidth_load0()
		u16 number; // < unicode(21bit) - level3(7bit etc.)
	};
} table3_lookup_s;
static int address_order (const void *lhs_, const void *rhs_)
{
const table3_lookup_s *lhs, *rhs;
	lhs = (const table3_lookup_s *)lhs_, rhs = (const table3_lookup_s *)rhs_;
	if (! (lhs->address == rhs->address))
		return (lhs->address < rhs->address) ? -1 : 1;
	if (! (lhs->wc == rhs->wc))
		return (lhs->wc < rhs->wc) ? -1 : 1;
	return 0;
}
static int unicode_order (const void *lhs_, const void *rhs_)
{
const table3_lookup_s *lhs, *rhs;
	lhs = (const table3_lookup_s *)lhs_, rhs = (const table3_lookup_s *)rhs_;
	if (! (lhs->wc == rhs->wc))
		return (lhs->wc < rhs->wc) ? -1 : 1;
	return 0;
}

typedef struct table3 {
	u8 *forked_data;
	u23 address;
	u21 refcount;
	u8 hash[4];
} table3_s;

typedef struct wcwidth_ {
	u16 cb;
	u16 table3_max;
	u16 table3_used;
	u16 padding_06;
	table3_lookup_s *table3_lookup;
	table3_s *table3;
	u23 wcwidth_size;
	wcwidth_header_s header;
} wcwidth_s;

static bool _check_offset (wcwidth_s *m_, u23 offset)
{
u5 index1_end; unsigned lookup1_end;
	index1_end = m_->header.bound >> m_->header.shift1;
	lookup1_end = offsetof(wcwidth_header_s, lookup1_top[index1_end]);
	if (! (lookup1_end <= offset))
		return false;
	if (! (offset + sizeof(u32) <= m_->wcwidth_size))
		return false;
	return true;
}

static u16 _create_table3 (wcwidth_s *m_, u23 address)
{
	// TODO: lookup element of refcount=0 and recycle if it exists.
	if (! (m_->table3_used < m_->table3_max)) {
ASSERTE(m_->table3_max < 0x10000 / 2) // expected .shift2 >= 6 (*1)
		m_->table3_max *= 2;
		m_->table3 = (table3_s *)realloc (m_->table3, sizeof(table3_s) * m_->table3_max);
	}

BUG(0 == address || (offsetof(wcwidth_header_s, lookup1_top[m_->header.bound]) <= address && address + (1 << m_->header.shift2) <= m_->wcwidth_size))
table3_s *table3; u16 new_index;
	table3 = &m_->table3[new_index = m_->table3_used];
	memset (table3, 0, sizeof(*table3));
//	table3->forked_data = NULL;
	table3->refcount = 1;
	if (0 == (table3->address = address))
		table3->forked_data = (u8 *)malloc (1 << m_->header.shift2);
	++m_->table3_used;
	return new_index +1;
}

static u23 _get_table3 (wcwidth_s *m_, const void *wcwidth, u21 wc)
{
ASSERTE(wc >> m_->header.shift1 < m_->header.bound)
u5 index1; u23 lookup2_top;
	index1 = wc >> m_->header.shift1;
	lookup2_top = m_->header.lookup1_top[index1];
	if (!_check_offset (m_, lookup2_top))
		return 0;

u9 index2; u23 offset2, lookup3_top;
	index2 = m_->header.mask2 & wc >> m_->header.shift2;
	offset2 = lookup2_top + sizeof(u32) * index2;
	lookup3_top = load_le32 ((const u8 *)wcwidth + offset2);
	if (!_check_offset (m_, lookup3_top))
		return 0;

	return lookup3_top;
}

static u16 _find_table3 (const wcwidth_s *m_, void *wcwidth, const void *data)
{
u16 index;
	for (index = 0; index < m_->table3_used; ++index) {
const table3_s *table3;
		if (0 == (table3 = &m_->table3[index])->refcount)
			continue;
const void *table3_data;
		if (NULL == (table3_data = table3->forked_data))
			table3_data = (u8 *)wcwidth + table3->address;
		if (0 == memcmp (table3_data, data, 1 << m_->header.shift2))
			return index +1;
	}
	return 0;
}

///////////////////////////////////////////////////////////////
// _NL_CTYPE_WIDTH access

void wcwidth_dtor (void *this_)
{
ASSERTE(this_)
wcwidth_s *m_;
	m_ = (wcwidth_s *)this_;

	if (m_->table3_lookup) {
		free (m_->table3_lookup);
		m_->table3_lookup = NULL;
	}

	if (!m_->table3)
		return;
unsigned i;
	for (i = 0; i < m_->table3_used; ++i) {
		if (!m_->table3[i].forked_data)
			continue;
		free (m_->table3[i].forked_data);
		m_->table3[i].forked_data = NULL;
	}
	free (m_->table3);
	m_->table3 = NULL;
}

bool wcwidth_ctor (void *this_, unsigned cb, const void *wcwidth, u23 wcwidth_size)
{
#ifndef __cplusplus
ASSERT2(sizeof(wcwidth_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(wcwidth_s), cb)
#endif //ndef __cplusplus
	memset (this_, 0, cb);
wcwidth_s *m_;
	m_ = (wcwidth_s *)this_;

	m_->cb = cb;
	m_->wcwidth_size = wcwidth_size;
	if (!_read_header (wcwidth, &m_->header, m_->cb - offsetof(wcwidth_s, header)))
		return false;
	if (! (MINIMUM_SHIFT2 <= m_->header.shift2))
fprintf (stderr, "LC_CTYPE#WIDTH.shift2: " VTRR "%d" VTO " < %d: level3 table too small. unexpected and unsupported on this source code." "\n", m_->header.shift2, MINIMUM_SHIFT2);

unsigned unicode_max;
	unicode_max = m_->header.bound << m_->header.shift1;
	m_->table3_lookup = (table3_lookup_s *)malloc (sizeof(table3_lookup_s) * (unicode_max >> m_->header.shift2));
table3_lookup_s *lookup; u21 wc;
	for (lookup = m_->table3_lookup, wc = 0; wc < unicode_max; ++lookup, wc += 1 << m_->header.shift2) {
		memset (lookup, 0, sizeof(*lookup));
		lookup->wc = wc;
		lookup->address = _get_table3 (m_, wcwidth, wc);
	}

	qsort (m_->table3_lookup, unicode_max >> m_->header.shift2, sizeof(m_->table3_lookup[0]), address_order);
ASSERTE(NULL == m_->table3 && 0 == m_->table3_max)
	m_->table3 = (table3_s *)malloc (sizeof(table3_s) * (m_->table3_max = 16));
	m_->table3_used = 0;
	for (lookup = m_->table3_lookup; lookup < m_->table3_lookup + (wc >> m_->header.shift2); ++lookup) {
		if (0 == lookup->address)
			continue;
table3_s *previous_table3;
		previous_table3 = &m_->table3[(lookup -1)->number -1];
		if (m_->table3_lookup < lookup && previous_table3->address == lookup->address) {
			++previous_table3->refcount;
			lookup->number = (lookup -1)->number;
			continue;
		}
		lookup->number = _create_table3 (m_, lookup->address);
	}

	qsort (m_->table3_lookup, unicode_max >> m_->header.shift2, sizeof(m_->table3_lookup[0]), unicode_order);
	return true;
}

void wcwidth_modify (void *this_, void *wcwidth, u21 wc, u8 value)
{
wcwidth_s *m_;
	m_ = (wcwidth_s *)this_;
ASSERTE(wc >> m_->header.shift1 < m_->header.bound)

table3_lookup_s *lookup;
	lookup = &m_->table3_lookup[wc >> m_->header.shift2];
table3_s *old_table3; u8 *old_data;
	old_table3 = &m_->table3[lookup->number -1];
	if (NULL == (old_data = old_table3->forked_data))
		old_data = (u8 *)wcwidth + old_table3->address;
u21 index3;
	if (value == old_data[index3 = m_->header.mask3 & wc])
		return;
	lookup->is_dirty = 1;

	// don't dangling allocated level3-table since before patching.
BUG(1 != old_table3->refcount || 0 < lookup->number && lookup->number < m_->table3_used +1)
	if (1 == old_table3->refcount && !m_->table3[lookup->number -1].forked_data) {
		old_data[index3] = value;
		return;
	}

u8 *new_data;
	new_data = (u8 *)alloca (1 << m_->header.shift2);
	memcpy (new_data, old_data, 1 << m_->header.shift2);
	new_data[index3] = value;
u16 number;
	if (number = _find_table3 (m_, wcwidth, new_data))
		++m_->table3[number -1].refcount;
	else if (1 == old_table3->refcount) {
		old_data[index3] = value;
		return;
	}
	else {
		number = _create_table3 (m_, 0);
		memcpy (m_->table3[number -1].forked_data, new_data, 1 << m_->header.shift2);
	}

BUG(0 < lookup->number && lookup->number < m_->table3_used +1)
	if (m_->table3[lookup->number -1].forked_data) {
BUG(0 < old_table3->refcount)
		--old_table3->refcount;
	}
	lookup->number = number;
}

unsigned wcwidth_commit (void *this_, void *wcwidth, void *dst, unsigned max_bytes)
{
wcwidth_s *m_;
	m_ = (wcwidth_s *)this_;
unsigned table3_size;
	table3_size = 1 << m_->header.shift2;

	// calculate total expanding size
unsigned expand_count, i;
	for (expand_count = 0, i = 0; i < m_->table3_used; ++i)
		if (m_->table3[i].forked_data)
			++expand_count;

	if (! (wcwidth && (0 == expand_count || dst && 0 < max_bytes)))
		return table3_size * expand_count;
	
	// output all expanding data
unsigned used;
	for (used = 0, i = 0; i < m_->table3_used; ++i) {
table3_s *table3;
		if (NULL == (table3 = &m_->table3[i])->forked_data)
			continue;
BUG(dst)
		if (max_bytes < used + table3_size)
			return used;
BUG(0 == table3->address) // WARN: multiple _commit() calling
		table3->address = m_->wcwidth_size + used;
		memcpy ((u8 *)dst +used, table3->forked_data, table3_size);
		used += table3_size;
	}

	// modify level2 table
unsigned unicode_max;
	unicode_max = m_->header.bound << m_->header.shift1;
table3_lookup_s *lookup; u21 wc;
	for (lookup = m_->table3_lookup, wc = 0; wc < unicode_max; ++lookup, wc += 1 << m_->header.shift2) {
		if (!lookup->is_dirty)
			continue;
u5 index1; u9 index2;
		index1 = wc >> m_->header.shift1;
		index2 = m_->header.mask2 & wc >> m_->header.shift2;
u23 lookup2_top, offset2, lookup3_top;
		lookup2_top = m_->header.lookup1_top[index1];
		offset2 = lookup2_top + sizeof(u32) * index2;
		lookup3_top = load_le32 ((u8 *)wcwidth + offset2);
		if (lookup3_top == m_->table3[lookup->number -1].address)
			continue;
		store_le32 ((u8 *)wcwidth + offset2, m_->table3[lookup->number -1].address);
	}
	return used;
}
