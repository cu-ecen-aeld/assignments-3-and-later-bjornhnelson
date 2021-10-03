#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>

#define MAX_BACKLOG 10

int main(int argc, char** argv) {

    printf("Hello world!\n");
    int status;
    int daemon_flag = 0;
    struct addrinfo hints;
    struct addrinfo* server_info;

    // process command line arguments
    if (argc == 2 && strncmp("-d", argv[1], 3) == 0) {
        daemon_flag = 1;
    }

    // setup addrinfo data structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    // initialize server_info data structure
    status = getaddrinfo(NULL, "9000", &hints, &server_info);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo");
        return -1;
    }

    // create socket
    int socket_num = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_num == -1) {
        syslog(LOG_ERR, "socket");
        return -1;
    }

    // bind socket
    status = bind(socket_num, server_info->ai_addr, server_info->ai_addrlen);
    if (status != 0) {
        syslog(LOG_ERR, "bind");
        return -1;
    }

    // set up daemon
    if (daemon_flag) {
        pid_t pid = fork();

        if (pid == -1) { // error
            perror("fork failure");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0) { // parent
            printf("Daemon pid: %d\n", pid);
            exit(EXIT_SUCCESS);
        }
        else { // child

            if (setsid() == -1) {
                perror("setsid");
                exit(EXIT_FAILURE);
            }

            if (chdir("/") == -1) {
                perror("chdir");
                exit(EXIT_FAILURE);
            }

            int dev_null_fd = open("/dev/null", O_RDWR);   

            dup2(dev_null_fd, STDIN_FILENO);
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
    }

    // listen on socket
    status = listen(socket_num, MAX_BACKLOG);
    if (status == -1) {
        syslog(LOG_ERR, "listen");
        return -1;
    }

    // free memory earlier?
    freeaddrinfo(server_info); // free the linked list of addrinfos

    return 0;
}
