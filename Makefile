OBJECTS=main.o i8085.o i8279.o i8155.o serial.o mem.o io.o
CFLAGS=-Wall -Wextra
LDFLAGS=-lncurses

all: sdk85emu

sdk85emu: ${OBJECTS}
	gcc -o sdk85emu $^ ${LDFLAGS}

main.o: main.c
	gcc -c $^ ${CFLAGS}

i8085.o: i8085.c
	gcc -c $^ ${CFLAGS}

i8279.o: i8279.c
	gcc -c $^ ${CFLAGS}

i8155.o: i8155.c
	gcc -c $^ ${CFLAGS}

serial.o: serial.c
	gcc -c $^ ${CFLAGS}

mem.o: mem.c
	gcc -c $^ ${CFLAGS}

io.o: io.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o sdk85emu

