#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>

struct stat;

char *header_200_ok(char *time, struct stat *st, const char *content_type);

char *header_201_created();

#endif