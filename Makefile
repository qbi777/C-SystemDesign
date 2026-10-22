CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
TARGET  = lsb_steg
OBJS    = main.o encode.o decode.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test_encode: test_encode.o encode.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c encode.h decode.h types.h common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET) test_encode steged_img.bmp stego_img.bmp decoded.*

.PHONY: all clean
