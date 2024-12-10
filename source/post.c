#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../include/post.h"

// Function to split a string by a delimiter
char **split_pairs(const char *str) {
    char **result = (char **)malloc(sizeof(char *));
    int size = 0;
    char *copy = strdup(str);
    char *token = strtok_r(copy, "&", &copy);
    
    while (token) {
        result = (char **)realloc(result, sizeof(char *) * (size + 1));
        result[size] = strdup(token);
        size++;
        token = strtok_r(NULL, "&", &copy);
    }
    
    return result;
}

// Function to extract values from a POST request body
char **parse_request_body(const char *body) {
    char **pairs = NULL;
    char **values = NULL;
    int value_index = 0;

    // Split the body by '&' to get key-value pairs
    pairs = split_pairs(body);
    values = malloc(sizeof(char *) * 4);

    for (int i = 0; i < 4; i++) {
        char *key_value = pairs[i];
        char *delimiter_pos = strchr(key_value, '=');

        if (delimiter_pos) {
            // Move pointer to the value part
            values[value_index++] = strdup(delimiter_pos + 1);
        } else {
            values[value_index++] = strdup(""); // Handle missing value
        }

        free(key_value);
    }

    free(pairs);
    return values;
}

// Free the array of strings
void free_data_array(char **values) {
    for (int i = 0; i < 4; i++) {
        free(values[i]);
    }
    free(values);
}