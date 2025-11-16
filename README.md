# TCP Socket Programming

[![C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Windows](https://img.shields.io/badge/Platform-Windows-orange.svg)](https://www.microsoft.com/windows)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This repository contains a TCP-based file server and two client programs (admin and user) implemented in C for Windows using Winsock. It was developed as the second project for the Computer Networks course. The implementation matches the code in the repository: a server that accepts multiple clients, assigns admin privileges to the first connected client, enforces per-client permissions, supports file operations, maintains logs and periodic stats, and implements timeouts and a simple keepalive.

## Developed by
- erzaduraku ([@erzaduraku](https://github.com/erzaduraku))
- edonitagashi ([@edonitagashi](https://github.com/edonitagashi))
- engjiosmani ([@engjiosmani](https://github.com/engjiosmani))
- fatlindaosdautaj ([@fatlindaosdautaj](https://github.com/fatlindaosdautaj))

## Project overview

- server: TCP server that hosts a file repository, accepts up to MAX_CLIENTS connections, assigns the first connected client as Admin, and logs activity and statistics.
- client_admin: Admin client with full privileges — /list, /read, /upload, /download, /delete, /search, /info.
- client_user: Regular client with limited privileges — /read and general text messages; prompted for USERNAME at connect.

## Key features

Server
- Configurable IP and port via macros in source code.
- Accepts up to MAX_CLIENTS simultaneous connections; rejects new connections when full.
- First client that connects becomes Admin (full privileges); others are Users.
- Supported commands: /list, /read <filename>, /upload <filename>, /download <filename>, /delete <filename>, /search <keyword>, /info <filename>.
- Per-client permissions enforced (admin-only commands blocked for users).
- Monitoring and logging:
  - active connections count
  - active client IPs
  - number of messages received per client
  - bytes sent/received per client
- Logs appended to server_log.txt and periodic snapshots written to server_stats.txt.
- Automatic disconnection of idle clients after TIMEOUT_SECONDS.
- PING/PONG keepalive support from clients.

Clients
- Use Winsock to connect to the server.
- Admin client handles file upload and download; downloads are saved to a local downloads\ folder.
- User client sends USERNAME:<name> on connect and has slower read response to reflect lower priority.
- Background receive thread and ping thread present in both clients.

## Repository structure

```text
tcp_project/
├── server.c                 # TCP server (Winsock)
├── client_admin.c           # Admin client
├── client_user.c            # User client
├── server_files/            # Server directory with files served
│   ├── server.txt
│   ├── prova2.txt
│   └── ...
├── downloads/               # client_admin downloads folder (created at runtime)
├── server_log.txt           # server log (appended)
├── server_stats.txt         # periodic server statistics snapshot
└── README.md                # this file
```

## Requirements
- Windows OS (Winsock and Windows filesystem APIs used)
- C compiler that links ws2_32 (gcc/MinGW or Visual Studio)
- Network: at least 4 different devices on the same network recommended for full testing (per assignment)

## Build & run

The commands used to build and run this project are:

```bash
gcc server.c -o server.exe -lws2_32
gcc client_admin.c -o client_admin.exe -lws2_32
gcc client_user.c -o client_user.exe -lws2_32
```

Run (on server machine):
```
.\server.exe
```

Run clients (on client machines / consoles):
```
.\client_admin.exe
.\client_user.exe
```

## Usage — commands & examples

Admin commands (client_admin)
- /list — list files in server directory
- /read <filename> — read file contents
- /upload <filename> — uploads a local file to the server (admin client reads the file and sends content)
- /download <filename> — server streams file (header: DOWNLOAD:<name>|<size>\n followed by raw bytes); client saves to downloads\
- /delete <filename> — delete file on server
- /search <keyword> — search filenames by keyword
- /info <filename> — show size and timestamps
- /help — show help
- /quit — disconnect

User commands (client_user)
- /read <filename> — read file (slower than admin)
- /help — show help
- /quit — disconnect
- On connect user is prompted for a username and sends USERNAME:<name> to server.

Server console commands
- STATS — print current server statistics to the server terminal
- QUIT — shut down the server

Monitoring & logs
- server_log.txt — timestamped events and client actions
- server_stats.txt — periodically updated snapshot with timestamp, active connections, per-client messages and bytes

## Screenshots 
<img width="1781" height="351" alt="server 1" src="https://github.com/user-attachments/assets/a552c3bf-7a76-4ff9-87d8-a7f36356c989" />
<img width="1482" height="319" alt="server 2" src="https://github.com/user-attachments/assets/3c5f1c9f-6a47-4f44-8724-5fd9936c887c" />
<img width="945" height="208" alt="server 3" src="https://github.com/user-attachments/assets/16a81696-e64d-406f-8e08-5181c523a328" />
<img width="1499" height="670" alt="admin 1" src="https://github.com/user-attachments/assets/d1863046-873a-43b7-99cf-894566a53112" />
<img width="1354" height="516" alt="admin 2" src="https://github.com/user-attachments/assets/39a8730d-0d49-4dc1-a2c0-ff1965980800" />
<img width="1339" height="795" alt="admin 3" src="https://github.com/user-attachments/assets/4cc8e193-ee3c-4f09-8d09-e1cc1ecdd0da" />
<img width="1672" height="922" alt="user" src="https://github.com/user-attachments/assets/4c6599ca-2715-426c-8481-f118a767d595" />


## Quick walkthrough
1. Start server on machine A with .\server.exe
2. Start admin client on machine B with .\client_admin.exe (first connector becomes admin)
3. Start user clients on machines C and D with .\client_user.exe
4. From admin: /list, /upload test.txt, /download prova2.txt
5. From users: /read server.txt, send chat messages
6. Check server_log.txt and server_stats.txt

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT)
