CC=gcc

CFLAGS=-Wall -Wextra

SRC = main.c admin_tareas.c ../cifrado/AES.c

INC = -I../cifrado

LDLIBS = -lcrypto

all:
	$(CC) $(CFLAGS) $(INC) $(SRC) -o servidor $(LDLIBS)

clean:
	rm -f servidor