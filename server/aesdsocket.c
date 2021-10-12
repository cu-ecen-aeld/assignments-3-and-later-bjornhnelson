#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#define PORT_NUM "9000"
#define MAX_BACKLOG 10
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"
#define MAX_BUF 100

// global variables
int socket_num; // fd for socket
int client_fd; // fd for most recent thread connection
int file_fd; // fd for output file

struct sockaddr_in client_addr; // needed for IP address
bool run_flag = true; // flag for main loop
pthread_mutex_t mutex; // used for synchronization

sigset_t cur_set; // signal masking
sigset_t prev_set; // signal masking

struct thread_data { // node structure for linked list
    pthread_t thread_id;
    int connection_fd;
    bool complete_flag;
};

struct list_data { // linked list data structure
    struct thread_data info;
    SLIST_ENTRY(list_data) entries;
};

void handle_signals(int sig_num) {
    if ((sig_num == SIGINT) || (sig_num == SIGTERM)) {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        printf("** Entering signal handler\n");

        if (shutdown(socket_num, SHUT_RDWR) == -1) {
            perror("shutdown");
        }

        run_flag = false;
    }
}

static inline void timespec_add(struct timespec* result, const struct timespec* ts_1, const struct timespec* ts_2) {
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if (result->tv_nsec > 1000000000L) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec++;
    }
}

void timer_thread() {
    time_t rawtime;
    struct tm* info;
    char* buf = malloc(sizeof(char) * MAX_BUF);
    if (buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    time(&rawtime);
    info = localtime(&rawtime);

    // build the string with timestamp
    int num_bytes = strftime(buf, MAX_BUF, "timestamp: %Y %b %d - %H:%M:%S\n", info);

    if (num_bytes == 0) {
        perror("strftime");
        free(buf);
    }

    if (pthread_mutex_lock(&mutex) != 0) {
        perror("mutex lock error");
        free(buf);
    }

    int bytes_written = write(file_fd, buf, num_bytes);
    if (bytes_written != num_bytes) {
        perror("writing timestamp error");
        free(buf);
    }

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror("mutex unlock error");
        free(buf);
    }

    free(buf);
}

/* Activities per thread
    1. receive data from client
    2. write data to output file
    3. read data from output file
    4. send data back to client */
void* thread_function(void* thread_data) {
	struct thread_data* thread_info = (struct thread_data*) thread_data;
    int status;
    int num_bytes;

    char buf[MAX_BUF]; // initialize static buffer
    memset(buf, '\0', MAX_BUF); // clear buf

	char* ip_addr = inet_ntoa(client_addr.sin_addr);
	syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_addr);

    // receive buffer setup
    int recv_buf_pos = 0;
    long recv_buf_size = MAX_BUF;
    char* recv_buf = malloc(recv_buf_size * sizeof(char));
    if (recv_buf == NULL) {
        perror("malloc failure");
        exit(EXIT_FAILURE);
    }

    // mask signals
    status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
    if (status == -1) {
        printf("signal masking failed\n");
        return NULL;
    }

    // receive data from client
    while (1) {
        // save up to 100 bytes into buf
        num_bytes = recv(thread_info->connection_fd, buf, MAX_BUF, 0);

        // exit loop on error
        if (num_bytes == -1) {
            perror("recv");
            return NULL;
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
        return NULL;
    }

    // write new bytes to file
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("mutex lock error");
        return NULL;
    }

    num_bytes = write(file_fd, recv_buf, recv_buf_pos);
    if (num_bytes == -1 || num_bytes != recv_buf_pos) {
        perror("write");
        return NULL;
    }

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror("mutex unlock error");
        return NULL;
    }

    // calculate total number of bytes in file
    status = lseek(file_fd, 0, SEEK_END);
    if (status == -1) {
        perror("lseek");
        return NULL;
    }
    int file_size = status;

    // reset pointer to beginning of file
    status = lseek(file_fd, 0, SEEK_SET);
    if (status == -1) {
        perror("lseek");
        return NULL;
    }

    // send buffer setup
    long send_buf_size = file_size;
    char* send_buf = malloc(send_buf_size * sizeof(char));
    if (send_buf == NULL) {
        perror("malloc failure");
        exit(EXIT_FAILURE);
    }

    // read the ENTIRE file
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("mutex lock error");
        return NULL;
    }

    num_bytes = read(file_fd, send_buf, send_buf_size);
    if (num_bytes == -1 || num_bytes != send_buf_size) {
        perror("read");
        return NULL;
    }

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror("mutex unlock error");
        return NULL;
    }

    // mask signals
    status = sigprocmask(SIG_BLOCK, &cur_set, &prev_set);
    if (status == -1) {
        printf("signal masking failed\n");
        return NULL;
    }

    // send data to the client
    num_bytes = send(thread_info->connection_fd, send_buf, send_buf_size, 0);
	if (num_bytes == -1 || num_bytes != send_buf_size) {
		perror("send");
		return NULL;
	}

    // unmask signals
    status = sigprocmask(SIG_UNBLOCK, &prev_set, NULL);
    if (status == -1) {
        printf("signal unmasking failed\n");
        return NULL;
    }

    // free buffers
    free(recv_buf);
    free(send_buf);

    close(client_fd);
    syslog(LOG_DEBUG, "Closed connection from %s\n", ip_addr);	   
    thread_info->complete_flag = true;

    return NULL;
}

void program_cleanup() {
    printf("** Program cleanup\n");

    pthread_mutex_destroy(&mutex);
    closelog();
    close(socket_num);
    close(file_fd);
    close(client_fd);

    if (remove(OUTPUT_FILE_PATH) == -1) {
       perror("remove");
    }
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv) {
    int status;
    bool daemon_flag = false;
    pid_t pid = 0;

    printf("** Starting server **\n");

    // initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // initialize linked list
    struct list_data* list_ptr = NULL;
    SLIST_HEAD(slisthead, list_data) head;
    SLIST_INIT(&head);

    openlog(NULL, 0, LOG_USER);

    // set up signal handlers
    if (signal(SIGINT, handle_signals) == SIG_ERR) {
        printf("SIGINT setup error\n");
        return -1;
    }
    if (signal(SIGTERM, handle_signals) == SIG_ERR) {
        printf("SIGTERM setup error\n");
        return -1;
    }

    // setup signal masking (happens during recv and send)
    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGINT);
    sigaddset(&cur_set, SIGTERM);

    // process command line arguments
    if (argc == 2 && strncmp("-d", argv[1], 3) == 0) {
        daemon_flag = true;
    }

    // setup addrinfo data structure
    struct addrinfo hints;
    struct addrinfo* server_info;
    socklen_t client_addr_len = sizeof(struct sockaddr);

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
    if (daemon_flag == true) {
        pid = fork();

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
         	
	// set up timer in child process if daemon is running
    timer_t timer_id;
    struct sigevent sev;
    int clock_id = CLOCK_MONOTONIC;
    struct timespec start_time;

    if ((daemon_flag == false) || (pid == 0)) {
        memset(&sev, 0, sizeof(struct sigevent));
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = timer_thread;

        // create timer
        if (timer_create(clock_id, &sev, &timer_id) != 0) {
			perror("timer_create");
            program_cleanup();
        }

        // get current time for start
        if (clock_gettime(clock_id, &start_time) != 0) {
            perror("clock_gettime");
            program_cleanup();
        }

        // define interval values
        struct itimerspec itimerspec;
        itimerspec.it_interval.tv_sec = 10;
        itimerspec.it_interval.tv_nsec = 1000000;
        timespec_add(&itimerspec.it_value, &start_time, &itimerspec.it_interval);
        if (timer_settime(timer_id, TIMER_ABSTIME, &itimerspec, NULL) != 0) {
            perror("timer_set_time");
            program_cleanup();
        }
    }

	// main loop for creating threads
    while (run_flag == true) {
        
        // accept connection from client
        client_fd = accept(socket_num, (struct sockaddr*) &client_addr, &client_addr_len);

        if (run_flag != false) {

            // check for errors on accept call
            if (client_fd == -1) {
                perror("accept");
                return -1;
            }

            // add node to linked list
            list_ptr = malloc(sizeof(struct list_data));
            if (list_ptr == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            (list_ptr->info).connection_fd = client_fd;
            (list_ptr->info).complete_flag = false;
            SLIST_INSERT_HEAD(&head, list_ptr, entries);

            // create new thread with fd from accept
            status = pthread_create(&((list_ptr->info).thread_id), NULL, thread_function, (void*) &(list_ptr->info));
            if (status == -1) {
                perror("pthread_create");
                return -1;
            }

            // join each thread in list with flag marked as completed
            SLIST_FOREACH(list_ptr, &head, entries) {
                if ((list_ptr->info).complete_flag == true) {
                    pthread_join((list_ptr->info).thread_id, NULL);
                }
            }
        }

    }
	
	// after exiting main loop, do cleanup activities

    // join all threads
    SLIST_FOREACH(list_ptr, &head, entries) {
        pthread_join((list_ptr->info).thread_id, NULL);
    }

    // free all nodes of linked list
    while (!SLIST_EMPTY(&head)) {
        list_ptr = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        free(list_ptr);
    }

    program_cleanup();
    return 0;
}
