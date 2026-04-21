CC := $(CROSS_COMPILE)gcc
LDFLAGS ?= -pthread
TARGET = aesdsocket


all: $(TARGET)


$(TARGET): aesdsocket.c
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
