#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "172.20.10.2"
#define SERVER_PORT 9999
#define BUFFER_SIZE 4096

SOCKET client_socket;
int running = 1;

unsigned __stdcall receive_thread(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (running)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0)
        {
            if (running)
            {
                printf("\n[Connection lost]\n");
                running = 0;
            }
            break;
        }

        buffer[bytes_received] = '\0';

        printf("\n%s", buffer);
        if (buffer[strlen(buffer) - 1] != '\n')
        {
            printf("\n");
        }
        printf("> ");
        fflush(stdout);
    }

    return 0;
}

unsigned __stdcall ping_thread(void *arg)
{
    while (running)
    {
        Sleep(60000);
        if (running)
        {
            send(client_socket, "PING", 4, 0);
        }
    }
    return 0;
}

void print_help()
{
    printf("\n=== USER COMMANDS ===\n");
    printf("/read <filename>    - Read content of a file (slower than admin)\n");
    printf("/help               - Show this help\n");
    printf("/quit               - Exit client\n");
    printf("Note: You can send any text message to the server\n");
    printf("=====================\n\n");
}

int main()
{
    WSADATA wsa;
    struct sockaddr_in server_addr;

    printf("=== TCP CLIENT (USER) ===\n");
    printf("Initializing...\n");

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET)
    {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    printf("Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        printf("Connection failed: %d\n", WSAGetLastError());
        printf("\nMake sure the server is running!\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    printf("Connected successfully!\n\n");

    char username[256];
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;
    
    char login_msg[512];
    sprintf(login_msg, "USERNAME:%s\n", username);
    send(client_socket, login_msg, strlen(login_msg), 0);

    _beginthreadex(NULL, 0, receive_thread, NULL, 0, NULL);
    _beginthreadex(NULL, 0, ping_thread, NULL, 0, NULL);

    Sleep(500);

    print_help();

    char input[BUFFER_SIZE];

    while (running)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0)
        {
            continue;
        }

        if (strcmp(input, "/help") == 0)
        {
            print_help();
            continue;
        }

        if (strcmp(input, "/quit") == 0)
        {
            printf("Disconnecting...\n");
            break;
        }

        strcat(input, "\n");
        int sent = send(client_socket, input, strlen(input), 0);

        if (sent == SOCKET_ERROR)
        {
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
