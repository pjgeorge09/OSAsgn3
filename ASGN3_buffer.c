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
#include <sys/time.h>
#include <time.h>

int users, consumers;
pthread_t *MyStructThreads;
pthread_t pthreadArray[40]; // Max 40 threads

#define SHMSZ 30*sizeof( MyStruct)
#define SHMSZ2 sizeof(int)
int *buffer_index, *print_index; // --> "Traverse print queue" //Global count concept
struct timespec start, end;

//-------------------------------STRUCTURES-----------------------------//
struct countingSemaphore{ // Counting Semaphore Struct
    int val;
    sem_t gate; // Binary Semaphore
    sem_t mutex; // Binary Semaphore
} *full, *empty;

typedef struct {
    int processID, thisJobSize;
    sem_t lock; // This locks this struct when it is being read or written
    int Initialized; // 0 no, 1 yes
} MyStruct;

//------------------------------- GLOBALS -----------------------------//
int *bookkeeping;
// Shared Memory Integers
int shmid, shmid2, shmid3, full2, empty2, bookkeepingPRE,sleepSHM;
// Shared Memory Objects
MyStruct *p1;

//------------------------------- INIT METHOD -----------------------------//
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

/* Method to insert the data into the shared memory buffer 
   arg1 is the size of the job to be printed
   arg2 is the ID it came from*/
void insertbuffer(int datasize, int thisID) {

    // Init a new MyStruct
    MyStruct thisMyStruct; 
    thisMyStruct.processID = thisID; // Set the ID
    thisMyStruct.thisJobSize = datasize; // Set the Print Size
    sem_init(&thisMyStruct.lock, 1,1); // binary semaphore
    thisMyStruct.Initialized = 1;

    // shmctl required for accessing / changing values at shmid
    shmctl(shmid, SHM_UNLOCK, NULL);
    // Save the current printer index and update it
    int thisPrinterIndex = *print_index;
    *print_index = (*print_index+1)%30; // Circular
    // Lock this binary
    sem_wait(&p1[thisPrinterIndex].lock);
    // Set this spot to temp struct
    p1[thisPrinterIndex] = thisMyStruct;
    // Unlock the binary
    sem_post(&p1[thisPrinterIndex].lock);
    return;
}

/* Method to remove a struct from the buffer */
MyStruct dequeuebuffer() {
    // Create temp var
    MyStruct temp;
    int found = 0;
    // While a location has not been found
    while(!found){
        if(*bookkeeping == 0){
          pthread_exit(0);
        }
        // Never try to read before something has been initialized here once
        while(p1[*buffer_index].Initialized == 0){
            // sleep(1);
        }
        // Save the current buffer index and update it
        int thisBufferIndex = *buffer_index;
        *buffer_index = (*buffer_index+1)%30;
        // Lock this binary 
        sem_wait(&p1[thisBufferIndex].lock);
        // Save this current state
        temp = p1[thisBufferIndex];
        // Unlock this binary
        sem_post(&p1[thisBufferIndex].lock);
        found = 1;
        return(temp);
    }
    return(temp);
}
 
/* Producer Method */
void *producer(int jobs) {
    // int value;
    int myID = getpid();
    // For this thread, we need randm number for number of jobs this user wants to submit (1-30)
    // Then we need to give each of those jobs a random size
    // Then this user will start trying to put all of his jobs into the shared print queue (At the same time as the other users)
    int i=0;
    while (i < jobs) {
        // sleep(rand() % 10); // Sleep between jobs for a particular user
        int value = rand() % 900 + 100;
        // Lock Full Semaphore
        proberen(full);
        // Insert buffer once Full allows.
        insertbuffer(value, myID);   
        // Unlock Empty   
        verhogen(empty);
        printf("Producer %d added %d to buffer\n", myID, value);
        int mySleep = (rand() % 10);
        sleep(mySleep/10); // Sleep between jobs for a particular user
        i++;
    }
    return(NULL);
}
// consumer print process.
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n; // My <consumer-id>
    // MyStruct tempMyStruct;
    int i=0;
    while (1) {
        proberen(empty);
        // Dequeue an item
        MyStruct tempMyStruct = dequeuebuffer();
        // Unlock Full
        verhogen(full);
        // Sleep for a time proportional to the job size
        // sleep(tempMyStruct.thisJobSize / 200); 
        i++;
        printf("Consumer %d dequeue %d , %d from buffer. Jobs left : %d. Consumer %d has completed %d jobs.\n", thread_numb, tempMyStruct.processID, tempMyStruct.thisJobSize, *bookkeeping, thread_numb, i);
        // Sleep proportionally    
        int toSleep = tempMyStruct.thisJobSize/200;
        sleep(toSleep);
        
        // Update bookkeeping
        *bookkeeping -=1;
   }
}

// Suicide
void sigintHandler(int sig_num){    
    puts("Exit Ctrl C\n");
    int i;
    sleep(10);
    for (i = 0; i < consumers; i++) { // Just added this loop. Maybe use.
        pthread_cancel(MyStructThreads[i]);
        pthread_join(MyStructThreads[i], NULL);
        printf("Canceled %lx\n", MyStructThreads[i]);
    }
    shmdt(p1);
    shmdt(buffer_index);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID,NULL);
    shmctl(full2, IPC_RMID, NULL);
    shmctl(empty2, IPC_RMID,NULL);
    shmctl(bookkeepingPRE, IPC_RMID, NULL);
}
int main(int argc, char **argv) {
    clock_gettime(_POSIX_MONOTONIC_CLOCK, &start);
    users = atoi(argv[1]);
    consumers = atoi(argv[2]);
    int i;
    signal(SIGINT, sigintHandler);

    // Creating shared Memory for buffer
    shmid = shmget(10212, SHMSZ, IPC_CREAT | 0666);
    // Creating shared Memory for integer
    shmid2 = shmget(10623, SHMSZ2, IPC_CREAT | 0666);
    // Creating shared Memory for integer
    shmid3 = shmget(10925, SHMSZ2, IPC_CREAT | 0666);
    // Creating shared memory for Full counting semaphore
    full2 = shmget(10424, sizeof(struct countingSemaphore), IPC_CREAT | 0666 );
    // Creating shared memory for Empty counting semaphore
    empty2 = shmget(10725, sizeof(struct countingSemaphore), IPC_CREAT | 0666);
    // Create a bookkeeping number for graceful exit
    bookkeepingPRE = shmget(10226, sizeof(int), IPC_CREAT | 0666);

    p1 = (MyStruct*)shmat(shmid, NULL, 0); // Create the MyStruct array on shared memory
    buffer_index = (int*)shmat(shmid2, NULL, 0); // Create the buffer index on this shared memory
    print_index = (int*)shmat(shmid3, NULL, 0); // Create the buffer index on this shared memory
    full = (struct countingSemaphore*)shmat(full2, NULL, 0); // Create the FULL semaphore on shared memory
    empty = (struct countingSemaphore*)shmat(empty2, NULL, 0); // Create the EMPTY semaphore on shared memory
    bookkeeping = (int*)shmat(bookkeepingPRE, NULL, 0); // Create the bookkeeping integer for jobs left to finish

    // Initialize stuff
    *buffer_index = 0;
    *print_index = 0;
    init(full, 30);
    init(empty, 0);
    *bookkeeping = 0;


    // Init all 30 spaces of MyStructs
    shmctl(shmid, SHM_UNLOCK, NULL);
    for (i = 0; i < 30; i++){
        p1[i].Initialized = 0;
        p1[i].processID = 5000;
        p1[i].thisJobSize = 5000;
        sem_init(&p1[i].lock, 1,1); // binary semaphore
    }

    // Fork User Processes
    for (i = 0; i < users ; i++){
        int numJobs = (rand() % 30 + 1);
        *bookkeeping += numJobs; 
        if(fork() == 0){ // create child id
            // printf("Bookkeeping is %d\n", *bookkeeping);
            // printf("Added %d jobs.\n", numJobs);
            producer(numJobs);
            exit(0);
        }
        else{
            while(NULL); // I believe this waits for all the child forks to complete, but I need the consumers to start
        }
    }

    while(*bookkeeping < 1){
        // sleep(1);
    }
   
     // These two lines declare an array of actual threads, and an array of int integer value of threads for the MyStructS
    pthread_t MyStructThreads[consumers];
    int MyStructThreadNumber[consumers];
    // For all the consumer threads from command line, start this many MyStructs.
    for (i = 0; i < consumers; ) {
        MyStructThreadNumber[i] = i;
        pthreadArray[i] = MyStructThreads[i];
        pthread_create(&MyStructThreads[i], // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       consumer, // void *(*start_routine) (void *)
                       &MyStructThreadNumber[i]);  // void *arg
        i++;
    }

    // Wait to move on until all of the threads have completed. (starting with the users!)
    while(*bookkeeping != 0){
        // sleep(5);
        // printf("Bookkeeping is %d\n", *bookkeeping);
    }
    // Once Bookkeeping is done, all the threads can exit.
    for (i = 0; i < consumers; i++) { 
        pthread_cancel(MyStructThreads[i]);
        pthread_join(MyStructThreads[i], NULL);
        printf("Canceled %lx\n", MyStructThreads[i]);
    }

    // sem_destroy(&full_sem);
    // sem_destroy(&empty_sem);
    clock_gettime(_POSIX_MONOTONIC_CLOCK, &end);
    double deltaTime = (end.tv_sec - start.tv_sec) * 1e9;
    deltaTime = (deltaTime + (end.tv_nsec - start.tv_nsec)) * 1e-9;
    printf("Elapsed time:    %f\n", deltaTime);
    shmdt(p1);
    shmdt(buffer_index);
    shmdt(print_index);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);

    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID,NULL);
    shmctl(shmid3, IPC_RMID,NULL);
    shmctl(full2, IPC_RMID, NULL);
    shmctl(empty2, IPC_RMID,NULL);
    shmctl(bookkeepingPRE, IPC_RMID, NULL);
    printf("----------------------------------------------FINAL EXIT----------------------------------\n");
    return 0;
}
