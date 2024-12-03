#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include "../include/auth.h"

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

char *auth_header(char *webspace_path, char *resource, ValueNode* credentials) {
    // get the current date
    struct timeval tv;
    struct tm *tm_info;
    char time[30];

    gettimeofday(&tv, NULL);    
    tm_info = localtime(&tv.tv_sec); // Convert it to UTC time
    strftime(time, sizeof(time), "%a, %d %b %Y %H:%M:%S BRT", tm_info); // Format the time according to the HTTP date format

    // get the authentication realm
    char *realm = get_realm(webspace_path, resource, credentials);

    size_t header_len = snprintf(NULL, 0,
"HTTP/1.1 401 Authorization Required\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Transfer-Encoding: chunked\n\
Content-Type: text/html\n\
WWW-Authenticate: Basic realm=%s", time, realm);
    char *header = malloc(header_len + 1);
    sprintf(header,
"HTTP/1.1 401 Authorization Required\n\
Date: %s\n\
Server: Vinicius Andreossi's Simple HTTP Server v0.1\n\
Transfer-Encoding: chunked\n\
Content-Type: text/html\n\
WWW-Authenticate: Basic realm=%s", time, realm);
    return header;
}

int check_credentials(char *htaccess_path, ValueNode* credentials) {
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

char *get_realm(char *webspace_path, char *resource, ValueNode* credentials) {
    size_t abs_path_len = snprintf(NULL, 0, "%s%s", webspace_path, resource);
    char abs_path[abs_path_len + 1];
    sprintf(abs_path, "%s%s", webspace_path, resource);
                        
    // Tokenize the resource path by '/'
    char *token;
    char *resource_copy = strdup(resource);  // Duplicate the string as strtok modifies it
    char partial_path[strlen(abs_path) + 1];
    char htaccess_path[strlen(abs_path) + 11];
    char real_path[strlen(abs_path) + 1];
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

            // Reading the .htaccess file
            FILE *htaccess_file = fopen(htaccess_path, "r");
            if (htaccess_file == NULL) {
                perror("Error while opening .htaccess file");
                return NULL;
            }

            char line[1024];
            while (fgets(line, sizeof(line), htaccess_file)) {
                if (strncmp(line, "AuthName", 8) == 0) {
                    // Found the AuthName directive
                    char *realm = strchr(line, ' ');
                    realm[strlen(realm) - 1] = '\0';

                    fclose(htaccess_file);
                    return strdup(realm+1); // removing the space
                }
            }
            fclose(htaccess_file);
        }
        // Move to the next token (directory or file)
        token = strtok(NULL, "/");
    }
}