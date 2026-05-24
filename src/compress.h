#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>
#include <stdint.h>

int compress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);
int decompress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);

#endif
