#include "compress.h"
#include "encrypt.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_BLOCK_SIZE 4096U

#define OUTPUT_A "output_a.bin"
#define OUTPUT_B "output_b.bin"
#define OUTPUT_C "output_c.bin"

typedef struct {
    double cpu_s;
    double real_s;
    double io_s;
    double compress_cpu_s;
    double encrypt_cpu_s;
    size_t out_bytes;
} bench_result_t;

static double timespec_to_s(const struct timespec *ts)
{
    return (double)ts->tv_sec + (double)ts->tv_nsec * 1e-9;
}

static double elapsed_s(const struct timespec *start, const struct timespec *end)
{
    return timespec_to_s(end) - timespec_to_s(start);
}

static int read_file(const char *path, uint8_t **buf, size_t *len)
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
        size_t bytes_read = fread(chunk, 1, sizeof(chunk), file);

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

static int write_file(const char *path, const uint8_t *buf, size_t len)
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

static int run_scenario_a(const char *input_path, bench_result_t *result)
{
    uint8_t *buf;
    size_t len;
    struct timespec cpu_start;
    struct timespec cpu_end;
    struct timespec real_start;
    struct timespec real_end;

    buf = NULL;
    len = 0;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC, &real_start);

    if (read_file(input_path, &buf, &len) != 0) {
        fprintf(stderr, "[A] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    if (write_file(OUTPUT_A, buf, len) != 0) {
        fprintf(stderr, "[A] Error escribiendo '%s': %s\n", OUTPUT_A, strerror(errno));
        free(buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s = elapsed_s(&cpu_start, &cpu_end);
    result->real_s = elapsed_s(&real_start, &real_end);
    result->io_s = result->real_s - result->cpu_s;
    result->compress_cpu_s = 0.0;
    result->encrypt_cpu_s = 0.0;
    result->out_bytes = len;

    free(buf);
    return 0;
}

static int run_scenario_b(const char *input_path, bench_result_t *result)
{
    uint8_t *input_buf;
    uint8_t *compressed_buf;
    size_t input_len;
    size_t compressed_len;
    struct timespec cpu_start;
    struct timespec cpu_end;
    struct timespec real_start;
    struct timespec real_end;
    struct timespec compress_start;
    struct timespec compress_end;

    input_buf = NULL;
    compressed_buf = NULL;
    input_len = 0;
    compressed_len = 0;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC, &real_start);

    if (read_file(input_path, &input_buf, &input_len) != 0) {
        fprintf(stderr, "[B] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &compress_start);
    if (compress_buffer(input_buf, input_len, &compressed_buf, &compressed_len) != 0) {
        fprintf(stderr, "[B] Error comprimiendo el buffer\n");
        free(input_buf);
        return -1;
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &compress_end);

    if (write_file(OUTPUT_B, compressed_buf, compressed_len) != 0) {
        fprintf(stderr, "[B] Error escribiendo '%s': %s\n", OUTPUT_B, strerror(errno));
        free(input_buf);
        free(compressed_buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s = elapsed_s(&cpu_start, &cpu_end);
    result->real_s = elapsed_s(&real_start, &real_end);
    result->io_s = result->real_s - result->cpu_s;
    result->compress_cpu_s = elapsed_s(&compress_start, &compress_end);
    result->encrypt_cpu_s = 0.0;
    result->out_bytes = compressed_len;

    free(input_buf);
    free(compressed_buf);
    return 0;
}

static int run_scenario_c(
    const char *input_path,
    const uint8_t *key_data,
    size_t key_len,
    bench_result_t *result
)
{
    uint8_t *input_buf;
    uint8_t *compressed_buf;
    uint8_t *encrypted_buf;
    size_t input_len;
    size_t compressed_len;
    size_t encrypted_len;
    struct timespec cpu_start;
    struct timespec cpu_end;
    struct timespec real_start;
    struct timespec real_end;
    struct timespec compress_start;
    struct timespec compress_end;
    struct timespec encrypt_start;
    struct timespec encrypt_end;

    input_buf = NULL;
    compressed_buf = NULL;
    encrypted_buf = NULL;
    input_len = 0;
    compressed_len = 0;
    encrypted_len = 0;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC, &real_start);

    if (read_file(input_path, &input_buf, &input_len) != 0) {
        fprintf(stderr, "[C] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &compress_start);
    if (compress_buffer(input_buf, input_len, &compressed_buf, &compressed_len) != 0) {
        fprintf(stderr, "[C] Error comprimiendo el buffer\n");
        free(input_buf);
        return -1;
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &compress_end);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &encrypt_start);
    if (encrypt_buffer_with_key(
            compressed_buf,
            compressed_len,
            key_data,
            key_len,
            &encrypted_buf,
            &encrypted_len
        ) != 0) {
        fprintf(stderr, "[C] Error cifrando el buffer: %s\n", strerror(errno));
        free(input_buf);
        free(compressed_buf);
        return -1;
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &encrypt_end);

    if (write_file(OUTPUT_C, encrypted_buf, encrypted_len) != 0) {
        fprintf(stderr, "[C] Error escribiendo '%s': %s\n", OUTPUT_C, strerror(errno));
        free(input_buf);
        free(compressed_buf);
        free(encrypted_buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s = elapsed_s(&cpu_start, &cpu_end);
    result->real_s = elapsed_s(&real_start, &real_end);
    result->io_s = result->real_s - result->cpu_s;
    result->compress_cpu_s = elapsed_s(&compress_start, &compress_end);
    result->encrypt_cpu_s = elapsed_s(&encrypt_start, &encrypt_end);
    result->out_bytes = encrypted_len;

    free(input_buf);
    free(compressed_buf);
    free(encrypted_buf);
    return 0;
}

static int verify_round_trip(
    const char *input_path,
    const uint8_t *key_data,
    size_t key_len
)
{
    uint8_t *original_buf;
    uint8_t *encrypted_buf;
    uint8_t *decrypted_buf;
    uint8_t *restored_buf;
    size_t original_len;
    size_t encrypted_len;
    size_t decrypted_len;
    size_t restored_len;
    int status;

    original_buf = NULL;
    encrypted_buf = NULL;
    decrypted_buf = NULL;
    restored_buf = NULL;
    original_len = 0;
    encrypted_len = 0;
    decrypted_len = 0;
    restored_len = 0;
    status = -1;

    if (read_file(input_path, &original_buf, &original_len) != 0) {
        fprintf(stderr, "[V] Error leyendo original '%s': %s\n", input_path, strerror(errno));
        goto cleanup;
    }

    if (read_file(OUTPUT_C, &encrypted_buf, &encrypted_len) != 0) {
        fprintf(stderr, "[V] Error leyendo '%s': %s\n", OUTPUT_C, strerror(errno));
        goto cleanup;
    }

    printf("[*] Verificacion de integridad: descifrar y descomprimir '%s'\n", OUTPUT_C);

    if (decrypt_buffer_with_key(
            encrypted_buf,
            encrypted_len,
            key_data,
            key_len,
            &decrypted_buf,
            &decrypted_len
        ) != 0) {
        fprintf(stderr, "[V] Error descifrando '%s': %s\n", OUTPUT_C, strerror(errno));
        goto cleanup;
    }

    if (decompress_buffer(decrypted_buf, decrypted_len, &restored_buf, &restored_len) != 0) {
        fprintf(stderr, "[V] Error descomprimiendo el buffer descifrado\n");
        goto cleanup;
    }

    if (restored_len != original_len) {
        fprintf(stderr, "[V] Falla de integridad: tamanos distintos (%zu vs %zu)\n",
                restored_len, original_len);
        goto cleanup;
    }

    if (memcmp(restored_buf, original_buf, original_len) != 0) {
        fprintf(stderr, "[V] Falla de integridad: el contenido restaurado no coincide\n");
        goto cleanup;
    }

    printf("    OK - integridad verificada: el contenido restaurado coincide byte a byte\n");
    status = 0;

cleanup:
    free(original_buf);
    free(encrypted_buf);
    free(decrypted_buf);
    free(restored_buf);
    return status;
}

static void print_results(
    size_t input_bytes,
    const bench_result_t *a,
    const bench_result_t *b,
    const bench_result_t *c,
    int verify_enabled
)
{
    double ratio_b;
    double ratio_c;

    ratio_b = (input_bytes > 0U) ? (double)b->out_bytes / (double)input_bytes * 100.0 : 0.0;
    ratio_c = (input_bytes > 0U) ? (double)c->out_bytes / (double)input_bytes * 100.0 : 0.0;

    printf("\n");
    printf("=============================================================\n");
    printf("  BENCHMARK - Compresion y Cifrado en C\n");
    printf("=============================================================\n");
    printf("  Archivo de entrada : %zu bytes (%.2f MB)\n",
           input_bytes, (double)input_bytes / (1024.0 * 1024.0));
    printf("-------------------------------------------------------------\n");
    printf("  %-28s %-14s %-14s %-14s\n",
           "Metrica", "A. Clasico", "B. Compresion", "C. Comp+Cifr");
    printf("  %-28s %-14s %-14s %-14s\n",
           "----------------------------", "--------------", "--------------", "--------------");
    printf("  %-28s %-14zu %-14zu %-14zu\n",
           "Tamano transmitido (bytes)",
           a->out_bytes, b->out_bytes, c->out_bytes);
    printf("  %-28s %-14s %-.2f%%%-10s %-.2f%%%-10s\n",
           "  (% del original)", "100.00%", ratio_b, "", ratio_c, "");
    printf("  %-28s %-14.6f %-14.6f %-14.6f\n",
           "Tiempo CPU total (s)",
           a->cpu_s, b->cpu_s, c->cpu_s);
    printf("  %-28s %-14.6f %-14.6f %-14.6f\n",
           "Tiempo CPU compresion (s)",
           0.0, b->compress_cpu_s, c->compress_cpu_s);
    printf("  %-28s %-14.6f %-14.6f %-14.6f\n",
           "Tiempo CPU cifrado (s)",
           0.0, 0.0, c->encrypt_cpu_s);
    printf("  %-28s %-14.6f %-14.6f %-14.6f\n",
           "Tiempo espera I/O (s)",
           a->io_s, b->io_s, c->io_s);
    printf("  %-28s %-14.6f %-14.6f %-14.6f\n",
           "Tiempo total (s)",
           a->real_s, b->real_s, c->real_s);
    printf("=============================================================\n");
    printf("\n");
    printf("Aislamiento de cargas de CPU:\n");
    printf("  - Compresion sola (B): %.6f s de CPU\n", b->compress_cpu_s);
    printf("  - Cifrado adicional en C: %.6f s de CPU\n", c->encrypt_cpu_s);
    printf("\n");
    if (verify_enabled != 0) {
        printf("Verificacion: habilitada, revisar el mensaje OK de integridad arriba.\n");
    } else {
        printf("Verificacion: deshabilitada. Usa --verify para validar integridad byte a byte.\n");
    }
    printf("Strace sugerido:\n");
    printf("  strace -c -e trace=read,write,open,openat,close ./benchmark --verify <archivo>\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    const char *input_path;
    uint8_t *probe_buf;
    size_t input_len;
    bench_result_t result_a;
    bench_result_t result_b;
    bench_result_t result_c;
    uint8_t *key_data;
    size_t key_len;
    int verify_enabled;
    int arg_index;

    probe_buf = NULL;
    input_len = 0;
    key_data = NULL;
    key_len = 0U;
    result_a = (bench_result_t){0};
    result_b = (bench_result_t){0};
    result_c = (bench_result_t){0};
    verify_enabled = 0;
    arg_index = 1;

    if (argc >= 2 && strcmp(argv[1], "--verify") == 0) {
        verify_enabled = 1;
        arg_index = 2;
    }

    if (argc <= arg_index) {
        fprintf(stderr, "Uso: %s [--verify] <archivo_entrada>\n", argv[0]);
        fprintf(stderr, "Ejemplo: ./benchmark --verify archivo_50mb.bin\n");
        return EXIT_FAILURE;
    }

    input_path = argv[arg_index];

    if (read_file(input_path, &probe_buf, &input_len) != 0) {
        fprintf(stderr, "No se pudo leer '%s': %s\n", input_path, strerror(errno));
        return EXIT_FAILURE;
    }
    free(probe_buf);

    printf("\n[*] Iniciando escenario A - I/O clasico...\n");
    if (run_scenario_a(input_path, &result_a) != 0) {
        return EXIT_FAILURE;
    }
    printf("    OK - salida: %s (%zu bytes)\n", OUTPUT_A, result_a.out_bytes);

    printf("[*] Iniciando escenario B - Solo compresion...\n");
    if (run_scenario_b(input_path, &result_b) != 0) {
        return EXIT_FAILURE;
    }
    printf("    OK - salida: %s (%zu bytes)\n", OUTPUT_B, result_b.out_bytes);

    printf("[*] Iniciando escenario C - Compresion + Cifrado...\n");
    printf("    (Se pedira la llave por consola fuera de la medicion)\n");
    if (prompt_secure_key("Llave de cifrado: ", &key_data, &key_len) != 0) {
        fprintf(stderr, "No se pudo leer la llave de cifrado: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (run_scenario_c(input_path, key_data, key_len, &result_c) != 0) {
        destroy_secure_key_data(key_data, key_len);
        return EXIT_FAILURE;
    }
    printf("    OK - salida: %s (%zu bytes)\n", OUTPUT_C, result_c.out_bytes);

    if (verify_enabled != 0 && verify_round_trip(input_path, key_data, key_len) != 0) {
        destroy_secure_key_data(key_data, key_len);
        return EXIT_FAILURE;
    }

    destroy_secure_key_data(key_data, key_len);
    print_results(input_len, &result_a, &result_b, &result_c, verify_enabled);

    return EXIT_SUCCESS;
}
