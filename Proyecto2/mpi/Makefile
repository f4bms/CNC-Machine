CC=mpic++

CFLAGS=-Wall -Wextra

OPENCV = $(shell pkg-config --cflags --libs opencv4)

all:
	$(CC) $(CFLAGS) main.c -o mpi_processor $(OPENCV)

clean:
	rm -f mpi_processor