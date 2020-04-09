#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
 
#define SIZE 5
#define NUMB_THREADS 6
#define PRODUCER_LOOPS 2
#define printJobSize 1000 // Up to 1000 bytes
#define SMemorySize     30*printJobSize 
 


// Handle Command Line Arguements
// #1 , Number of producer (user) processes
// #2 , Number of consumer (printer) threads

// Usenanosleep() or sleep() to simulate a random delay between 0.1 and 1 seconds BETWEEN two successive print jobs SUBMITTED BY THE SAME USER.
// User 1) Insert print request into global print queue (Additional book-keeping params allowed to be inserted here!)
// Use a struct for producer print request and the globalqueue will be a linked-list of structs
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
// Threads exit when all print requests completed. 

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

typedef int buffer_t;
buffer_t buffer[SIZE];
int buffer_index;
 
pthread_mutex_t buffer_mutex;
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
} full, empty;

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
 
void insertbuffer(buffer_t value) {
    if (buffer_index < SIZE) {
        buffer[buffer_index++] = value;
    } else {
        printf("Buffer overflow\n");
    }
}
 
buffer_t dequeuebuffer() {
    if (buffer_index > 0) {
        return buffer[--buffer_index]; // buffer_index-- would be error!
    } else {
        printf("Buffer underflow\n");
    }
    return 0;
}
 
 
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    buffer_t value;
    int i=0;
    while (i++ < PRODUCER_LOOPS) {
        sleep(rand() % 10);
        value = rand() % 100;
        proberen(&full);
        // sem_wait(&full_sem); // sem=0: wait. sem>0: go and decrement it
        /* possible race condition here. After this thread wakes up,
           another thread could aqcuire mutex before this one, and add to list.
           Then the list would be full again
           and when this thread tried to insert to buffer there would be
           a buffer overflow error */
        pthread_mutex_lock(&buffer_mutex); /* protecting critical section */
        insertbuffer(value);
        pthread_mutex_unlock(&buffer_mutex);
        verhogen(&empty);
        // sem_post(&empty_sem); // post (increment) emptybuffer semaphore
        printf("Producer %d added %d to buffer\n", thread_numb, value);
    }
    pthread_exit(0);
}

// Printer 
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    buffer_t value;
    int i=0;
    while (i++ < PRODUCER_LOOPS) {
        // sem_wait(&empty_sem);
        proberen(&empty);
        /* there could be race condition here, that could cause
           buffer underflow error */
        pthread_mutex_lock(&buffer_mutex);
        value = dequeuebuffer(value);
        pthread_mutex_unlock(&buffer_mutex);
        verhogen(&full);
        // sem_post(&full_sem); // post (increment) fullbuffer semaphore
        printf("Consumer %d dequeue %d from buffer\n", thread_numb, value);
   }
    pthread_exit(0);
}
 
int main(int argc, int **argv) {
    buffer_index = 0;
 
    pthread_mutex_init(&buffer_mutex, NULL);
    init(&full, SIZE);
    init(&empty, 0);
    // sem_init(&full_sem, // sem_t *sem
    //          0, // int pshared. 0 = shared between threads of process,  1 = shared between processes
    //          SIZE); // unsigned int value. Initial value
    // sem_init(&empty_sem,
    //          0,
    //          0);
    /* full_sem is initialized to buffer size because SIZE number of
       producers can add one element to buffer each. They will wait
       semaphore each time, which will decrement semaphore value.
       empty_sem is initialized to 0, because buffer starts empty and
       consumer cannot take any element from it. They will have to wait
       until producer posts to that semaphore (increments semaphore
       value) */
    pthread_t thread[NUMB_THREADS];
    int thread_numb[NUMB_THREADS];
    int i;
    for (i = 0; i < NUMB_THREADS; ) {
        thread_numb[i] = i;
        pthread_create(thread + i, // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       producer, // void *(*start_routine) (void *)
                       thread_numb + i);  // void *arg
        i++;
        thread_numb[i] = i;
        // playing a bit with thread and thread_numb pointers...
        pthread_create(&thread[i], // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       consumer, // void *(*start_routine) (void *)
                       &thread_numb[i]);  // void *arg
        i++;
    }
 
    for (i = 0; i < NUMB_THREADS; i++)
        pthread_join(thread[i], NULL);
 
    pthread_mutex_destroy(&buffer_mutex);
    // sem_destroy(&full_sem);
    // sem_destroy(&empty_sem);
 
    return 0;
}
