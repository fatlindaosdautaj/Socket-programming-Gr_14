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

void handle_delete_command(int client_index, char *filename, char *response) {
    char filepath[512];
    sprintf(filepath, "%s\\%s", SERVER_DIR, filename);

    if (DeleteFileA(filepath)) {
        sprintf(response, "File '%s' deleted successfully\n", filename);
    } else {
        sprintf(response, "Error: Cannot delete file '%s'\n", filename);
    }
}

void handle_search_command(int client_index, char *keyword, char *response) {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[512];
    int found = 0;

    sprintf(search_path, "%s\\*", SERVER_DIR);
    sprintf(response, "Files matching '%s':\n", keyword);

    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                if (strstr(find_data.cFileName, keyword) != NULL) {
                    strcat(response, "  - ");
                    strcat(response, find_data.cFileName);
                    strcat(response, "\n");
                    found = 1;
                }
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }

    if (!found) {
        strcat(response, "  No files found\n");
    }
}

unsigned __stdcall handle_client(void *arg) {
    int client_index = *(int *)arg;
    free(arg);

    SOCKET client_socket = clients[client_index].socket;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    char welcome[256];
    sprintf(welcome, "Welcome! You are connected as %s\n",
            clients[client_index].is_admin ? "ADMIN" : "USER");
    send(client_socket, welcome, strlen(welcome), 0);
    clients[client_index].bytes_sent += strlen(welcome);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
            break;
        }

        EnterCriticalSection(&cs);
        clients[client_index].last_activity = time(NULL);
        clients[client_index].messages_received++;
        clients[client_index].bytes_received += bytes_received;
        LeaveCriticalSection(&cs);

        buffer[bytes_received] = '\0';

        if (buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }

        char log_msg[512];
        sprintf(log_msg, "Client %s (%s): %s", clients[client_index].ip,
                clients[client_index].is_admin ? "Admin" : "User", buffer);
        log_message(log_msg);

        memset(response, 0, BUFFER_SIZE);

        if (strcmp(buffer, "PING") == 0) {
            strcpy(response, "PONG");
        }
        else if (strncmp(buffer, "/list", 5) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
            } else {
                Sleep(50);
                handle_list_command(client_index, response);
            }
        }
        else if (strncmp(buffer, "/read ", 6) == 0) {
            int delay = clients[client_index].is_admin ? 50 : 200;
            Sleep(delay);
            handle_read_command(client_index, buffer + 6, response);
        }
        else if (strncmp(buffer, "/upload ", 8) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
            } else {
                Sleep(50);
                handle_upload_command(client_index, buffer + 8, response);
            }
        }
        else if (strncmp(buffer, "/download ", 10) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
                send(client_socket, response, strlen(response), 0);
            } else {
                Sleep(50);
                handle_download_command(client_index, buffer + 10, client_socket);
                continue;
            }
        }
        else if (strncmp(buffer, "/delete ", 8) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
            } else {
                Sleep(50);
                handle_delete_command(client_index, buffer + 8, response);
            }
        }
        else if (strncmp(buffer, "/search ", 8) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
            } else {
                Sleep(50);
                handle_search_command(client_index, buffer + 8, response);
            }
        }
        else if (strncmp(buffer, "/info ", 6) == 0) {
            if (!clients[client_index].is_admin) {
                strcpy(response, "Error: Permission denied (admin only)\n");
            } else {
                Sleep(50);
                handle_info_command(client_index, buffer + 6, response);
            }
        }
        else {
            sprintf(response, "Echo: %s\n", buffer);
        }

        if (strlen(response) > 0) {
            send(client_socket, response, strlen(response), 0);
            clients[client_index].bytes_sent += strlen(response);
        }
    }

    EnterCriticalSection(&cs);
    clients[client_index].active = 0;
    client_count--;
    LeaveCriticalSection(&cs);

    closesocket(client_socket);

    sprintf(response, "Client %s disconnected", clients[client_index].ip);
    log_message(response);

    return 0;
}
unsigned __stdcall monitor_thread(void *arg) {
    while (1) {
        Sleep(10000);

        time_t now = time(NULL);

        EnterCriticalSection(&cs);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                if (difftime(now, clients[i].last_activity) > TIMEOUT_SECONDS) {
                    char msg[256];
                    sprintf(msg, "Client %s timed out", clients[i].ip);
                    log_message(msg);

                    closesocket(clients[i].socket);
                    clients[i].active = 0;
                    client_count--;
                }
            }
        }
        LeaveCriticalSection(&cs);

        write_stats();
    }
    return 0;
}

unsigned __stdcall command_thread(void *arg) {
    char cmd[256];
    while (1) {
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "STATS") == 0) {
            print_stats();
        } else if (strcmp(cmd, "QUIT") == 0) {
            printf("Shutting down server...\n");
            exit(0);
        } else if (strlen(cmd) > 0) {
            printf("Unknown command. Available: STATS, QUIT\n");
        }
    }
    return 0;
}