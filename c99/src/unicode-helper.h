#ifndef UNICODE_HELPER_H_INCLUDED__
#define UNICODE_HELPER_H_INCLUDED__

unsigned utf16to8 (void *retval, wchar_t w);
unsigned utf8len (const void *utf8);
unsigned utf8to16 (wchar_t *retval, const void *utf8, unsigned len);

#endif //ndef UNICODE_HELPER_H_INCLUDED__
