#ifndef LISTS_H
#define LISTS_H

typedef struct ValueNode {
    char *value;
    struct ValueNode *next;
} ValueNode;

typedef struct FieldNode {
    char *field;
    struct FieldNode *next;
    ValueNode *values;
} FieldNode;

FieldNode *get_field(FieldNode *field_list, char *field);

/* Inserts a new field with a value list in the end of the field list*/
FieldNode *create_node(char *field, ValueNode *value_list);

/* Inserts a new field in the end of the field list*/
FieldNode *add_node(FieldNode* field_list, FieldNode *new_node);

/* Prints the field list */
void dump(FieldNode *field_list);

void dump_values(ValueNode *field_list);

/* Inserts the values in the end of the value list*/
ValueNode *add_value(ValueNode *value_list, char *value);

void *free_field_list(FieldNode *field_list);

void *free_value_list(ValueNode *value_list);

#endif