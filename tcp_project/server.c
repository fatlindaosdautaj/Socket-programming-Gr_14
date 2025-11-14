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
void write_stats() {
    FILE *stats_file = fopen("server_stats.txt", "w");
    if (!stats_file) return;

    time_t now = time(NULL);
    fprintf(stats_file, "=== SERVER STATISTICS ===\n");
    fprintf(stats_file, "Generated: %s\n", ctime(&now));
    fprintf(stats_file, "Active Connections: %d/%d\n\n", client_count, MAX_CLIENTS);

    EnterCriticalSection(&cs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            fprintf(stats_file, "Client %d:\n", i + 1);
            fprintf(stats_file, "  IP: %s\n", clients[i].ip);
            fprintf(stats_file, "  Type: %s\n", clients[i].is_admin ? "Admin" : "User");
            fprintf(stats_file, "  Messages: %lu\n", clients[i].messages_received);
            fprintf(stats_file, "  Bytes Sent: %lu\n", clients[i].bytes_sent);
            fprintf(stats_file, "  Bytes Received: %lu\n", clients[i].bytes_received);
            fprintf(stats_file, "\n");
        }
    }
    LeaveCriticalSection(&cs);

    fclose(stats_file);
}

void print_stats() {
    printf("\n=== CURRENT STATISTICS ===\n");
    printf("Active Connections: %d/%d\n\n", client_count, MAX_CLIENTS);

    EnterCriticalSection(&cs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            printf("Client %d: %s (%s) - Messages: %lu, Sent: %lu bytes, Recv: %lu bytes\n",
                   i + 1, clients[i].ip,
                   clients[i].is_admin ? "Admin" : "User",
                   clients[i].messages_received,
                   clients[i].bytes_sent,
                   clients[i].bytes_received);
        }
    }
    LeaveCriticalSection(&cs);
    printf("\n");
}

void handle_list_command(int client_index, char *response) {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[512];

    sprintf(search_path, "%s\\*", SERVER_DIR);

    strcpy(response, "Files in server directory:\n");

    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                strcat(response, "  - ");
                strcat(response, find_data.cFileName);
                strcat(response, "\n");
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    } else {
        strcpy(response, "Error: Cannot list directory\n");
    }
}

void handle_read_command(int client_index, char *filename, char *response) {
    char filepath[512];
    sprintf(filepath, "%s\\%s", SERVER_DIR, filename);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        sprintf(response, "Error: Cannot open file '%s'\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize > BUFFER_SIZE - 200) {
        sprintf(response, "Error: File too large (max %d bytes)\n", BUFFER_SIZE - 200);
        fclose(file);
        return;
    }

    sprintf(response, "Content of '%s':\n---\n", filename);
    size_t offset = strlen(response);
    size_t read_bytes = fread(response + offset, 1, filesize, file);
    response[offset + read_bytes] = '\0';
    strcat(response, "\n---\n");

    fclose(file);
}
void handle_upload_command(int client_index, char *data, char *response) {
    char filename[256];
    char *file_content;

    char *newline = strchr(data, '\n');
    if (!newline) {
        strcpy(response, "Error: Invalid upload format\n");
        return;
    }

    size_t filename_len = newline - data;
    strncpy(filename, data, filename_len);
    filename[filename_len] = '\0';

    file_content = newline + 1;

    char filepath[512];
    sprintf(filepath, "%s\\%s", SERVER_DIR, filename);

    FILE *file = fopen(filepath, "wb");
    if (!file) {
        sprintf(response, "Error: Cannot create file '%s'\n", filename);
        return;
    }

    fwrite(file_content, 1, strlen(file_content), file);
    fclose(file);

    sprintf(response, "File '%s' uploaded successfully\n", filename);
}

void handle_download_command(int client_index, char *filename, SOCKET client_socket) {
    char filepath[512];
    sprintf(filepath, "%s\\%s", SERVER_DIR, filename);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        char error_msg[256];
        sprintf(error_msg, "ERROR:Cannot open file '%s'", filename);
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char header[256];
    sprintf(header, "DOWNLOAD:%s|%ld\n", filename, filesize);
    send(client_socket, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
        clients[client_index].bytes_sent += bytes_read;
    }

    fclose(file);
}