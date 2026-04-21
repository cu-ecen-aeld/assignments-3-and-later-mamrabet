CC := $(CROSS_COMPILE)gcc
LDFLAGS ?= -pthread -lrt
TARGET = aesdsocket


all: $(TARGET)


$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
