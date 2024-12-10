#ifndef AUTH_H
#define AUTH_H

#include "lists.h"

char *base64_decode(const char *input);

char *header_401_unauthorized(char *resource, ValueNode* credentials);

/* Return the realm from the first protected directory which the credentials
from the list do not match */
char *get_realm(char *resource, ValueNode* credentials_list);

/* Get the credentials from the "Authentication" fields in a given field list */
ValueNode* get_credentials(FieldNode *field_list);

/* Check if any of the credentials required by a .htaccess file is in a credentials list */
int check_credentials(char *htaccess_path, ValueNode* credentials);

/* Update the password on a given realm based on values from the form page. */
int update_passwords(char *htaccess_path, char **form_values);

#endif