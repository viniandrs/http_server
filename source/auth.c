#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <crypt.h>

#include "../include/auth.h"
#include "../include/base64.h"
#include "../include/path.h"
#include "../include/utils.h"

extern char *webspace_path;

int is_dir_protected(char *dir_abs_path) {
    char htaccess_path[1024]="";
    strcat(htaccess_path, dir_abs_path);
    strcat(htaccess_path, "/.htaccess");

    if (access(htaccess_path, F_OK) != -1) {
        return 1;
    }
    return 0;
}

char *get_field_from_htaccess(char *htaccess_path, char *field) {
    FILE *htaccess_file = fopen(htaccess_path, "r");
    if (!htaccess_file) {
        fprintf(stderr, "Error while opening .htaccess file: %s\n", strerror(errno));
        return NULL;
    }

    char line[1024];
    while (fgets(line, sizeof(line), htaccess_file)) {
        if (strncmp(line, field, strlen(field)) == 0) {
            // Found the field
            char *value = strchr(line, ' ');
            value[strlen(value) - 1] = '\0';
            fclose(htaccess_file);
            return strdup(value+1); // removing the space
        }
    }
    fclose(htaccess_file);
    printf("Field %s not found in .htaccess file located at %s\n", field, htaccess_path);
    return NULL;
}

char *get_userfile_abs_path(char *htacces_abs_path, char *AuthUserFile) {
    char *userfile_relative_path = calloc(1024, sizeof(char));
    strcpy(userfile_relative_path, dirname(htacces_abs_path));
    if (AuthUserFile[0] != '/') strcat(userfile_relative_path, "/");
    strcat(userfile_relative_path, AuthUserFile);

    char *userfile_abs_path = calloc(1024, sizeof(char));
    realpath(userfile_relative_path, userfile_abs_path);
    free(userfile_relative_path);
    return userfile_abs_path;
}

int parse_hash(char *hash, int *n, char *salt) {
    char *copy = strdup(hash);
    char *token = strtok_r(copy, "$", &copy);

    // Get n
    *n = atoi(token); // Parse the algorithm identifier as an integer

    // Get the salt
    token = strtok_r(NULL, "$", &copy);
    strncpy(salt, token, 128); // Limit salt length to 128
    salt[127] = '\0'; // Ensure null-termination

    return 1;
}

int credentials_match_userfile(char *user_file, ValueNode *credentials) {
    FILE *user_file_ptr = fopen(user_file, "r");
    if (user_file_ptr == NULL) {
        perror("Error while opening user file");
        return 0;
    }

    char *username, *password, *encoded_credentials, *decoded_credentials, *file_hash, *credentials_hash;
    int SHA_n;
    char user_line[1024], salt[128], crypt_prefix[256];

    // Iterate over the credentials
    while (credentials != NULL) {
        encoded_credentials = strdup(credentials->value + 7);
        decoded_credentials = base64_decode(encoded_credentials);
        printf("Decoded credentials: %s\n", decoded_credentials);
        password = strdup(strchr(decoded_credentials, ':') + 1);
        username = strtok_r(decoded_credentials, ":", &decoded_credentials);
        
        // Iterate over the lines in the user file
        while (fgets(user_line, sizeof(user_line), user_file_ptr)) {
            user_line[strlen(user_line) - 1] = '\0'; // Remove the newline character

            // If the username is found, get n and the salt from the hash in the file
            if (strncmp(user_line, username, strlen(username)) == 0) {
                file_hash = strchr(user_line, ':') + 1;
                if (!parse_hash(file_hash, &SHA_n, salt)) {
                    printf("Error while parsing hash\n");
                    fclose(user_file_ptr);
                    return 0;
                }

                // Compare the hashes
                snprintf(crypt_prefix, 256, "$%d$%s$", SHA_n, salt);
                credentials_hash = crypt(password, crypt_prefix);
                printf("Comparing hashes: %d\n", strncmp(file_hash, credentials_hash, strlen(file_hash)));
                if (strncmp(file_hash, credentials_hash, strlen(file_hash)) == 0) {
                    fclose(user_file_ptr);
                    return 1; // Credentials match
                }
            }
        }
        credentials = credentials->next;
        decoded_credentials = "";
    }
    printf("Credentials provided don't match\n");
    fclose(user_file_ptr);
    return 0;
}

char *change_userfile_line(char *line, char *new_password) {
    int SHA_n;
    char salt[128];
    char* line_copy = strdup(line);

    char *username = strtok_r(line_copy, ":", &line_copy);
    char *hash = strtok_r(NULL, ":", &line_copy);

    // create the new hash
    parse_hash(hash, &SHA_n, salt);
    char crypt_prefix[256];
    snprintf(crypt_prefix, 256, "$%d$%s$", SHA_n, salt);
    char *new_hash = crypt(new_password, crypt_prefix);

    // create the new line
    char *new_line = calloc(1024, sizeof(char));
    snprintf(new_line, 1024, "%s:%s\n", username, new_hash);

    return new_line;
}

int update_user_credentials(char *user_file_abs_path, char *username, char *password) {
    char **lines;
    size_t line_count = 0;
    char line[1024];
    FILE *user_file_ptr;

    // Counting lines in file
    if (!(user_file_ptr = fopen(user_file_abs_path, "r"))) {
        printf("Error while opening user file: %s\n", strerror(errno));
        return 0;
    }
    while (fgets(line, sizeof(line), user_file_ptr)) {
        line_count++;
    }
    fclose(user_file_ptr);

    user_file_ptr = fopen(user_file_abs_path, "r");
    lines = (char **)calloc(line_count, sizeof(char *));
    line_count = 0;
    while (fgets(line, sizeof(line), user_file_ptr)) {
        printf("Line: %s\n", line);
        lines[line_count] = strdup(line);
        line_count++;
    }
    fclose(user_file_ptr);

    // Update the password for the specified user
    char *username_line, *new_line, *temp;
    int user_found = 0;
    for (size_t i = 0; (i < line_count) && !user_found; i++) {
        temp = strdup(lines[i]);
        username_line = strtok_r(temp, ":", &temp);
        if (strcmp(username_line, username) == 0) {
            printf("line: %s\n", lines[i]);
            new_line = change_userfile_line(lines[i], password);
            lines[i] = new_line;
            user_found = 1;
        }
    }

    // Write the updated lines back to the file
    user_file_ptr = fopen(user_file_abs_path, "w");
    for (size_t i = 0; i < line_count; i++) {
        fputs(lines[i], user_file_ptr);
        free(lines[i]);
    }
    printf("User password updated\n");

    free(lines);
    fclose(user_file_ptr);
    return 1;
}

int check_credentials(char *htaccess_abs_path_, ValueNode* credentials) {
    char *htaccess_abs_path = strdup(htaccess_abs_path_);
    if (credentials == NULL) { // no credentials provided
        return 0;
    }

    // Reading the .htaccess file    
    char *AuthUserFile;
    AuthUserFile = get_field_from_htaccess(htaccess_abs_path, "AuthUserFile");
    if (!AuthUserFile) {
        fprintf(stderr, "AuthUserFile directive not found in .htaccess file.\n");
        return 0;
    }

    // Check if the user file exists
    char *userfile_abs_path;
    userfile_abs_path = get_userfile_abs_path(htaccess_abs_path, AuthUserFile);
    if (access(userfile_abs_path, F_OK) != 0) {
        printf("User file not found at %s\n", userfile_abs_path);
        return 0;
    }

    // Check if the credentials match the ones in the user file
    if (!credentials_match_userfile(userfile_abs_path, credentials)) {
        return 0;
    }

    return 1;
}

char* parse_credentials(char *authorization_value) {
    // Check if the header starts with "Basic "
    if (strncmp(authorization_value, "Basic ", 6) != 0) {
        fprintf(stderr, "Invalid Basic Authentication header.\n");
        return NULL;  // Not Basic Authentication
    }

    // Extract the base64-encoded credentials part
    char *encoded_credentials = authorization_value + 6;
    
    size_t decoded_len;
    char *decoded_credentials = base64_decode(encoded_credentials);

    if (decoded_credentials == NULL) {
        printf("Base64 decoding failed\n");
        return NULL;  // Base64 decoding failed
    }

    // The decoded credentials should be in the format "username:password"
    return decoded_credentials;
}

ValueNode* get_credentials(FieldNode *field_list) {
    FieldNode *field_node = field_list;
    ValueNode *credentials = NULL;
    
    while (field_node != NULL) {
        if (strcmp(field_node->field, "Authorization") == 0) {
            ValueNode *value = malloc(sizeof(ValueNode));
            value->value = parse_credentials(field_node->values->value+1);
            value->next = credentials;
            credentials = value;
        }
        field_node = field_node->next;
    }
    return credentials;
}

char *get_realm(char *resource, ValueNode* credentials) {                      
    // Tokenize the resource path by '/'
    char *token, *realm;
    char *resolved_resource_path = resolve_resource_path(resource);
    char partial_path[1024], htaccess_path[1024];
    strcpy(partial_path, webspace_path);
    htaccess_path[0] = '\0';

    // Fetching recursively each directory in the path until the final file
    token = strtok_r(resolved_resource_path, "/", &resolved_resource_path);
    while (token != NULL) {
        // Append the token (directory) to the partial path
        strcat(partial_path, "/");
        strcat(partial_path, token);

        // checking if there is a .htaccess file in the directory
        if (is_dir_protected(partial_path)) {
            strcat(htaccess_path, partial_path);
            strcat(htaccess_path, "/.htaccess");

            // No credentials provided
            if(!credentials){
                realm = get_field_from_htaccess(htaccess_path, "AuthName");
                return realm;    
            } 
            // Credentials provided but don't match the ones in the .htaccess file
            else if (!check_credentials(htaccess_path, credentials)) {
                realm = get_field_from_htaccess(htaccess_path, "AuthName");
                return realm;    
            }
            printf("Access granted for %s\n", htaccess_path);
            htaccess_path[0] = '\0';
        }
        // Move to the next token (directory or file)
        token = strtok_r(NULL, "/", &resolved_resource_path);
    }   
    return NULL;
}

char *htaccess_abs_path_from_form(char *resource) {
    char *resource_abs_path = get_resource_abs_path(resource);
    char *htaccess_abs_path = calloc(1024, sizeof(char));

    strcat(htaccess_abs_path, dirname(resource_abs_path));
    strcat(htaccess_abs_path, "/.htaccess");

    return htaccess_abs_path;
}

int update_passwords(char *resource, char **values) {
    char *username = values[0];
    char *password = values[1];
    char *new_password = values[2];
    char *conf_new_password = values[3];

    if (strcmp(new_password, conf_new_password) != 0) {
        fprintf(stderr, "Passwords do not match.\n");
        return 0;
    }

    // checking credentials for current password:
    char unencoded_credentials[128], value[128];
    snprintf(unencoded_credentials, 128, "%s:%s", username, password);
    snprintf(value, 128, " Basic %s", base64_encode(unencoded_credentials));
    ValueNode *credentials = append_value(NULL, value);

    char *htaccess_abs_path = htaccess_abs_path_from_form(resource);

    if (!check_credentials(htaccess_abs_path, credentials)) {
        return 0;
    }
    free(credentials);

    // get .htpasswd file from .htaccess file
    char *AuthUserFile = get_field_from_htaccess(htaccess_abs_path, "AuthUserFile");
    if (AuthUserFile == NULL) return 0;
    char *userfile_abs_path = get_userfile_abs_path(htaccess_abs_path, AuthUserFile);
    if (access(userfile_abs_path, F_OK) != 0) {
        printf("User file not found at %s\n", userfile_abs_path);
        return 0;
    }
    printf("Updating userfile located in %s\n", userfile_abs_path);

    if (!update_user_credentials(userfile_abs_path, username, new_password)) {
        return 0;
    }
    return 1;
}