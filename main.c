#include "include/server.h"
#include "include/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *webspace_path, *log_file;

int main(int argc, char *argv[]) {
    // Parsing arguments
    if (argc != 5) {
        printf("Usage: %s <port> <webspace_path> <max_threads> <log_file>\n", argv[0]);
        return 1;
    }
    
    const int port = atoi(argv[1]); 
    char *unf_webspace_path = argv[2];
    const int max_threads = atoi(argv[3]);
    log_file = argv[4];  

    webspace_path = format_webspace_path(unf_webspace_path);

    // Clearing log file
    FILE *pFile;
    if(!(pFile = fopen(log_file, "w"))) {
        printf("Error while opening output_file file: %s\n", strerror(errno));
        exit(errno);
    }
    fclose(pFile);

    // starting server
    printf("Starting server on port %d with %d threads\n", port, max_threads);
    start_server(port, max_threads);

    return 0;
}