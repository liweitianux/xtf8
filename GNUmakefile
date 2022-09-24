all: xtf8

CFLAGS=	-g -O3 -std=gnu99 -pedantic -Wall -Wextra -DNDEBUG
CFLAGS+=-D_POSIX_C_SOURCE=200112L

ifneq ($(DEBUG),)
CFLAGS+=-ggdb3 -Og -UNDEBUG -DXTF8_DEBUG -DDEBUG
endif

xtf8: xtf8.h utf8.h
xtf8: xtf8_main.c xtf8.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f xtf8
