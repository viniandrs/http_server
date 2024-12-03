#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>

#include "../include/server.h"
#include "../include/utils.h"
#include "../include/lists.h"

#include "../build/parser.tab.h"

#define MAX_CHILDREN 10
extern int max_threads;
int current_thread = 0;

extern void yy_scan_string(const char *s);
extern void yylex_destroy(void);

// handle the http request
void process_request(char* request, int client_socket_fd) {
    // pointers to address of body and header strings
    char **body_addr = malloc(sizeof(char *));
    char **header_addr = malloc(sizeof(char *));

    // parse the request to get the body and header
    yy_scan_string(request);
    yyparse(request, &body_addr, &header_addr);
    yylex_destroy();

    char *body = *body_addr;
    char *header = *header_addr;

    size_t reponse_len = snprintf(NULL, 0, "%s\n\n%s", header, body);
    char reponse[reponse_len + 1];
    sprintf(reponse, "%s\n\n%s", header, body);

    // Send response to browser
    send_page(reponse, client_socket_fd);    
    log_request(request, header);

    // free(body);
    // free(header);
}

int open_socket(int port, struct sockaddr_in *addr) {
    int socket_fd, client_socket;

    // Creating socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERROR opening socket: %s\n", strerror(errno));
        exit(errno);
    }
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    // Bind the socket to the port
    if (bind(socket_fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) < 0) {
        printf("Bind error: %s\n", strerror(errno));
        close(socket_fd);
        exit(errno);
    }

    // Listen for incoming connections
    if (listen(socket_fd, 3) < 0) {
        printf("Listen error: %s\n", strerror(errno));
        close(socket_fd);
        exit(errno);
    }
    printf("Server is listening on port %d\n", port);

    return socket_fd;
}

void* handle_request_thread(void* arg) {
    int client_socket = *((int*)arg);
    free(arg); // Free the dynamically allocated memory for client_socket

    size_t buffer_size = 1024;
    size_t request_len = 1024;
    char buffer[buffer_size];
    char *request;
    int timeout = 8000; // timeout in milliseconds
    struct pollfd fds[1];

    // Configuring struct pollfd
    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    // Reading the request until it is ended with a double CRLF or a timeout
    int n, n_bytes, total_bytes_read = 0;
    request = (char *)calloc(request_len, sizeof(char));
    while (1) {
        if ((n = poll(fds, 1, timeout)) == -1) {
            printf("Poll error: %s\n", strerror(errno));
            close(client_socket);
            current_thread--;
            pthread_exit(NULL);

        } else if (n == 0) {
            printf("Timeout reached\n");
            close(client_socket);
            current_thread--;
            pthread_exit(NULL);

        } else if (fds[0].revents & POLLIN) {
            // Reading up to n_bytes from the socket and appending it to the request buffer
            n_bytes = recv(client_socket, buffer, buffer_size, 0);
            if (n_bytes < 0) {
                printf("Receive error: %s\n", strerror(errno));
                close(client_socket);
                current_thread--;
                pthread_exit(NULL);
            } else if (n_bytes == 0) {
                printf("Connection closed by client\n");
                close(client_socket);
                current_thread--;
                pthread_exit(NULL);
            }

            buffer[n_bytes] = '\0';
            total_bytes_read += n_bytes;
            if (total_bytes_read + 1 > request_len) {
                request_len *= 2;
                request = (char *)realloc(request, request_len);
            }

            strcat(request, buffer);

            // if the request is ended with a double CRLF, process it
            if (strstr(request, "\r\n\r\n") != NULL) {
                // printf("Received request:\n%s\nStarting to parse...\n", request);
                process_request(request, client_socket);
                close(client_socket);
                free(request);
                current_thread--;
                pthread_exit(NULL); // End the thread after processing the request
            }
            
        } else {
            printf("Undefined poll error: %s\n", strerror(errno));
            close(client_socket);
            free(request);
            current_thread--;
            pthread_exit(NULL);
        }
    }
}

void start_server(int port, const int max_threads) {
    int socket_fd, client_socket_fd;
    struct sockaddr_in serv_addr;
    int address_len = sizeof(struct sockaddr_in);
    pthread_t thread_id[max_threads];

    // Set the address and port for the server
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    // Creating socket
    socket_fd = open_socket(port, &serv_addr);

    while (1) {
        // Accept an incoming connection
        if ((client_socket_fd = accept(socket_fd, (struct sockaddr *)&serv_addr, (socklen_t *)&address_len)) < 0) {
            printf("Accept error: %s\n", strerror(errno));
            continue; // keep trying to accept a connection instead of exiting
        }
        setsockopt(client_socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

        // Allocate memory for the client socket argument and pass it to the thread
        current_thread++;
        int* client_socket_ptr = (int*)malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            printf("Error allocating memory for client_socket\n");
            close(client_socket_fd);
            return;
        }
        *client_socket_ptr = client_socket_fd;

        // Create a thread to handle the request
        if (pthread_create(&thread_id[current_thread], NULL, handle_request_thread, (void*)client_socket_ptr) != 0) {
            printf("Thread creation failed: %s\n", strerror(errno));
            close(client_socket_fd);
            free(client_socket_ptr);
            current_thread--;
            continue;
        }

        // close(client_socket_fd);
        // close(socket_fd);
        // exit(1);
    }
}