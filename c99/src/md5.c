// This code was ported from PuTTY (sshmd5.c).

/* [LICENCE description]
PuTTY is copyright 1997-2007 Simon Tatham.

Portions copyright Robert de Bath, Joris van Rantwijk, Delian
Delchev, Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry,
Justin Bradford, Ben Harris, Malcolm Smith, Ahmad Khalifa, Markus
Kuhn, and CORE SDI S.A.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <endian.h>
#include <memory.h>

#include "md5.h"

typedef uint32_t u32;
typedef  uint8_t u8;

///////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"

#define ASSERT2(expr, fmt, a, b) \
	if (! (expr)) { \
printf (VTRR "ASSERT" VTO "! " #expr fmt "\n", a, b); \
		exit (-1); \
	}

///////////////////////////////////////////////////////////////

static void *store_le32 (void *dst, unsigned val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)dst = (u32)val;
	return (u32 *)dst +1;
#else // __BIG_ENDIAN
u8 *p;
	p = (u8 *)dst;
	*p++ = (u8)val;
	*p++ = (u8)(0xFF & val >> 8);
	*p++ = (u8)(0xFF & val >> 16);
	*p++ = (u8)(0xFF & val >> 24);
	return p;
#endif
}

static u32 load_le32 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const u32 *)src;
#else // __BIG_ENDIAN
const uint8_t *p;
	p = (const u8 *)src;
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
#endif
}

///////////////////////////////////////////////////////////////

typedef struct {
	u32 h[4];
} md5core_s;

typedef struct md5sum_ {
	md5core_s core;
	u8 block[64];
	int blkused;
	u32 lenhi, lenlo;
} md5sum_s;

#define F(x,y,z) ( ((x) & (y)) | ((~(x)) & (z)) )
#define G(x,y,z) ( ((x) & (z)) | ((~(z)) & (y)) )
#define H(x,y,z) ( (x) ^ (y) ^ (z) )
#define I(x,y,z) ( (y) ^ ( (x) | ~(z) ) )

#define rol(x,y) ( ((x) << (y)) | (((u32)x) >> (32-y)) )

#define subround(f,w,x,y,z,k,s,ti) \
	w = x + rol(w + f(x,y,z) + block[k] + ti, s)

static void _core_init (md5core_s *s)
{
	s->h[0] = 0x67452301;
	s->h[1] = 0xefcdab89;
	s->h[2] = 0x98badcfe;
	s->h[3] = 0x10325476;
}

static void _core_block (md5core_s *s, u32 *block)
{
	u32 a, b, c, d;

	a = s->h[0];
	b = s->h[1];
	c = s->h[2];
	d = s->h[3];

	subround(F, a, b, c, d, 0, 7, 0xd76aa478);
	subround(F, d, a, b, c, 1, 12, 0xe8c7b756);
	subround(F, c, d, a, b, 2, 17, 0x242070db);
	subround(F, b, c, d, a, 3, 22, 0xc1bdceee);
	subround(F, a, b, c, d, 4, 7, 0xf57c0faf);
	subround(F, d, a, b, c, 5, 12, 0x4787c62a);
	subround(F, c, d, a, b, 6, 17, 0xa8304613);
	subround(F, b, c, d, a, 7, 22, 0xfd469501);
	subround(F, a, b, c, d, 8, 7, 0x698098d8);
	subround(F, d, a, b, c, 9, 12, 0x8b44f7af);
	subround(F, c, d, a, b, 10, 17, 0xffff5bb1);
	subround(F, b, c, d, a, 11, 22, 0x895cd7be);
	subround(F, a, b, c, d, 12, 7, 0x6b901122);
	subround(F, d, a, b, c, 13, 12, 0xfd987193);
	subround(F, c, d, a, b, 14, 17, 0xa679438e);
	subround(F, b, c, d, a, 15, 22, 0x49b40821);
	subround(G, a, b, c, d, 1, 5, 0xf61e2562);
	subround(G, d, a, b, c, 6, 9, 0xc040b340);
	subround(G, c, d, a, b, 11, 14, 0x265e5a51);
	subround(G, b, c, d, a, 0, 20, 0xe9b6c7aa);
	subround(G, a, b, c, d, 5, 5, 0xd62f105d);
	subround(G, d, a, b, c, 10, 9, 0x02441453);
	subround(G, c, d, a, b, 15, 14, 0xd8a1e681);
	subround(G, b, c, d, a, 4, 20, 0xe7d3fbc8);
	subround(G, a, b, c, d, 9, 5, 0x21e1cde6);
	subround(G, d, a, b, c, 14, 9, 0xc33707d6);
	subround(G, c, d, a, b, 3, 14, 0xf4d50d87);
	subround(G, b, c, d, a, 8, 20, 0x455a14ed);
	subround(G, a, b, c, d, 13, 5, 0xa9e3e905);
	subround(G, d, a, b, c, 2, 9, 0xfcefa3f8);
	subround(G, c, d, a, b, 7, 14, 0x676f02d9);
	subround(G, b, c, d, a, 12, 20, 0x8d2a4c8a);
	subround(H, a, b, c, d, 5, 4, 0xfffa3942);
	subround(H, d, a, b, c, 8, 11, 0x8771f681);
	subround(H, c, d, a, b, 11, 16, 0x6d9d6122);
	subround(H, b, c, d, a, 14, 23, 0xfde5380c);
	subround(H, a, b, c, d, 1, 4, 0xa4beea44);
	subround(H, d, a, b, c, 4, 11, 0x4bdecfa9);
	subround(H, c, d, a, b, 7, 16, 0xf6bb4b60);
	subround(H, b, c, d, a, 10, 23, 0xbebfbc70);
	subround(H, a, b, c, d, 13, 4, 0x289b7ec6);
	subround(H, d, a, b, c, 0, 11, 0xeaa127fa);
	subround(H, c, d, a, b, 3, 16, 0xd4ef3085);
	subround(H, b, c, d, a, 6, 23, 0x04881d05);
	subround(H, a, b, c, d, 9, 4, 0xd9d4d039);
	subround(H, d, a, b, c, 12, 11, 0xe6db99e5);
	subround(H, c, d, a, b, 15, 16, 0x1fa27cf8);
	subround(H, b, c, d, a, 2, 23, 0xc4ac5665);
	subround(I, a, b, c, d, 0, 6, 0xf4292244);
	subround(I, d, a, b, c, 7, 10, 0x432aff97);
	subround(I, c, d, a, b, 14, 15, 0xab9423a7);
	subround(I, b, c, d, a, 5, 21, 0xfc93a039);
	subround(I, a, b, c, d, 12, 6, 0x655b59c3);
	subround(I, d, a, b, c, 3, 10, 0x8f0ccc92);
	subround(I, c, d, a, b, 10, 15, 0xffeff47d);
	subround(I, b, c, d, a, 1, 21, 0x85845dd1);
	subround(I, a, b, c, d, 8, 6, 0x6fa87e4f);
	subround(I, d, a, b, c, 15, 10, 0xfe2ce6e0);
	subround(I, c, d, a, b, 6, 15, 0xa3014314);
	subround(I, b, c, d, a, 13, 21, 0x4e0811a1);
	subround(I, a, b, c, d, 4, 6, 0xf7537e82);
	subround(I, d, a, b, c, 11, 10, 0xbd3af235);
	subround(I, c, d, a, b, 2, 15, 0x2ad7d2bb);
	subround(I, b, c, d, a, 9, 21, 0xeb86d391);

	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
}

#define BLKSIZE 64

static void _init (md5sum_s *m_)
{
	_core_init (&m_->core);
	m_->blkused = 0;
	m_->lenhi = m_->lenlo = 0;
}

static void _update (md5sum_s *m_, const u8 *p, unsigned len)
{
	/*
	 * Update the length field.
	 */
	m_->lenlo += (u32)len;
	m_->lenhi += (m_->lenlo < (u32)len);
u8 *q;
	q = (u8 *)p;

	if (m_->blkused + len < BLKSIZE) {
		/*
		 * Trivial case: just add to the block.
		 */
		memcpy (m_->block + m_->blkused, q, len);
		m_->blkused += len;
		return;
	}

	/*
	 * We must complete and process at least one block.
	 */
	while (m_->blkused + len >= BLKSIZE) {
		memcpy (m_->block + m_->blkused, q, BLKSIZE - m_->blkused);
		q += BLKSIZE - m_->blkused;
		len -= BLKSIZE - m_->blkused;
		/* Now process the block. Gather bytes little-endian into words */
u32 wordblock[16];
int i;
		for (i = 0; i < 16; i++)
			wordblock[i] = load_le32 (m_->block + i * 4);
		_core_block (&m_->core, wordblock);
		m_->blkused = 0;
	}
	memcpy (m_->block, q, len);
	m_->blkused = len;
}

static void _final (md5sum_s *m_, u8 output[16])
{
unsigned pad;
	if (m_->blkused >= 56)
		pad = 56 + 64 - m_->blkused;
	else
		pad = 56 - m_->blkused;

u32 lenhi, lenlo;
	lenhi = (m_->lenhi << 3) | (m_->lenlo >> (32 - 3));
	lenlo = (m_->lenlo << 3);

u8 c[64];
	memset (c, 0, pad);
	c[0] = 0x80;
	_update (m_, c, pad);

	store_le32 (c +0, lenlo);
	store_le32 (c +4, lenhi);

	_update (m_, c, 8);

int i;
	for (i = 0; i < 4; i++)
		store_le32 (output + 4 * i, m_->core.h[i]);
}

///////////////////////////////////////////////////////////////

void md5init (struct md5sum *this_)
{
ASSERT2(sizeof(md5sum_s) <= sizeof(struct md5sum), " %d <= " VTRR "%d" VTO, (unsigned)sizeof(md5sum_s), (unsigned)sizeof(struct md5sum))
	_init ((md5sum_s *)this_);
}
void md5update (struct md5sum *this_, const u8 *p, unsigned cb)
{
	_update ((md5sum_s *)this_, p, cb);
}
void md5final (struct md5sum *this_, u8 output[16])
{
	_final ((md5sum_s *)this_, output);
}

void md5sum (const void *p, unsigned len, u8 output[16])
{
md5sum_s s;
	_init (&s);
	_update (&s, (const u8 *)p, len);
	_final (&s, output);
}
