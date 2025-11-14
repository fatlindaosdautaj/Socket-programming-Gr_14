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
