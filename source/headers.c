#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>

#include "../include/headers.h"

char *header_200_ok(char *time, struct stat *st, const char *content_type) {
    // last modified date
    char last_modified_time[30];
    struct tm *mod_time_info = localtime(&(st->st_mtime));
    strftime(last_modified_time, sizeof(last_modified_time), "%a, %d %b %Y %H:%M:%S BRT", mod_time_info);

    // writing the header
    size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 200 OK\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: keep-alive\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", time, last_modified_time, st->st_size, content_type);
        char *header = calloc(header_len + 1, sizeof(char));
        sprintf(header,
"HTTP/1.1 200 OK\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: keep-alive\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", time, last_modified_time, st->st_size, content_type);

    return header;
}

char *header_201_created() {
    size_t content_len = strlen("foo");

    // writing the header
    size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 201 Created\n\
Content-Type: application/json\n\
Content-Length: %jd\n\
", content_len);
        char *header = calloc(header_len + 1, sizeof(char));
        sprintf(header,
"HTTP/1.1 201 Created\n\
Content-Type: application/json\n\
Content-Length: %jd\n\
", content_len);

    return header;
}