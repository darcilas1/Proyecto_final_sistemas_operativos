#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <stddef.h>
#include <stdint.h>

int encrypt_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);
int decrypt_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);

#endif
