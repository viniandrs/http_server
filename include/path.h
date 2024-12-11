#ifndef PATH_H
#define PATH_H

#include "lists.h"

char *format_webspace_path(char *unf_webspace_path);

/* Return the absolute path of a resource in the webspace */
char *get_resource_abs_path(char *resource_path);

/* Return the resolved path of a resource without the webspace path */
char *resolve_resource_path(char *resource_path);

int is_in_webspace(char *resource_path);

#endif