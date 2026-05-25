#include "compress.h"

#include <limits.h>
#include <stdlib.h>
#include <zlib.h>

#define BUFFER_BLOCK_SIZE 4096U

int compress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len)
{
    uLongf compressed_capacity;
    uint8_t *compressed_data;
    int z_result;

    if ((in == NULL && in_len > 0U) || out == NULL || out_len == NULL) {
        return -1;
    }

    *out = NULL;
    *out_len = 0;

    if (in_len > (size_t)ULONG_MAX) {
        return -1;
    }

    compressed_capacity = compressBound((uLong)in_len);
    compressed_data = (uint8_t *)malloc((size_t)compressed_capacity);
    if (compressed_data == NULL) {
        return -1;
    }

    z_result = compress2(
        compressed_data,
        &compressed_capacity,
        in,
        (uLong)in_len,
        Z_BEST_COMPRESSION
    );
    if (z_result != Z_OK) {
        free(compressed_data);
        return -1;
    }

    *out = compressed_data;
    *out_len = (size_t)compressed_capacity;
    return 0;
}

int decompress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len)
{
    size_t capacity;
    uint8_t *decompressed_data;
    int z_result;

    if ((in == NULL && in_len > 0U) || out == NULL || out_len == NULL) {
        return -1;
    }

    *out = NULL;
    *out_len = 0;

    if (in_len > (size_t)ULONG_MAX) {
        return -1;
    }

    capacity = in_len;
    if (capacity == 0U) {
        capacity = BUFFER_BLOCK_SIZE;
    }

    while (capacity % BUFFER_BLOCK_SIZE != 0U) {
        capacity++;
    }

    for (;;) {
        uLongf decompressed_size;

        if (capacity > (size_t)ULONG_MAX) {
            return -1;
        }

        decompressed_data = (uint8_t *)malloc(capacity);
        if (decompressed_data == NULL) {
            return -1;
        }

        decompressed_size = (uLongf)capacity;
        z_result = uncompress(
            decompressed_data,
            &decompressed_size,
            in,
            (uLong)in_len
        );

        if (z_result == Z_OK) {
            *out = decompressed_data;
            *out_len = (size_t)decompressed_size;
            return 0;
        }

        free(decompressed_data);

        if (z_result != Z_BUF_ERROR) {
            return -1;
        }

        if (capacity > SIZE_MAX / 2U) {
            if (capacity > SIZE_MAX - BUFFER_BLOCK_SIZE) {
                return -1;
            }
            capacity += BUFFER_BLOCK_SIZE;
        } else {
            capacity *= 2U;
        }

        while (capacity % BUFFER_BLOCK_SIZE != 0U) {
            if (capacity == SIZE_MAX) {
                return -1;
            }
            capacity++;
        }
    }
}
