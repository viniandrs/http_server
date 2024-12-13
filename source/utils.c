#include "../include/lists.h"
#include "../include/path.h"
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
#include <libgen.h>

extern char *log_file, *webspace_path;

int fetchr(char *resource, ValueNode *credentials) {
    if (!is_in_webspace(resource)) {
        return 403;  // Forbidden
    }

    int status;
                        
    // Tokenize the resource path by '/'
    char *token;
    char *resolved_resource_path = resolve_resource_path(resource);  // Duplicate the string as strtok modifies it
    char *p = resolved_resource_path;
    char partial_path[1024];
    char htaccess_path[1024];
    strcpy(partial_path, webspace_path);
    htaccess_path[0] = '\0';

    // The first dir to fetch is the webspace itself
    status = fetch(partial_path);
    if (status != 200) {
        free(p);
        return status;
    }

    // Fetching recursively each directory in the path until the final file
    token = strtok_r(resolved_resource_path, "/", &resolved_resource_path);
    while (token != NULL && status == 200) {
        // Append the token (directory) to the partial path
        strcat(partial_path, "/");
        strcat(partial_path, token);

        // checking if the resource is protected by .htaccess file
        if (is_dir_protected(partial_path)) {
            strcat(htaccess_path, partial_path);
            strcat(htaccess_path, "/.htaccess");
            
            if (!check_credentials(htaccess_path, credentials)) {
                free(p);
                return 401;  // Forbidden
            }
            htaccess_path[0] = '\0';
        }

        // Fetch the resource
        status = fetch(partial_path);

        // Move to the next token (directory or file)
        token = strtok_r(NULL, "/", &resolved_resource_path);
    }
    free(p); // Free the resolved_resource_path
    if (status != 200) {
        return status;
    }

    // if the final file is a directory, fetch for index.html or welcome.html
    struct stat st;
    stat(partial_path, &st);
    if (S_ISDIR(st.st_mode)) {

        // checking if the resource is protected by .htaccess file
        if (is_dir_protected(partial_path)) {
            strcat(htaccess_path, partial_path);
            strcat(htaccess_path, "/.htaccess");

            if (!check_credentials(htaccess_path, credentials)) {
                return 401;  // Forbidden
            }
        }
        
        // look for index.html or welcome.html
        char *dir_path = strdup(partial_path);
        if ((status = fetch_for_file_in_dir(dir_path, "index.html")) == 200) {
            free(dir_path);
            return 200;
        }
        if ((status = fetch_for_file_in_dir(dir_path, "welcome.html")) == 200) {
            free(dir_path);
            return 200;
        }
        free(dir_path);
        return status; 
    }
    return status;
}

int fetch_for_file_in_dir(char *dir_abs_path, char *filename) {
    int status;
    char path[1024]="";
    DIR *dir = opendir(dir_abs_path);
    if (!dir) {
        printf("Error while opening dir %s: %s\n",dir_abs_path, strerror(errno));
        return 500;  // Internal server error
    }
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, filename) == 0) {
            strcat(path, dir_abs_path);
            strcat(path, "/");
            strcat(path, entry->d_name);
            
            status = fetch(path);
            printf("Status for %s: %d \n", path, status);

            closedir(dir);
            return status;
        }
    }
    // file was not found
    closedir(dir);
    return 404;
}

// get the header and body of a resource in the webspace. If body is NULL, only the header is returned
int fetch(const char *abs_path) {
    struct stat st;
    int fd;

    // Using stat to get file information
    if (stat(abs_path, &st) != 0) {
        if (errno == ENOENT) {
            return 404; // Not found
        } else {
            printf("Stat error: %s\n", strerror(errno));
            return 500;  // Internal server error
        }
    }

    // if the resource is a regular file
    if (S_ISREG(st.st_mode)) { 
        // check for read permission with bitmask S_IRUSR (Read permission, owner) 
        if (!(st.st_mode & S_IRUSR)) {
            return 403; // Forbidden
        }

        // open it in read-only mode
        fd = open(abs_path, O_RDONLY);
        if ((fd = open(abs_path, O_RDONLY)) == -1) {
            printf("Open error: %s\n", strerror(errno));
            return 500;  // Internal server error
        }
        close(fd);
        return 200;

    } 

    // if the resource is a directory
    else if (S_ISDIR(st.st_mode)) {
        // check if it has execution permission with bitmask S_IXUSR (Execute (for ordinary files) or search (for directories))
        if (!(st.st_mode & S_IXUSR)) {
            return 403; // Forbidden
        }

        // Open the directory
        DIR *dir = opendir(abs_path);
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

int write_buffer(char *buffer, int n_bytes, int fd) {
    size_t bytes_written = 0;

    // Send the response
    if ((bytes_written = send(fd, buffer, n_bytes, 0)) < 0) {
        printf("Send error: %s\n", strerror(errno));
        close(fd);
        return errno;
    }
    return 0;
}

int log_request(char *request, char *header) {
    FILE *log = fopen(log_file, "a");
    if (log == NULL) {
        printf("Error while opening log file: %s\n", strerror(errno));
        return errno;
    }

    fprintf(log, "%s\n%s\n\n", request, header);
    fclose(log);
    return 0;
}

char *htaccess_from_form(char *form_path) {
    size_t htaccess_len = strlen(form_path); // Form path and .htaccess path have same len
    char *htaccess_path = calloc(htaccess_len + 1, sizeof(char));

    // Copy the directory part and append ".htaccess"
    strcat(htaccess_path, dirname(form_path));
    strcat(htaccess_path, "/.htaccess");

    char *absolute_htaccess_path = get_resource_abs_path(htaccess_path);
    free(htaccess_path);
    
    return absolute_htaccess_path;
}
