#ifndef POST_H
#define POST_H

#include "lists.h"

char **parse_request_body(const char *request_body);

void free_data_array(char **data_array);

#endif