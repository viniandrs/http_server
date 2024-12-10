#ifndef UTILS_H
#define UTILS_H

#include "lists.h"
#include <sys/stat.h>

/* Fetch recursively each directory in the path until the final file. */
int fetchr(char *resource, struct stat *st, ValueNode *credentials);

int fetch(const char *path, struct stat *_st);

/* Format the webspace path and resolve the absolute path. */
char *format_webspace_path(char *webspace_path);

/* Format the resource path */
char *format_resource_path(char *resource);

/* Format the resource path and resolve the absolute path */
char *format_and_resolve_resource_path(char *resource);

char *get_absolute_path(char *resource);

int send_page(char *body, int socket_fd);

int log_request(char *request, char *header);

char *htaccess_from_form(char *form_path);

const char *get_content_type(const char *filepath);

#endif