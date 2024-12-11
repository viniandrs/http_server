#ifndef UTILS_H
#define UTILS_H

#include "lists.h"
#include <sys/stat.h>

/* Fetch recursively each directory in the path until the final file. Return the status. */
int fetchr(char *resource, ValueNode *credentials);

/* Fetch directly a single file. Return the status. */
int fetch(const char *path);

/* Fetch a single file in a directory. Return the status. */
int fetch_for_file_in_dir(char *dir_abs_path, char *filename);

/* Check if a directory has a file. Return 1 if it has, 0 otherwise. */
int dir_has_file(char *dir_abs_path, char *filename);

int write_buffer(char *buffer, int n_bytes, int socket_fd);

int log_request(char *request, char *header);

char *htaccess_from_form(char *form_path);

#endif