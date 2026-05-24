# pipeline-io-c

## Integrantes
- **Daniel Arcila** 
- **Juan Esteban Peña** 
- **Jeronimo Contreras** 

Pipeline de seguridad en C que implementa lectura, compresión y cifrado de archivos optimizando el bus I/O mediante buffers alineados al tamaño de página del sistema operativo.

## Arquitectura del pipeline

```
Archivo en disco
      │
      ▼
 read_file_to_buffer()        ← bloques de 4096 bytes (tamaño de página x86)
      │
      ▼
 compress_buffer()            ← zlib Z_BEST_COMPRESSION
      │
      ▼
 encrypt_buffer()             ← cifrado simétrico en RAM (TODO: Persona 2)
      │
      ▼
 write_buffer_to_file()       ← escribe output.bin al disco
```

> **Regla arquitectónica clave:** siempre comprimir primero, encriptar después.
> La encriptación genera datos pseudoaleatorios (entropía máxima) que hacen
> imposible cualquier compresión posterior.

## Estructura del proyecto

```
pipeline-io-c/
├── src/
│   ├── compress.c       Módulo de compresión (Persona 1)
│   ├── compress.h       Módulo de compresión (Persona 1)
│   ├── main.c           Pipeline base + TODO hooks para P2 (Persona 1)
│   ├── encrypt.c        Módulo de cifrado (Persona 2 — en progreso)
│   ├── encrypt.h        Módulo de cifrado (Persona 2 — en progreso)
│   └── benchmark.c      Mediciones de rendimiento (Persona 3 — pendiente)
├── tests/
│   └── test_file_50mb.bin   ← generado con make test
├── docs/
│   └── benchmark_results.md ← tabla comparativa final (Persona 3)
├── Makefile
└── README.md
```

## Requisitos

- GCC con soporte C99 o superior
- zlib (`sudo apt install zlib1g-dev` en Ubuntu)
- Make

## Compilar y correr

```bash
# Compilar
make all

# Generar archivo de prueba (10 MB) y correr el pipeline
make test

# Limpiar binarios
make clean
```

### Salida esperada

```
Tamano original:    10485760 bytes
Tamano comprimido:  XXXXXX bytes
Ratio de compresion: 0.XXXX
```

## Módulos

### compress.c / compress.h 

Implementa compresión y descompresión en memoria usando zlib.

| Función | Descripción |
|---|---|
| `compress_buffer(in, in_len, out, out_len)` | Comprime un buffer en memoria con Z_BEST_COMPRESSION |
| `decompress_buffer(in, in_len, out, out_len)` | Descomprime un buffer previamente comprimido |

Ambas retornan `0` en éxito y `-1` en error. El llamador es responsable de liberar `*out` con `free()`.

**¿Por qué 4096 bytes?**
El buffer de lectura usa bloques de 4096 bytes porque coincide con el tamaño de página de memoria virtual en arquitectura x86/Linux y con el bloque estándar del sistema de archivos ext4. Alinear los buffers a este tamaño evita lecturas parciales y maximiza la eficiencia del bus I/O.

### encrypt.c / encrypt.h  *(Persona 2 — pendiente)*

Debe implementar cifrado simétrico operando **únicamente en RAM**, nunca escribiendo la llave a disco.

Requisitos:
- Función `encrypt_buffer()` y `decrypt_buffer()`
- La llave debe pedirse por consola (no hardcodeada)
- Borrar la llave de la RAM con `explicit_bzero()` inmediatamente después de usarla
- Usar `mlock()` para evitar que el SO mueva la llave al Swap
- Enchufar en `main.c` donde están los comentarios `TODO Persona 2`

### benchmark.c  *(Persona 3 — pendiente)*

Debe medir y comparar los tres escenarios:

| Escenario | Descripción |
|---|---|
| A. Clásico | Lectura y escritura directa sin transformaciones |
| B. Solo compresión | Pipeline con compresión activada |
| C. Compresión + cifrado | Pipeline completo |

Usar `clock_gettime(CLOCK_MONOTONIC)` para medir tiempo de CPU y `strace` para aislar el tiempo de espera I/O.

## Benchmark (resultados finales)

Ver [`docs/benchmark_results.md`](docs/benchmark_results.md) — se completa cuando Persona 3 termine su módulo.

## División de trabajo

| Módulo | Responsable | Estado |
|---|---|---|
| Estructura base del proyecto | Persona 1 | Completo |
| compress.c / compress.h | Persona 1 | Completo |
| main.c (pipeline base) | Persona 1 | Completo |
| encrypt.c / encrypt.h | Persona 2 | En progreso |
| benchmark.c | Persona 3 | Pendiente |
| docs/benchmark_results.md | Persona 3 | Pendiente |
| Integración final main.c | Los 3 | Pendiente |
