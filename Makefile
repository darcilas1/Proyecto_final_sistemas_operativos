CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lz
TARGET = pipeline
SRC = src/main.c src/compress.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) output.bin tests/test_10mb.bin

test: $(TARGET)
	dd if=/dev/zero of=tests/test_10mb.bin bs=1M count=10 status=none
	./$(TARGET) tests/test_10mb.bin
