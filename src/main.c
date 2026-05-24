#include "compress.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_BLOCK_SIZE 4096U

static int read_file_to_buffer(const char *path, uint8_t **buf, size_t *len)
{
    FILE *file;
    uint8_t *data;
    uint8_t chunk[BUFFER_BLOCK_SIZE];
    size_t capacity;
    size_t total;

    if (path == NULL || buf == NULL || len == NULL) {
        return -1;
    }

    *buf = NULL;
    *len = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    data = NULL;
    capacity = 0;
    total = 0;

    for (;;) {
        size_t bytes_read;

        bytes_read = fread(chunk, 1, sizeof(chunk), file);
        if (bytes_read > 0U) {
            if (total > SIZE_MAX - bytes_read) {
                free(data);
                fclose(file);
                return -1;
            }

            if (total + bytes_read > capacity) {
                size_t new_capacity = capacity;
                uint8_t *new_data;

                while (new_capacity < total + bytes_read) {
                    if (new_capacity > SIZE_MAX - BUFFER_BLOCK_SIZE) {
                        free(data);
                        fclose(file);
                        return -1;
                    }
                    new_capacity += BUFFER_BLOCK_SIZE;
                }

                new_data = (uint8_t *)realloc(data, new_capacity);
                if (new_data == NULL) {
                    free(data);
                    fclose(file);
                    return -1;
                }

                data = new_data;
                capacity = new_capacity;
            }

            memcpy(data + total, chunk, bytes_read);
            total += bytes_read;
        }

        if (bytes_read < sizeof(chunk)) {
            if (ferror(file) != 0) {
                free(data);
                fclose(file);
                return -1;
            }
            break;
        }
    }

    if (fclose(file) != 0) {
        free(data);
        return -1;
    }

    *buf = data;
    *len = total;
    return 0;
}

static int write_buffer_to_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *file;
    size_t bytes_written;

    if (path == NULL || (buf == NULL && len > 0U)) {
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    if (len > 0U) {
        bytes_written = fwrite(buf, 1, len, file);
        if (bytes_written != len) {
            fclose(file);
            return -1;
        }
    }

    if (fclose(file) != 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    const char *input_path;
    uint8_t *input_buffer;
    uint8_t *compressed_buffer;
    size_t input_len;
    size_t compressed_len;
    double compression_ratio;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_entrada>\n", argv[0]);
        return EXIT_FAILURE;
    }

    input_path = argv[1];
    input_buffer = NULL;
    compressed_buffer = NULL;
    input_len = 0;
    compressed_len = 0;

    if (read_file_to_buffer(input_path, &input_buffer, &input_len) != 0) {
        fprintf(stderr, "Error al leer '%s': %s\n", input_path, strerror(errno));
        free(input_buffer);
        return EXIT_FAILURE;
    }

    if (compress_buffer(input_buffer, input_len, &compressed_buffer, &compressed_len) != 0) {
        fprintf(stderr, "Error al comprimir el archivo de entrada\n");
        free(input_buffer);
        free(compressed_buffer);
        return EXIT_FAILURE;
    }

    /* TODO Persona 2: llamar encrypt_buffer() aqui antes de escribir a disco. */
    /* TODO Persona 2: ajustar el buffer de salida si el cifrado cambia tamano/formato. */

    if (write_buffer_to_file("output.bin", compressed_buffer, compressed_len) != 0) {
        fprintf(stderr, "Error al escribir 'output.bin': %s\n", strerror(errno));
        free(input_buffer);
        free(compressed_buffer);
        return EXIT_FAILURE;
    }

    if (input_len == 0U) {
        compression_ratio = 0.0;
    } else {
        compression_ratio = (double)compressed_len / (double)input_len;
    }

    printf("Tamano original: %zu bytes\n", input_len);
    printf("Tamano comprimido: %zu bytes\n", compressed_len);
    printf("Ratio de compresion: %.4f\n", compression_ratio);

    free(input_buffer);
    free(compressed_buffer);
    return EXIT_SUCCESS;
}
