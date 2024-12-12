#ifndef PROCESS_REQUEST_H
#define PROCESS_REQUEST_H

#include "../include/lists.h"

/* Get the response header and status. */
char *get_header(char *filepath, FieldNode *field_list, int *status);

/* Send the file to the client. */
int send_file(char *filepath, int fd);

/* Send the error file to the client. */
int send_error_file(int status, int fd);

/* Extract values from a POST request body. */
char **parse_request_body(const char *body);

void free_data_array(char **data_array);

#endif