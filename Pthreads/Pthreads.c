#include <stdio.h>
#include <signal.h>
#include <unistd.h> 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdarg.h> 
#include <math.h>

// testing command : ./a.out 4 10 99 5 17 28 67 65 22 19 11 77 89 78 45 40 20 10 90 76

//declarations relating to input
int PARAMETERS[4];
char** MATRIX; 
void printMatrix(); 
char** inputSanityCheck(int argv, char** argc); 


//declarations relating to process creation
char** printArray(int row); 
int procReturn = 0;
void childKillHandler();
void createNProcs(int n); 


//declarations relating to thread creation
int threadReturn = 0; 
pthread_mutex_t LOCK;
int isPrime(int x); 
void* calcAvg(void* x); 
int createNThreads(int row);



int main(int argv, char** argc) {

    argc = inputSanityCheck(argv, argc); 

    if(argc) {
        printMatrix(); 
    } 
    else {
        printf("Exiting parent process\n"); 
        exit(1);
    }

    int n = PARAMETERS[0], a = PARAMETERS[1], b = PARAMETERS[2], p = PARAMETERS[3]; 

    createNProcs(n); 
}

//4 10 99 5 17 28 67 65 22 19 11 77 89 78 45 40 20 10 90 76

//---INPUT AND UTILITY HANDLING

//To prevent segmentation faults.
//Checks if arguments passed are valid
//Returns a pointer to matrix if all ok else returns NULL pointer
char** inputSanityCheck(int argv, char** argc) {
    if(argv < 5) {
        printf("Enough arguments are not provided\n"); 
        exit(1); 
    }
    int n = atoi(argc[1]), a = atoi(argc[2]), b = atoi(argc[3]), p = atoi(argc[4]); 
    int wrongInp = 0;
    if(n*n + 4 != argv -1) wrongInp++, printf("Number of arguments entered are incorrect\n");
    if(a >= b || a < 0 || b < 0) wrongInp++, printf("Values of 'a' and 'b' are incorrect \n"); 

    if(wrongInp) return NULL;
    PARAMETERS[0] = n, PARAMETERS[1] = a, PARAMETERS[2] = b, PARAMETERS[3] = p;
    printf("Entered arguments : \n");
    printf("n : %d | a : %d | b : %d | p : %d \n", PARAMETERS[0], PARAMETERS[1], PARAMETERS[2], PARAMETERS[3]); 
    MATRIX = argc+5;
    return argc + 4; 
}

//Utility function to print the matrix for the user
void printMatrix() {
    int size = PARAMETERS[0];
    int i=0;
    char** inp = MATRIX;
    putchar('\n'); printf("Here is the entered array");
    for(; i<size*size; i++) {
        char* s = inp[i];
        int num = atoi(s); 
        if((i)%size == 0) putchar('\n');
        printf("%d ", num);
    }
    putchar('\n');
    return; 
}

//Utility function to print a row of the matrix to show the row being processed
char** printArray(int row) {
    int n = PARAMETERS[0]; 
    char** start = MATRIX + n*row;
    for(int i=0; i<n; i++) {
        printf("%d ", atoi(*(start + i))); 
    }
    putchar('\n');
    return start; 
}


//----PROCESS HANDLING----]

//Handles the signal SIGCHLD
//If there is an error (returnValue != 0) it kills the parent process
void childKillHandler() {
    if(!procReturn) {
        printf("Process terminated successfully->"); 
    } else {
        printf("!!!Child process has terminated unexpectedly!!! | Exit code : %d\n", procReturn); 
        printf("Exiting current process and terminating all other running processes gracefully\n"); 
        exit(1); 
    }
}

//Creates n child processes and establishes IPC through pipe() sys call
void createNProcs(int n) {
    int p[n][2];

    signal(SIGCHLD, childKillHandler);

    int controller_pid = getpid();
    printf("Creating %d processes. Parent proess pid : %d\n", n, controller_pid); 

    int acc = 0; //accumulates all average values to produce final sum

    for(int i=0; i<n; i++) {

        pipe(p[i]); //creating communication between child and parent

        if(getpid() == controller_pid) { //forking exactly n children for parent

            int child_pid = fork();

            if(getppid() == controller_pid) { //Child : Perform threading
                printf("========"); putchar('\n'); putchar('\n'); 
                printf("Child (pid : %d) created for parent (ppid : %d)\n", getpid(), getppid()); 
                printf("Processing row %d : ", i);
                printArray(i);

                printf("\n");
                int rowAv = createNThreads(i);
                write(p[i][1], &rowAv, sizeof(int)); 
                printf("Value :: %d written to pipe successfully->", rowAv); 
                exit(0); 

            } else { //Parent : Perform read

                int x; 
                wait(&procReturn); 
                read(p[i][0], &x, sizeof(int)); 
                acc += x; 
                printf("Value :: %d read from pipe in parent\n", x); 
            }
        }
    }

    printf("\nAll child processes completed :: Calculating average of n rows\n");
    printf("\n"); putchar('\n'); 
    printf("========="); putchar('\n');
    printf("Final Answer : %d \n", acc / n); 
    printf("========="); 
    int number = acc /n ; 
    
}


//---THREAD HANDLING----

//Creates n threads for the parent process, calls the 'calcAvg' func and joins all threads finallyy
int createNThreads(int row) {
    int n = PARAMETERS[0]; 
    char** inp = MATRIX + n*row; 
    pthread_t workers[n]; //captures thread id of all workers to join them later 
    // pthread_t ptid; 

    if (pthread_mutex_init(&LOCK, NULL) != 0) { //initializing mutext, if failed, return with nozero value
        printf("\n !!MUTEX INIT HAS FAILED!!\n");
        return 1;
    }

    pthread_attr_t attr; 
    for(int i=0; i<n; i++) {
        pthread_attr_init(&attr); 
        printf("--\nCreating worker thread (tid : %ld) for number %d\n--\n", workers[i], atoi(MATRIX[n*row + i]));
        pthread_create(&workers[i], &attr, calcAvg, MATRIX[n*row + i]); 
    }
    for(int i=0; i<n; i++) {
        pthread_join(workers[i], NULL); 
        printf("(Joined thread tid(%ld) for number :: %d)\n", workers[i], atoi(MATRIX[n*row + i]));
    }
    printf("\n----\nJoined all threads for row %d\n\n---Average value of row :: %d---\n----\n", row,threadReturn/n); 

    pthread_mutex_destroy(&LOCK);

    return threadReturn / n; 
}

//Main logic function to calculate px for an element in the matrix
//Accumulaes the value of px into threadReturn
void* calcAvg(void *val) {
    int n = PARAMETERS[0], a = PARAMETERS[1], b = PARAMETERS[2], p = PARAMETERS[3]; 

    int mid = atoi((char*)val);
    printf("Evaluating thread for number :: %d\n", mid); 
    if(mid < a || mid > b) {
        printf("!!!Err :: Value :: %d doesn't lie between 'a' and 'b'!!!\n", mid);
        exit(1); 
    }
    int acc  = 0; 
    int nPrimes = 0;
    int curr = mid+1; 

    printf("Discovered prime numbers :: %d ::", mid);

    while(nPrimes < p) {//primes greater than
        if(isPrime(curr)) {
            acc += curr;
            nPrimes++; 
            printf("%d ", curr); 
        }
        
        curr++;
    }

    curr = mid - 1; 
    while(curr > 1 && nPrimes < 2*p) {
        if(isPrime(curr)) {
            acc += curr;
            nPrimes++;
            printf("%d ", curr); 
        }
        curr--;
    }
    if(isPrime(mid)) {
        printf("%d ", mid); 
        nPrimes++;
        acc += mid; 
    }
    acc /= nPrimes;
    putchar('\n');
    pthread_t ptid;
    printf("--Average prime value of %d :: %d--\nExiting thread for number :: %d\n\n", mid, acc, mid);  

    pthread_mutex_lock(&LOCK);
        threadReturn += acc; 
    pthread_mutex_unlock(&LOCK);

    pthread_exit(NULL);
}

//Utility function
int isPrime(int x) {
    if(x == 2) return 1;
    for(int i=2; i*i<=x; i++) if(x%i == 0) return 0; 
    return 1; 
}