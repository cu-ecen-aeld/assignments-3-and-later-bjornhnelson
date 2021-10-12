CROSS_COMPILE=
ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	${CROSS_COMPILE}${CC} ${CFLAGS} aesdsocket.c -o aesdsocket $(LDFLAGS)

clean:
	rm -f aesdsocket
