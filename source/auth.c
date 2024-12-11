#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include "../include/auth.h"
#include "../include/utils.h"

extern char *webspace_path;

int is_dir_protected(char *dir_abs_path) {
    char htaccess_path[1024];
    strcat(htaccess_path, dir_abs_path);
    strcat(htaccess_path, "/.htaccess");

    printf("Checking for .htaccess in: %s\n", htaccess_path);
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

// Function to decode a Base64-encoded string
char *base64_decode(const char *input) {
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

int check_credentials(char *htaccess_path_original, ValueNode* credentials) {
    char *htaccess_path = strdup(htaccess_path_original);
    if (credentials == NULL) { // no credentials provided
        return 0;
    }

    FILE *htaccess_file = fopen(htaccess_path, "r");
    if (htaccess_file == NULL) {
        perror("Error while opening .htaccess file");
        return 0;
    }

    // Reading the .htaccess file
    char line[1024];
    while (fgets(line, sizeof(line), htaccess_file)) {
        if (strncmp(line, "AuthUserFile", 12) == 0) {
            // Found the AuthUserFile directive
            char *AuthUserFile = strchr(line, ' '); // relative path to the user file (from .htaccess directory)
            AuthUserFile[strlen(AuthUserFile) - 1] = '\0';
            char user_file_relative_path[1024]="";
            char *htaccess_dir = dirname(htaccess_path);
            strcat(user_file_relative_path, htaccess_dir);
            strcat(user_file_relative_path, "/");
            strcat(user_file_relative_path, AuthUserFile + 1);

            char user_file[1024]; // Absolute path to the user file
            realpath(user_file_relative_path, user_file);

            // Check if the user file exists
            if (access(user_file, F_OK) != 0) {
                printf("User file not found at %s\n", user_file);
                fclose(htaccess_file);
                return 0;
            }

            // Check if the credentials match the ones in the user file
            FILE *user_file_ptr = fopen(user_file, "r");
            if (user_file_ptr == NULL) {
                perror("Error while opening user file");
                fclose(htaccess_file);
                return 0;
            }
            
            char *username, *password, *temp ;
            char user_line[1024];

            // Iterate over the credentials
            while (credentials != NULL) {
                char *end_token;
                temp = strdup(credentials->value);
                username = strtok_r(temp, ":", &end_token);
                password = strchr(credentials->value, ':') + 1;
                while (fgets(user_line, sizeof(user_line), user_file_ptr)) {
                    if (strncmp(user_line, username, strlen(username)) == 0) {
                        // Found the username
                        if (strncmp(user_line + strlen(username) + 1, password, strlen(password)) == 0) {
                            // Found the password
                            fclose(user_file_ptr);
                            fclose(htaccess_file);
                            return 1;
                        }
                    }
                }
                credentials = credentials->next;
                temp = "";
            }
            fclose(user_file_ptr);
        }
    }

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
    size_t max_len = strlen(webspace_path) + strlen(resource) + 1;                     
    // Tokenize the resource path by '/'
    char *token;
    char *resource_copy = strdup(resource);  // Duplicate the string as strtok modifies it
    char partial_path[max_len];
    char htaccess_path[strlen(webspace_path) + strlen(resource) + 11];
    char real_path[max_len];
    strcpy(partial_path, webspace_path);

    // Fetching recursively each directory in the path until the final file
    token = strtok(resource_copy, "/");
    while (token != NULL) {
        // Append the token (directory) to the partial path
        strcat(partial_path, "/");
        strcat(partial_path, token);

        // checking if there is a .htaccess file in the directory
        sprintf(htaccess_path, "%s%s", partial_path, "/.htaccess");
        if (access(htaccess_path, F_OK) == 0) {
            // if the credentials don't match the ones in the .htaccess file
            // return the realm
            if (check_credentials(htaccess_path, credentials)) {
                continue;
            }

            char *realm  = get_field_from_htaccess(htaccess_path, "AuthName");

            if (!realm) {
                fprintf(stderr, "AuthName directive not found in .htaccess file.\n");
                free(resource_copy);
                return NULL;
            }
            free(resource_copy);
            return realm;
        }
        // Move to the next token (directory or file)
        token = strtok(NULL, "/");
    }
    free(resource_copy);
    return NULL;
}

int update_passwords(char *htaccess_path, char **values) {
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

    if (!check_credentials(htaccess_path, credentials)) {
        fprintf(stderr, "Incorrect password.\n");
        return 0;
    }
    free(credentials->value);
    free(credentials);

    // get .htpasswd file from .htaccess file
    char *AuthUserFile = get_field_from_htaccess(htaccess_path, "AuthUserFile");
    if (AuthUserFile == NULL) {
        fprintf(stderr, "AuthUserFile directive not found in .htaccess file.\n");
        return 0;
    }

    char htpasswd_path[1024]="";
    char htpasswd_abs_path[1024];
    strcat(htpasswd_path, dirname(htaccess_path));
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