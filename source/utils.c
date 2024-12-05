#include "../include/lists.h"
#include "../include/methods.h"
#include "../include/utils.h"
#include "../include/auth.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stddef.h>

extern char *log_file;

char *format_webspace_path(char *webspace_path) {
    // Temporary buffer for the resolved absolute path
    char resolved_webspace_path[1024];

    // If webspace_path ends with '/', remove it
    char *path = strdup(webspace_path); // Make a copy of the input string
    if (path[strlen(path) - 1] == '/') {
        path[strlen(path) - 1] = '\0';
    }

    // Get the absolute path of the webspace
    if (realpath(path, resolved_webspace_path) == NULL) {
        // Handle the case where realpath fails
        free(path);
        return NULL;  // Or handle error appropriately
    }

    // Dynamically allocate memory for the resulting path
    char *formatted_path = strdup(resolved_webspace_path);

    // Free the temporary path copy and return the formatted path
    free(path);
    return formatted_path;
}

char *format_resource_path(char *resource) {
    // if resource is ends with '/', remove it
    if (resource[strlen(resource) - 1] == '/') {
        resource[strlen(resource) - 1] = '\0';
    }
    return resource;
}

int fetchr(char* webspace_path, char *resource, struct stat *st, ValueNode *credentials) {
    int status;
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

    // The first dir to fetch is the webspace itself
    status = fetch(partial_path, st);

    // Fetching recursively each directory in the path until the final file
    token = strtok(resource_copy, "/");
    while (token != NULL && status == 200) {
        // Append the token (directory) to the partial path
        strcat(partial_path, "/");
        strcat(partial_path, token);

        // checking if the webspace path is in the beginning of the real path
        // if it's not, the resource is trying to access a file outside the webspace
        realpath(partial_path, real_path);
        if (strncmp(webspace_path, real_path, strlen(webspace_path)) != 0) {
            status = 403;  // Forbidden
            break;
        }

        // checking if the resource is protected by .htaccess file
        sprintf(htaccess_path, "%s%s", partial_path, "/.htaccess");
        if (access(htaccess_path, F_OK) != -1) {
            // if the credentials match the ones in the .htaccess file, continue
            printf("Checking credentials for %s\n", htaccess_path);
            printf("Credentials provided: ");
            dump_values(credentials);
            if (!check_credentials(htaccess_path, credentials)) {
                printf("Access denied for %s\n", htaccess_path);
                status = 401;  // Forbidden
                break;
            }
        }
        htaccess_path[0] = '\0';

        // Fetch the resource
        status = fetch(real_path, st);
        printf("Status for %s: %d\n", partial_path, status);

        // Move to the next token (directory or file)
        token = strtok(NULL, "/");
    }

    // if the final file is a directory, fetch for index.html or welcome.html
    if (S_ISDIR(st->st_mode) && status == 200) {
        printf("fetching for index.html or welcome.html in dir %s\n", partial_path);
        DIR *dir = opendir(partial_path);
        struct dirent *entry;
        int file_found = 0;

        // checking if there's a .htaccess file in the directory
        sprintf(htaccess_path, "%s%s", partial_path, "/.htaccess");
        if (access(htaccess_path, F_OK) != -1) {
            // checking credentials
            printf("Checking credentials for %s\n", htaccess_path);
            printf("Credentials provided: ");
            dump_values(credentials);
            if (!check_credentials(htaccess_path, credentials)) {
                printf("Access denied for %s\n", htaccess_path);
                status = 401;  // Forbidden
            }
        }

        char *dir_path = strdup(partial_path);
        while ((entry = readdir(dir)) != NULL) {
            if ((strcmp(entry->d_name, "index.html") == 0) 
            ||  (strcmp(entry->d_name, "welcome.html") == 0)) {
                
                strcat(partial_path, "/");
                strcat(partial_path, entry->d_name);
                file_found = 1;
                status = fetch(partial_path, st);
                printf("Found %s with status %d\n", partial_path, status);
                
                if(status == 200) break;

                // if the first file found is not accessible, keep looking for the another one
                strcpy(partial_path, dir_path);
            }
        }
        // return the st pointer to directory
        stat(dir_path, st);
        free(dir_path);
        closedir(dir);
        if (!file_found) status = 404; // Not found
    }
    free(resource_copy);
    return status;
}

// get the header and body of a resource in the webspace. If body is NULL, only the header is returned
int fetch(const char *path, struct stat *st) {
    int fd;

    // Using stat to get file information
    if (stat(path, st) != 0) {
        if (errno == ENOENT) {
            return 404; // Not found
        } else {
            printf("Stat error: %s\n", strerror(errno));
            return 500;  // Internal server error
        }
    }

    // if the resource is a regular file
    if (S_ISREG(st->st_mode)) { 
        // check for read permission with bitmask S_IRUSR (Read permission, owner) 
        if (!(st->st_mode & S_IRUSR)) {
            return 403; // Forbidden
        }

        // open it in read-only mode
        fd = open(path, O_RDONLY);
        if ((fd = open(path, O_RDONLY)) == -1) {
            printf("Open error: %s\n", strerror(errno));
            return 500;  // Internal server error
        }
        close(fd);
        return 200;

    } 

    // if the resource is a directory
    else if (S_ISDIR(st->st_mode)) {
        // check if it has execution permission with bitmask S_IXUSR (Execute (for ordinary files) or search (for directories))
        if (!(st->st_mode & S_IXUSR)) {
            return 403; // Forbidden
        }

        // Open the directory
        DIR *dir = opendir(path);
        if (!dir) {
            printf("Open dir error: %s\n", strerror(errno));
            return 500;  // Internal server error
        }
        closedir(dir);
        return 200;
    }    

    // If neither a file nor a directory, raises error
    printf("Resource is not a file or directory\n");
    return 500;  // Internal server error
}

int send_page(char *response, int socket_fd) {
    int response_len = strlen(response);
    int n_bytes;

    // Send the response
    if ((n_bytes = send(socket_fd, response, response_len, 0)) < 0) {
        printf("Send error: %s\n", strerror(errno));
        close(socket_fd);
        return errno;
    }
    printf("Sent response with size of %d bytes\n", n_bytes);
    close(socket_fd);
    return 0;
}

int log_request(char *request, char *header) {
    FILE *log = fopen(log_file, "a");
    if (log == NULL) {
        printf("Error while opening log file: %s\n", strerror(errno));
        return errno;
    }

    fprintf(log, "Request:\n%sHeader:\n%s\n", request, header);
    fclose(log);
    return 0;
}