CROSS_COMPILE=
CC=gcc
CFLAGS = -g -Wall -Werror

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	${CROSS_COMPILE}${CC} ${CFLAGS} aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket