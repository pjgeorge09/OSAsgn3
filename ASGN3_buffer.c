#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
 


// Handle Command Line Arguements
// #1 , Number of producer (user) processes
// #2 , Number of consumer (printer) threads

// Usenanosleep() or sleep() to simulate a random delay between 0.1 and 1 seconds BETWEEN two successive print jobs SUBMITTED BY THE SAME USER.
// User 1) Insert print request into global print queue (Additional book-keeping params allowed to be inserted here!)

// Use a struct for producer print request and the global queue will be a linked-list of structs
// Use a struct for consumer print process. 
// Producer process cannot remove/update a currently existing print request from the queue ( Producer can only add when not full )

// Each printer thread process 1 print request at a time depending on availability
// NEED THREE SEMAPHORES.
        // 1) First one to check if the queue is full
        // 2) To check if the queue is empty
        // 3) For reading/writing the same queue element.
// Init these before starting processes/threads ON SHARED MEMORY.
// Use counting semaphore from ASGN2 for this assignments's counting semaphore

// Main process needs to solve when all print's are done, then deallocate the global print queue & semaphores.
// Threads exit when all print requests compleUse global count concept for Pro/Con from ASGN2 maintained by variable "buffer_index" (Below)ted. 

// Print the results, how many print jobs, what size were they. Then how many print jobs processed by what thread
// Sig handler in code to ensure graceful termination of the threads on receiving a ^C from the console

// Report execution time of code as a function of users, printers, and queue sizes. 
// Plot the average waiting times for all the rint jobs as a function of these metrics.
// Expect SJF to work better than FCFS. (Less bytes in queue goes first!)



// The global print queue: two implementations are needed *********************
// 1) Use global count concept for Pro/Con from ASGN2 maintained by variable "buffer_index" (Below)
// 2) Implement a Priority Queue based on SSJF.
        // in/out pointers WILL NOT WORK
        // if the queue is not full, next print request inserted at right position based on job size
        // printer threads will always read from one end of the queue
        // therefore best is to implement a linked listof structs.
        // Linked list is fixed sizequeue.
        // Items consumed marked with an additional field in the struct (marked = 0 ---> marked = 1) this.marked = 1
int users, userProcesses, consumerThreads, consumers;
#define printJobSize 1000 // Up to 1000 bytes
#define printQueueSize 30 // Number of structs in shared memory
#define SHMSZ 30*sizeof( printer)
#define SHMSZ2 sizeof(int)

typedef int buffer_t; // Global print queue
buffer_t buffer[printQueueSize]; // 30 structures
int *buffer_index, *print_index; // --> "Traverse print queue" //Global count concept
pthread_mutex_t buffer_mutex; // Our binary semaphore. Need 30
    char *shm_addr1;
int bookkeeping;
/* initially buffer will be empty.  full_sem
   will be initialized to buffer SIZE, which means
   SIZE number of producer threads can write to it.
   And empty_sem will be initialized to 0, so no
   consumer can read from buffer until a producer
   thread posts to empty_sem */
// sem_t full_sem;  /* when 0, buffer is full */
// sem_t empty_sem; /* when 0, buffer is empty. Kind of
                    // like an index for the buffer */
 
struct countingSemaphore{ // Counting Semaphore Struct
    int val;
    sem_t gate; // Binary Semaphore
    sem_t mutex; // Binary Semaphore
} *full, *empty;

struct countingSemaphore *full2, *empty2;

typedef struct {
    int processID, thisJobSize;
    sem_t lock; // This locks this struct when it is being read or written
    int free; // unlock when consumed, allows to be rewritten 0 for NOT FREE, 1 for free to be used.
} printer;
printer *p1;




void init(struct countingSemaphore *thisCountingSemaphore, int aK){
    thisCountingSemaphore->val = aK;
    if(aK > 0){
        sem_init(&thisCountingSemaphore->gate, 0 , 1 );
    }
    else if (aK == 0){
        sem_init(&thisCountingSemaphore->gate, 0 , 0);
    }
    else{
        printf("Error on sem init\n");
        exit(0);
    }
    sem_init(&thisCountingSemaphore->mutex, 0 , 1);
    }

// wait
void proberen(struct countingSemaphore *thisCountingSemaphore){
    sem_wait(&thisCountingSemaphore->gate);
    sem_wait(&thisCountingSemaphore->mutex);
    thisCountingSemaphore->val = (thisCountingSemaphore->val - 1);
    if(thisCountingSemaphore->val > 0){
        sem_post(&thisCountingSemaphore->gate);
    }
    sem_post(&thisCountingSemaphore->mutex);
}

// P = sem_wait , V = sem_post
// release
void verhogen(struct countingSemaphore *thisCountingSemaphore){
    sem_wait(&thisCountingSemaphore->mutex);
    thisCountingSemaphore->val = (thisCountingSemaphore->val + 1);
    if (thisCountingSemaphore->val == 1){
        sem_post(&thisCountingSemaphore->gate);
        }
    sem_post(&thisCountingSemaphore->mutex);
    }


/*
 CSem(K) cs { // counting semaphore initialized to K
    int val ← K; // the value of csem
    BSem gate(min(1,val)); // 1 if val > 0; 0 if val = 0
    BSem mutex(1); // protects val

Pc(cs) {
    P(gate)
    P(mutex);
    val ← val − 1;
    if val > 0
        V(gate);
    V(mutex);
}

Vc(cs) {
    P(mutex);
    val ← val + 1;
    if val = 1
        V(gate);
    V(mutex);
}
}
 */
 
void insertbuffer(int datasize, int thisID) {
    printer thisPrinter; // Init a new printer
    thisPrinter.processID = thisID; // Set the ID
    thisPrinter.thisJobSize = datasize; // Set the Print Size
    sem_wait(&thisPrinter.lock); // Lock this (??? I GUESS???)
    thisPrinter.free = 0; // Set it to "I have not yet been consumed"
    for (int i = 0; i < 30; i++){ // For structs 0 to 29
        if(p1[i].free == 1 || p1[i].free == NULL){ // if the struct at this space has been consumed    // OR NULL IF NEVER USED
            sem_wait(&p1[i].lock);
            p1[i].free = 0; // Lock it from others right quick.
            p1[i] = thisPrinter; // Now we found one. Set it.
            sem_post(&p1[i].lock);
            return;  // if it finds a 1, then this p1[location] is the next to be used.
        }
    }
}
 
printer dequeuebuffer(int number) {
    // Make a temp for this.thisJobSize
    // Set everything to null or something
    // return the temp
    printer temp;
    int circle = number;
    int found = 0;
    while(!found){
        for (int i = 0; i < 30; i++){ // For structs 0 to 29
            int j = (number + i) % 30;
            if(p1[j].free == 0){ // if the struct at this space NEEDS to be consumed
                sem_wait(&p1[j].lock); // lock it because being consumed
                // Sem lock because it's being consumed
                p1[j].free = 1; // 1 means consumed
                found = 1;  // Found the next item that needs to be consumed. Now break the while loop and return it
                // Now I need to increment the global count or whatever
                bookkeeping--;
                temp = p1[j];
                sem_post(&p1[j].lock); // unlock it 
                return(temp);
            }
        }
    }



    // if (buffer_index > 0) {
    //     return buffer[--*buffer_index]; // buffer_index-- would be error!
    // } else {
    //     printf("Buffer underflow\n");
    // }
    // return 0;
}
 
// print request 
void *producer(int jobs) {
    // int thread_numb = *(int *)thread_n; // idk what this does
    int value;
    int myID = getpid();
    // For this thread, we need randm number for number of jobs this user wants to submit (1-30)
    // Then we need to give each of those jobs a random size
    // Then this user will start trying to put all of his jobs into the shared print queue (At the same time as the other users)
    int i=0;
    while (i++ < jobs) {
        sleep(rand() % 10); // Sleep between jobs for a particular user
        value = rand() % 1000 + 100;
        proberen(&full);
        // sem_wait(&full_sem); // sem=0: wait. sem>0: go and decrement it
        // pthread_mutex_lock(&buffer_mutex); /* protecting critical section */
        insertbuffer(value, myID);
        // pthread_mutex_unlock(&buffer_mutex);
        verhogen(&empty);
        // sem_post(&empty_sem); // post (increment) emptybuffer semaphore
        printf("Producer %d added %d to buffer\n", myID, value);
    }
    pthread_exit(0);
}

// consumer print process.
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n; // My <consumer-id>
    printer tempPrinter;
    int i=0;
    while (1) {
        // sem_wait(&empty_sem);
        proberen(&empty);
        /* there could be race condition here, that could cause
           buffer underflow error */
        pthread_mutex_lock(&buffer_mutex);
        // I need to LOCK buffer_index first
        // IF BOOKKEEPING IS ZERO, GRACEFUL EXIT REQUIRED.
        tempPrinter = dequeuebuffer(buffer_index);
        // UNLOCK buffer_index in dequeuebuffer 
        // Now that we know the value, we can sleep proportionally to this.
        sleep(tempPrinter.thisJobSize / 10);
        pthread_mutex_unlock(&buffer_mutex);
        verhogen(&full);
        printf("Consumer %d dequeue %d , %d from buffer\n", thread_numb, tempPrinter.processID, tempPrinter.thisJobSize);
   }
    pthread_exit(0);
}
 
int main(int argc, char **argv) {
    time_t start, end; // For timing
    int shmid, shmid2; 
    key_t key, key2;
    struct printer *myPointer;
    int *shm, *s;
    int *shm2 , *s2 , *thing1, *thing2 , *s3 , *s4;
    start = clock();
    users = atoi(argv[1]);
    consumers = atoi(argv[2]);
    key = 8369;
    key2 = 5519; // Need to make more keys.
    buffer_index = 0;
    print_index = 0;

    // Creating shared Memory for buffer
    shmid = shmget(key, SHMSZ, IPC_CREAT | 0666);
    // Creating shared Memory for integer
    shmid2 = shmget(key2, SHMSZ2, IPC_CREAT | 0666);
    // Creating shared memory for Full counting semaphore
    full2 = shmget(1099, sizeof(struct countingSemaphore), IPC_CREAT | 0666 );
    // Creating shared memory for Empty counting semaphore
    empty2 = shmget(4912, sizeof(struct countingSemaphore), IPC_CREAT | 0666);
    // Create a bookkeeping number for graceful exit
    int bookkeepingPRE = shmget(6210, sizeof(int), IPC_CREAT | 0666);

    shm_addr1 = shmat(shmid, NULL, 0);
    p1 = (printer*)shm_addr1; // Create the printer array on shared memory
    buffer_index = (int*)shmat(shmid2, NULL, 0); // Create the buffer index on this shared memory
    *buffer_index = 0; // init
    full = (struct countingSemaphore*)shmat(full2, NULL, 0); // Create the FULL semaphore on shared memory
    empty = (struct countingSemaphore*)shmat(empty2, NULL, 0); // Create the EMPTY semaphore on shared memory
    bookkeeping = (int*)shmat(bookkeepingPRE, NULL, 0); // Create the bookkeeping integer for jobs left to finish

    //Currently have no idea what these do.
    s = p1;
    s2 = shm2;
    s3 = thing1;
    s4 = thing2;


    pthread_mutex_init(&buffer_mutex, NULL); // binary semaphore, we need 30 of these
    init(&full, printQueueSize);
    init(&empty, 0);
    
    int i;
    for (i = 0; i < users ; i++){
        if(fork() == 0){ // create child id
            int numJobs = (rand() % 30);
            bookkeeping += numJobs; // Maybe have to take the pointer off
            producer(numJobs);
            exit(0);
        }
        else{
            // while(NULL);
        }
    }
    // These two lines declare an array of actual threads, and an array of int integer value of threads for the USERS
    pthread_t thread[users]; // Making this many threads.
    int thread_numb[users];  // Making this many integers
    
    
    // These two lines declare an array of actual threads, and an array of int integer value of threads for the PRINTERS
    pthread_t printerThreads[consumers];
    int printerThreadNumber[consumers];
    // For all the consumer threads from command line, start this many printers.
    for (i = 0; i < consumers; ) {
        printerThreadNumber[i] = i;
        // playing a bit with thread and thread_numb pointers...
        pthread_create(&printerThreads[i], // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       consumer, // void *(*start_routine) (void *)
                       &printerThreadNumber[i]);  // void *arg
        i++;
    }
    // Wait to move on until all of the threads have completed. (starting with the users!)
    for (i = 0; i < users; i++)
        pthread_join(thread[i], NULL);
    for (i = 0; i < consumerThreads; i++)
        pthread_join(printerThreads[i], NULL);
    
    pthread_mutex_destroy(&buffer_mutex);
    // sem_destroy(&full_sem);
    // sem_destroy(&empty_sem);
    
    end = clock() - start;
    printf("ASGN3_buffer.c took %ld seconds.\n", end);
    return 0;
}
