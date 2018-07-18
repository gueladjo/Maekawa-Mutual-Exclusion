#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<time.h>
#include<netinet/in.h>
#include<netdb.h>
#include<pthread.h>
#include<semaphore.h>
#include "config.h"

#define BUFFERSIZE 512
#define APP_MSG 'A'
#define MARKER_MSG 'M'
#define CONVERGE_CAST 'C'
#define HALT 'H'

// Mutex
sem_t send_marker;

typedef struct Quorum_Member {
    int id;
    int port;
    char hostname[100];
    int receive_socket; // I'm not sure if this is the right terminology to call this the 'server' socket, but it's the new socket created from the accept() call
    int send_socket;
} Quorum_Member;



void* handle_quorum_member(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);

char * create_vector_msg(int * vector_clk);
int * parse_vector(char * char_vector);
void send_marker_messages(int snapshot_id) ;

void send_msg(int sockfd, char * buffer, int msglen);
int receive_message(char * message, int length);
int compare_timestamps(int * incoming_ts);
int merge_timestamps(int * incoming_ts);

int message_source(char * msg);
int message_dst(char * msg);
char message_type(char * msg);
char * message_payload(char * msg);

void record_snapshot();
void snapshot_channel(char* message);
void activate_node();
void* snapshot_handler();
void output();

// Global parameters
int nb_nodes;
int inter_request_delay;
int cs_execution_time;
int num_requests;

config system_config; 

// Node Paramters
int node_id;
int port;

Quorum_Member* quorum;
int quorum_size;

int main(int argc, char* argv[])
{
    int i, j, k;
    // Config struct filled when config file parsed
    srand(time(NULL));

    read_config_file(&system_config, argv[2]);
    display_config(system_config); 

   /* nb_nodes = system_config.nodes_in_system;
    inter_request_delay = system_config.inter_request_delay;
    cs_execution_time = system_config.cs_execution_time;
    num_requests = system_config.num_requests;

    sscanf(argv[1], "%d", &node_id);

    quorum_size = system_config.quorumSize[node_id];
    port = system_config.portNumbers[node_id];

    // Set up quorum information and initialize vector timestamp
    quorum =  malloc(quorum_size * sizeof(Quorum_Member));
    

    // allocate quorum array
    for (i = 0; i < quorum_size; i++)
    {
        quorum[i].id = system_config.quorum[node_id][i];
        quorum[i].port = system_config.portNumbers[quorum[i].id];
        memmove(quorum[i].hostname, system_config.hostNames[quorum[i].id], 18);
    }

    // Initialize mutex
    if (sem_init(&send_marker, 0, 1) == -1) {
        printf("Error during mutex init.\n");
        exit(1);
    }

    // Client sockets information
    struct hostent* h;

    // Server Socket information
    int s;
    struct sockaddr_in sin;
    struct sockaddr_in sin2;
    struct sockaddr_in pin;
    int addrlen;

    pthread_t tid;
    pthread_attr_t attr;

    // Create TCP server socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error creating socket\n");
        exit(1);
    }

    // Fill in socket with host information
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    printf("PORT : %d\n", port);
    sin.sin_port = htons(port);


    // Reuse port
    int yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        printf("Error changing bind option\n");
        exit(1);
    }

    // Bind socket to address and port number
    if (bind(s, (struct sockaddr*) &sin, sizeof(sin)) == -1) {
        printf("Error on bind call.\n");
        exit(1);
    }

    // Set queuesize of pending connections
    if (listen(s, quorum_size + 10) == -1) {
        printf("Error on listen call\n");
        exit(1);
    }

    // Create client sockets to neighbors of the node
    for (j = 0; j < quorum_size; j++) {
        // Create TCP socket
        if ((quorum[j].send_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("Error creating socket\n");
            exit(1); 
        }

        // Get host info
        if ((h = gethostbyname(quorum[j].hostname)) == 0) {
            printf("Error on gethostbyname\n");
            exit(1);
        }

        // Fill in socket address structure with host info
        memset(&pin, 0, sizeof(pin));
        pin.sin_family = AF_INET;
        pin.sin_addr.s_addr = ((struct in_addr *)(h->h_addr))->s_addr;
        pin.sin_port = htons(quorum[j].port);

        // Connect to port on neighbor
        int connect_return = connect(quorum[j].send_socket, (struct sockaddr *) &pin, sizeof(pin));
        printf("Node %d trying to connect to node %d.\n", node_id, quorum[j].id);
        while (connect_return == -1) {
            connect_return = connect(quorum[j].send_socket, (struct sockaddr *) &pin, sizeof(pin));
            sleep(1);
        }
        printf("Node %d connected to neighbor %d.\n", node_id, quorum[j].id);
    }

    // Create thread for receiving each neighbor messages
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    addrlen = sizeof(sin2);

    i = 0;
    while (i < quorum_size) {
        if ((quorum[i].receive_socket = accept(s, (struct sockaddr *) &sin2, (socklen_t*)&addrlen)) == -1) {
            printf("Error on accept call.\n");
            exit(1);
        }
        pthread_create(&tid, &attr, handle_quorum_member, &(quorum[i].receive_socket));
        i++;
    }*/
}

void* handle_quorum_member(void * arg)
{
    
}

// Function to send whole message
void send_msg(int sockfd, char * buffer, int msglen)
{
    int bytes_to_send = msglen; // |Source | Destination | Protocol ('M') | Length (0)
    buffer[msglen] = '\0';
    printf("MSG SENT: %s \n", buffer);
    while (bytes_to_send > 0)
    {
        bytes_to_send -= send(sockfd, buffer + (msglen - bytes_to_send), msglen, 0);
    }
}

// Message accessor functions, for easy reading
int message_source(char * msg)
{
    int source;
    sscanf(msg, "%2d", &source);
    return source; 
}

int message_dst(char * msg)
{
    int dest;
    sscanf(msg+2, "%2d", &dest);
    return dest; 
}

char message_type(char * msg)
{
    return msg[4];
}

char * message_payload(char * msg)
{
    return msg+5;
}