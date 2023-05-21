#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>

#define SHM_NAME "/connect" 
#define SHM_SIZE sizeof(struct connect_data)

#define MAX_CLIENTS 100 
#define MAX_USERNAME_LENGTH 100 

//connect channel
struct connect_data { 
    pthread_rwlock_t lock; 
    int response_code; //0 - ok | 1 - username in use | 2 - username too long
    int served_registration_request; 
    char request[MAX_USERNAME_LENGTH]; 
    char response[MAX_USERNAME_LENGTH*2]; 
    int server_on;
}; 


// client channel
struct client_request { 
    int service_code; //0 - operation | 1 - even or odd | 2 - is prime | 3 - is negative 
    int n1, n2, op_type; 
    int evenOdd; 
    int isPrime; 
    int isNegative; 
};

struct server_response {
    int ans; 
    int even, odd; 
    int isPrime; 
    int isNegative; 
};

struct client_data { 
    pthread_rwlock_t lock; //unique channel lock 
    pthread_t tid; 
    int times_serviced; 
    int active; //when active = 0, termination request
    int served_registration_request;
    struct client_request request; 
    struct server_response response;
};


//client struct
struct client {
    char username[MAX_USERNAME_LENGTH];
    int times_serviced; 
    int is_active;
    pthread_t tid; 
};

int CLIENTS_SERVICED = 0; 

struct client clients[MAX_CLIENTS]; 


//Thread handler
void* thread_handler(void* arg); 

//Signal handler 
void printStats(int x); 

int main() {
    //CREATING CONNECTION SHM

    signal(SIGINT, printStats);

    int connect_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (connect_fd < 0) {
        perror("shm_open");
        return 1;
    }
    printf("Connected to shm fd :: %d : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", connect_fd, getpid(), __FILE__, __LINE__, __FUNCTION__); 

    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    //mmaping the data for the connection shm 
    struct connect_data* connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0);
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1;
    }
    connect_data->response_code = 0;
    connect_data->request[0] = '\0'; 
    connect_data->request[0] = '\0'; 
    connect_data->served_registration_request = 1; 
    pthread_rwlock_init(&connect_data->lock, NULL);
    printf("Server initialised : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 
    //SERVER LOOP

    while(CLIENTS_SERVICED < MAX_CLIENTS) {
        connect_data->server_on = 1;
        if(connect_data->served_registration_request==0) {
            //Start serving the request
            //Bit flipped to 0 only after all write funcs done 
            char current_username[MAX_USERNAME_LENGTH];
            strcpy(current_username, connect_data->request); 

            int clash = 0; 
            for(int i=0; i<CLIENTS_SERVICED; i++) 
                if(strcmp(clients[i].username, current_username)==0 && clients[i].is_active == 1) clash++;

            if(clash) {
                printf("Connect : !!Username clash :: %s!!\n", current_username); 
                connect_data->response_code = 1; 
                connect_data->response[0] = '\0'; 
            } 
            else {
                int new_user = 1; 
                for(int i=0; i<CLIENTS_SERVICED; i++)
                    if(strcmp(clients[i].username, current_username) == 0) new_user = 0; 
                
                char channel_name[MAX_USERNAME_LENGTH*2]; 

                strcpy(channel_name, current_username); 
                strcat(channel_name, current_username); 
                strcpy(connect_data->response, channel_name);

                connect_data->response_code = 0; //ok

                if(new_user) {
                    printf("Connect : New Register Request. Username :: %s : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", current_username, getpid(), __FILE__, __LINE__, __FUNCTION__);
                    //creating new client object
                    strcpy(clients[CLIENTS_SERVICED].username, current_username); 
                    clients[CLIENTS_SERVICED].times_serviced = 1; 
                    clients[CLIENTS_SERVICED].is_active = 1; 

                    CLIENTS_SERVICED++; //new client also serviced
                    printf("Connect : Created a channel :: %s for new user :: %s\n", channel_name, current_username); 
                } else {
                    printf("Connect : Pre-existing user login. Username :: %s : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", current_username, getpid(), __FILE__, __LINE__, __FUNCTION__); 

                    strcpy(channel_name, current_username); 
                    strcat(channel_name, current_username); 
                    strcpy(connect_data->response, channel_name);
                    printf("Connect : Created a channel :: %s for existing user :: %s\n", channel_name, current_username); 
                }

                printf("(Clients Serviced :: %d)\n", CLIENTS_SERVICED);

                //creating thread to handle client
                int locn;
                for(int i=0; i<CLIENTS_SERVICED; i++) if(!strcmp(clients[i].username, current_username)) locn = i; 
                pthread_create(&clients[locn].tid, NULL, thread_handler, channel_name); 
            }
            
            connect_data->served_registration_request =1; 
        }

    }
    munmap(connect_data, SHM_SIZE); 
    shm_unlink("/connect");
}


void* thread_handler(void* arg){
    char username[MAX_USERNAME_LENGTH]; 
    char channel_name[MAX_USERNAME_LENGTH*2]; 
    strcpy(channel_name, (char*)arg); 

    int l=0; while(channel_name[l]) l++; 
    l/=2; 
    for(int i=0; i<l; i++) username[i] = channel_name[i]; 
    username[l] = '\0'; 

    int posn=0;
    for(int i=0; i<MAX_CLIENTS; i++) if(strcmp(clients[i].username, username) == 0){
        posn = i; 
    }

    printf("Connect : Created thread tid :: %lu to handle user :: %s : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", clients[posn].tid, username, getpid(), __FILE__, __LINE__, __FUNCTION__); 

    int client_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); 
    if(client_fd < 0) {
        perror("shm_open"); 
        return NULL; 
    } else printf("%s's channel : Succesfully connected to client shm. fd :: %d\n", username, client_fd); 

    if(ftruncate(client_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        return NULL; 
    }

    struct client_data* data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        return NULL; 
    }

    data->served_registration_request = 1; 
    data->active = 1; 
    data->times_serviced = 0; 
    pthread_rwlock_init(&data->lock, NULL); 
    printf("%s : Succesfully Initialised\n : pid :: %d : File :: %s : Line :: %d : Function :: %s", channel_name, getpid(), __FILE__, __LINE__, __FUNCTION__);

    while(data->active) {
        if(data->served_registration_request == 0 && data->active) {

            int code = data->request.service_code; 
            
            printf("%s : received response of code :: %d \n", channel_name, code); 

            if(code == 0) {
                int n1 = data->request.n1, n2 = data->request.n2, op= data->request.op_type; 
                int ans;
                switch(op) {
                    case 0:
                        ans = n1 + n2; 
                        break; 
                    case 1: 
                        ans = n1 - n2; 
                        break; 
                    case 2: 
                        ans = n1*n2; 
                        break; 
                    case 3: 
                        ans = n1/n2; 
                        break; 
                }
                data->response.ans = ans; 
            } else if(code == 1) {
                int x = data->request.evenOdd;
                data->response.even = !(x%2); 
                data->response.odd = x%2; 
            } else if(code == 2) {
                int x = data->request.isPrime; 
                int prime = 1; 
                for(int i=2; i<x/2; i++) if(x%i == 0) prime = 0;
                data->response.isPrime = prime; 
            } else if(code == 3){
                int x = data->request.isNegative; 
                data->response.isNegative = x < 0; 
            } else {
                printf("%s : Unregister request sent\n", channel_name);
                long unsigned int ti = clients[posn].tid; 
                for(int i=posn; i<CLIENTS_SERVICED-1; i++) {
                    clients[i] = clients[i+1]; 
                }
                CLIENTS_SERVICED--; 
                printf("%s : Unregistered user :: %s\n", channel_name, username); 
                printf("%s : Session Ended\nterminating thread (tid :: %lu) gracefully\n", channel_name, ti); 
                pthread_exit(NULL); 
            }
            data->served_registration_request = 1; 
            clients[posn].times_serviced++; 
            printf("%s : Sent Response to client\n(%d times serviced this session | %d total times serviced)\n", channel_name, data->times_serviced, clients[posn].times_serviced);  
        }
         
    }
    printf("%s : Session Ended\nterminating thread (tid :: %lu) gracefully : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", channel_name, clients[posn].tid, getpid(), __FILE__, __LINE__, __FUNCTION__); 
    clients[posn].is_active = 0; 
    shm_unlink(channel_name); 
    munmap(data, sizeof(struct client_data));
    pthread_exit(NULL);  
}

void printStats(int x) {
    for(int i=0; i<CLIENTS_SERVICED; i++) {
    printf("{\n");
        printf("\tusername : %s\n", clients[i].username);
        printf("\ttid : %lu\n", clients[i].tid);
        printf("\ttimes_serviced : %d\n", clients[i].times_serviced);
    printf("}\n--\n");
    }
    exit(0); 
}