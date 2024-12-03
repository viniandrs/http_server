#include "../include/lists.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

FieldNode *get_field(FieldNode *field_list, char *field) {
    FieldNode *current_node = field_list;
    while (current_node != NULL) {
        if (strcmp(current_node->field, field) == 0) {
            return current_node;
        }
        current_node = current_node->next;
    }
    return NULL;
}

FieldNode *create_node(char *field, ValueNode *value_list) {
    FieldNode *new_node = malloc(sizeof(FieldNode));
    new_node->field = strdup(field);
    new_node->values = value_list;
    new_node->next = NULL;

    return new_node;
}

FieldNode *add_node(FieldNode* field_list, FieldNode *new_node) {
    if (!field_list){
        field_list = new_node;
        return field_list;
    }
    if (!new_node) {
        return field_list;
    }

    // add the new node to the end of the list
    FieldNode *current_node = field_list;
    while(current_node->next != NULL) {
        current_node = current_node->next;
    }
    current_node->next = new_node;
    return field_list;
}

void dump(FieldNode *field_list) {
    FieldNode *current_node = field_list;
    ValueNode *current_value = NULL;
    while (current_node->next != NULL) {
        printf("Field: %s\n", current_node->field);
        current_value = current_node->values;
        while (current_value->next != NULL) {
            printf("Value: %s\n", current_value->value);
            current_value = current_value->next;
        }
        printf("Value: %s\n", current_value->value);
        current_node = current_node->next;
    }
    printf("Field: %s\n", current_node->field);
    current_value = current_node->values;
    while (current_value->next != NULL) {
        printf("Value: %s\n", current_value->value);
        current_value = current_value->next;
    }
    printf("Value: %s\n", current_value->value);
}

void dump_values(ValueNode *value_list) {
    ValueNode *current_node = value_list;
    if (!current_node) {
        printf("Empty list\n");
        return;
    }
    
    while (current_node->next != NULL) {
        printf("Value: %s\n", current_node->value);
        current_node = current_node->next;
    }
    printf("Value: %s\n", current_node->value);
}

ValueNode* add_value(ValueNode *value_list, char *value) {
    ValueNode *new_node = malloc(sizeof(ValueNode));
    new_node->value = strdup(value);
    new_node->next = NULL;

    if (!value_list){
        value_list = new_node;
        return value_list;
    }

    ValueNode *current_node = value_list;
    while(current_node->next != NULL) {
        current_node = current_node->next;
    }
    current_node->next = new_node;

    return value_list;
}

void *free_value_list(ValueNode *value_list) {
    ValueNode *current_node = value_list;
    ValueNode *next_node = NULL;
    while (current_node->next != NULL) {
        next_node = current_node->next;
        free(current_node->value);
        free(current_node);
        current_node = next_node;
    }
    free(current_node->value);
    free(current_node);
}

void *free_field_list(FieldNode *field_list) {
    FieldNode *current_node = field_list;
    FieldNode *next_node = NULL;
    while (current_node->next != NULL) {
        next_node = current_node->next;
        free_value_list(current_node->values);
        free(current_node->field);
        free(current_node);
        current_node = next_node;
    }
    free_value_list(current_node->values);
    free(current_node->field);
    free(current_node);
}