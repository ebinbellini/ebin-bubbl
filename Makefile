ebin-bubbl: *.c 
	tcc -O0 *.c -lX11 -lXft -lfontconfig -lm -lXext -lpng \
	-I/usr/include/freetype2 \
	-I/usr/X11R6/include -L/usr/X11R6/lib \
	-L/usr/lib -lImlib2 -ljpeg -ltiff -lgif -lz -lXext -lX11 \
	-o ebin-bubbl


all: ebin-bubbl


install: all
	cp -f ./ebin-bubbl /usr/bin/
	chmod 755 /usr/bin/ebin-bubbl
