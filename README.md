# pipeline-io-c

## Integrantes

- **Daniel Arcila**
- **Juan Esteban Pena**
- **Jeronimo Contreras**

Pipeline de seguridad en C que implementa lectura, compresion y cifrado de archivos optimizando el bus I/O mediante buffers alineados al tamano de pagina del sistema operativo.

## Arquitectura del pipeline

```text
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

> **Regla arquitectonica clave:** siempre comprimir primero, encriptar despues.
> La encriptacion genera datos pseudoaleatorios de alta entropia, lo que hace
> ineficiente cualquier compresion posterior.

## Requisitos

Este proyecto esta pensado para ejecutarse en Linux o WSL con Ubuntu. En PowerShell puro de Windows puede fallar porque el proyecto usa herramientas y cabeceras POSIX como `make`, `dd`, `getpass()`, `mlock()` y zlib.

En Ubuntu/WSL instala las dependencias:

```bash
sudo apt update
sudo apt install build-essential zlib1g-dev make
```

Si todavia no tienes Ubuntu en WSL, puedes instalarlo desde PowerShell:

```powershell
wsl --install -d Ubuntu
```


## Como ejecutar el programa paso a paso

1. Crear la carpeta de pruebas si no existe:

```bash
mkdir -p tests
```

2. Limpiar binarios anteriores:

```bash
make clean
```

3. Compilar el proyecto:

```bash
make all
```

4. Generar un archivo de prueba de 10 MB y ejecutar el pipeline:

```bash
make test
```

5. Cuando el programa pida la llave, escribe cualquier clave para la prueba. La terminal no la muestra mientras escribes:

```text
Llave de cifrado:
```

6. Verifica que aparezca una salida parecida a esta:

```text
Tamano original: 10485760 bytes
Tamano comprimido: X bytes
Tamano cifrado: X bytes
Ratio de compresion: 0.XXXX
```

7. Verifica que se haya generado el archivo cifrado:

```bash
ls -lh output.bin
```

Si `output.bin` existe y la salida muestra los tamanos del archivo, la prueba del pipeline principal fue exitosa.

## Ejecutar con un archivo propio

Tambien puedes ejecutar el pipeline con cualquier archivo:

```bash
make all
./pipeline ruta/al/archivo.bin
```

El resultado se guarda en:

```text
output.bin
```

## Comandos utiles

```bash
# Compilar
make all

# Generar archivo de prueba de 10 MB y ejecutar
make test

# Limpiar binarios y archivos generados por make
make clean
```

## Estructura del proyecto

```text
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

## Modulos

### compress.c / compress.h

Implementa compresion y descompresion en memoria usando zlib.

| Funcion | Descripcion |
|---|---|
| `compress_buffer(in, in_len, out, out_len)` | Comprime un buffer en memoria con `Z_BEST_COMPRESSION` |
| `decompress_buffer(in, in_len, out, out_len)` | Descomprime un buffer previamente comprimido |

Ambas funciones retornan `0` en exito y `-1` en error. El llamador debe liberar `*out` con `free()`.

### encrypt.c / encrypt.h

Implementa cifrado simetrico RC4 operando unicamente en RAM.

Detalles de seguridad implementados:

- `encrypt_buffer()` y `decrypt_buffer()` trabajan sobre buffers en memoria.
- La llave se pide por consola con `getpass()`, no por `argv` ni hardcodeada.
- La llave se copia a un buffer bloqueado con `mlock()` para evitar swap.
- La copia temporal de `getpass()` y el buffer bloqueado se borran con `explicit_bzero()`.
- El buffer bloqueado se libera con `munlock()` despues de usar la llave.

### benchmark.c

Modulo pendiente para medir y comparar:

| Escenario | Descripcion |
|---|---|
| A. Clasico | Lectura y escritura directa sin transformaciones |
| B. Solo compresion | Pipeline con compresion activada |
| C. Compresion + cifrado | Pipeline completo |

Los resultados finales deben documentarse en [`docs/benchmark_results.md`](docs/benchmark_results.md).

## Estado del trabajo

| Modulo | Estado |
|---|---|
| Estructura base del proyecto | Completo |
| compress.c / compress.h | Completo |
| main.c | Completo |
| encrypt.c / encrypt.h | Completo |
| Integracion final main.c | Cifrado integrado |
| benchmark.c | Pendiente |
| docs/benchmark_results.md | Pendiente |
