CC = gcc
CFLAGS = -D_DEFAULT_SOURCE -Wall -Wextra -O2
LDFLAGS = -lz
TARGET = pipeline
BENCHMARK = benchmark
SRC = src/main.c src/compress.c src/encrypt.c
BENCHMARK_SRC = src/benchmark.c src/compress.c src/encrypt.c

.PHONY: all clean test benchmark-test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

$(BENCHMARK): $(BENCHMARK_SRC)
	$(CC) $(CFLAGS) -o $@ $(BENCHMARK_SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(BENCHMARK) output.bin output_a.bin output_b.bin output_c.bin tests/test_10mb.bin tests/test_50mb.txt

test: $(TARGET)
	dd if=/dev/zero of=tests/test_10mb.bin bs=1M count=10 status=none
	./$(TARGET) tests/test_10mb.bin

benchmark-test: $(BENCHMARK)
	mkdir -p tests
	dd if=/dev/zero bs=1M count=50 status=none | tr '\0' 'A' > tests/test_50mb.txt
	./$(BENCHMARK) --verify tests/test_50mb.txt
