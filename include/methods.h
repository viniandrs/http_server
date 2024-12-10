#ifndef METHODS_H
#define METHODS_H

#include <sys/stat.h>
#include <stddef.h>

#include "lists.h"

struct stat;

// request processing methods
char *get(char* path, struct stat *st);

char *head(struct stat *st, int code, const char *content_type);

char *options(struct stat *st, int code);

char *error_handler(int code);

#endif
