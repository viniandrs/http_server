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
#include "../include/path.h"
#include "../include/utils.h"

extern char *webspace_path;

// Function to decode a Base64-encoded string
char *base64_decode(char *input) {
    const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int input_length = strlen(input);
    int output_length = (input_length / 4) * 3;

    if (input[input_length - 1] == '=') output_length--;
    if (input[input_length - 2] == '=') output_length--;

    char *decoded = (char *)malloc(output_length + 1);
    if (!decoded) return NULL;

    int i, j = 0;
    unsigned int temp = 0;

    for (i = 0; i < input_length; i++) {
        if (input[i] == '=') break;

        temp = temp << 6 | (strchr(lookup, input[i]) - lookup);
        if (i % 4 == 3) {
            decoded[j++] = (temp >> 16) & 0xFF;
            decoded[j++] = (temp >> 8) & 0xFF;
            decoded[j++] = temp & 0xFF;
            temp = 0;
        }
    }

    if (i % 4 == 2) {
        decoded[j++] = (temp >> 4) & 0xFF;
    } else if (i % 4 == 3) {
        decoded[j++] = (temp >> 10) & 0xFF;
        decoded[j++] = (temp >> 2) & 0xFF;
    }

    decoded[j] = '\0';
    return decoded;
}

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
    if (token == NULL) {
        free(copy);
        printf("Invalid hash format\n");
        return 0; // Invalid hash format
    }
    *n = atoi(token); // Parse the algorithm identifier as an integer

    // Get the salt
    token = strtok_r(NULL, "$", &copy);
    if (token == NULL) {
        free(copy);
        printf("Invalid hash format\n");
        return 0; // Invalid hash format
    }
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
        char *end_token;
        encoded_credentials = strdup(credentials->value + 7);
        decoded_credentials = base64_decode(encoded_credentials);
        printf("Decoded credentials: %s\n", decoded_credentials);
        password = strdup(strchr(decoded_credentials, ':') + 1);
        username = strtok_r(decoded_credentials, ":", &end_token);
        
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
                // printf("hash of credentials: |%s|\n", credentials_hash);
                // printf("hash in file: |%s|\n", file_hash);
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

int check_credentials(char *htaccess_abs_path, ValueNode* credentials) {
    char *htaccess_path = strdup(htaccess_abs_path);
    if (credentials == NULL) { // no credentials provided
        return 0;
    }

    // Reading the .htaccess file    
    char *AuthUserFile;
    AuthUserFile = get_field_from_htaccess(htaccess_path, "AuthUserFile");
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
                if (!realm) {
                    fprintf(stderr, "AuthName directive not found in .htaccess file located at %s\n", htaccess_path);        
                    return NULL;
                }    
                return realm;    
            } 
            // Credentials provided but don't match the ones in the .htaccess file
            else if (!check_credentials(htaccess_path, credentials)) {
                realm = get_field_from_htaccess(htaccess_path, "AuthName");
                if (!realm) {
                    fprintf(stderr, "AuthName directive not found in .htaccess file.\n");        
                    return NULL;
                }    
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

int update_passwords(char *htaccess_abs_path, char **values) {
    char *username = values[0];
    char *password = values[1];
    char *new_password = values[2];
    char *conf_new_password = values[3];

    if (strcmp(new_password, conf_new_password) != 0) {
        fprintf(stderr, "Passwords do not match.\n");
        return 0;
    }

    // checking credentials for current password:
    ValueNode *credentials = malloc(sizeof(ValueNode));
    credentials->value = malloc(strlen(username) + strlen(password) + 2);
    snprintf(credentials->value, strlen(username) + strlen(password) + 2, "%s:%s", username, password);
    credentials->next = NULL;

    printf("Checking password... %d\n", check_credentials(htaccess_abs_path, credentials));
    if (!check_credentials(htaccess_abs_path, credentials)) {
        fprintf(stderr, "Incorrect password.\n");
        return 0;
    }
    free(credentials->value);
    free(credentials);

    // get .htpasswd file from .htaccess file
    char *AuthUserFile = get_field_from_htaccess(htaccess_abs_path, "AuthUserFile");
    if (AuthUserFile == NULL) {
        fprintf(stderr, "AuthUserFile directive not found in .htaccess file.\n");
        return 0;
    }

    char htpasswd_path[1024]="";
    char htpasswd_abs_path[1024];
    strcat(htpasswd_path, dirname(htaccess_abs_path));
    strcat(htpasswd_path, "/");
    strcat(htpasswd_path, AuthUserFile);
    realpath(htpasswd_path, htpasswd_abs_path);

    // Check if the user file exists
    if (access(htpasswd_abs_path, F_OK) != 0) {
        printf("User file not found at %s\n", htpasswd_abs_path);
        return 0;
    }

     // Saving credentials into memory
    FILE *user_file_ptr = fopen(htpasswd_abs_path, "r");
    if (user_file_ptr == NULL) {
        perror("Error while opening user file");
        return 0;
    }

    char **lines = NULL;
    size_t line_count = 0;
    char line[1024];

    while (fgets(line, sizeof(line), user_file_ptr)) {
        lines = realloc(lines, sizeof(char *) * (line_count + 1));
        lines[line_count] = strdup(line);
        line_count++;
    }

    fclose(user_file_ptr);

    // Update the password for the specified user
    int user_found = 0;
    for (size_t i = 0; i < line_count; i++) {
        char *temp = strdup(lines[i]);
        char *username_line = strtok(temp, ":");
        if (strcmp(username_line, username) == 0) {
            free(lines[i]);
            size_t new_line_size = strlen(username) + strlen(new_password) + 3;
            lines[i] = malloc(new_line_size);
            snprintf(lines[i], new_line_size, "%s:%s\n", username, new_password);
            user_found = 1;
        }
        free(temp);
    }
    // Add the user to the file
    if (!user_found) {
        size_t new_line_size = strlen(username) + strlen(new_password) + 3;
        lines = realloc(lines, sizeof(char *) * (line_count + 1));
        lines[line_count] = malloc(new_line_size);
        snprintf(lines[line_count], new_line_size, "%s:%s\n", username, new_password);
        line_count++;
    }

    // Write the updated lines back to the file
    FILE *new_user_file_ptr = fopen(htpasswd_abs_path, "w");
    if (new_user_file_ptr == NULL) {
        perror("Error while opening user file for writing");
        for (size_t i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return 0;
    }

    for (size_t i = 0; i < line_count; i++) {
        fputs(lines[i], new_user_file_ptr);
        free(lines[i]);
    }

    free(lines);
    fclose(new_user_file_ptr);

    return 1;
}