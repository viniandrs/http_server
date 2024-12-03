#ifndef METHODS_H
#define METHODS_H

#include <sys/stat.h>
#include <stddef.h>

#include "lists.h"

struct stat;

// request processing methods
char *get(char* path, struct stat *st);

char *head(struct stat *st, int code);

char *options(struct stat *st, int code);

int trace(const char *web_path, const char *resource, const char *message);

char *error_handler(int code);

char *auth();

#endif
