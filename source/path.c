#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../include/path.h"

extern char *webspace_path;

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

char *get_resource_abs_path(char *resource_path) {
    char absolute_path[1024]="";
    strcat(absolute_path, webspace_path);
    strcat(absolute_path, resource_path);

    // Temporary buffer for the resolved absolute path
    char *resolved_path = calloc(1024, sizeof(char));
    realpath(absolute_path, resolved_path);
    
    return resolved_path;    
}

char *resolve_resource_path(char *resource_path) {
    // Temporary buffer for the resolved path
    char *abs_path = get_resource_abs_path(resource_path);
    strcat(abs_path, "/");

    // Remove the webspace path from the resolved path
    char *resolved_path;
    resolved_path = strdup(abs_path + strlen(webspace_path));
    free(abs_path);
    return resolved_path;
}

int is_in_webspace(char *resource_path) {
    // Get the absolute path of the resource
    char *abs_path = get_resource_abs_path(resource_path);

    // Check if the resource is in the webspace
    if (strncmp(webspace_path, abs_path, strlen(webspace_path)) != 0) {
        free(abs_path);
        return 0; // Resource is outside the webspace
    }
    free(abs_path);
    return 1; // Resource is in the webspace

}