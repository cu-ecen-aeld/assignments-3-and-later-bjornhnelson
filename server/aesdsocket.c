#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_NUM "9000"
#define MAX_BACKLOG 10
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"
#define MAX_BUF_SIZE 100

// global variables
int socket_num;
int client_fd;
int file_fd;

void handle_signals(int sig_num) {
    if ((sig_num == SIGINT) || (sig_num == SIGTERM)) {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        close(socket_num);
        close(file_fd);

        if (remove(OUTPUT_FILE_PATH) == -1) {
            perror("remove");
        }

        closelog();
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char** argv) {

    printf("Hello world!\n");
    int status;
    int daemon_flag = 0;
    struct addrinfo hints;
    struct addrinfo* server_info;

    struct sockaddr_in client_sockaddr;
    socklen_t client_addrlen = sizeof(struct sockaddr);

    long total_bytes = 0;

    openlog(NULL, 0, LOG_USER);

    // process command line arguments
    if (argc == 2 && strncmp("-d", argv[1], 3) == 0) {
        daemon_flag = 1;
    }

    // setup signal handlers
    if (signal(SIGINT, handle_signals) == SIG_ERR) {
        printf("SIGINT setup error\n");
        return -1;
    }
    if (signal(SIGTERM, handle_signals) == SIG_ERR) {
        printf("SIGTERM setup error\n");
        return -1;
    }

    // setup signal masking (happens during recv and send)
    sigset_t cur_set;
    //sigset_t prev_set;
    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGINT);
    sigaddset(&cur_set, SIGTERM);

    printf("** Signals setup\n");

    // setup addrinfo data structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    // initialize server_info data structure
    status = getaddrinfo(NULL, PORT_NUM, &hints, &server_info);
    if (status != 0) {
        perror("getaddrinfo");
        freeaddrinfo(server_info);
        return -1;
    }

    // create socket
    socket_num = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_num == -1) {
        perror("socket");
        return -1;
    }

    // reference: https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    int optval = 1;
    if (setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
        perror("setsockopt");
        close(socket_num);
        return -1;
    }

    printf("** socket() call\n");

    // bind socket
    status = bind(socket_num, server_info->ai_addr, server_info->ai_addrlen);
    if (status != 0) {
        perror("bind");
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);
    printf("** bind() call\n");

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

            // necessary?
            dup2(dev_null_fd, STDIN_FILENO);
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
    }
    printf("** Dameon setup\n");

    // listen on socket
    status = listen(socket_num, MAX_BACKLOG);
    if (status == -1) {
        perror("listen");
        return -1;
    }
    printf("** listen() call\n");

    // open file for writing
    file_fd = open(OUTPUT_FILE_PATH, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);
    if (file_fd == -1) {
        perror("open");
        return -1;
    }

    /*// malloc receive buffer
    recv_buf = malloc(recv_buf_size * sizeof(char));
    if (recv_buf == NULL) {
        printf("malloc failure\n");
        return -1;
    }

    // malloc send buffer
    send_buf = malloc(send_buf_size * sizeof(char));
    if (send_buf == NULL) {
        printf("malloc failure\n");
        return -1;
    }*/

    printf("** enter while loop\n");

    while (1) {

        //char* send_buf;
        //long send_buf_size = 0;
        int num_bytes;

        // accept connection from client
        client_fd = accept(socket_num, (struct sockaddr*) &client_sockaddr, &client_addrlen);
        if (client_fd == -1) {
            printf("accept failure\n");
            return -1;
        }

        // log IP to syslog
        char* ip_addr = inet_ntoa(client_sockaddr.sin_addr);
        syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_addr);


        long recv_buf_pos = 0;
        long recv_buf_size = MAX_BUF_SIZE;
        char* recv_buf = malloc(recv_buf_size * sizeof(char));
        memset(recv_buf, '\0', recv_buf_size);

        if (recv_buf == NULL) {
            perror("malloc failure");
            return -1;
        }

        // receive bytes
        while (1) {
            printf("\n** RECV\n");
            num_bytes = recv(client_fd, recv_buf, recv_buf_size, 0);
            if (num_bytes == -1) {
                perror("recv");
                return -1;
            }
            printf("Actual # Bytes: %d\n", num_bytes);
            printf("Recv BUF: %s", recv_buf);

            if ((num_bytes == 0) || (strchr(recv_buf, '\n') != NULL)) {
                recv_buf_pos += num_bytes;
                break;
            }

            if ((recv_buf_size - recv_buf_pos) < num_bytes) {
                recv_buf_size += num_bytes;
                recv_buf = realloc(recv_buf, recv_buf_size * sizeof(char));
            }

            //printf("A: %ld B: %d\n", recv_buf_pos, num_bytes);
            recv_buf_pos += num_bytes;
            //printf("A: %ld B: %d\n", recv_buf_pos, num_bytes);

        }

        printf("\n** WRITE\n");
        // write to file
        printf("Buf Pos: %ld\n", recv_buf_pos);
        //lseek(file_fd, total_bytes, SEEK_SET);
        //printf("^^^Lseek pos before write: %ld\n", total_bytes);
        num_bytes = write(file_fd, recv_buf, recv_buf_pos);
        if (num_bytes == -1) { // additional check here
            perror("write");
            return -1;
        }
        printf("Receive BUF: %s\n", recv_buf);
        printf("Actual # Bytes: %d\n", num_bytes);

        //close(file_fd);
        //int new_fd = open(OUTPUT_FILE_PATH, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

        long send_buf_size = recv_buf_size;
        char* send_buf = malloc(send_buf_size * sizeof(char));
        if (send_buf == NULL) {
            perror("malloc failure");
            return -1;
        }
        memset(send_buf, '\0', send_buf_size);

        printf("\n** READ\n");
        // read the file
        printf("Expected # Bytes: %d\n", num_bytes);
        //lseek(new_fd, 0, SEEK_CUR);
        printf("****Lseek pos before read: %ld\n", total_bytes);
        lseek(file_fd, total_bytes, SEEK_SET);
        num_bytes = read(file_fd, send_buf, num_bytes);
        total_bytes += num_bytes;
        if (num_bytes == -1) {
            perror("read");
            return -1;
        }
        printf("Send BUF: %s\n", send_buf);
        printf("Actual # Bytes: %d\n", num_bytes);


        printf("\n** SEND\n");
        // send data
        printf("Expected # Bytes: %d\n", num_bytes);
        num_bytes = send(client_fd, send_buf, num_bytes, 0);
        if (num_bytes == -1) {
            perror("send");
            return -1;
        }
        printf("BUF: %s\n", send_buf);
        printf("Actual # Bytes: %d\n", num_bytes);

        close(client_fd);
        syslog(LOG_DEBUG, "Closed connection");
        printf("** Finished loop iteration**\n\n");

    }

    /*// main loop
    while (1) {

        // accept connection from client
        client_fd = accept(socket_num, (struct sockaddr*) &client_sockaddr, &client_addrlen);
        if (client_fd == -1) {
            printf("accept failure\n");
            return -1;
        }

        char* ip_addr = inet_ntoa(client_sockaddr.sin_addr);
        syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_addr);

        // mask signals
        status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
        if (status == -1) {
            printf("signal masking failed\n");
        }

        // receive data
        buf_pos = 0;
        while ((num_bytes = recv(client_fd, recv_buf + buf_pos, recv_buf_size, 0)) > 0) {
            
            char* newline_ptr = strchr(recv_buf, '\n');
            buf_pos += num_bytes;

            // increase buffer size by 100 bytes
            if (buf_pos >= recv_buf_size) {
                recv_buf_size += BUF_SIZE;
                recv_buf = realloc(recv_buf, recv_buf_size * sizeof(char));

                if (recv_buf == NULL) {
                    free(recv_buf);
                    printf("realloc failure\n");
                    return -1;
                }
            }

            if (newline_ptr != NULL) {
                break;
            }
        }

        // unmask signals
        status = sigprocmask(SIG_UNBLOCK, &prev_set, NULL);
        if (status == -1) {
            printf("signal unmasking failed\n");
        }


        //file_size = lseek(file_fd, 0, SEEK_CUR);
        //printf("File Size: %ld\n", file_size);

        // reallocate send buffer if smaller than receive buffer
        if (send_buf_size < recv_buf_size) {
            send_buf_size = recv_buf_size;
            send_buf = realloc(send_buf, send_buf_size * sizeof(char));
            if (send_buf == NULL) {
                free(send_buf);
                printf("realloc failure\n");
                return -1;
            }
        }

        printf("** WRITE\n");
        // write to file
        num_bytes = buf_pos;
        printf("Expected # Bytes: %d\n", num_bytes);
        num_bytes = write(file_fd, recv_buf, num_bytes);
        printf("Receive BUF: %s\n", recv_buf);
        printf("Actual # Bytes: %d\n", num_bytes);

        if (num_bytes != buf_pos) {
            printf("error writing bytes to file\n");
        }

        close(file_fd);
        int new_fd = open(OUTPUT_FILE_PATH, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

        printf("** READ\n");
        // read the file
        printf("Expected # Bytes: %d\n", num_bytes);
        //lseek(new_fd, 0, SEEK_CUR);
        num_bytes = read(new_fd, send_buf, num_bytes);
        if (num_bytes == -1) {
            printf("read failure\n");
            return -1;
        }
        printf("Send BUF: %s\n", send_buf);
        printf("Actual # Bytes: %d\n", num_bytes);

        //lseek(file_fd, num_bytes, SEEK_CUR); // update file pos before next read

        // mask signals
        status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
        if (status == -1) {
            printf("signal masking failed\n");
        }


        printf("** SEND\n");
        // send data
        printf("Expected # Bytes: %d\n", num_bytes);
        num_bytes = send(client_fd, send_buf, num_bytes, 0);
        printf("BUF: %s\n", send_buf);
        printf("Actual # Bytes: %d\n", num_bytes);
        

        if (num_bytes == -1) {
            printf("send failure\n");
            return -1;
        }

        // unmask signals
        status = sigprocmask(SIG_UNBLOCK, &prev_set, NULL);
        if (status == -1) {
            printf("signal unmasking failed\n");
        }

        close(client_fd);
        syslog(LOG_DEBUG, "Closed connection");
        printf("** Finished loop iteration**\n\n");
    }*/


    closelog();
    close(file_fd);
    close(socket_num);

    return 0;
}
