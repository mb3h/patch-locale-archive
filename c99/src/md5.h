#ifndef MD5_H_INCLUDED__
#define MD5_H_INCLUDED__

void md5sum (const void *p, unsigned len, uint8_t output[16]);

struct md5sum {
	uint8_t priv[92]; // gcc's value to x86_64 (same as i386)
};
void md5init (struct md5sum *this_);
void md5update (struct md5sum *this_, const uint8_t *p, unsigned cb);
void md5final (struct md5sum *this_, uint8_t output[16]);

#endif //ndef MD5_H_INCLUDED__
