#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>
#include <sys/stat.h>

#include "../include/lists.h"

FieldNode *headers_200_ok(char *filepath, struct stat *st);
FieldNode *headers_201_created();
FieldNode *headers_401_unauthorized(char *filepath, ValueNode *credentials);
FieldNode *headers_403_forbidden(struct stat *st);
FieldNode *headers_404_not_found(struct stat *st);
FieldNode *headers_500_internal_server_error(struct stat *st);
FieldNode *headers_501_not_implemented(struct stat *st);
FieldNode *headers_options(char *filepath, struct stat *st);

char *list_to_headers(FieldNode *header_list);

#endif