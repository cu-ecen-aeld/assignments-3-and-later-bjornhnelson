// Author: Bjorn Nelson

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>

#define EXIT_FAILURE 1

int main(int argc, char* argv[]) {
    char path[100];
    char* writestr;
    char* directory_name;
    //char* file_name;
    //char cur_directory[100];
    FILE* curFile;
    struct stat st;
    int status;

    openlog(NULL, LOG_PERROR, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "ERROR: invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }

    strcpy(path, argv[1]);
    //printf("Path: %s\n", path);
    directory_name = dirname(argv[1]);

    if (stat(directory_name, &st) != 0) {
        syslog(LOG_ERR, "ERROR: path does not exist\n");
        exit(EXIT_FAILURE);
    }

    
    printf("Path: %s\n", path);
    curFile = fopen(path, "w");
    if (curFile == NULL) {
        syslog(LOG_ERR, "ERROR: file could not be opened\n");
        exit(EXIT_FAILURE);
    }

    writestr = argv[2];
    status = fwrite(writestr, sizeof(char), strlen(writestr), curFile);
    if (status == 0) {
        syslog(LOG_ERR, "ERROR: file could not be written to\n");
	exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG, "Writing %s to %s\n", writestr, path);

    status = fclose(curFile);
    if (status != 0) {
        syslog(LOG_ERR, "ERROR: file could not be closed\n");
	exit(EXIT_FAILURE);
    }

    closelog();
    return 0;

}

