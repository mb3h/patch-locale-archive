/* gcc -std=c99 -o wcwidth-test wcwidth-test.c
 */
#define _XOPEN_SOURCE // wcwidth
#include <features.h>
#include <stdint.h>
#include <endian.h>
#include <wchar.h> // wcwidth
#include <locale.h>

#include <stdio.h>
#include <stdlib.h> // strtoul

#include <memory.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define VT0 "\033[0m"
#define VTRR "\033[31m"
#define VTGG "\033[32m"
///////////////////////////////////////////////////////////////

static uint16_t load_le16 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const uint16_t *)src;
#else // __BIG_ENDIAN
const uint8_t *p;
	p = (const uint8_t *)src;
	return p[1]<<8 | p[0];
#endif
}

static void *store_le16 (void *dst, unsigned val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)dst = (u16)val;
	return (u16 *)dst +1;
#else // __BIG_ENDIAN
u8 *p;
	p = (u8 *)dst;
	*p++ = (u8)val;
	*p++ = (u8)(0xFF & val >> 8);
	return p;
#endif
}

///////////////////////////////////////////////////////////////

static unsigned utf16to8 (void *retval, wchar_t w)
{
u8 *dst;
	dst = (u8 *)retval;
u16 u;
	memcpy (&u, &w, 2);
	if (u < 0x80)
		*dst++ = (u8)u;
	else if (u < 0x800) { // C0..DF 80..BF
		*dst++ = (u8)(0xC0 | 0x1F & u >> 6);
		*dst++ = (u8)(0x80 | 0x3F & u);
	}
	else { // E0..EF 80..BF 80..BF
		*dst++ = (u8)(0xE0 | 0x0F & u >> 12);
		*dst++ = (u8)(0x80 | 0x3F & u >> 6);
		*dst++ = (u8)(0x80 | 0x3F & u);
	}
	return dst - (u8 *)retval;
}

static unsigned utf8len (const void *utf8)
{
	if (!utf8) return 0;
u8 c;
	c = *(const u8 *)utf8;
	return (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 1;
}

static unsigned utf8to16 (wchar_t *retval, const void *utf8, unsigned len)
{
	const u8 *src, *end;
	u8 *dst;
	u16 w;
	u8 c;

	src = (const u8 *)utf8;
	end = src + len;
	dst = (u8 *)retval;
	for (; src < end && !('\0' == (c = src[0])); ++src) {
		if (c < 0x80)
			w = c;
		else if (c < 0xC0)
			w = 0x003F; // '?'
		else if (c < 0xE0) {
			if (src[1] < 0x80 || 0xBF < src[1])
				w = 0xFF1F; // '？'
			else
				w = (u16)(0xFC0 & c << 6 | 0x3F & src[1]);
			--len;
			++src;
		}
		else if (c < 0xF0) {
			if (src[1] < 0x80 || 0xBF < src[1] || src[2] < 0x80 || 0xBF < src[2])
				w = 0xFF1F; // '？'
			else
				w = (u16)(0xF000 & c << 12 | 0xFC0 & src[1] << 6 | 0x3F & src[2]);
			len -= 2;
			src += 2;
		}
		else {
			w = 0xFF1F; // '？'
			len -= 2;
			src += 2;
		}
		store_le16 (dst, w);
		dst += 2;
	}
	return (wchar_t *)dst - retval;
}

///////////////////////////////////////////////////////////////

static void test_wcwidth_utf16 (wchar_t w)
{
u16 unicode;
	memcpy (&unicode, &w, 2);

u8 utf8[3];
unsigned len, i;
	len = utf16to8 (utf8, w);
	for (i = 0; i < len; ++i)
printf ("%02X ", *(const u8 *)(utf8 +i));
	for (; i < 3; ++i)
printf ("   ");

printf ("-> %04X -> " VTRR "%d" VT0 " : '%s'" "\n", unicode, wcwidth (w), utf8);
}

enum {
	OPT_HEX = 1,
	OPT_UTF16 = 2,
};
int main (int argc, char *argv[])
{
	if (argc < 2) {
printf ("usage: [-h|--hex] [-w|--utf16] [-l|--locale <locale-name>] [-|<char> <char>...]" "\n");
		return -1;
	}
FILE *fp; unsigned flags, errcount;
	fp = NULL, flags = errcount = 0;
const char *locale; int i;
	for (locale = "", i = 1; i < argc; ++i) {
const char *opt;
		if (! ('-' == (opt = argv[i])[0]))
			continue;
		if ('\0' == opt[1])
			fp = stdin;
		else if (0 == strcmp ("-h", opt) || 0 == strcmp ("--hex", opt))
			flags |= OPT_HEX;
		else if (0 == strcmp ("-w", opt) || 0 == strcmp ("--utf16", opt))
			flags |= OPT_UTF16;
		else if (i +1 < argc && (0 == strcmp ("-l", opt) || 0 == strcmp ("--locale", opt)))
			locale = argv[++i];
		else {
fprintf (stderr, "'%s': " VTRR "invalid option" VT0 "\n", opt);
			++errcount;
		}
	}
	if (0 < errcount)
		return -1;

	setlocale (LC_ALL, locale);

printf (VTGG "UTF-8      UTF-16   wcwidth()" VT0 "\n");

	i = 1;
	while (1) {
const char *src; char linebuf[128];
		if (!fp) {
			if (! (i < argc))
				break;
			src = argv[i++];
			if ('-' == src[0])
				continue;
		}
		else {
			if (feof (fp))
				break;
			*linebuf = '\0';
			fgets (linebuf, sizeof(linebuf), fp);
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

		test_wcwidth_utf16 (w);
	}

	return 0;
}
