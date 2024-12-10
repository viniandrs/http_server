#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <dirent.h>

#include "../include/methods.h"
#include "../include/lists.h"
#include "../include/auth.h"
#include "../include/utils.h"
#include "../include/headers.h"

extern char *webspace_path;

char *error_handler(int code) {
    char *body;
    if (code == 404) {
        body =
        "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
  <title>404 Not Found</title>\n\
</head>\n\
<body>\n\
  <h1>404 Not Found</h1>\n\
  <p>The resource you are looking for might have been removed, had its name changed, or is temporarily unavailable.</p>\n\
</body>\n\
</html>\n";
    } else if (code == 403) {
        body =
        "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
  <title>403 Forbidden</title>\n\
</head>\n\
<body>\n\
  <h1>403 Forbidden</h1>\n\
  <p>You do not have permission to access this resource.</p>\n\
</body>\n\
</html>\n";
    } else if (code == 500) {
        body =
        "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
  <title>500 Internal Server Error</title>\n\
</head>\n\
<body>\n\
  <h1>500 Internal Server Error</h1>\n\
  <p>The server encountered an unexpected condition that prevented it from fulfilling the request.</p>\n\
</body>\n\
</html>\n";
    } else {
        body =
        "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
  <title>501 Not implemented</title>\n\
</head>\n\
<body>\n\
  <h1>501 Not implemented</h1>\n\
  <p>The server either doesn't recognize the requested method, or it lacks the ability to fulfill the request.</p>\n\
</body>\n\
</html>\n";
    }
    return body;
}

char *head(struct stat *st, int code, const char *content_type) {
    // getting the code status
    char *status = (code == 403) ? "Forbidden" : (code == 404) ? "Not Found" : (code == 501) ? "Not implemented" : "Internal Server Error";

    // getting the current date
    struct timeval tv;
    struct tm *tm_info;
    char time[30];

    gettimeofday(&tv, NULL);    
    tm_info = localtime(&tv.tv_sec); // Convert it to UTC time
    strftime(time, sizeof(time), "%a, %d %b %Y %H:%M:%S BRT", tm_info); // Format the time according to the HTTP date format

    // getting the connection type
    char *connection = (code == 200) ? "keep-alive" : "close";

    // fields now depend on the status code
    char *header;
    if (code == 200) {
        header = header_200_ok(time, st, content_type);
    } else if (code == 201 ) {
        header = header_201_created(time, st, content_type);
    } else {
        // writing the header
        size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", code, status, time, st->st_size, content_type);
        header = (char *)calloc(header_len + 1, sizeof(char));
        sprintf(header,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", code, status, time, st->st_size, content_type);
    }
    return header;
}

// return the body of a resource in the webspace
char* get(char* path, struct stat *st) {
    printf("Opening file: %s\n", path);

    // if the resource is a dir, looks for either index.html or welcome.html 
    if (S_ISDIR(st->st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *entry;
        size_t new_path_len;
        int file_found = 0;

        char *dir_path = strdup(path);
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, "index.html") != 0 && 
                strcmp(entry->d_name, "welcome.html") != 0) {
                continue;
            }
            path = (char *)realloc(path, strlen(path) + sizeof("/welcome.html") + 1); // worst case scenario
            strcat(path, "/");
            strcat(path, entry->d_name);

            if(strcmp(entry->d_name, "index.html") == 0) {
                // found index.html
                if(fetch(path, st) == 200) break;

                //index.html is not accessible, return welcome.html
                sprintf(path, "%s/welcome.html", dir_path);
                break;                        
            }

            // found welcome.html
            if(fetch(path, st) == 200) break;

            //welcome.html is not accessible, return index.html
            sprintf(path, "%s/index.html", dir_path);
            break;             
        }
        free(dir_path);
        closedir(dir);

        if (stat(path, st) != 0) {
            printf("Error while accessing the file: %s\n", strerror(errno));
            exit(errno);
        }

        printf("File was directory so GET will return %s\n", path);
    }
    
    printf("File size of %s: %jd\n", path, st->st_size);
    FILE *pFile;
    if(!(pFile = fopen(path, "r"))) {
        printf("Error while opening the file: %s\n", strerror(errno));
        return NULL;
    }
    char *body = (char *)calloc(st->st_size + 1, sizeof(char));
    fread(body, st->st_size+1, 1, pFile);
    fclose(pFile);
    return body;
}

char *options(struct stat *st, int code) {
    // getting the code status
    char *status = (code == 200) ? "OK" : (code == 403) ? "Forbidden" : (code == 404) ? "Not Found" : "Internal Server Error";

    // getting the current date
    struct timeval tv;
    struct tm *tm_info;
    char time[30];

    gettimeofday(&tv, NULL);    
    tm_info = localtime(&tv.tv_sec); // Convert it to UTC time
    strftime(time, sizeof(time), "%a, %d %b %Y %H:%M:%S BRT", tm_info); // Format the time according to the HTTP date format

    // getting the connection type
    char *connection = (code == 200) ? "keep-alive" : "close";

    // last modified date
    char last_modified_time[30];
    struct tm *mod_time_info = localtime(&(st->st_mtime));
    strftime(last_modified_time, sizeof(last_modified_time), "%a, %d %b %Y %H:%M:%S BRT", mod_time_info);

    // content type
    char *content_type;
    if (S_ISDIR(st->st_mode)) {
        content_type = "text/html; charset=UTF-8";
    } else {
        content_type = "text/html; charset=UTF-8";
    }

    // writing the header
    size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 %d %s\n\
Allow: GET, HEAD, OPTIONS, TRACE\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: %s\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
    ", code, status, time, connection, last_modified_time, st->st_size, content_type);
    char *header = (char *)calloc(header_len + 1, sizeof(char));
    sprintf(header,
"HTTP/1.1 %d %s\n\
Allow: GET, HEAD, OPTIONS, TRACE\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: %s\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
    ", code, status, time, connection, last_modified_time, st->st_size, content_type);
    return header;
}
