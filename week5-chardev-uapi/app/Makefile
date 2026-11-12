CC	?= gcc
CFLAGS	?= -Wall -Wextra -O2
CPPFLAGS += -I..
TARGET	= test_edu5

all: $(TARGET)

$(TARGET): test_edu5.c ../edu_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_edu5.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
