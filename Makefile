ebin-bubbl: *.c
	sh ./generate_icon_data.sh
	tcc -o ebin-bubbl \
	*.c drw/*.c -O0 -lX11 -lXft -lXext \
	-lfontconfig -lfreetype \
	-lm -I/usr/include/freetype2 -lImlib2 \


all: ebin-bubbl


clean:
	rm -f icon_data.h


install: all
	cp -f ./ebin-bubbl /usr/bin/
	chmod 755 /usr/bin/ebin-bubbl
