#ifndef RRSH_SERVER_H
#define RRSH_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define MAXBUF 256
#define MAXARGS 32

static int port = -1;
static int listenSocket = -1;

// Structs for the commands and users linked-lists:
struct commandNode {
    char name[64];
    struct commandNode* next;
};

struct userNode {
    char username[64];
    char password[64];
    struct userNode* next;
};

// Heads for the commands and users linked-lists:
struct commandNode* commands = 0;
struct userNode* users = 0;

// Function prototypes:
int trim(char* str1, char* str2, int len);
int sockListen(int port);
int sockAccept(int listenSocket, char* address);
int parseline(char* cmdline, char** argv);
void handleClient(int connectSocket, char* address);
int checkLogin(char* username, char* password);
int checkCommand(char* command);
void dieWell(int stat);

#endif
