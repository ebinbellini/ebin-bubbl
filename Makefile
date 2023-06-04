ebin-bubbl: *.c 
	tcc -O0 *.c drw/*.c -lX11 -lXft -lXext \
	-lm -I/usr/include/freetype2 -lImlib2 \
	-o ebin-bubbl


all: ebin-bubbl


install: all
	cp -f ./ebin-bubbl /usr/bin/
	chmod 755 /usr/bin/ebin-bubbl
