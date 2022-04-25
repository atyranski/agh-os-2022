#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "printutils.h"
#include "configs.h"
#include <errno.h>
#include <string.h>
#include <fcntl.h>

// ---- Client globals
bool running = true;
int client_queue;
int server_queue;

// requests
Request get_request(int recipent, int expected_type){
    Request request;

    if((msgrcv(recipent, &request, MAX_REQUEST_SIZE, expected_type, 0)) == -1 && running){
        error("COULDNT_RECEIVE_MESSAGE", "msgrcv() couldn't received message");
        printf("Errno: %s\n", strerror(errno));
    }

    return request;
}

void send_request(int recipent_id, Command type, const char *content){
    Request request;

    request.type = type;
    request.sender_id = client_queue;
    request.recipent_id = recipent_id;
    strcpy(request.content, content);


    if(msgsnd(server_queue, &request, MAX_REQUEST_SIZE, 0) == -1){
        error("COULDNT_SEND_MESSAGE", "msgsnd() couldn't send message");
        printf("Errno: %s\n", strerror(errno));
    }
}

// client actions
void action_connect(int client_id, const char *content){
    if(strcmp(content, "full") == 0){
        error("SERVER_IS_FULL", "server is full and cannot connect another client");
        running = false;
        return;
    }

    if(strcmp(content, "ok") == 0){
        char message[100];
        sprintf(message, "successfully connected to the server and received id#%d", client_id);
        printInfo("CONNECT", message);

        display_commands();
        return;
    }

    error("UNEXPECTED_VALUE", "client received an unexpected value of response");
}

void action_stop(){
    printf("\n");
    printOper("STOP", "stopping work");

    send_request(-1, STOP, "");

    if((msgctl(client_queue, IPC_RMID, NULL)) == -1){
        error("COULDNT_REMOVE_QUEUE", "msgctl() couldn't remove queue");
        printf("Errno: %s\n", strerror(errno));
    }

    printInfo("STOP", "successfully disconnected from the server");

    running = false;
}

void action_list(){

}

void get_command(){
    char *line, *start, *end, *command;
    int length;
    size_t len = 0;
    ssize_t line_size = 0;

    display_prompt();
    line_size = getline(&line, &len, stdin);

    start = line;
    while(*line != 0 && *line != ' ') line++;
    end = line;
    length = end - start - 1;
    command = calloc(length, sizeof(char));

    for(int i=0; i<length; i++){
        command[i] = *(start + i);
    }

    if(strcmp(command, "LIST") == 0) {
        send_request(server_queue, LIST, "");

        Request request = get_request(client_queue, LIST);

        printf("%s", request.content);
    }

    if(strcmp(command, "STOP") == 0) {
        exit(0);
    }

    if(strcmp(command, "ALL") == 0) {
        printf("Commencing: LIST\n");
    }

    if(strcmp(command, "ONE") == 0) {
        printf("Commencing: STOP\n");
    }

    if(strcmp(command, "REFRESH") == 0) {
        printf("Commencing: STOP\n");
    }
}

// initialization
int connect_server(){
    key_t key;
    int queue_id;

    if((key = ftok(QUEUE_PATH, PROJ_ID)) == (key_t) -1){
        error("COULDNT_CREATE_KEY", "ftok() couldn't generate new key");
        printf("Errno: %s\n", strerror(errno));
        return RETURN_COULDNT_CREATE_KEY;
    }

    if((queue_id = msgget(key, IPC_EXCL | 0666)) == -1){
        error("COULDNT_OPEN_QUEUE", "msgget() couldn't open server queue");
        return RETURN_COULDNT_OPEN_QUEUE;
    }

    return queue_id;
}

int initialize(){
    char message[100];
    char id_to_char[4];

    printInfo("INITIALIZATION", "initializing client application");
    
    if((client_queue = msgget(IPC_PRIVATE, 0666)) == NULL) {
        error("COULDNT_CREATE_CLIENT", "msgget() couldn't create queue for new client");
        printf("Errno: %s\n", strerror(errno));
        return NULL;
    }

    if((server_queue = connect_server()) < 0) return server_queue;

    sprintf(message, "reached server and created queue with id#%d", client_queue);
    printOper("CONNECT", message);

    send_request(-1, CONNECT, "");

    Request response = get_request(client_queue, CONNECT);

    action_connect(response.recipent_id, response.content);

    return RETURN_SUCCESS;
}

// other
void display_commands(){
    printf("Available commands:\n");
    printf("LIST\t\t\t-listing currently connected clients\n");
    printf("ALL [content]\t\t-sending message with 'content' to all connected clients\n");
    printf("ONE [id] [content]\t-sending message with 'content' to client with provided 'id'\n");
    printf("STOP\t\t\t-stopping work and disconnecting from the server\n");
}

void display_prompt(){
    printf("\033[0;34mAction:\033[0m ");
}

// ---- Main program
int main(int argc, char **argv){
    signal(SIGINT, action_stop);
    atexit(action_stop);

    int init_result;
    char message[100];

    if((init_result = initialize()) != 0) return init_result;

    while(running) get_command();

    // while(running){
    //     display_prompt();
    //     Request request = get_request(server_queue, EVERY_REQUEST_TYPE);

    //     if(&(request) != NULL && running){
    //         printf("sender_id: %d | recipent_id: %d | type: %d | content: %d \n", request.sender_id, request.recipent_id, request.type, request.content);

    //         switch(request.type){
    //             case ALL:
    //                 break;

    //             case ONE:
    //                 break;

    //             default:
    //                 sprintf(message, "client received a message with incorrect type from client id#%d", request.sender_id, request.recipent_id);
    //                 error("INCORRECT_MESSAGE_TYPE", message);
    //         }
    //     }
    // }

    return RETURN_SUCCESS;
}