#ifndef ENDIAN_HELPER_H_INCLUDED__
#define ENDIAN_HELPER_H_INCLUDED__

// PENDING: 'inline' keyword

static uint32_t load_le32 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const uint32_t *)src;
#else // __BIG_ENDIAN
const uint8_t *p;
	p = (const uint8_t *)src;
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
#endif
}

static void *store_le32 (void *dst, unsigned val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(uint32_t *)dst = (uint32_t)val;
	return (uint32_t *)dst +1;
#else // __BIG_ENDIAN
uint8_t *p;
	p = (uint8_t *)dst;
	*p++ = (uint8_t)val;
	*p++ = (uint8_t)(0xFF & val >> 8);
	*p++ = (uint8_t)(0xFF & val >> 16);
	*p++ = (uint8_t)(0xFF & val >> 24);
	return p;
#endif
}

static uint16_t load_le16 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const uint16_t *)src;
#else // __BIG_ENDIAN
const uint8_t *p;
	p = (const uint8_t *)src;
	return p[0] | p[1]<<8;
#endif
}

static void *store_le16 (void *dst, unsigned val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(uint16_t *)dst = (uint16_t)val;
	return (uint16_t *)dst +1;
#else // __BIG_ENDIAN
uint8_t *p;
	p = (uint8_t *)dst;
	*p++ = (uint8_t)val;
	*p++ = (uint8_t)(0xFF & val >> 8);
	return p;
#endif
}

#endif //ndef ENDIAN_HELPER_H_INCLUDED__
