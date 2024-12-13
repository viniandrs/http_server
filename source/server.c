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
#include "../include/request_processing.h"
#include "../include/auth.h"

#include "../build/parser.tab.h"

#define MAX_CHILDREN 10
extern int max_threads;
int current_thread = 0;
pthread_mutex_t lock; 

extern void yy_scan_string(const char *s);
extern void yylex_destroy(void);

// handle the http request
void process_request(char* request, char *request_body,  int client_socket_fd) {
    // printing request details on the screen
    printf("\nReceived request:\n%s\nwith request body:\n%s\n\n", request, request_body);
    FieldNode *field_list;
    char *method, *resource;

    FieldNode **field_list_addr = malloc(sizeof(FieldNode*));
    char **method_addr = malloc(sizeof(char*));
    char **filepath_addr = malloc(sizeof(char*));

    // parse the request to get the body and header
    yy_scan_string(request);
    if(yyparse(method_addr, filepath_addr, field_list_addr)) {
        printf("Error while parsing the request\n");
        close(client_socket_fd);
        return;
    }
    yylex_destroy();

    field_list = *field_list_addr;
    method = *method_addr;
    resource = *filepath_addr;

    // getting header
    int status;
    char *header;
    header = get_header(resource, field_list, &status);
    
    printf("\n%s\n", header);
    if (write_buffer(header, strlen(header), client_socket_fd) != 0) {
        printf("Error while writing header in socket: %s\n", strerror(errno));
        close(client_socket_fd);
        return;
    }

    // sending the resource according to the HTTP method
    if ((status != 200) && (status != 201)) {
        send_error_file(status, client_socket_fd); // if the status is not 200 or 201, send the error body
    } else {
        if (strcmp(method, "GET") == 0) {
            // send the resource
            if (send_file(resource, client_socket_fd)) {
                printf("Error while sending file: %s\n", strerror(errno));
                close(client_socket_fd);
                return;
            }
        } else if (strcmp(method, "HEAD") == 0) {
            // send only the header
            if (write_buffer(NULL, 0, client_socket_fd)) {
                printf("Error while writing header in socket: %s\n", strerror(errno));
                close(client_socket_fd);
                return;
            }
        } else if (strcmp(method, "TRACE") == 0) {
            // write the request back to the client
            size_t body_len = strlen(request) + strlen(request_body) + 2;
            char body[body_len];
            strcpy(body, request);
            strcat(body, "\r\n");
            strcat(body, request_body);

            if (write_buffer(body, strlen(body), client_socket_fd)) {
                printf("Error while writing body in socket: %s\n", strerror(errno));
                close(client_socket_fd);
                return;
            }
        } else if (strcmp(method, "OPTIONS") == 0) {
            // send only the header
            if (write_buffer(NULL, 0, client_socket_fd)) {
                printf("Error while writing header in socket: %s\n", strerror(errno));
                close(client_socket_fd);
                return;
            }
        } else if (strcmp(method, "POST") == 0) {
            char **values = parse_request_body(request_body);

            status = 201;
            if(!update_passwords(resource, values)) {
                // password not changed
                printf("Error while changing password\n");
                send_file("/status_pages/password_not_updated.html", client_socket_fd);
                
            } else {
                printf("Password changed successfully!\n");
                send_file("/status_pages/password_updated.html", client_socket_fd);
            }

            free_data_array(values);
        } else {
            send_error_file(501, client_socket_fd); // Not implemented
        }
        
    }   
    close(client_socket_fd);
    log_request(request, header);

    free(header);
    free(method);
    free(resource);
    free(field_list_addr);
    free(method_addr);
    free(filepath_addr);
    free_field_list(field_list);
}

#define REQUEST_LEN 1024
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

    char *crlf; // Pointer to the double CRLF in the request, allowing to split the request into header and body;
    char request[REQUEST_LEN]; // Buffer to store the request
    int timeout = 8000; // timeout in milliseconds
    struct pollfd fds[1];

    // Configuring struct pollfd
    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    // Reading the request
    int n, n_bytes;
    while (1) {
        if ((n = poll(fds, 1, timeout)) == -1) { // Poll error
            printf("Poll error: %s\n", strerror(errno));
            close(client_socket);

            pthread_mutex_lock(&lock);
            current_thread--;
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);

        } else if (n == 0) { // Timeout reached
            printf("Timeout reached\n");
            close(client_socket);
            
            pthread_mutex_lock(&lock);
            current_thread--;
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);

        } else if (fds[0].revents & POLLIN) { // Read the request
            n_bytes = recv(client_socket, request, REQUEST_LEN, 0);
            if (n_bytes < 0) {
                printf("Receive error: %s\n", strerror(errno));
                close(client_socket);
                
                pthread_mutex_lock(&lock);
                current_thread--;
                pthread_mutex_unlock(&lock);
                pthread_exit(NULL);
            } else if (n_bytes == 0) {
                printf("Connection closed by client\n");
                close(client_socket);
                
                pthread_mutex_lock(&lock);
                current_thread--;
                pthread_mutex_unlock(&lock);
                pthread_exit(NULL);
            }

            request[REQUEST_LEN] = '\0'; // Ensure the request is null-terminated

            // process the request if is ended with a double CRLF (from browser)
            if ((crlf = strstr(request, "\r\n\r\n")) != NULL) {
                crlf[2] = '\0'; // End the request string
                char *request_body = crlf + 4;
                process_request(request, request_body, client_socket);
            }
            // process the request if is ended with a single CRLF (from telnet)
            else if ((crlf = strstr(request, "\r\n")) != NULL) {
                crlf[2] = '\0'; // End the request string
                char *request_body = crlf + 2;
                process_request(request, request_body, client_socket);
            }

            else {
                printf("Bad request\n");
            }

            close(client_socket);    
            pthread_mutex_lock(&lock);
            current_thread--;
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
            
        } else {
            printf("Undefined poll error: %s\n", strerror(errno));
            close(client_socket);
            
            pthread_mutex_lock(&lock);
            current_thread--;
            pthread_mutex_unlock(&lock);
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

    // Initialize the mutex
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return; 
    } 

    while (1) {
        // Accept an incoming connection
        if ((client_socket_fd = accept(socket_fd, (struct sockaddr *)&serv_addr, (socklen_t *)&address_len)) < 0) {
            // printf("Accept error: %s\n", strerror(errno));
            continue; // keep trying to accept a connection instead of exiting
        }
        setsockopt(client_socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

        // Allocate memory for the client socket argument and pass it to the thread
        int* client_socket_ptr = (int*)malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            printf("Error allocating memory for client_socket\n");
            close(client_socket_fd);
            return;
        }
        *client_socket_ptr = client_socket_fd;

        // Create a thread to handle the request
        pthread_mutex_lock(&lock);
        if (pthread_create(&thread_id[current_thread], NULL, handle_request_thread, (void*)client_socket_ptr) != 0) {
            printf("Thread creation failed: %s\n", strerror(errno));
            close(client_socket_fd);
            free(client_socket_ptr);
            pthread_mutex_unlock(&lock);
            continue;
        }
        current_thread++;
        pthread_mutex_unlock(&lock);
    }
}