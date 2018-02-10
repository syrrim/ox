CCFLAGS=-Iinclude/ -Wall -Werror

CC=cc ${CCFLAGS}

ox: bin/*.o include/*.h src/*
	$(CC) -g -lm -lreadline bin/*.o src/*.c -o ox

bin/lodepng.o: lib/lodepng.c 
	$(CC) -c lib/lodepng.c -o bin/lodepng.o

bin/stb_image.o: lib/stb_image.c 
	$(CC) -DSTB_IMAGE_IMPLEMENTATION -c lib/stb_image.c -o bin/stb_image.o

bin/tiny_jpeg.o: lib/tiny_jpeg.c 
	$(CC) -DTJE_IMPLEMENTATION -c lib/tiny_jpeg.c -o bin/tiny_jpeg.o
