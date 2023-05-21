#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

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



int main() {
    //CREATING CONNECTION SHM

    int connect_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(connect_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } else printf("Succesfully connected to 'connect' shm. fd :: %d : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", connect_fd, getpid(), __FILE__, __LINE__, __FUNCTION__); 
    
    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    struct connect_data* connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0); 
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }
    connect_data->server_on = 0;

    while(connect_data->server_on != 1){
        sleep(1);
        printf("Error:: Server not Running : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__);
    }
    printf("Server Currently Running : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__);
    //REGISTRATION LOOP 
    int satisfied = 0; 
    char channel_name[MAX_USERNAME_LENGTH*2]; 
    while(!satisfied) {
        printf("Enter your username :: "); 
        char username[MAX_USERNAME_LENGTH]; 
        scanf("%s", username); 

        while(1) {
            int busy_server = pthread_rwlock_trywrlock(&connect_data->lock); 
            if(!busy_server) {
                //no other client is waiting for response
                //will not release rdlock until response
                strcpy(connect_data->request, username);
                printf("Registration request sent from user :: %s : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", username, getpid(), __FILE__, __LINE__, __FUNCTION__); 
                
                //wait for the response from server
                connect_data->served_registration_request = 0; 
                while(connect_data->served_registration_request == 0); 
                
                if(connect_data->response_code) {
                    satisfied = 0; 
                    printf("Error :: Username already in use : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 
                }
                else {
                    satisfied = 1; 
                    printf("Response 'ok' recieved from server\nUser Registered :: Channel name :: %s : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", connect_data->response, getpid(), __FILE__, __LINE__, __FUNCTION__); 
                    strcpy(channel_name, connect_data->response); 
                }
                pthread_rwlock_unlock(&connect_data->lock); 
                break;
            }

        }
    }

    //CHANNEL COMMUNICATION LOOP
    int client_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(client_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } else printf("Succesfully connected to client shm. fd :: %d : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", client_fd, getpid(), __FILE__, __LINE__, __FUNCTION__); 

    if(ftruncate(client_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    struct client_data* data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }
    

    printf("~Connection to server has been made : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 
    int exit = 0; 

    while(!exit) {
        if(data->served_registration_request == 0) continue; 
        int sc; 
        printf("Which Service? \n0 : Calculator | 1 : Even or Odd | 2 : Is Prime | 3 : Is Negative | Anything else : Unregister User :\n");
        scanf("%d", &sc); 
        switch(sc) {
            case 0 : 
                printf("Service Chosen : Calculator : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 
                int n1, n2, op; 
                printf("0 - Addition | 1 - Subtraction | 2 - Multiplication | 3 - Division\n");
                printf("Operator : "); 
                scanf("%d", &op); putchar('\n'); 
                printf("First Number : ");
                scanf("%d", &n1); putchar('\n');  
                printf("Second Number : ");

                while(1){
                    scanf( "%d", &n2); putchar('\n'); 
                    if(n2 == 0 && op == 3){
                        printf("Division by Zero: Bad Client Request : Try Again\n");
                    }
                    else{
                        break;
                    }
                } 
                data->request.service_code = 0;
                data->request.n1 = n1; data->request.n2 = n2; data->request.op_type = op; 
                break; 
            case 1 : 
                printf("Service Chosen : Even / Odd \n"); 
                int x; 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.evenOdd = x; 
                data->request.service_code = 1;
                break; 
            case 2 : 
                printf("Service Chosen : Is Prime\n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.isPrime = x; 
                data->request.service_code = 2;
                break; 
            case 3 : 
                printf("Service Chosen : Is Negative \n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.isNegative = x; 
                data->request.service_code = 3;
                break; 
            default: 
                printf("Unregistering User...\n"); 
                data->request.service_code = 99; 
                data->served_registration_request = 0; 
                return 0;
        }
        data->times_serviced++; 
        data->served_registration_request = 0; 
        printf("Request Sent to Server : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 
        while(data->served_registration_request == 0); //wait until response 
        printf("Received Response from Server : pid :: %d : File :: %s : Line :: %d : Function :: %s\n", getpid(), __FILE__, __LINE__, __FUNCTION__); 

        switch (sc) {
            case 0: 
                printf("Answer : %d\n", data->response.ans); 
                break; 
            case 1: 
                printf("Even : %d | Odd : %d \n", data->response.even, data->response.odd); 
                break; 
            case 2: 
                printf("Is Prime : %d \n", data->response.isPrime); 
                break; 
            case 3: 
                printf("Is Negative : %d\n", data->response.isNegative); 
        }

        char cont; 
        printf("Do you want' to continue (y/n):");
        scanf("%s", &cont); 
        putchar('\n'); 
        if(cont != 'y'){
            printf("Stopping Client... \n");
            data->active = 0;
            printf("Terminating...\n"); 
            break;
        }
    }
 
}