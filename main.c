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

typedef struct CS_Time
{
    int start_time;
    int end_time;
} CS_Time;

// Semaphore
sem_t enter_request;
sem_t request_grant;
sem_t execution_end;
sem_t wait_grant;

// Lamport clock
int timestamp = 0;
int grant_timestamp = 0;


int exponential_rand(int mean);

void* handle_quorum_member(void* arg);
void parse_buffer(char* buffer, size_t* rcv_len);
int handle_message(char* message, size_t length);

void send_msg(int sockfd, char * buffer, int msglen);
int receive_message(char * message, int length);
int merge_timestamps(int incoming_ts);

int message_source(char * msg);
int message_dst(char * msg);
char message_type(char * msg);
char * message_payload(char * msg);
void app();
void cs_enter();
void cs_leave();
void output();
int message_ts(char * msg);
int can_request();

void* mutual_exclusion_handler();
void maekawa_protocol_release();
void maekawa_protocol_request();

// Global parameters
int nb_nodes;
int inter_request_delay;
int cs_execution_time;
int num_requests;

//Timing Variables
int prev_ms = 0;
int launch_time_s;

config system_config; 

// Node Paramters
int node_id;
int port;
int request_num;
CS_Time* execution_times;
int wait_time;

Quorum_Member* quorum;
Quorum_Member* membership;
int quorum_size;
int membership_size = 0;
int lock_holder = -1;
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

    for (i = 0; i < system_config.nodes_in_system; i++)
    {
        if (i != node_id)
        {
            for (j = 0; j < system_config.quorumSize[i]; j++)
            {
                if (system_config.quorum[i][j] == node_id)
                {
                    membership_size++;
                    break;
                }
            }
        }
    }

    // Set up quorum information
    quorum =  malloc(quorum_size * sizeof(Quorum_Member));
    membership = malloc(membership_size * sizeof(Quorum_Member));

    // List of all start and end times of critical sections for this node
    execution_times = malloc(num_requests * sizeof(CS_Time));

    // allocate quorum array
    for (i = 0; i < quorum_size; i++)
    {
        quorum[i].id = system_config.quorum[node_id][i];
        quorum[i].port = system_config.portNumbers[quorum[i].id];
        memmove(quorum[i].hostname, system_config.hostNames[quorum[i].id], 18);
    }

    i = 0;
    while(i < membership_size)
    {
        for (k = 0; k < system_config.nodes_in_system; k++)
        {
            if (k != node_id)
            {
                for (j = 0; j < system_config.quorumSize[k]; j++)
                {
                    if (system_config.quorum[k][j] == node_id)
                    {
                        membership[i].id = k;
                        membership[i].port = system_config.portNumbers[k];
                        memmove(membership[i].hostname, system_config.hostNames[i], 18);
                        i++;
                        break;
                    }
                }
            }
        }
    }

    printf("Membership size: %d\nMembership: ", membership_size);

    for(i = 0; i < membership_size; i++)
    {
        printf("%d ", membership[i].id);
    }
    printf("\n");
    
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
       // if (quorum[j].id != node_id)
        //{
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
            printf("Node %d connected to quorum member %d.\n", node_id, quorum[j].id);
        //}
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


    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    launch_time_s = ts.tv_sec;

    // Create mutual exclusion service thread
    pthread_t pid;
    pthread_create(&pid, &attr, mutual_exclusion_handler, NULL);
    wait_time = exponential_rand(inter_request_delay);
    // Application loop
    printf("Enter App\n");
    app();

    exit(0);
}


void app()
{
   while(1)
    {
        if (can_request())
        {
            cs_enter();
        }
    }
}

void cs_enter()
{
    printf("Enter CS");
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

    int sec, new_sec;
    long nsec, new_nsec;
    struct timespec ts;
    int time_elapsed = 0; //ms
    int ms, new_ms;

    clock_gettime(CLOCK_REALTIME, &ts);
    sec = ts.tv_sec - launch_time_s;
    nsec = ts.tv_nsec;
    prev_ms = sec * 1000 + nsec / 1000000;

    execution_times[request_num].start_time = prev_ms;

    while (time_elapsed < cs_execution_time)
    {   
        clock_gettime(CLOCK_REALTIME, &ts);
        new_sec = ts.tv_sec - launch_time_s;
        new_nsec = ts.tv_nsec;
        new_ms = new_sec * 1000 + new_nsec / 1000000;

        time_elapsed += (new_ms - ms);
        ms = new_ms;
    }
    cs_leave();

}

void cs_leave()
{
    int sec;
    long nsec;
    int ms;
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    sec = ts.tv_sec - launch_time_s;
    nsec = ts.tv_nsec;
    ms = sec * 1000 + nsec / 1000000 ;
    execution_times[request_num].end_time = ms;
    request_num++;

    // Signal that application is done executing critical section
    if (sem_post(&execution_end) == -1) {
        printf("Error during signal on mutex.\n");
        exit(1);
    }
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
    static int failed_received = 0;
    static int inquire_received = 0;
    char temp[300];
    strcpy(temp, message);
    temp[length] = '\0';
    printf("MSG RCVD: %s LENGTH: %d\n", temp, (int) length);
    
    int sender = message_source(message);
    int sender_ts = message_ts(message);
    merge_timestamps(sender_ts);

    if (message_type(message) == GRANT)
    {
        lock_received++;
        // Check if all locks are received
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
        char msg[20];
        if (lock_holder == -1) {
            // Grant lock
            timestamp++;
            lock_holder = sender;
            grant_timestamp = message_ts(message);
            snprintf(msg, 9, "%02d%02dG%03d", node_id, sender, timestamp);
            send_msg(quorum[sender].send_socket, msg, 8);
        }

        // If lock is already held check timestamps
        else 
        {
            if (message_ts(message) < grant_timestamp)
            {
                timestamp++;
                snprintf(msg, 9, "%02d%02dF%03d", node_id, lock_holder, timestamp);
                send_msg(quorum[sender].send_socket, msg, 8);

            }
            else
            {
                timestamp++;
                snprintf(msg, 9, "%02d%02dF%03d", node_id, sender, timestamp);
                send_msg(quorum[sender].send_socket, msg, 8);
            }
        }
        
    }

    if (message_type(message) == RELEASE)
    {
        lock_holder = -1;
    }

    if (message_type(message) == FAILED)
    {
        if (inquire_received)
        {
            maekawa_protocol_release(); // release all or just 1?
            inquire_received = 0;
        }
        else
        {
            failed_received = 1;
        }
    }

    if (message_type(message) == INQUIRE)
    {
        if (failed_received)
        {
            maekawa_protocol_release();
            failed_received = 0;
        }
        else
            inquire_received = 1;
    }

    if (message_type(message) == YIELD)
    {
        lock_holder = -1;
    }

    return 0;
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


int can_request()
{
    int current_sec;
    long current_nsec;
    long current_ms;
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    current_sec = ts.tv_sec - launch_time_s;
    current_nsec = ts.tv_nsec;
    current_ms = current_sec * 1000 + current_nsec / 1000000;

    printf("current_ms:%d\n", current_nsec);

    if (current_ms - prev_ms > wait_time && request_num < num_requests)
        return 1;
    else
        return 0;
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

int merge_timestamps(int incoming_ts)
{
    if (incoming_ts > timestamp)
        timestamp = incoming_ts + 1;
    else
        timestamp = timestamp + 1;

    return 0;
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

void output()
{
    char fileName[15];
    snprintf(fileName, 15, "node%doutput", node_id);
    FILE * fp = fopen(fileName, "w");
    int i;
    for (i = 0; i < request_num; i++)
    {
        fprintf(fp, "%d %d\n", execution_times[request_num].start_time, execution_times[request_num].end_time);
    }
}


int exponential_rand(int mean)
{
    double random_exp = rand() / (RAND_MAX + 1.0);
    random_exp = -log(1 - random_exp) * (double) mean;

    int ret;
    ret = random_exp;

    return ret;
}
