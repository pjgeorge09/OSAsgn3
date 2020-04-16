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
#include <sys/times.h>
#include <time.h>

int users, consumers; // For global of system args
pthread_t *MyStructThreads;
struct timespec start, end;

//-------------------------------STRUCTURES-----------------------------//
struct countingSemaphore{ // Counting Semaphore Struct
    int val;
    sem_t gate; // Binary Semaphore
    sem_t mutex; // Binary Semaphore
} *full, *empty;

// A job to put into shared memory.
typedef struct Job{
    int processID, size;
    sem_t lock; // This locks this struct when it is being read or written. Binary semaphore
    int Available; // 0 no, 1 yes
    struct Job* next;
    struct Job* prev;
    int index;
} WhyDoINeedThis;


//------------------------------- GLOBALS -----------------------------//
int *bookkeeping;
pthread_t pthreadArray[40]; // Max 40 threads
// Shared Memory Integers
int shmid, full2, empty2, bookkeepingPRE, sleepSHM;
// Shared Memory Objects
struct Job myJob, *MyNodes;


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
    if (thisCountingSemaphore->val == 1){ 
        sem_post(&(thisCountingSemaphore->gate));
        }
    sem_post(&(thisCountingSemaphore->mutex));
    }

void addNode(int datasize, int thisID) {
    int Z;
    int myspot = -1;
    // For 1 to 30 jobs, find a slot ANYWHERE that's available (although start at front for speed)
    for (Z=1; Z<31; Z++){
        // If available, and NOT PLACED (-1) , place it.
        if(MyNodes[Z].Available == 1 && myspot == -1){
            MyNodes[Z].Available = 0;
            myspot = Z; // this is the INDEX in the SHM of 31 that local is using
        }
    }
    // Set the data at this node.
    MyNodes[myspot].processID = thisID;
    MyNodes[myspot].size = datasize;
    MyNodes[myspot].Available = 0;

    // Init here? Why.
    sem_init(&MyNodes[myspot].lock,1,1);

    // Start at the HEAD and HEAD + 1
    MyNodes[myspot].prev = (struct Job*)&MyNodes[0];
    MyNodes[myspot].next = ((struct Job*)&MyNodes[0])->next;    

    while(1){
        if(sem_trywait(&MyNodes[myspot].prev->lock) != -1){
            int toFix = MyNodes[myspot].prev->index;
        // I have reached the end of the list. The last item just needs to point to me.
        if(MyNodes[myspot].next == NULL){

            sem_wait(&MyNodes[myspot].lock); // Bring this into the whileloop
            // The last node's NEXT is the address of this node
            MyNodes[myspot].prev->next = (struct Job*)&MyNodes[myspot]; // Not null anymore!
            MyNodes[myspot].next = NULL;
            sem_post(&MyNodes[myspot].lock);
            sem_post(&MyNodes[myspot].prev->lock);
            return;
        }

        // If the next thing is SMALLER
        else if (MyNodes[myspot].next->size < MyNodes[myspot].size){
            // Move forward
            MyNodes[myspot].prev = MyNodes[myspot].prev->next; // My back pointer is now my back pointer's next (Ex, was 2 3, is now 3 3)
            MyNodes[myspot].next = MyNodes[myspot].prev->next; // My forward pointer is now my forward pointer's next thing. (ex, was 3 3, now 3 4);
            sem_post(&MyNodes[toFix].lock);
            // And check again.
        }

        // If the next thing is LARGER , then I want my last to point to me, and my nextprev to point to me
        else{
            // Previous for this is locked, but
            sem_wait(&MyNodes[myspot].next->lock);  
            sem_wait(&MyNodes[myspot].lock); 
            MyNodes[myspot].prev->next = (struct Job*)&MyNodes[myspot]; 
            MyNodes[myspot].next->prev = (struct Job*)&MyNodes[myspot]; 
            sem_post(&MyNodes[myspot].lock); 
            sem_post(&MyNodes[myspot].next->lock);  
            sem_post(&MyNodes[toFix].lock);  
            return;
        }
        }
    } 
}

/* Method to remove a struct from the buffer */
void dequeuebuffer(int t_num) {
    // temp Job struct initially as head
    struct Job* temp = (struct Job*)&MyNodes[0]; // pretty sure i dont use this
    // If there's no job in the queue, wait and listen until a job or a signal.
    while(MyNodes[0].next == NULL){
        
    }

    sem_wait(&MyNodes[0].lock); // Wait Head
    sem_wait(&MyNodes[0].next->lock); // Wait Head->next
    // Flag = 1 is the case where there is at least TWO items in the queue
    int flag = 0; 
    if(MyNodes[0].next->next != NULL){
        sem_wait(&MyNodes[0].next->next->lock); // Wait 2
        flag = 1;    
    }

    // Record the size, pid, and location.
    int dequeueSize = MyNodes[0].next->size;
    int dequeuePID = MyNodes[0].next->processID;
    int save = MyNodes[0].next->index;
    if(flag == 1){
        MyNodes[0].next->next->prev = (struct Job*)&MyNodes[0]; // Head.next.next = 2, looking at what 2 points backwards to
    }
    if(flag == 1){
        // 2 existed
        MyNodes[0].next = MyNodes[0].next->next;
    }
    else{
        // 2 did not exist
        MyNodes[0].next = NULL;
    }
    
    // Now NULL the old.
    MyNodes[save].next = NULL;
    MyNodes[save].prev = NULL;
    MyNodes[save].processID = 6666;
    MyNodes[save].size = 6666;
    MyNodes[save].Available = 1; // Available again!

    if( flag == 1){
        sem_post(&MyNodes[0].next->lock); // Post 2 EVEN IF NULL
    }
    sem_post(&MyNodes[save].lock); // Post the locked thing
    sem_post(&MyNodes[0].lock); // Finally, free the head.
    printf("Consumer %d dequeue %d , %d from buffer. Jobs left : %d.\n", t_num, dequeuePID, dequeueSize, *bookkeeping);
    
    // Sleep proportionally    
    sleep(dequeueSize/200);
    return;
}
 
/* Producer Method */
void *producer(int jobs) {
    int myID = getpid();
    int i=0;
    while (i < jobs) {
        
        int value = rand() % 900 + 100;
        
        // Lock Full Semaphore
        proberen(full);
        // Insert buffer once Full allows.
        addNode(value, myID);   
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
        dequeuebuffer(thread_numb);
        // Unlock Full
        verhogen(full);
        i++;
        *bookkeeping -=1;
        printf("Consumer %d has completed %d jobs.\n",thread_numb,i);
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

    shmdt(MyNodes);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);
    
    shmctl(shmid, IPC_RMID, NULL);
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

    // Creating shared Memory for jobs
    shmid = shmget(15602, 31*sizeof(struct Job), IPC_CREAT | 0666);
    // Creating shared memory for Full counting semaphore
    full2 = shmget(10624, sizeof(struct countingSemaphore), IPC_CREAT | 0666 );
    // Creating shared memory for Empty counting semaphore
    empty2 = shmget(10625, sizeof(struct countingSemaphore), IPC_CREAT | 0666);
    // Creating shared memory for bookkeeping.
    bookkeepingPRE = shmget(10626, sizeof(int), IPC_CREAT | 0666);

  
    MyNodes = (struct Job*)shmat(shmid, NULL, 0); // Using this.
    full = (struct countingSemaphore*)shmat(full2, NULL, 0); // Create the FULL semaphore on shared memory
    empty = (struct countingSemaphore*)shmat(empty2, NULL, 0); // Create the EMPTY semaphore on shared memory
    bookkeeping = (int*)shmat(bookkeepingPRE, NULL, 0); // Create the bookkeeping integer for jobs left to finish

    // Initialize stuff
    init(full, 30);
    init(empty, 0);
    *bookkeeping = 0;

    // // Init all 30 spaces of MyStructs
    // MAIN INIT
    shmctl(shmid, SHM_UNLOCK, NULL); // Required for access.
    for (i = 0; i < 31; i++){
        MyNodes[i].processID = 5000;
        MyNodes[i].size = (rand() % 8000 + 2000);
        printf("MyNode[%d] size %d id %d\n", i, MyNodes[i].size, MyNodes[i].processID);
        MyNodes[i].Available = 1;
        MyNodes[i].next = NULL;
        MyNodes[i].prev = NULL;
        MyNodes[i].index = i;
        sem_init(&MyNodes[i].lock,1,1);
        if(i==0){
            MyNodes[0].Available = 0;
            MyNodes[0].size = 10000;
        }
        
    }
    // Fork User Processes
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

    // // Wait to move on until all of the threads have completed. (starting with the users!)
    while(*bookkeeping != 0){
        // sleep(60);
        // for(i = 0; i < 31; i++){
        //     printf("Location %d has size = %d , PID = %d.\n", MyNodes[i].index, MyNodes[i].size, MyNodes[i].processID);
        // }
        
    }

    for (i = 0; i < consumers; i++) { // Just added this loop. Maybe use.
        pthread_cancel(MyStructThreads[i]);
        pthread_join(MyStructThreads[i], NULL);
        // printf("Canceled %lx\n", MyStructThreads[i]);
    }

    clock_gettime(_POSIX_MONOTONIC_CLOCK, &end);
    double deltaTime = (end.tv_sec - start.tv_sec) * 1e9;
    deltaTime = (deltaTime + (end.tv_nsec - start.tv_nsec)) * 1e-9;
    printf("Elapsed time:    %f\n", deltaTime);
    // sem_destroy(&full_sem);
    // sem_destroy(&empty_sem);
    
    shmdt(MyNodes);
    shmdt(full);
    shmdt(empty);
    shmdt(bookkeeping);

    shmctl(shmid, IPC_RMID, NULL);
    shmctl(full2, IPC_RMID, NULL);
    shmctl(empty2, IPC_RMID,NULL);
    shmctl(bookkeepingPRE, IPC_RMID, NULL);
    printf("---------------------------------------FINAL EXIT----------------------------------\n");
    return 0;
}