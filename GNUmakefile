all: xtf8 lib

CFLAGS=	-g -O3 -std=gnu99 -pedantic -Wall -Wextra -DNDEBUG
CFLAGS+=-D_POSIX_C_SOURCE=200112L

ifneq ($(DEBUG),)
CFLAGS+=-ggdb3 -Og -UNDEBUG -DXTF8_DEBUG -DDEBUG
endif

xtf8: xtf8_main.c xtf8.c xtf8.h utf8.h
	$(CC) $(CFLAGS) -o $@ $^

lib: CFLAGS+=-fPIC -shared
lib: libxtf8.so
libxtf8.so: xtf8.c xtf8.h utf8.h
	$(CC) $(CFLAGS) -o $@ $^

lualib: CFLAGS+=-fPIC -shared -I$(LUA_INCDIR)
lualib: xtf8.so
xtf8.so: xtf8_lua.c xtf8.c xtf8.h utf8.h
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f xtf8 libxtf8.so xtf8.so *.gch
