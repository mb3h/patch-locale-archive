#ifndef WCWIDTH_H_INCLUDED__
#define WCWIDTH_H_INCLUDED__

void wcwidth_dtor (void *this_);
bool wcwidth_ctor (void *this_, unsigned cb, const void *width, uint32_t width_size);
void wcwidth_modify (void *this_, void *width, uint32_t wc, uint8_t value);
unsigned wcwidth_commit (void *this_, void *width, void *dst, unsigned max_bytes);

#endif //def WCWIDTH_H_INCLUDED__
