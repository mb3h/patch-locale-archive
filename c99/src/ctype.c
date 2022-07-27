#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

#include <stdio.h>
#include <stdlib.h> // exit

#include <alloca.h>

#include "ctype.h"
#include "archive.h"
#include "vt100.h"
#include "assert.h"
#include "endian-helper.h"

typedef uint32_t u32, u23;
typedef  uint8_t u8;
///////////////////////////////////////////////////////////////
// LC_CTYPE access

static void ctype_write_header (const ctype_header_s *src, void *dst_)
{
ASSERTE(src && dst_)
u8 *dst;
	dst = (u8 *)dst_;
	store_le32 (dst +0x00, 0x20090720 ^ LC_CTYPE); // ignore src->magic
	store_le32 (dst +0x04, src->n_elements);
unsigned i;
	for (i = 0; i < src->n_elements; ++i)
		store_le32 (dst +0x08 +i * sizeof(u32), src->offsets[i]);
}

u23 ctype_read_header (const void *src_, ctype_header_s *dst)
{
ASSERTE(src_)
const u8 *src;
	src = (const u8 *)src_;
u32 magic, n_elements;
	magic = load_le32 (src +0x00);
	if (! ((0x20090720 ^ LC_CTYPE) == magic))
		return 0; // error 'not LC_CTYPE' cf) locale/localeinfo.h
	n_elements = load_le32 (src +0x04);
u23 retval;
	retval = 0x08 +n_elements * sizeof(u32);

	if (!dst)
		return retval;
	dst->magic      = magic;
	dst->n_elements = n_elements;
unsigned i;
	for (i = 0; i < n_elements; ++i)
		dst->offsets[i] = load_le32 (src +0x08 +i * sizeof(u32));
	return retval;
}

typedef struct {
	u32 offset;
	u32 index;
} offset_sort;
static int offset_ascend (const void *lhs_, const void *rhs_)
{
const offset_sort *lhs, *rhs;
	lhs = (const offset_sort *)lhs_, rhs = (const offset_sort *)rhs_;
	if (! (lhs->offset == rhs->offset))
		return (lhs->offset < rhs->offset) ? -1 : 1;
	if (! (lhs->index == rhs->index))
		return (lhs->index < rhs->index) ? -1 : 1;
	return 0;
}
u23 ctype_read_position (const void *ctype_, unsigned ctype_size, unsigned index, u23 *block_offset)
{
unsigned header_size;
	if (! (header_size = ctype_read_header (ctype_, NULL)))
		return 0; // not LC_CTYPE
ctype_header_s *ctype; 
	ctype = (ctype_header_s *)alloca (header_size);
	ctype_read_header (ctype_, ctype);

	// address sort (safety)
	if (ctype->n_elements < index)
		return 0; // LC_CTYPE not found
offset_sort *sorted;
	sorted = (offset_sort *)alloca (sizeof(offset_sort) * ctype->n_elements);
u32 i;
	for (i = 0; i < ctype->n_elements; ++i) {
		sorted[i].index = i;
		sorted[i].offset = ctype->offsets[i];
	}
	qsort (sorted, ctype->n_elements, sizeof(offset_sort), offset_ascend);
	for (i = 0; i < ctype->n_elements; ++i)
		if (index == sorted[i].index)
			break;
BUG(i < ctype->n_elements)

	// wcwidth() data-block pickup
u23 block_begin, block_end;
	block_begin = sorted[i].offset;
	block_end = (i +1 < ctype->n_elements) ? sorted[i +1].offset : ctype_size;
	if (block_offset)
		*block_offset = block_begin;

	return block_end - block_begin;
}

bool ctype_resize_block (void *ctype_, unsigned index, u32 expand_size)
{
unsigned header_size;
	if (! (header_size = ctype_read_header (ctype_, NULL)))
		return false; // not LC_CTYPE
ctype_header_s *ctype; 
	ctype = (ctype_header_s *)alloca (header_size);
	ctype_read_header (ctype_, ctype);

	if (! (index < ctype->n_elements))
		return false;
u32 block_offset;
	block_offset = ctype->offsets[index];

unsigned i;
	for (i = 0; i < ctype->n_elements; ++i) {
		if (! (block_offset < ctype->offsets[i]))
			continue;
		ctype->offsets[i] += expand_size;
	}
	ctype_write_header (ctype, ctype_);
	return true;
}
