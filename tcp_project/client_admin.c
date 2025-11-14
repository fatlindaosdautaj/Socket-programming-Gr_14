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

//Thread qe pranon mesazhe nga serveri
unsigned __stdcall receive_thread(void *arg) {
    char buffer[BUFFER_SIZE];

    while (running) {
        
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        //nese serveri e mbyll lidhjen
        if (bytes_received <= 0) {
            if (running) {
                printf("\n[Connection lost]\n");
                running = 0;
            }
            break;
        }

        buffer[bytes_received] = '\0';
        
        //kontroli per download
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
            
            // krijon folderin downloads
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
