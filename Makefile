PREFIX = /usr/local
CC = gcc

CFLAGS = -std=c99 -pedantic -D_DEFAULT_SOURCE

nchk: nchk.c config.h
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ $<

config.h: config.def.h
	@cp config.h config.h.bak || echo \"config backup on config.h.bak\"
	cp config.def.h config.h

install: nchk
	mkdir -p ${PREFIX}/bin
	install -Dm755 $< ${PREFIX}/bin/$<

clean:
	rm -f nchk

distclean:
	rm -f nchk config.h config.h.bak

.PHONY: install clean
