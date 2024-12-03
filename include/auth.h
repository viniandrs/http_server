#ifndef AUTH_H
#define AUTH_H

#include "lists.h"

char *base64_decode(const char *input);

char *auth_header(char *webspace_path, char *resource, ValueNode* credentials);

/* Return the realm from the first protected directory which the credentials
from the list do not match */
char *get_realm(char *webspace_path, char *resource, ValueNode* credentials_list);

/* Get the credentials from the "Authentication" fields in a given field list */
ValueNode* get_credentials(FieldNode *field_list);

/* Check if any of the credentials required by a .htaccess file is in a credentials list */
int check_credentials(char *htaccess_path, ValueNode* credentials);

#endif