#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char *base64_encode(char *input) {
    const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int input_length = strlen(input);
    int output_length = ((input_length + 2) / 3) * 4;

    char *encoded = (char *)malloc(output_length + 1);
    if (!encoded) return NULL;

    int i, j = 0;
    unsigned int temp = 0;

    for (i = 0; i < input_length; i++) {
        temp = temp << 8 | input[i];
        if ((i + 1) % 3 == 0) {
            encoded[j++] = lookup[(temp >> 18) & 0x3F];
            encoded[j++] = lookup[(temp >> 12) & 0x3F];
            encoded[j++] = lookup[(temp >> 6) & 0x3F];
            encoded[j++] = lookup[temp & 0x3F];
            temp = 0;
        }
    }

    if (i % 3 == 1) {
        encoded[j++] = lookup[(temp >> 2) & 0x3F];
        encoded[j++] = lookup[(temp << 4) & 0x3F];
        encoded[j++] = '=';
        encoded[j++] = '=';
    } else if (i % 3 == 2) {
        encoded[j++] = lookup[(temp >> 10) & 0x3F];
        encoded[j++] = lookup[(temp >> 4) & 0x3F];
        encoded[j++] = lookup[(temp << 2) & 0x3F];
        encoded[j++] = '=';
    }

    encoded[j] = '\0';
    return encoded;
}