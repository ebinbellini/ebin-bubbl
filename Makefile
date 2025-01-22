SRC = bubbl.c config.h drw/drw.c drw/util.c
CC = tcc
PREFIX = /usr/local

ebin-bubbl: ${SRC}
	sh ./generate_icon_data.sh
	${CC} -o $@ \
	*.c drw/*.c -O0 -lX11 -lXft -lXext \
	-lfontconfig -lfreetype \
	-lm -I/usr/include/freetype2 -lImlib2


all: ebin-bubbl


clean:
	rm -f icon_data.h
	rm ebin-bubbl


install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ./ebin-bubbl ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ebin-bubbl


uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ebin-bubbl

.PHONY: all clean install uninstall ebin-bubbl
