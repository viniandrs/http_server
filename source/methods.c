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

char *head(struct stat *st, int code) {
    // getting the code status
    char *status = (code == 200) ? "OK" : (code == 403) ? "Forbidden" : (code == 404) ? "Not Found" 
                 : (code == 501) ? "Not implemented" : (code == 401) ? "Authorization Required" : "Internal Server Error";

    // getting the current date
    struct timeval tv;
    struct tm *tm_info;
    char time[30];

    gettimeofday(&tv, NULL);    
    tm_info = localtime(&tv.tv_sec); // Convert it to UTC time
    strftime(time, sizeof(time), "%a, %d %b %Y %H:%M:%S BRT", tm_info); // Format the time according to the HTTP date format

    // getting the connection type
    char *connection = (code == 200) ? "keep-alive" : "close";

    // content type
    char *content_type = "text/html; charset=UTF-8";

    // fields now depend on the status code
    char *header;
    if (code == 200) {
        // last modified date
        char last_modified_time[30];
        struct tm *mod_time_info = localtime(&(st->st_mtime));
        strftime(last_modified_time, sizeof(last_modified_time), "%a, %d %b %Y %H:%M:%S BRT", mod_time_info);

        // writing the header
        size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: %s\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", code, status, time, connection, last_modified_time, st->st_size, content_type);
        header = (char *)calloc(header_len + 1, sizeof(char));
        sprintf(header,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Connection: %s\n\
Last-Modified: %s\n\
Content-Length: %jd\n\
Content-Type: %s\n\
", code, status, time, connection, last_modified_time, st->st_size, content_type);
    } 
    else if (code == 401) {
        // writing the header
        size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
WWW-Authenticate:  Basic realm=\"Espaço Protegido 1\"\n\
Transfer-Encoding: chunked\n\
Content-Type: text/html\n\
", code, status, time);
        header = (char *)calloc(header_len + 1, sizeof(char));
        sprintf(header,
"HTTP/1.1 %d %s\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
WWW-Authenticate:  Basic realm=\"Espaço Protegido 1\"\n\
Transfer-Encoding: chunked\n\
Content-Type: text/html\n\
", code, status, time);
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

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, "index.html") == 0) {
                new_path_len = snprintf(NULL, 0, "%s/index.html", path);
                path = (char *)realloc(path, new_path_len + 1);
                sprintf(path, "%s/index.html", path);
                file_found = 1;
                break;
            }
            else if (strcmp(entry->d_name, "welcome.html") == 0) {
                new_path_len = snprintf(NULL, 0, "%s/welcome.html", path);
                path = (char *)realloc(path, new_path_len + 1);
                sprintf(path, "%s/welcome.html", path);
                file_found = 1;
                break;
            }
        }
        closedir(dir);
        if (!file_found) {
            printf("404 Not Found: No index.html or welcome.html found in the directory\n");
            exit(1);
        }

        if (stat(path, st) != 0) {
            printf("Error while accessing the file: %s\n", strerror(errno));
            exit(errno);
        }
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

int trace(const char *web_path, const char *resource, const char *message) {
    // Combining path and resource to get the complete path
    char path[1024];  
    sprintf(path, "%s/%s", web_path, resource);

    struct stat *st;         // structure to store file information
    if (stat(path, st) != 0) {
        if (errno == ENOENT) {
            printf("Not found %s\n", path);
            return 404; // Not found
        } else {
            perror("Error while accessing the file");
            return 500;  // Internal server error
        }
    }

    char *header = head(st, 200);
    char *response = (char *)malloc(2048);
    sprintf(response, "%s\n%s", header, message);
    write(STDOUT_FILENO, response, strlen(response));  // Writing the response in STDOUT
    return 200;
}
