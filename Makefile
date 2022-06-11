CFLAGS = -Wall -Wextra -std=c11 -pedantic -ggdb `pkg-config --cflags sdl2`
LIBS   = `pkg-config --libs sdl2` -lm

count: main.o
	$(CC) $(CFLAGS) -o count main.o $(LIBS)

main.o: main.c image.h
	$(CC) $(CFLAGS) -c main.c -o main.o $(LIBS)

png2c.o: png2c.c
	$(CC) $(CFLAGS) -c png2c.c -o png2c.o -lm

png2c: png2c.o
	$(CC) $(CFLAGS) -o png2c png2c.o -lm

digits.png:
	@curl -fLo digits.png https://raw.githubusercontent.com/tsoding/sowon/master/digits.png

image.h: png2c digits.png
	./png2c digits.png > image.h
