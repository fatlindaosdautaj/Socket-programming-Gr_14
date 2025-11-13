#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "0.0.0.0"
#define SERVER_PORT 9999
#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define TIMEOUT_SECONDS 300
#define SERVER_DIR "./server_files"

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    char ip[INET_ADDRSTRLEN];
    int is_admin;
    int active;
    time_t last_activity;
    unsigned long messages_received;
    unsigned long bytes_sent;
    unsigned long bytes_received;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
CRITICAL_SECTION cs;
FILE *log_file = NULL;

void init_server_directory() {
    CreateDirectoryA(SERVER_DIR, NULL);
}

void log_message(const char *msg) {
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0';

    printf("[%s] %s\n", timestamp, msg);

    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, msg);
        fflush(log_file);
    }
}