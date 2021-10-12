CROSS_COMPILE=
CFLAGS = -g -Wall -Werror
LDFLAGS = -pthread -lrt

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	${CROSS_COMPILE}${CC} ${CFLAGS} aesdsocket.c -o aesdsocket $(LDFLAGS)

clean:
	rm -f aesdsocket
