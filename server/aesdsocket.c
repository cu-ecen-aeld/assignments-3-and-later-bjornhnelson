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
#define HUNDRED_BYTES 100

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

int main(int argc, char **argv) {

    printf("Starting server\n");
    int status;
    int daemon_flag = 0;
    struct addrinfo hints;
    struct addrinfo* server_info;
    struct sockaddr_in client_sockaddr;
    socklen_t client_addrlen = sizeof(struct sockaddr);

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
    sigset_t prev_set;
    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGINT);
    sigaddset(&cur_set, SIGTERM);

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

    // code to avoid binding on same socket issue
    // reference: https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    int optval = 1;
    if (setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
        perror("setsockopt");
        close(socket_num);
        return -1;
    }

    // bind socket
    status = bind(socket_num, server_info->ai_addr, server_info->ai_addrlen);
    if (status != 0) {
        perror("bind");
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);

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
        perror("listen");
        return -1;
    }
	
	// open file for writing
    file_fd = open(OUTPUT_FILE_PATH, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);
    if (file_fd == -1) {
        perror("open");
        return -1;
    }

    int num_bytes;
    char buf[HUNDRED_BYTES];
	memset(buf, '\0', HUNDRED_BYTES); // clear buf
	int send_buf_size = 0;

	while (1) {
        // accept connection from client
        client_fd = accept(socket_num, (struct sockaddr*) &client_sockaddr, &client_addrlen);
        if (client_fd == -1) {
            perror("accept");
            return -1;
        }

		// log IP to syslog
        char* ip_addr = inet_ntoa(client_sockaddr.sin_addr);
        syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_addr);

		int recv_buf_pos = 0;
		long recv_buf_size = HUNDRED_BYTES;
		
		char* recv_buf = malloc(recv_buf_size * sizeof(char));
        if (recv_buf == NULL) {
            perror("malloc failure");
            return -1;
        }

        // mask signals
        status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
        if (status == -1) {
            printf("signal masking failed\n");
        }

        // receive data from client
        // each loop iteration is 1 newline of input
        while (1) {
            // save up to 100 bytes into buf
			num_bytes = recv(client_fd, buf, HUNDRED_BYTES, 0);

            // exit loop on error
            if (num_bytes == -1) {
                perror("recv");
                return -1;
            }

			// check if allocated buf size is sufficient
			if(recv_buf_pos + num_bytes > recv_buf_size) {
				recv_buf_size += num_bytes;
				recv_buf = realloc(recv_buf, recv_buf_size * sizeof(char));
			}
			
			// copy buf to recv_buf
			memcpy(recv_buf + recv_buf_pos, buf, num_bytes);
			recv_buf_pos += num_bytes;

            // exit loop if there was a new line character received
			if(strchr(buf, '\n') != NULL)
				break;
        }

        // unmask signals
        status = sigprocmask(SIG_UNBLOCK, &prev_set, NULL);
        if (status == -1) {
            printf("signal unmasking failed\n");
        }
		
		// write new bytes to file
		num_bytes = write(file_fd, recv_buf, recv_buf_pos);
        printf("** Writing %d bytes\n", num_bytes);
		if (num_bytes == -1 || num_bytes != recv_buf_pos) {
			perror("write");
			return -1;
		}
		
		// go to start of file
		lseek(file_fd, 0, SEEK_SET);
		
		// allocate memory for send buffer
		send_buf_size += recv_buf_pos;
		char* send_buf = malloc(send_buf_size * sizeof(char));
        if (send_buf == NULL) {
            perror("malloc failure");
            return -1;
        }
			
		// read the ENTIRE file
		num_bytes = read(file_fd, send_buf, send_buf_size);
        printf("** Reading %d bytes\n", send_buf_size);
		if (num_bytes == -1 || num_bytes != send_buf_size) {
			perror("read");
			return -1;
		}

        // mask signals
        status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
        if (status == -1) {
            printf("signal masking failed\n");
        }
		
		// send data to the client
		num_bytes = send(client_fd, send_buf, num_bytes, 0);
		if (num_bytes == -1 || num_bytes != send_buf_size) {
			perror("send");
			return -1;
		}

        // unmask signals
        status = sigprocmask(SIG_UNBLOCK, &prev_set, NULL);
        if (status == -1) {
            printf("signal unmasking failed\n");
        }
		
		free(send_buf);
		free(recv_buf);
		
		close(client_fd);
		syslog(LOG_DEBUG, "Closed connection from %s\n", ip_addr);
		
	}
	
    close(socket_num);
	close(file_fd);
	closelog();
	
	return 0;
}
