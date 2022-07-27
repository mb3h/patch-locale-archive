#include <stdint.h>

#include <stdio.h>
#include <string.h>

typedef  uint8_t u8;
///////////////////////////////////////////////////////////////

#define PARALLEL_HEXDUMP (1 << 3) // threads and parallel-use limit.
const char *hexdump (const void *src_, unsigned len)
{
static char text[PARALLEL_HEXDUMP][128];
static unsigned i_text = 0;
unsigned i;
	i = (PARALLEL_HEXDUMP -1) & ++i_text;
char *p;
	p = text[i];
const u8 *src, *end;
	src = (const u8 *)src_, end = src +len;
	for (; src < end; ++src) {
		if (! (p +2 +3 < text[i] +128 || src +1 == end)) {
			strcpy (p, "...");
			break;
		}
		sprintf (p, "%02x", *src);
		p += 2;
	}
	return text[i];
}
