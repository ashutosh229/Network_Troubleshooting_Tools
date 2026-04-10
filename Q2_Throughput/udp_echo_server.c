#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_type;
#define CLOSESOCKET closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef socklen_t socklen_type;
#define CLOSESOCKET close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef int SOCKET;
#endif

static void cleanup_and_exit(SOCKET sock, int code) {
    if (sock != INVALID_SOCKET) {
        CLOSESOCKET(sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    exit(code);
}

int main(int argc, char *argv[]) {
    int port = 9000;
    int buffer_size = 1024;
    SOCKET server_socket = INVALID_SOCKET;
    struct sockaddr_in server_addr;
    char *buffer = NULL;

    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        buffer_size = atoi(argv[2]);
    }
    if (port <= 0 || buffer_size <= 0) {
        fprintf(stderr, "Usage: %s [port] [buffer_size]\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    buffer = (char *)malloc((size_t)buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate buffer\n");
        cleanup_and_exit(server_socket, 1);
    }

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create UDP socket\n");
        free(buffer);
        cleanup_and_exit(server_socket, 1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Failed to bind on port %d\n", port);
        free(buffer);
        cleanup_and_exit(server_socket, 1);
    }

    printf("UDP echo server listening on port %d with buffer size %d bytes\n", port, buffer_size);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_type client_len = (socklen_type)sizeof(client_addr);
        int received = recvfrom(
            server_socket,
            buffer,
            buffer_size,
            0,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (received == SOCKET_ERROR) {
            continue;
        }

        sendto(
            server_socket,
            buffer,
            received,
            0,
            (struct sockaddr *)&client_addr,
            client_len
        );
    }

    free(buffer);
    cleanup_and_exit(server_socket, 0);
    return 0;
}
