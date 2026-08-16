# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c layout.c util.c IPCClient.c ipc.c yajl_dumps.c
OBJ = ${SRC:.c=.o}

all: dwm dwm-msg

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS} -lyajl

dwm-msg: dwm-msg.c
	${CC} -o $@ dwm-msg.c ${CFLAGS} ${LDFLAGS} -lyajl

test_util: test_util.c util.c
	${CC} -o $@ test_util.c util.c ${CFLAGS} ${LDFLAGS}

test: test_util
	./test_util

clean:
	rm -f dwm dwm-msg ${OBJ} dwm-${VERSION}.tar.gz test_util

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README config.def.h config.mk\
		dwm.1 drw.h dwm.h util.h ${SRC} dwm-msg.c dwm.png transient.c dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm dwm-msg ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm ${DESTDIR}${PREFIX}/bin/dwm-msg
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm ${DESTDIR}${PREFIX}/bin/dwm-msg\
		${DESTDIR}${MANPREFIX}/man1/dwm.1

.PHONY: all clean dist install uninstall test test_util
