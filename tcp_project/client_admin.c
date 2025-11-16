#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "172.20.10.12"
#define SERVER_PORT 9999
#define BUFFER_SIZE 4096
#define DOWNLOADS_DIR "downloads"

SOCKET client_socket;
int running = 1;


unsigned __stdcall receive_thread(void *arg) {
    char buffer[BUFFER_SIZE];

    while (running) {
        
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        
        if (bytes_received <= 0) {
            if (running) {
                printf("\n[Connection lost]\n");
                running = 0;
            }
            break;
        }

        buffer[bytes_received] = '\0';
        
        
         if (strncmp(buffer, "DOWNLOAD:", 9) == 0) {
            char *fileinfo = buffer + 9;
            char filename[256];
            char filepath[512];
            long filesize = 0;
            
            
            char *pipe = strchr(fileinfo, '|');
            if (pipe) {
                strncpy(filename, fileinfo, pipe - fileinfo);
                filename[pipe - fileinfo] = '\0';
                filesize = atol(pipe + 1);
            } else {
                strcpy(filename, "downloaded_file");
                filesize = atol(fileinfo);
            }
            
            
            CreateDirectoryA(DOWNLOADS_DIR, NULL);
            

            sprintf(filepath, "%s\\%s", DOWNLOADS_DIR, filename);
            
            printf("\n[Receiving file: %s (%ld bytes)]\n", filename, filesize);

            FILE *file = fopen(filepath, "wb");
            if (!file) {
                printf("Error: Cannot create file '%s'\n", filepath);
                continue;
            }

            long received = 0;
            while (received < filesize) {
                int to_receive = (filesize - received > BUFFER_SIZE) ? BUFFER_SIZE : (filesize - received);
                int chunk = recv(client_socket, buffer, to_receive, 0);
                if (chunk <= 0) break;

                fwrite(buffer, 1, chunk, file);
                received += chunk;
            }

            fclose(file);
        
            char abs_path[512];
            GetFullPathNameA(filepath, sizeof(abs_path), abs_path, NULL);
            
            printf("[Download complete: %s (%ld bytes)]\n", abs_path, received);
            printf("> ");
            fflush(stdout);
        } else {
            printf("\n%s", buffer);
            if (buffer[strlen(buffer) - 1] != '\n') {
                printf("\n");
            }
            printf("> ");
            fflush(stdout);
        }
    }

    return 0;
}

unsigned __stdcall ping_thread(void *arg) {
    while (running) {
        Sleep(60000);
        if (running) {
            send(client_socket, "PING", 4, 0);
        }
    }
    return 0;
}

void print_help() {
    printf("\n=== ADMIN COMMANDS ===\n");
    printf("/list                    - List all files in server directory\n");
    printf("/read <filename>         - Read content of a file\n");
    printf("/upload <filename>       - Upload a file to server\n");
    printf("/download <filename>     - Download a file from server\n");
    printf("/delete <filename>       - Delete a file from server\n");
    printf("/search <keyword>        - Search for files by keyword\n");
    printf("/info <filename>         - Show file information\n");
    printf("/help                    - Show this help\n");
    printf("/quit                    - Exit client\n");
    printf("======================\n\n");
}

void handle_upload(char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file '%s'\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize > BUFFER_SIZE - 256) {
        printf("Error: File too large (max %d bytes)\n", BUFFER_SIZE - 256);
        fclose(file);
        return;
    }

    char *buffer = malloc(BUFFER_SIZE);
    sprintf(buffer, "/upload %s\n", filename);
    size_t offset = strlen(buffer);

    size_t bytes_read = fread(buffer + offset, 1, filesize, file);
    fclose(file);

    send(client_socket, buffer, offset + bytes_read, 0);
    free(buffer);

    printf("Upload request sent...\n");
}

int main() {
    WSADATA wsa;
    struct sockaddr_in server_addr;

    printf("=== TCP CLIENT (ADMIN) ===\n");
    printf("Initializing...\n");

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);


    printf("Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed: %d\n", WSAGetLastError());
        printf("\nMake sure the server is running!\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    printf("Connected successfully!\n\n");

    _beginthreadex(NULL, 0, receive_thread, NULL, 0, NULL);
    _beginthreadex(NULL, 0, ping_thread, NULL, 0, NULL);

    Sleep(500);

    print_help();

    char input[BUFFER_SIZE];

    while (running) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) {
            continue;
        }

        if (strcmp(input, "/help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(input, "/quit") == 0) {
            printf("Disconnecting...\n");
            break;
        }

        if (strncmp(input, "/upload ", 8) == 0) {
            handle_upload(input + 8);
            continue;
        }

        strcat(input, "\n");
        int sent = send(client_socket, input, strlen(input), 0);

        if (sent == SOCKET_ERROR) {
            printf("Send failed: %d\n", WSAGetLastError());
            break;
        }
    }
    running = 0;
    closesocket(client_socket);
    WSACleanup();

    printf("Disconnected.\n");

    return 0;
}
