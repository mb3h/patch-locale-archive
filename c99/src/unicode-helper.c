#include <stdint.h>
#include <stddef.h> // wchar_t
#include <endian.h>

#include "endian-helper.h"
#include "unicode-helper.h"

typedef  uint8_t u8;
typedef uint16_t u16;
///////////////////////////////////////////////////////////////

unsigned utf16to8 (void *retval, wchar_t w)
{
u8 *dst;
	dst = (u8 *)retval;
	if (w < 0x80)
		*dst++ = (u8)w;
	else if (w < 0xA0) // undefined ?
		*dst++ = (u8)'?';
	else if (w < 0x800) { // C0..DF 80..BF
		*dst++ = (u8)(0xC0 | 0x1F & w >> 6);
		*dst++ = (u8)(0x80 | 0x3F & w);
	}
	else if (0x2028 == w || 0x2029 == w) // undefined ?
		*dst++ = (u8)'?';
	else if (w < 0x10000) { // E0..EF 80..BF 80..BF
		*dst++ = (u8)(0xE0 | 0x0F & w >> 12);
		*dst++ = (u8)(0x80 | 0x3F & w >> 6);
		*dst++ = (u8)(0x80 | 0x3F & w);
	} // 17bit over
	else
		*dst++ = (u8)'?';
	return dst - (u8 *)retval;
}

unsigned utf8len (const void *utf8)
{
	if (!utf8) return 0;
u8 c;
	c = *(const u8 *)utf8;
	return (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 1;
}

unsigned utf8to16 (wchar_t *retval, const void *utf8, unsigned len)
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
