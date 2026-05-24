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

    data     = NULL;
    capacity = 0;
    total    = 0;

    for (;;) {
        size_t bytes_read = fread(chunk, 1, sizeof(chunk), file);

        if (bytes_read > 0U) {
            /* Protección contra desbordamiento de size_t */
            if (total > SIZE_MAX - bytes_read) {
                free(data);
                fclose(file);
                return -1;
            }

            if (total + bytes_read > capacity) {
                size_t new_cap = capacity;
                uint8_t *new_data;

                while (new_cap < total + bytes_read) {
                    if (new_cap > SIZE_MAX - BUFFER_BLOCK_SIZE) {
                        free(data);
                        fclose(file);
                        return -1;
                    }
                    new_cap += BUFFER_BLOCK_SIZE;
                }

                new_data = (uint8_t *)realloc(data, new_cap);
                if (new_data == NULL) {
                    free(data);
                    fclose(file);
                    return -1;
                }

                data     = new_data;
                capacity = new_cap;
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
    uint8_t *buf  = NULL;
    size_t   len  = 0;
    struct timespec cpu_start, cpu_end;
    struct timespec real_start, real_end;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC,          &real_start);

    if (read_file(input_path, &buf, &len) != 0) {
        fprintf(stderr, "[A] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    if (write_file(OUTPUT_A, buf, len) != 0) {
        fprintf(stderr, "[A] Error escribiendo '%s': %s\n", OUTPUT_A, strerror(errno));
        free(buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC,          &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s     = elapsed_s(&cpu_start,  &cpu_end);
    result->real_s    = elapsed_s(&real_start, &real_end);
    result->io_s      = result->real_s - result->cpu_s;
    result->out_bytes = len;

    free(buf);
    return 0;
}



static int run_scenario_b(const char *input_path, bench_result_t *result)
{
    uint8_t *input_buf      = NULL;
    uint8_t *compressed_buf = NULL;
    size_t   input_len      = 0;
    size_t   compressed_len = 0;
    struct timespec cpu_start, cpu_end;
    struct timespec real_start, real_end;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC,          &real_start);

    if (read_file(input_path, &input_buf, &input_len) != 0) {
        fprintf(stderr, "[B] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    if (compress_buffer(input_buf, input_len, &compressed_buf, &compressed_len) != 0) {
        fprintf(stderr, "[B] Error comprimiendo el buffer\n");
        free(input_buf);
        return -1;
    }

    if (write_file(OUTPUT_B, compressed_buf, compressed_len) != 0) {
        fprintf(stderr, "[B] Error escribiendo '%s': %s\n", OUTPUT_B, strerror(errno));
        free(input_buf);
        free(compressed_buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC,          &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s     = elapsed_s(&cpu_start,  &cpu_end);
    result->real_s    = elapsed_s(&real_start, &real_end);
    result->io_s      = result->real_s - result->cpu_s;
    result->out_bytes = compressed_len;

    free(input_buf);
    free(compressed_buf);
    return 0;
}



static int run_scenario_c(const char *input_path, bench_result_t *result)
{
    uint8_t *input_buf      = NULL;
    uint8_t *compressed_buf = NULL;
    uint8_t *encrypted_buf  = NULL;
    size_t   input_len      = 0;
    size_t   compressed_len = 0;
    size_t   encrypted_len  = 0;
    struct timespec cpu_start, cpu_end;
    struct timespec real_start, real_end;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    clock_gettime(CLOCK_MONOTONIC,          &real_start);

    if (read_file(input_path, &input_buf, &input_len) != 0) {
        fprintf(stderr, "[C] Error leyendo '%s': %s\n", input_path, strerror(errno));
        return -1;
    }

    if (compress_buffer(input_buf, input_len, &compressed_buf, &compressed_len) != 0) {
        fprintf(stderr, "[C] Error comprimiendo el buffer\n");
        free(input_buf);
        return -1;
    }


    if (encrypt_buffer(compressed_buf, compressed_len, &encrypted_buf, &encrypted_len) != 0) {
        fprintf(stderr, "[C] Error cifrando el buffer: %s\n", strerror(errno));
        free(input_buf);
        free(compressed_buf);
        return -1;
    }

    if (write_file(OUTPUT_C, encrypted_buf, encrypted_len) != 0) {
        fprintf(stderr, "[C] Error escribiendo '%s': %s\n", OUTPUT_C, strerror(errno));
        free(input_buf);
        free(compressed_buf);
        free(encrypted_buf);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC,          &real_end);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    result->cpu_s     = elapsed_s(&cpu_start,  &cpu_end);
    result->real_s    = elapsed_s(&real_start, &real_end);
    result->io_s      = result->real_s - result->cpu_s;
    result->out_bytes = encrypted_len;

    free(input_buf);
    free(compressed_buf);
    free(encrypted_buf);
    return 0;
}



static void print_results(
    size_t           input_bytes,
    const bench_result_t *a,
    const bench_result_t *b,
    const bench_result_t *c
)
{
    double ratio_b = (input_bytes > 0U) ? (double)b->out_bytes / (double)input_bytes * 100.0 : 0.0;
    double ratio_c = (input_bytes > 0U) ? (double)c->out_bytes / (double)input_bytes * 100.0 : 0.0;

    printf("\n");
    printf("=============================================================\n");
    printf("  BENCHMARK — Compresión y Cifrado en C\n");
    printf("=============================================================\n");
    printf("  Archivo de entrada : %zu bytes (%.2f MB)\n",
           input_bytes, (double)input_bytes / (1024.0 * 1024.0));
    printf("-------------------------------------------------------------\n");
    printf("  %-28s %-14s %-14s %-14s\n",
           "Métrica", "A. Clásico", "B. Compresión", "C. Comp+Cifr");
    printf("  %-28s %-14s %-14s %-14s\n",
           "----------------------------", "--------------", "--------------", "--------------");

  
    printf("  %-28s %-14zu %-14zu %-14zu\n",
           "Tamaño transmitido (bytes)",
           a->out_bytes, b->out_bytes, c->out_bytes);

   
    printf("  %-28s %-14s %-.1f%%%-11s %-.1f%%%-11s\n",
           "  (% del original)",
           "100%",
           ratio_b, "",
           ratio_c, "");


    printf("  %-28s %-14.4f %-14.4f %-14.4f\n",
           "Tiempo CPU (s)",
           a->cpu_s, b->cpu_s, c->cpu_s);

  
    printf("  %-28s %-14.4f %-14.4f %-14.4f\n",
           "Tiempo espera I/O (s)",
           a->io_s, b->io_s, c->io_s);

   
    printf("  %-28s %-14.4f %-14.4f %-14.4f\n",
           "Tiempo total (s)",
           a->real_s, b->real_s, c->real_s);

    printf("=============================================================\n");
    printf("\n");
    printf("NOTA: Para la tabla de benchmark_results.md copia los valores\n");
    printf("anteriores. Recuerda correr tambien:\n");
    printf("  strace -c -e trace=read,write,open,openat,close ./benchmark <archivo>\n");
    printf("para contar syscalls de I/O por escenario.\n");
    printf("\n");
}


int main(int argc, char *argv[])
{
    const char   *input_path;
    uint8_t      *probe_buf = NULL;
    size_t        input_len = 0;
    bench_result_t result_a = {0};
    bench_result_t result_b = {0};
    bench_result_t result_c = {0};

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_entrada>\n", argv[0]);
        fprintf(stderr, "Ejemplo: ./benchmark archivo_50mb.bin\n");
        return EXIT_FAILURE;
    }

    input_path = argv[1];

  
    if (read_file(input_path, &probe_buf, &input_len) != 0) {
        fprintf(stderr, "No se pudo leer '%s': %s\n", input_path, strerror(errno));
        return EXIT_FAILURE;
    }
    free(probe_buf);
    probe_buf = NULL;

    printf("\n[*] Iniciando escenario A — I/O clasico...\n");
    if (run_scenario_a(input_path, &result_a) != 0) {
        return EXIT_FAILURE;
    }
    printf("    OK — salida: %s (%zu bytes)\n", OUTPUT_A, result_a.out_bytes);

    printf("[*] Iniciando escenario B — Solo compresion...\n");
    if (run_scenario_b(input_path, &result_b) != 0) {
        return EXIT_FAILURE;
    }
    printf("    OK — salida: %s (%zu bytes)\n", OUTPUT_B, result_b.out_bytes);

    printf("[*] Iniciando escenario C — Compresion + Cifrado...\n");
    printf("    (Se pedira la llave por consola)\n");
    if (run_scenario_c(input_path, &result_c) != 0) {
        return EXIT_FAILURE;
    }
    printf("    OK — salida: %s (%zu bytes)\n", OUTPUT_C, result_c.out_bytes);

    print_results(input_len, &result_a, &result_b, &result_c);

    return EXIT_SUCCESS;
}