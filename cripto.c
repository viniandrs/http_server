/* Compile com: gcc -o cripto cripto.c -lcrypt */
#define _XOPEN_SOURCE
#include <crypt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv){
    if (argc < 4){
        printf("Usage: %s <password> <n> <salt>\n", argv[0]);
        exit(0);
    }
    
    char salt[8];
    snprintf(salt, 8, "$%s$%s$", argv[2], argv[3]);    

    printf("\n( Salt = %s ) + ( Password = %s ) ==> crypt = %s \n\n",
        salt, argv[1], crypt(argv[1], salt));

    return(0);
}