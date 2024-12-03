#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <arpa/inet.h>

struct sockaddr_in;

// open a socket on the given port and return the socket file descriptor
int open_socket(int port, struct sockaddr_in *addr);

// receive a request from server_socket and creates a new thread to handle it 
void handle_request(int server_socket, int client_socket);

// start the server on the given port and wait for incoming connections
void start_server(const int port, const int max_threads);

#endif
