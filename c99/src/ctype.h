#ifndef CTYPE_H_INCLUDED__
#define CTYPE_H_INCLUDED__

typedef struct ctype_header {
	uint32_t magic;
	uint32_t n_elements;
	uint32_t offsets[1]; // __locale_data::values[]
} ctype_header_s;

uint32_t ctype_read_header (const void *src_, ctype_header_s *dst);
uint32_t ctype_read_position (const void *ctype_, unsigned ctype_size, unsigned index, uint32_t *block_offset);
bool ctype_resize_block (void *ctype_, unsigned index, uint32_t expand_size);

#endif //def CTYPE_H_INCLUDED__
