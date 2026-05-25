# pipeline-io-c

## Integrantes
- Daniel Arcila
- Juan Esteban Peña
- Jerónimo Contreras

Pipeline de seguridad en C que implementa lectura, compresión y cifrado de archivos optimizando el bus I/O mediante buffers alineados al tamaño de página del sistema operativo.

## Arquitectura del pipeline

```
Archivo en disco
      |
      v
 read_file_to_buffer()        <- bloques de 4096 bytes
      |
      v
 compress_buffer()            <- zlib Z_BEST_COMPRESSION
      |
      v
 encrypt_buffer()             <- RC4 en RAM + llave protegida con mlock()
      |
      v
 write_buffer_to_file()       <- escribe output.bin al disco
```

**Regla arquitectónica clave:** siempre comprimir primero, encriptar después. La encriptación genera datos pseudoaleatorios de alta entropía, lo que hace ineficiente cualquier compresión posterior.

## Requisitos

Este proyecto está pensado para ejecutarse en **Linux o WSL con Ubuntu**. En PowerShell puro de Windows puede fallar porque el proyecto usa herramientas y cabeceras POSIX como `make`, `dd`, `getpass()`, `mlock()` y `zlib`.

En Ubuntu/WSL instala las dependencias:

```bash
sudo apt update
sudo apt install build-essential zlib1g-dev make
```

## Cómo ejecutar el programa paso a paso

**Crear la carpeta de pruebas si no existe:**
```bash
mkdir -p tests
```

**Limpiar binarios anteriores:**
```bash
make clean
```

**Compilar el proyecto:**
```bash
make all
```

**Generar un archivo de prueba de 10 MB y ejecutar el pipeline:**
```bash
make test
```

Cuando el programa pida la llave, escribe cualquier clave para la prueba. La terminal no la muestra mientras escribes:
```
Llave de cifrado:
```

Verifica que aparezca una salida parecida a esta:
```
Tamano original: 10485760 bytes
Tamano comprimido: X bytes
Tamano cifrado: X bytes
Ratio de compresion: 0.XXXX
```

Verifica que se haya generado el archivo cifrado:
```bash
ls -lh output.bin
```

Si `output.bin` existe y la salida muestra los tamaños del archivo, la prueba del pipeline principal fue exitosa.

## Ejecutar con un archivo propio

También puedes ejecutar el pipeline con cualquier archivo:

```bash
make all
./pipeline ruta/al/archivo.bin
```

El resultado se guarda en:
```
output.bin
```

## Ejecutar el benchmark

```bash
# Compilar benchmark
make benchmark

# Generar archivo de prueba de 50 MB y correr benchmark con verificacion
make benchmark-test
```

Detalles del benchmark:
- El archivo de prueba es un texto sintetico de 50 MB altamente compresible.
- El benchmark pide la llave **fuera** de la ventana de medicion para no contaminar el wall-clock con tiempo humano.
- `--verify` descifra, descomprime y compara byte a byte contra el archivo original.

Tambien puedes correrlo manualmente:

```bash
mkdir -p tests
dd if=/dev/zero bs=1M count=50 status=none | tr '\0' 'A' > tests/test_50mb.txt
./benchmark --verify tests/test_50mb.txt
```

Los resultados y evidencias se documentan en:
- [`docs/benchmark_results.md`](docs/benchmark_results.md)
- [`docs/benchmark_run.txt`](docs/benchmark_run.txt)
- [`docs/time_run.txt`](docs/time_run.txt)
- [`docs/strace_run.txt`](docs/strace_run.txt)

## Comandos útiles

```bash
# Compilar
make all

# Generar archivo de prueba de 10 MB y ejecutar
make test

# Compilar benchmark
make benchmark

# Generar archivo de 50 MB y correr benchmark con verificacion
make benchmark-test

# Limpiar binarios y archivos generados por make
make clean
```

## Estructura del proyecto

```
pipeline-io-c/
|-- src/
|   |-- compress.c
|   |-- compress.h
|   |-- main.c
|   |-- encrypt.c
|   |-- encrypt.h
|   `-- benchmark.c
|-- tests/
|-- docs/
|   `-- benchmark_results.md
|-- Makefile
`-- README.md
```

## Módulos

### compress.c / compress.h

Implementa compresión y descompresión en memoria usando zlib.

| Función | Descripción |
|---|---|
| `compress_buffer(in, in_len, out, out_len)` | Comprime un buffer en memoria con `Z_BEST_COMPRESSION` |
| `decompress_buffer(in, in_len, out, out_len)` | Descomprime un buffer previamente comprimido |

Ambas funciones retornan `0` en éxito y `-1` en error. El llamador debe liberar `*out` con `free()`.

### encrypt.c / encrypt.h

Implementa cifrado simétrico RC4 operando únicamente en RAM.

**Detalles de seguridad implementados:**
- `encrypt_buffer()` y `decrypt_buffer()` trabajan sobre buffers en memoria.
- La llave se pide por consola con `getpass()`, no por `argv` ni hardcodeada.
- La llave se copia a un buffer bloqueado con `mlock()` para evitar swap.
- La copia temporal de `getpass()` y el buffer bloqueado se borran con `explicit_bzero()`.
- El buffer bloqueado se libera con `munlock()` después de usar la llave.

### benchmark.c

Mide y compara los tres escenarios del pipeline y aísla el costo de CPU de cada transformación:

| Escenario | Descripción |
|---|---|
| A. Clásico | Lectura y escritura directa sin transformaciones |
| B. Solo compresión | Pipeline con compresión activada |
| C. Compresión + cifrado | Pipeline completo |

Ademas, `--verify` ejecuta el recorrido inverso `descifrar -> descomprimir` y valida que el contenido restaurado coincide byte a byte con el original.

---

## Reglas Arquitectónicas

### Regla 1 — Buffers de 4096 bytes
Los bloques de lectura y escritura están alineados al tamaño de página del sistema operativo (4096 bytes = 4 KB). Este valor coincide con el tamaño de página estándar en x86/Linux y con el bloque del sistema de archivos ext4, lo que evita lecturas parciales y maximiza la eficiencia del bus I/O.

### Regla 2 — Comprimir antes de encriptar
La compresión busca patrones repetitivos en los datos. La encriptación genera salida pseudoaleatoria de alta entropía, eliminando cualquier patrón. Si se encripta primero, la compresión posterior es inútil y el archivo puede incluso crecer. El orden correcto es siempre: comprimir → encriptar.

### Regla 3 — Todo en RAM
Ninguna transformación intermedia toca el disco. Los buffers de entrada, comprimido y cifrado viven en el heap durante todo el pipeline. Solo la lectura inicial y la escritura final realizan syscalls de I/O.

### Regla 4 — Gestión segura de llaves
La llave criptográfica nunca se pasa por argumentos de línea de comandos ni se hardcodea. Se solicita por consola con `getpass()`, se bloquea en RAM con `mlock()` para evitar swap, y se destruye con `explicit_bzero()` inmediatamente después de usarse.

### Regla 5 — Liberar siempre
Todo buffer allocado con `malloc()` o `realloc()` tiene un camino de liberación con `free()` en todos los flujos posibles, incluyendo los caminos de error.

### Regla 6 — Criptografía en C Space

**Mandato:** El algoritmo de encriptación simétrico opera exclusivamente sobre buffers en memoria RAM (C Space), nunca sobre archivos en disco. La implementación usa RC4 propio sin depender de funciones de alto nivel de librerías que escriban a disco.

**Seguridad de la llave:** La llave debe ser borrada de la memoria RAM inmediatamente después de usarse. Un ingeniero de OS no deja basura criptográfica en la pila (stack) ni en el heap. El flujo obligatorio es:

```
getpass() → malloc() → mlock() → memcpy() → usar llave → explicit_bzero() → munlock() → free()
```

**Prohibido:**
- Pasar la llave por `argv[]`
- Hardcodear la llave en el código fuente
- Usar funciones de OpenSSL que escriban directamente a disco
- Dejar la llave en memoria sin destruir después de cifrar
