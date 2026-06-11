CC=gcc
CFLAGS=-Wall -Wextra

CRYPTO_SRC=../cifrado/AES.c
CRYPTO_INC=-I../cifrado
LDLIBS=-lcrypto

all:
	$(CC) $(CFLAGS) $(CRYPTO_INC) main.c $(CRYPTO_SRC) -o cliente $(LDLIBS)

clean:
	rm -f cliente