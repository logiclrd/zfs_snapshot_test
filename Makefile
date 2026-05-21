probe: probe.c Makefile
	gcc -o probe probe.c -lzfs -I/usr/include/libzfs -I/usr/include/libspl -g

clean:
	rm probe

.PHONY: clean
