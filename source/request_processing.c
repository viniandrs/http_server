#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "../include/headers.h"
#include "../include/request_processing.h"
#include "../include/path.h"
#include "../include/lists.h"
#include "../include/utils.h"

char *get_header(char *resource, FieldNode *field_list, int *status) {
    struct stat st;
    ValueNode *credentials;
    FieldNode *header_list;

    FieldNode *auth = get_field(field_list, "Authorization");
    if (auth) credentials = auth->values;
    else credentials = NULL;

    printf("Fetching the resource recursively...\n");
    *status = fetchr(resource, credentials);
    printf("\n");
    
    // if the resource is a directory, check for index.html or welcome.html
    char *resource_abs_path = get_resource_abs_path(resource);
    stat(resource_abs_path, &st);
    if (*status == 200 && S_ISDIR(st.st_mode)) {
        if (dir_has_file(resource_abs_path, "index.html")) {
            strcat(resource_abs_path, "/index.html");
            stat(resource_abs_path, &st);
        } else {
            strcat(resource_abs_path, "/welcome.html");
            stat(resource_abs_path, &st);
        }   
    }

    switch (*status) {
        case 200:
            header_list = headers_200_ok(resource_abs_path, &st);
            break;
        case 201:
            header_list = headers_201_created();
            break;
        case 401:
            header_list = headers_401_unauthorized(resource, credentials);
            break;
        case 403:
            header_list = headers_403_forbidden(&st);
            break;
        case 404:
            header_list = headers_404_not_found(&st);
            break;
        case 500:
            header_list = headers_500_internal_server_error(&st);
            break;
        default:
            header_list = headers_500_internal_server_error(&st);
    }
    char *header = list_to_headers(header_list);
    free(resource_abs_path);
    return header;
}

int send_error_file(int status, int fd) {
    if (status == 401) return 0;
    
    struct stat st;
    size_t content_length;
    FieldNode *header_list = NULL;
    char *error_message_path;

    // Get the error file based on the status
    switch (status) {
        // case 401:
        //     error_message_path = "/status_pages/unauthorized.html";
        //     break;
        case 403:
            error_message_path = "/status_pages/forbidden.html";
            break;
        case 404:
            error_message_path = "/status_pages/not_found.html";
            break;
        case 500:
            error_message_path = "/status_pages/internal_server_error.html";
            break;
        default:
            error_message_path = "/status_pages/internal_server_error.html";
            break;
    }
    char *error_message_abs_path = get_resource_abs_path(error_message_path);
    if(stat(error_message_abs_path, &st) != 0) {
        printf("Stat error for file %s: %s\n", error_message_abs_path, strerror(errno));
        free(error_message_abs_path);
        return 500;
    }
    content_length = st.st_size;
    
    FILE *error_file = fopen(error_message_abs_path, "r");
    if (error_file == NULL) {
        printf("Error while opening error file: %s\n", strerror(errno));
        free(error_message_abs_path);
        return errno;
    }

    char buffer[content_length];
    int n_bytes = fread(buffer, 1, content_length, error_file);
    fclose(error_file);

    if(write_buffer(buffer, n_bytes, fd)) {
        printf("Error while writing error file: %s\n", strerror(errno));
        free(error_message_abs_path);
        return errno;
    }

    free(error_message_abs_path);
    return 0;
}

#define BUFFER_SIZE 32768
int send_file(char *resource, int fd) {
    struct stat st;
    char buffer[BUFFER_SIZE]="";
    FieldNode *header_list = NULL;
    size_t bytes_read = 0;

    char *resource_abs_path = get_resource_abs_path(resource);
    stat(resource_abs_path, &st);

    // if the resource is a directory, fetch first for index.html
    if (S_ISDIR(st.st_mode)) {
        if (dir_has_file(resource_abs_path, "index.html")) {
            strcat(resource_abs_path, "/index.html");
            stat(resource_abs_path, &st);
        } else {
            strcat(resource_abs_path, "/welcome.html");
            stat(resource_abs_path, &st);
        }
    }
    printf("Sending file %s\n", resource_abs_path);

    // if the content length is smaller than the buffer size, read the whole file
    if (st.st_size < BUFFER_SIZE) {
        printf("Content length is smaller than buffer size\n");
        FILE *resource_file = fopen(resource_abs_path, "r");
        if (!resource_file) {
            printf("Error while opening resource file: %s\n", strerror(errno));
            return errno;
        }
        printf("reading %ld bytes\n", st.st_size);
        bytes_read = fread(buffer, 1, st.st_size, resource_file);
        printf("buffer size: %ld\n", bytes_read);
        fclose(resource_file);

        if(write_buffer(buffer, bytes_read, fd)) {
            printf("Error while writing resource file: %s\n", strerror(errno));
            return errno;
        }
    } else {
        // if the content length is bigger than the buffer size, read the file in chunks
        printf("Content length is bigger than buffer size. Sending the file in chunks...\n");
        FILE *resource_file = fopen(resource_abs_path, "r");
        if (resource_file == NULL) {
            printf("Error while opening resource file: %s\n", strerror(errno));
            return errno;
        }

        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, resource_file)) > 0) {
            if(write_buffer(buffer, bytes_read, fd)) {
                printf("Error while writing resource file: %s\n", strerror(errno));
                return errno;
            }
            buffer[0] = '\0';
        }
        fclose(resource_file);
    }

    return 0;
}

