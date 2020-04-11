#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>



// Handle Command Line Arguements
// #1 , Number of producer (user) processes
// #2 , Number of consumer (MyStruct) threads

// Usenanosleep() or sleep() to simulate a random delay between 0.1 and 1 seconds BETWEEN two successive print jobs SUBMITTED BY THE SAME USER.
// User 1) Insert print request into global print queue (Additional book-keeping params allowed to be inserted here!)

// Use a struct for producer print request and the global queue will be a linked-list of structs
// Use a struct for consumer print process. 
// Producer process cannot remove/update a currently existing print request from the queue ( Producer can only add when not full )

// Each MyStruct thread process 1 print request at a time depending on availability
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

// Report execution time of code as a function of users, MyStructs, and queue sizes. 
// Plot the average waiting times for all the rint jobs as a function of these metrics.
// Expect SJF to work better than FCFS. (Less bytes in queue goes first!)



// The global print queue: two implementations are needed *********************
// 1) Use global count concept for Pro/Con from ASGN2 maintained by variable "buffer_index" (Below)
// 2) Implement a Priority Queue based on SSJF.
        // in/out pointers WILL NOT WORK
        // if the queue is not full, next print request inserted at right position based on job size
        // MyStruct threads will always read from one end of the queue
        // therefore best is to implement a linked listof structs.
        // Linked list is fixed sizequeue.
        // Items consumed marked with an additional field in the struct (marked = 0 ---> marked = 1) this.marked = 1
int users, userProcesses, consumerThreads, consumers;
#define printJobSize 1000 // Up to 1000 bytes
#define printQueueSize 30 // Number of structs in shared memory
#define SHMSZ 30*sizeof( MyStruct)
#define SHMSZ2 sizeof(int)

typedef int buffer_t; // Global print queue
buffer_t buffer[printQueueSize]; // 30 structures
int *buffer_index, *print_index; // --> "Traverse print queue" //Global count concept

pthread_mutex_t *buffer_mutex; // Our binary semaphore. Need 30  // TODO *Must be on shared memory

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

typedef struct {
    int processID, thisJobSize;
    sem_t lock; // This locks this struct when it is being read or written
    int free; // unlock when consumed, allows to be rewritten 0 for NOT FREE, 1 for free to be used.
} MyStruct;




int *bookkeeping;
key_t key, key2 , key3 , key4; // Not currently used, make later.
// Shared Memory Integers
int shmid, shmid2, full2, empty2, bookkeepingPRE, shmidMutex;
// Shared Memory Objects
// Question for Dr Ghosh - Structure, especially ask about sem_t lock
MyStruct *p1;




void init(struct countingSemaphore *thisCountingSemaphore, int aK){
    thisCountingSemaphore->val = aK;
    if(aK > 0){
        sem_init(&thisCountingSemaphore->gate, 1 , 1 );
    }
    else if (aK == 0){
        sem_init(&thisCountingSemaphore->gate, 1 , 0);
    }
    else{
        printf("Error on sem init\n");
        exit(0);
    }
    sem_init(&thisCountingSemaphore->mutex, 1 , 1);
    }
// Question for Dr Ghosh - I did not change these much
// wait
void proberen(struct countingSemaphore *thisCountingSemaphore){
    sem_wait(&(thisCountingSemaphore->gate));
    sem_wait(&(thisCountingSemaphore->mutex));
    thisCountingSemaphore->val = (thisCountingSemaphore->val - 1);
    if(thisCountingSemaphore->val > 0){
        sem_post(&(thisCountingSemaphore->gate));
    }
    sem_post(&(thisCountingSemaphore->mutex));
}

// P = sem_wait , V = sem_post
// release
void verhogen(struct countingSemaphore *thisCountingSemaphore){
    sem_wait(&(thisCountingSemaphore->mutex));
    thisCountingSemaphore->val = (thisCountingSemaphore->val + 1);
    if (thisCountingSemaphore->val == 1){ // This line is the one that my friend has as ONE = sign, just setting the value every time.
        sem_post(&(thisCountingSemaphore->gate));
        }
    sem_post(&(thisCountingSemaphore->mutex));
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
    MyStruct thisMyStruct; // Init a new MyStruct
    thisMyStruct.processID = thisID; // Set the ID
    thisMyStruct.thisJobSize = datasize; // Set the Print Size
    sem_init(&thisMyStruct.lock, 1,1); // binary semaphore

    // sem_wait(&thisMyStruct.lock); // Lock this (??? I GUESS???)
    thisMyStruct.free = 0; // Set it to "I have not yet been consumed"
    // printf("insert 1\n");
    for (int i = 0; i < 30; i++){ // For structs 0 to 29
        // printf("For Loop At %d\n", i);
        // printf("%d outside\n", p1[i].free);
        if(p1[i].free == 1)   { // if the struct at this space has been consumed    // OR NULL IF NEVER USED || p1[i].free == NULL
            // printf("%d inside\n", p1[i].free);
            shmctl(shmid, SHM_UNLOCK, NULL);
            (p1[i]).free =0;

            // printf("Found a free spot in insert\n");
            sem_wait(&p1[i].lock);
            // printf("got past the sem_wait lock\n");
            // p1[i].free = 0; // Lock it from others right quick.
            p1[i] = thisMyStruct; // Now we found one. Set it.
            // printf("Set the structure?\n");
            shmctl(shmid, SHM_UNLOCK, NULL);

            sem_post(&p1[i].lock);

            // printf("Set the lock back.\n");
            return;  // if it finds a 1, then this p1[location] is the next to be used.
        }
        else{

        }
    }
    return;
}
 
MyStruct dequeuebuffer(int number) {
    *bookkeeping -=1;
    // Make a temp for this.thisJobSize
    // Set everything to null or something
    // return the temp
    // printf("Got into Dequeue with %d\n", number);
    MyStruct temp;
    int circle = number;
    int found = 0;
    while(!found){
        for (int i = 0; i < 30; i++){ // For structs 0 to 29
            int j = (number + i) % 30;
            if(p1[j].free == 0){ // if the struct at this space NEEDS to be consumed
                p1[j].free = 1;
                // printf("Dequeue found somewhere with p1 = 0\n");
                sem_wait(&p1[j].lock); // lock it because being consumed
                // Sem lock because it's being consumed
                temp = p1[j];
                
                p1[j].free = 1; // 1 means consumed
                found = 1;  // Found the next item that needs to be consumed. Now break the while loop and return it
                // Now I need to increment the global count or whatever
                
                sem_post(&p1[j].lock); // unlock it 
                found = 1;
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
    // printf("New Producer Method Called. ");
    // printf("Num jobs : %d\n", jobs);
    // int thread_numb = *(int *)thread_n; // idk what this does
    int value;
    int myID = getpid();
    // For this thread, we need randm number for number of jobs this user wants to submit (1-30)
    // Then we need to give each of those jobs a random size
    // Then this user will start trying to put all of his jobs into the shared print queue (At the same time as the other users)
    int i=0;
    while (i < jobs) {
        // printf("Sleeping...\n");
        sleep(rand() % 10); // Sleep between jobs for a particular user
        // printf("Done sleeping.\n");
        value = rand() % 900 + 100;
        // printf("Bookkeeping : %d, Job Size : %d\n", *bookkeeping, value);
        proberen(full);
        // printf("Got past proberen for fork number %d\n", myID);
        // sem_wait(&full_sem); // sem=0: wait. sem>0: go and decrement it
        pthread_mutex_lock(buffer_mutex); /* protecting critical section */
        insertbuffer(value, myID);        
        // printf("Bookkeeping2 : %d, Job Size2 : %d\n", *bookkeeping, value);
        pthread_mutex_unlock(buffer_mutex);
        // printf("Got past insertbuffer for fork number %d\n", myID);

        verhogen(empty);
        // printf("Got past verhogen for fork number %d\n", myID);

        // sem_post(&empty_sem); // post (increment) emptybuffer semaphore
        printf("Producer %d added %d to buffer\n", myID, value);
        i++;
    }
    // printf("Got to producer outside loop\n");
    // pthread_exit(0);
}

// consumer print process.
void *consumer(void *thread_n) {
    // printf("Got into consumer with bookkeeping value : %d\n", *bookkeeping);
    int thread_numb = *(int *)thread_n; // My <consumer-id>
    MyStruct tempMyStruct;
    int i=0;
    while (1) {
        // printf("While 1!\n");
        // sem_wait(&empty_sem);
            // printf("1Got into consumer with bookkeeping value : %d\n", *bookkeeping);

        proberen(empty);
            // printf("2Got into consumer with bookkeeping value : %d\n", *bookkeeping);

        // printf("Past Consumer Proberen\n");
        /* there could be race condition here, that could cause buffer underflow error */
        pthread_mutex_lock(buffer_mutex);
        // printf("Past Consumer Mutex\n");
        // I need to LOCK buffer_index first
        // IF BOOKKEEPING IS ZERO, GRACEFUL EXIT REQUIRED.
            // printf("3Got into consumer with bookkeeping value : %d\n", *bookkeeping);

        tempMyStruct = dequeuebuffer(*buffer_index);
        printf("Thread %d decremented Bookkeeping to %d\n", thread_numb, *bookkeeping);
        // printf("Set the structure in Consumer!\n");
        // UNLOCK buffer_index in dequeuebuffer 
        // Now that we know the value, we can sleep proportionally to this.
        sleep(tempMyStruct.thisJobSize / 1000);
        pthread_mutex_unlock(buffer_mutex);
        // printf("Unlock the Mutex and Slept in Consumer");
        verhogen(full);
        printf("Consumer %d dequeue %d , %d from buffer\n", thread_numb, tempMyStruct.processID, tempMyStruct.thisJobSize);
        // *bookkeeping--;
        if (*bookkeeping == 0){
            printf("Canceling thread %d because Bookkeeping is at %d\n", thread_numb, *bookkeeping);
            pthread_cancel(thread_numb);
        }
        // printf("Consumer Bookkeeping is %d\n", *bookkeeping);
   }
    
    pthread_exit(0);
}

void sigintHandler(int sig_num){
    shmdt(p1);
    shmdt(buffer_index);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);
    shmdt(buffer_mutex);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID,NULL);
    shmctl(full2, IPC_RMID, NULL);
    shmctl(empty2, IPC_RMID,NULL);
    shmctl(bookkeepingPRE, IPC_RMID, NULL);
    shmctl(shmidMutex, IPC_RMID,NULL);
    pthread_mutex_destroy(buffer_mutex);
    printf("Exit Ctrl C\n");
}
int main(int argc, char **argv) {
    time_t start, end; // For timing
    start = clock();
    users = atoi(argv[1]);
    consumers = atoi(argv[2]);
    int i;
    signal(SIGINT, sigintHandler);

    // Creating shared Memory for buffer
    shmid = shmget(10212, SHMSZ, IPC_CREAT | 0666);
    // Creating shared Memory for integer
    shmid2 = shmget(10623, SHMSZ2, IPC_CREAT | 0666);
    // Creating shared memory for Full counting semaphore
    full2 = shmget(10424, sizeof(struct countingSemaphore), IPC_CREAT | 0666 );
    // Creating shared memory for Empty counting semaphore
    empty2 = shmget(10725, sizeof(struct countingSemaphore), IPC_CREAT | 0666);
    // Create a bookkeeping number for graceful exit
    bookkeepingPRE = shmget(10226, sizeof(int), IPC_CREAT | 0666);
    // Creating shared memory for Mutex Semaphore
    shmidMutex = shmget(10727, sizeof(pthread_mutex_t), IPC_CREAT | 0666);

    p1 = (MyStruct*)shmat(shmid, NULL, 0); // Create the MyStruct array on shared memory
    buffer_index = (int*)shmat(shmid2, NULL, 0); // Create the buffer index on this shared memory
    full = (struct countingSemaphore*)shmat(full2, NULL, 0); // Create the FULL semaphore on shared memory
    empty = (struct countingSemaphore*)shmat(empty2, NULL, 0); // Create the EMPTY semaphore on shared memory
    bookkeeping = (int*)shmat(bookkeepingPRE, NULL, 0); // Create the bookkeeping integer for jobs left to finish
    buffer_mutex = (pthread_mutex_t*)shmat(shmidMutex, NULL, 0); // Create the Buffer_Mutex for consumer

    *buffer_index = 0; // init
    // *print_index = 0; // init
    
    // For GCC use -g for GDB for exact line
    // Question for Dr Ghosh - Do we still need this, or is this what should be in shared memory
    pthread_mutex_init(buffer_mutex, NULL); // binary semaphore
    init(full, 5);
    init(empty, 0);
    *bookkeeping = -1;

    for (i = 0; i < 30; i++){
        MyStruct temp;
        temp.free = 1;
        // temp.processID = 0;
        // temp.thisJobSize = 0;
        shmctl(shmid, SHM_UNLOCK, NULL);
        p1[i] = temp;
        sem_init(&p1[i].lock, 1,1); // binary semaphore

    }


    for (i = 0; i < users ; i++){
        int numJobs = (rand() % 30 + 1);
        *bookkeeping += numJobs; 

        if(fork() == 0){ // create child id
        
            printf("Bookkeeping is %d\n", *bookkeeping);
            printf("Added %d jobs.\n", numJobs);
            producer(numJobs);
            exit(0);
        }
        else{
            while(NULL); // I believe this waits for all the child forks to complete, but I need the consumers to start
        }
    }
    while(*bookkeeping < 1){
        sleep(1);
    }
   
     // These two lines declare an array of actual threads, and an array of int integer value of threads for the MyStructS
    pthread_t MyStructThreads[consumers];
    int MyStructThreadNumber[consumers];
    // For all the consumer threads from command line, start this many MyStructs.
    for (i = 0; i < consumers; ) {
        printf("Made a consumer!\n");
        MyStructThreadNumber[i] = i;
        // playing a bit with thread and thread_numb pointers...
        pthread_create(&MyStructThreads[i], // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       consumer, // void *(*start_routine) (void *)
                       &MyStructThreadNumber[i]);  // void *arg
        i++;
    }
    // Wait to move on until all of the threads have completed. (starting with the users!)
    // for (i = 0; i < 6; i++)
    //     pthread_join(MyStructThreads[i], NULL);
    while(*bookkeeping != 0){
        sleep(1);
    }
    pthread_mutex_destroy(buffer_mutex);
    // sem_destroy(&full_sem);
    // sem_destroy(&empty_sem);
    while(*bookkeeping != 0){
        sleep(1);
    }
    end = clock() - start;
    printf("ASGN3_buffer.c took %ld seconds.\n", end);
    shmdt(p1);
    shmdt(buffer_index);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);
    shmdt(buffer_mutex);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID,NULL);
    shmctl(full2, IPC_RMID, NULL);
    shmctl(empty2, IPC_RMID,NULL);
    shmctl(bookkeepingPRE, IPC_RMID, NULL);
    shmctl(shmidMutex, IPC_RMID,NULL);
    return 0;
}
