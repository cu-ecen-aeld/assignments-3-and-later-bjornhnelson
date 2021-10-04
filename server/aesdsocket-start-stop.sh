#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n /usr/bin/aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K -n /usr/bin/aesdsocket --signal TERM
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac
exit 0
