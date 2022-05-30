#include "configs.h"

int port;
char *socket_path;

int descriptor;
Server *server;
int waiting_for_enemy = -1;
pthread_mutex_t mutex_client_register = PTHREAD_MUTEX_INITIALIZER;


// Utilities
int getFreeRegisterSpot(Server *server){
    for(int i=0; i<MAX_CLIENTS_REGISTERED; i++){
        if(server->clients[i] == NULL) return i;
    }
    return -1;
}

int getFreeGame(Game **games){
    for(int i=0; i<MAX_CLIENTS_REGISTERED/2; i++){
        if(games[i] == NULL) return i;
    }
    return -1;
}

bool clientExists(Server *server, char *nickname){
    for(int i=0; i<server->clients_amount; i++){
        if(strcmp(server->clients[i]->nick, nickname) == 0) return true;
    }
    
    return false;
}


// Server Initialization
int createLocalServer(char *socket_path){
    int descriptor;
    
    if ((descriptor = socket(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        error("ERROR", "local socket descriptor");
        exit(1);
    }

    struct sockaddr_un address;
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, socket_path);

    if(bind(descriptor, (struct sockaddr*) &address, sizeof(address)) == -1){
        error("ERROR", "local server binding problem");
        exit(1);
    }

    if(remove(socket_path) == -1 && errno != ENOENT){
        error("ERROR", "removing existing local socket problem");
        exit(1);
    }

    return descriptor;
}

int createOnlineServer(int port){
    int descriptor;
    
    if ((descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        error("ERROR", "local socket descriptor");
        exit(1);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(descriptor, (struct sockaddr*) &address, sizeof(address)) == -1){
        error("ERROR", "online server binding problem");
        exit(1);
    }

    return descriptor;
}

Server *createServer(int port, char *socket_path){
    int local_descriptor = createLocalServer(socket_path);
    int online_descriptor = createOnlineServer(port);
    int epoll;

    Server *server = calloc(1, sizeof(Server));
    server->local = local_descriptor;
    server->online = online_descriptor;
    server->clients = calloc(MAX_CLIENTS_REGISTERED, sizeof(Client));
    server->clients_amount = 0;
    server->games = calloc(MAX_CLIENTS_REGISTERED/2, sizeof(Client));
    server->games_amount = 0;

    return server;
}


// Server Actions
void *handleRegister(void *arguments){
    printInfo("SERVER", "stargin register thread");

    listen(server->local, 10);
    liste(server->online, 10);

    while(true){
        int client_descriptor, nickname_length;
        char nickname[CLIENT_NICK_LENGTH];
        ConnectionType connection_type = CONN_NONE;

        if((client_descriptor = accept(server->local, NULL, NULL)) != -1){
            printOper("REGISTER", "connection type: local");
            connection_type = CONN_LOCAL;
        }

        if((client_descriptor = accept(server->online, NULL, NULL)) != -1){
            printOper("REGISTER", "connection type: online");
            connection_type = CONN_ONLINE;
        }

        if(connection_type == CONN_NONE){
            error("CONNECTION_ERROR", "server received incorrect type of connection");
            continue;
        }

        if(server->clients_amount == MAX_CLIENTS_REGISTERED){
            error("SERVER_FULL", "cannot register any other client");
            send(client_descriptor, "SERVER_FULL", strlen("SERVER_FULL") + 1, MSG_DONTWAIT);
            shutdown(client_descriptor, SHUT_RDWR);
            close(client_descriptor);
            continue;
        }

        Client *client = calloc(1, sizeof(Client *));
        client->descriptor = client_descriptor;
        client->connection = connection_type;
        nickname_length = read(client_descriptor, client->nick, CLIENT_NICK_LENGTH);
        client->nick[nickname_length] = 0;

        pthread_mutex_lock(&mutex_client_register);

        if(!clientExists(server->clients, client->nick)){
            send(client_descriptor, "NICKNAME_TAKEN", strlen("NICKNAME_TAKEN") + 1, MSG_DONTWAIT);
            shutdown(client_descriptor, SHUT_RDWR);
            close(client_descriptor);
            free(client);
            continue;
        }
        
        registerClient(server, client);
        send(client_descriptor, "ACCEPTED", strlen("ACCEPTED") + 1, MSG_DONTWAIT);

        char message[100];

        sprintf(message, "client with nickname %s registered", client->nick);
        printInfo("SUCCESS", message);

        pthread_mutex_unlock(&mutex_client_register);
    }    
}

void *handleGame(void *arguments){

}


void shutdownServer(Server *server){
    printInfo("SERVER", "shutdown");
    close(server->local);
    close(server->online);
    free(server->clients);
    free(server->games);
    free(server);
}

bool registerClient(Server *server, Client *client){
    struct sockaddr address;
    client->address = address;
    client->id = getFreeRegisterSpot(server);

    server->clients[client->id] = client;    

    return true;
}

// ---- Main program
int main(int argc, char **argv){

    // Validation of arguments
    if(argc != 3){
        error("INCORRECT_ARGUMENTS", "./bin/main [-port-] [-socket path-]");
        exit(1);
    }

    port = atoi(argv[1]);
    socket_path = argv[2];

    server = createServer(port, socket_path);

    pthread_t register_thread;
    pthread_t games_thread;

    pthread_create(&register_thread, NULL, handleRegister, NULL);
    pthread_create(&games_thread, NULL, handleGame, NULL);

    pthread_join(register_thread, NULL);
    pthread_join(games_thread, NULL);

    shutdownServer(server);

    return RETURN_SUCCESS;
}