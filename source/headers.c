#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>

#include "../include/path.h"
#include "../include/headers.h"
#include "../include/auth.h"
#include "../include/lists.h"

ValueNode *get_current_time() {
    time_t now;
    time(&now);
    struct tm *gmt = gmtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S %Z", gmt);

    return append_value(NULL, time_str);
}

ValueNode *get_server() {
    return append_value(NULL, "Vinicius Andreossi's simple HTTP server v0.1");
}

ValueNode *get_connection() {
    return append_value(NULL, "close");
}

ValueNode *get_last_modified(struct stat *st) {
    char time_str[64];
    struct tm *gmt = gmtime(&st->st_mtime);
    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S %Z", gmt);

    return append_value(NULL, time_str);
}

ValueNode *get_content_length(struct stat *st) {
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "%ld", st->st_size);

    return append_value(NULL, content_length);
}

ValueNode *get_content_type(char *resource_abs_path) {    
    // File extension map
    struct {
        const char *extension;
        const char *content_type;
    } mime_map[] = {
        { "gif", "image/gif" },
        { "png", "image/png" },
        { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },
        { "tif", "image/tiff" },
        { "tiff", "image/tiff" },
        { "pdf", "application/pdf" },
        { "zip", "application/zip" },
        { "html", "text/html;" },
        { "txt", "text/plain; charset=utf-8" },
        { "odt", "application/vnd.oasis.opendocument.text" },
        { NULL, NULL }  // Sentinel value
    };
    
    // Get the file extension
    const char *extension = strrchr(resource_abs_path, '.');
    if (extension == NULL) {
        return append_value(NULL, "application/octet-stream");
    }
    extension++;

    // Find the content type
    for (int i = 0; mime_map[i].extension != NULL; i++) {
        if (strcmp(extension, mime_map[i].extension) == 0) {
            return append_value(NULL, mime_map[i].content_type);
        }
    }
    return append_value(NULL, "application/octet-stream");

}

FieldNode *headers_200_ok(char *resource_abs_path, struct stat *st) {
    FieldNode *header_list = NULL;

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 200 OK", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the Last-Modified field
    header_list = append_field(header_list, "Last-Modified", get_last_modified(st));

    // Add the Content-Length field
    header_list = append_field(header_list, "Content-Length", get_content_length(st));

    // Add the Content-Type field
    header_list = append_field(header_list, "Content-Type", get_content_type(resource_abs_path));

    return header_list;
}

FieldNode *headers_201_created() {
    FieldNode *header_list = NULL;

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 201 Created", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    return header_list;
}

FieldNode *headers_401_unauthorized(char *resource, ValueNode *credentials) {
    FieldNode *header_list = NULL;

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 401 Unauthorized", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the WWW-Authenticate field
    char auth[128] = "Basic realm=";
    const char *realm = get_realm(resource, credentials);
    if (!realm) realm = "Restricted";

    strcat(auth, realm);
    header_list = append_field(header_list, "WWW-Authenticate", append_value(NULL, auth));

    return header_list;
}

FieldNode *headers_403_forbidden(struct stat *st) {
    char *error_message_path = "/status_pages/forbidden.html";
    FieldNode *header_list = NULL;
    char* error_message_abs_path = get_resource_abs_path(error_message_path);
    stat(error_message_abs_path, st);

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 403 Forbidden", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the Content-Type field
    header_list = append_field(header_list, "Content-Type", get_content_type(error_message_abs_path));

    // Add the Content-Length field
    header_list = append_field(header_list, "Content-Length", get_content_length(st));

    free(error_message_abs_path);
    return header_list;
}

FieldNode *headers_404_not_found(struct stat *st) {
    char *error_message_path = "/status_pages/not_found.html";
    FieldNode *header_list = NULL;
    char* error_message_abs_path = get_resource_abs_path(error_message_path);
    stat(error_message_abs_path, st);

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 404 Not Found", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the Content-Type field
    header_list = append_field(header_list, "Content-Type", get_content_type(error_message_abs_path));

    // Add the Content-Length field
    header_list = append_field(header_list, "Content-Length", get_content_length(st));

    free(error_message_abs_path);
    return header_list;
}

FieldNode *headers_500_internal_server_error(struct stat *st) {
    char *error_message_path = "/status_pages/internal_server_error.html";
    FieldNode *header_list = NULL;
    char* error_message_abs_path = get_resource_abs_path(error_message_path);
    stat(error_message_abs_path, st);

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 500 Internal Server Error", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the Content-Type field
    header_list = append_field(header_list, "Content-Type", get_content_type(error_message_abs_path));

    // Add the Content-Length field
    header_list = append_field(header_list, "Content-Length", get_content_length(st));

    free(error_message_abs_path);
    return header_list;
}

FieldNode *headers_501_not_implemented(struct stat *st) {
    char *error_message_path = "/status_pages/not_implemented.html";
    FieldNode *header_list = NULL;
    char* error_message_abs_path = get_resource_abs_path(error_message_path);
    stat(error_message_abs_path, st);

    // Add the status line
    header_list = append_field(header_list, "HTTP/1.1 501 Not Implemented", NULL);

    // Add the Date field
    header_list = append_field(header_list, "Date", get_current_time());

    // Add the Server field
    header_list = append_field(header_list, "Server", get_server());

    // Add the Connection field
    header_list = append_field(header_list, "Connection", get_connection());

    // Add the Content-Type field
    header_list = append_field(header_list, "Content-Type", get_content_type(error_message_abs_path));

    // Add the Content-Length field
    header_list = append_field(header_list, "Content-Length", get_content_length(st));

    free(error_message_abs_path);
    return header_list;
}

FieldNode *headers_options(char *filepath, struct stat *st) {
    FieldNode *field_list = headers_200_ok(filepath, st);
    ValueNode *methods = append_value(NULL, "GET, HEAD, POST, TRACE, OPTIONS");
    
    // Add the Allow field
    field_list = append_field(field_list, "Allow", methods);

    return field_list;
}

char *list_to_headers(FieldNode *header_list) {
    char *headers = (char *)calloc(1024, sizeof(char));
    FieldNode *current_field = header_list;
    ValueNode *current_value;

    while (current_field != NULL) {
        strcat(headers, current_field->field);
        strcat(headers, ": ");
        current_value = current_field->values;
        while (current_value != NULL) {
            strcat(headers, current_value->value);
            current_value = current_value->next;
        }
        strcat(headers, "\r\n");
        current_field = current_field->next;
    }
    strcat(headers, "\r\n");
    return headers;
}