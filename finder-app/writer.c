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
    FILE* fp;
    struct stat st;
    int status;

    openlog(NULL, LOG_PERROR, LOG_USER);

    // check command line arguments
    if (argc != 3) {
        syslog(LOG_ERR, "ERROR: invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }

    // get directory path without filename appended
    strcpy(path, argv[1]);
    directory_name = dirname(argv[1]);

    // verify that directory path exists
    if (stat(directory_name, &st) != 0) {
        syslog(LOG_ERR, "ERROR: path does not exist\n");
        exit(EXIT_FAILURE);
    }

    // open the file
    fp = fopen(path, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "ERROR: file could not be opened\n");
        exit(EXIT_FAILURE);
    }

    // write to the file
    writestr = argv[2];
    status = fwrite(writestr, sizeof(char), strlen(writestr), fp);
    if (status == 0) {
        syslog(LOG_ERR, "ERROR: file could not be written to\n");
	exit(EXIT_FAILURE);
    }

    // debug info to syslog
    syslog(LOG_DEBUG, "Writing %s to %s\n", writestr, path);

    // close the file
    status = fclose(fp);
    if (status != 0) {
        syslog(LOG_ERR, "ERROR: file could not be closed\n");
	exit(EXIT_FAILURE);
    }

    closelog();
    return 0;

}

