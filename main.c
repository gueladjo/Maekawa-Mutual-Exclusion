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
#include<math.h>
#include "config.h"

#define BUFFERSIZE 512
#define REQUEST 'R'
#define RELEASE 'L'
#define GRANT 'G'
#define FAILED 'F'
#define INQUIRE 'I'
#define YIELD 'Y'

typedef struct Quorum_Member {
    int id;
    int port;
    int lock_held;
    char hostname[100];
    int receive_socket;
    int send_socket;
} Quorum_Member;

// Semaphore
sem_t enter_request;
sem_t request_grant;
sem_t execution_end;
sem_t wait_grant;

// Lamport clock
int timestamp = 0;

void cs_enter();
void cs_leave();

void* mutual_exclusion_handler(); 
void maekawa_protocol_release();
void maekawa_protocol_request();

int exponential_rand(int mean);

void* handle_quorum_member(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);

void send_msg(int sockfd, char * buffer, int msglen);
int receive_message(char * message, int length);
int merge_timestamps(int * incoming_ts);

int message_source(char * msg);
int message_dst(char * msg);
char message_type(char * msg);
int message_ts(char * msg);

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
int lock_holder;
int lock_received = 0;

int main(int argc, char* argv[])
{
    int i, j, k;
    // Config struct filled when config file parsed
    srand(time(NULL));

    read_config_file(&system_config, argv[2]);
    display_config(system_config); 

    nb_nodes = system_config.nodes_in_system;
    inter_request_delay = system_config.inter_request_delay;
    cs_execution_time = system_config.cs_execution_time;
    num_requests = system_config.num_requests;

    sscanf(argv[1], "%d", &node_id);

    quorum_size = system_config.quorumSize[node_id];
    port = system_config.portNumbers[node_id];

    // Set up quorum information
    quorum =  malloc(quorum_size * sizeof(Quorum_Member));
    

    // allocate quorum array
    for (i = 0; i < quorum_size; i++)
    {
        quorum[i].id = system_config.quorum[node_id][i];
        quorum[i].port = system_config.portNumbers[quorum[i].id];
        memmove(quorum[i].hostname, system_config.hostNames[quorum[i].id], 18);
    }

    // Initialize mutex
    if (sem_init(&enter_request, 0, 0) == -1) {
        printf("Error during mutex init.\n");
        exit(1);
    }

    if (sem_init(&request_grant, 0, 0) == -1) {
        printf("Error during mutex init.\n");
        exit(1);
    }

    if (sem_init(&wait_grant, 0, 0) == -1) {
        printf("Error during mutex init.\n");
        exit(1);
    }
    
    if (sem_init(&execution_end, 0, 0) == -1) {
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
    }

    // Create mutual exclusion service thread
    pthread_t pid;
    pthread_create(&pid, &attr, mutual_exclusion_handler, NULL);

    struct timespec current_time, previous_time;
    clock_gettime(CLOCK_REALTIME, &previous_time);
    uint64_t delta_ms;

    struct timespec random_cs_time;
    int random_delay = exponential_rand(inter_request_delay);
    int random_exec_time = exponential_rand(cs_execution_time);

    random_cs_time.tv_sec = random_exec_time / 1000;
    random_cs_time.tv_nsec = (random_exec_time % 1000) * 1000000;

    // Application loop
    int request_generated = 0;
    while (request_generated < num_requests) {
        clock_gettime(CLOCK_REALTIME, &current_time);
        delta_ms = (current_time.tv_sec - previous_time.tv_sec) * 1000 +
            (current_time.tv_nsec - previous_time.tv_nsec) / 1000000;

        if (delta_ms > random_delay) {
            request_generated++;
            cs_enter();
            nanosleep(&random_cs_time, NULL);
            cs_leave();
            previous_time.tv_sec = current_time.tv_sec;
            previous_time.tv_nsec = current_time.tv_nsec;
        }
    }

    exit(0);
}

void* handle_quorum_member(void * arg)
{
    // Initialize buffer and size variable
    int count = 0;
    size_t rcv_len = 0;
    char buffer[BUFFERSIZE];

    int s = *((int*) arg);

    while (1) {
        if (((count = recv(s, buffer + rcv_len, BUFFERSIZE - rcv_len, 0)) == -1)) {
            printf("Error during socket read.\n");
            close(s);
            exit(1); 
        }
        else if (count > 0) {
            rcv_len = rcv_len + count;
            parse_buffer(buffer, &rcv_len);
        }
    }
}

//Src dst prot timestamp->
//|##|##|Char|###| (Pipes not included in actual messages)
void parse_buffer(char* buffer, size_t* rcv_len)
{
    // Check if we have enough byte to read message length
    int message_len = 3;
    while (*rcv_len > 4 ) {
        // Check if we received a whole message

        if (*rcv_len < 5 + message_len) 
           break; 

        // Handle message received
        handle_message(buffer, message_len + 5);

        // Remove message from buffer and shuffle bytes of next message to start of the buffer
        *rcv_len = *rcv_len - 5 - message_len;
        if (*rcv_len != 0) {
            memmove(buffer, buffer + 5 + message_len, *rcv_len);
        }
    }
}

// Check type of message (application or marker) and process it
// Source | Dest | Protocol | Length | Payload
int handle_message(char* message, size_t length)
{
    char temp[300];
    strcpy(temp, message);
    temp[length] = '\0';
    printf("MSG RCVD: %s LENGTH: %d\n", temp, (int) length);

    if (message_type(message) == GRANT)
    {
        lock_received++;

        if (lock_received == quorum_size) {
            // Signal CS can be executed
            if (sem_post(&wait_grant) == -1) {
                printf("Error during signal on mutex.\n");
                exit(1);
            }

        }
    }

    if (message_type(message) == REQUEST)
    {
    }

    if (message_type(message) == RELEASE)
    {
    }

    if (message_type(message) == FAILED)
    {
    }

    if (message_type(message) == INQUIRE)
    {
    }

    if (message_type(message) == YIELD)
    {
    }

    return 0;
}

void cs_enter() 
{
    // Request CS enter by signaling semaphore
    if (sem_post(&enter_request) == -1) {
        printf("Error during signal on mutex.\n");
        exit(1);
    } 

    // Wait on mutual exclusion module to grant request
    if (sem_wait(&request_grant) == -1) {
        printf("Error during wait on mutex.\n");
        exit(1);
    }
}

void cs_leave()
{
    // Signal that application is done executing critical section
    if (sem_post(&execution_end) == -1) {
        printf("Error during signal on mutex.\n");
        exit(1);
    } 
}

void* mutual_exclusion_handler()
{
    while (1) {
        // Wait for CS request from application
        if (sem_wait(&enter_request) == -1) {
            printf("Error during wait on mutex.\n");
            exit(1);
        }

        maekawa_protocol_request();
    
        // Signal to application it can execute CS
        if (sem_post(&request_grant) == -1) {
            printf("Error during signal on mutex.\n");
            exit(1);
        } 

        // Wait for application to finish executing CS
        if (sem_wait(&execution_end) == -1) {
            printf("Error during wait on mutex.\n");
            exit(1);
        }
        maekawa_protocol_release();
    }    
} 

void maekawa_protocol_release()
{
    int i;
    char msg[50];

    // Send release message to all quorum members
    timestamp++;
    for (i = 0; i < quorum_size; i++)
    {
        snprintf(msg, 9, "%02d%02dL%03d", node_id, quorum[i].id, timestamp);
        send_msg(quorum[i].send_socket, msg, 8);
    }
}

void maekawa_protocol_request()
{
    int i;
    char msg[50];

    lock_received = 0;
    // Send request message to all quorum members
    timestamp++;
    for (i = 0; i < quorum_size; i++)
    {
        snprintf(msg, 9, "%02d%02dR%03d", node_id, quorum[i].id, timestamp);
        send_msg(quorum[i].send_socket, msg, 8);
    }

    // Wait until all GRANT messages are received
    if (sem_wait(&wait_grant) == -1) {
        printf("Error during signal on mutex.\n");
        exit(1);
    } 
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

int message_ts(char *msg)
{
    int ts; 
    sscanf(msg+5, "%3d", &ts);
    return ts;
}

int exponential_rand(int mean)
{
    double random_exp = rand() / (RAND_MAX + 1.0);
    random_exp = -log(1 - random_exp) * (double) mean;

    int ret;
    ret = random_exp;

    return ret;
}
