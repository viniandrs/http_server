#ifndef UTILS_H
#define UTILS_H

#include "lists.h"
#include <sys/stat.h>

/* Fetch recursively each directory in the path until the final file. */
int fetchr(char* webspace_path, char *resource, struct stat *st, ValueNode *credentials);

int fetch(const char *path, struct stat *_st);

/* Formats the webspace path. */
char *format_webspace_path(char *webspace_path);

/* Formats the resource path. */
char *format_resource_path(char *resource);

int send_page(char *body, int socket_fd);

int log_request(char *request, char *header);

#endif