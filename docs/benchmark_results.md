# Benchmark Results

## Contexto de la medicion
- **Fecha:** 2026-05-24
- **Entorno:** WSL2 Ubuntu sobre Windows
- **Archivo de prueba:** `tests/test_50mb.txt`
- **Tamano del archivo:** `52428800` bytes = `50.00 MB`
- **Carga usada:** texto sintetico altamente compresible generado con `dd ... | tr '\0' 'A'`

## Metodologia
- `benchmark` mide tres escenarios: A clasico, B solo compresion y C compresion + cifrado.
- La llave se solicita **antes** de medir el escenario C, para no meter tiempo humano en el wall-clock.
- `--verify` valida el camino inverso `descifrar -> descomprimir` y compara el resultado byte a byte con el archivo original.
- La evidencia cruda queda guardada en:
  - [`docs/benchmark_run.txt`](docs/benchmark_run.txt)
  - [`docs/time_run.txt`](docs/time_run.txt)
  - [`docs/strace_run.txt`](docs/strace_run.txt)

## Resultado principal

Valores tomados de [`docs/benchmark_run.txt`](docs/benchmark_run.txt):

| Metrica | A. Clasico | B. Solo compresion | C. Compresion + cifrado |
|---|---:|---:|---:|
| Tamano transmitido (bytes) | 52428800 | 50976 | 50976 |
| % del original | 100.00% | 0.10% | 0.10% |
| Tiempo CPU total (s) | 0.170145 | 0.296133 | 0.336252 |
| Tiempo CPU compresion (s) | 0.000000 | 0.131128 | 0.122295 |
| Tiempo CPU cifrado (s) | 0.000000 | 0.000000 | 0.000109 |
| Tiempo espera I/O (s) | 1.713708 | 1.677707 | 1.679565 |
| Tiempo total (s) | 1.883853 | 1.973840 | 2.015817 |

## Lectura tecnica

### 1. Orden correcto del pipeline
El proyecto ejecuta `compresion -> cifrado`, que es el orden correcto frente a la entropia. El archivo cifrado mide exactamente lo mismo que el comprimido (`50976` bytes), lo cual confirma ademas que **RC4 no agrega padding**.

### 2. Aislamiento de CPU
- La compresion sola en el escenario B consume `0.131128 s` de CPU.
- El costo adicional del cifrado dentro del escenario C fue `0.000109 s` de CPU.
- Como el archivo comprimido termina siendo muy pequeno (`50976` bytes), el cifrado opera sobre muy pocos datos y su overhead queda en el orden de las decimas de milisegundo.

### 3. Beneficio de I/O
- Escenario A escribe `52428800` bytes.
- Escenarios B y C escriben `50976` bytes.
- La reduccion de bytes transmitidos es de aproximadamente `99.90%`.

En esta carga particular, la compresion reduce tanto el tamano final que el costo de escribir al disco cae drasticamente y el cifrado no cambia el volumen de I/O.

### 4. Integridad
La verificacion de [`docs/benchmark_run.txt`](docs/benchmark_run.txt) termina con:

`OK - integridad verificada: el contenido restaurado coincide byte a byte`

Eso demuestra que el pipeline completo no corrompe los datos al hacer `comprimir -> cifrar -> descifrar -> descomprimir`.

## Evidencia con /usr/bin/time

Valores tomados de [`docs/time_run.txt`](docs/time_run.txt):

```text
real 14.70
user 0.46
sys 0.72
```

Interpretacion:
- Este `time` externo mide la ejecucion completa del comando interactivo.
- Incluye el benchmark completo, la verificacion y la interaccion humana para escribir la llave.
- Por eso sus valores **no** se comparan directamente con la tabla interna A/B/C; la tabla interna es la que aísla las cargas del pipeline.

## Evidencia con strace

Valores tomados de [`docs/strace_run.txt`](docs/strace_run.txt):

| Syscall | Calls | Tiempo acumulado (s) | % del tiempo medido |
|---|---:|---:|---:|
| `read` | 64022 | 1.281666 | 99.12% |
| `write` | 42 | 0.010542 | 0.82% |
| `openat` | 13 | 0.000594 | 0.05% |
| `close` | 13 | 0.000292 | 0.02% |
| **Total** | **64090** | **1.293094** | **100.00%** |

Interpretacion:
- `read` domina el tiempo de syscalls observadas, lo cual es consistente con un pipeline que mueve un archivo grande desde disco hacia RAM y luego verifica el contenido restaurado.
- `write` aparece pocas veces porque solo se materializan tres archivos de salida (`output_a.bin`, `output_b.bin`, `output_c.bin`).
- Las transformaciones de compresion y cifrado ocurren en memoria, no mediante syscalls adicionales de archivo.

## Conclusiones
- El proyecto ya demuestra el orden correcto `compresion -> cifrado`.
- La llave se usa sin hardcodeo ni `argv`, y el benchmark ya no contamina las mediciones con tiempo humano de digitacion.
- RC4 no agrega padding, por lo que `output_b.bin` y `output_c.bin` tienen el mismo tamano en esta implementacion.
- La integridad del recorrido inverso quedo verificada byte a byte.
- La evidencia de `time` y `strace` ya esta almacenada en el repo para sustentar la entrega.
