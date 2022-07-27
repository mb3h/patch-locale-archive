#ifndef ARCHIVE_H_INCLUDED__
#define ARCHIVE_H_INCLUDED__

// cf) locale/bits/locale.h
#define LC_CTYPE    0
#define LC_COLLATE  3
#define LC_ALL      6
#define LC_LAST    13
const char *strcategory (unsigned category);

typedef struct locarhead {
//	uint32_t magic;
//	uint32_t serial;
	uint32_t namehash_offset;
//	uint32_t namehash_used;
	uint32_t namehash_size;
	uint32_t string_offset;
	uint32_t string_used;
//	uint32_t string_size;
	uint32_t locrectab_offset;
	uint32_t locrectab_used;
//	uint32_t locrectab_size;
	uint32_t sumhash_offset;
//	uint32_t sumhash_used;
	uint32_t sumhash_size;
} locarhead_s;
void archive_read_header (const void *src_, locarhead_s *dst);
unsigned archive_enum_locale (const void *src, void (*lpfn)(const char *name, uint32_t locrec_offset, void *priv), void *priv);

struct locrecent_record {
	uint32_t offset;
	uint32_t size;
};
typedef struct locrecent {
	uint32_t refs;
	struct locrecent_record record[LC_LAST];
} locrecent_s;
void archive_read_locrec (const void *src_, locrecent_s *dst);
uint32_t archive_find_locrec (const void *ar, const char *locale_name, uint8_t category, struct locrecent_record *dst);
unsigned archive_enum_locrec (const void *ar, void (*lpfn)(uint32_t locrec_offset, void *priv), void *priv);

typedef struct sumhashent {
	uint8_t sum[16];
	uint32_t file_offset;
} sumhashent_s;
bool archive_remove_sumhash (const void *ar, uint32_t index);
bool archive_append_sumhash (void *ar, const uint8_t *sum, uint32_t file_offset);
uint32_t archive_find_sumhash (const void *ar, const void *sum, sumhashent_s *dst);
unsigned archive_enum_sumhash (const void *ar, void (*lpfn)(uint32_t sumhash_offset, uint32_t locrec_offset, uint8_t category, void *priv), void *priv);

void archive_resize_file (void *ar, uint32_t insert_before, uint32_t insert_size);

#endif //def ARCHIVE_H_INCLUDED__
