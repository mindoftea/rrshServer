// Server code for the Restricted Remote Shell.
// Project 4 for CS485G:001, Fall 2015, Professor Griffioen
// Written by Emory Hufbauer, November 27th — December 1st, 2015

#include "rrshServer.h"

int main(int argc, char* argv[]) {
    // Check that the user supplied a valid port:
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        fflush(stderr);
        dieWell(1);
    }
    port = atoi(argv[1]);
    if(port < 0 || port > 65535) {
        fprintf(stderr, "Port %d is out of range.", port);
        fflush(stderr);
        dieWell(2);
    }

    // Load the commands and users files into their respective linked lists:
    FILE* cmdfile = fopen("rrshcommands.txt", "r");
    FILE* usrfile = fopen("rrshusers.txt", "r");
    if (cmdfile == 0) {
        fprintf(stderr, "Cannot open 'rrshcommands.txt'.\n");
        fflush(stderr);
        dieWell(3);
    }
    if (usrfile == 0) {
        fprintf(stderr, "Cannot open 'rrshusers.txt'.\n");
        fflush(stderr);
        dieWell(4);
    }
    char command[64];
    while(fscanf(cmdfile, "%s", command) == 1) {
        struct commandNode* p = malloc(sizeof(struct commandNode));
        if(p == 0) {
            fprintf(stderr, "Fatal error: out of memory.\n");
            fflush(stderr);
            dieWell(5);
        }
        strcpy(p->name, command);
        p->next = commands;
        commands = p;
    }
    char username[64];
    char password[64];
    while(fscanf(usrfile, "%s %s", username, password) == 2) {
        struct userNode* p = malloc(sizeof(struct userNode));
        if(p == 0) {
            fprintf(stderr, "Fatal error: out of memory.\n");
            fflush(stderr);
            dieWell(6);
        }
        strcpy(p->username, username);
        strcpy(p->password, password);
        p->next = users;
        users = p;
    }
    fclose(cmdfile);
    fclose(usrfile);

    // Initialize the network connection:
    listenSocket = sockListen(port);
    if(listenSocket < 0) {
        dieWell(7);
    } else {
        fprintf(stdout, "Listening on port %d.\n\n", port);
        fflush(stdout);
    }

    // Free the commands and users linked lists if the user quits:
    signal(SIGINT, dieWell);
    signal(SIGTERM, dieWell);

    // Ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // Handle new clients as they connect:
    char address[20];
    int connectSocket = 0;
    while((connectSocket = sockAccept(listenSocket, address))) {
        // // To handle multiple clients simultaneously:
        // if(!fork()) {
        //     handleClient(connectSocket, address);
        //     close(connectSocket);
        //     exit(0);
        // }
        // close(connectSocket);
        handleClient(connectSocket, address);
        close(connectSocket);
    }

    // This should only execute if the network connection broke:
    dieWell(0);
    return 0;
}

// Prepare a listening socket on a given port:
int sockListen(int port) {
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    // Connection settings:
    // Use internet protocols:
    serverAddress.sin_family = AF_INET;
    // Set the port, making sure to use network byte order:
    serverAddress.sin_port = htons(port);
    // Allow connections from any IP address:
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // Create a listening socket, bind it to the network
    //  settings object, and listen on it:
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (listenSocket < 0) {
        fprintf(stderr, "Could not open listening socket.\n");
        fflush(stderr);
        return -1;
    }
    int optval;
    if(setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
        fprintf(stderr, "Could not set listening socket options.\n");
        fflush(stderr);
        return -2;
    }
    if(bind(listenSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "Could not bind listening socket.\n");
        fflush(stderr);
        return -3;
    }
    if(listen(listenSocket, 1) < 0) {
        fprintf(stderr, "Could not listen for clients.\n");
        fflush(stderr);
        return -4;
    }
    return listenSocket;
}

// Accept a connection on a given listening socket:
int sockAccept(int listenSocket, char* address) {
    socklen_t socksize = sizeof(struct sockaddr);
    struct sockaddr_in client;
    int connectSocket = accept(listenSocket, (struct sockaddr *) &client, &socksize);
    // Write the client;s address to the given buffer:
    strcpy(address, inet_ntoa(client.sin_addr));
    if(connectSocket <= 0) {
        fprintf(stderr, "Could not connect to %s.\n", address);
        fflush(stderr);
        return -1;
    }
    return connectSocket;
}

// Handle client interaction:
void handleClient(int connectSocket, char* address) {
    // Cache the current working directory so we can return to it if the client cd's:
    char dirBuf[128];
    char* directory;
    directory = getcwd(dirBuf, 128);

    char buffer[MAXBUF + 1];
    int len;

    // Recieve the client's username:
    len = recv(connectSocket, buffer, MAXBUF, 0);
    char username[64];
    trim(buffer, username, len);
    fprintf(stdout, "\nUser %s connecting from %s on TCP port %d.\n", username, address, port);
    fflush(stdout);

    // Recieve the client's password:
    len = recv(connectSocket, buffer, MAXBUF, 0);
    char password[64];
    trim(buffer, password, len);

    // Check that the client's login is valid:
    if(!checkLogin(username, password)) {
        // Lag on login failure to slow brute-force guessing:
        sleep(1);
        char* message = "Login Failed\n";
        send(connectSocket, message, strlen(message), 0);
        fprintf(stdout, "User %s denied access.\n", username);
        fprintf(stdout, "User %s disconnected.\n\n", username);
        fflush(stdout);
        return;
    } else {
        char* message = "Login Approved\n";
        send(connectSocket, message, strlen(message), 0);
        fprintf(stdout, "User %s successfully logged in.\n", username);
        fflush(stdout);
    }

    fprintf(stdout, "\n");
    fflush(stdout);
    int failCount = 0;

    // Repeatedly wait for the client to send commands, executing them when they arrive.
    while(1) {
        char* argv[MAXARGS];

        // Read a line from the client:
        len = recv(connectSocket, buffer, MAXBUF, 0);

        // Trim trailing newlines from the buffer:
        trim(buffer, buffer, len);

        // If the string is empty once the and is trimmed,
        //  ignore it. If 100 such commands are received, quit.
        if(strlen(buffer) < 1) {
            char* message = "RRSH COMMAND COMPLETED\n";
            if(send(connectSocket, message, strlen(message), 0) < 0) {
                break;
            }
            if(failCount++ < 100) {
                continue;
            } else break;
        };
        failCount = 0;

        // Parse the buffer into argv.
        int argc = parseline(buffer, argv);

        // If the command is all whitespace, ignore it.
        if(argc == 0) {
            char* message = "RRSH COMMAND COMPLETED\n";
            send(connectSocket, message, strlen(message), 0);
            continue;
        }
        failCount = 0;

        // Handle builtin commands 'quit' and 'cd':
        if(!strcmp(argv[0], "quit")) {
            break;
        } else if(!strcmp(argv[0], "cd")) {
            if(argc == 2 && chdir(argv[1]) == 0) {
                fprintf(stdout, "User %s changed to the directory '%s'.\n\n", username, argv[1]);
            } else {
                fprintf(stdout, "\tFailed to change directory!\n");
            }
            fflush(stdout);
            char* message = "RRSH COMMAND COMPLETED\n\0";
            send(connectSocket, message, strlen(message), 0);
            continue;
        }

        fprintf(stdout, "User %s sent the command '%s' to be executed.\n", username, argv[0]);
        fflush(stdout);

        // Check if the command is allowed:
        if(!checkCommand(argv[0])) {
            // If not, alert the client, and log it on the server:
            char* message = "Cannot execute '";
            send(connectSocket, message, strlen(message), 0);
            message = argv[0];
            send(connectSocket, message, strlen(message), 0);
            message = "' on this server\n";
            send(connectSocket, message, strlen(message), 0);
            fprintf(stdout, "The command '%s' is not allowed on this server.\n", argv[0]);
            fflush(stdout);
        } else {
            // If so, log the execution on the server:
            fprintf(stdout, "Now executing '%s' for %s.\n", argv[0], username);
            fflush(stdout);
            // Fork and exec the given command, duping stdin, stdout, and stderr to the client:
            pid_t pid = fork();
            if(!pid) {
                dup2(connectSocket, STDOUT_FILENO);
                dup2(connectSocket, STDERR_FILENO);
                if (execve(argv[0], argv, 0) < 0) {
                    fprintf(stdout, "'%s': Command not found.\n", argv[0]);
                    fflush(stdout);
                }
                exit(0);
            } else {
                // The main process waits for the child to complete before continuing:
                waitpid(pid, 0, 0);
            }
        }

        char* message = "RRSH COMMAND COMPLETED\n";
        send(connectSocket, message, strlen(message), 0);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    // When the IO loop exits, log the client's disconnection on the server:
    fprintf(stdout, "User %s disconnected.\n\n", username);
    fflush(stdout);

    // CD back to the original working directory:
    if(chdir(directory) != 0) {
        fprintf(stdout, "Failed to return to home directory.\n");
        fflush(stdout);
    }
}

// Verify that the given username and password are in the users linked-list:
int checkLogin(char* username, char* password) {
    struct userNode* p = users;
    while(p) {
        if(!strcmp(p->username, username) && !strcmp(p->password, password)) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

// Verify that given command is in the commands linked-list:
int checkCommand(char* command) {
    struct commandNode* p = commands;
    while(p) {
        if(!strcmp(p->name, command)) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

// Copy str1 into str2, up to len chars or the first
//  newline or NUL, returning the number of chars copied:
int trim(char* str1, char* str2, int len) {
    int k = 0;
    while(1) {
        if(k >= len) {
            k++;
            break;
        }
        char c = str1[k++];
        if(
            c == '\0' ||
            c == '\n' ||
            c == '\r'
        ) break;
    }
    str2[--k] = '\0';
    if(k == 0) return 0;
    int i = k;
    while(i--) {
        str2[i] = str1[i];
    }
    return k;
}

// Split the given line into an argument array:
int parseline(char* cmdline, char* argv[]) {
    int argc = 0;
    int word = 0;

    char* p = cmdline;
    char* q = cmdline;

    // Read through to the end of the line, with p pointing
    //  to each character and q pointing to the beginning of
    //  each word. When a word ends, add q it to the array and
    //  then advance q to the next word. Replace spaces with
    //  NULs to separate the words. Ignore consecutive spaces.
    while(*p != 0) {
        if(*p == ' ') {
            *p = 0;
            if(word) {
                argv[argc++] = q;
                word = 0;
            }
            q = p + 1;
        } else {
            word = 1;
        }
        p++;
    }

    // Add the last word to the array, if necessary:
    if(word) {
        argv[argc++] = q;
    }

    // NULL-terminate the array:
    argv[argc] = 0;

    return argc;
}

// Free the commands and users linked-lists,
//  and then exit with the given status:
void dieWell(int stat) {
    close(listenSocket);
    struct commandNode* pc = commands;
    while(pc) {
        struct commandNode* q = pc->next;
        free(pc);
        pc = q;
    }
    struct userNode* pu = users;
    while(pu) {
        struct userNode* q = pu->next;
        free(pu);
        pu = q;
    }
    exit(stat);
}
